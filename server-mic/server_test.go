package main

import (
	"bytes"
	"context"
	"errors"
	"io"
	"net"
	"sync"
	"sync/atomic"
	"testing"
	"time"
)

// Only the external audio driver is replaced. TCP, deadlines, parsing and
// connection cleanup exercise the production receiver.
type capturedSink struct {
	data         bytes.Buffer
	closed       chan struct{}
	block        bool
	writeStarted chan struct{}
}

func newCapturedSink() *capturedSink { return &capturedSink{closed: make(chan struct{})} }
func (s *capturedSink) WritePCM(ctx context.Context, pcm []byte) error {
	if s.block {
		if s.writeStarted != nil {
			close(s.writeStarted)
		}
		<-ctx.Done()
		return ctx.Err()
	}
	_, err := s.data.Write(pcm)
	return err
}
func (s *capturedSink) Close() error { close(s.closed); return nil }

func startReceiver(t *testing.T, open func(audioFormat) (pcmSink, error), timeout time.Duration) (string, *statusTracker, context.CancelFunc, <-chan error) {
	t.Helper()
	listener, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatal(err)
	}
	ctx, cancel := context.WithCancel(context.Background())
	status := newStatusTracker(listener.Addr().String(), "test cable")
	server := receiver{OpenSink: open, Status: status, ReadTimeout: timeout}
	done := make(chan error, 1)
	go func() { done <- server.Serve(ctx, listener); close(done) }()
	t.Cleanup(func() {
		cancel()
		select {
		case err := <-done:
			if err != nil {
				t.Errorf("receiver exit: %v", err)
			}
		case <-time.After(2 * time.Second):
			t.Error("receiver failed to stop")
		}
	})
	return listener.Addr().String(), status, cancel, done
}

func dialReceiver(t *testing.T, address string) net.Conn {
	t.Helper()
	conn, err := net.DialTimeout("tcp", address, time.Second)
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { conn.Close() })
	conn.SetDeadline(time.Now().Add(2 * time.Second))
	return conn
}

func handshake(t *testing.T, conn net.Conn) {
	t.Helper()
	if _, err := conn.Write(validHeader); err != nil {
		t.Fatal(err)
	}
	var ack [4]byte
	if _, err := io.ReadFull(conn, ack[:]); err != nil {
		t.Fatalf("handshake: %v", err)
	}
	if string(ack[:]) != "OKAY" {
		t.Fatalf("unexpected acknowledgment: %q", ack)
	}
}

func acknowledgeFrames(t *testing.T, conn net.Conn, count int) {
	t.Helper()
	for i := 0; i < count; i++ {
		var ack [1]byte
		if _, err := io.ReadFull(conn, ack[:]); err != nil {
			t.Fatalf("frame ACK %d: %v", i, err)
		}
		if ack[0] != 0x06 {
			t.Fatalf("unexpected frame ACK: %x", ack)
		}
	}
}

func waitForSubmitted(t *testing.T, status *statusTracker, count uint64) {
	t.Helper()
	deadline := time.Now().Add(time.Second)
	for status.Snapshot().Frames < count {
		if time.Now().After(deadline) {
			t.Fatalf("output did not reach %d frames: %+v", count, status.Snapshot())
		}
		time.Sleep(time.Millisecond)
	}
}

func waitClosed(t *testing.T, sink *capturedSink) {
	t.Helper()
	select {
	case <-sink.closed:
	case <-time.After(time.Second):
		t.Fatal("audio output was not closed")
	}
}

func TestReceiverStreamsPCMAndClosesOnCleanDisconnect(t *testing.T) {
	sink := newCapturedSink()
	address, status, _, _ := startReceiver(t, func(f audioFormat) (pcmSink, error) {
		if f != (audioFormat{48000, 1, 16}) {
			return nil, errors.New("wrong output format")
		}
		return sink, nil
	}, time.Second)
	conn := dialReceiver(t, address)
	handshake(t, conn)
	if _, err := conn.Write([]byte{4, 0, 0, 0, 0, 0x80, 0xff, 0x7f, 2, 0, 0, 0, 0x34, 0x12}); err != nil {
		t.Fatal(err)
	}
	acknowledgeFrames(t, conn, 2)
	waitForSubmitted(t, status, 2)
	conn.Close()
	waitClosed(t, sink)
	if !bytes.Equal(sink.data.Bytes(), []byte{0, 0x80, 0xff, 0x7f, 0x34, 0x12}) {
		t.Fatalf("output PCM: %x", sink.data.Bytes())
	}
	state := status.Snapshot()
	if state.Frames != 2 || state.Bytes != 6 || state.Active || state.LastError != "" {
		t.Fatalf("wrong disconnect status: %+v", state)
	}
}

func TestReceiverRejectsTruncatedStreamAndAcceptsNextClient(t *testing.T) {
	outputs := make(chan *capturedSink, 2)
	address, status, _, _ := startReceiver(t, func(audioFormat) (pcmSink, error) {
		s := newCapturedSink()
		outputs <- s
		return s, nil
	}, time.Second)
	first := dialReceiver(t, address)
	handshake(t, first)
	first.Write([]byte{4, 0, 0, 0, 0x12})
	first.Close()
	sink1 := <-outputs
	waitClosed(t, sink1)
	if sink1.data.Len() != 0 {
		t.Fatal("partial PCM reached output")
	}
	// Wait for the next handshake to establish that the first handler completed.
	second := dialReceiver(t, address)
	handshake(t, second)
	if status.Snapshot().LastError == "" {
		t.Fatal("truncated frame was not reported")
	}
	second.Write([]byte{2, 0, 0, 0, 0x34, 0x12})
	acknowledgeFrames(t, second, 1)
	waitForSubmitted(t, status, 1)
	second.Close()
	sink2 := <-outputs
	waitClosed(t, sink2)
	if !bytes.Equal(sink2.data.Bytes(), []byte{0x34, 0x12}) {
		t.Fatalf("next client PCM: %x", sink2.data.Bytes())
	}
}

func TestReceiverDoesNotAcknowledgeBeforeOutputOpens(t *testing.T) {
	opening, release := make(chan struct{}), make(chan struct{})
	var once sync.Once
	t.Cleanup(func() { once.Do(func() { close(release) }) })
	address, _, _, _ := startReceiver(t, func(audioFormat) (pcmSink, error) {
		close(opening)
		<-release
		return newCapturedSink(), nil
	}, time.Second)
	conn := dialReceiver(t, address)
	conn.Write(validHeader)
	select {
	case <-opening:
	case <-time.After(time.Second):
		t.Fatal("output not opened")
	}
	conn.SetReadDeadline(time.Now().Add(50 * time.Millisecond))
	var ack [4]byte
	if n, err := conn.Read(ack[:]); n != 0 || err == nil {
		t.Fatal("acknowledged before output was ready")
	}
	once.Do(func() { close(release) })
	conn.SetReadDeadline(time.Now().Add(time.Second))
	if _, err := io.ReadFull(conn, ack[:]); err != nil || string(ack[:]) != "OKAY" {
		t.Fatalf("ack after open: %q %v", ack, err)
	}
}

func TestReceiverRejectsHeaderWithoutOpeningOutput(t *testing.T) {
	var opened atomic.Int32
	address, _, _, _ := startReceiver(t, func(audioFormat) (pcmSink, error) { opened.Add(1); return newCapturedSink(), nil }, time.Second)
	conn := dialReceiver(t, address)
	bad := append([]byte(nil), validHeader...)
	bad[12] = 2
	conn.Write(bad)
	var ack [4]byte
	if n, err := conn.Read(ack[:]); n != 0 || err == nil {
		t.Fatal("invalid header acknowledged")
	}
	if opened.Load() != 0 {
		t.Fatal("opened output for invalid header")
	}
}

func TestReceiverClosesPeerWhenOutputUnavailable(t *testing.T) {
	address, _, _, _ := startReceiver(t, func(audioFormat) (pcmSink, error) { return nil, errors.New("cable unavailable") }, time.Second)
	for i := 0; i < 2; i++ {
		conn := dialReceiver(t, address)
		conn.Write(validHeader)
		var ack [4]byte
		if n, err := conn.Read(ack[:]); n != 0 || err == nil {
			t.Fatal("unavailable output acknowledged")
		}
		conn.Close()
	}
}

func TestReceiverHandlesTypedNilOutputFailure(t *testing.T) {
	address, _, _, _ := startReceiver(t, func(audioFormat) (pcmSink, error) {
		var unavailable *capturedSink
		return unavailable, errors.New("output open failed")
	}, time.Second)
	conn := dialReceiver(t, address)
	conn.Write(validHeader)
	var ack [4]byte
	if n, err := conn.Read(ack[:]); n != 0 || err == nil {
		t.Fatal("failed output acknowledged")
	}
}

func TestReceiverTimesOutStalledHeader(t *testing.T) {
	address, _, _, _ := startReceiver(t, func(audioFormat) (pcmSink, error) { return newCapturedSink(), nil }, 50*time.Millisecond)
	conn := dialReceiver(t, address)
	var b [1]byte
	if n, err := conn.Read(b[:]); n != 0 || err == nil {
		t.Fatal("stalled header not disconnected")
	}
}

func TestReceiverCancellationUnblocksSocketRead(t *testing.T) {
	sink := newCapturedSink()
	address, _, cancel, done := startReceiver(t, func(audioFormat) (pcmSink, error) { return sink, nil }, time.Hour)
	conn := dialReceiver(t, address)
	handshake(t, conn)
	cancel()
	select {
	case err := <-done:
		if err != nil {
			t.Fatal(err)
		}
	case <-time.After(time.Second):
		t.Fatal("cancellation did not unblock read")
	}
	waitClosed(t, sink)
}

func TestReceiverCancellationUnblocksAudioWrite(t *testing.T) {
	sink := newCapturedSink()
	sink.block = true
	sink.writeStarted = make(chan struct{})
	address, _, cancel, done := startReceiver(t, func(audioFormat) (pcmSink, error) { return sink, nil }, time.Hour)
	conn := dialReceiver(t, address)
	handshake(t, conn)
	conn.Write([]byte{2, 0, 0, 0, 0, 0})
	select {
	case <-sink.writeStarted:
	case <-time.After(time.Second):
		t.Fatal("audio write did not start")
	}
	cancel()
	select {
	case err := <-done:
		if err != nil {
			t.Fatal(err)
		}
	case <-time.After(time.Second):
		t.Fatal("cancellation did not unblock audio")
	}
	waitClosed(t, sink)
}

func TestReceiverEOFInterruptsBlockedOutput(t *testing.T) {
	sink := newCapturedSink()
	sink.block = true
	sink.writeStarted = make(chan struct{})
	address, _, _, _ := startReceiver(t, func(audioFormat) (pcmSink, error) { return sink, nil }, time.Hour)
	conn := dialReceiver(t, address)
	handshake(t, conn)
	conn.Write([]byte{2, 0, 0, 0, 0, 0})
	acknowledgeFrames(t, conn, 1)
	select {
	case <-sink.writeStarted:
	case <-time.After(time.Second):
		t.Fatal("audio write did not start")
	}
	conn.Close()
	select {
	case <-sink.closed:
	case <-time.After(200 * time.Millisecond):
		t.Fatal("EOF did not cancel blocked playback promptly")
	}
}

type gatedSink struct {
	data    bytes.Buffer
	started chan struct{}
	release chan struct{}
	closed  chan struct{}
	first   bool
}

func (s *gatedSink) WritePCM(ctx context.Context, pcm []byte) error {
	if !s.first {
		s.first = true
		close(s.started)
		select {
		case <-ctx.Done():
			return ctx.Err()
		case <-s.release:
		}
	}
	_, err := s.data.Write(pcm)
	return err
}
func (s *gatedSink) Close() error { close(s.closed); return nil }

func TestReceiverAcknowledgesAheadOfSlowOutputAndKeepsLatestFrames(t *testing.T) {
	sink := &gatedSink{started: make(chan struct{}), release: make(chan struct{}), closed: make(chan struct{})}
	address, status, _, _ := startReceiver(t, func(audioFormat) (pcmSink, error) { return sink, nil }, time.Second)
	conn := dialReceiver(t, address)
	handshake(t, conn)
	conn.Write([]byte{2, 0, 0, 0, 1, 0})
	acknowledgeFrames(t, conn, 1)
	select {
	case <-sink.started:
	case <-time.After(time.Second):
		t.Fatal("output did not start")
	}
	for _, sample := range []byte{2, 3, 4, 5} {
		conn.Write([]byte{2, 0, 0, 0, sample, 0})
		acknowledgeFrames(t, conn, 1)
	}
	state := status.Snapshot()
	if state.ReceivedFrames != 5 || state.ReceivedBytes != 10 || state.DroppedFrames != 2 || state.DroppedBytes != 4 {
		t.Fatalf("slow output did not retain just the two newest pending frames: %+v", state)
	}
	close(sink.release)
	waitForSubmitted(t, status, 3)
	conn.Close()
	select {
	case <-sink.closed:
	case <-time.After(time.Second):
		t.Fatal("output not closed")
	}
	if !bytes.Equal(sink.data.Bytes(), []byte{1, 0, 4, 0, 5, 0}) {
		t.Fatalf("played stale pending PCM: %x", sink.data.Bytes())
	}
}

func TestReceiverDoesNotAcknowledgeMalformedFrame(t *testing.T) {
	address, _, _, _ := startReceiver(t, func(audioFormat) (pcmSink, error) { return newCapturedSink(), nil }, time.Second)
	conn := dialReceiver(t, address)
	handshake(t, conn)
	conn.Write([]byte{3, 0, 0, 0, 1, 2, 3})
	var ack [1]byte
	if n, err := conn.Read(ack[:]); n != 0 || err == nil {
		t.Fatal("malformed frame acknowledged")
	}
}
