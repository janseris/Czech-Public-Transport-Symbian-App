// PC test (0.69): the panorama's sphere as DrawPano draws it, looking for holes (the background showing)
// and wrapped texels (a tile's top row at its bottom). g++ -O2 -I../inc panoseam.cpp ../src/r3d.cpp ../src/r3dspan.c ../src/r3dgeo.c ../src/fastmem_ref.c ../src/panosph.cpp
// env: TRI=1 triangles, CULL=1 only the pieces in view, MODECMP=1 the 0.59 span against 0.75's (the same frames)
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "r3d.h"
#include "panosph.h"
static R3Pix tile[256 * 256], base[1024 * 512];
int main(int argc, char** argv)
    {
    int I = argc > 1 ? atoi(argv[1]) : 512;
    for (int y = 0; y < 256; y++) for (int x = 0; x < 256; x++) tile[y * 256 + x] = (y < 1 || y > 254 || x < 1 || x > 254) ? 0x07e0 : 0x8410;
    for (int i = 0; i < 1024 * 512; i++) base[i] = 0x4208;
    const int W = 400, H = 200;
    static R3Pix fb[W * H];
    R3D r;
    static PanoSphTab tab;
    PanoSphBuild(tab);
    static unsigned char vis[KPsLat * KPsLon];
    long subm = 0;
    long holes = 0, wraps = 0, frames = 0;
    for (int zoomF = 100; zoomF <= 330; zoomF += 230)
    for (int pitch = -700; pitch <= 700; pitch += 50)
        for (int yaw = 0; yaw < 4096; yaw += 37)
            {
            for (int pass = 0; pass < (getenv("MODECMP") ? 2 : 1); pass++) {
            r.iSpanMode = getenv("MODECMP") ? pass : 1;
            r.Begin(fb, W, H, W, W * 3 / 5 * zoomF / 100);
            r.SetCamera(0, 0, 0, yaw, pitch);
            r.SetFog(1 << 28, (1 << 28) + 1, 0);
            r.SetMax(3000);
            r.SetPerspective(false);
            r.Background(0xf81f, 0xf81f);
            R3Tex bt = { base, 10, 9, 1024 }, tt = { tile, 8, 8, 256 };
            const int R = 1000, NL = 48, NT = 16;
            int focal = W * 3 / 5 * zoomF / 100;
            bool cull = getenv("CULL") != 0;
            if (cull) subm += PanoSphVisible(tab, yaw, pitch, W, H, focal, vis);
            for (int j = 0; j < NT; j++)
                for (int i = 0; i < NL; i++)
                    {
                    if (cull && !vis[j * NL + i]) continue;
                    R3Vtx v[4];
                    for (int c = 0; c < 4; c++)
                        {
                        int a = i + (c == 1 || c == 2 ? 1 : 0), b = j + (c >= 2 ? 1 : 0);
                        int lon = 4096 * a / NL, lat = 1024 - 2048 * b / NT;
                        int cl = R3D::Cos(lat), sl = R3D::Sin(lat);
                        v[c].x = (R * ((cl * R3D::Sin(lon)) >> 14)) >> 14;
                        v[c].z = (R * ((cl * R3D::Cos(lon)) >> 14)) >> 14;
                        v[c].y = (R * sl) >> 14;
                        if (cull) { const int* q = tab.v[b * (NL + 1) + a]; if (q[0] != v[c].x || q[1] != v[c].y || q[2] != v[c].z) { printf("table differs\n"); return 1; } }
                        v[c].u = (512 + 1024 * a / NL) * 256;
                        v[c].v = (512 * b / NT * 256) < 512 * 256 - 1 ? 512 * b / NT * 256 : 512 * 256 - 1;
                        }
                    const R3Tex* tex = &bt;
                    if ((i / 3 + j / 2) % 2 == 0)
                        {
                        tex = &tt;
                        int T = 256 * 256, S = T - 2 * I;
                        for (int c = 0; c < 4; c++)
                            {
                            int a = (i % 3) + (c == 1 || c == 2 ? 1 : 0), b = (j % 2) + (c >= 2 ? 1 : 0);
                            v[c].u = I + S * a / 3; v[c].v = I + S * b / 2;
                            }
                        }
                    if (getenv("TRI")) { R3Vtx t2[3] = { v[0], v[2], v[3] }; r.Poly(v, 3, tex, 0x6186, true); r.Poly(t2, 3, tex, 0x6186, true); }
                    else r.Poly(v, 4, tex, 0x6186, true);
                    }
            r.End();
            if (getenv("MODECMP")) { static R3Pix prev[W * H]; if (pass == 0) memcpy(prev, fb, sizeof prev); else if (memcmp(prev, fb, sizeof prev)) { printf("mode differs at pitch %d yaw %d\n", pitch, yaw); return 1; } }
            }
            frames++;
            long h = 0;
            for (int k = 0; k < W * H; k++) { if (fb[k] == 0xf81f) h++; if (fb[k] == 0x07e0) wraps++; }
            if (0) printf("zoom %d pitch %d yaw %d: %ld background pixels\n", zoomF, pitch, yaw, h);
            if (0) { int rows=0; for (int y=0;y<H;y++){int c=0; for(int x=0;x<W;x++) if(fb[y*W+x]==0xf81f)c++; if(c) {if(rows<6) printf("  row %d: %d px\n", y, c); rows++;}} }
            holes += h;
            }
    printf("%ld frames, %ld background pixels, %ld edge texels, %ld pieces submitted a frame\n", frames, holes, wraps, frames ? subm / frames : 0);
    return 0;
    }
