# Native Symbian C++ on the Nokia 9300: GCC 2.9 pitfalls and ARM speed-ups

These notes come from building Maps 9300 (`maps9300/`, versions 0.11–1.19) as a native Series 80 v2
app (Symbian 7.0s, EKA1) with the SDK's GCC 2.9 (`arm-epoc-pe-gcc`, target `armi`, `-O2`). They
apply to any native app or DLL for the 9300/9500. Each item gives what you see, why it happens,
and what to do. The code mentioned is in `maps9300/src` and `maps9300/test`.

The phone: TI OMAP1510, an ARM925T core at 150 MHz (ARMv4T: no `CLZ`, `SMULxy`, `QADD`, ...),
no FPU, 16 KB instruction / 8 KB data cache (16-byte lines), slow RAM. The compiler's `armi` target
is ARMv4, and it is a 1999 compiler: it crashes on some valid code and makes slow code from much more.

## 0. What's in this folder (`symbian-build/native9300/`)

The build tools and what Maps 9300 found, for other native 9300 apps. The code in `lib/` and
`emu/examples/` are snapshots (1.19); the live copies are in `maps9300/`. What is Mapy.com's (its
API and formats, the 3D tiles, places) stays in `maps9300/` (`docs/mapy-api.md`, `docs/3d-view.md`).

| Where | What |
|---|---|
| `../toolchain/setup_eka1_toolchain.sh`, `setup_s80_sdk.sh` | The EKA1 GCC 2.9 toolchain and the Series 80 SDK on a Linux PC |
| `../toolchain/setup_vm_noroot.sh`, `symenv.sh.example` | The same in a shell with no root and ~45 s a command (staged; section 8) |
| `../toolchain/build_s80.sh` | Build any S80 app: `SYM=~/sym build_s80.sh <project> <name>` (group/ with bld.inf, sis/<name>.pkg.in) |
| `emu/armemu.py`, `build_emu.sh`, `drv_template.cpp`, `example.py` | Run phone code (GCC 2.9 armi -O2) in the Unicorn ARM emulator: instruction counts per call and function, hot blocks, the 8 KB data-cache model, faults with their function (section 5) |
| Other emulator tests | `mp4player9300/tools/armemu/` (the video decoders: `run.py decode|profile|colour|h264`), `webbrowser9300/browser9300/test/armtest.sh` (the browser's `igarm.c`), `lib/fastmem/test/` |
| `emu/examples/` | Maps 9300's drivers and scripts: renderer scenes asm vs C (`drv.cpp`, `emu75.py`), whole 3D frames from scene dumps (`drvvts.cpp`, `emuvts.py`), the JPEG decoder (`drvdec.cpp`, `emudec.py`, `emujpg.py`), profiles (`prof*.py`); PC tests (`chunkcmp.cpp`, `panocull.cpp`, `panoseam.cpp`, `jpegtest.cpp`, `mapypoitest.cpp`) |
| `lib/fastmem/` | memcpy / memset / strlen / 16- and 32-bit fills in ARM assembly (32 bytes a step), the C reference, its own README and emulator test (`test/armtest.sh`); the other `lib/` files include `fastmem.h` (add `-I.../lib/fastmem`). Copies: `webbrowser9300/fastmem/` (the browser builds from it) |
| `lib/jpegdec.*`, `jpegasm.*` | Baseline JPEG to RGB565, full or half size; IDCTs, colour, the usual block's Huffman in assembly (section 6b) |
| `lib/pngdec.*` | PNG (zlib) to RGB565 |
| `lib/r3d.*`, `r3dgeo.*`, `r3dspan.*` | The 3D renderer: fixed point, bucket-sorted, affine / perspective textures, meshes with shared corners, chunk culling; the hot loops in assembly (sections 4, 4b, 6) |
| `lib/rotate.*` | 16-bit pixel quarter turns (portrait screens, a turned map) |
| `lib/json.*` | A small JSON reader (no C library) |
| `lib/wait.h` | Blocking waits for worker threads with a nested active scheduler (from Net Helper 9300) |

---

## 1. Compiler crashes ("Internal compiler error") and how to get around them

GCC 2.9 crashes with `Internal compiler error` (often "insn does not satisfy its constraints")
on valid code. The crash is reported at a line inside the function that triggered it, but the
trigger is often a function that was *inlined* into it, or simply the size of the function.
Changing unrelated code in the same file can make an old crash come back (it happened twice in
Maps 9300: 0.78 and 0.91).

| Trigger | Fix |
|---|---|
| A `union` used to reinterpret bytes, e.g. `union { double d; unsigned w[2]; }` | Write and read through a pointer instead (`volatile unsigned* w = (volatile unsigned*) &d;`) and keep it in a small function of its own. See `MakeF64` in `vts3d.cpp`. |
| A small helper inlined many times into one function (`Rd::F64` inlined 7 times) | Define it out of line (in the .cpp, not in the class body). |
| One very large function | Split it into several functions. |

When it crashes, check the generated code afterwards (`arm-epoc-pe-objdump -d` on the `.o` under
`epoc32/build/.../armi/urel/`): the workaround must still do what you meant.

## 2. Things that compile but break on the phone

- **No writable static data.** An EKA1 `.app` or `.dll` can't have global or `static` variables
  that are written (petran refuses it, or it fails at load). Put state in your app or document
  class (`CBase` objects are zero-filled when created), or in `Dll::Tls()`. `static const` tables
  are fine. Remember that `static` locals in functions count too.
- **No `&_L("...")` temporaries kept by pointer.** `_L()` makes a temporary `TPtrC`. Its address
  is garbage after the statement. Use `_LIT` constants, or copy into a `TBuf`.
- **Pointers to locals that outlive the function.** Maps 9300's KERN-EXEC 3 crashes in 0.43–0.58
  were all one bug: the renderer kept a pointer to a texture description that was a local
  variable of the function that submitted the polygon; it was read later, after the stack had
  been reused. Anything a callee keeps must live in a member or on the heap (or be copied, as
  `R3D::Poly` now copies the `R3Tex`).
- **Descriptor overflow panics (USER 23 and friends).** `TBuf8<60>::Format` with a longer result
  panics the app. Size every `TBuf` for the worst case of its `Format` string (numbers: 11 chars
  each, `%S`: the source's maximum length, or `.Left(n)` it first). Before each delivery, check
  every `TBuf<N>` used with `Format`/`Append` against its format string.
- **Small stack (8 KB by default).** A few KB of local arrays plus a deep call chain overflow it
  silently (corruption, then KERN-EXEC 3 somewhere else). Big buffers go on the heap, allocated
  once (Maps 9300 moved a ~5 KB list to the heap in 0.46).
- **`new` doesn't leave, `new (ELeave)` does.** In plain C++ code shared with the PC (`r3d.cpp`,
  `vts3d.cpp`), use plain `new` and check for `NULL`.
- **Heap memory freed isn't free memory.** `HAL` free RAM doesn't go up when you `delete` things.
  The freed space stays in your heap. Count both: the phone's free RAM plus
  `User::Available()`, and call `User::CompressAllHeaps()` when really low. Otherwise an
  "evict until there's free RAM" loop never ends (see `Free3KB` in `mapy.cpp`).
- **No C library.** No `memset`, `memcpy`, `sin`, `sqrt` from libc. Use `Mem::Copy`,
  `Mem::FillZ`, `Math::Sin` (with `TReal`), or the fast versions in `lib/fastmem/`. Keep portable
  modules buildable on the PC with small wrappers (`#ifdef __SYMBIAN32__` → `Math::`, else
  `<math.h>`; see the top of `vts3d.cpp`).
- **`double` has the FPA word order**: the high 32-bit word comes first in memory, unlike a PC.
  Code that builds a `double` from file bytes must detect it (compare the words of `1.0`, as
  `MakeF64` does).
- **No 64-bit conversions between `double` and `long long`** worth using. Keep doubles within
  ±2^31 and convert to `int` (`VFloor` in `vts3d.cpp`).
- **Keys while the virtual cursor is on** (`TEikVirtualCursor`, used to read the joystick as a
  mouse for looking around): **Space alone** never arrives as a key. Each press becomes ONE pointer
  event, the cursor's button down and up in turn (a toggle), so a held Space can't be seen. Use
  Space only for one action a press (read `EButton1Down` and `EButton1Up` alike, in
  `HandleWsEventL`), and other keys (E / C...) for anything held. Chr+Space does come as a key going
  down and up.
- **Shift is sticky on the 9300.** Press and let go of Shift, and the next key still has
  `EModifierShift`. For "while Shift is held", watch the Shift keys' own down / up events
  (`EStdKeyLeftShift` / `EStdKeyRightShift`) in `HandleWsEventL`.
- **Chr is `EModifierFunc`** (`0x2000` with its left/right bits); Chr+key comes with the key's normal
  scan code, so test the modifier, not the code.
- **Card and log writes are slow.** Writing a cache or log file on every event made the 3D view
  crawl. Buffer in RAM and flush every ~10 s, give the user a setting to turn the log file off,
  and prefer the memory card (E:) for caches.

## 2b. Build and packaging gotchas (from GnuBox for S80)

- **`rcomp` and long paths:** "Failed to open ...aif.rsc" / "RCOMP failed" when the toolchain's path
  plus the project's is very long. Keep both short (`~/sym`, `~/myapp`).
- **CRLF:** `rcomp` stops with "Unknown character 0xd" on files with Windows line ends (the SDK's
  examples have them): `sed -i 's/\r$//' file`.
- **`AppendNum(TUint32)` is ambiguous** in GCC 2.9: `AppendNum((TUint) x, EDecimal)`.
- **`TRAP(...)` right before `else`** breaks the parsing: put the TRAP in braces.
- **Renamed a source file:** `abld reallyclean` (stale objects get linked).
- **Check an app:** `petran -v <name>.app` must say `Data Size 0` (no writable static data).
- **Harmless noise:** perl "uninitialized value" warnings from `abld`, `Platform "" not supported`
  from `bldmake`.
- **The `.pkg`'s Series 80 platform line:** `(0x101F8ED2), 0, 0, 0, {"Series80ProductID"}`.
- **Case:** `#include`s, `LIBRARY x.lib` and the mmp / bld.inf names must match the lower-cased SDK.
- **Measured network speeds:** USB RNDIS IP passthrough at most ~107 kB/s; Bluetooth through GnuBox
  ~20-30 kB/s, held back by the 8 KB default TCP receive window per connection (more connections, or a
  bigger window: Maps 9300's TCP window setting).
- **A ready-made toolchain** (no setup run): `prebuilt/` in the private SDK repo `s80_DP_2_0_SDK`
  (Ubuntu 24.04 or newer); there too: the S80 API reference as HTML, the SDK's guides and examples.

## 3. Code GCC 2.9 makes slow, and what to write instead

Measured with the emulator (section 5), in instructions.

| You write | GCC 2.9 makes | Write instead |
|---|---|---|
| A 16-bit load or store (`unsigned short`, `TUint16`) in a hot loop | Two byte loads/stores plus shifts | Keep tables as 32-bit (`unsigned int pal[256]`), write 2 pixels per 32-bit store (`jpegdec.cpp`, `pngdec.cpp`). In assembly, `LDRH`/`STRH` are fine. |
| `x / y`, `x % y` with variable `y` | A call to `__divsi3` (~40–100 instructions): the ARM9 has no divide | A reciprocal table (`iRecip[n] = 2^30 / n` in `r3d.cpp`) and a multiply; shifts for powers of 2; `/ 3` as `* 0xAAAAAAAB >> 33`. |
| `long long a * long long b` | `__muldi3` or long inline code with spills | `(long long) intA * intB` (one `SMULL`), keep one side 32-bit. |
| `T x[3] = { a, b, c };` or `S s = { ... };` | A `memset` call (a byte loop), then the stores | Assign the fields one by one (`VtsDraw`). |
| Loops with many live variables | Every pointer and counter kept on the stack: ~90 instructions per vertex transform | Fewer live variables, or assembly for the hot loop. |
| A byte-by-byte `memcpy`/`memset`/fill | ~5 cycles a byte | `fm_memcpy`, `fm_fill16` (`lib/fastmem/`): 8 registers per `LDM`/`STM`. |
| `double` arithmetic (`a * b`, `(double) i`, `sqrt`) | A call into EUSER's floating-point emulation for every operation (`__muldf3`, `__floatsidf`, ...; tens to hundreds of cycles each) | Integers and fixed point. Maps 9300 1.13: the mesh decoder made ~110,000 such calls per 3D tile, now ~170 (a fixed-point transform built once per submesh, an integer square root for the shading). Find them: `arm-epoc-pe-objdump -dr x.o \| grep df3`. |
| One function with a fast path and a big slow path | The whole function's prologue (all registers saved, all pointers loaded) runs even when the fast path returns at once: ~100 instructions a call | Put the fast path in a small function of its own that calls the big one only when needed (`Block`/`BlockC` in `jpegdec.cpp`). |

## 4. Writing ARM assembly for it

Maps 9300's assembly lives in `.c` files as one top-level `__asm__("...")` block, guarded by
`#ifdef FM_ASM` (set for the phone build in `fastmem.h`), with a C version of every function
beside it. The PC build and the phone's self-test use the C versions; the emulator test checks
that both give the same results bit for bit (`r3dgeo.c`, `r3dspan.c`, `fastmem.c`).

Rules that matter on this target:

- **Calling convention:** arguments in r0–r3, the 5th and later on the stack (`[sp]` at entry,
  `[sp, #36]` after pushing 9 registers). Save r4–r11 and lr if used, return with `bx lr` (Thumb
  callers work). More than 4 arguments? Pass a pointer to a struct (`R3TriPrm`, `R3Rows`) and
  document the offsets next to it.
- **ARMv4 instruction rules** (the assembler enforces some, not all):
  - `MUL Rd, Rm, Rs`: Rd must differ from Rm. `MLA` likewise.
  - `SMULL/UMULL RdLo, RdHi, Rm, Rs`: RdLo, RdHi and Rm must all differ.
  - Immediates are an 8-bit value rotated by an even amount (`#16000` is fine, `#16001` isn't:
    load it from memory or build it with two instructions).
  - `LDRH Rd, [Rn], #2` (post-indexed halfword) is fine.
- **Get the exact C semantics.** A 64-bit product `(long long) n * inv` taken `>> 22` into 32
  bits is `UMULL lo, hi, nlo, inv`, then `MLA hi, nhi, inv, hi`, then subtract `nlo` from `hi`
  if `inv` is negative, then `(lo >> 22) | (hi << 10)` (`r3_texgrad` in `r3dgeo.c`). Signed vs
  unsigned compares (`BHI` vs `BGT`) and `asr` vs `lsr` are where results usually differ.
- **Range checks in one compare:** `(unsigned)(x + 30000) > 60000u` is "outside ±30000"
  (`ADD`, `CMP`, `BHI`).
- **Unroll the pixel loop by 4 and write 2 pixels per 32-bit store**, keep every value in
  registers, and move the per-row bookkeeping into the same function (`r3_rows_tex4`): the call
  and the struct per row cost more than the pixels of a small triangle.

- **Halfwords:** `LDRSH`/`LDRH`/`STRH` with a post-increment (`ldrsh r4, [r0], #2`) read 16-bit data
  at no extra cost in assembly. 16-bit vertex data (from the tile's middle) halved Maps 9300's cache
  misses there (1.13), but C code reading it gets slower (GCC 2.9's byte loads): keep such data for the
  assembly loops.

## 4b. The data cache (8 KB, 16-byte lines)

Instruction counts miss the memory stalls. Maps 9300's emulator models the cache (`emuvts.py -c`):
read misses per function. What helped (1.13, about -35 % misses per frame, the same frames):
- **Smaller per-element data**: the projected corners 8 bytes instead of 16 (a tile's ~700 fit in the
  cache), the mesh's corners 16-bit.
- **Lay structs out by what is read together**: the fields every pass reads (count, flags, pointer,
  next) in the first 16 bytes, arrays starting on a 16-byte boundary (allocate 15 bytes more and round
  the pointer up), so a triangle's 3 values of an array are one line, not two.
- **Share instead of copying**: polygons pointing at one texture descriptor rather than each a copy.
- **Don't touch it**: a box-against-the-view test per tile (8 corners) skipped ~15-25 % of the
  geometry work outright.
- Writes don't fill the cache (read-allocate), so data written in one pass and read in the next misses
  again.

## 5. Measuring: the ARM emulator test

You can't profile on the phone, and the PC's compiler tells you nothing about GCC 2.9's code. So
Maps 9300 runs the phone's own machine code in the Unicorn ARM emulator (`pip install unicorn`):

1. Compile the modules with the phone's compiler and flags:
   `arm-epoc-pe-gcc -O2 -fno-exceptions -fno-rtti -D__MARM__ -D__MARM_ARMI__`.
2. Add a small driver (`test/arm/drv.cpp`, `drvvts.cpp`) with `extern "C"` entry points and the
   few things the missing OS would provide: a bump allocator for `operator new`/`delete`,
   `memset`/`memcpy`, `__pure_virtual`.
3. Link with `arm-epoc-pe-ld -e emu_init --image-base 0x10000 ... libgcc.a`, list the symbols
   with `arm-epoc-pe-nm`.
4. In Python, map the PE's sections into Unicorn, write the test data (a scene, a texture) into
   memory, call the entry points with r0–r3, and count instructions with a basic-block hook,
   per function (by the symbol table) and per block (the hot spots, then `objdump` them).
5. Hash the output (a frame) of the old and new code: they must match to the bit.

Ready-made: `maps9300/test/arm/build.sh` (renderer, panorama, random asm-vs-C tests) and
`buildvts.sh` + `emuvts.py` (the 3D view's city tiles from a scene dumped by `test/vtstest.cpp`
with `DUMP=`). `lib/fastmem/test/armtest.sh` does the same for fastmem. Also check that the
emulator ran **the same code** as the phone build: compare `objdump -d` of the app's objects and
the test's (Maps 9300's `emuhash.py`), since different flags give different code.

More in Maps 9300's `test/arm` (1.13):
- **A data-cache model** (`emuvts.py -c`): a memory-read hook simulating 8 KB, 2-way, 16-byte lines,
  read-allocate; misses per function. This is what found the layout changes in 4b.
- **Count the soft-float calls**: count how often the emulator enters `__muldf3`, `__adddf3`,
  `__floatsidf`, ... (their first block). `emudec.py` does this for the mesh decoder: ~110,000 calls
  a tile showed where its time went, which the instruction counts (libgcc's slow float code)
  and the PC (a real FPU) both hid.
- **Feed it what the OS would**: `drvdec.cpp` compiles the decoder without `__SYMBIAN32__`
  (`-U__SYMBIAN32__`, stub `math.h`/`string.h`) and gets the data already inflated, so zlib
  isn't needed in the emulator.

Instruction counts aren't time: cache misses and the 9300's slow memory add a lot. Use them to
compare versions, then confirm on the phone (Maps 9300 logs per-frame milliseconds).

## 6. What actually made the 3D view faster

In order of gain, measured in the emulator:

1. **Don't do the work at all.** Skip tiles outside the view before touching them; drop faces
   behind or beside the camera before transforming them; no background fill where the scene
   covers the screen; a polygon budget, submitted near to far so the far ones are what's lost.
2. **Transform shared vertices once (0.92).** A mesh's corner is shared by ~4 triangles.
   Transforming and projecting per triangle did it 3 times per triangle; per vertex per tile is
   ~4x less. Triangles then only look up screen positions. Result: 40–55% fewer instructions
   for the whole frame, the same picture to the pixel.
3. **Sort out triangles in assembly** (back face, off-screen, skipped squares) before building
   any polygon: ~45 instructions a triangle instead of ~140.
4. **The span loop and row loop in assembly** (0.75): ~7 instructions a pixel, 4 pixels a step.
5. **Per-polygon setup in assembly** (the texture gradients, 0.92): ~60 instructions instead of
   ~190. With small triangles (~45 pixels each) the setup costs as much as the pixels.
6. **Fixed point everywhere**, angles as table lookups (`R3D::Sin`), reciprocals from a table.

Maps 9300 1.12–1.13 (the same frames to the bit; a test frame 5.41 M → 4.78 M instructions, ~35 %
fewer cache misses):

7. **An exact box test per tile** (`R3D::BoxVisible`): the 8 corners of the tile's box in camera
   space against the near plane and the 4 sides (a few pixels wider). The old 2D wedge kept ~1/3
   more tiles than the screen shows. Cheapest win of all: 9–23 % of a frame.
8. **The data cache** (section 4b): smaller corners, cache-line-aware struct layout, shared texture
   descriptors.
9. **The bucket loop and a polygon's whole drawing in assembly** (`r3_draw`, `r3_raster_tex`):
   C's per-call glue (struct packing, prologues) cost ~70 instructions a polygon.
10. **Remove tests that can't change the result**: the row loop skipped rows with left > right,
    but then ceil(left) > floor(right) anyway, and the later count < 0 test catches it. Prove it
    before removing it (the clamps keep it true), then check it on the frames.

And outside the frame, **the mesh decoder without floating point** (1.13): a fixed-point transform
built once a submesh (rotation and scale together; the 64-bit sums `(long long) int * int`, one
`SMULL` each), an integer square root for the shading, integer division for the texture
coordinates, shifts for power-of-two cells. On the phone it went from ~168 ms to ~37 ms a tile.
The results differ from the doubles only where a value was within ~1/512 of a rounding edge
(2 corners of 60,000 moved 1/16 m), so it was checked by comparing meshes, not by hashes.

Maps 9300 1.17 (the same frames; 6–14 % fewer instructions):

11. **Chunks inside a mesh without reordering it.** The triangles came in no spatial order (any run
    of 64 spanned the whole tile), and reordering them would change the draw order inside the depth
    buckets (not the same frames). Instead each face gets a grid chunk (a byte); the chunks' boxes
    are culled a frame and the face loop skips a hidden chunk's faces with one table lookup. The
    corners can be reordered freely (only the indices change): by their first face's chunk, the
    corners a set of visible chunks needs are a few runs, which r3_verts takes as they are.
12. **Box against plane with the "p-vertex"** (n·c + |n|·h), unit normals in world space worked out
    once a frame: ~30 multiplies a box, no 64-bit maths. Hierarchical (tile, 4 x 4, 8 x 8), each box
    against only the planes its parent straddles: the plain 64-chunk test cost nearly what it saved.
13. **A wide screen culls a lot at its sides**: in a street-level scene 36–68 % of the projected
    triangles were wholly off one side.
14. **Special cases without flag tests**: a clamp-free row loop for triangles wholly inside, chosen
    with three unsigned compares (`cmp; cmpls; cmpls; bhi`: below 0 counts as huge); loop copies
    with GAS `.macro` / `.if` instead of a test each time round. Prove the clamp can go: the rows'
    ends stay within 1/8 px of the corners (floor rounding of 16.16 steps, under 8192 rows).

The panorama (Maps 9300 1.16) shares the renderer, so 3–6 and 8–10 were already its own. Of the
rest only 7 carried over: its sphere's pieces were picked by a cone around the view, which on a
640 x 200 screen is round and ~3x too tall; testing the pieces' corners against the 4 sides
(each corner once, cached a frame) halved the pieces (87 → 41) for the same frame. A rule that
follows: **a round test on a wide screen wastes half its area; test the real sides.** After
that ~80 % of the frame is the 4-pixel span loop, which only packing u and v in one register
(one add instead of two a pixel, ~11 % of the frame, but 16-bit halves: not the same pixels)
would still shorten.

## 6b. JPEG decoding (the 3D photos, the panorama)

- **Decode at the size you need**: a half-size IDCT (4x4 from the lowest frequencies of each 8x8
  block) instead of decoding fully and halving: 45 % of the instructions (1.11).
- **IDCTs and colour conversion in assembly**, with the C kept as the reference (1.12).
- **Huffman decoding of the usual block in assembly** (`jpeg_block`, 1.13): DC, AC, end of block,
  stuffed `FF 00` bytes, all from 9-bit lookup tables; anything else (a longer code, a marker, the
  end of the data) returns "not done" **before changing any state**, and the C decodes that block
  from the start. Fast paths that can bail out to the full C code are simple to get right.
- **Combined tables**: the AC table gives the run, the value bits and the code length in one
  lookup when they fit 9 bits (most do); the zigzag table carries the row bit (`z | 1 << (8 + row)`)
  so tracking non-zero rows is one `ORR`.
- **Move table bases, not indices**: per pixel pair, add the chroma terms to the three
  table pointers once (`r565 + crR`, ...), then each pixel is `ldr [base, y, lsl #2]` x 3:
  30 instructions a pair instead of 34.

## 7. Working method that saved phone installs

- Test everything you can on the PC first (the renderer, decoders and parsers are portable C++
  with PC tests in `maps9300/test/`), then in the emulator, and only then install.
- Keep a C reference for every piece of assembly and compare them on random inputs, including
  edge cases (out of range, behind the camera, empty, huge).
- When the compiler crashes or the phone crashes, write the cause and the fix next to the code
  and in the changelog. Several bugs came back months later in a new form.
- Every optimisation was checked against the old code: frame hashes of 3 scenes in the emulator,
  the PC renderer's 16 views, the JPEG decoder's output hashes for 49 files (plus files with
  restart markers, noise and truncation), and `objdump` of the app's objects against the
  emulator's (the same machine code ran in the test).

## 8. Building in a Cowork VM shell (no root, ~45 s a command)

The shell Claude gets on the user's laptop (Cowork) is a Linux VM without root, where each
command runs ~45 s at most and nothing survives in the background. The whole toolchain still
builds there in stages: `symbian-build/toolchain/setup_vm_noroot.sh debs | clone | gccsrc |
configure | make (repeat until rc=0) | install | tools | s60 | s80`:
- missing packages (flex, bison, 7z, cabextract, unshield) with `apt-get download` + `dpkg -x` into
  `~/loc`, put on `PATH`/`LD_LIBRARY_PATH`; 7z's wrapper script rewritten to its new place;
- texinfo's `ginfo` wants termcap: link it with the system's `libtinfo.so.6` (a symlink named
  `libtinfo.so` in a directory of your own, added to that Makefile's `LIBS`);
- `make` is restartable: run it again until it finishes.
Then the app builds in ~10 s, to the same machine code as elsewhere.

## 9. From the other native apps

Found in MP4 Player 9300 (`mp4player9300/README.md`), Browser 9300 (`webbrowser9300/browser9300/README.md`),
Instagram 9300 (`instagram9300/README.md`), Net Helper 9300 (`nethelper9300/README.md`) and GnuBox
(`gnubox-s80/notes/`); the version that found it in brackets.

**Speed**
- **Build with `-O2`** (`OPTION GCC -O2` in the `.mmp`): the SDK's default is `-O`; MP4 Player's
  decoding went 19 % faster (MP4 1.2/1.4).
- **The emulator's instructions on the phone:** ~14-20 ns each (2.1-2.9 cycles at 150 MHz) for decoding
  that mostly hits the cache (MP4 1.7: 2.04 M instructions = 28.9 ms; 1.11: ~19 ns). Much more than
  that means memory traffic (MP4 1.20).
- **GCC 2.9's `short`s:** no 16-bit loads or stores in its output (each access becomes byte
  operations); use `int` for working arrays (MP4's coefficient blocks and IDCT, 1.7). Constant
  multiplies become shift / add chains; big loops spill registers.
- **Tables that fit the 8 KB data cache:** MP4's colour conversion tables went from ~11 KB to ~3.5 KB,
  two rows per chroma row, two pixels per 32-bit store (1.3); then in assembly with byte tables
  indexed by Y + chroma: 42.3 to 12.7 instructions a pixel (1.7).
- **`memcpy` / `memset` / `strlen` in assembly** (`lib/fastmem/`, from Browser 0.6): 4 KB memcpy 527
  instructions against 20,484 for GCC's loop; a quadratic copy (an array copied on every start tag)
  was 81 % of a page's work: look for those first.
- **The timers:** `User::FastCounter` runs at 64 Hz on the 9300, the same as the tick: average over
  many frames (MP4 1.2).
- **Writing files:** in 32 KB pieces, not per 2 KB network record (Instagram 0.19: a flash write
  each was the slowness).
- **Start fast:** open `RSocketServ` sessions on the first request, not at start (the first may start
  ESOCK); read settings files when first needed (Browser 0.26).

**Drawing**
- **Straight into the screen memory** (direct screen access; MP4 1.14-1.17): find the layout with a
  self-test (draw 16 pixels with the screen's graphics context, look for them in the memory: the
  offset and line length; write 8 and read them back), every run (a saved "passed" skipped it once:
  NULL screen address, KERN-EXEC 3, 1.15). Start the picture at an even x (word-aligned lines, or
  the slow path runs); the window's screen position is window coordinates + the window's position
  (the normal window starts at x = 92). When the window server reports the area covered (a menu),
  draw the old way. Saved ~7-9 ms a frame of the window server's copy.
- **`BitBltMasked` copies the source where the mask is black** unless `aInvertMask` (Instagram 0.43,
  Browser 0.24: icons and AIF masks the other way round showed white boxes).
- **BITGDI 3 = no font selected:** a `DrawText` after another part of the code discarded the font
  (Instagram 0.57). Also from a masked blit asking for more than the bitmap has (16x16 of a 16x15
  picture, 0.41).
- **Draw from a callback, not inside network callbacks:** the main thread has ~8 KB of stack; a page
  drawn deep in the HTTP client's callbacks overflowed it (KERN-EXEC 3, Browser 0.15). Measure frames
  from `sub sp` in the ARM objects; let `Render()` only ask, and draw from an idle callback.
- **Don't give `.ico` files to the phone's ICO decoder:** PNG-in-ICO favicons crash it (KERN-EXEC 3,
  Browser 0.17); decode them yourself.

**Panics worth knowing**
- **USER 23 / USER 11:** an 8-bit / 16-bit descriptor over its maximum length. A `TPtr8` taken before
  its buffer was flushed keeps the old length (Browser 0.25, Instagram 0.60); a `TBuf<80>` status line
  that got 84 characters (Instagram 0.58). Size buffers for the worst escaping (`&` to 5 bytes, UTF-8
  up to 3).
- **E32USER-CBase 40** in the Java comms when Java read Bluetooth while downloading (Net Helper 0.4):
  do Bluetooth in a native thread.
- **Killing a thread in the middle of socket / TLS calls** was followed by the phone restarting (Net
  Helper 0.3): let it close its own connections and finish.

**Memory**
- `EPOCHEAPSIZE` in the `.mmp` (up to 32 MB used); size caches from the free RAM (`HAL`,
  `UserHal::MemoryInfo`) and keep a reserve (2.5-3 MB); compress the heap after dropping things so the
  phone gets the RAM back (Browser 0.12, Instagram 0.31).
- A decoder with its own chunk heap (`User::SwitchHeap` around its calls; MP4 Player, up to 12 MB).
- No writable static data: tables `static const` without pointers; lookup tables built at run time
  in an object (MP4's VLC tables).
- CPU load without the kernel's CPU times: a lowest-priority thread counts how far it gets in 0.1 s
  once a second; less than its best = busy (Browser 0.13).

**Network**
- **Speeds measured:** USB IP passthrough: plain HTTP ~222 KB/s, TLS ~121 KB/s (Instagram 0.25; the
  phone's RNDIS + TCP, not the cable); GnuBox measured ~107 kB/s through it. Bluetooth via GnuBox
  ~20-30 kB/s (the 8 KB default TCP window per connection).
- **Bluetooth:** error -6031 reaching another phone usually means a Bluetooth link to a PC (PC Suite)
  is up and blocks it (Net Helper 0.5).
- **Java on the 9300** can't keep connections open or open sockets to 80/443 unsigned, but may call
  `http://127.0.0.1`: a native helper can do its network work (Net Helper).
- **Installing:** `FR,RI` in the `.pkg` starts the app after installing; the installer's own prompts
  stay (Series 80 has no silent install API; Instagram's auto-update).
- The SDK's zlib (1.1.3) inflates gzip after skipping its header (raw inflate; Instagram 9300).
