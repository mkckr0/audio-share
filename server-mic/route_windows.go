//go:build windows

package main

import (
	"fmt"
	"hash/fnv"
	"os"
	"sync/atomic"
	"syscall"
	"unsafe"
)

var (
	procWaveOutMessage = winmm.NewProc("waveOutMessage")
	procCreateMutex    = kernel32.NewProc("CreateMutexW")
)

// WinMM and WASAPI expose the same opaque endpoint ID. Friendly names can be
// localized, truncated to 31 characters by WinMM, or shared by two devices.
func renderEndpointID(waveHandle uintptr) (string, error) {
	const queryID, queryIDSize = 0x0800 + 17, 0x0800 + 18 // mmddk.h
	var deviceID uint32
	result, _, _ := procWaveOutGetID.Call(waveHandle, uintptr(unsafe.Pointer(&deviceID)))
	if err := winmmError("waveOutGetID", result); err != nil {
		return "", err
	}
	var size uint32
	result, _, _ = procWaveOutMessage.Call(uintptr(deviceID), queryIDSize, uintptr(unsafe.Pointer(&size)), 0)
	if err := winmmError("query render endpoint ID size", result); err != nil {
		return "", err
	}
	if size < 4 || size%2 != 0 || size > 65536 {
		return "", fmt.Errorf("invalid render endpoint ID size: %d", size)
	}
	buffer := make([]uint16, size/2)
	result, _, _ = procWaveOutMessage.Call(uintptr(deviceID), queryID, uintptr(unsafe.Pointer(&buffer[0])), uintptr(size))
	if err := winmmError("query render endpoint ID", result); err != nil {
		return "", err
	}
	return syscall.UTF16ToString(buffer), nil
}

// Shared with server-core/src/win32/microphone_route.hpp. The high 32 bits
// change on every transition, even if a whole mic session fits between reads.
// The low 32 bits identify the process actually rendering the microphone.
func routeName(id string) string {
	hash := fnv.New64a()
	hash.Write([]byte(id))
	return fmt.Sprintf(`Local\AudioShare.Microphone.v1.%016x`, hash.Sum64())
}

type routeState struct {
	mapping syscall.Handle
	address uintptr
	value   *uint64
}

func openRouteState(id string) (*routeState, error) {
	name, err := syscall.UTF16PtrFromString(routeName(id))
	if err != nil {
		return nil, err
	}
	mapping, err := syscall.CreateFileMapping(syscall.InvalidHandle, nil, syscall.PAGE_READWRITE, 0, 8, name)
	if err != nil {
		return nil, err
	}
	address, err := syscall.MapViewOfFile(mapping, syscall.FILE_MAP_WRITE, 0, 0, 8)
	if err != nil {
		syscall.CloseHandle(mapping)
		return nil, err
	}
	return &routeState{mapping, address, *(**uint64)(unsafe.Pointer(&address))}, nil
}

func (s *routeState) close() {
	syscall.UnmapViewOfFile(s.address)
	syscall.CloseHandle(s.mapping)
}

type microphoneRoute struct {
	state *routeState
	owner syscall.Handle
}

func claimMicrophoneRoute(id string) (*microphoneRoute, error) {
	// Only writers open this object. Windows releases it on process exit,
	// including crashes. Holding the handle requires no OS thread affinity.
	name, err := syscall.UTF16PtrFromString(routeName(id) + ".writer")
	if err != nil {
		return nil, err
	}
	handle, _, lastErr := procCreateMutex.Call(0, 0, uintptr(unsafe.Pointer(name)))
	if handle == 0 {
		return nil, fmt.Errorf("reserve microphone route: %w", lastErr)
	}
	owner := syscall.Handle(handle)
	if lastErr == syscall.ERROR_ALREADY_EXISTS {
		syscall.CloseHandle(owner)
		return nil, fmt.Errorf("another microphone receiver is using this playback endpoint")
	}
	state, err := openRouteState(id)
	if err != nil {
		syscall.CloseHandle(owner)
		return nil, err
	}
	route := &microphoneRoute{state, owner}
	route.publish(uint32(os.Getpid()))
	return route, nil
}

func (r *microphoneRoute) publish(pid uint32) {
	epoch := uint32(atomic.LoadUint64(r.state.value)>>32) + 1
	atomic.StoreUint64(r.state.value, uint64(epoch)<<32|uint64(pid))
}

func (r *microphoneRoute) close() {
	r.publish(0)
	r.state.close()
	syscall.CloseHandle(r.owner)
}
