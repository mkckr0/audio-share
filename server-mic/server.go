package main

import (
	"context"
	"errors"
	"fmt"
	"io"
	"net"
	"time"
)

type pcmSink interface {
	WritePCM(context.Context, []byte) error
	Close() error
}

type receiver struct {
	OpenSink    func(audioFormat) (pcmSink, error)
	Status      *statusTracker
	ReadTimeout time.Duration
	Logf        func(string, ...any)
}

func (s *receiver) Serve(ctx context.Context, listener net.Listener) error {
	defer listener.Close()
	stop := context.AfterFunc(ctx, func() { listener.Close() })
	defer stop()
	if s.ReadTimeout <= 0 {
		s.ReadTimeout = 5 * time.Second
	}
	for {
		conn, err := listener.Accept()
		if err != nil {
			if ctx.Err() != nil {
				return nil
			}
			return fmt.Errorf("accept microphone: %w", err)
		}
		if tcp, ok := conn.(*net.TCPConn); ok {
			tcp.SetNoDelay(true)
			tcp.SetReadBuffer(4 * maxFrameBytes)
		}
		err = s.handleConnection(ctx, conn)
		if err != nil && ctx.Err() == nil && s.Logf != nil {
			s.Logf("Microphone disconnected: %v", err)
		}
		var cleanup *outputCleanupError
		if errors.As(err, &cleanup) {
			return err
		}
		if ctx.Err() != nil {
			return nil
		}
	}
}

// Stop accepting if a native output cannot be released. Exiting lets Windows
// reclaim the resources instead of accumulating handles on each reconnect.
type outputCleanupError struct{ err error }

func (e *outputCleanupError) Error() string { return "release audio output: " + e.err.Error() }
func (e *outputCleanupError) Unwrap() error { return e.err }

func (s *receiver) handleConnection(ctx context.Context, conn net.Conn) (err error) {
	defer conn.Close()
	stop := context.AfterFunc(ctx, func() { conn.Close() })
	defer stop()
	var sink pcmSink
	defer func() {
		if ctx.Err() != nil {
			err = nil
		}
		s.Status.disconnected(err)
		if sink != nil {
			if closeErr := sink.Close(); closeErr != nil {
				err = &outputCleanupError{closeErr}
				s.Status.disconnected(err)
			}
		}
	}()
	if err = conn.SetReadDeadline(time.Now().Add(s.ReadTimeout)); err != nil {
		return err
	}
	format, err := readHeader(conn)
	if err != nil {
		return err
	}
	if err = ctx.Err(); err != nil {
		return err
	}
	opened, err := s.OpenSink(format)
	if err != nil {
		return fmt.Errorf("open microphone output: %w", err)
	}
	sink = opened
	if err = conn.SetWriteDeadline(time.Now().Add(s.ReadTimeout)); err != nil {
		return err
	}
	if _, err = io.WriteString(conn, "OKAY"); err != nil {
		return fmt.Errorf("acknowledge microphone: %w", err)
	}
	s.Status.connected(conn.RemoteAddr().String())
	if s.Logf != nil {
		s.Logf("Microphone connected from %s", conn.RemoteAddr())
	}
	return s.streamPCM(ctx, conn, sink)
}

type pcmFrame struct {
	data   [maxFrameBytes]byte
	length int
}

func (s *receiver) streamPCM(parent context.Context, conn net.Conn, sink pcmSink) error {
	ctx, cancel := context.WithCancel(parent)
	stop := context.AfterFunc(ctx, func() { conn.Close() })
	defer stop()
	frames := make(chan pcmFrame, 2)
	readerDone := make(chan struct{})
	var readerError error
	go func() {
		readerError = s.receiveFrames(ctx, conn, frames)
		// EOF cancels a native buffer wait immediately; do not drain stale PCM.
		cancel()
		close(readerDone)
	}()
	defer func() {
		cancel()
		conn.Close()
		<-readerDone
		for len(frames) > 0 {
			s.Status.droppedFrame((<-frames).length)
		}
	}()
	readResult := func() error {
		<-readerDone
		if err := parent.Err(); err != nil {
			return err
		}
		return readerError
	}
	var frame pcmFrame
	for {
		if ctx.Err() != nil {
			return readResult()
		}
		select {
		case <-ctx.Done():
			return readResult()
		case frame = <-frames:
		}
		if ctx.Err() != nil {
			s.Status.droppedFrame(frame.length)
			return readResult()
		}
		if err := sink.WritePCM(ctx, frame.data[:frame.length]); err != nil {
			s.Status.droppedFrame(frame.length)
			if ctx.Err() != nil {
				return readResult()
			}
			return fmt.Errorf("play microphone: %w", err)
		}
		s.Status.addFrame(frame.data[:frame.length])
	}
}

func (s *receiver) receiveFrames(ctx context.Context, conn net.Conn, frames chan pcmFrame) error {
	var frame pcmFrame
	for {
		if err := ctx.Err(); err != nil {
			return err
		}
		if err := conn.SetReadDeadline(time.Now().Add(s.ReadTimeout)); err != nil {
			return err
		}
		pcm, err := readFrame(conn, frame.data[:])
		if err == io.EOF {
			return nil
		}
		if err != nil {
			return fmt.Errorf("receive microphone: %w", err)
		}
		frame.length = len(pcm)
		s.Status.receivedFrame(frame.length)
		select {
		case frames <- frame:
		default:
			// The output can lag without applying TCP/ADB backpressure. Replace
			// only the oldest pending frame; never touch the active native write.
			select {
			case old := <-frames:
				s.Status.droppedFrame(old.length)
			default:
			}
			select {
			case frames <- frame:
			case <-ctx.Done():
				s.Status.droppedFrame(frame.length)
				return ctx.Err()
			}
		}
		if err := conn.SetWriteDeadline(time.Now().Add(min(s.ReadTimeout, time.Second))); err != nil {
			return err
		}
		if _, err := conn.Write([]byte{0x06}); err != nil {
			return fmt.Errorf("acknowledge PCM frame: %w", err)
		}
	}
}
