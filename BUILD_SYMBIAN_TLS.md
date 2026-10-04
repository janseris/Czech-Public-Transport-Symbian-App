# Building SSLADAPTOR.dll for the Nokia 9300

The DLL is built from [janseris/symbian-tls](https://github.com/janseris/symbian-tls)
(branch `eka1-java-fixes`) and [janseris/bearssl-symbian](https://github.com/janseris/bearssl-symbian)
(branch `eka1-fixes`). The full step-by-step guide is in the fork:
`symbian-build/symbian-tls/BUILD_LINUX_EKA1.md`. This page is the short version.

It builds on **Linux** (WSL on Windows works) with GnuPoc. The original Windows SDK
toolchain (Metrowerks/ActivePerl 5.6) isn't needed.

## Inputs

| File | Where |
|---|---|
| `gcc-539-2aeh-source.tar.bz2` (Symbian GCC 2.9-psion-98r2) | in this repo, `symbian-build/` |
| `S60_SDK_2_1_NET.zip` (S60 2nd Edition FP1 SDK, Symbian 7.0s, 116 MB) | not in git: over GitHub's 100 MB file limit, and it is Nokia's SDK. Keep a copy in `symbian-build/` on your PC, or get it from the [Symbian Archive](https://mrrosset.github.io/Symbian-Archive/SDKs-Series.html) |
| GnuPoc scripts | `git clone https://github.com/mstorsjo/gnupoc-package` |
| packages | `build-essential flex bison libncurses-dev zlib1g-dev cabextract perl` |

## Setup in one step

`symbian-build/toolchain/setup_eka1_toolchain.sh` does all of the steps below and applies our fixes:

- `gnupoc-bmconv.patch`
- `sdk-perl-fixes.patch`
- the `collect2.c` fix for GCC

GnuPoc is pinned to commit `d3ddaf5`. Tested on 2026-10-04 on Ubuntu 24.04, starting from nothing. With it, the tag `v20-fix10` builds a DLL of the same size as `symbian-build/out/ssladaptor_v20-fix10.dll`; only 5 bytes differ (timestamp and checksum).

```sh
# both archives in one folder: gcc-539-2aeh-source.tar.bz2 (this repo) + S60_SDK_2_1_NET.zip
symbian-build/toolchain/setup_eka1_toolchain.sh symbian-build ~/sym
export EPOCROOT=~/sym/s60_21/ PATH=~/sym/wrap:$PATH
cd symbian-build/bearssl/group && bldmake bldfiles && abld build armi urel
cd ../../symbian-tls/group && bldmake bldfiles && abld build armi urel ssladaptor
```

On Windows, run it in WSL. Building GCC takes ~2 minutes.

## Steps (what the script does)

1. **Compiler:** `gnupoc-package/tools/install_gcc_539`. A modern host GCC needs
   `CC="gcc -std=gnu89 -fcommon -w -fpermissive"`. Also replace `sys_siglist[...]` with
   `strsignal(...)` in `collect2.c`, because it was removed from glibc.
2. **EKA1 tools:** `install_eka1_tools`. In bmconv, change `(foundPath > 0)` to `(foundPath != 0)`. Its last step, GNU make 3.81 as `extmake`, fails with a modern glibc; it isn't needed.
3. **SDK:** `install_gnupoc_s60_21` and `install_wrapper`. Then replace `defined(%hash)` /
   `defined(@array)` in the SDK's perl scripts, because modern perl rejects them.
4. **Build:**
   ```sh
   export EPOCROOT=~/sym/s60_21/ PATH=~/sym/wrap:$PATH
   cd symbian-build/bearssl/group && bldmake bldfiles && abld build armi urel
   cd ../../symbian-tls/group && bldmake bldfiles && abld build armi urel ssladaptor
   ```
   The output is `$EPOCROOT/epoc32/release/armi/urel/ssladaptor.dll`.
   For the log build, add `MACRO SSL_LOG` to `group/ssladaptor.mmp` and rebuild with
   `abld reallyclean` first.

EKA1 DLLs must not have writable static data: petran stops with "Dll has initialised data".
Keep tables `const` and avoid `static` arrays of pointers.

Copy finished DLLs to `symbian-build/out/` as `ssladaptor_v20-fixN.dll` / `ssladaptor_v20-fixN_log.dll` (our Nth fix build on shinovon's v20)
and commit them here. Commit the sources to the forks.
