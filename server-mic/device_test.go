package main

import "testing"

func TestSelectOutputRequiresUniqueExplicitName(t *testing.T) {
	devices := []outputDevice{{0, "Speakers (Realtek)"}, {1, "CABLE Input (VB-Audio Virtual Ca"}, {2, "CABLE In 16 Ch (VB-Audio Virtual"}}
	device, err := selectOutput(devices, "cable input")
	if err != nil || device.ID != 1 {
		t.Fatalf("selected %+v: %v", device, err)
	}
	for _, query := range []string{"", "missing", "CABLE"} {
		if _, err := selectOutput(devices, query); err == nil {
			t.Errorf("accepted unsafe/ambiguous query %q", query)
		}
	}
	if _, err := selectOutput([]outputDevice{{0, "Speakers (Realtek)"}}, "CABLE Input"); err == nil {
		t.Fatal("fell back to speakers")
	}
}

func TestListenAddressMustBeExplicitLoopback(t *testing.T) {
	for _, address := range []string{"127.0.0.1:65529", "[::1]:65529"} {
		if err := validateListenAddress(address); err != nil {
			t.Errorf("%s: %v", address, err)
		}
	}
	for _, address := range []string{":65529", "0.0.0.0:65529", "192.168.1.2:65529", "localhost:65529", "127.0.0.1:0", "127.0.0.1:70000"} {
		if err := validateListenAddress(address); err == nil {
			t.Errorf("accepted non-loopback/invalid %q", address)
		}
	}
}

func TestSelectOutputRejectsExplicitPhysicalSpeaker(t *testing.T) {
	devices := []outputDevice{{0, "Speakers (Realtek)"}, {1, "CABLE Input (VB-Audio Virtual C"}}
	if _, err := selectOutput(devices, "Speakers (Realtek)"); err == nil {
		t.Fatal("explicit physical speaker selection was accepted")
	}
	if _, err := selectOutput([]outputDevice{{0, "CABLE Input (Realtek)"}}, "CABLE Input"); err == nil {
		t.Fatal("cable-like endpoint without the VB-Audio driver was accepted")
	}
	if _, err := selectOutput([]outputDevice{{0, "Speakers (VB-Audio Virtual Cable"}}, "Speakers"); err != nil {
		t.Fatalf("verified virtual cable alias rejected: %v", err)
	}
}
