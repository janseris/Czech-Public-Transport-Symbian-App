#!/bin/sh
# 1.13: VtsDecode (vts3d.cpp) as on the phone (GCC 2.9 armi -O2) in the Unicorn ARM emulator (emudec.py).
set -e
cd "$(dirname "$0")"
# zlib.h from the SDK (SYM: the SDKs' folder, as ../../build.sh)
ZINC=$(dirname $(find ${SYM:-/home/claude/sym} -path "*s80*epoc32/include/zlib.h" | head -1))
INC="-Istub -I../../inc -I$ZINC"
F="-O2 -fno-exceptions -fno-rtti -D__MARM__ -D__MARM_ARMI__ -U__SYMBIAN32__"
arm-epoc-pe-gcc $F $INC -c ../../src/vts3d.cpp -o vts3d.o
arm-epoc-pe-gcc $F $INC -c drvdec.cpp -o drvdec.o
LG=$(dirname $(arm-epoc-pe-gcc -print-libgcc-file-name))/libgcc.a
arm-epoc-pe-ld -e emu_init --image-base 0x10000 -o emudec.exe drvdec.o vts3d.o $LG
arm-epoc-pe-nm emudec.exe > emudec.sym
