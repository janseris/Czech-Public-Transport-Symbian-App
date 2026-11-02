// PC test (1.16): PanoSphVisible's pieces against all 768: the same frames over many views (yaw, pitch,
// 3 view sizes, 4 zooms). g++ -O2 -I../inc panocull.cpp ../src/r3d.cpp ../src/r3dspan.c ../src/r3dgeo.c ../src/fastmem_ref.c ../src/panosph.cpp
#include <stdio.h>
#include <string.h>
#include "r3d.h"
#include "panosph.h"
static R3Pix base[1024 * 512], fa[640 * 220], fb[640 * 220];
static PanoSphTab tab;
static void Draw(R3Pix* aFb, int W, int H, int F, int yaw, int pitch, const unsigned char* vis)
    {
    static R3D r;
    r.Begin(aFb, W, H, W, F);
    r.SetCamera(0, 0, 0, yaw, pitch);
    r.SetFog(1 << 28, (1 << 28) + 1, 0);
    r.SetMax(3000);
    r.SetPerspective(false);
    r.Background(0x1234, 0x1234);
    R3Tex t = { base, 10, 9, 1024 };
    for (int j = 0; j < KPsLat; j++)
        for (int i = 0; i < KPsLon; i++)
            {
            if (vis && !vis[j * KPsLon + i]) continue;
            R3Vtx v[4];
            for (int c = 0; c < 4; c++)
                {
                int a = i + (c == 1 || c == 2), b = j + (c >= 2);
                const int* q = tab.v[b * (KPsLon + 1) + a];
                v[c].x = q[0]; v[c].y = q[1]; v[c].z = q[2];
                v[c].u = (512 + 1024 * a / KPsLon) * 256;
                int vv = 512 * b / KPsLat * 256; v[c].v = vv < 512 * 256 - 1 ? vv : 512 * 256 - 1;
                }
            r.Poly(v, 3, &t, 0x8410, true);
            R3Vtx t2[3] = { v[0], v[2], v[3] };
            r.Poly(t2, 3, &t, 0x8410, true);
            }
    r.End();
    }
int main()
    {
    for (int i = 0; i < 1024 * 512; i++) base[i] = (R3Pix) (i * 2654435761u >> 16);
    PanoSphBuild(tab);
    static unsigned char vis[KPsLat * KPsLon];
    static const int sizes[3][2] = { { 640, 200 }, { 400, 200 }, { 480, 160 } };
    long views = 0, bad = 0, pieces = 0;
    for (int s = 0; s < 3; s++)
        for (int z = 0; z < 4; z++)
            {
            int W = sizes[s][0], H = sizes[s][1], F = W * 3 / 5 * (100 + 50 * z * z) / 100;
            for (int pitch = -1000; pitch <= 1000; pitch += 61)
                for (int yaw = 0; yaw < 4096; yaw += 97)
                    {
                    pieces += PanoSphVisible(tab, yaw, pitch, W, H, F, vis);
                    Draw(fa, W, H, F, yaw, pitch, NULL);
                    Draw(fb, W, H, F, yaw, pitch, vis);
                    views++;
                    if (memcmp(fa, fb, W * H * 2)) { if (bad < 5) printf("differ: %dx%d f %d yaw %d pitch %d\n", W, H, F, yaw, pitch); bad++; }
                    }
            }
    printf("%ld views, %ld differ, %.1f pieces a view\n", views, bad, (double) pieces / views);
    return bad != 0;
    }
