#!/usr/bin/env python3
"""Every benchmark scene and the panorama scenes: instructions and an MD5 of the frame (to compare builds)."""
import struct, sys, hashlib, random
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_BLOCK
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
count = [0]
uc.hook_add(UC_HOOK_BLOCK, lambda uc, a, s, d: count.__setitem__(0, count[0] + s // 4))
def call(name, *args):
    for r, v in zip([UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3], args): uc.reg_write(r, v)
    uc.reg_write(UC_ARM_REG_SP, 0x300000); uc.reg_write(UC_ARM_REG_LR, 0x2000); count[0] = 0
    uc.emu_start(syms[name], 0x2000)
    return uc.reg_read(UC_ARM_REG_R0), count[0]
FB, BASE = 0x2100000, 0x2400000
call("emu_init", 0x2000000)
rnd = random.Random(5)
uc.mem_write(BASE, bytes(rnd.getrandbits(8) for _ in range(1024 * 512 * 2)))
for t in range(15):
    if t % 3 == 2 and t > 2: continue        # perspective: slow, unused
    _, g = call("emu_scene", FB, t, 0); n, a = call("emu_scene", FB, t, 1)
    print("scene %2d geometry %9d all %9d  %s" % (t, g, a, hashlib.md5(bytes(uc.mem_read(FB, 256000))).hexdigest()[:12]))
for k in range(6):
    _, g = call("emu_pano", FB, BASE, k, 0); n, a = call("emu_pano", FB, BASE, k, 1)
    print("pano  %2d geometry %9d all %9d  %s" % (k, g, a, hashlib.md5(bytes(uc.mem_read(FB, 256000))).hexdigest()[:12]))
