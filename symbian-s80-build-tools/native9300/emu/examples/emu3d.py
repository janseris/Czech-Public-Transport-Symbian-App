#!/usr/bin/env python3
"""Runs the 3D renderer as the phone's GCC 2.9 builds it (r3d.cpp + drv.cpp, linked with the S80 ld) in
the Unicorn ARM emulator: the benchmark's scenes, the instructions each takes (geometry and whole frames),
and any memory fault (what would be a KERN-EXEC 3 on the phone). See build.sh.

  emu3d.py emu.exe emu.sym [out.ppm [scene to save, default the last]]
"""
import re, struct, sys, time
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_BLOCK, UcError
from unicorn.arm_const import *

exe, symf = sys.argv[1], sys.argv[2]
out = sys.argv[3] if len(sys.argv) > 3 else None
data = open(exe, "rb").read()
# PE: the section table
pe = struct.unpack_from("<I", data, 0x3C)[0]
nsec = struct.unpack_from("<H", data, pe + 6)[0]
optsz = struct.unpack_from("<H", data, pe + 20)[0]
base = struct.unpack_from("<I", data, pe + 24 + 28)[0]
sec = pe + 24 + optsz
uc = Uc(UC_ARCH_ARM, UC_MODE_ARM)
MEM = 0x4000000
uc.mem_map(0x1000, MEM - 0x1000)            # page 0 unmapped: NULL pointers fault
for i in range(nsec):
    name, vsize, va, rawsz, rawptr = struct.unpack_from("<8sIIII", data, sec + i * 40)
    if rawsz and not name.startswith(b".stab"):
        uc.mem_write(base + va, data[rawptr:rawptr + rawsz])
syms = {}
for line in open(symf):
    p = line.split()
    if len(p) == 3: syms[p[2]] = int(p[0], 16)
RET, STACK, TEX, FB = 0x2000, 0x300000, 0x2000000, 0x2100000
count = [0]
def blk(uc, addr, size, ud): count[0] += size // 4
uc.hook_add(UC_HOOK_BLOCK, blk)

def call(name, *args):
    for r, v in zip([UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3], args): uc.reg_write(r, v)
    uc.reg_write(UC_ARM_REG_SP, STACK)
    uc.reg_write(UC_ARM_REG_LR, RET)
    count[0] = 0
    try:
        uc.emu_start(syms[name], RET)
    except UcError as e:
        pc = uc.reg_read(UC_ARM_REG_PC)
        near = max((a, n) for n, a in syms.items() if a <= pc)
        print("FAULT in %s: %s at 0x%x (%s+0x%x)" % (name, e, pc, near[1], pc - near[0]))
        sys.exit(1)
    return uc.reg_read(UC_ARM_REG_R0), count[0]

call("emu_init", TEX)
names = ["fill flat", "fill affine", "fill perspective"]
for n in (500, 1000, 2000, 4000):
    for k in ("flat", "affine", "perspective"): names.append("%d triangles %s" % (n, k))
print("%-26s %12s %12s %12s  %s" % ("scene", "geometry", "rasterising", "all", "ms at 150 MHz, ~1.3 cycles/instr"))
for t, nm in enumerate(names):
    t0 = time.time()
    _, g = call("emu_scene", FB, t, 0)
    drawn, a = call("emu_scene", FB, t, 1)
    ms = a * 1.3 / 150000.0
    print("%-26s %12d %12d %12d  %.1f ms (%d drawn)" % (nm, g, a - g, a, ms, drawn))
    sys.stdout.flush()
if out:
    dump = int(sys.argv[4]) if len(sys.argv) > 4 else len(names) - 1
    call("emu_scene", FB, dump, 1)
    fb = uc.mem_read(FB, 640 * 200 * 2)
    with open(out, "wb") as o:
        o.write(b"P6 640 200 255\n")
        for i in range(640 * 200):
            c = fb[i * 2] | (fb[i * 2 + 1] << 8)
            o.write(bytes([((c >> 11) & 31) << 3, ((c >> 5) & 63) << 2, (c & 31) << 3]))
