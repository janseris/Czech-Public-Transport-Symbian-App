#!/usr/bin/env python3
"""1.11: JPEG decoding as on the phone (jpegdec.cpp, GCC 2.9) in the emulator: full and half size, the
instructions and the busiest functions. emujpg.py emuvts.exe emuvts.sym photo.jpg... (build: buildvts.sh)"""
import struct, sys, bisect, collections
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_BLOCK
from unicorn.arm_const import *
exe, symf = sys.argv[1], sys.argv[2]
data = open(exe, "rb").read()
pe = struct.unpack_from("<I", data, 0x3C)[0]
nsec = struct.unpack_from("<H", data, pe + 6)[0]; optsz = struct.unpack_from("<H", data, pe + 20)[0]
base = struct.unpack_from("<I", data, pe + 24 + 28)[0]; sec = pe + 24 + optsz
uc = Uc(UC_ARCH_ARM, UC_MODE_ARM); uc.mem_map(0x1000, 0x8000000 - 0x1000)
for i in range(nsec):
    name, vsize, va, rawsz, rawptr = struct.unpack_from("<8sIIII", data, sec + i * 40)
    if rawsz and not name.startswith(b".stab"): uc.mem_write(base + va, data[rawptr:rawptr + rawsz])
syms = {}; fn = {}
for line in open(symf):
    p = line.split()
    if len(p) == 3:
        syms[p[2]] = int(p[0], 16)
        if p[1] in "Tt" and not p[2].startswith(".L"): fn[int(p[0], 16)] = p[2]
addrs = sorted(fn)
blocks = collections.Counter()
uc.hook_add(UC_HOOK_BLOCK, lambda uc, a, s, d: blocks.__setitem__((a, s), blocks[(a, s)] + 1))
def call(name, *args):
    for r, v in zip([UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3], args): uc.reg_write(r, v)
    uc.reg_write(UC_ARM_REG_SP, 0x300000); uc.reg_write(UC_ARM_REG_LR, 0x2000); blocks.clear()
    uc.emu_start(syms[name], 0x2000)
    return uc.reg_read(UC_ARM_REG_R0)
call("emu_init")
tot = [0, 0]
for f in [a for a in sys.argv[3:] if not a.startswith("-")]:
    d = open(f, "rb").read()
    uc.mem_write(0x3000000, d)
    res = []
    for half in (0, 1):
        r = call("emu_jpeg", 0x3000000, len(d), 0x2000000, half)
        n = sum(sz // 4 * c for (a, sz), c in blocks.items())
        if "-h" in sys.argv:
            w, hh = struct.unpack(">HH", d[d.find(b"\xff\xc0") + 5:d.find(b"\xff\xc0") + 9])[::-1] if d.find(b"\xff\xc0") >= 0 else (0, 0)
            ow, oh = ((w + 1) // 2, (hh + 1) // 2) if half else (w, hh)
            px = struct.unpack("<%dH" % (ow * oh), uc.mem_read(0x2000000, ow * oh * 2))
            hs = 2166136261
            for v in px: hs = ((hs ^ v) * 16777619) & 0xffffffff
            print("%s %d %dx%d r=%d %08x" % (f, half, ow, oh, r, hs))
        tot[half] += n
        res.append((r, n))
        if "-p" in sys.argv:
            per = collections.Counter()
            for (a, sz), c in blocks.items(): per[fn[addrs[bisect.bisect_right(addrs, a) - 1]]] += sz // 4 * c
            for k, c in per.most_common(6): print("     %s %9d %5.1f%%  %s" % ("half" if half else "full", c, 100.0 * c / n, k))
        if "-b" in sys.argv and half:
            hot = sorted(((sz // 4 * c, a, sz, c) for (a, sz), c in blocks.items()), reverse=True)[:25]
            for nn, a, sz, c in hot:
                f0 = addrs[bisect.bisect_right(addrs, a) - 1]
                print("   0x%x %s+0x%x: %d instr x %d = %d" % (a, fn[f0][:24], a - f0, sz // 4, c, nn))
    if "-h" not in sys.argv: print("%-30s full %8d (%d)  half %8d (%d)  %.0f%%" % (f.split("/")[-1], res[0][1], res[0][0], res[1][1], res[1][0], 100.0 * res[1][1] / res[0][1]))
if "-h" not in sys.argv: print("all: full %d, half %d: %.0f%%" % (tot[0], tot[1], 100.0 * tot[1] / max(1, tot[0])))
