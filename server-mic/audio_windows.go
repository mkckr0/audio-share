//go:build windows

package main

import (
	"context"
	"errors"
	"fmt"
	"sync/atomic"
	"syscall"
	"time"
	"unsafe"
)

type waveHeader struct {
	Data          uintptr
	BufferLength  uint32
	BytesRecorded uint32
	User          uintptr
	Flags         uint32
	Loops         uint32
	Next          uintptr
	Reserved      uintptr
}

type waveFormatEx struct {
	FormatTag             uint16
	Channels              uint16
	SampleRate            uint32
	AverageBytesPerSecond uint32
	BlockAlign            uint16
	BitsPerSample         uint16
	ExtraSize             uint16
}

type waveBuffer struct {
	memory   unsafe.Pointer
	header   *waveHeader
	pcm      []byte
	prepared bool
	queued   bool
}

func allocateWaveBuffer() (waveBuffer, error) {
	size := unsafe.Sizeof(waveHeader{}) + maxFrameBytes
	address, _, callErr := procVirtualAlloc.Call(0, size, 0x3000, 0x04) // MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE
	if address == 0 {
		return waveBuffer{}, fmt.Errorf("VirtualAlloc audio buffer: %w", callErr)
	}
	// VirtualAlloc returns non-Go memory. Reinterpret the native address without
	// an intermediate Go heap pointer; both header and samples remain outside
	// the moving/collected Go heap until waveOutUnprepareHeader has succeeded.
	memory := *(*unsafe.Pointer)(unsafe.Pointer(&address))
	region := unsafe.Slice((*byte)(memory), int(size))
	header := (*waveHeader)(memory)
	pcm := region[int(unsafe.Sizeof(waveHeader{})):]
	header.Data = uintptr(unsafe.Pointer(&pcm[0]))
	header.BufferLength = maxFrameBytes
	return waveBuffer{memory: memory, header: header, pcm: pcm}, nil
}

func (b *waveBuffer) release() error {
	if b.memory == nil {
		return nil
	}
	if b.prepared {
		return errors.New("refusing to free an audio buffer still owned by WinMM")
	}
	result, _, callErr := procVirtualFree.Call(uintptr(b.memory), 0, 0x8000) // MEM_RELEASE
	if result == 0 {
		return fmt.Errorf("VirtualFree audio buffer: %w", callErr)
	}
	b.memory, b.header, b.pcm = nil, nil, nil
	return nil
}

var (
	winmm                      = syscall.NewLazyDLL("winmm.dll")
	procWaveOutGetNumDevs      = winmm.NewProc("waveOutGetNumDevs")
	procWaveOutGetDevCapsW     = winmm.NewProc("waveOutGetDevCapsW")
	procWaveOutGetID           = winmm.NewProc("waveOutGetID")
	procWaveOutGetErrorTextW   = winmm.NewProc("waveOutGetErrorTextW")
	procWaveOutOpen            = winmm.NewProc("waveOutOpen")
	procWaveOutPrepareHeader   = winmm.NewProc("waveOutPrepareHeader")
	procWaveOutWrite           = winmm.NewProc("waveOutWrite")
	procWaveOutReset           = winmm.NewProc("waveOutReset")
	procWaveOutUnprepareHeader = winmm.NewProc("waveOutUnprepareHeader")
	procWaveOutClose           = winmm.NewProc("waveOutClose")
	kernel32                   = syscall.NewLazyDLL("kernel32.dll")
	procVirtualAlloc           = kernel32.NewProc("VirtualAlloc")
	procVirtualFree            = kernel32.NewProc("VirtualFree")
)

type waveOutCaps struct {
	Manufacturer  uint16
	Product       uint16
	DriverVersion uint32
	Name          [32]uint16
	Formats       uint32
	Channels      uint16
	Reserved      uint16
	Support       uint32
}

func listOutputDevices() ([]outputDevice, error) {
	count, _, _ := procWaveOutGetNumDevs.Call()
	devices := make([]outputDevice, 0, count)
	for i := uintptr(0); i < count; i++ {
		var caps waveOutCaps
		result, _, _ := procWaveOutGetDevCapsW.Call(i, uintptr(unsafe.Pointer(&caps)), unsafe.Sizeof(caps))
		if err := winmmError("waveOutGetDevCaps", result); err != nil {
			return nil, err
		}
		devices = append(devices, outputDevice{i, syscall.UTF16ToString(caps.Name[:])})
	}
	return devices, nil
}

func winmmError(operation string, result uintptr) error {
	if result == 0 {
		return nil
	}
	var message [256]uint16
	textResult, _, _ := procWaveOutGetErrorTextW.Call(result, uintptr(unsafe.Pointer(&message[0])), uintptr(len(message)))
	if textResult == 0 {
		return fmt.Errorf("%s: %s (code %d)", operation, syscall.UTF16ToString(message[:]), result)
	}
	return fmt.Errorf("%s failed (code %d)", operation, result)
}

// Six buffers cap native queued PCM at 120 ms. The first block starts playing
// immediately; there is no prebuffer delay or unbounded application queue.
type waveSink struct {
	handle  uintptr
	buffers [6]waveBuffer
	next    int
	route   *microphoneRoute
}

func openWaveSink(device outputDevice, format audioFormat) (*waveSink, error) {
	if format != (audioFormat{48000, 1, 16}) {
		return nil, fmt.Errorf("unsupported output format: %+v", format)
	}
	waveFormat := waveFormatEx{FormatTag: 1, Channels: 1, SampleRate: 48000, AverageBytesPerSecond: 96000, BlockAlign: 2, BitsPerSample: 16}
	sink := &waveSink{}
	result, _, _ := procWaveOutOpen.Call(uintptr(unsafe.Pointer(&sink.handle)), device.ID, uintptr(unsafe.Pointer(&waveFormat)), 0, 0, 0)
	if err := winmmError("waveOutOpen "+device.Name, result); err != nil {
		return nil, err
	}
	// Publish the route before WritePCM can submit even the first sample.
	endpoint, err := renderEndpointID(sink.handle)
	if err == nil {
		sink.route, err = claimMicrophoneRoute(endpoint)
	}
	if err != nil {
		if closeErr := sink.Close(); closeErr != nil {
			err = errors.Join(err, &outputCleanupError{closeErr})
		}
		return nil, err
	}
	for i := range sink.buffers {
		buffer, err := allocateWaveBuffer()
		if err == nil {
			sink.buffers[i] = buffer
			result, _, _ := procWaveOutPrepareHeader.Call(sink.handle, uintptr(unsafe.Pointer(buffer.header)), unsafe.Sizeof(waveHeader{}))
			err = winmmError("waveOutPrepareHeader", result)
			if err == nil {
				sink.buffers[i].prepared = true
			}
		}
		if err != nil {
			if closeErr := sink.Close(); closeErr != nil {
				err = errors.Join(err, &outputCleanupError{closeErr})
			}
			return nil, err
		}
	}
	return sink, nil
}

func (s *waveSink) WritePCM(ctx context.Context, pcm []byte) error {
	if len(pcm) == 0 || len(pcm)%2 != 0 || len(pcm) > maxFrameBytes {
		return fmt.Errorf("invalid output PCM length: %d", len(pcm))
	}
	if s.handle == 0 {
		return errors.New("audio output is closed")
	}
	if err := ctx.Err(); err != nil {
		return err
	}
	buffer := &s.buffers[s.next]
	if buffer.queued && atomic.LoadUint32(&buffer.header.Flags)&1 == 0 {
		ticker := time.NewTicker(2 * time.Millisecond)
		timer := time.NewTimer(250 * time.Millisecond)
		defer ticker.Stop()
		defer timer.Stop()
		for atomic.LoadUint32(&buffer.header.Flags)&1 == 0 { // WHDR_DONE, written by WinMM
			select {
			case <-ctx.Done():
				return ctx.Err()
			case <-timer.C:
				return errors.New("audio output stalled for 250 ms")
			case <-ticker.C:
			}
		}
	}
	buffer.queued = false
	if err := ctx.Err(); err != nil {
		return err
	}
	copy(buffer.pcm, pcm)
	buffer.header.BufferLength = uint32(len(pcm))
	result, _, _ := procWaveOutWrite.Call(s.handle, uintptr(unsafe.Pointer(buffer.header)), unsafe.Sizeof(waveHeader{}))
	if err := winmmError("waveOutWrite", result); err != nil {
		return err
	}
	buffer.queued = true
	s.next = (s.next + 1) % len(s.buffers)
	return nil
}

func (s *waveSink) Close() error {
	if s.handle == 0 {
		return nil
	}
	var failures []error
	// Reset first: stopping/disconnecting discards queued audio immediately.
	result, _, _ := procWaveOutReset.Call(s.handle)
	if err := winmmError("waveOutReset", result); err != nil {
		failures = append(failures, err)
	}
	deadline := time.Now().Add(time.Second)
	for i := range s.buffers {
		buffer := &s.buffers[i]
		if buffer.prepared {
			for {
				result, _, _ := procWaveOutUnprepareHeader.Call(s.handle, uintptr(unsafe.Pointer(buffer.header)), unsafe.Sizeof(waveHeader{}))
				if result == 0 {
					buffer.prepared = false
					break
				}
				if result != 33 || time.Now().After(deadline) { // WAVERR_STILLPLAYING
					failures = append(failures, winmmError("waveOutUnprepareHeader", result))
					break
				}
				time.Sleep(5 * time.Millisecond)
			}
		}
		if !buffer.prepared {
			if err := buffer.release(); err != nil {
				failures = append(failures, err)
			}
		}
	}
	result, _, _ = procWaveOutClose.Call(s.handle)
	if err := winmmError("waveOutClose", result); err != nil {
		failures = append(failures, err)
	} else {
		s.handle = 0
		if s.route != nil {
			s.route.close()
			s.route = nil
		}
	}
	// A failing unprepare leaves its native allocation alive, never dangling.
	// The receiver treats cleanup failure as fatal and terminates the process.
	return errors.Join(failures...)
}
