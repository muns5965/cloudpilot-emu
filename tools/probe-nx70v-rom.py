#!/usr/bin/env python3
"""Inspect Palm OS 5 NOR metadata without executing ROM code."""
import argparse
import hashlib
from pathlib import Path

SIGNATURE = bytes.fromhex("efbeedfe")  # little-endian 0xFEEDBEEF
KNOWN_NX70V = (0x736F6E79, 0x63726462)  # company 'sony', HAL 'crdb'


def inspect(path):
    raw = Path(path).read_bytes()
    print(f"file: {path}")
    print(f"size: {len(raw)} bytes")
    print(f"sha256: {hashlib.sha256(raw).hexdigest()}")
    headers = [start for start in range(0, len(raw) - 12, 32768)
               if raw[start + 8:start + 12] == SIGNATURE]
    if not headers:
        print("no Palm OS ROM header on a 32-KiB boundary")
        return 1
    for pos in headers:
        if pos + 0xac > len(raw):
            continue
        def text_at(off):
            return raw[pos + off:pos + off + 32].split(b"\0", 1)[0].decode("latin1")
        company = int.from_bytes(raw[pos + 0x80:pos + 0x84], "little")
        hal = int.from_bytes(raw[pos + 0x84:pos + 0x88], "little")
        print(f"header: 0x{pos:x}")
        print(f"card: {text_at(0x10)}")
        print(f"manufacturer: {text_at(0x30)}")
        print(f"rom_name: {text_at(0x8c)}")
        print(f"company_id: 0x{company:08x}")
        print(f"hal_id: 0x{hal:08x}")
        print(f"NX70V header pair: {'match' if (company, hal) == KNOWN_NX70V else 'not matched'}")
    return 0


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("rom", help="raw NOR ROM image (.rom/.bin), not SplitROM ZIP")
    args = parser.parse_args()
    raise SystemExit(inspect(args.rom))
