#!/usr/bin/env python3
"""0.92: the 3D view's city tiles (VtsDraw) drawn as on the phone in the Unicorn ARM emulator: for each
scene (test/vtstest.cpp DUMP=) and way (the corners per triangle, or once a mesh: MeshBegin), the
instructions, the busiest functions and the frame's hash (the ways must draw the same frame).
  emuvts.py emuvts.exe emuvts.sym scene.bin... [-p: per function]"""
import struct, sys, hashlib, bisect, collections
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_BLOCK, UcError
from unicorn.arm_const import *
exe, symf = sys.argv[1], sys.argv[2]
scenes = [a for a in sys.argv[3:] if not a.startswith("-")]
prof = "-p" in sys.argv
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
BUDGET = 5000
RET, STACK, FB, BLOB, ST = 0x2000, 0x300000, 0x2000000, 0x3000000, 0x2800000
blocks = collections.Counter()
uc.hook_add(UC_HOOK_BLOCK, lambda uc, a, s, d: blocks.__setitem__((a, s), blocks[(a, s)] + 1))
# 1.12: -c: a data cache model (the 9300's OMAP1510 ARM925T: 8 KB D-cache; here 2-way, 16-byte lines,
# reads allocate): the read misses a frame (memory stalls, which the instruction counts don't show)
CACHE = "-c" in sys.argv
if CACHE:
    from unicorn import UC_HOOK_MEM_READ
    LINE, WAYS, SETS = 16, 2, 8192 // 16 // 2
    tags = [[-1] * WAYS for _ in range(SETS)]
    miss = [0, 0]
    missby = collections.Counter()
    def memhook(uc, access, addr, size, value, data):
        ln = addr // LINE
        st = tags[ln % SETS]
        miss[1] += 1
        if st[0] == ln: return
        if st[1] == ln: st[0], st[1] = st[1], st[0]; return
        miss[0] += 1
        st[1] = st[0]; st[0] = ln
        pc = uc.reg_read(UC_ARM_REG_PC)
        missby[fn[addrs[bisect.bisect_right(addrs, pc) - 1]]] += 1
    uc.hook_add(UC_HOOK_MEM_READ, memhook)
def call(name, *args):
    for r, v in zip([UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3], args): uc.reg_write(r, v)
    uc.reg_write(UC_ARM_REG_SP, STACK); uc.reg_write(UC_ARM_REG_LR, RET); blocks.clear()
    try: uc.emu_start(syms[name], RET)
    except UcError as e:
        pc = uc.reg_read(UC_ARM_REG_PC); a = addrs[bisect.bisect_right(addrs, pc) - 1]
        print("FAULT in %s: %s at 0x%x (%s+0x%x)" % (name, e, pc, fn[a], pc - a)); sys.exit(1)
    return uc.reg_read(UC_ARM_REG_R0)
call("emu_init")
if "-t" in sys.argv:
    print("random corners and triangles (1500 x 64): %d differ" % call("emu_vtstest", 1500))
WAYS = [("per triangle", 2), ("mesh", 3), ("mesh, chunks", 0x103), ("chunks, C", 0x107)] if "-only" not in sys.argv else [("mesh", 3), ("mesh, chunks", 0x103)] + [(a[2:], int(a[2:])) for a in sys.argv if a.startswith("-w")]
fails = 0
for sc in scenes:
    blob = open(sc, "rb").read()
    uc.mem_write(BLOB, blob)
    w, h = struct.unpack_from("<II", blob, 0)
    ref = None
    if "emu_prep" in syms: call("emu_prep", BLOB)
    for nm, flags in WAYS:
        if CACHE: miss[0] = miss[1] = 0; missby.clear()
        n = call("emu_vts", FB, BLOB, flags, BUDGET)
        snap = dict(blocks)
        if CACHE:
            print("   data reads %d, misses %d (%.1f %%)" % (miss[1], miss[0], 100.0 * miss[0] / max(1, miss[1])))
            for k, c in missby.most_common(6): print("      misses %7d  %s" % (c, k))
        tot = sum(sz // 4 * c for (a, sz), c in blocks.items())
        per = collections.Counter()
        for (a, sz), c in blocks.items(): per[fn[addrs[bisect.bisect_right(addrs, a) - 1]]] += sz // 4 * c
        call("emu_stats", ST); st = struct.unpack("<4i", uc.mem_read(ST, 16))
        hsh = hashlib.md5(bytes(uc.mem_read(FB, w * h * 2))).hexdigest()[:8]
        if ref is None: ref = hsh
        same = hsh == ref
        fails += 0 if same else 1
        print("%-14s %-14s %10d instr  %5d submitted, %4d drawn, %5d culled, %6d px  %s %s" % (sc.split("/")[-1], nm, tot, st[0], st[1], st[2], st[3], hsh, "" if same else "DIFFERS"))
        if prof:
            for f, c in per.most_common(8): print("     %10d %5.1f%%  %s" % (c, 100.0 * c / tot, f))
        if "-b" in sys.argv and flags == 3:
            hot = sorted(((sz // 4 * c, a, sz, c) for (a, sz), c in snap.items()), reverse=True)[:30]
            for n, a, sz, c in hot:
                f = addrs[bisect.bisect_right(addrs, a) - 1]
                print("   0x%x %s+0x%x: %d instr x %d = %d" % (a, fn[f][:30], a - f, sz // 4, c, n))
        if "-ppm" in sys.argv:
            px = struct.unpack("<%dH" % (w * h), uc.mem_read(FB, w * h * 2))
            with open(sc + ".%d.ppm" % flags, "wb") as o:
                o.write(b"P6 %d %d 255\n" % (w, h))
                o.write(bytes(b for c in px for b in (((c >> 11) & 31) << 3, ((c >> 5) & 63) << 2, (c & 31) << 3)))
print("OK" if fails == 0 else "FAILED")
