package main

import (
	"context"
	"encoding/binary"
	"encoding/json"
	"math"
	"os"
	"path/filepath"
	"sync"
	"time"
)

type receiverStatus struct {
	ReceivedFrames uint64    `json:"received_frames"`
	ReceivedBytes  uint64    `json:"received_bytes"`
	DroppedFrames  uint64    `json:"dropped_frames"`
	DroppedBytes   uint64    `json:"dropped_bytes"`
	Active         bool      `json:"active"`
	Frames         uint64    `json:"frames"`
	Bytes          uint64    `json:"bytes"`
	LastError      string    `json:"last_error,omitempty"`
	Samples        uint64    `json:"samples"`
	Peak           float64   `json:"peak"`
	RMS            float64   `json:"rms"`
	Connections    uint64    `json:"connections"`
	SessionFrames  uint64    `json:"session_frames"`
	SessionBytes   uint64    `json:"session_bytes"`
	State          string    `json:"state"`
	Listen         string    `json:"listen"`
	Device         string    `json:"device"`
	Peer           string    `json:"peer,omitempty"`
	UpdatedAt      time.Time `json:"updated_at"`
	LastFrameAt    time.Time `json:"last_frame_at"`
}

type statusTracker struct {
	mu    sync.Mutex
	value receiverStatus
}

func newStatusTracker(listen, device string) *statusTracker {
	return &statusTracker{value: receiverStatus{State: "waiting", Listen: listen, Device: device}}
}

func (s *statusTracker) Snapshot() receiverStatus {
	s.mu.Lock()
	defer s.mu.Unlock()
	value := s.value
	value.UpdatedAt = time.Now().UTC()
	if time.Since(value.LastFrameAt) > time.Second {
		value.Peak, value.RMS = 0, 0
	}
	return value
}

func (s *statusTracker) connected(peer string) {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.value.Active, s.value.State, s.value.Peer = true, "streaming", peer
	s.value.Connections++
	s.value.SessionFrames, s.value.SessionBytes = 0, 0
}

func (s *statusTracker) disconnected(err error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.value.Active, s.value.State, s.value.Peer = false, "waiting", ""
	s.value.Peak, s.value.RMS = 0, 0
	if err != nil {
		s.value.LastError = err.Error()
	}
}

func (s *statusTracker) stopped(err error) {
	s.disconnected(err)
	s.mu.Lock()
	defer s.mu.Unlock()
	s.value.State = "stopped"
	if err != nil {
		s.value.State = "error"
	}
}

func (s *statusTracker) addFrame(pcm []byte) {
	var peak, sumSquares float64
	for i := 0; i+1 < len(pcm); i += 2 {
		value := float64(int16(binary.LittleEndian.Uint16(pcm[i:i+2]))) / 32768
		if absolute := math.Abs(value); absolute > peak {
			peak = absolute
		}
		sumSquares += value * value
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	s.value.Frames++
	s.value.SessionFrames++
	s.value.Bytes += uint64(len(pcm))
	s.value.SessionBytes += uint64(len(pcm))
	s.value.Samples += uint64(len(pcm) / 2)
	s.value.Peak, s.value.RMS = peak, math.Sqrt(sumSquares/float64(len(pcm)/2))
	s.value.LastFrameAt = time.Now().UTC()
}

func (s *statusTracker) receivedFrame(bytes int) {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.value.ReceivedFrames++
	s.value.ReceivedBytes += uint64(bytes)
}

func (s *statusTracker) droppedFrame(bytes int) {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.value.DroppedFrames++
	s.value.DroppedBytes += uint64(bytes)
}

func writeStatusFile(path string, status receiverStatus) error {
	file, err := os.CreateTemp(filepath.Dir(path), ".audio-share-mic-status-*.tmp")
	if err != nil {
		return err
	}
	temporary := file.Name()
	defer os.Remove(temporary)
	encoder := json.NewEncoder(file)
	encoder.SetIndent("", "  ")
	if err := encoder.Encode(status); err != nil {
		file.Close()
		return err
	}
	if err := file.Close(); err != nil {
		return err
	}
	// Go's Windows implementation uses MoveFileEx(REPLACE_EXISTING). Keeping
	// the temporary file in the same directory provides an atomic replacement.
	return os.Rename(temporary, path)
}

func (s *statusTracker) report(ctx context.Context, path string, logf func(string, ...any)) {
	ticker := time.NewTicker(250 * time.Millisecond)
	defer ticker.Stop()
	lastFailure := ""
	write := func() {
		if err := writeStatusFile(path, s.Snapshot()); err != nil {
			if err.Error() != lastFailure && logf != nil {
				logf("Status file: %v", err)
			}
			lastFailure = err.Error()
		} else {
			lastFailure = ""
		}
	}
	write()
	for {
		select {
		case <-ctx.Done():
			write()
			return
		case <-ticker.C:
			write()
		}
	}
}
