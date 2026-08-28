"""
AudioStream Protocol Definitions & Binary Framing Utilities.
Implements the protocol contracts defined in PROJECT.md and protos/audiostream.proto:
1. Extended Protobuf Messages (AudioFormat, StreamConfig, MicControl, DiscoveryBeacon)
2. TCP Command Framing (<uint32_t cmd_id>[<uint32_t len><protobuf_payload>])
3. UDP 7-Byte Binary Header (<uint8_t packet_type><uint16_t seq_num><uint32_t timestamp>)
4. Opus Frame TOC & Packet Utilities
"""

import struct
from enum import IntEnum
from typing import Tuple, Optional, Dict, Any


class AudioCodec(IntEnum):
    CODEC_PCM = 0
    CODEC_OPUS = 1


class AudioEncoding(IntEnum):
    ENCODING_INVALID = 0
    ENCODING_PCM_FLOAT = 1
    ENCODING_PCM_8BIT = 2
    ENCODING_PCM_16BIT = 3
    ENCODING_PCM_24BIT_PACKED = 4
    ENCODING_PCM_32BIT = 5


class CommandId(IntEnum):
    CMD_NONE = 0
    CMD_GET_FORMAT = 1
    CMD_START_PLAY = 2
    CMD_HEARTBEAT = 3
    CMD_NEGOTIATE_CODEC = 4
    CMD_START_MIC = 5
    CMD_STOP_MIC = 6
    CMD_MIC_DATA = 7
    CMD_DISCOVERY = 8
    CMD_ENCRYPTION_HANDSHAKE = 9


class PacketType(IntEnum):
    PC_AUDIO = 0x01
    PHONE_MIC = 0x02


# ---------------------------------------------------------------------------
# Lightweight pure-Python Proto3 Wire Serialization / Deserialization
# (Ensures zero-dependency standalone execution across all environments)
# ---------------------------------------------------------------------------

def encode_varint(value: int) -> bytes:
    """Encode an unsigned integer into protobuf varint format."""
    if value < 0:
        value = (1 << 64) + value
    out = bytearray()
    while True:
        towrite = value & 0x7F
        value >>= 7
        if value:
            out.append(towrite | 0x80)
        else:
            out.append(towrite)
            break
    return bytes(out)


def decode_varint(data: bytes, offset: int = 0) -> Tuple[int, int]:
    """Decode a protobuf varint from data starting at offset."""
    res = 0
    shift = 0
    pos = offset
    while pos < len(data):
        b = data[pos]
        pos += 1
        res |= (b & 0x7F) << shift
        if not (b & 0x80):
            return res, pos
        shift += 7
    raise ValueError("Truncated varint in protobuf stream")


def encode_tag(field_number: int, wire_type: int) -> bytes:
    return encode_varint((field_number << 3) | wire_type)


class AudioFormat:
    def __init__(self, encoding: AudioEncoding = AudioEncoding.ENCODING_PCM_16BIT,
                 channels: int = 2, sample_rate: int = 48000):
        self.encoding = AudioEncoding(encoding)
        self.channels = channels
        self.sample_rate = sample_rate

    def serialize(self) -> bytes:
        buf = bytearray()
        if self.encoding != AudioEncoding.ENCODING_INVALID:
            buf += encode_tag(1, 0) + encode_varint(int(self.encoding))
        if self.channels != 0:
            buf += encode_tag(2, 0) + encode_varint(self.channels)
        if self.sample_rate != 0:
            buf += encode_tag(3, 0) + encode_varint(self.sample_rate)
        return bytes(buf)

    @classmethod
    def deserialize(cls, data: bytes) -> "AudioFormat":
        pos = 0
        obj = cls(AudioEncoding.ENCODING_INVALID, 0, 0)
        while pos < len(data):
            tag, pos = decode_varint(data, pos)
            field_num = tag >> 3
            wire_type = tag & 0x07
            if wire_type == 0:
                val, pos = decode_varint(data, pos)
                if field_num == 1:
                    obj.encoding = AudioEncoding(val)
                elif field_num == 2:
                    obj.channels = val
                elif field_num == 3:
                    obj.sample_rate = val
            elif wire_type == 2:
                length, pos = decode_varint(data, pos)
                pos += length
            else:
                raise ValueError(f"Unsupported wire type: {wire_type}")
        return obj

    def __eq__(self, other):
        return (isinstance(other, AudioFormat) and
                self.encoding == other.encoding and
                self.channels == other.channels and
                self.sample_rate == other.sample_rate)

    def __repr__(self):
        return f"AudioFormat(encoding={self.encoding.name}, channels={self.channels}, sample_rate={self.sample_rate})"


class StreamConfig:
    def __init__(self, format: Optional[AudioFormat] = None,
                 codec: AudioCodec = AudioCodec.CODEC_OPUS,
                 opus_bitrate: int = 128000,
                 opus_frame_size_ms: int = 20,
                 target_latency_ms: int = 30):
        self.format = format or AudioFormat()
        self.codec = AudioCodec(codec)
        self.opus_bitrate = opus_bitrate
        self.opus_frame_size_ms = opus_frame_size_ms
        self.target_latency_ms = target_latency_ms

    def serialize(self) -> bytes:
        buf = bytearray()
        fmt_bytes = self.format.serialize()
        if fmt_bytes:
            buf += encode_tag(1, 2) + encode_varint(len(fmt_bytes)) + fmt_bytes
        if self.codec != AudioCodec.CODEC_PCM:
            buf += encode_tag(2, 0) + encode_varint(int(self.codec))
        if self.opus_bitrate != 0:
            buf += encode_tag(3, 0) + encode_varint(self.opus_bitrate)
        if self.opus_frame_size_ms != 0:
            buf += encode_tag(4, 0) + encode_varint(self.opus_frame_size_ms)
        if self.target_latency_ms != 0:
            buf += encode_tag(5, 0) + encode_varint(self.target_latency_ms)
        return bytes(buf)

    @classmethod
    def deserialize(cls, data: bytes) -> "StreamConfig":
        pos = 0
        obj = cls(AudioFormat(), AudioCodec.CODEC_PCM, 0, 0, 0)
        while pos < len(data):
            tag, pos = decode_varint(data, pos)
            field_num = tag >> 3
            wire_type = tag & 0x07
            if wire_type == 0:
                val, pos = decode_varint(data, pos)
                if field_num == 2:
                    obj.codec = AudioCodec(val)
                elif field_num == 3:
                    obj.opus_bitrate = val
                elif field_num == 4:
                    obj.opus_frame_size_ms = val
                elif field_num == 5:
                    obj.target_latency_ms = val
            elif wire_type == 2:
                length, pos = decode_varint(data, pos)
                sub_bytes = data[pos:pos+length]
                pos += length
                if field_num == 1:
                    obj.format = AudioFormat.deserialize(sub_bytes)
            else:
                raise ValueError(f"Unsupported wire type: {wire_type}")
        return obj

    def __eq__(self, other):
        return (isinstance(other, StreamConfig) and
                self.format == other.format and
                self.codec == other.codec and
                self.opus_bitrate == other.opus_bitrate and
                self.opus_frame_size_ms == other.opus_frame_size_ms and
                self.target_latency_ms == other.target_latency_ms)


class MicControl:
    def __init__(self, start: bool = False,
                 config: Optional[StreamConfig] = None,
                 mic_source: int = 1):
        self.start = start
        self.config = config or StreamConfig()
        self.mic_source = mic_source

    def serialize(self) -> bytes:
        buf = bytearray()
        if self.start:
            buf += encode_tag(1, 0) + encode_varint(1)
        cfg_bytes = self.config.serialize()
        if cfg_bytes:
            buf += encode_tag(2, 2) + encode_varint(len(cfg_bytes)) + cfg_bytes
        if self.mic_source != 0:
            buf += encode_tag(3, 0) + encode_varint(self.mic_source)
        return bytes(buf)

    @classmethod
    def deserialize(cls, data: bytes) -> "MicControl":
        pos = 0
        obj = cls(False, StreamConfig(), 0)
        while pos < len(data):
            tag, pos = decode_varint(data, pos)
            field_num = tag >> 3
            wire_type = tag & 0x07
            if wire_type == 0:
                val, pos = decode_varint(data, pos)
                if field_num == 1:
                    obj.start = bool(val)
                elif field_num == 3:
                    obj.mic_source = val
            elif wire_type == 2:
                length, pos = decode_varint(data, pos)
                sub_bytes = data[pos:pos+length]
                pos += length
                if field_num == 2:
                    obj.config = StreamConfig.deserialize(sub_bytes)
            else:
                raise ValueError(f"Unsupported wire type: {wire_type}")
        return obj


class DiscoveryBeacon:
    def __init__(self, app_name: str = "AudioStream",
                 server_name: str = "AudioStream Server",
                 port: int = 65530,
                 version: str = "1.0.0",
                 supports_opus: bool = True,
                 supports_mic: bool = True,
                 ip_address: str = "127.0.0.1"):
        self.app_name = app_name
        self.server_name = server_name
        self.port = port
        self.version = version
        self.supports_opus = supports_opus
        self.supports_mic = supports_mic
        self.ip_address = ip_address

    def serialize(self) -> bytes:
        buf = bytearray()
        if self.app_name:
            b = self.app_name.encode('utf-8')
            buf += encode_tag(1, 2) + encode_varint(len(b)) + b
        if self.server_name:
            b = self.server_name.encode('utf-8')
            buf += encode_tag(2, 2) + encode_varint(len(b)) + b
        if self.port != 0:
            buf += encode_tag(3, 0) + encode_varint(self.port)
        if self.version:
            b = self.version.encode('utf-8')
            buf += encode_tag(4, 2) + encode_varint(len(b)) + b
        if self.supports_opus:
            buf += encode_tag(5, 0) + encode_varint(1)
        if self.supports_mic:
            buf += encode_tag(6, 0) + encode_varint(1)
        if self.ip_address:
            b = self.ip_address.encode('utf-8')
            buf += encode_tag(7, 2) + encode_varint(len(b)) + b
        return bytes(buf)

    @classmethod
    def deserialize(cls, data: bytes) -> "DiscoveryBeacon":
        pos = 0
        obj = cls("", "", 0, "", False, False, "")
        while pos < len(data):
            tag, pos = decode_varint(data, pos)
            field_num = tag >> 3
            wire_type = tag & 0x07
            if wire_type == 0:
                val, pos = decode_varint(data, pos)
                if field_num == 3:
                    obj.port = val
                elif field_num == 5:
                    obj.supports_opus = bool(val)
                elif field_num == 6:
                    obj.supports_mic = bool(val)
            elif wire_type == 2:
                length, pos = decode_varint(data, pos)
                sub_bytes = data[pos:pos+length]
                pos += length
                s = sub_bytes.decode('utf-8', errors='replace')
                if field_num == 1:
                    obj.app_name = s
                elif field_num == 2:
                    obj.server_name = s
                elif field_num == 4:
                    obj.version = s
                elif field_num == 7:
                    obj.ip_address = s
            else:
                raise ValueError(f"Unsupported wire type: {wire_type}")
        return obj


# ---------------------------------------------------------------------------
# UDP 7-Byte Binary Framing
# ---------------------------------------------------------------------------

UDP_HEADER_SIZE = 7
UDP_HEADER_FORMAT = "<BHI"  # uint8 packet_type, uint16 seq_num, uint32 timestamp


def pack_udp_packet(packet_type: PacketType, seq_num: int, timestamp: int, payload: bytes) -> bytes:
    """
    Pack an AudioStream UDP packet with the 7-byte binary header:
    - Packet Type (1 byte): 0x01 (PC Audio) or 0x02 (Phone Mic)
    - Sequence Number (2 bytes): uint16 Little-Endian
    - Timestamp (4 bytes): uint32 Little-Endian RTP sampling timestamp
    - Payload: Opus frame or PCM bytes
    """
    header = struct.pack(UDP_HEADER_FORMAT, int(packet_type) & 0xFF, seq_num & 0xFFFF, timestamp & 0xFFFFFFFF)
    return header + payload


def unpack_udp_packet(data: bytes) -> Tuple[PacketType, int, int, bytes]:
    """
    Unpack an AudioStream UDP packet.
    Raises ValueError if data is smaller than 7 bytes.
    """
    if len(data) < UDP_HEADER_SIZE:
        raise ValueError(f"UDP packet too short: {len(data)} bytes (minimum {UDP_HEADER_SIZE})")
    
    packet_type_raw, seq_num, timestamp = struct.unpack(UDP_HEADER_FORMAT, data[:UDP_HEADER_SIZE])
    payload = data[UDP_HEADER_SIZE:]
    return PacketType(packet_type_raw), seq_num, timestamp, payload


# ---------------------------------------------------------------------------
# TCP Command Framing Utilities
# ---------------------------------------------------------------------------

def pack_tcp_command(cmd: CommandId, payload: Optional[bytes] = None) -> bytes:
    """Pack a TCP command with Little-Endian uint32 ID and optional uint32 length prefix."""
    cmd_bytes = struct.pack("<I", int(cmd))
    if payload is not None:
        len_bytes = struct.pack("<I", len(payload))
        return cmd_bytes + len_bytes + payload
    return cmd_bytes


def unpack_tcp_command_header(data: bytes) -> Tuple[CommandId, int]:
    """Unpack TCP command ID and consumed bytes (4 bytes)."""
    if len(data) < 4:
        raise ValueError("Truncated TCP command header")
    cmd_val = struct.unpack("<I", data[:4])[0]
    return CommandId(cmd_val), 4


# ---------------------------------------------------------------------------
# Opus Frame Simulation Utilities (RFC 6716 compliant TOC generation)
# ---------------------------------------------------------------------------

def create_mock_opus_frame(frame_size_ms: int = 20, stereo: bool = True, sequence: int = 0) -> bytes:
    """
    Generate a syntactically valid Opus packet with standard Table of Contents (TOC) byte.
    Opus TOC byte:
    - Bits 7..3: Config number (e.g. 16 for FB 20ms SILK/CELT hybrid)
    - Bit 2: Stereo flag (1=stereo, 0=mono)
    - Bits 1..0: Frame count code (0=1 frame in packet)
    """
    config = 16  # Fullband 20ms
    s_flag = 1 if stereo else 0
    c_code = 0  # 1 frame
    toc = (config << 3) | (s_flag << 2) | c_code
    
    # Synthetic payload with entropy-like bytes
    payload_len = 80 + (sequence % 40)
    synthetic_data = bytes([(toc ^ i ^ (sequence & 0xFF)) & 0xFF for i in range(payload_len)])
    return bytes([toc]) + synthetic_data
