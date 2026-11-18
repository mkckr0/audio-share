//go:build windows

package main

import (
	"bytes"
	"context"
	"encoding/json"
	"os"
	"path/filepath"
	"testing"
)

func TestCLIRejectsNonLoopbackAndWritesFailureStatus(t *testing.T) {
	path := filepath.Join(t.TempDir(), "status.json")
	var output bytes.Buffer
	err := run(context.Background(), []string{"--listen", "0.0.0.0:65529", "--status-file", path}, &output, &output)
	if err == nil {
		t.Fatal("non-loopback listener accepted")
	}
	data, err := os.ReadFile(path)
	if err != nil {
		t.Fatal(err)
	}
	var status receiverStatus
	if err := json.Unmarshal(data, &status); err != nil {
		t.Fatal(err)
	}
	if status.State != "error" || status.Active || status.LastError == "" {
		t.Fatalf("wrong startup error status: %s", data)
	}
}
