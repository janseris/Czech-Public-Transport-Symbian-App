import sys, struct, bisect, collections
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_BLOCK
from unicorn.arm_const import *
exe, symf, kind, mode = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
data = open(exe, "rb").read()
pe = struct.unpack_from("<I", data, 0x3C)[0]
nsec = struct.unpack_from("<H", data, pe + 6)[0]; optsz = struct.unpack_from("<H", data, pe + 20)[0]
base = struct.unpack_from("<I", data, pe + 24 + 28)[0]; sec = pe + 24 + optsz
uc = Uc(UC_ARCH_ARM, UC_MODE_ARM); uc.mem_map(0x1000, 0x4000000 - 0x1000)
for i in range(nsec):
    name, vsize, va, rawsz, rawptr = struct.unpack_from("<8sIIII", data, sec + i * 40)
    if rawsz and not name.startswith(b".stab"): uc.mem_write(base + va, data[rawptr:rawptr + rawsz])
syms = {}; allsyms = {}
for line in open(symf):
    p = line.split()
    if len(p) == 3:
        allsyms[p[2]] = int(p[0], 16)
        if p[1] in "Tt" and not p[2].startswith(".L"): syms[int(p[0], 16)] = p[2]
addrs = sorted(syms)
blocks = collections.Counter()
uc.hook_add(UC_HOOK_BLOCK, lambda uc, a, s, d: blocks.__setitem__((a, s), blocks[(a, s)] + 1))
def call(fn, *args):
    for r, v in zip([UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3], args): uc.reg_write(r, v)
    uc.reg_write(UC_ARM_REG_SP, 0x300000); uc.reg_write(UC_ARM_REG_LR, 0x2000)
    uc.emu_start(allsyms[fn], 0x2000)
call("emu_init", 0x2000000); call("emu_mode", mode)
call("emu_pano", 0x2100000, 0x2400000, kind, 1)
blocks.clear()
call("emu_pano", 0x2100000, 0x2400000, kind, 1)
per = collections.Counter()
for (a, size), n in blocks.items():
    per[syms[addrs[bisect.bisect_right(addrs, a) - 1]]] += n * size // 4
tot = sum(per.values()); print("total", tot)
for f, n in per.most_common(10): print("%10d %5.1f%%  %s" % (n, 100.0 * n / tot, f))
hot = collections.Counter()
for (a, size), n in blocks.items(): hot[(a, size)] = n * size // 4
for (a, size), n in hot.most_common(10):
    f = addrs[bisect.bisect_right(addrs, a) - 1]
    print("  0x%x (%s+0x%x) %d instr x %d = %d" % (a, syms[f], a - f, size // 4, blocks[(a, size)], n))
