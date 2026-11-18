package main

import (
	"encoding/binary"
	"fmt"
	"io"
)

const maxFrameBytes = 1920

type audioFormat struct {
	SampleRate    uint32
	Channels      uint32
	BitsPerSample uint32
}

func readHeader(r io.Reader) (audioFormat, error) {
	var header [20]byte
	if _, err := io.ReadFull(r, header[:]); err != nil {
		return audioFormat{}, fmt.Errorf("read microphone header: %w", err)
	}
	if string(header[:8]) != "ASMIC002" {
		return audioFormat{}, fmt.Errorf("unsupported microphone protocol")
	}
	format := audioFormat{
		SampleRate:    binary.LittleEndian.Uint32(header[8:12]),
		Channels:      binary.LittleEndian.Uint32(header[12:16]),
		BitsPerSample: binary.LittleEndian.Uint32(header[16:20]),
	}
	if format != (audioFormat{48000, 1, 16}) {
		return audioFormat{}, fmt.Errorf("unsupported PCM format: %+v; require 48000 Hz, mono, 16-bit", format)
	}
	return format, nil
}

func readFrame(r io.Reader, buffer []byte) ([]byte, error) {
	var prefix [4]byte
	if _, err := io.ReadFull(r, prefix[:]); err != nil {
		return nil, err
	}
	length := binary.LittleEndian.Uint32(prefix[:])
	if length == 0 || length%2 != 0 || length > maxFrameBytes {
		return nil, fmt.Errorf("invalid PCM frame length: %d", length)
	}
	if uint32(len(buffer)) < length {
		return nil, fmt.Errorf("PCM receive buffer too small")
	}
	frame := buffer[:int(length)]
	if _, err := io.ReadFull(r, frame); err != nil {
		if err == io.EOF {
			err = io.ErrUnexpectedEOF
		}
		return nil, fmt.Errorf("read PCM payload: %w", err)
	}
	return frame, nil
}
