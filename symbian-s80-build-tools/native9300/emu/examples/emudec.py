#!/usr/bin/env python3
"""1.13: VtsDecode (vts3d.cpp) in the Unicorn ARM emulator: the instructions by function, the calls of the
double-precision helpers (libgcc's here; on the phone EUSER's), and a hash of the mesh (old and new code
must agree, or say how far apart). emudec.py emudec.exe emudec.sym [-p] [-h] [-cell N] tiles.bin..."""
import struct, sys, gzip, bisect, collections
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_BLOCK, UcError
from unicorn.arm_const import *
exe, symf = sys.argv[1], sys.argv[2]
args = sys.argv[3:]
cellArg = None
if "-cell" in args: i = args.index("-cell"); cellArg = int(args[i + 1]); del args[i:i + 2]
files = [a for a in args if not a.startswith("-")]
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
RET, STACK, BLOB, OUT = 0x2000, 0x300000, 0x3000000, 0x2800000
blocks = collections.Counter()
uc.hook_add(UC_HOOK_BLOCK, lambda uc, a, s, d: blocks.__setitem__((a, s), blocks[(a, s)] + 1))
def call(name, *a):
    for r, v in zip([UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3], a): uc.reg_write(r, v)
    uc.reg_write(UC_ARM_REG_SP, STACK); uc.reg_write(UC_ARM_REG_LR, RET); blocks.clear()
    try: uc.emu_start(syms[name], RET)
    except UcError as e:
        pc = uc.reg_read(UC_ARM_REG_PC); f = addrs[bisect.bisect_right(addrs, pc) - 1]
        print("FAULT in %s: %s at 0x%x (%s+0x%x)" % (name, e, pc, fn[f], pc - f)); sys.exit(1)
    return uc.reg_read(UC_ARM_REG_R0)
FP = ["__adddf3", "__subdf3", "__muldf3", "__divdf3", "__floatsidf", "__fixdfsi", "__cmpdf2", "__eqdf2", "__nedf2", "__gtdf2", "__gedf2", "__ltdf2", "__ledf2", "__negdf2", "sqrt", "__divsi3", "__udivsi3", "__modsi3"]
call("emu_init")
call("emu_frame", 49854144, 18541832)
cells = {17: 64, 18: 32, 19: 16, 20: 8, 21: 0}
tot = collections.Counter(); calls = collections.Counter(); ninstr = 0
for f in files:
    raw = open(f, "rb").read()
    if raw[:2] == b"\x1f\x8b": raw = gzip.decompress(raw)
    lod = int(f.split("/")[-1].split("-")[0])
    cell = cellArg if cellArg is not None else cells.get(lod, 0)
    uc.mem_write(BLOB, raw)
    mark = call("emu_mark")
    call("emu_dec", BLOB, len(raw), cell, OUT)
    r = struct.unpack("<6i", uc.mem_read(OUT, 24))
    n = 0
    for (a, sz), c in blocks.items():
        fa = addrs[bisect.bisect_right(addrs, a) - 1]
        tot[fn[fa]] += sz // 4 * c; n += sz // 4 * c
        if a == fa and fn[fa] in FP: calls[fn[fa]] += c
    ninstr += n
    if "-h" in sys.argv: print("%-28s r=%d nv=%d nf=%d of %d  mesh %08x shade %08x  %d instr" % (f.split("/")[-1], r[0], r[1], r[2], r[3], r[4] & 0xffffffff, r[5] & 0xffffffff, n))
    call("emu_release", mark)
print("%d files: %d instructions, %d a file" % (len(files), ninstr, ninstr // max(1, len(files))))
if "-p" in sys.argv:
    for k, c in tot.most_common(16): print("   %10d %5.1f%%  %s" % (c, 100.0 * c / ninstr, k))
    print("   calls:", ", ".join("%s %d" % (k, c) for k, c in calls.most_common()))
