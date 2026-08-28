"""
AudioStream Network Simulation Harness.
Simulates:
1. Bidirectional TCP Control sessions (Format query, playback start, heartbeat ping/pong, codec negotiation)
2. UDP Audio streaming over lossy/jittery simulated network links
3. Tethering disconnect/reconnect and interface failovers
"""

import time
import random
from typing import List, Dict, Tuple, Optional, Callable
from .protocol import (
    CommandId, PacketType, AudioFormat, StreamConfig, MicControl,
    pack_udp_packet, unpack_udp_packet, pack_tcp_command
)
from .jitter_buffer import AdaptiveJitterBuffer


class SimulatedPacket:
    def __init__(self, data: bytes, scheduled_arrival_ms: float):
        self.data = data
        self.scheduled_arrival_ms = scheduled_arrival_ms


class MockNetworkLink:
    def __init__(self, base_latency_ms: float = 5.0, jitter_stddev_ms: float = 1.0,
                 loss_rate: float = 0.0, reorder_rate: float = 0.0):
        self.base_latency_ms = base_latency_ms
        self.jitter_stddev_ms = jitter_stddev_ms
        self.loss_rate = loss_rate
        self.reorder_rate = reorder_rate
        self.is_connected = True
        self.in_flight: List[SimulatedPacket] = []
        self.simulated_clock_ms: float = 0.0

    def transmit(self, packet_bytes: bytes):
        if not self.is_connected:
            return  # Dropped due to disconnect
        
        # Loss check
        if random.random() < self.loss_rate:
            return  # Packet dropped
        
        # Calculate latency with jitter
        jitter = random.gauss(0, self.jitter_stddev_ms)
        latency = max(0.5, self.base_latency_ms + jitter)
        
        if random.random() < self.reorder_rate:
            latency += random.uniform(5.0, 15.0)
            
        arrival_time = self.simulated_clock_ms + latency
        self.in_flight.append(SimulatedPacket(packet_bytes, arrival_time))

    def advance_time(self, delta_ms: float) -> List[bytes]:
        """Advance simulation clock and return all packets delivered in this window."""
        self.simulated_clock_ms += delta_ms
        delivered = []
        remaining = []
        for p in self.in_flight:
            if p.scheduled_arrival_ms <= self.simulated_clock_ms:
                delivered.append(p.data)
            else:
                remaining.append(p)
        self.in_flight = remaining
        return delivered

    def disconnect(self):
        self.is_connected = False
        self.in_flight.clear()

    def reconnect(self, new_base_latency_ms: Optional[float] = None):
        self.is_connected = True
        if new_base_latency_ms is not None:
            self.base_latency_ms = new_base_latency_ms


class MockAudioStreamSession:
    """
    Stateful model of an AudioStream TCP + UDP session.
    """
    def __init__(self, session_id: int = 1):
        self.session_id = session_id
        self.is_playing = False
        self.is_mic_active = False
        self.active_codec = StreamConfig()
        self.last_heartbeat_time_ms = 0.0
        self.heartbeat_timeout_ms = 6000.0  # 6.0s timeout per plan
        self.pc_jitter_buffer = AdaptiveJitterBuffer()
        self.phone_jitter_buffer = AdaptiveJitterBuffer()
        
        self.pc_seq = 0
        self.phone_seq = 0
        self.sample_timestamp = 0

    def handle_tcp_command(self, cmd: CommandId, payload: bytes = b"", now_ms: float = 0.0) -> Tuple[CommandId, bytes]:
        """Process an incoming TCP command and return (response_cmd, response_payload)."""
        if cmd == CommandId.CMD_GET_FORMAT:
            fmt = AudioFormat()
            return CommandId.CMD_GET_FORMAT, fmt.serialize()
            
        elif cmd == CommandId.CMD_START_PLAY:
            self.is_playing = True
            self.last_heartbeat_time_ms = now_ms
            import struct
            return CommandId.CMD_START_PLAY, struct.pack("<i", self.session_id)
            
        elif cmd == CommandId.CMD_HEARTBEAT:
            self.last_heartbeat_time_ms = now_ms
            return CommandId.CMD_HEARTBEAT, b""
            
        elif cmd == CommandId.CMD_NEGOTIATE_CODEC:
            req_config = StreamConfig.deserialize(payload)
            self.active_codec = req_config
            # Server accepts negotiated config
            return CommandId.CMD_NEGOTIATE_CODEC, req_config.serialize()
            
        elif cmd == CommandId.CMD_START_MIC:
            self.is_mic_active = True
            mic_ctl = MicControl.deserialize(payload) if payload else MicControl(start=True)
            return CommandId.CMD_START_MIC, mic_ctl.serialize()
            
        elif cmd == CommandId.CMD_STOP_MIC:
            self.is_mic_active = False
            return CommandId.CMD_STOP_MIC, b""
            
        return CommandId.CMD_NONE, b""

    def generate_pc_audio_packet(self, payload: bytes) -> bytes:
        seq = self.pc_seq
        self.pc_seq = (self.pc_seq + 1) & 0xFFFF
        ts = self.sample_timestamp
        self.sample_timestamp = (self.sample_timestamp + 960) & 0xFFFFFFFF
        return pack_udp_packet(PacketType.PC_AUDIO, seq, ts, payload)

    def generate_phone_mic_packet(self, payload: bytes) -> bytes:
        seq = self.phone_seq
        self.phone_seq = (self.phone_seq + 1) & 0xFFFF
        ts = self.sample_timestamp
        return pack_udp_packet(PacketType.PHONE_MIC, seq, ts, payload)

    def check_heartbeat_timeout(self, now_ms: float) -> bool:
        """Returns True if heartbeat timed out (> 6s)."""
        if self.is_playing and (now_ms - self.last_heartbeat_time_ms > self.heartbeat_timeout_ms):
            self.is_playing = False
            return True
        return False
