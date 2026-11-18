//go:build windows

package main

import (
	"context"
	"errors"
	"flag"
	"fmt"
	"io"
	"log"
	"net"
	"os"
	"os/signal"
	"time"
)

func main() {
	ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt)
	defer stop()
	if err := run(ctx, os.Args[1:], os.Stdout, os.Stderr); err != nil && !errors.Is(err, flag.ErrHelp) {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
}

func run(parent context.Context, args []string, output, errorOutput io.Writer) (returnErr error) {
	flags := flag.NewFlagSet("audio-share-mic", flag.ContinueOnError)
	flags.SetOutput(errorOutput)
	listen := flags.String("listen", "127.0.0.1:65529", "loopback TCP listen address")
	deviceQuery := flags.String("device", "CABLE Input", "unique substring of the VB-CABLE playback device name")
	list := flags.Bool("list-devices", false, "list Windows playback device names")
	statusPath := flags.String("status-file", "", "atomically replace this JSON status file every 250 ms")
	stopPath := flags.String("stop-file", "", "stop gracefully when this sentinel file exists (checked every 250 ms)")
	if err := flags.Parse(args); err != nil {
		return err
	}
	if flags.NArg() != 0 {
		return fmt.Errorf("unexpected positional arguments: %v", flags.Args())
	}
	if *list {
		devices, err := listOutputDevices()
		if err != nil {
			return err
		}
		for _, device := range devices {
			fmt.Fprintf(output, "%d\t%s\n", device.ID, device.Name)
		}
		return nil
	}
	logger := log.New(errorOutput, "", log.LstdFlags)
	status := newStatusTracker(*listen, *deviceQuery)
	reportCtx, reportCancel := context.WithCancel(context.Background())
	defer reportCancel()
	var reportDone chan struct{}
	defer func() {
		status.stopped(returnErr)
		if reportDone != nil {
			reportCancel()
			<-reportDone
		} else if *statusPath != "" {
			if err := writeStatusFile(*statusPath, status.Snapshot()); err != nil {
				logger.Printf("Final status file: %v", err)
			}
		}
	}()
	if err := validateListenAddress(*listen); err != nil {
		return err
	}
	devices, err := listOutputDevices()
	if err != nil {
		return err
	}
	selected, err := selectOutput(devices, *deviceQuery)
	if err != nil {
		return err
	}
	status.value.Device = selected.Name
	listener, err := net.Listen("tcp", *listen)
	if err != nil {
		return fmt.Errorf("listen: %w", err)
	}
	defer listener.Close()
	ctx, cancel := context.WithCancel(parent)
	defer cancel()
	if *statusPath != "" {
		if err := writeStatusFile(*statusPath, status.Snapshot()); err != nil {
			return fmt.Errorf("write initial status: %w", err)
		}
		reportDone = make(chan struct{})
		go func() { defer close(reportDone); status.report(reportCtx, *statusPath, logger.Printf) }()
	}
	stopError := make(chan error, 1)
	if *stopPath != "" {
		go func() {
			err := waitForStopFile(ctx, *stopPath, 250*time.Millisecond)
			if err != nil && ctx.Err() != nil {
				return
			}
			stopError <- err
			cancel()
		}()
	}
	server := receiver{
		Status: status, ReadTimeout: 5 * time.Second, Logf: logger.Printf,
		OpenSink: func(format audioFormat) (pcmSink, error) {
			// Device indices can change after a driver reconnect. Resolve the
			// explicit name again for every session, never use WAVE_MAPPER.
			devices, err := listOutputDevices()
			if err != nil {
				return nil, err
			}
			device, err := selectOutput(devices, *deviceQuery)
			if err != nil {
				return nil, err
			}
			status.mu.Lock()
			status.value.Device = device.Name
			status.mu.Unlock()
			return openWaveSink(device, format)
		},
	}
	logger.Printf("Audio Share microphone listening on %s -> %s", *listen, selected.Name)
	err = server.Serve(ctx, listener)
	select {
	case stopErr := <-stopError:
		if stopErr != nil {
			return stopErr
		}
	default:
	}
	return err
}
