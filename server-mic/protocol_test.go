package main

import (
	"bytes"
	"errors"
	"io"
	"testing"
)

// A literal wire fixture, independent of the production parser and Android code.
var validHeader = []byte{'A', 'S', 'M', 'I', 'C', '0', '0', '2', 0x80, 0xbb, 0, 0, 1, 0, 0, 0, 16, 0, 0, 0}

func TestReadHeaderAcceptsWireFixture(t *testing.T) {
	f, err := readHeader(bytes.NewReader(validHeader))
	if err != nil {
		t.Fatalf("valid fixed-format header rejected: %v", err)
	}
	if f.SampleRate != 48000 || f.Channels != 1 || f.BitsPerSample != 16 {
		t.Fatalf("decoded wrong format: %+v", f)
	}
}

func TestReadHeaderRejectsOtherFormats(t *testing.T) {
	for _, tc := range []struct {
		name  string
		index int
		value byte
	}{
		{"magic", 0, 'B'}, {"legacy version", 7, '1'}, {"rate", 8, 0x44},
		{"channels", 12, 2}, {"bits", 16, 24}, {"wide channels", 15, 1},
	} {
		t.Run(tc.name, func(t *testing.T) {
			input := append([]byte(nil), validHeader...)
			input[tc.index] = tc.value
			if _, err := readHeader(bytes.NewReader(input)); err == nil {
				t.Fatal("invalid header accepted")
			}
		})
	}
}

func TestReadHeaderRejectsTruncation(t *testing.T) {
	for n := 0; n < len(validHeader); n++ {
		if _, err := readHeader(bytes.NewReader(validHeader[:n])); err == nil {
			t.Fatalf("accepted %d-byte header", n)
		}
	}
}

func TestReadFrameReturnsExactPCMAndReusesBuffer(t *testing.T) {
	storage := make([]byte, 1920)
	frame, err := readFrame(bytes.NewReader([]byte{6, 0, 0, 0, 0, 0x80, 0xff, 0x7f, 0, 0}), storage)
	if err != nil {
		t.Fatal(err)
	}
	if !bytes.Equal(frame, []byte{0, 0x80, 0xff, 0x7f, 0, 0}) {
		t.Fatalf("wrong PCM: %x", frame)
	}
	if &frame[0] != &storage[0] {
		t.Fatal("frame allocated a new buffer")
	}
}

func TestReadFrameAcceptsFullBlock(t *testing.T) {
	input := append([]byte{0x80, 7, 0, 0}, bytes.Repeat([]byte{0x12, 0x34}, 960)...)
	frame, err := readFrame(bytes.NewReader(input), make([]byte, 1920))
	if err != nil || len(frame) != 1920 || frame[1919] != 0x34 {
		t.Fatalf("full block: len=%d, err=%v", len(frame), err)
	}
}

func TestReadFrameRejectsLengthsBeforeReadingPayload(t *testing.T) {
	for _, length := range [][]byte{{0, 0, 0, 0}, {1, 0, 0, 0}, {3, 0, 0, 0}, {0x82, 7, 0, 0}, {0xff, 0xff, 0xff, 0xff}} {
		input := bytes.NewReader(append(append([]byte(nil), length...), 0xde, 0xad))
		if _, err := readFrame(input, make([]byte, 1920)); err == nil {
			t.Errorf("accepted length %x", length)
		}
		if input.Len() != 2 {
			t.Errorf("read payload for invalid length %x", length)
		}
	}
}

func TestReadFrameDistinguishesDisconnectFromTruncation(t *testing.T) {
	for _, tc := range []struct {
		name  string
		input []byte
		want  error
	}{
		{"between frames", nil, io.EOF},
		{"partial length", []byte{2, 0}, io.ErrUnexpectedEOF},
		{"no payload", []byte{2, 0, 0, 0}, io.ErrUnexpectedEOF},
		{"partial payload", []byte{2, 0, 0, 0, 0x12}, io.ErrUnexpectedEOF},
	} {
		t.Run(tc.name, func(t *testing.T) {
			_, err := readFrame(bytes.NewReader(tc.input), make([]byte, 1920))
			if !errors.Is(err, tc.want) {
				t.Fatalf("got %v, want %v", err, tc.want)
			}
		})
	}
}
