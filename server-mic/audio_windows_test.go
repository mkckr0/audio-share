//go:build windows

package main

import (
	"context"
	"os"
	"runtime"
	"testing"
	"time"
	"unsafe"
)

// Opt-in integration test: writes zero PCM to the explicitly named VB-CABLE
// endpoint, without opening a microphone or changing Windows audio settings.
func TestNativePCMThroughput(t *testing.T) {
	query := os.Getenv("AUDIO_SHARE_MIC_TEST_DEVICE")
	if query == "" {
		t.Skip("set AUDIO_SHARE_MIC_TEST_DEVICE to run the native zero-PCM throughput check")
	}
	devices, err := listOutputDevices()
	if err != nil {
		t.Fatal(err)
	}
	device, err := selectOutput(devices, query)
	if err != nil {
		t.Fatal(err)
	}
	sink, err := openWaveSink(device, audioFormat{48000, 1, 16})
	if err != nil {
		t.Fatal(err)
	}
	defer func() {
		if err := sink.Close(); err != nil {
			t.Error(err)
		}
	}()
	ctx, cancel := context.WithTimeout(context.Background(), 15*time.Second)
	defer cancel()
	var pcm [1920]byte
	start := time.Now()
	for i := 0; i < 500; i++ {
		if err := sink.WritePCM(ctx, pcm[:]); err != nil {
			t.Fatal(err)
		}
	}
	elapsed := time.Since(start)
	t.Logf("submitted 10 seconds of PCM in %s with %d native buffers", elapsed, len(sink.buffers))
	if elapsed > 10600*time.Millisecond {
		t.Fatalf("native output cannot sustain real time: %s for 10 seconds of PCM", elapsed)
	}
}

func TestNativeReceiverKeepsUpWithTwentyMillisecondFrames(t *testing.T) {
	query := os.Getenv("AUDIO_SHARE_MIC_TEST_DEVICE")
	if query == "" {
		t.Skip("set AUDIO_SHARE_MIC_TEST_DEVICE to run the native zero-PCM receiver check")
	}
	devices, err := listOutputDevices()
	if err != nil {
		t.Fatal(err)
	}
	device, err := selectOutput(devices, query)
	if err != nil {
		t.Fatal(err)
	}
	address, status, _, _ := startReceiver(t, func(format audioFormat) (pcmSink, error) {
		return openWaveSink(device, format)
	}, 5*time.Second)
	conn := dialReceiver(t, address)
	conn.SetDeadline(time.Now().Add(20 * time.Second))
	handshake(t, conn)
	var packet [1924]byte
	packet[0], packet[1] = 0x80, 7
	start := time.Now()
	for i := 0; i < 500; i++ {
		if wait := time.Until(start.Add(time.Duration(i) * 20 * time.Millisecond)); wait > 0 {
			time.Sleep(wait)
		}
		if _, err := conn.Write(packet[:]); err != nil {
			t.Fatal(err)
		}
		acknowledgeFrames(t, conn, 1)
	}
	deadline := time.Now().Add(time.Second)
	for {
		state := status.Snapshot()
		if state.Frames+state.DroppedFrames >= 500 {
			break
		}
		if time.Now().After(deadline) {
			t.Fatalf("native receiver fell behind: %+v", state)
		}
		time.Sleep(time.Millisecond)
	}
	state := status.Snapshot()
	t.Logf("paced 500 frames in %s: submitted=%d received=%d dropped=%d", time.Since(start), state.Frames, state.ReceivedFrames, state.DroppedFrames)
	if state.ReceivedFrames != 500 || state.Frames < 498 || state.DroppedFrames > 2 {
		t.Fatalf("chronic native receiver drops: %+v", state)
	}
	conn.Close()
}

func TestNativeAudioBufferSurvivesGoGarbageCollection(t *testing.T) {
	buffer, err := allocateWaveBuffer()
	if err != nil {
		t.Fatal(err)
	}
	defer func() {
		if err := buffer.release(); err != nil {
			t.Error(err)
		}
	}()
	buffer.pcm[0], buffer.pcm[1919] = 0x12, 0x34
	for i := 0; i < 3; i++ {
		runtime.GC()
	}
	if buffer.pcm[0] != 0x12 || buffer.pcm[1919] != 0x34 {
		t.Fatal("native audio storage moved or changed")
	}
	if buffer.header.Data != uintptr(unsafe.Pointer(&buffer.pcm[0])) || buffer.header.BufferLength != 1920 {
		t.Fatalf("header does not address native PCM: %+v", buffer.header)
	}
}

func TestWindowsAudioStructuresMatchNativeABI(t *testing.T) {
	if unsafe.Sizeof(waveOutCaps{}) != 84 {
		t.Fatalf("WAVEOUTCAPSW size=%d", unsafe.Sizeof(waveOutCaps{}))
	}
	var header waveHeader
	wantSize, wantFlags := uintptr(48), uintptr(24)
	if unsafe.Sizeof(uintptr(0)) == 4 {
		wantSize, wantFlags = 32, 16
	}
	if unsafe.Sizeof(header) != wantSize || unsafe.Offsetof(header.Flags) != wantFlags {
		t.Fatalf("WAVEHDR size=%d flags offset=%d", unsafe.Sizeof(header), unsafe.Offsetof(header.Flags))
	}
	var format waveFormatEx
	if unsafe.Offsetof(format.SampleRate) != 4 || unsafe.Offsetof(format.BlockAlign) != 12 || unsafe.Offsetof(format.ExtraSize) != 16 {
		t.Fatal("WAVEFORMATEX has invalid field alignment")
	}
}
