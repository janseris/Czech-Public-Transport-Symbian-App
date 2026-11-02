/* Maps 9300: the 3D renderer's textured span (one scanline of a polygon), in ARM assembly on the phone
 * (r3dspan.c) and in C everywhere else (the PC, the emulator's C reference). C89, no writable data. */

#ifndef R3DSPAN_H
#define R3DSPAN_H

#ifdef __cplusplus
extern "C" {
#endif

/* the fields at fixed offsets: the assembly loads them with one LDM */
typedef struct
    {
    unsigned short* dst;        /*  0: the first pixel */
    int count;                  /*  4: pixels (> 0) */
    const unsigned short* tex;  /*  8: the texture, RGB565, its rows 1 << shift pixels apart */
    int u, v;                   /* 12, 16: texel position, 16.16 */
    int du, dv;                 /* 20, 24: per pixel */
    int shift;                  /* 28: log2 of the texture's row length (= its width) */
    int wmask, hmask;           /* 32, 36: width - 1, height - 1 (the texture repeats) */
    } R3Span;

/* affine: dst[i] = tex[((v >> 16) & hmask) << shift | ((u >> 16) & wmask)] */
void r3_span_tex(const R3Span* s);

/* 0.75: the same, 4 pixels a loop with paired stores; only while r3_span_ok (else r3_span_tex) */
void r3_span_tex4(const R3Span* s);
#define r3_span_ok(shift, hshift) ((shift) >= 3 && (shift) + (hshift) <= 19)

/* 0.75: a whole textured polygon's scanlines (the row loop in assembly too: ~110 instructions a span
 * of overhead in C, the call and the struct). Per row: the ends from L (16.16, left then right), the
 * pixel centres inside, clipped to 0..wmax; u = urow + du * x, v' = vrow + dv * x (v' as in
 * r3_span_tex4: v << (shift - 3), with dv, vrow, vy scaled the same), then the 4-pixel span. */
typedef struct
    {
    const int* L;               /*  0: the first row's ends; advanced */
    unsigned short* row;        /*  4: the first row's pixel 0; advanced */
    unsigned int urow;          /*  8: u at x = 0 of the row; advanced by uy */
    unsigned int vrow;          /* 12: v' at x = 0 */
    int rows;                   /* 16: > 0; counted down (1.12: the advanced ones first, together) */
    int stride;                 /* 20: bytes from row to row */
    unsigned int uy;            /* 24 */
    unsigned int vy;            /* 28 */
    int wmax;                   /* 32: the last column */
    const unsigned short* tex;  /* 36 */
    unsigned int du, dv;        /* 40, 44: per pixel (dv scaled) */
    unsigned int umask, vmask;  /* 48, 52: wmask << 1, hmask << (shift + 1) */
    int pixels;                 /* 56: added to */
    } R3Rows;

void r3_rows_tex4(R3Rows* r);
/* 1.16: the same without clipping the rows to 0..wmax (all the polygon's corners inside 0..wmax) */
void r3_rows_tex4n(R3Rows* r);

#ifdef __cplusplus
}
#endif

#endif
