// Maps 9300 (0.92): the 3D view's city tiles (VtsDraw) in the Unicorn ARM emulator, built with the
// phone's GCC 2.9: a scene dumped by the PC test (test/vtstest.cpp, DUMP=) drawn as the phone does it,
// the instructions it takes and the frame. See emuvts.py and buildvts.sh.
#include "r3d.h"
#include "vts3d.h"

static char* gHeap = (char*) 0x400000;
void* operator new(unsigned int n) { void* p = gHeap; gHeap += (n + 7) & ~7; return p; }
void* operator new[](unsigned int n) { return operator new(n); }
void operator delete(void*) {}
void operator delete[](void*) {}
extern "C" void __pure_virtual() {}
extern "C" void* memset(void* d, int c, unsigned int n) { char* p = (char*) d; while (n--) *p++ = (char) c; return d; }
extern "C" void* memcpy(void* d, const void* s, unsigned int n) { char* p = (char*) d; const char* q = (const char*) s; while (n--) *p++ = *q++; return d; }

VtsMesh::VtsMesh() : nv(0), nf(0), xyz(0), idx(0), uv(0), shade(0), ch(0) {}
VtsMesh::~VtsMesh() {}

static R3D* gR;

extern "C" int emu_init() { gR = new R3D; return 0; }

// 1.13: the scenes' corners (32 bits in the file) as the app keeps them: 16 bits from the box's middle,
// made before the frames (not counted)
static short* gXyz[64];
static int gOrg[64][3], gLoY[64], gHiY[64];
static VtsChunkSet* gCh[64];
extern "C" int emu_prep(const int* aBlob)
    {
    const int* h = aBlob;
    for (int k = 0; k < h[13] && k < 64; k++)
        {
        const int* t = h + 14 + k * 32;
        const int* x = (const int*) ((const char*) aBlob + t[26]);
        int lo[3], hi[3];
        for (int a = 0; a < 3; a++) { lo[a] = 0x7fffffff; hi[a] = -0x7fffffff; }
        for (int i = 0; i < t[0] * 3; i++) { int a = i % 3; if (x[i] < lo[a]) lo[a] = x[i]; if (x[i] > hi[a]) hi[a] = x[i]; }
        for (int a = 0; a < 3; a++) gOrg[k][a] = lo[a] + ((hi[a] - lo[a]) >> 1);
        gLoY[k] = lo[1]; gHiY[k] = hi[1];
        gXyz[k] = new short[t[0] * 3];
        for (int i = 0; i < t[0] * 3; i++) gXyz[k][i] = (short) (x[i] - gOrg[k][i % 3]);
        // 1.17: the chunks as VtsDecode makes them (the corners reordered, the file's idx changed to match)
        VtsMesh m;
        m.nv = t[0]; m.nf = t[1]; m.xyz = gXyz[k]; m.idx = (unsigned short*) ((char*) aBlob + t[27]); m.ch = 0;
        VtsChunks(m);
        gXyz[k] = m.xyz; gCh[k] = m.ch;
        }
    return 0;
    }

// aFlags: bit 0 the mesh path (MeshBegin), bit 1 the view cone in VtsDraw, bit 2 C geometry, bit 7 (1.13)
// the tiles the app's InView3D leaves out not drawn
extern "C" int emu_vts(R3Pix* aFrame, const int* aBlob, int aFlags, int aMax)
    {
    R3D& r = *gR;
    const int* h = aBlob;
    r.iMeshMode = aFlags & 1;
    r.iGeoMode = (aFlags & 4) ? 0 : 1;
    r.Begin(aFrame, h[0], h[1], h[0], h[2]);
    r.SetCamera(h[3], h[4], h[5], h[6], h[7]);
    r.SetFog(h[8], h[9], (R3Pix) h[10]);
    r.SetMax(aMax);
    r.SetPerspective(h[11] != 0);
    r.Background((R3Pix) h[10], (R3Pix) 0x9d6e);
    int s = R3D::Sin(h[6]), c = R3D::Cos(h[6]);
    VtsView view;
    view.x = h[3]; view.z = h[5]; view.sin = s; view.cos = c;
    int n = 0;
    for (int k = 0; k < h[13]; k++)
        {
        const int* t = h + 14 + k * 32;
        const char* b = (const char*) aBlob;
        VtsMesh m;
        m.nv = t[0]; m.nf = t[1]; m.minX = t[2]; m.maxX = t[3]; m.minZ = t[4]; m.maxZ = t[5];
        m.xyz = gXyz[k]; m.ox = gOrg[k][0]; m.oy = gOrg[k][1]; m.oz = gOrg[k][2]; m.minY = gLoY[k]; m.maxY = gHiY[k]; m.idx = (unsigned short*) (b + t[27]); m.uv = (unsigned short*) (b + t[28]);
        m.shade = (R3Pix*) (b + t[29]);
        m.ch = (aFlags & 0x100) ? gCh[k] : 0;          // 1.17: the chunks
        R3Tex tex;
        tex.pix = (const R3Pix*) (b + t[30]); tex.wShift = tex.hShift = t[7]; tex.stride = 1 << t[7];
        if (aFlags & 0x80)
            {
            // 1.13: the app's InView3D (mapy.cpp): tiles wholly behind or beside the view not drawn
            int behind = 0, left = 0, right = 0;
            for (int q = 0; q < 4; q++)
                {
                long long x = (q & 1 ? m.maxX : m.minX) - h[3], z = (q & 2 ? m.maxZ : m.minZ) - h[5];
                long long f = x * s + z * c, rr = x * c - z * s, M = 16384 * 16 * 10;
                if (f < -M) behind++;
                if (rr > f + f / 4 + M) right++;
                if (-rr > f + f / 4 + M) left++;
                }
            if (behind == 4 || right == 4 || left == 4) continue;
            }
        if (r.Full()) break;
        n += VtsDraw(r, m, t[7] ? &tex : 0, h[12], t + 9, t[8], t[6], (aFlags & 2) ? &view : 0);
        }
    r.End();
    return n;
    }

extern "C" int emu_stats(int* aOut) { aOut[0] = gR->iStats.polys; aOut[1] = gR->iStats.drawn; aOut[2] = gR->iStats.culled; aOut[3] = gR->iStats.pixels; return 0; }

// random corners and triangles: the assembly against the C (r3dgeo.c); returns the differences
#include "r3dgeo.h"
static unsigned gS = 777;
static int Rnd(int aN) { gS = gS * 1103515245u + 12345u; return (int) ((gS >> 8) % (unsigned) aN); }
extern "C" int emu_vtstest(int aRounds)
    {
    static int oa[64 * 2], ob[64 * 2];
    static short xyz[3 * 64];               // 1.13: 16 bits (from a mesh's origin)
    static short a[4 * 64], b[4 * 64];      // 1.13: the corners as shorts
    static unsigned short idx[3 * 64];
    static int sk[16];
    int recip[8192];
    recip[0] = 1 << 30;
    for (int i = 1; i < 8192; i++) recip[i] = (1 << 30) / i;
    int bad = 0;
    for (int r = 0; r < aRounds; r++)
        {
        int cam[9];
        cam[0] = Rnd(4000) - 2000; cam[1] = Rnd(400) - 100; cam[2] = Rnd(4000) - 2000;
        int yaw = Rnd(4096), pitch = Rnd(1400) - 700;
        cam[3] = R3D::Cos(yaw); cam[4] = R3D::Sin(yaw); cam[5] = R3D::Cos(pitch); cam[6] = R3D::Sin(pitch); cam[7] = 30000; cam[8] = 60000;
        R3ProjPrm prm; prm.focal = 240; prm.cx = 200; prm.cy = 100; prm.recip = recip;
        int base = Rnd(3) == 0 ? 0 : Rnd(1000);
        int spread = 1 << (6 + Rnd(9));
        for (int i = 0; i < 64; i++)
            {
            xyz[i * 3] = (short) (cam[0] + Rnd(spread * 2) - spread);
            xyz[i * 3 + 1] = (short) (Rnd(4) == 0 ? base : base + Rnd(spread) - spread / 4);
            xyz[i * 3 + 2] = (short) (cam[2] + Rnd(spread * 2) - spread);
            if (Rnd(50) == 0) xyz[i * 3] = 32000;          // more than 30000 from the camera
            }
        for (int i = 0; i < 64 * 4; i++) a[i] = b[i] = 0x5555;
        int ra = r3_verts(xyz, 64, base, a, cam, &prm), rb = r3_verts_c(xyz, 64, base, b, cam, &prm);
        for (int i = 0; i < 64; i++)
            {
            int fl = a[i * 4 + 3];
            if (fl != b[i * 4 + 3]) { bad++; continue; }
            if ((fl & 1) && (a[i * 4] != b[i * 4] || a[i * 4 + 1] != b[i * 4 + 1] || a[i * 4 + 2] != b[i * 4 + 2])) bad++;
            if ((fl & 4) && a[i * 4 + 2] != b[i * 4 + 2]) bad++;
            }
        (void) ra; (void) rb;
        for (int i = 0; i < 3 * 64; i++) idx[i] = (unsigned short) Rnd(64);
        int nsk = Rnd(5);
        for (int i = 0; i < nsk; i++)
            {
            int x0 = (cam[0] + Rnd(spread * 2) - spread) * 3, z0 = (cam[2] + Rnd(spread * 2) - spread) * 3;
            sk[i * 4] = x0; sk[i * 4 + 1] = z0; sk[i * 4 + 2] = x0 + Rnd(spread * 3); sk[i * 4 + 3] = z0 + Rnd(spread * 3);
            }
        for (int g = 0; g < 16; g++)
            {
            int poly[40];
            int span = Rnd(4) == 0 ? 16000 : 1 << (2 + Rnd(9));
            int c0x = Rnd(800) - 200, c0y = Rnd(400) - 100;
            for (int i = 0; i < 3; i++)
                {
                poly[i] = c0x + Rnd(span * 2) - span; poly[8 + i] = c0y + Rnd(span * 2) - span;
                if (poly[i] > 16000) poly[i] = 16000; if (poly[i] < -16000) poly[i] = -16000;
                if (poly[8 + i] > 16000) poly[8 + i] = 16000; if (poly[8 + i] < -16000) poly[8 + i] = -16000;
                poly[24 + i] = Rnd(131072 * 2) - 131072 / 2; poly[32 + i] = Rnd(131072 * 2) - 131072 / 2;
                }
            unsigned int ga[6], gb[6];
            for (int i = 0; i < 6; i++) ga[i] = gb[i] = 0;
            int x = r3_texgrad(poly, recip, ga), y = r3_texgrad_c(poly, recip, gb);
            if (x != y) bad++;
            else if (x == 0) for (int i = 0; i < 6; i++) if (ga[i] != gb[i]) bad++;
            }
        for (int g = 0; g < 8; g++)
            {
            static int poly[40], La[400 * 2], Lb[400 * 2];
            int n = 3 + Rnd(2);
            int span = Rnd(5) == 0 ? 16000 : 1 << (2 + Rnd(9));
            int c0x = Rnd(800) - 200, c0y = Rnd(400) - 100;
            for (int i = 0; i < n; i++)
                {
                int x = c0x + Rnd(span * 2) - span, y = c0y + Rnd(span * 2) - span;
                if (x > 16000) x = 16000; if (x < -16000) x = -16000;
                if (y > 16000) y = 16000; if (y < -16000) y = -16000;
                poly[i] = x; poly[8 + i] = y;
                }
            for (int i = 0; i < 800; i++) La[i] = Lb[i] = 0x1234;
            R3EdgePrm ea, eb;
            ea.sx = poly; ea.n = n; ea.down = Rnd(2); ea.L = La; ea.h = 200; ea.recip = recip;
            eb = ea; eb.L = Lb;
            int x = r3_edges(&ea), y = r3_edges_c(&eb);
            if (x != y) bad++;
            else for (int i = 0; i < 400; i++) if (La[i] != Lb[i]) { bad++; break; }
            }
        R3TriPrm pa, pb;
        pa.idx = idx; pa.nf = 64; pa.mv = b; pa.xyz = xyz; pa.skip = sk; pa.nsk = nsk; pa.w = 400; pa.h = 200; pa.out = oa; pa.skipped = 0;
        static unsigned char fch[64];               // 1.17: random chunks, some off
        for (int i = 0; i < 64; i++) fch[i] = (unsigned char) Rnd(16);
        static unsigned char vt[16];
        for (int i = 0; i < 16; i++) vt[i] = (unsigned char) Rnd(2);
        pa.fch = Rnd(2) ? fch : 0; pa.vis = vt;
        pb = pa; pb.out = ob;
        int ka = r3_tris(&pa), kb = r3_tris_c(&pb);
        if (ka != kb || pa.skipped != pb.skipped) bad++;
        else for (int i = 0; i < ka; i++) if (oa[i] != ob[i]) bad++;
        }
    return bad;
    }

// 1.11: a JPEG decoded at full or half size (the 3D photos)
#include "jpegdec.h"
extern "C" int emu_jpeg(const unsigned char* aData, int aLen, unsigned short* aOut, int aHalf)
    {
    JpegHeader h;
    int r = JpegInfo(aData, aLen, h);
    if (r != KJpegOk) return r;
    static unsigned char* work = 0;
    if (!work) work = new unsigned char[2 * 1024 * 1024];
    return JpegDecode565Scaled(aData, aLen, work, 2 * 1024 * 1024, aOut, aHalf ? (h.width + 1) / 2 : h.width, aHalf);
    }
