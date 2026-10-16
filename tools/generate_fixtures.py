"""Deterministic synthetic decoder tests; no captured temperatures or networks."""
import json
from pathlib import Path
import struct

def header(name, values):
    lines = ["inline constexpr uint8_t " + name + "[] = {"]
    lines += ["    " + ",".join("0x%02x" % v for v in values[i:i+20]) + ","
              for i in range(0, len(values), 20)]
    return "\n".join(lines + ["};", ""])

def frame(kind, payload):
    size = len(payload) + 10
    data = bytearray([0x68, size & 255, size >> 8, 0, 69, 48, 5, kind]) + payload
    checksum = 0
    for value in data:
        checksum ^= value
    return data + bytes([checksum, 0x16])

def generate(profile_path, destination):
    profile = json.loads(Path(profile_path).read_text(encoding="ascii"))
    schema = bytes.fromhex(profile["schema_payload"])
    count = int.from_bytes(schema[:2], "little")
    assert len(schema) == 2 + count * 3 and count == 194
    payload = bytearray.fromhex(profile["schema_prefix"]) + bytes([0, 1, 1, 85])
    payload += struct.pack("<H", profile["schema_version"])
    expectations, bits, bit_count = [], 0, 0
    for pos in range(count):
        kind, identifier = struct.unpack_from("<BH", schema, 2 + pos * 3)
        value = identifier % 50 + 1
        if kind != 10 and bit_count:
            payload.append(bits)
            bits, bit_count = 0, 0
        if kind == 10:
            value = identifier % 2
            bits |= value << bit_count
            bit_count += 1
            if bit_count == 8:
                payload.append(bits)
                bits, bit_count = 0, 0
        elif kind in (0, 8):
            continue
        elif kind in (11, 12):
            payload += b"Example-WiFi\0"
            continue
        elif kind == 15:
            payload += bytes([192, 0, 2, value])
            continue
        elif kind == 16:
            payload += bytes.fromhex("20010db8") + bytes(12)
            continue
        else:
            formats = {1:"b", 2:"h", 3:"i", 4:"B", 5:"H", 6:"I", 7:"f", 9:"d", 13:"q", 14:"Q"}
            if kind in (7, 9):
                value += .25
            payload += struct.pack("<" + formats[kind], value)
        if kind not in (13, 14):
            expectations.append((identifier, value))
    if bit_count:
        payload.append(bits)
    protocol = "#pragma once\n#include <cstdint>\n// Synthetic test values; profile metadata only. Never transmitted.\nnamespace protocol_fixtures {\n"
    protocol += header("frame_8", frame(8, payload))
    protocol += header("frame_213", frame(213, schema))
    protocol += header("frame_177", frame(177, bytes([0, 0, 139]) + bytes([0, 0, 100]) * 139))
    protocol += header("frame_178", frame(178, bytes([0, 0, 14, 4]) + bytes([0, 0, 100]) * 56))
    protocol += header("frame_212", frame(212, bytes.fromhex(profile["current_parameter_payload"])))
    protocol += "struct Value { unsigned id; double value; };\ninline constexpr Value values[] = {\n"
    # The wire schema can repeat an ID; the lookup in the real self-test selects its last occurrence.
    protocol += "".join("    {%d, %.8f},\n" % pair for pair in dict(expectations).items()) + "};\n}\n"
    sensor = "#pragma once\n#include <cstddef>\n#include <cstdint>\n// Synthetic protocol test cases, not recordings.\n"
    for index in range(3):
        data = bytearray([1, 85]) + struct.pack("<H", profile["schema_version"])
        data += bytes([5]) + struct.pack("<II", 5, 0) + bytes([4])
        for key, value in [(0, 55.0 + index), (2, 45.0 + index), (3, 8.0), (5, 90.0)]:
            data += bytes([key]) + struct.pack("<f", value)
        data += bytes([75, 2, 44, 0, 0, 50, 12])
        data += struct.pack("<fBffB", 25.0, 30, 4.5, 1.0, 0)
        data += bytes([8, 41, 38, ord("L"), 0, 255, 255, 255, 1, 21, 38, 8, 30, 28])
        data += bytes([255, 1, 3])
        for slot in range(3):
            data += struct.pack("<Bff", 2 if slot == 0 else 255,
                                21.0 if slot == 0 else float("nan"), 22.0 if slot == 0 else float("nan"))
        data += bytes([4])
        for slot in range(4):
            data += struct.pack("<fBBBB", 36.0 if slot == 0 else float("nan"), 40, 0, 1, 0)
        sensor += header("sensor_fixture_" + str(index), data)
    sensor += "struct SensorFixture { const uint8_t *data; size_t size; float heating, water; };\nstatic const SensorFixture sensor_fixtures[] = {\n"
    sensor += "".join("    {sensor_fixture_%d, sizeof(sensor_fixture_%d), %.1ff, %.1ff},\n" % (i, i, 55+i, 45+i) for i in range(3))
    sensor += "};\nconstexpr size_t sensor_fixture_count = sizeof(sensor_fixtures) / sizeof(sensor_fixtures[0]);\n"
    destination = Path(destination)
    destination.mkdir(parents=True, exist_ok=True)
    (destination / "protocol_fixtures.hpp").write_text(protocol, encoding="ascii")
    (destination / "sensor_fixtures.hpp").write_text(sensor, encoding="ascii")

if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", required=True)
    args = parser.parse_args()
    generate(Path(__file__).resolve().parent.parent / "profiles/ecomax860p_lz_touch.json", args.out)
