#!/usr/bin/env python3
# ./build_emu.sh out drv_template.cpp && python3 example.py out/emu.exe out/emu.sym
import sys, random
from armemu import Emu
e = Emu(sys.argv[1], sys.argv[2], cache=True)
e.call("emu_init")
data = bytes(random.getrandbits(8) for _ in range(4096))
e.write(0x2000000, data)
r, n = e.call("emu_sum", 0x2000000, len(data))
print("sum %d (Python %d), %d instructions" % (r, sum(data), n))
e.profile()
print("reads, misses, by function:", e.misses())
