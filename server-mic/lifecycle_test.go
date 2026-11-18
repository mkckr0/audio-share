package main

import (
	"context"
	"errors"
	"os"
	"path/filepath"
	"testing"
	"time"
)

func TestStopFileWakesReceiverShutdown(t *testing.T) {
	path := filepath.Join(t.TempDir(), "stop")
	ctx, cancel := context.WithTimeout(context.Background(), time.Second)
	defer cancel()
	done := make(chan error, 1)
	go func() { done <- waitForStopFile(ctx, path, 10*time.Millisecond) }()
	select {
	case err := <-done:
		t.Fatalf("stopped before sentinel: %v", err)
	case <-time.After(20 * time.Millisecond):
	}
	if err := os.WriteFile(path, nil, 0600); err != nil {
		t.Fatal(err)
	}
	select {
	case err := <-done:
		if err != nil {
			t.Fatal(err)
		}
	case <-ctx.Done():
		t.Fatal("stop sentinel ignored")
	}
}

func TestStopFileWatcherCancelsWithoutSentinel(t *testing.T) {
	ctx, cancel := context.WithCancel(context.Background())
	cancel()
	err := waitForStopFile(ctx, filepath.Join(t.TempDir(), "stop"), time.Second)
	if !errors.Is(err, context.Canceled) {
		t.Fatalf("got %v", err)
	}
}
