#!/bin/sh
# Assembles fastmem.c with the S80 GCC 2.9 (arm-epoc-pe-gcc on PATH, e.g. `. ~/sym/env.sh`), runs it in
# the Unicorn ARM emulator against Python references, and counts instructions against GCC's build of
# fastmem_ref.c. Needs `pip install unicorn`.
set -e
cd "$(dirname "$0")"
[ -f ~/sym/env.sh ] && . ~/sym/env.sh
T=${TMPDIR:-/tmp}/fastmemtest
mkdir -p $T
arm-epoc-pe-gcc -O2 -D__MARM__ -D__MARM_ARMI__ -c -o $T/asm.o ../fastmem.c
arm-epoc-pe-gcc -O2 -DFM_NO_ASM -c -o $T/ref.o ../fastmem_ref.c
for o in asm ref; do arm-epoc-pe-objdump -d $T/$o.o > $T/$o.dis; arm-epoc-pe-nm $T/$o.o | grep " T " > $T/$o.sym; done
python3 armtest.py $T/asm.dis $T/asm.sym $T/ref.dis $T/ref.sym
