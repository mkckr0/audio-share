"""
AudioStream E2E Test Suite — Tier 3: Cross-Feature Combinations.
Tests:
1. Dynamic codec switching (PCM <-> Opus) during active streaming session
2. Bidirectional concurrent streaming (PC audio + phone mic)
3. USB tethering disconnect / reconnect and interface failover simulation
4. Multi-client concurrent session tracking and individual heartbeat timeout
"""

import unittest
try:
    from framework.protocol import (
        AudioCodec, AudioEncoding, CommandId, PacketType,
        AudioFormat, StreamConfig, MicControl,
        pack_udp_packet, unpack_udp_packet, create_mock_opus_frame
    )
    from framework.jitter_buffer import AdaptiveJitterBuffer, JitterPreset
    from framework.network_sim import MockAudioStreamSession, MockNetworkLink
except ImportError:
    from ..framework.protocol import (
        AudioCodec, AudioEncoding, CommandId, PacketType,
        AudioFormat, StreamConfig, MicControl,
        pack_udp_packet, unpack_udp_packet, create_mock_opus_frame
    )
    from ..framework.jitter_buffer import AdaptiveJitterBuffer, JitterPreset
    from ..framework.network_sim import MockAudioStreamSession, MockNetworkLink


class TestTier3Combinations(unittest.TestCase):

    def test_01_dynamic_codec_switching_active_session(self):
        """Tier 3.1: Verify seamless mid-stream codec renegotiation (PCM <-> Opus)."""
        session = MockAudioStreamSession(session_id=101)
        
        # 1. Start playback session
        resp_cmd, resp_payload = session.handle_tcp_command(CommandId.CMD_START_PLAY, now_ms=0.0)
        self.assertEqual(resp_cmd, CommandId.CMD_START_PLAY)
        self.assertTrue(session.is_playing)

        # 2. Stream 10 PCM frames
        pcm_frames = []
        for i in range(10):
            raw_pcm = b"\x11\x22" * 480  # 960 bytes
            pkt = session.generate_pc_audio_packet(raw_pcm)
            pt, seq, ts, payload = unpack_udp_packet(pkt)
            self.assertEqual(pt, PacketType.PC_AUDIO)
            self.assertEqual(payload, raw_pcm)
            pcm_frames.append(pkt)

        self.assertEqual(session.pc_seq, 10)

        # 3. Dynamic switch to Opus (128kbps, 20ms)
        opus_config = StreamConfig(
            format=AudioFormat(encoding=AudioEncoding.ENCODING_PCM_FLOAT, channels=2, sample_rate=48000),
            codec=AudioCodec.CODEC_OPUS,
            opus_bitrate=128000,
            opus_frame_size_ms=20,
            target_latency_ms=20
        )
        resp_cmd, resp_payload = session.handle_tcp_command(
            CommandId.CMD_NEGOTIATE_CODEC, opus_config.serialize(), now_ms=200.0
        )
        self.assertEqual(resp_cmd, CommandId.CMD_NEGOTIATE_CODEC)
        accepted_config = StreamConfig.deserialize(resp_payload)
        self.assertEqual(accepted_config.codec, AudioCodec.CODEC_OPUS)

        # 4. Stream 10 Opus frames - sequence numbers should continue monotonically
        for i in range(10):
            opus_frame = create_mock_opus_frame(frame_size_ms=20, stereo=True, sequence=i)
            pkt = session.generate_pc_audio_packet(opus_frame)
            pt, seq, ts, payload = unpack_udp_packet(pkt)
            self.assertEqual(pt, PacketType.PC_AUDIO)
            self.assertEqual(seq, 10 + i)
            self.assertEqual(payload, opus_frame)

        self.assertEqual(session.pc_seq, 20)

    def test_02_bidirectional_concurrent_streaming(self):
        """Tier 3.2: Verify simultaneous bidirectional PC audio + Phone mic streaming."""
        session = MockAudioStreamSession(session_id=202)
        session.is_playing = True
        session.is_mic_active = True

        pc_jb = AdaptiveJitterBuffer(preset=JitterPreset.LOW)
        phone_jb = AdaptiveJitterBuffer(preset=JitterPreset.LOW)

        # Interleave PC audio frames and Phone mic frames
        for step in range(25):
            # PC sends Desktop Audio -> Android
            pc_audio_data = b"pc_audio_" + str(step).encode()
            pc_pkt_bytes = session.generate_pc_audio_packet(pc_audio_data)
            
            # Phone sends Mic Audio -> PC
            phone_mic_data = b"phone_mic_" + str(step).encode()
            phone_pkt_bytes = session.generate_phone_mic_packet(phone_mic_data)

            # Android receiver demuxes PC packet
            pt_pc, seq_pc, ts_pc, pay_pc = unpack_udp_packet(pc_pkt_bytes)
            self.assertEqual(pt_pc, PacketType.PC_AUDIO)
            pc_jb.push(seq_pc, ts_pc, pay_pc, arrival_time_ms=step * 20.0)

            # PC receiver demuxes Phone Mic packet
            pt_phone, seq_phone, ts_phone, pay_phone = unpack_udp_packet(phone_pkt_bytes)
            self.assertEqual(pt_phone, PacketType.PHONE_MIC)
            phone_jb.push(seq_phone, ts_phone, pay_phone, arrival_time_ms=step * 20.0)

        # Verify Android receiver plays PC audio in sequence
        for step in range(25):
            d, plc = pc_jb.pull()
            self.assertFalse(plc)
            self.assertEqual(d, b"pc_audio_" + str(step).encode())

        # Verify PC receiver plays Mic audio in sequence
        for step in range(25):
            d, plc = phone_jb.pull()
            self.assertFalse(plc)
            self.assertEqual(d, b"phone_mic_" + str(step).encode())

    def test_03_usb_tethering_disconnect_and_reconnect(self):
        """Tier 3.3: Simulate USB tethering unplug and Wi-Fi reconnection."""
        link = MockNetworkLink(base_latency_ms=2.0)  # USB tethering latency ~2ms
        session = MockAudioStreamSession(session_id=303)
        session.is_playing = True

        # Send initial packets over USB
        for i in range(5):
            pkt = session.generate_pc_audio_packet(f"usb_frame_{i}".encode())
            link.transmit(pkt)

        delivered = link.advance_time(5.0)
        self.assertEqual(len(delivered), 5)

        # USB Cable Unplugged!
        link.disconnect()
        for i in range(5, 10):
            pkt = session.generate_pc_audio_packet(f"lost_frame_{i}".encode())
            link.transmit(pkt)

        delivered_during_dc = link.advance_time(50.0)
        self.assertEqual(len(delivered_during_dc), 0, "Packets during disconnect must be dropped")

        # Wi-Fi Reconnect (Latency higher ~15ms)
        link.reconnect(new_base_latency_ms=15.0)
        for i in range(10, 15):
            pkt = session.generate_pc_audio_packet(f"wifi_frame_{i}".encode())
            link.transmit(pkt)

        delivered_wifi = link.advance_time(20.0)
        self.assertEqual(len(delivered_wifi), 5)

    def test_04_multi_peer_heartbeat_timeout(self):
        """Tier 3.4: Verify individual peer heartbeat tracking and timeout."""
        peer1 = MockAudioStreamSession(session_id=1)
        peer2 = MockAudioStreamSession(session_id=2)

        peer1.handle_tcp_command(CommandId.CMD_START_PLAY, now_ms=0.0)
        peer2.handle_tcp_command(CommandId.CMD_START_PLAY, now_ms=0.0)

        # Time reaches 3.0s: Peer 1 sends heartbeat, Peer 2 is silent
        peer1.handle_tcp_command(CommandId.CMD_HEARTBEAT, now_ms=3000.0)

        # Time reaches 7.0s (> 6.0s timeout):
        timed_out_1 = peer1.check_heartbeat_timeout(now_ms=7000.0)
        timed_out_2 = peer2.check_heartbeat_timeout(now_ms=7000.0)

        self.assertFalse(timed_out_1, "Peer 1 sent heartbeat at 3s, should NOT time out at 7s (elapsed 4s)")
        self.assertTrue(timed_out_2, "Peer 2 was silent since 0s, MUST time out at 7s (elapsed 7s > 6s)")


if __name__ == "__main__":
    unittest.main()
