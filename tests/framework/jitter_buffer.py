"""
AudioStream Adaptive Jitter Buffer Reference Implementation.
Implements:
1. Modulo-65536 sequence number wrap-around arithmetic
2. Out-of-order packet reordering and deduplication
3. RFC 3550 interarrival jitter calculation
4. Packet Loss Concealment (PLC) detection and frame synthesis
5. Adaptive buffer depth management with Low/Med/High presets
"""

from enum import Enum
from typing import Dict, List, Optional, Tuple, Any
import math


class JitterPreset(Enum):
    LOW = "LOW"        # 20ms target depth (USB Tethering)
    MEDIUM = "MEDIUM"  # 50ms target depth (Stable Wi-Fi)
    HIGH = "HIGH"      # 100ms target depth (Unstable Wi-Fi)
    CUSTOM = "CUSTOM"


def seq_diff(seq_a: int, seq_b: int) -> int:
    """
    Calculate modular difference between two 16-bit sequence numbers:
    Returns positive if seq_a > seq_b (within 32767 distance),
    negative if seq_a < seq_b.
    """
    diff = (seq_a - seq_b) & 0xFFFF
    if diff > 0x7FFF:
        diff -= 0x10000
    return diff


class JitterPacket:
    def __init__(self, seq: int, timestamp: int, data: bytes, is_plc: bool = False):
        self.seq = seq & 0xFFFF
        self.timestamp = timestamp & 0xFFFFFFFF
        self.data = data
        self.is_plc = is_plc
        self.arrival_time_ms: float = 0.0


class JitterBufferStats:
    def __init__(self):
        self.packets_received = 0
        self.packets_lost = 0
        self.packets_reordered = 0
        self.packets_dropped_late = 0
        self.duplicates = 0
        self.underruns = 0
        self.overruns = 0
        self.plc_frames = 0
        self.estimated_jitter_ms = 0.0
        self.avg_latency_ms = 0.0


class AdaptiveJitterBuffer:
    def __init__(self, sample_rate: int = 48000, channels: int = 2,
                 frame_size_ms: int = 20, preset: JitterPreset = JitterPreset.LOW):
        self.sample_rate = sample_rate
        self.channels = channels
        self.frame_size_ms = frame_size_ms
        self.preset = preset
        
        # Preset depth targets
        self.target_depth_ms = {
            JitterPreset.LOW: 20,
            JitterPreset.MEDIUM: 50,
            JitterPreset.HIGH: 100,
            JitterPreset.CUSTOM: 30
        }[preset]
        
        self.max_buffer_packets = 50
        self.packets: Dict[int, JitterPacket] = {}
        self.next_play_seq: Optional[int] = None
        self.stats = JitterBufferStats()
        
        # RFC 3550 tracking
        self._last_transit_diff: Optional[float] = None
        self._last_rx_ms: Optional[float] = None
        self._last_rtp_ts: Optional[int] = None

    def set_preset(self, preset: JitterPreset):
        self.preset = preset
        self.target_depth_ms = {
            JitterPreset.LOW: 20,
            JitterPreset.MEDIUM: 50,
            JitterPreset.HIGH: 100,
            JitterPreset.CUSTOM: 30
        }[preset]

    def push(self, seq: int, timestamp: int, data: bytes, arrival_time_ms: float = 0.0) -> bool:
        """
        Insert a packet into the jitter buffer.
        Returns True if accepted, False if discarded (duplicate or too late).
        """
        seq = seq & 0xFFFF
        self.stats.packets_received += 1
        
        # RFC 3550 Jitter update
        if self._last_rx_ms is not None and self._last_rtp_ts is not None:
            # Transit time difference D(i, j) = (R_j - R_i) - (S_j - S_i)
            # Timestamp in samples at sample_rate
            rtp_time_ms = (timestamp - self._last_rtp_ts) / (self.sample_rate / 1000.0)
            rx_diff_ms = arrival_time_ms - self._last_rx_ms
            d = abs(rx_diff_ms - rtp_time_ms)
            # J_i = J_{i-1} + (|D| - J_{i-1}) / 16.0
            self.stats.estimated_jitter_ms += (d - self.stats.estimated_jitter_ms) / 16.0
        
        self._last_rx_ms = arrival_time_ms
        self._last_rtp_ts = timestamp

        # First packet initialization
        if self.next_play_seq is None:
            self.next_play_seq = seq
            pkt = JitterPacket(seq, timestamp, data)
            pkt.arrival_time_ms = arrival_time_ms
            self.packets[seq] = pkt
            return True

        # Check for duplicate
        if seq in self.packets:
            self.stats.duplicates += 1
            return False

        # Check if too late (behind current play sequence)
        diff_from_play = seq_diff(seq, self.next_play_seq)
        if diff_from_play < 0:
            self.stats.packets_dropped_late += 1
            return False

        # Check out-of-order arrival
        # If diff > 1 from expected highest, it's out of order or burst
        if diff_from_play > 1:
            self.stats.packets_reordered += 1

        # Check buffer overflow
        if len(self.packets) >= self.max_buffer_packets:
            self.stats.overruns += 1
            # Evict earliest
            earliest_seq = min(self.packets.keys(), key=lambda s: seq_diff(s, self.next_play_seq))
            del self.packets[earliest_seq]

        pkt = JitterPacket(seq, timestamp, data)
        pkt.arrival_time_ms = arrival_time_ms
        self.packets[seq] = pkt
        return True

    def pull(self) -> Tuple[Optional[bytes], bool]:
        """
        Pull the next expected sequential packet for rendering.
        Returns: (data_bytes, is_plc)
        - If packet is present: (payload, False)
        - If packet missing (gap) and buffer under threshold: triggers PLC (plc_bytes, True)
        - If buffer empty: (None, False) -> underrun
        """
        if self.next_play_seq is None:
            self.stats.underruns += 1
            return None, False

        seq = self.next_play_seq

        if seq in self.packets:
            pkt = self.packets.pop(seq)
            self.next_play_seq = (seq + 1) & 0xFFFF
            return pkt.data, False

        # Missing packet - check if there are future packets buffered
        future_packets = [s for s in self.packets.keys() if seq_diff(s, seq) > 0]
        if future_packets:
            # We have future packets waiting, so packet `seq` was lost!
            # Trigger PLC and advance next_play_seq
            self.stats.packets_lost += 1
            self.stats.plc_frames += 1
            self.next_play_seq = (seq + 1) & 0xFFFF
            # Synthesize PLC silence frame (e.g. 960 samples of 16-bit 0s = 1920 bytes)
            plc_size = int(self.sample_rate * (self.frame_size_ms / 1000.0) * self.channels * 2)
            plc_payload = bytes(plc_size)
            return plc_payload, True

        # Complete buffer starvation
        self.stats.underruns += 1
        return None, False

    def get_buffered_count(self) -> int:
        return len(self.packets)

    def get_buffered_duration_ms(self) -> float:
        return len(self.packets) * self.frame_size_ms
