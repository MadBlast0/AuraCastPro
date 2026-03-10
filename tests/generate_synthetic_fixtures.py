#!/usr/bin/env python3
"""
generate_synthetic_fixtures.py — Task 088
Generates minimal synthetic binary test fixtures for unit tests that don't
need real video frames (NAL parser, packet validator, FEC tests, etc.)

These fixtures work WITHOUT ffmpeg — they are raw binary byte sequences.
Run this script once before building/running tests:
    python tests/generate_synthetic_fixtures.py

For real video fixtures (integration tests), use generate_fixtures.bat
which requires ffmpeg.
"""

import struct, os, sys

FIXTURES_DIR = os.path.join(os.path.dirname(__file__), "fixtures")
os.makedirs(FIXTURES_DIR, exist_ok=True)

def write(name, data):
    path = os.path.join(FIXTURES_DIR, name)
    with open(path, "wb") as f:
        f.write(data)
    print(f"  wrote {name} ({len(data)} bytes)")

# ── H.265 NAL unit start codes ────────────────────────────────────────────────
# Annex B start code: 0x00 0x00 0x00 0x01
START_CODE = b'\x00\x00\x00\x01'

def nal_unit(nal_type_h265, payload=b'\x00' * 32):
    """Build a minimal H.265 NAL unit with Annex B start code."""
    # NAL header: 2 bytes  [forbidden_zero(1) | nal_unit_type(6) | nuh_layer_id(6) | nuh_temporal_id_plus1(3)]
    header = ((nal_type_h265 & 0x3F) << 9) | (1)  # layer=0, temporal=1
    hdr = struct.pack(">H", header)
    return START_CODE + hdr + payload

# H.265 NAL unit types
VPS_NUT   = 32   # Video Parameter Set
SPS_NUT   = 33   # Sequence Parameter Set
PPS_NUT   = 34   # Picture Parameter Set
IDR_W_RADL = 19  # IDR keyframe
TRAIL_R   = 1    # Non-IDR P/B frame

# 1. Minimal H.265 keyframe (VPS + SPS + PPS + IDR)
h265_keyframe = (
    nal_unit(VPS_NUT,    b'\x70' + b'\x00' * 10)  +
    nal_unit(SPS_NUT,    b'\x42\x01' + b'\x00' * 20) +
    nal_unit(PPS_NUT,    b'\xc0' + b'\x00' * 8)   +
    nal_unit(IDR_W_RADL, b'\xaf\x9f' + b'\x00' * 50)
)
write("h265_keyframe.bin", h265_keyframe)

# 2. Minimal H.265 inter frame (P-frame, non-IDR)
h265_pframe = nal_unit(TRAIL_R, b'\x02\x80' + b'\x00' * 30)
write("h265_pframe.bin", h265_pframe)

# 3. Concatenated stream (keyframe + 3 P-frames) — tests NAL parser continuity
h265_stream = h265_keyframe + h265_pframe * 3
write("h265_stream.bin", h265_stream)

# ── H.264 NAL units ───────────────────────────────────────────────────────────
def nal_unit_h264(nal_type, payload=b'\x00' * 20):
    hdr = bytes([nal_type & 0x1F])
    return START_CODE + hdr + payload

SPS_H264 = 7; PPS_H264 = 8; IDR_H264 = 5; NONIDR_H264 = 1

h264_keyframe = (
    nal_unit_h264(SPS_H264,   b'\x42\xc0\x28' + b'\x00' * 15) +
    nal_unit_h264(PPS_H264,   b'\xe8\x43\x8f' + b'\x00' * 8)  +
    nal_unit_h264(IDR_H264,   b'\x65\x88' + b'\x00' * 40)
)
write("h264_keyframe.bin", h264_keyframe)
write("h264_pframe.bin", nal_unit_h264(NONIDR_H264, b'\x61\xe0' + b'\x00' * 20))

# ── RTP packets (UDP payload format for ReceiverSocket tests) ─────────────────
def rtp_packet(seq, timestamp, ssrc=0xDEADBEEF, payload=b'\x00' * 100):
    """Build a minimal RTP/2 header + payload."""
    # Byte 0: V=2 P=0 X=0 CC=0  → 0x80
    # Byte 1: M=0 PT=96 (H.265 dynamic) → 0x60
    header = struct.pack(">BBHII",
        0x80,   # V=2, P=0, X=0, CC=0
        0x60,   # M=0, PT=96
        seq & 0xFFFF,
        timestamp & 0xFFFFFFFF,
        ssrc & 0xFFFFFFFF)
    return header + payload

# Ordered sequence of 8 RTP packets
rtp_stream = b''.join(
    rtp_packet(i, i * 3000, payload=b'\xAB' * 80 + bytes([i % 256]))
    for i in range(8)
)
write("rtp_8packets.bin", rtp_stream)

# Out-of-order packet sequence (for PacketReorderCache tests)
# Packets 0,2,1,4,3 — reorder cache should output 0,1,2,3,4
ooo_stream = b''.join([
    rtp_packet(0, 0),
    rtp_packet(2, 6000),
    rtp_packet(1, 3000),
    rtp_packet(4, 12000),
    rtp_packet(3, 9000),
])
write("rtp_ooo_5packets.bin", ooo_stream)

# ── Corrupt / edge-case fixtures ─────────────────────────────────────────────
# Empty file (0 bytes)
write("empty.bin", b'')
# Only a start code, no payload (truncated NAL)
write("truncated_startcode.bin", b'\x00\x00\x00\x01')
# Random bytes (not valid video data)
import random; random.seed(42)
write("random_noise.bin", bytes(random.randint(0, 255) for _ in range(256)))
# Very long single NAL (stress test)
write("large_nal.bin", START_CODE + bytes([IDR_W_RADL << 1, 1]) + b'\xAB' * 65536)

print(f"\nAll synthetic fixtures written to: {FIXTURES_DIR}")
print("For real video fixtures (integration tests): run generate_fixtures.bat (requires ffmpeg)")
