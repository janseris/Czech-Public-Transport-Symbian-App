#!/bin/bash
# The EKA1 toolchain + S80 SDK in a Cowork VM shell without root, in stages that each fit a
# ~45 s command (background processes don't survive between commands there). Run the stages in
# order:  setup_vm_noroot.sh debs | clone | gccsrc | configure | make (repeat until "rc=0") |
#         install | tools | s60 | s80
# Then:  . ~/symenv.sh; cd maps9300 && SYM=~/sym ./build.sh   (~10 s for the whole app)
set -e
R=$(cd "$(dirname "$0")/../.." && pwd)          # phoneapk
H=$R/symbian-build/toolchain
case "$1" in
debs)
  mkdir -p ~/debs ~/loc ~/loc/lnk && cd ~/debs
  apt-get download flex bison p7zip-full p7zip cabextract unshield libunshield0 libmspack0 m4 libfl2
  for d in *.deb; do dpkg -x $d ~/loc; done
  for f in 7z 7za 7zr; do printf '#!/bin/sh\nexec %s/usr/lib/p7zip/%s "$@"\n' ~/loc $f > ~/loc/usr/bin/$f; chmod +x ~/loc/usr/bin/$f; done
  ln -sf /lib/x86_64-linux-gnu/libtinfo.so.6 ~/loc/lnk/libtinfo.so
  cat > ~/symenv.sh <<E2
export PATH=$HOME/loc/usr/bin:\$PATH
export LD_LIBRARY_PATH=$HOME/loc/usr/lib/x86_64-linux-gnu:\$LD_LIBRARY_PATH
export BISON_PKGDATADIR=$HOME/loc/usr/share/bison
export M4=$HOME/loc/usr/bin/m4
E2
  mkdir -p ~/symsrc && ln -sf $R/symbian-build/gcc-539-2aeh-source.tar.bz2 ~/symsrc/ && ln -sfn $R/s60-sdk-2.1 ~/symsrc/s60-sdk-2.1
  . ~/symenv.sh; 7z x -y -o$HOME/symsrc ~/symsrc/s60-sdk-2.1/S60_SDK_2_1_NET_2.7z.001 > /dev/null ;;
clone)
  mkdir -p ~/sym && rm -rf ~/sym/gnupoc && git clone -q https://github.com/mstorsjo/gnupoc-package ~/sym/gnupoc
  cd ~/sym/gnupoc && git checkout -q d3ddaf58c734ad3ab34dc60a012f9d4768a9ed3e && git apply $H/gnupoc-bmconv.patch ;;
gccsrc)
  cd ~/sym/gnupoc/tools && rm -rf src obj && tar -jxf ~/symsrc/gcc-539-2aeh-source.tar.bz2 && patch -s -p0 < gcc-539.patch
  sed -i 's/sys_siglist\[\([^]]*\)\]/strsignal(\1)/g' src/gcc/collect2.c ;;
configure)
  . ~/symenv.sh; mkdir -p ~/sym/gnupoc/tools/obj && cd ~/sym/gnupoc/tools/obj
  CC="gcc -std=gnu89 -fcommon -w -fpermissive" CFLAGS="-g" sh ../src/configure --prefix="$HOME/sym/gcc539" --target=arm-epoc-pe > /dev/null
  sed -i 's/-O2//' Makefile ;;
make)
  . ~/symenv.sh; cd ~/sym/gnupoc/tools/obj
  [ -f texinfo/info/Makefile ] && sed -i "s|^LIBS = -lz$|LIBS = -lz -L$HOME/loc/lnk -ltinfo|" texinfo/info/Makefile
  timeout 40 make -s all-binutils all-gas all-ld all-gcc CC="gcc -std=gnu89 -fcommon -w -fpermissive" > ~/mk.log 2>&1; echo rc=$? ;;
install)
  . ~/symenv.sh; cd ~/sym/gnupoc/tools/obj && make -s install-binutils install-gas install-ld install-gcc > /dev/null
  cd .. && rm -rf obj src && cp arm-specs ~/sym/gcc539/lib/gcc-lib/arm-epoc-pe/2.9-psion-98r2/specs
  cd ~/sym/gcc539/bin && { [ -e arm-epoc-pe-cpp ] || ln -s ../lib/gcc-lib/arm-epoc-pe/2.9-psion-98r2/cpp arm-epoc-pe-cpp; [ -e as ] || ln -s arm-epoc-pe-as as; [ -e cpp ] || ln -s arm-epoc-pe-cpp cpp; } ;;
tools)
  . ~/symenv.sh; cd ~/sym/gnupoc/tools && ./install_eka1_tools ~/sym/gcc539 > /dev/null 2>&1 || true
  for t in petran rcomp bmconv makesis; do [ -x ~/sym/gcc539/bin/$t ] || { echo "missing $t"; exit 1; }; done ;;
s60)
  . ~/symenv.sh; cd ~/sym/gnupoc/sdks && rm -rf ~/sym/s60_21 && ./install_gnupoc_s60_21 ~/symsrc/S60_SDK_2_1_NET.zip ~/sym/s60_21 > /dev/null
  (cd ~/sym/s60_21/epoc32/tools && patch -s -p1 < $H/sdk-perl-fixes.patch)
  rm -rf ~/sym/wrap && ./install_wrapper ~/sym/wrap > /dev/null && sed -i "s|^EKA1TOOLS=.*|EKA1TOOLS=$HOME/sym/gcc539/bin|" ~/sym/wrap/gnupoc-common.sh ;;
s80)
  . ~/symenv.sh
  # the zip in the main repo's folder, else the split 7z of the private repo s80_DP_2_0_SDK
  Z=$R/S80_DP_2_0_SDK.zip; [ -f "$Z" ] || Z=$R/s80_DP_2_0_SDK/S80_DP_2_0_SDK.7z.001
  bash $H/setup_s80_sdk.sh "$Z" ~/sym ;;
*) echo "stages: debs clone gccsrc configure make install tools s60 s80"; exit 1 ;;
esac
echo "stage $1 done"
