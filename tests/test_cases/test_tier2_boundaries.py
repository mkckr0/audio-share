"""
AudioStream E2E Test Suite — Tier 2: Boundary & Corner Cases.
Tests:
1. Sequence number 16-bit wrap-around (65535 -> 0 -> 1)
2. Out-of-order UDP packet reassembly and duplicate rejection
3. Packet Loss Concealment (PLC) triggering on sequence gaps
4. Invalid port and IP boundaries / tethering subnet classification
5. Empty audio frames, zero-length payloads, and truncated headers (< 7 bytes)
6. MTU payload clamping and segment size boundaries
"""

import unittest
import ipaddress
try:
    from framework.protocol import (
        PacketType, pack_udp_packet, unpack_udp_packet, UDP_HEADER_SIZE
    )
    from framework.jitter_buffer import (
        AdaptiveJitterBuffer, JitterPreset, seq_diff
    )
except ImportError:
    from ..framework.protocol import (
        PacketType, pack_udp_packet, unpack_udp_packet, UDP_HEADER_SIZE
    )
    from ..framework.jitter_buffer import (
        AdaptiveJitterBuffer, JitterPreset, seq_diff
    )


class TestTier2Boundaries(unittest.TestCase):

    def test_01_sequence_number_wrap_around(self):
        """Tier 2.1: Verify uint16 sequence number wrap-around (65535 -> 0)."""
        # Test modular sequence arithmetic
        self.assertEqual(seq_diff(0, 65535), 1)
        self.assertEqual(seq_diff(1, 65535), 2)
        self.assertEqual(seq_diff(65535, 0), -1)
        self.assertEqual(seq_diff(65534, 65535), -1)
        self.assertEqual(seq_diff(10, 65530), 16)

        # Test Jitter Buffer through wrap-around boundary
        jb = AdaptiveJitterBuffer(preset=JitterPreset.LOW)
        test_seqs = [65533, 65534, 65535, 0, 1, 2, 3]

        for s in test_seqs:
            accepted = jb.push(seq=s, timestamp=s * 960, data=f"payload_{s}".encode())
            self.assertTrue(accepted, f"Sequence {s} should be accepted during wrap-around")

        # Pull sequentially
        pulled_seqs = []
        for _ in test_seqs:
            data, is_plc = jb.pull()
            self.assertFalse(is_plc)
            self.assertIsNotNone(data)
            pulled_seqs.append(int(data.decode().split("_")[1]))

        self.assertEqual(pulled_seqs, test_seqs)

    def test_02_out_of_order_packet_reassembly(self):
        """Tier 2.2: Verify out-of-order UDP packet reordering."""
        jb = AdaptiveJitterBuffer(preset=JitterPreset.LOW)
        
        # Ingestion order: scrambled
        arrival_order = [100, 103, 101, 104, 102, 105]
        for s in arrival_order:
            jb.push(seq=s, timestamp=s * 960, data=f"data_{s}".encode())

        # Expected pull order: monotonically increasing
        expected_order = [100, 101, 102, 103, 104, 105]
        pulled = []
        for _ in expected_order:
            data, is_plc = jb.pull()
            self.assertFalse(is_plc)
            self.assertIsNotNone(data)
            pulled.append(int(data.decode().split("_")[1]))

        self.assertEqual(pulled, expected_order)
        self.assertGreater(jb.stats.packets_reordered, 0)

    def test_03_duplicate_packet_rejection(self):
        """Tier 2.3: Verify duplicate packets are discarded."""
        jb = AdaptiveJitterBuffer(preset=JitterPreset.LOW)
        
        acc1 = jb.push(seq=200, timestamp=2000, data=b"original")
        self.assertTrue(acc1)

        # Duplicate injection
        acc2 = jb.push(seq=200, timestamp=2000, data=b"duplicate")
        self.assertFalse(acc2, "Duplicate packet must be rejected")
        self.assertEqual(jb.stats.duplicates, 1)

    def test_04_packet_loss_concealment_simulation(self):
        """Tier 2.4: Verify PLC frame generation on sequence gap."""
        jb = AdaptiveJitterBuffer(preset=JitterPreset.LOW)
        
        # Push 300, 301, [missing 302], 303, 304
        jb.push(seq=300, timestamp=3000, data=b"p300")
        jb.push(seq=301, timestamp=3010, data=b"p301")
        # 302 is lost!
        jb.push(seq=303, timestamp=3030, data=b"p303")
        jb.push(seq=304, timestamp=3040, data=b"p304")

        # Pull 300
        d0, plc0 = jb.pull()
        self.assertEqual(d0, b"p300")
        self.assertFalse(plc0)

        # Pull 301
        d1, plc1 = jb.pull()
        self.assertEqual(d1, b"p301")
        self.assertFalse(plc1)

        # Pull 302 (Missing -> PLC frame expected)
        d2, plc2 = jb.pull()
        self.assertTrue(plc2, "Missing packet 302 must trigger PLC")
        self.assertIsNotNone(d2)
        self.assertEqual(jb.stats.plc_frames, 1)
        self.assertEqual(jb.stats.packets_lost, 1)

        # Pull 303 (Normal resumption)
        d3, plc3 = jb.pull()
        self.assertEqual(d3, b"p303")
        self.assertFalse(plc3)

    def test_05_invalid_port_and_ip_handling(self):
        """Tier 2.5: Verify boundary handling for network ports and tethering subnets."""
        # Port boundary tests
        valid_ports = [1, 80, 59200, 65530, 65535]
        invalid_ports = [0, -1, 65536, 70000, "invalid"]

        for p in valid_ports:
            self.assertTrue(0 < p <= 65535)

        for p in invalid_ports:
            is_valid = isinstance(p, int) and (1 <= p <= 65535)
            self.assertFalse(is_valid, f"Port {p} should be invalid")

        # Tethering subnet verification
        tether_subnets = ["192.168.42.0/24", "192.168.137.0/24"]
        tether_ips = ["192.168.42.1", "192.168.42.129", "192.168.137.1"]
        non_tether_ips = ["10.0.0.5", "172.16.0.1", "8.8.8.8"]

        def is_tether_ip(ip_str: str) -> bool:
            try:
                addr = ipaddress.ip_address(ip_str)
                for subnet_str in tether_subnets:
                    if addr in ipaddress.ip_network(subnet_str):
                        return True
            except ValueError:
                pass
            return False

        for ip in tether_ips:
            self.assertTrue(is_tether_ip(ip), f"{ip} should match tethering subnets")

        for ip in non_tether_ips:
            self.assertFalse(is_tether_ip(ip), f"{ip} should not match tethering subnets")

    def test_06_empty_audio_frames_and_truncated_headers(self):
        """Tier 2.6: Verify 0-byte payload handling and truncated header rejection."""
        # Valid 7-byte header with 0-byte audio payload
        empty_pkt = pack_udp_packet(PacketType.PC_AUDIO, 42, 1000, b"")
        self.assertEqual(len(empty_pkt), 7)

        pt, s, ts, payload = unpack_udp_packet(empty_pkt)
        self.assertEqual(pt, PacketType.PC_AUDIO)
        self.assertEqual(s, 42)
        self.assertEqual(ts, 1000)
        self.assertEqual(len(payload), 0)

        # Truncated packets (< 7 bytes)
        for bad_len in range(0, 7):
            bad_data = b"\x01\x02\x03\x04\x05\x06"[:bad_len]
            with self.assertRaises(ValueError):
                unpack_udp_packet(bad_data)

    def test_07_mtu_payload_clamping(self):
        """Tier 2.7: Verify UDP MTU boundary clamping calculation."""
        mtu = 1500
        ip_header = 20
        udp_header = 8
        as_header = UDP_HEADER_SIZE  # 7 bytes
        
        max_udp_payload = mtu - ip_header - udp_header  # 1472 bytes
        max_audio_payload = max_udp_payload - as_header  # 1465 bytes

        self.assertEqual(max_udp_payload, 1472)
        self.assertEqual(max_audio_payload, 1465)

        # Oversized frame calculation
        oversized_pcm = bytes(1466)
        total_packet = pack_udp_packet(PacketType.PC_AUDIO, 1, 0, oversized_pcm)
        self.assertGreater(len(total_packet), max_udp_payload)

        # Compliant frame
        compliant_pcm = bytes(1465)
        total_compliant = pack_udp_packet(PacketType.PC_AUDIO, 1, 0, compliant_pcm)
        self.assertEqual(len(total_compliant), max_udp_payload)


if __name__ == "__main__":
    unittest.main()
