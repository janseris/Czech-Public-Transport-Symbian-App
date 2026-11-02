#!/bin/sh
# 0.92: the 3D view's tiles drawn as on the phone (GCC 2.9 armi -O2) in the Unicorn ARM emulator (emuvts.py).
# Scenes: test/vtstest.cpp with DUMP=<file>. Needs arm-epoc-pe-gcc on PATH and unicorn.
set -e
cd "$(dirname "$0")"
INC=-I../../inc
F="-O2 -fno-exceptions -fno-rtti -D__MARM__ -D__MARM_ARMI__"
arm-epoc-pe-gcc $F $INC -c ../../src/r3d.cpp -o r3d.o
arm-epoc-pe-gcc $F $INC -c ../../src/vtsdraw.cpp -o vtsdraw.o
arm-epoc-pe-gcc $F $INC -c drvvts.cpp -o drvvts.o
arm-epoc-pe-gcc $F $INC -c ../../src/jpegdec.cpp -o jpegdec.o
arm-epoc-pe-gcc -O2 -D__MARM__ -D__MARM_ARMI__ $INC -c ../../src/jpegasm.c -o jpegasm.o
for c in fastmem fastmem_ref r3dspan r3dgeo; do arm-epoc-pe-gcc -O2 $XF -D__MARM__ -D__MARM_ARMI__ $INC -c ../../src/$c.c -o $c.o; done
LG=$(dirname $(arm-epoc-pe-gcc -print-libgcc-file-name))/libgcc.a
arm-epoc-pe-ld -e emu_init --image-base 0x10000 -o emuvts.exe drvvts.o jpegdec.o jpegasm.o vtsdraw.o r3d.o fastmem.o fastmem_ref.o r3dspan.o r3dgeo.o $LG
arm-epoc-pe-nm emuvts.exe > emuvts.sym
python3 emuvts.py emuvts.exe emuvts.sym "$@"
