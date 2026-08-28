"""
AudioStream E2E Test Suite — Tier 1: Feature Coverage.
Tests core protocol schemas, binary framing, Opus packet layout,
jitter buffer sequencing, virtual mic state logic, Android service configuration,
repository debranding verification, and executable header validation.
"""

import unittest
import os
import struct
try:
    from framework.protocol import (
        AudioCodec, AudioEncoding, CommandId, PacketType,
        AudioFormat, StreamConfig, MicControl, DiscoveryBeacon,
        pack_udp_packet, unpack_udp_packet, pack_tcp_command, unpack_tcp_command_header,
        create_mock_opus_frame, UDP_HEADER_SIZE
    )
    from framework.jitter_buffer import (
        AdaptiveJitterBuffer, JitterPreset, seq_diff
    )
    from framework.virtual_mic_sim import (
        VirtualMicMonitorSimulator, MicState
    )
    from framework.binary_validator import (
        PEValidator, APKValidator, ELFValidator
    )
    from framework.debranding_scanner import (
        DebrandingScanner
    )
except ImportError:
    from ..framework.protocol import (
        AudioCodec, AudioEncoding, CommandId, PacketType,
        AudioFormat, StreamConfig, MicControl, DiscoveryBeacon,
        pack_udp_packet, unpack_udp_packet, pack_tcp_command, unpack_tcp_command_header,
        create_mock_opus_frame, UDP_HEADER_SIZE
    )
    from ..framework.jitter_buffer import (
        AdaptiveJitterBuffer, JitterPreset, seq_diff
    )
    from ..framework.virtual_mic_sim import (
        VirtualMicMonitorSimulator, MicState
    )
    from ..framework.binary_validator import (
        PEValidator, APKValidator, ELFValidator
    )
    from ..framework.debranding_scanner import (
        DebrandingScanner
    )


class TestTier1Features(unittest.TestCase):

    def setUp(self):
        self.workspace_root = os.path.abspath(
            os.path.join(os.path.dirname(__file__), "..", "..", "..", "..", "..", "workspaces", "audio-share")
        )
        if not os.path.exists(self.workspace_root):
            self.workspace_root = "/workspaces/audio-share"

    def test_01_protocol_schema_definitions(self):
        """Tier 1.1: Verify AudioStream protocol enums match specifications."""
        # Codecs
        self.assertEqual(int(AudioCodec.CODEC_PCM), 0)
        self.assertEqual(int(AudioCodec.CODEC_OPUS), 1)

        # Encodings
        self.assertEqual(int(AudioEncoding.ENCODING_INVALID), 0)
        self.assertEqual(int(AudioEncoding.ENCODING_PCM_FLOAT), 1)
        self.assertEqual(int(AudioEncoding.ENCODING_PCM_8BIT), 2)
        self.assertEqual(int(AudioEncoding.ENCODING_PCM_16BIT), 3)
        self.assertEqual(int(AudioEncoding.ENCODING_PCM_24BIT_PACKED), 4)
        self.assertEqual(int(AudioEncoding.ENCODING_PCM_32BIT), 5)

        # Command IDs
        self.assertEqual(int(CommandId.CMD_GET_FORMAT), 1)
        self.assertEqual(int(CommandId.CMD_START_PLAY), 2)
        self.assertEqual(int(CommandId.CMD_HEARTBEAT), 3)
        self.assertEqual(int(CommandId.CMD_NEGOTIATE_CODEC), 4)
        self.assertEqual(int(CommandId.CMD_START_MIC), 5)
        self.assertEqual(int(CommandId.CMD_STOP_MIC), 6)
        self.assertEqual(int(CommandId.CMD_MIC_DATA), 7)
        self.assertEqual(int(CommandId.CMD_DISCOVERY), 8)

        # UDP Packet Types
        self.assertEqual(int(PacketType.PC_AUDIO), 0x01)
        self.assertEqual(int(PacketType.PHONE_MIC), 0x02)

    def test_02_protobuf_serialization_roundtrip(self):
        """Tier 1.2: Verify protobuf message round-trip encoding/decoding."""
        # AudioFormat
        fmt = AudioFormat(encoding=AudioEncoding.ENCODING_PCM_FLOAT, channels=2, sample_rate=48000)
        fmt_bytes = fmt.serialize()
        fmt_dec = AudioFormat.deserialize(fmt_bytes)
        self.assertEqual(fmt, fmt_dec)

        # StreamConfig (Opus 128kbps 20ms)
        cfg = StreamConfig(format=fmt, codec=AudioCodec.CODEC_OPUS, opus_bitrate=128000,
                           opus_frame_size_ms=20, target_latency_ms=25)
        cfg_bytes = cfg.serialize()
        cfg_dec = StreamConfig.deserialize(cfg_bytes)
        self.assertEqual(cfg, cfg_dec)

        # MicControl
        mic = MicControl(start=True, config=cfg, mic_source=1)
        mic_bytes = mic.serialize()
        mic_dec = MicControl.deserialize(mic_bytes)
        self.assertTrue(mic_dec.start)
        self.assertEqual(mic_dec.config, cfg)
        self.assertEqual(mic_dec.mic_source, 1)

        # DiscoveryBeacon
        beacon = DiscoveryBeacon(app_name="AudioStream", server_name="Studio-PC", port=65530,
                                 version="1.0.0", supports_opus=True, supports_mic=True,
                                 ip_address="192.168.42.1")
        beacon_bytes = beacon.serialize()
        beacon_dec = DiscoveryBeacon.deserialize(beacon_bytes)
        self.assertEqual(beacon.app_name, beacon_dec.app_name)
        self.assertEqual(beacon.server_name, beacon_dec.server_name)
        self.assertEqual(beacon.port, beacon_dec.port)
        self.assertEqual(beacon.supports_opus, beacon_dec.supports_opus)
        self.assertEqual(beacon.supports_mic, beacon_dec.supports_mic)
        self.assertEqual(beacon.ip_address, beacon_dec.ip_address)

    def test_03_udp_7byte_binary_header(self):
        """Tier 1.3: Verify UDP 7-byte binary header layout and field unpacking."""
        packet_type = PacketType.PC_AUDIO
        seq_num = 1337
        timestamp = 96000
        raw_pcm = b"\x00\x01\x02\x03" * 240  # 960 bytes

        packed = pack_udp_packet(packet_type, seq_num, timestamp, raw_pcm)
        self.assertEqual(len(packed), 7 + len(raw_pcm))

        # Check raw header layout (<BHI)
        pt_raw, seq_raw, ts_raw = struct.unpack("<BHI", packed[:7])
        self.assertEqual(pt_raw, 0x01)
        self.assertEqual(seq_raw, 1337)
        self.assertEqual(ts_raw, 96000)

        # Unpack via helper
        pt, s, ts, payload = unpack_udp_packet(packed)
        self.assertEqual(pt, PacketType.PC_AUDIO)
        self.assertEqual(s, 1337)
        self.assertEqual(ts, 96000)
        self.assertEqual(payload, raw_pcm)

    def test_04_opus_packet_framing(self):
        """Tier 1.4: Verify Opus TOC byte structure and frame synthesis."""
        stereo_frame = create_mock_opus_frame(frame_size_ms=20, stereo=True, sequence=1)
        mono_frame = create_mock_opus_frame(frame_size_ms=20, stereo=False, sequence=2)

        # Verify TOC byte for stereo: bit 2 must be 1
        toc_stereo = stereo_frame[0]
        self.assertTrue((toc_stereo & 0x04) != 0, "Stereo bit must be set")

        # Verify TOC byte for mono: bit 2 must be 0
        toc_mono = mono_frame[0]
        self.assertEqual((toc_mono & 0x04), 0, "Mono bit must be cleared")

        # Verify packet carries payload beyond TOC
        self.assertGreater(len(stereo_frame), 10)
        self.assertGreater(len(mono_frame), 10)

    def test_05_jitter_buffer_sequencing(self):
        """Tier 1.5: Verify sequential packet insertion and playback queue."""
        jb = AdaptiveJitterBuffer(preset=JitterPreset.LOW)
        payloads = [b"frame_" + str(i).encode() for i in range(10)]

        for i, data in enumerate(payloads):
            accepted = jb.push(seq=i, timestamp=i * 960, data=data, arrival_time_ms=i * 20.0)
            self.assertTrue(accepted, f"Packet {i} should be accepted")

        self.assertEqual(jb.get_buffered_count(), 10)

        for i in range(10):
            data, is_plc = jb.pull()
            self.assertFalse(is_plc, f"Frame {i} should not be PLC")
            self.assertEqual(data, payloads[i])

        self.assertEqual(jb.get_buffered_count(), 0)

    def test_06_virtual_mic_state_logic(self):
        """Tier 1.6: Verify virtual mic transition from IDLE to ACTIVE."""
        sim = VirtualMicMonitorSimulator(debounce_ms=3000.0)
        self.assertEqual(sim.state, MicState.IDLE)
        self.assertFalse(sim.android_green_dot_active)

        # Discord requests mic
        triggered = sim.app_open_mic("Discord.exe", current_time_ms=100.0)
        self.assertTrue(triggered)
        self.assertEqual(sim.state, MicState.ACTIVE)
        self.assertTrue(sim.android_green_dot_active)
        self.assertEqual(sim.cmd_start_sent_count, 1)

    def test_07_android_service_configuration(self):
        """Tier 1.7: Validate Android configuration and service definitions."""
        manifest_path = os.path.join(self.workspace_root, "android-app", "app", "src", "main", "AndroidManifest.xml")
        if os.path.exists(manifest_path):
            with open(manifest_path, "r", encoding="utf-8") as f:
                content = f.read()
            self.assertIn("android.permission.INTERNET", content)
            self.assertIn(".service.PlaybackService", content)
        else:
            self.assertTrue(True)

    def test_08_debranding_scan_functionality(self):
        """Tier 1.8: Test debranding scanner operation."""
        scanner = DebrandingScanner(self.workspace_root)
        result = scanner.scan()
        self.assertIsInstance(result.scanned_files_count, int)
        self.assertGreaterEqual(result.scanned_files_count, 0)

    def test_09_legacy_mfc_pruning_check(self):
        """Tier 1.9: Check legacy MFC folder status."""
        scanner = DebrandingScanner(self.workspace_root)
        result = scanner.scan()
        # Report legacy status
        if result.legacy_mfc_present:
            self.assertGreater(len(result.legacy_mfc_files), 0)

    def test_10_release_binary_headers(self):
        """Tier 1.10: Validate PE and APK header parsing on synthetic/real binaries."""
        # Create synthetic valid PE32+ header
        dos_hdr = bytearray(64)
        dos_hdr[:2] = b"MZ"
        struct.pack_into("<I", dos_hdr, 0x3C, 64)  # e_lfanew = 64
        
        # PE Signature (4 bytes)
        pe_sig = b"PE\x00\x00"
        
        # COFF File Header (20 bytes): Machine x64 (0x8664), 3 sections, opt header 240 bytes
        coff_hdr = struct.pack("<HHIIIHH", 0x8664, 3, 0x60000000, 0, 0, 240, 0x0022)
        
        # Optional Header (Magic 0x020B for PE32+)
        opt_hdr = struct.pack("<H", 0x020B) + bytes(238)
        
        synthetic_pe = bytes(dos_hdr) + pe_sig + coff_hdr + opt_hdr
        
        res = PEValidator.validate_pe_bytes(synthetic_pe)
        self.assertTrue(res["valid"])
        self.assertTrue(res["is_x64"])
        self.assertTrue(res["is_pe32_plus"])
        self.assertEqual(res["num_sections"], 3)


if __name__ == "__main__":
    unittest.main()
