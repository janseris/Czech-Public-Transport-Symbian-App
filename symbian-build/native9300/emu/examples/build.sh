#!/bin/sh
# The 3D renderer built as on the phone (GCC 2.9 armi -O2) and run in the Unicorn ARM emulator (emu3d.py):
# instruction counts of the benchmark's scenes and any memory fault, before installing anything.
# Needs the S80 toolchain on PATH (arm-epoc-pe-gcc) and `pip install unicorn`.
set -e
cd "$(dirname "$0")"
INC=-I../../inc
F="-O2 -fno-exceptions -fno-rtti -D__MARM__ -D__MARM_ARMI__"
arm-epoc-pe-gcc $F $INC -c ../../src/r3d.cpp -o r3d.o
arm-epoc-pe-gcc $F $INC -c drv.cpp -o drv.o
arm-epoc-pe-gcc $F $INC -c ../../src/panosph.cpp -o panosph.o
for c in fastmem fastmem_ref r3dspan r3dgeo; do arm-epoc-pe-gcc -O2 -D__MARM__ -D__MARM_ARMI__ $INC -c ../../src/$c.c -o $c.o; done
LG=$(dirname $(arm-epoc-pe-gcc -print-libgcc-file-name))/libgcc.a
arm-epoc-pe-ld -e emu_init --image-base 0x10000 -o emu.exe drv.o r3d.o panosph.o fastmem.o fastmem_ref.o r3dspan.o r3dgeo.o $LG
arm-epoc-pe-nm emu.exe > emu.sym
python3 ${EMU:-emu3d.py} emu.exe emu.sym "$@"
