#!/bin/sh
# Builds a Series 80 (Nokia 9300) app: its group/ (bld.inf, .mmp), then the .sis from sis/<name>.pkg.in
# (EPOC32 in it replaced by the SDK's epoc32 path). From Maps 9300's build.sh.
#   SYM=~/sym ./build_s80.sh <project dir> <name>          ->  <project>/bin/<name>.sis
# Needs the EKA1 toolchain and the S80 SDK: setup_eka1_toolchain.sh and setup_s80_sdk.sh here (or,
# in a shell with no root and ~45 s a command, setup_vm_noroot.sh; then `. ~/symenv.sh` first).
set -e
SYM=${SYM:-$HOME/sym}
P=$(cd "$1" && pwd); N=$2
export EPOCROOT=$SYM/s80_20/ PATH=$SYM/wrap:$PATH
LOG=/tmp/${N}_build.log
cd "$P/group"
bldmake bldfiles
abld build armi urel 2>&1 | tee "$LOG"
if grep -q "Error [0-9]" "$LOG"; then echo "BUILD FAILED (see $LOG)"; exit 1; fi
mkdir -p "$P/bin"
sed "s|EPOC32|$SYM/s80_20/epoc32|g" "$P/sis/$N.pkg.in" > "$P/bin/$N.pkg"
cd "$P/bin" && makesis "$N.pkg" "$N.sis" && rm "$N.pkg"
ls -l "$P/bin/$N.sis"
