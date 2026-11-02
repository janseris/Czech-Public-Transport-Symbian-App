// Maps 9300: the 3D renderer's benchmark scenes for the Unicorn ARM emulator (test/arm/emu3d.py),
// built with the phone's GCC 2.9 (armi, -O2): the instructions each scene takes, and no fault.
#include "r3d.h"
#include "r3dspan.h"
#include "panosph.h"
#include "r3dgeo.h"

// a bump allocator for new / delete (the emulator has no heap)
static char* gHeap = (char*) 0x400000;
void* operator new(unsigned int n) { void* p = gHeap; gHeap += (n + 7) & ~7; return p; }
void* operator new[](unsigned int n) { return operator new(n); }
void operator delete(void*) {}
void operator delete[](void*) {}
extern "C" void __pure_virtual() {}

static unsigned BR(unsigned& s) { s = s * 1103515245u + 12345u; return (s >> 8) & 0xffffff; }
static R3Pix Rgb(int r, int g, int b) { return (R3Pix) (((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)); }

static R3D* gR;
static R3Pix* gTex;

extern "C" int emu_init(R3Pix* aTex)
    {
    gR = new R3D;
    gTex = aTex;
    for (int y = 0; y < 256; y++)
        for (int x = 0; x < 256; x++)
            gTex[y * 256 + x] = (((x >> 5) ^ (y >> 5)) & 1) ? Rgb(200, 80 + (y >> 2), 60) : Rgb(60, 120, 200 - (x >> 2));
    for (int y = 0; y < 128; y++)
        for (int x = 0; x < 128; x++) gTex[65536 + y * 128 + x] = gTex[y * 2 * 256 + x * 2];
    return 0;
    }

// the same scenes as CMapAppUi::BenchScene; aRaster 0: the geometry only
extern "C" int emu_scene(R3Pix* aFrame, int aTest, int aRaster)
    {
    R3D& r = *gR;
    r.Begin(aFrame, 640, 200, 640, 384);
    r.SetCamera(0, 0, 0, 0, 0);
    r.SetFog(1 << 28, (1 << 28) + 1, 0);
    r.SetMax(4000);
    if (aRaster) r.Background(Rgb(40, 40, 48), Rgb(40, 40, 48));
    R3Tex tex = { gTex, 8, 8, 256 };
    int kind = aTest < 3 ? aTest : (aTest - 3) % 3;
    const R3Tex* tp = kind == 0 ? 0 : &tex;
    r.SetPerspective(kind == 2);
    if (aTest < 3)
        {
        R3Vtx a[4] = { { -3000, -1200, 1200, 0, 255 * 256 }, { 3000, -1200, 1200, 255 * 256, 255 * 256 },
                       { 3000, 1200, 2600, 255 * 256, 0 }, { -3000, 1200, 2600, 0, 0 } };
        R3Vtx b[4] = { { -3000, 1200, 2600, 0, 0 }, { 3000, 1200, 2600, 255 * 256, 0 },
                       { 3000, 3000, 2600, 255 * 256, 255 * 256 }, { -3000, 3000, 2600, 0, 255 * 256 } };
        r.Poly(a, 4, tp, Rgb(180, 120, 90), true);
        r.Poly(b, 4, tp, Rgb(120, 160, 200), true);
        }
    else
        {
        static const int KTris[] = { 500, 1000, 2000, 4000 };
        int n = KTris[(aTest - 3) / 3];
        unsigned seed = 12345;
        for (int i = 0; i < n; i++)
            {
            int z = 300 + (int) (BR(seed) % 3700);
            int x = ((int) (BR(seed) % 1600) - 800) * z / 1000;
            int y = ((int) (BR(seed) % 600) - 300) * z / 1000;
            int s = z / 60 + 2;
            int u0 = (BR(seed) & 127) * 256, v0 = (BR(seed) & 127) * 256;
            R3Vtx v[3] = { { x - s, y - s, z, u0, v0 + 64 * 256 }, { x + s, y - s, z, u0 + 64 * 256, v0 + 64 * 256 }, { x, y + s, z, u0 + 32 * 256, v0 } };
            r.Poly(v, 3, tp, Rgb(100 + (i & 127), 140, 180 - (i & 63)), true);
            }
        }
    if (aRaster) r.End();
    return r.iStats.pixels * 0 + r.iStats.drawn;
    }

extern "C" int emu_mode(int aMode) { gR->iSpanMode = aMode; return 0; }

// 0.75: r3_span_tex4 against r3_span_tex on random spans (odd starts, any counts, steps either way,
// every texture size it takes): the number of spans that differ
extern "C" int emu_spantest(R3Pix* aA, R3Pix* aB, int aRounds)
    {
    unsigned seed = 777;
    int bad = 0;
    static const int KSh[5][2] = { { 8, 8 }, { 7, 7 }, { 9, 9 }, { 10, 9 }, { 3, 3 } };
    for (int k = 0; k < aRounds; k++)
        {
        const int* sh = KSh[k % 5];
        R3Span s;
        s.tex = gTex; s.shift = sh[0]; s.wmask = (1 << sh[0]) - 1; s.hmask = (1 << sh[1]) - 1;
        s.count = (int) (BR(seed) % 70);
        int off = (int) (BR(seed) & 7);
        s.u = (int) (BR(seed) << 8) ^ (int) (BR(seed) << 20); s.v = (int) (BR(seed) << 8) ^ (int) (BR(seed) << 20);
        s.du = (int) (BR(seed) % 0x60000) - 0x30000; s.dv = (int) (BR(seed) % 0x60000) - 0x30000;
        if (k % 7 == 0) { s.du *= 9; s.dv *= 9; }
        for (int i = 0; i < 80; i++) aA[i] = aB[i] = 0x1234;
        s.dst = aA + off; r3_span_tex(&s);
        s.dst = aB + off; r3_span_tex4(&s);
        for (int i = 0; i < 80; i++) if (aA[i] != aB[i]) { bad++; break; }
        }
    return bad;
    }

// 0.75: the panorama's sphere as the app draws it (PanoSphere's 0.72 path): aKind 0 the whole picture
// (1024 x 512 at aBase), 1 tiles at 128 px, 2 at 256 px (aTile) on every piece; level or 35 degrees up
static PanoSphTab* gTab;
extern "C" int emu_pano(R3Pix* aFrame, R3Pix* aBase, int aKind, int aRaster)
    {
    if (!gTab) { gTab = new PanoSphTab; PanoSphBuild(*gTab); }
    R3D& r = *gR;
    int up = aKind >= 3; aKind %= 3;
    int yaw = 300, pitch = up ? 400 : 0, W = 640, H = 200, F = 384;
    r.Begin(aFrame, W, H, W, F);
    r.SetCamera(0, 0, 0, yaw, pitch);
    r.SetFog(1 << 28, (1 << 28) + 1, 0);
    r.SetMax(3000);
    r.SetPerspective(false);
    if (aRaster) r.Background(Rgb(150, 175, 205), Rgb(110, 110, 110));
    static unsigned int visw[KPsLat * KPsLon / 4]; unsigned char* vis = (unsigned char*) visw;
    PanoSphVisible(*gTab, yaw, pitch, W, H, F, vis);
    R3Tex base = { aBase, 10, 9, 1024 };
    R3Tex tile = { aKind == 1 ? gTex + 65536 : gTex, aKind == 1 ? 7 : 8, aKind == 1 ? 7 : 8, aKind == 1 ? 128 : 256 };
    // 1.16: as CMapAppUi::PanoSphere: the corners' texture places from tables
    const PanoSphTab& T = *gTab;
    int size = 1 << tile.wShift, tu[4], tv[3];
    { int Tt = size * 256, I = 384 * size / 256, S = Tt - 2 * I;
      for (int a = 0; a < 4; a++) tu[a] = I + S * a / 3;
      for (int b = 0; b < 3; b++) tv[b] = I + S * b / 2; }
    for (int j = 0; j < KPsLat; j++)
        for (int i = 0; i < KPsLon; i++)
            {
            if (!vis[j * KPsLon + i]) continue;
            int col = T.col3[i], a0 = col & 3, b0 = j & 1;
            R3Vtx v[4];
            const int* q0 = T.v[j * (KPsLon + 1) + i];
            const int* q3 = q0 + 3 * (KPsLon + 1);
            v[0].x = q0[0]; v[0].y = q0[1]; v[0].z = q0[2];
            v[1].x = q0[3]; v[1].y = q0[4]; v[1].z = q0[5];
            v[2].x = q3[3]; v[2].y = q3[4]; v[2].z = q3[5];
            v[3].x = q3[0]; v[3].y = q3[1]; v[3].z = q3[2];
            const R3Tex* tex = &base;
            if (aKind)
                {
                tex = &tile;
                v[0].u = v[3].u = tu[a0]; v[1].u = v[2].u = tu[a0 + 1];
                v[0].v = v[1].v = tv[b0]; v[2].v = v[3].v = tv[b0 + 1];
                }
            else
                {
                v[0].u = v[3].u = T.bu[i]; v[1].u = v[2].u = T.bu[i + 1];
                v[0].v = v[1].v = T.bv[j]; v[2].v = v[3].v = T.bv[j + 1];
                }
            r.Poly(v, 3, tex, Rgb(96, 96, 104), true);
            R3Vtx t2[3];
            t2[0] = v[0]; t2[1] = v[2]; t2[2] = v[3];
            r.Poly(t2, 3, tex, Rgb(96, 96, 104), true);
            }
    if (aRaster) r.End();
    return r.iStats.pixels;
    }

// 0.75: the assembly geometry against its C reference on random corners (near and far, behind, out
// of range, every angle): the number of differences
extern "C" int emu_geotest(int aRounds)
    {
    unsigned seed = 4242;
    int bad = 0;
    int* recip = new int[8192];
    recip[0] = 1 << 30;
    for (int i = 1; i < 8192; i++) recip[i] = (1 << 30) / i;
    for (int k = 0; k < aRounds; k++)
        {
        int in[20], a[20], b[20], cam[9];
        int range = (k % 4 == 0) ? 40000 : (k % 4 == 1) ? 3000 : (k % 4 == 2) ? 300 : 12000;
        for (int i = 0; i < 20; i++) in[i] = (int) (BR(seed) % (unsigned) (2 * range)) - range;
        if (k % 5 == 0) in[1] = in[6] = in[11] = in[16] = 0;
        cam[0] = (int) (BR(seed) % 200) - 100; cam[1] = (int) (BR(seed) % 200) - 100; cam[2] = (int) (BR(seed) % 200) - 100;
        int yaw = (int) (BR(seed) & 4095), pitch = (int) (BR(seed) % 1400) - 700;
        cam[3] = R3D::Cos(yaw); cam[4] = R3D::Sin(yaw); cam[5] = R3D::Cos(pitch); cam[6] = R3D::Sin(pitch); cam[7] = 30000; cam[8] = 60000;
        int n = 3 + (k & 1);
        for (int i = 0; i < 20; i++) a[i] = b[i] = 0x55555555;
        int ra = r3_xform(in, n, a, cam), rb = r3_xform_c(in, n, b, cam);
        if (ra != rb) { bad++; continue; }
        if (ra >= 0) for (int i = 0; i < n * 5; i++) if (a[i] != b[i]) { bad++; break; }
        // projection of corners in front (z made >= 4)
        for (int i = 0; i < n; i++) { if (a[i * 5 + 2] < 4) a[i * 5 + 2] = 4 + (k & 7); }
        if (k % 9 == 0) a[2] = 8192 + k % 50;
        R3ProjPrm prm;
        prm.focal = 200 + (k & 511); prm.cx = (int) (BR(seed) % 700); prm.cy = (int) (BR(seed) % 300); prm.recip = recip;
        int sa[40], sb[40], ba[5], bb[5];
        for (int i = 0; i < 40; i++) sa[i] = sb[i] = 7;
        for (int i = 0; i < 5; i++) ba[i] = bb[i] = 9;
        int pa = r3_project(a, n, sa, &prm, ba), pb = r3_project_c(a, n, sb, &prm, bb);
        if (pa != pb) { bad++; continue; }
        if (pa == 0)
            {
            for (int i = 0; i < 8; i++) for (int j = 0; j < 5; j++) if (i < n && sa[i + j * 8] != sb[i + j * 8]) { bad++; i = 8; break; }
            for (int i = 0; i < 5; i++) if (ba[i] != bb[i]) { bad++; break; }
            }
        }
    return bad;
    }

// the C library's two the compiler calls (struct copies and zeroing)
extern "C" void* memset(void* d, int c, unsigned int n) { char* p = (char*) d; while (n--) *p++ = (char) c; return d; }
extern "C" void* memcpy(void* d, const void* s, unsigned int n) { char* p = (char*) d; const char* q = (const char*) s; while (n--) *p++ = *q++; return d; }
