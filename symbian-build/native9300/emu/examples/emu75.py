#!/usr/bin/env python3
"""0.75: r3_span_tex4 against r3_span_tex: random spans, then every benchmark scene and the panorama
drawn both ways (pixel-identical frames, instructions). emu75.py emu.exe emu.sym"""
import struct, sys, hashlib
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_BLOCK, UcError
from unicorn.arm_const import *
exe, symf = sys.argv[1], sys.argv[2]
data = open(exe, "rb").read()
pe = struct.unpack_from("<I", data, 0x3C)[0]
nsec = struct.unpack_from("<H", data, pe + 6)[0]; optsz = struct.unpack_from("<H", data, pe + 20)[0]
base = struct.unpack_from("<I", data, pe + 24 + 28)[0]; sec = pe + 24 + optsz
uc = Uc(UC_ARCH_ARM, UC_MODE_ARM); uc.mem_map(0x1000, 0x4000000 - 0x1000)
for i in range(nsec):
    name, vsize, va, rawsz, rawptr = struct.unpack_from("<8sIIII", data, sec + i * 40)
    if rawsz and not name.startswith(b".stab"): uc.mem_write(base + va, data[rawptr:rawptr + rawsz])
syms = {}
for line in open(symf):
    p = line.split()
    if len(p) == 3: syms[p[2]] = int(p[0], 16)
RET, STACK, TEX, FB, BASE, A, B = 0x2000, 0x300000, 0x2000000, 0x2100000, 0x2400000, 0x2800000, 0x2801000
count = [0]
uc.hook_add(UC_HOOK_BLOCK, lambda uc, a, s, d: count.__setitem__(0, count[0] + s // 4))
def call(name, *args):
    for r, v in zip([UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3], args): uc.reg_write(r, v)
    uc.reg_write(UC_ARM_REG_SP, STACK); uc.reg_write(UC_ARM_REG_LR, RET); count[0] = 0
    try: uc.emu_start(syms[name], RET)
    except UcError as e:
        pc = uc.reg_read(UC_ARM_REG_PC); near = max((a, n) for n, a in syms.items() if a <= pc)
        print("FAULT in %s: %s at 0x%x (%s+0x%x)" % (name, e, pc, near[1], pc - near[0])); sys.exit(1)
    return uc.reg_read(UC_ARM_REG_R0), count[0]
call("emu_init", TEX)
# a 1024 x 512 picture with detail everywhere
import random
rnd = random.Random(5)
uc.mem_write(BASE, bytes(rnd.getrandbits(8) for _ in range(1024 * 512 * 2)))
bad, n = call("emu_spantest", A, B, 20000)
print("random spans: %d of 20000 differ" % bad)
fails = bad
gb, _ = call("emu_geotest", 200000)
print("random geometry: %d of 200000 differ" % gb)
fails += gb
def frame(): return hashlib.md5(bytes(uc.mem_read(FB, 640 * 200 * 2))).hexdigest()
rows = [("fill affine", lambda: call("emu_scene", FB, 1, 1)), ("1000 triangles affine", lambda: call("emu_scene", FB, 10, 1)),
        ("4000 triangles affine", lambda: call("emu_scene", FB, 13, 1))]
for k, nm in enumerate(["panorama, whole picture", "panorama, 128 px tiles", "panorama, 256 px tiles", "panorama up, whole picture", "panorama up, 128 px tiles", "panorama up, 256 px tiles"]):
    rows.append((nm, (lambda kk: lambda: call("emu_pano", FB, BASE, kk, 1))(k)))
print("%-28s %12s %12s %7s  %s" % ("scene", "0.59 span", "4-pixel", "ratio", "frames"))
for nm, fn in rows:
    call("emu_mode", 0); pa, a = fn(); fa = frame()
    call("emu_mode", 1); pb, b = fn(); fb = frame()
    same = fa == fb and pa == pb
    fails += 0 if same else 1
    print("%-28s %12d %12d %6.0f%%  %s" % (nm, a, b, 100.0 * b / a, "same" if same else "DIFFER"))
print("OK" if fails == 0 else "FAILED")
