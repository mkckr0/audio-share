//go:build windows

package main

import (
	"fmt"
	"os"
	"sync/atomic"
	"testing"
)

func TestMicrophoneRoutePublishesEveryTransition(t *testing.T) {
	if name := routeName("test"); name != `Local\AudioShare.Microphone.v1.f9e6e6ef197c2b25` {
		t.Fatalf("route name differs from the speaker server: %q", name)
	}
	// Keep a view open like the speaker server, including across a reconnect.
	id := fmt.Sprintf("test-%d", os.Getpid())
	reader, err := openRouteState(id)
	if err != nil {
		t.Fatal(err)
	}
	defer reader.close()
	read := func() uint64 { return atomic.LoadUint64(reader.value) }
	idle := read()
	for i := 0; i < 2; i++ {
		writer, err := claimMicrophoneRoute(id)
		if err != nil {
			t.Fatal(err)
		}
		active := read()
		if active == idle || uint32(active) != uint32(os.Getpid()) {
			t.Fatalf("receiver was not published: %#x", active)
		}
		if duplicate, err := claimMicrophoneRoute(id); err == nil {
			duplicate.close()
			t.Fatal("two receivers can render into one cable without exclusion")
		}
		writer.close()
		if next := read(); next == idle || uint32(next) != 0 {
			t.Fatalf("short-lived session went undetected: before=%#x after=%#x", idle, next)
		} else {
			idle = next
		}
	}
}
