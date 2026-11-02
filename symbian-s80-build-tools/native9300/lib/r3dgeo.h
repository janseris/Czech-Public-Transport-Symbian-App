/* Maps 9300 (0.75): the renderer's per-corner geometry in ARM assembly (r3dgeo.c; C on the PC): GCC 2.9
 * kept the loops' pointers on the stack (~90 instructions a corner to camera space, ~50 onto the
 * screen); these take ~35 and ~30. The same results to the bit (checked in the emulator, test/arm). */
#ifndef R3DGEO_H
#define R3DGEO_H

#ifdef __cplusplus
extern "C" {
#endif

/* aIn: n corners of 5 ints (x, y, z, u, v: R3Vtx); aOut: the same in camera space (x right, y up,
 * z forward; u, v copied); aCam: camX, camY, camZ, yawCos, yawSin, pitchCos, pitchSin, 30000, 60000.
 * Returns -1 if a corner is more than 30000 off the camera on an axis, else the corners with
 * z >= 4 (in front of the near plane), + 256 if any corner's y isn't 0 (not the ground). */
int r3_xform(const int* aIn, int aN, int* aOut, const int* aCam);

/* the projection's parameters (the assembly takes recip at offset 12) */
typedef struct { int focal, cx, cy; const int* recip; } R3ProjPrm;

/* aIn: n camera space corners (5 ints each); aS: the polygon's sx[8], then sy, sz, su, sv (8 each);
 * aPrm: focal, cx, cy, the reciprocal table (2^30 / z); aBox gets minx, maxx, miny, maxy, the z sum.
 * Screen x, y clamped to +-16000. Returns -1 (nothing done) if a z is 8192 or more. */
int r3_project(const int* aIn, int aN, int* aS, const R3ProjPrm* aPrm, int* aBox);

/* 0.92: a mesh's corners once each (shared by ~4 triangles): aXyz n x (x, y, z) (1.13: 16 bits, from
 * the mesh's origin: aCam and aBase from it too), y less aBase;
 * aOut 4 shorts a corner (1.13; 8 bytes: a mesh's ~700 fit the 8 KB data cache, 16 didn't): screen x,
 * y (clamped to +-16000), camera z (its low 16 bits: z is read only when projected, < 8192), flags (1: z >= 4 and in range:
 * projected; 2: y - aBase isn't 0; 4: in range, z < 4: behind). z >= 8192 (512 m): not projected. The same arithmetic as r3_xform and r3_project (or R3D::Poly's
 * division for z >= 8192), so a triangle from these is the one Poly would make. */
int r3_verts(const short* aXyz, int aN, int aBase, short* aOut, const int* aCam, const R3ProjPrm* aPrm);

/* 0.92: a mesh's triangles sorted out: idx 3 corners each, mv from r3_verts, xyz the world corners
 * (for the skip squares: nsk x (x0, z0, x1, z1), each * 3). out gets, in order, the triangles kept: f
 * (all corners projected, a front face, on the w x h screen) or f | 0x80000000 (a corner not
 * projected nor all behind: Poly decides). skipped: += those in a skip square. Returns the count. */
typedef struct { const unsigned short* idx; int nf; const short* mv; const short* xyz; const int* skip; int nsk; int w, h; int* out; int skipped;
    const unsigned char* fch; const unsigned char* vis; } R3TriPrm;     /* 1.17: fch (not NULL): face f only if vis[fch[f]] (40, 44) */
int r3_tris(R3TriPrm* aP);

/* 0.92: a textured polygon's planes from its first 3 corners (aS: SPoly's sx[8], sy, sz, su, sv): aOut
 * ux, uy, vx, vy (16.16 per pixel), u00, v00 (at the screen's 0, 0). Returns 0, 1 if the corners are in a
 * line (nothing to draw), 2 if twice the area is 8192 px or more (no reciprocal: R3D::Raster divides). */
int r3_texgrad(const int* aS, const int* aRecip, unsigned int* aOut);

/* 1.07: a polygon's rows: per screen row y, L[y * 2 + side] = the 16.16 x where its edge crosses (the
 * edges going down on side "down", the others on the other; R3D::Raster's loop). sx: SPoly's sx[8], sy
 * after it. Returns y0 | y1 << 16 (the rows, clipped to 0..h-1), -2 for none, -1 if the polygon is
 * 8192 rows or taller (no reciprocal: R3D::Raster divides). */
typedef struct { const int* sx; int n; int down; int* L; int h; const int* recip; } R3EdgePrm;
int r3_edges(const R3EdgePrm* aP);

/* 1.07: R3D::MeshTris's polygons (its SPoly, 1.13: 208 bytes: n 0, down 4, tex 8, next 12, depth 16,
 * color 20, sx 48, sy 80, sz 112, su 144, sv 176; tex: the pointer stored, 1.13) from list[i..kept), while count < max, up
 * to the first entry < 0 (Poly's): textured triangles only. i and count are advanced. */
typedef struct { const int* list; int i; int kept; const unsigned short* idx; const unsigned short* uv;
    const unsigned short* shade; const short* mv; char* polys; int count; int max; int* bucket; const int* tex;
    int shl; int shr; unsigned int third; } R3BuildPrm;   /* 1.16: shl holds 16 + shr - shl, shr holds shl (uv as words) */
void r3_build(R3BuildPrm* aP);

/* 1.12: a textured affine polygon (a power-of-two texture, its rows 1 << wShift pixels) drawn: r3_edges,
 * r3_texgrad, then r3_rows_tex4 (R3D::Raster's glue in assembly). The first 6 words are r3_edges'
 * R3EdgePrm. Returns 0 (drawn, or nothing to draw; pixels added to), 1 (R3D::Raster draws it: taller
 * than 8191 rows, or twice its area 8192 px or more). */
typedef struct { const int* sx; int n; int down; int* L; int h; const int* recip;
    int wmax; unsigned short* frame; int strideBytes; const unsigned short* tex; int wShift; int hShift; int pixels; } R3RasterPrm;
int r3_raster_tex(R3RasterPrm* aP);

/* 1.13: R3D::End's loop over the depth buckets in assembly: the textured affine polygons drawn with
 * r3_raster_tex (the polygon's tex not NULL, its rows 1 << wShift pixels, r3_span_ok); returns 1 at one
 * it doesn't draw (b, i that one: R3D::Raster draws it, then i = its next and call again), 0 at the end.
 * polys: the SPoly list (208 bytes each: n 0, down 4, tex 8, next 12, sx 48); rp: L, h, recip, wmax,
 * frame, strideBytes set (sx, n, down, tex, wShift, hShift per polygon; pixels added to). */
typedef struct { const char* polys; const int* bucket; int b; int i; int nb; int drawn; R3RasterPrm rp; } R3DrawPrm;
int r3_draw(R3DrawPrm* aP);

/* the same in C (the drawing test compares) */
int r3_xform_c(const int* aIn, int aN, int* aOut, const int* aCam);
int r3_project_c(const int* aIn, int aN, int* aS, const R3ProjPrm* aPrm, int* aBox);
int r3_verts_c(const short* aXyz, int aN, int aBase, short* aOut, const int* aCam, const R3ProjPrm* aPrm);
int r3_tris_c(R3TriPrm* aP);
int r3_texgrad_c(const int* aS, const int* aRecip, unsigned int* aOut);
int r3_edges_c(const R3EdgePrm* aP);
int r3_raster_tex_c(R3RasterPrm* aP);
int r3_draw_c(R3DrawPrm* aP);

#ifdef __cplusplus
}
#endif

#endif
