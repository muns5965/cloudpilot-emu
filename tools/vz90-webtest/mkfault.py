#!/usr/bin/env python3
# local test tool: copy a DAL bundle with instructions of one function replaced, hashes recomputed (format 1)
# usage: mkfault.py <bundle> <out> <hex address> <hex word> [<hex word> ...]
import hashlib, struct, sys
b = bytearray(open(sys.argv[1], 'rb').read()); out = sys.argv[2]; addr = int(sys.argv[3], 16)
off, ln = struct.unpack_from('<II', b, 0x30 + 8)                  # resource 0 = boot, linked at 0x10000230
at = off + (addr - 0x10000230)
assert off <= at < off + ln, 'address outside boot'
for k, w in enumerate(sys.argv[4:]): struct.pack_into('<I', b, at + 4 * k, int(w, 16))
b[0x60:0x80] = hashlib.sha256(b[off:off + ln]).digest()
b[0xC0:0xE0] = b'\0' * 32; b[0xC0:0xE0] = hashlib.sha256(b).digest()
open(out, 'wb').write(b); print('patched', hex(addr), sys.argv[4:])
