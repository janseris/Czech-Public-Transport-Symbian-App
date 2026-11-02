// A driver for the ARM emulator (armemu.py): what the code under test needs from the system, nothing
// of Symbian's: a bump heap for new (from 0x400000, never freed), memset / memcpy, and extern "C"
// entry points the Python side calls by name (up to 4 int arguments, r0 back). emu_init is the
// image's entry (build_emu.sh links with -e emu_init). See emu/examples/ for real ones (drvvts.cpp:
// whole 3D frames from a scene dump; drvdec.cpp: a decoder; drv.cpp: renderer scenes, asm vs C).

static char* gHeap = (char*) 0x400000;
void* operator new(unsigned int n) { void* p = gHeap; gHeap += (n + 7) & ~7; return p; }
void* operator new[](unsigned int n) { return operator new(n); }
void operator delete(void*) {}
void operator delete[](void*) {}
extern "C" void __pure_virtual() {}
extern "C" void* memset(void* d, int c, unsigned int n) { char* p = (char*) d; while (n--) *p++ = (char) c; return d; }
extern "C" void* memcpy(void* d, const void* s, unsigned int n) { char* p = (char*) d; const char* q = (const char*) s; while (n--) *p++ = *q++; return d; }

extern "C" int emu_init() { return 0; }

// an example: sums aN bytes at aData (the Python side writes them at 0x2000000 first)
extern "C" int emu_sum(const unsigned char* aData, int aN)
    {
    int s = 0;
    for (int i = 0; i < aN; i++) s += aData[i];
    return s;
    }
