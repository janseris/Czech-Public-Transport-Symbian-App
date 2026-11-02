"""1.16: the panorama's frame (emu_pano) with the data-cache model of emuvts.py -c (8 KB, 2-way,
16-byte lines): instructions and read misses per function. profpanoc.py emu.exe emu.sym kind"""
import sys, struct, bisect, collections
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_BLOCK, UC_HOOK_MEM_READ
from unicorn.arm_const import *
exe, symf, kind = sys.argv[1], sys.argv[2], int(sys.argv[3])
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
def fnat(a): return syms[addrs[bisect.bisect_right(addrs, a) - 1]]
blocks = collections.Counter()
uc.hook_add(UC_HOOK_BLOCK, lambda uc, a, s, d: blocks.__setitem__((a, s), blocks[(a, s)] + 1))
LINE, WAYS, SETS = 16, 2, 8192 // 16 // 2
tags = [[-1] * WAYS for _ in range(SETS)]
miss = collections.Counter(); on = [False]
def memhook(uc, access, addr, size, value, d):
    if not on[0]: return
    ln = addr // LINE; st = tags[ln % SETS]
    if st[0] == ln: return
    if st[1] == ln: st[0], st[1] = st[1], st[0]; return
    st[1] = st[0]; st[0] = ln
    miss[fnat(uc.reg_read(UC_ARM_REG_PC))] += 1
uc.hook_add(UC_HOOK_MEM_READ, memhook)
def call(fn, *args):
    for r, v in zip([UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_R3], args): uc.reg_write(r, v)
    uc.reg_write(UC_ARM_REG_SP, 0x300000); uc.reg_write(UC_ARM_REG_LR, 0x2000)
    uc.emu_start(allsyms[fn], 0x2000)
    return uc.reg_read(UC_ARM_REG_R0)
call("emu_init", 0x2000000); call("emu_mode", 1)
import random
rnd = random.Random(5); uc.mem_write(0x2400000, bytes(rnd.getrandbits(8) for _ in range(1024 * 512 * 2)))
call("emu_pano", 0x2100000, 0x2400000, kind, 1)
blocks.clear(); on[0] = True
px = call("emu_pano", 0x2100000, 0x2400000, kind, 1)
per = collections.Counter()
for (a, size), n in blocks.items(): per[fnat(a)] += n * size // 4
tot = sum(per.values()); tm = sum(miss.values())
import hashlib
fh = hashlib.md5(bytes(uc.mem_read(0x2100000, 640 * 200 * 2))).hexdigest()[:8]
print("kind %d: %d instructions, %d read misses, %d pixels, frame %s" % (kind, tot, tm, px, fh))
for f, n in per.most_common(6): print("%10d %5.1f%%  misses %6d  %s" % (n, 100.0 * n / tot, miss[f], f))
if len(sys.argv) > 4:
    hot = collections.Counter()
    for (a, size), n in blocks.items():
        if fnat(a) == sys.argv[4]: hot[(a, size, n)] = n * size // 4
    for (a, size, n), t in hot.most_common(8): print("  0x%x %d instr x %d = %d" % (a, size // 4, n, t))
