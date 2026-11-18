package main

import (
	"fmt"
	"net"
	"strconv"
	"strings"
)

type outputDevice struct {
	ID   uintptr
	Name string
}

func selectOutput(devices []outputDevice, query string) (outputDevice, error) {
	query = strings.ToLower(strings.TrimSpace(query))
	if query == "" {
		return outputDevice{}, fmt.Errorf("an explicit output device name is required")
	}
	var matches []outputDevice
	for _, device := range devices {
		if strings.Contains(strings.ToLower(device.Name), query) {
			matches = append(matches, device)
		}
	}
	if len(matches) == 0 {
		return outputDevice{}, fmt.Errorf("output device %q not found; use --list-devices and select the VB-CABLE playback endpoint", query)
	}
	if len(matches) != 1 {
		return outputDevice{}, fmt.Errorf("output device %q is ambiguous (%d matches); use a more specific name", query, len(matches))
	}
	if !strings.Contains(strings.ToLower(matches[0].Name), "vb-audio virtual") {
		return outputDevice{}, fmt.Errorf("refusing non-VB-CABLE output %q; choose a verified VB-Audio Virtual playback endpoint", matches[0].Name)
	}
	return matches[0], nil
}

func validateListenAddress(address string) error {
	host, portText, err := net.SplitHostPort(address)
	if err != nil {
		return fmt.Errorf("invalid listen address: %w", err)
	}
	ip := net.ParseIP(host)
	if ip == nil || !ip.IsLoopback() {
		return fmt.Errorf("--listen must use an explicit loopback IP address")
	}
	port, err := strconv.Atoi(portText)
	if err != nil || port < 1 || port > 65535 {
		return fmt.Errorf("listen port must be between 1 and 65535")
	}
	return nil
}
