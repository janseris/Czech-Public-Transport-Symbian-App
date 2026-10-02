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
| `S60_SDK_2_1_NET.zip` (S60 2nd Edition FP1 SDK, Symbian 7.0s) | not in git (over GitHub's 100 MB limit); download from the [Symbian Archive](https://mrrosset.github.io/Symbian-Archive/SDKs-Series.html) into `symbian-build/` |
| GnuPoc scripts | `git clone https://github.com/mstorsjo/gnupoc-package` |
| packages | `build-essential flex bison libncurses-dev zlib1g-dev cabextract perl` |

## Steps

1. **Compiler:** `gnupoc-package/tools/install_gcc_539`. A modern host GCC needs
   `CC="gcc -std=gnu89 -fcommon -w -fpermissive"`. Also replace `sys_siglist[...]` with
   `strsignal(...)` in `collect2.c`, because it was removed from glibc.
2. **EKA1 tools:** `install_eka1_tools`. In bmconv, change `(foundPath > 0)` to `(foundPath != 0)`.
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

Copy finished DLLs to `symbian-build/out/` as `ssladaptor_sni_vN.dll` / `ssladaptor_log_vN.dll`
and commit them here. Commit the sources to the forks.
