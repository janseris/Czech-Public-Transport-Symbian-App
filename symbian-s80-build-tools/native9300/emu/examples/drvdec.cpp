// Maps 9300 (1.13): the 3D view's mesh decoding (vts3d.cpp: VtsDecode) in the Unicorn ARM emulator, built
// with the phone's GCC 2.9: the instructions by function (emudec.py). The data is given inflated (the
// phone's zlib isn't here); the doubles are libgcc's (the phone: EUSER's, faster), counted separately.
#include "vts3d.h"
#include <math.h>
#include <string.h>
#include <zlib.h>

static char* gHeap = (char*) 0x400000;
void* operator new(unsigned int n) { void* p = gHeap; gHeap += (n + 7) & ~7; return p; }
void* operator new[](unsigned int n) { return operator new(n); }
void operator delete(void*) {}
void operator delete[](void*) {}
extern "C" void __pure_virtual() {}
extern "C" void* memset(void* d, int c, unsigned int n) { char* p = (char*) d; while (n--) *p++ = (char) c; return d; }
extern "C" void* memcpy(void* d, const void* s, unsigned int n) { char* p = (char*) d; const char* q = (const char*) s; while (n--) *p++ = *q++; return d; }
extern "C" int inflateInit2_(z_streamp, int, const char*, int) { return Z_STREAM_ERROR; }
extern "C" int inflate(z_streamp, int) { return Z_STREAM_ERROR; }
extern "C" int inflateEnd(z_streamp) { return Z_OK; }
// series good enough for the frame (once) and Newton for the faces' lengths
extern "C" double sin(double x) { double t = x, s = x; for (int i = 1; i < 12; i++) { t *= -x * x / ((2 * i) * (2 * i + 1)); s += t; } return s; }
extern "C" double cos(double x) { double t = 1, s = 1; for (int i = 1; i < 12; i++) { t *= -x * x / ((2 * i - 1) * (2 * i)); s += t; } return s; }
extern "C" double atan(double x) { return x; }
extern "C" double exp(double x) { return 1 + x; }
extern "C" double sqrt(double x)
    {
    if (x <= 0) return 0;
    double r = x > 1 ? x / 2 : 1;
    for (int i = 0; i < 30; i++) { double n = (r + x / r) * 0.5; if (n == r) break; r = n; }
    return r;
    }

static VtsFrame gF;
static VtsMesh* gM;
extern "C" int emu_init() { gM = new VtsMesh; return 0; }
extern "C" int emu_frame(int aLatE6, int aLonE6) { gF.Set(aLatE6 / 1e6, aLonE6 / 1e6); return 0; }
extern "C" int emu_mark() { return (int) gHeap; }
extern "C" int emu_release(int aMark) { gHeap = (char*) aMark; gM->nv = gM->nf = 0; gM->xyz = 0; gM->idx = 0; gM->uv = 0; gM->shade = 0; return 0; }
extern "C" int emu_dec(const unsigned char* aData, int aLen, int aCell, int* aOut)
    {
    int r = VtsDecode(aData, aLen, gF, aCell, *gM);
    aOut[0] = r; aOut[1] = gM->nv; aOut[2] = gM->nf; aOut[3] = gM->origFaces;
    unsigned int h = 2166136261u;
    for (int i = 0; i < gM->nv * 3; i++) h = (h ^ (unsigned int) (gM->xyz[i] + (i % 3 == 0 ? gM->ox : i % 3 == 1 ? gM->oy : gM->oz))) * 16777619u;
    for (int i = 0; i < gM->nf * 3; i++) h = (h ^ gM->idx[i]) * 16777619u;
    for (int i = 0; i < gM->nf * 6; i++) h = (h ^ gM->uv[i]) * 16777619u;
    aOut[4] = (int) h;
    h = 2166136261u;
    for (int i = 0; i < gM->nf; i++) h = (h ^ gM->shade[i]) * 16777619u;
    aOut[5] = (int) h;
    return r;
    }
