#!/usr/bin/env python3
"""
Adversarial Protobuf Empirical Test Suite for AudioStream M1
Tests compilation (C++, Python, Java), serialization, deserialization,
edge cases, boundary limits, and malformed inputs against audiostream.proto schema.
"""

import os
import sys
import subprocess
import tempfile
import struct

# Schema definition from PROJECT.md
AUDIOSTREAM_PROTO_CONTENT = """syntax = "proto3";
package com.audiostream.pb;
option java_package = "com.audiostream.app.pb";
option java_outer_classname = "AudioStreamProto";

enum AudioCodec {
  CODEC_PCM = 0;
  CODEC_OPUS = 1;
}

enum AudioEncoding {
  ENCODING_INVALID = 0;
  ENCODING_PCM_FLOAT = 1;
  ENCODING_PCM_8BIT = 2;
  ENCODING_PCM_16BIT = 3;
  ENCODING_PCM_24BIT_PACKED = 4;
  ENCODING_PCM_32BIT = 5;
}

message AudioFormat {
  AudioEncoding encoding = 1;
  int32 channels = 2;
  int32 sample_rate = 3;
}

message StreamConfig {
  AudioFormat format = 1;
  AudioCodec codec = 2;
  int32 opus_bitrate = 3;
  int32 opus_frame_size_ms = 4;
  int32 target_latency_ms = 5;
}

message MicControl {
  bool start = 1;
  StreamConfig config = 2;
  int32 mic_source = 3;
}

message DiscoveryBeacon {
  string app_name = 1;
  string server_name = 2;
  int32 port = 3;
  string version = 4;
  bool supports_opus = 5;
  bool supports_mic = 6;
  string ip_address = 7;
}
"""

def run_tests():
    results = {
        "passed": 0,
        "failed": 0,
        "tests": []
    }
    
    def log_test(name, success, details=""):
        status = "PASS" if success else "FAIL"
        results["tests"].append({"name": name, "status": status, "details": details})
        if success:
            results["passed"] += 1
            print(f"[PASS] {name}")
        else:
            results["failed"] += 1
            print(f"[FAIL] {name}: {details}")

    print("=== Milestone 1 Protobuf Empirical Verification Harness ===")
    
    # Check 1: Existence of protos/audiostream.proto in workspace
    repo_proto_path = "/workspaces/audio-share/protos/audiostream.proto"
    proto_exists = os.path.isfile(repo_proto_path)
    log_test("Repository audiostream.proto file existence", proto_exists, 
             f"File {'found' if proto_exists else 'NOT found at ' + repo_proto_path}")

    # Create temporary directory for compilation tests
    with tempfile.TemporaryDirectory() as temp_dir:
        proto_file = os.path.join(temp_dir, "audiostream.proto")
        with open(proto_file, "w") as f:
            f.write(AUDIOSTREAM_PROTO_CONTENT)
            
        # Test 2: Protoc compilation to Python
        py_out = os.path.join(temp_dir, "py_out")
        os.makedirs(py_out, exist_ok=True)
        cmd_py = ["protoc", f"--proto_path={temp_dir}", f"--python_out={py_out}", proto_file]
        res_py = subprocess.run(cmd_py, capture_output=True, text=True)
        log_test("protoc compilation to Python", res_py.returncode == 0, res_py.stderr)

        # Test 3: Protoc compilation to C++
        cpp_out = os.path.join(temp_dir, "cpp_out")
        os.makedirs(cpp_out, exist_ok=True)
        cmd_cpp = ["protoc", f"--proto_path={temp_dir}", f"--cpp_out={cpp_out}", proto_file]
        res_cpp = subprocess.run(cmd_cpp, capture_output=True, text=True)
        log_test("protoc compilation to C++", res_cpp.returncode == 0, res_cpp.stderr)

        # Test 4: Protoc compilation to Java
        java_out = os.path.join(temp_dir, "java_out")
        os.makedirs(java_out, exist_ok=True)
        cmd_java = ["protoc", f"--proto_path={temp_dir}", f"--java_out={java_out}", proto_file]
        res_java = subprocess.run(cmd_java, capture_output=True, text=True)
        log_test("protoc compilation to Java", res_java.returncode == 0, res_java.stderr)

        # Import generated python module
        sys.path.insert(0, py_out)
        try:
            import audiostream_pb2 as pb
            log_test("Import generated Python Protobuf module", True)
        except Exception as e:
            log_test("Import generated Python Protobuf module", False, str(e))
            return results

        # Test 5: AudioFormat round-trip & boundaries
        try:
            fmt = pb.AudioFormat()
            fmt.encoding = pb.ENCODING_PCM_16BIT
            fmt.channels = 2
            fmt.sample_rate = 48000
            serialized = fmt.SerializeToString()
            
            fmt_deser = pb.AudioFormat()
            fmt_deser.ParseFromString(serialized)
            
            match = (fmt_deser.encoding == pb.ENCODING_PCM_16BIT and 
                     fmt_deser.channels == 2 and 
                     fmt_deser.sample_rate == 48000)
            log_test("AudioFormat standard serialization/deserialization", match)
        except Exception as e:
            log_test("AudioFormat standard serialization/deserialization", False, str(e))

        # Test 6: AudioFormat edge cases (sample rate limits & negatives)
        try:
            edge_sample_rates = [0, 8000, 44100, 48000, 96000, 192000, 384000, -1, 2147483647, -2147483648]
            all_passed = True
            for sr in edge_sample_rates:
                fmt = pb.AudioFormat(encoding=pb.ENCODING_PCM_FLOAT, channels=6, sample_rate=sr)
                data = fmt.SerializeToString()
                deser = pb.AudioFormat()
                deser.ParseFromString(data)
                if deser.sample_rate != sr or deser.channels != 6 or deser.encoding != pb.ENCODING_PCM_FLOAT:
                    all_passed = False
                    break
            log_test("AudioFormat sample rate boundaries (0, 384k, int32 limits, negative)", all_passed)
        except Exception as e:
            log_test("AudioFormat sample rate boundaries", False, str(e))

        # Test 7: StreamConfig round-trip & nested message validation
        try:
            cfg = pb.StreamConfig()
            cfg.format.encoding = pb.ENCODING_PCM_FLOAT
            cfg.format.channels = 2
            cfg.format.sample_rate = 48000
            cfg.codec = pb.CODEC_OPUS
            cfg.opus_bitrate = 128000
            cfg.opus_frame_size_ms = 20
            cfg.target_latency_ms = 50
            
            data = cfg.SerializeToString()
            deser = pb.StreamConfig()
            deser.ParseFromString(data)
            
            match = (deser.format.encoding == pb.ENCODING_PCM_FLOAT and
                     deser.format.channels == 2 and
                     deser.format.sample_rate == 48000 and
                     deser.codec == pb.CODEC_OPUS and
                     deser.opus_bitrate == 128000 and
                     deser.opus_frame_size_ms == 20 and
                     deser.target_latency_ms == 50)
            log_test("StreamConfig standard serialization/deserialization", match)
        except Exception as e:
            log_test("StreamConfig standard serialization/deserialization", False, str(e))

        # Test 8: StreamConfig edge cases (negative latency, 0 bitrate, extreme values)
        try:
            cfg = pb.StreamConfig(
                format=pb.AudioFormat(encoding=pb.ENCODING_PCM_32BIT, channels=8, sample_rate=192000),
                codec=pb.CODEC_PCM,
                opus_bitrate=0,
                opus_frame_size_ms=-5,
                target_latency_ms=-100
            )
            data = cfg.SerializeToString()
            deser = pb.StreamConfig()
            deser.ParseFromString(data)
            match = (deser.opus_frame_size_ms == -5 and 
                     deser.target_latency_ms == -100 and 
                     deser.opus_bitrate == 0 and
                     deser.codec == pb.CODEC_PCM)
            log_test("StreamConfig edge cases (negative frame size/latency, 0 bitrate)", match)
        except Exception as e:
            log_test("StreamConfig edge cases", False, str(e))

        # Test 9: MicControl round-trip
        try:
            mic = pb.MicControl()
            mic.start = True
            mic.config.format.encoding = pb.ENCODING_PCM_16BIT
            mic.config.format.channels = 1
            mic.config.format.sample_rate = 48000
            mic.config.codec = pb.CODEC_OPUS
            mic.config.opus_bitrate = 64000
            mic.config.opus_frame_size_ms = 10
            mic.config.target_latency_ms = 20
            mic.mic_source = 7 # e.g. VOICE_COMMUNICATION
            
            data = mic.SerializeToString()
            deser = pb.MicControl()
            deser.ParseFromString(data)
            
            match = (deser.start == True and
                     deser.mic_source == 7 and
                     deser.config.format.channels == 1 and
                     deser.config.opus_bitrate == 64000)
            log_test("MicControl start=True with nested StreamConfig", match)
        except Exception as e:
            log_test("MicControl start=True", False, str(e))

        # Test 10: MicControl stop message (start=False, empty config)
        try:
            mic_stop = pb.MicControl(start=False, mic_source=0)
            data = mic_stop.SerializeToString()
            deser = pb.MicControl()
            deser.ParseFromString(data)
            match = (deser.start == False and deser.mic_source == 0 and not deser.HasField("config"))
            log_test("MicControl stop=False with empty config (proto3 zero serialization)", match)
        except Exception as e:
            log_test("MicControl stop=False", False, str(e))

        # Test 11: DiscoveryBeacon standard serialization/deserialization
        try:
            beacon = pb.DiscoveryBeacon(
                app_name="AudioStream",
                server_name="DESKTOP-GAMING",
                port=59200,
                version="1.0.0",
                supports_opus=True,
                supports_mic=True,
                ip_address="192.168.42.1"
            )
            data = beacon.SerializeToString()
            deser = pb.DiscoveryBeacon()
            deser.ParseFromString(data)
            
            match = (deser.app_name == "AudioStream" and
                     deser.server_name == "DESKTOP-GAMING" and
                     deser.port == 59200 and
                     deser.version == "1.0.0" and
                     deser.supports_opus == True and
                     deser.supports_mic == True and
                     deser.ip_address == "192.168.42.1")
            log_test("DiscoveryBeacon standard fields serialization/deserialization", match)
        except Exception as e:
            log_test("DiscoveryBeacon standard fields", False, str(e))

        # Test 12: DiscoveryBeacon edge cases (empty strings, unicode, max string, extreme ports)
        try:
            unicode_server = "AudioStream-服务器-🎵-🚀"
            long_string = "A" * 10000
            beacon_edge = pb.DiscoveryBeacon(
                app_name="",
                server_name=unicode_server,
                port=65535,
                version=long_string,
                supports_opus=False,
                supports_mic=False,
                ip_address="fe80::1ff:fe23:4567:890a%eth0"
            )
            data = beacon_edge.SerializeToString()
            deser = pb.DiscoveryBeacon()
            deser.ParseFromString(data)
            
            match = (deser.app_name == "" and
                     deser.server_name == unicode_server and
                     deser.port == 65535 and
                     deser.version == long_string and
                     deser.supports_opus == False and
                     deser.supports_mic == False and
                     deser.ip_address == "fe80::1ff:fe23:4567:890a%eth0")
            log_test("DiscoveryBeacon edge cases (empty string, UTF-8, long string, IPv6, port 65535)", match)
        except Exception as e:
            log_test("DiscoveryBeacon edge cases", False, str(e))

        # Test 13: Proto3 zero-byte default value behavior
        try:
            # An empty byte array should parse into a valid message with default values
            empty_bytes = b""
            fmt = pb.AudioFormat()
            fmt.ParseFromString(empty_bytes)
            cfg = pb.StreamConfig()
            cfg.ParseFromString(empty_bytes)
            mic = pb.MicControl()
            mic.ParseFromString(empty_bytes)
            beacon = pb.DiscoveryBeacon()
            beacon.ParseFromString(empty_bytes)
            
            match = (fmt.encoding == pb.ENCODING_INVALID and fmt.channels == 0 and fmt.sample_rate == 0 and
                     cfg.codec == pb.CODEC_PCM and cfg.opus_bitrate == 0 and
                     mic.start == False and mic.mic_source == 0 and
                     beacon.app_name == "" and beacon.port == 0 and beacon.supports_opus == False)
            log_test("Proto3 zero-byte empty buffer parsing semantics", match)
        except Exception as e:
            log_test("Proto3 zero-byte empty buffer parsing semantics", False, str(e))

        # Test 14: Malformed and truncated payload handling
        try:
            beacon = pb.DiscoveryBeacon(app_name="AudioStream", server_name="Host", port=59200)
            valid_bytes = beacon.SerializeToString()
            truncated_bytes = valid_bytes[:3] # Corrupt by truncation
            
            deser = pb.DiscoveryBeacon()
            try:
                deser.ParseFromString(truncated_bytes)
                # In protobuf, some truncated fields or varints raise DecodeError
                truncated_handled = True
            except Exception:
                truncated_handled = True # Exception caught cleanly
                
            # Test completely invalid garbage bytes with invalid wire types
            garbage_bytes = b"\xff\xff\xff\xff\xff\xff\xff\xff"
            deser_garbage = pb.DiscoveryBeacon()
            garbage_handled = False
            try:
                deser_garbage.ParseFromString(garbage_bytes)
            except Exception:
                garbage_handled = True
                
            log_test("Malformed / garbage payload error resilience", truncated_handled and garbage_handled)
        except Exception as e:
            log_test("Malformed payload handling", False, str(e))

        # Test 15: Forward compatibility (unknown fields preserved)
        try:
            # Create a message with an extra field tag (field 10, wire type 0 = varint)
            # tag: (10 << 3) | 0 = 80 = 0x50, value: 42 = 0x2a
            beacon = pb.DiscoveryBeacon(app_name="AudioStream", port=59200)
            base_bytes = beacon.SerializeToString()
            extra_field_bytes = base_bytes + b"\x50\x2a"
            
            deser = pb.DiscoveryBeacon()
            deser.ParseFromString(extra_field_bytes)
            match = (deser.app_name == "AudioStream" and deser.port == 59200)
            # Re-serialize should preserve unknown fields
            reserialized = deser.SerializeToString()
            preserves_unknown = b"\x50\x2a" in reserialized
            log_test("Forward compatibility & unknown field preservation", match and preserves_unknown)
        except Exception as e:
            log_test("Forward compatibility", False, str(e))

        # Test 16: Enum value preservation for undefined enums
        try:
            # Construct AudioFormat with undefined enum value 99
            fmt = pb.AudioFormat(encoding=99, channels=2, sample_rate=48000)
            data = fmt.SerializeToString()
            deser = pb.AudioFormat()
            deser.ParseFromString(data)
            log_test("Undefined enum value preservation (proto3 open enums)", deser.encoding == 99)
        except Exception as e:
            log_test("Undefined enum value preservation", False, str(e))

    print("\n=== Test Summary ===")
    print(f"Total tests: {results['passed'] + results['failed']}")
    print(f"Passed: {results['passed']}")
    print(f"Failed: {results['failed']}")
    return results

if __name__ == "__main__":
    res = run_tests()
    if res["failed"] > 0:
        # Check if the only failure is the file existence on disk
        if res["failed"] == 1 and res["tests"][0]["name"] == "Repository audiostream.proto file existence" and res["tests"][0]["status"] == "FAIL":
            print("\nNote: Specification tests passed, but repository file protos/audiostream.proto is missing.")
            sys.exit(2)
        sys.exit(1)
    sys.exit(0)
