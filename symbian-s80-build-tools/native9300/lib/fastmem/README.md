# fastmem: fast memory helpers in ARM assembly for Symbian EKA1 phones

The functions are hand-written ARMv4 assembly for the Nokia 9300 (S80 2.0, Symbian 7.0s, 150 MHz
ARM9, GCC 2.9 `armi` builds). Every other build (the PC, the emulator, THUMB) gets the same
functions in plain C. The library was first written for Browser 9300 0.6.

| Function | What it does |
|---|---|
| `fm_memcpy(dst, src, n)` | copies forward; an overlap is fine only when `dst < src` |
| `fm_memset(dst, c, n)` | sets `n` bytes to `c & 255` |
| `fm_strlen(s)` | the length of a string; 0 for NULL |
| `fm_fill16(dst, value, count)` | fills `count` 16-bit values; `dst` 2-byte aligned (a 16 bpp screen, a z-buffer) |
| `fm_fill32(dst, value, count)` | fills `count` 32-bit values; `dst` 4-byte aligned |

All of it is C89 with no libc and no writable static data, so it works in an EKA1 `.app`, which is
a DLL.

## Files

- `fastmem.h`: the declarations. It defines `FM_ASM` when `__MARM__` is set and `__MARM_THUMB__`
  isn't (that is, armi/ARM4 builds). `-DFM_NO_ASM` turns the assembly off.
- `fastmem.c`: the assembly, as a top-level `__asm__` block that GCC 2.9 accepts. It compiles to
  nothing without `FM_ASM`.
- `fastmem_ref.c`: the C versions. It compiles to nothing with `FM_ASM`.
- `test/armtest.sh`, `test/armtest.py`: the emulator test, described below.

## Using it

**In a new project:** copy `fastmem.h`, `fastmem.c` and `fastmem_ref.c` next to your sources and
`#include "fastmem.h"`. In the `.mmp`:

```
SOURCE fastmem.c fastmem_ref.c
```

Add both files to the PC test build too. Then check that `petran -v` on the `.app` still shows
`Data Size 0`.

**In a project built on the Instagram 9300 core** (`igcore.h` / `igutil.c`, used by Instagram 9300
and Browser 9300): the core has its own byte-loop `ig_memcpy`, `ig_memset` and `ig_strlen`.
To send those names to fastmem:

1. Add the three fastmem files as above.
2. Delete the C `ig_memcpy`, `ig_memset` and `ig_strlen` from `igutil.c`.
3. In `igcore.h`, before its declarations, add:
   ```c
   #include "fastmem.h"
   #define ig_memcpy fm_memcpy
   #define ig_memset fm_memset
   #define ig_strlen fm_strlen
   ```

Every existing call then goes to fastmem, with no other change. To check the phone build, run
`arm-epoc-pe-nm fastmem.o`: it should list `T fm_memcpy`.

## How fast

These are instruction counts in the emulator, against GCC 2.9 `-O2`'s build of the C loops in
`fastmem_ref.c`:

| | asm | C |
|---|---|---|
| memcpy 4 KB, aligned | 527 | 20,484 |
| memcpy 4 KB, source 1 byte off | 6,164 | 20,484 |
| memset 4 KB | 536 | 16,388 |
| strlen, 200 bytes | 269 | 809 |
| fill16, a 640 x 200 screen (128,000 values) | 24,022 | 512,004 |
| fill32, 64,000 values | 24,013 | 256,004 |

They're instructions, not cycles. An LDM/STM of 8 registers takes about 9 cycles on the ARM9,
and memory speed caps big fills and copies. Expect roughly 5-10x in real time for large aligned
work and little for short strings. Measure on the phone (`User::TickCount` or
`User::FastCounter`) before you rely on a number.

## How it works

- **memcpy:** aligns the destination with up to 3 single bytes. Then it moves 32 bytes per
  `LDMIA`/`STMIA` pair of 8 registers, then single words, then single bytes. A source that is
  still unaligned after that is read as aligned words and merged with shifts (little-endian),
  4 bytes per step. That path may read up to 3 bytes past the end of the source, but never past
  the aligned word that holds the last byte, so it can't cross a page.
- **memset:** aligns, then 32 bytes per `STMIA`, then words, then bytes.
- **strlen:** aligns, then checks a word at a time: `(w - 0x01010101) & ~w & 0x80808080` is
  nonzero exactly when the word has a zero byte.
- **fill16 / fill32:** handle one halfword to align, then 16 halfwords (8 words) per `STMIA`,
  then the tail.

All of them follow the ARM calling convention: r4-r8 and lr are saved when used, the stack comes
back as it was, and they return with `bx lr`, so THUMB callers work too.

## Testing

Run `test/armtest.sh`. It needs the S80 toolchain on `PATH` (`arm-epoc-pe-gcc`; it sources
`~/sym/env.sh` if there is one) and `pip install unicorn`.

The script assembles `fastmem.c` with the phone's own compiler and runs the resulting code in the
Unicorn ARM emulator against Python references:

- 4,000 random sizes and alignments of memcpy / memset / strlen, with guard bytes before and
  after every buffer;
- forward overlaps;
- 3,000 random fill16 / fill32 runs;
- `strlen(NULL)`;
- callee-saved registers and the stack pointer checked after every call.

It then prints the instruction counts above. Run it after any change to the assembly.

## For a 3D game (or any game) on the 9300

- **What this helps:** the per-frame work.
  - Clearing the back buffer with `fm_fill16`: a 640 x 200 16 bpp buffer is 256 KB a frame. That
    is about 0.5 M instructions with a C loop, or about 24 k with the assembly.
  - Clearing a z-buffer: `fm_fill16` with `0xFFFF` (or `fm_memset` with `0xFF`).
  - Copying the back buffer to the screen bitmap: `fm_memcpy`.
  - Moving vertex and sprite arrays: `fm_memcpy`.
- **What this doesn't help:** where a 3D renderer spends most of its time, which is
  - transforming vertices;
  - the rasterizer's inner span loop (flat or Gouraud fill, texture mapping, z test).

  Those need their own assembly. The ARM9 has no FPU, so use 16.16 fixed point and `SMULL`.
  ARMv5TE's `SMULxy` / `SMLAxy` 16x16 multiplies are worth using only if you build for that
  architecture; the 9300's ARM926 has them, but the GCC 2.9 `armi` target assumes ARMv4. Keep
  the span loop's values in registers and write 2 or 4 pixels per store. These are the next
  candidates to hand-write.
