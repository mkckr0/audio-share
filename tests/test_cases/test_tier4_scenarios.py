"""
AudioStream E2E Test Suite — Tier 4: Real-World Application Scenarios.
Tests:
1. High-load gaming audio stream simulation with network jitter and packet loss
2. Discord/VoIP on-demand mic session activation, debounce, and deactivation flow
3. Multi-server LAN discovery beacon parsing, deduplication, and expiration pruning
"""

import unittest
try:
    from framework.protocol import (
        DiscoveryBeacon, create_mock_opus_frame, unpack_udp_packet,
        PacketType, CommandId
    )
    from framework.jitter_buffer import AdaptiveJitterBuffer, JitterPreset
    from framework.network_sim import MockNetworkLink, MockAudioStreamSession
    from framework.virtual_mic_sim import VirtualMicMonitorSimulator, MicState
    from framework.discovery_sim import DiscoveryBeaconListener
except ImportError:
    from ..framework.protocol import (
        DiscoveryBeacon, create_mock_opus_frame, unpack_udp_packet,
        PacketType, CommandId
    )
    from ..framework.jitter_buffer import AdaptiveJitterBuffer, JitterPreset
    from ..framework.network_sim import MockNetworkLink, MockAudioStreamSession
    from ..framework.virtual_mic_sim import VirtualMicMonitorSimulator, MicState
    from ..framework.discovery_sim import DiscoveryBeaconListener


class TestTier4Scenarios(unittest.TestCase):

    def test_01_high_load_gaming_audio_simulation(self):
        """Tier 4.1: Simulate high-load 50 FPS gaming audio stream under network jitter and packet loss."""
        # 50 fps = 20ms frames, 200 frames = 4 seconds of gameplay
        num_frames = 200
        link = MockNetworkLink(
            base_latency_ms=10.0,
            jitter_stddev_ms=3.5,
            loss_rate=0.02,     # 2% packet loss
            reorder_rate=0.03   # 3% packet reorder
        )
        session = MockAudioStreamSession(session_id=401)
        session.is_playing = True
        jb = AdaptiveJitterBuffer(preset=JitterPreset.MEDIUM)

        # Transmit 200 frames
        for frame_idx in range(num_frames):
            opus_frame = create_mock_opus_frame(frame_size_ms=20, stereo=True, sequence=frame_idx)
            pkt = session.generate_pc_audio_packet(opus_frame)
            link.transmit(pkt)
            
            # Step network clock by 20ms and ingest arriving packets
            delivered_packets = link.advance_time(20.0)
            for raw_pkt in delivered_packets:
                pt, seq, ts, pay = unpack_udp_packet(raw_pkt)
                jb.push(seq, ts, pay, arrival_time_ms=link.simulated_clock_ms)

        # Flush remaining in-flight packets
        remaining = link.advance_time(200.0)
        for raw_pkt in remaining:
            pt, seq, ts, pay = unpack_udp_packet(raw_pkt)
            jb.push(seq, ts, pay, arrival_time_ms=link.simulated_clock_ms)

        # Render/pull frames
        pulled_frames = 0
        plc_count = 0
        for _ in range(num_frames):
            data, is_plc = jb.pull()
            if data is not None:
                pulled_frames += 1
                if is_plc:
                    plc_count += 1

        # Assertions
        self.assertGreater(pulled_frames, 180, "Must pull overwhelming majority of 200 frames")
        self.assertGreaterEqual(jb.stats.packets_received, 180)
        self.assertGreaterEqual(jb.stats.estimated_jitter_ms, 0.0)
        self.assertEqual(plc_count, jb.stats.plc_frames)

    def test_02_discord_voip_on_demand_lifecycle(self):
        """Tier 4.2: Full Discord on-demand mic activation, streaming, debounce, and deactivation flow."""
        monitor = VirtualMicMonitorSimulator(debounce_ms=3000.0)
        session = MockAudioStreamSession(session_id=402)
        clock_ms = 0.0

        # 1. Initially idle
        self.assertEqual(monitor.state, MicState.IDLE)
        self.assertFalse(monitor.android_green_dot_active)
        self.assertFalse(session.is_mic_active)

        # 2. Discord launches and enters voice call at t = 1000ms
        clock_ms = 1000.0
        start_triggered = monitor.app_open_mic("Discord.exe", clock_ms)
        self.assertTrue(start_triggered)
        self.assertEqual(monitor.state, MicState.ACTIVE)
        self.assertTrue(monitor.android_green_dot_active)

        # Server sends CMD_START_MIC to phone
        cmd, resp = session.handle_tcp_command(CommandId.CMD_START_MIC, now_ms=clock_ms)
        self.assertEqual(cmd, CommandId.CMD_START_MIC)
        self.assertTrue(session.is_mic_active)

        # 3. User speaks in Discord for 5 seconds (t = 1000ms to 6000ms)
        for step in range(25):
            clock_ms += 200.0
            mic_data = create_mock_opus_frame(frame_size_ms=20, stereo=False, sequence=step)
            pkt = session.generate_phone_mic_packet(mic_data)
            pt, seq, ts, pay = unpack_udp_packet(pkt)
            self.assertEqual(pt, PacketType.PHONE_MIC)

        # 4. User disconnects from Discord call at t = 6000ms
        clock_ms = 6000.0
        monitor.app_close_mic("Discord.exe", clock_ms)
        self.assertEqual(monitor.state, MicState.DEBOUNCING)
        # Green dot and streaming stay temporarily active during debounce
        self.assertTrue(monitor.android_green_dot_active)

        # 5. Advance clock by 2000ms (t = 8000ms < 9000ms debounce expiry)
        clock_ms = 8000.0
        expired = monitor.advance_time(clock_ms)
        self.assertFalse(expired, "Debounce should not expire before 3000ms")
        self.assertEqual(monitor.state, MicState.DEBOUNCING)

        # 6. Advance clock to t = 9001ms (debounce expired)
        clock_ms = 9001.0
        expired = monitor.advance_time(clock_ms)
        self.assertTrue(expired, "Debounce must expire at 3000ms")
        self.assertEqual(monitor.state, MicState.IDLE)
        self.assertFalse(monitor.android_green_dot_active, "Android green dot must turn off")

        # Server sends CMD_STOP_MIC
        cmd, resp = session.handle_tcp_command(CommandId.CMD_STOP_MIC, now_ms=clock_ms)
        self.assertEqual(cmd, CommandId.CMD_STOP_MIC)
        self.assertFalse(session.is_mic_active)

        # 7. Debounce interruption test: Discord quickly toggled back on
        clock_ms = 10000.0
        monitor.app_open_mic("Discord.exe", clock_ms)
        clock_ms = 11000.0
        monitor.app_close_mic("Discord.exe", clock_ms)
        self.assertEqual(monitor.state, MicState.DEBOUNCING)
        # Reopen at t = 12000ms before debounce expires
        clock_ms = 12000.0
        reopened = monitor.app_open_mic("Discord.exe", clock_ms)
        self.assertFalse(reopened, "Should re-activate without sending extra start command")
        self.assertEqual(monitor.state, MicState.ACTIVE)
        self.assertIsNone(monitor.debounce_start_ms)

    def test_03_multi_server_lan_discovery_and_pruning(self):
        """Tier 4.3: Multi-server LAN discovery beacon reception, deduplication, and stale server pruning."""
        listener = DiscoveryBeaconListener(timeout_ms=6000.0)

        # 3 servers on LAN
        server_a = DiscoveryBeacon(app_name="AudioStream", server_name="Gaming-Desktop", port=65530,
                                   version="1.0.0", supports_opus=True, supports_mic=True,
                                   ip_address="192.168.1.10")
        server_b = DiscoveryBeacon(app_name="AudioStream", server_name="Work-Laptop", port=65530,
                                   version="1.0.0", supports_opus=True, supports_mic=False,
                                   ip_address="192.168.1.20")
        server_c = DiscoveryBeacon(app_name="AudioStream", server_name="Media-Rig", port=65530,
                                   version="1.0.0", supports_opus=False, supports_mic=False,
                                   ip_address="192.168.1.30")

        bytes_a = server_a.serialize()
        bytes_b = server_b.serialize()
        bytes_c = server_c.serialize()

        # At t = 0s: all 3 broadcast beacons
        listener.receive_beacon_data(bytes_a, current_time_ms=0.0)
        listener.receive_beacon_data(bytes_b, current_time_ms=0.0)
        listener.receive_beacon_data(bytes_c, current_time_ms=0.0)

        active = listener.get_active_servers()
        self.assertEqual(len(active), 3)

        # At t = 2.0s: all 3 repeat beacons
        listener.receive_beacon_data(bytes_a, current_time_ms=2000.0)
        listener.receive_beacon_data(bytes_b, current_time_ms=2000.0)
        listener.receive_beacon_data(bytes_c, current_time_ms=2000.0)
        self.assertEqual(len(listener.get_active_servers()), 3)
        self.assertEqual(listener.total_beacons_received, 6)

        # At t = 4.0s: Server B shuts down; only Server A and C send beacons
        listener.receive_beacon_data(bytes_a, current_time_ms=4000.0)
        listener.receive_beacon_data(bytes_c, current_time_ms=4000.0)

        # At t = 6.0s: Server A and C send beacons
        listener.receive_beacon_data(bytes_a, current_time_ms=6000.0)
        listener.receive_beacon_data(bytes_c, current_time_ms=6000.0)

        # Check at t = 8.5s: Server B last seen at 2.0s (elapsed 6.5s > 6.0s timeout)
        pruned = listener.prune_stale_servers(current_time_ms=8500.0)
        self.assertIn("192.168.1.20:65530", pruned)
        self.assertEqual(len(pruned), 1)

        remaining_servers = listener.get_active_servers()
        self.assertEqual(len(remaining_servers), 2)
        remaining_names = [s.server_name for s in remaining_servers]
        self.assertIn("Gaming-Desktop", remaining_names)
        self.assertIn("Media-Rig", remaining_names)
        self.assertNotIn("Work-Laptop", remaining_names)


if __name__ == "__main__":
    unittest.main()
