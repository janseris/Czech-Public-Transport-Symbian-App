// PC test (1.17): the 3D view's chunks (VtsChunks, R3D::ChunksVisible) against none: a scene dumped by
// test/vtstest.cpp (DUMP=) drawn from many cameras around its own, both ways; the frames must be the same.
// g++ -O2 -I../inc chunkcmp.cpp ../src/vtsdraw.cpp ../src/r3d.cpp ../src/r3dspan.c ../src/r3dgeo.c ../src/fastmem_ref.c
// ./a.out scene.bin [views]   (the drawing as test/arm/drvvts.cpp, built for the PC)
// Maps 9300 (0.92): the 3D view's city tiles (VtsDraw) in the Unicorn ARM emulator, built with the
// phone's GCC 2.9: a scene dumped by the PC test (test/vtstest.cpp, DUMP=) drawn as the phone does it,
// the instructions it takes and the frame. See emuvts.py and buildvts.sh.
#include "r3d.h"
#include "vts3d.h"

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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// 1.17: the chunks against none from many cameras around the scene's: the same frames
int main(int argc, char** argv)
    {
    FILE* f = fopen(argv[1], "rb"); fseek(f, 0, 2); long n = ftell(f); fseek(f, 0, 0);
    int* blob = (int*) malloc(n); fread(blob, 1, n, f); fclose(f);
    emu_init(); emu_prep(blob);
    static R3Pix fa[640 * 240], fb[640 * 240];
    int W = blob[0], H = blob[1], views = 0, bad = 0;
    long long ta = 0, tb = 0;
    int h3 = blob[3], h4 = blob[4], h5 = blob[5], h6 = blob[6], h7 = blob[7];
    unsigned s = 1;
    for (int k = 0; k < (argc > 2 ? atoi(argv[2]) : 200); k++)
        {
        s = s * 1103515245u + 12345u; int dyaw = (s >> 8) % 4096;
        s = s * 1103515245u + 12345u; int dp = (int) ((s >> 8) % 900) - 450;
        s = s * 1103515245u + 12345u; int dx = (int) ((s >> 8) % 3200) - 1600;
        s = s * 1103515245u + 12345u; int dz = (int) ((s >> 8) % 3200) - 1600;
        s = s * 1103515245u + 12345u; int dh = (int) ((s >> 8) % 1600);
        blob[3] = h3 + dx; blob[5] = h5 + dz; blob[4] = h4 + dh; blob[6] = (h6 + dyaw) & 4095; blob[7] = dp;
        emu_vts(fa, blob, 3, 5000); int st[4]; emu_stats(st); ta += st[3];
        emu_vts(fb, blob, 0x103, 5000); emu_stats(st); tb += st[3];
        views++;
        if (memcmp(fa, fb, W * H * 2)) { bad++; if (bad < 5) printf("differ: yaw %d pitch %d dx %d dz %d dh %d\n", blob[6], dp, dx, dz, dh); }
        }
    printf("%s: %d views, %d differ (pixels %lld / %lld)\n", argv[1], views, bad, ta, tb);
    return bad != 0;
    }
