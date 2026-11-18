package main

import (
	"encoding/json"
	"math"
	"os"
	"path/filepath"
	"testing"
)

func TestStatusMeasuresSignedPCMAndClearsLevelsOnStop(t *testing.T) {
	status := newStatusTracker("127.0.0.1:65529", "CABLE Input")
	status.connected("127.0.0.1:12345")
	status.addFrame([]byte{0, 0x80, 0, 0})
	snapshot := status.Snapshot()
	if !snapshot.Active || snapshot.Frames != 1 || snapshot.Bytes != 4 || snapshot.Samples != 2 {
		t.Fatalf("wrong stream counters: %+v", snapshot)
	}
	if snapshot.Peak != 1 || math.Abs(snapshot.RMS-0.7071067811865476) > 1e-10 {
		t.Fatalf("wrong signed sample levels: peak %g RMS %g", snapshot.Peak, snapshot.RMS)
	}
	status.disconnected(nil)
	snapshot = status.Snapshot()
	if snapshot.Active || snapshot.Peak != 0 || snapshot.RMS != 0 || snapshot.Frames != 1 {
		t.Fatalf("wrong stopped levels/counters: %+v", snapshot)
	}
	status.connected("127.0.0.1:23456")
	snapshot = status.Snapshot()
	if snapshot.SessionFrames != 0 || snapshot.Connections != 2 || snapshot.Frames != 1 {
		t.Fatalf("wrong reconnect counters: %+v", snapshot)
	}
}

func TestStatusFileReplacesPreviousCompleteJSON(t *testing.T) {
	path := filepath.Join(t.TempDir(), "status.json")
	status := newStatusTracker("127.0.0.1:65529", "CABLE Input")
	if err := writeStatusFile(path, status.Snapshot()); err != nil {
		t.Fatal(err)
	}
	status.connected("127.0.0.1:12345")
	status.addFrame([]byte{0, 0x40})
	if err := writeStatusFile(path, status.Snapshot()); err != nil {
		t.Fatal(err)
	}
	data, err := os.ReadFile(path)
	if err != nil {
		t.Fatal(err)
	}
	var decoded map[string]any
	if err := json.Unmarshal(data, &decoded); err != nil {
		t.Fatalf("invalid status JSON: %v", err)
	}
	if decoded["active"] != true || decoded["frames"] != float64(1) || decoded["bytes"] != float64(2) || decoded["peak"] != 0.5 {
		t.Fatalf("status not replaced: %s", data)
	}
	if _, ok := decoded["pcm"]; ok {
		t.Fatal("status contains audio")
	}
	files, err := os.ReadDir(filepath.Dir(path))
	if err != nil {
		t.Fatal(err)
	}
	if len(files) != 1 {
		t.Fatalf("left temporary files: %v", files)
	}
}
