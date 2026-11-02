/* Maps 9300: the renderer's textured span (r3dspan.h). ARM assembly on Symbian armi builds (FM_ASM as in
 * fastmem.h: ARMv4, ARM state, interworking return), C elsewhere. 10 instructions a pixel, against
 * ~25 for GCC 2.9's build of the C loop (the 0.58 benchmark: affine 2x the flat fill's time). Checked
 * against the C version in the Unicorn ARM emulator (test/arm). */

#include "fastmem.h"
#include "r3dspan.h"

#ifdef FM_ASM

__asm__(
"	.text\n"
"	.align 2\n"
"	.global r3_span_tex\n"
"r3_span_tex:\n"
"	stmfd	sp!, {r4-r11, lr}\n"
"	ldmia	r0, {r1-r10}\n"           /* r1 dst, r2 count, r3 tex, r4 u, r5 v, r6 du, r7 dv, r8 shift, r9 wmask, r10 hmask */
"	cmp	r2, #0\n"
"	ble	.Lsp_done\n"
".Lsp_loop:\n"
"	and	r11, r9, r4, lsr #16\n"
"	and	ip, r10, r5, lsr #16\n"
"	add	r11, r11, ip, lsl r8\n"
"	add	r11, r3, r11, lsl #1\n"
"	ldrh	r11, [r11]\n"
"	add	r4, r4, r6\n"
"	add	r5, r5, r7\n"
"	strh	r11, [r1], #2\n"
"	subs	r2, r2, #1\n"
"	bne	.Lsp_loop\n"
".Lsp_done:\n"
"	ldmfd	sp!, {r4-r11, lr}\n"
"	bx	lr\n"
/* 0.75: 4 pixels a loop, 2 words stored at once (STM: a quarter of the bus writes of STRH); the
 * texel's byte offset with immediate shifts only: v is kept as v << (shift - 3), so
 * ((v >> 16) & hmask) << (shift + 1) = (v' >> 12) & (hmask << (shift + 1)), and
 * ((u >> 16) & wmask) << 1 = (u >> 15) & (wmask << 1). Same pixels as r3_span_tex while
 * 3 <= shift and shift + log2(height) <= 19 (r3_span_ok). ~7.3 instructions a pixel against 10. */
"	.global r3_span_tex4\n"
"r3_span_tex4:\n"
"	stmfd	sp!, {r4-r11, lr}\n"
"	ldmia	r0, {r1-r10}\n"
"	sub	r8, r8, #3\n"
"	mov	r5, r5, lsl r8\n"
"	mov	r7, r7, lsl r8\n"
"	add	r8, r8, #4\n"
"	mov	r10, r10, lsl r8\n"         /* hmask << (shift + 1) */
"	mov	r9, r9, lsl #1\n"           /* wmask << 1 */
"	cmp	r2, #0\n"
"	ble	.Lt4_done\n"
"	tst	r1, #2\n"                    /* to a word boundary: one pixel */
"	beq	.Lt4_aligned\n"
"	and	r0, r10, r5, lsr #12\n"
"	and	lr, r9, r4, lsr #15\n"
"	add	r0, r0, lr\n"
"	ldrh	r0, [r3, r0]\n"
"	add	r5, r5, r7\n"
"	add	r4, r4, r6\n"
"	sub	r2, r2, #1\n"
"	strh	r0, [r1], #2\n"
".Lt4_aligned:\n"
"	subs	r2, r2, #4\n"
"	blt	.Lt4_tail\n"
".Lt4_loop:\n"
"	and	r0, r10, r5, lsr #12\n"
"	and	lr, r9, r4, lsr #15\n"
"	add	r5, r5, r7\n"
"	add	r4, r4, r6\n"
"	add	r0, r0, lr\n"
"	and	r8, r10, r5, lsr #12\n"
"	and	lr, r9, r4, lsr #15\n"
"	ldrh	r0, [r3, r0]\n"
"	add	r5, r5, r7\n"
"	add	r4, r4, r6\n"
"	add	r8, r8, lr\n"
"	and	r11, r10, r5, lsr #12\n"
"	and	lr, r9, r4, lsr #15\n"
"	ldrh	r8, [r3, r8]\n"
"	add	r5, r5, r7\n"
"	add	r4, r4, r6\n"
"	add	r11, r11, lr\n"
"	and	ip, r10, r5, lsr #12\n"
"	and	lr, r9, r4, lsr #15\n"
"	ldrh	r11, [r3, r11]\n"
"	add	r5, r5, r7\n"
"	add	r4, r4, r6\n"
"	add	ip, ip, lr\n"
"	ldrh	ip, [r3, ip]\n"
"	orr	r0, r0, r8, lsl #16\n"
"	subs	r2, r2, #4\n"
"	orr	r11, r11, ip, lsl #16\n"
"	stmia	r1!, {r0, r11}\n"
"	bge	.Lt4_loop\n"
".Lt4_tail:\n"
"	adds	r2, r2, #4\n"
"	beq	.Lt4_done\n"
".Lt4_one:\n"
"	and	r0, r10, r5, lsr #12\n"
"	and	lr, r9, r4, lsr #15\n"
"	add	r0, r0, lr\n"
"	ldrh	r0, [r3, r0]\n"
"	add	r5, r5, r7\n"
"	add	r4, r4, r6\n"
"	subs	r2, r2, #1\n"
"	strh	r0, [r1], #2\n"
"	bne	.Lt4_one\n"
".Lt4_done:\n"
"	ldmfd	sp!, {r4-r11, lr}\n"
"	bx	lr\n"

/* 0.75: r3_rows_tex4 (r3dspan.h): the row loop around the same 4-pixel span; per row ~40
 * instructions of overhead (1.12: ~30). The struct stays in memory (its pointer at [sp]); r3 tex, r6 du, r7 dv,
 * r9 umask, r10 vmask are loaded once. */
/* 1.16: the row loop as a macro: r3_rows_tex4 clips each row to 0..wmax; r3_rows_tex4n doesn't (the
 * polygon's corners are all inside 0..wmax: its rows' ends are within ~1/8 px of its corners' x,
 * ceil / floor keep them inside; 5 instructions a row less) */
"	.macro	ROWS4 sfx, clamp\n"
"	stmfd	sp!, {r0, r4-r11, lr}\n"
"	ldr	r3, [r0, #36]\n"
"	ldr	r6, [r0, #40]\n"
"	ldr	r7, [r0, #44]\n"
"	ldr	r9, [r0, #48]\n"
"	ldr	r10, [r0, #52]\n"
"	ldmia	r0, {r1, r2, r4, r5}\n"      /* 1.12: L, row, urow, vrow kept in registers from row to row */
".Lr_row\\sfx:\n"
"	ldmia	r1, {r8, r11}\n"                 /* 1.13: no L0 > L1 test: then ceil(L0) > floor(L1), skipped below */
"	add	r8, r8, #0xff00\n"
"	add	r8, r8, #0xff\n"
"	mov	r8, r8, asr #16\n"           /* xa: the first pixel centre inside */
"	mov	r11, r11, asr #16\n"         /* xb */
"	.if \\clamp\n"
"	cmp	r8, #0\n"
"	movlt	r8, #0\n"
"	ldr	ip, [r0, #32]\n"
"	cmp	r11, ip\n"
"	movgt	r11, ip\n"
"	.endif\n"
"	subs	lr, r11, r8\n"
"	blt	.Lr_next\\sfx\n"
"	mla	r4, r6, r8, r4\n"           /* u = urow + du * xa */
"	mla	r5, r7, r8, r5\n"
"	add	r1, r2, r8, lsl #1\n"
"	add	r2, lr, #1\n"
"	ldr	ip, [r0, #56]\n"
"	add	ip, ip, r2\n"
"	str	ip, [r0, #56]\n"
"	tst	r1, #2\n"
"	beq	.LrA1\\sfx\n"
"	and	r0, r10, r5, lsr #12\n"
"	and	lr, r9, r4, lsr #15\n"
"	add	r0, r0, lr\n"
"	ldrh	r0, [r3, r0]\n"
"	add	r5, r5, r7\n"
"	add	r4, r4, r6\n"
"	sub	r2, r2, #1\n"
"	strh	r0, [r1], #2\n"
".LrA1\\sfx:\n"
"	subs	r2, r2, #4\n"
"	blt	.LrT1\\sfx\n"
".LrL1\\sfx:\n"
"	and	r0, r10, r5, lsr #12\n"
"	and	lr, r9, r4, lsr #15\n"
"	add	r5, r5, r7\n"
"	add	r4, r4, r6\n"
"	add	r0, r0, lr\n"
"	and	r8, r10, r5, lsr #12\n"
"	and	lr, r9, r4, lsr #15\n"
"	ldrh	r0, [r3, r0]\n"
"	add	r5, r5, r7\n"
"	add	r4, r4, r6\n"
"	add	r8, r8, lr\n"
"	and	r11, r10, r5, lsr #12\n"
"	and	lr, r9, r4, lsr #15\n"
"	ldrh	r8, [r3, r8]\n"
"	add	r5, r5, r7\n"
"	add	r4, r4, r6\n"
"	add	r11, r11, lr\n"
"	and	ip, r10, r5, lsr #12\n"
"	and	lr, r9, r4, lsr #15\n"
"	ldrh	r11, [r3, r11]\n"
"	add	r5, r5, r7\n"
"	add	r4, r4, r6\n"
"	add	ip, ip, lr\n"
"	ldrh	ip, [r3, ip]\n"
"	orr	r0, r0, r8, lsl #16\n"
"	subs	r2, r2, #4\n"
"	orr	r11, r11, ip, lsl #16\n"
"	stmia	r1!, {r0, r11}\n"
"	bge	.LrL1\\sfx\n"
".LrT1\\sfx:\n"
"	adds	r2, r2, #4\n"
"	beq	.LrD1\\sfx\n"
".LrO1\\sfx:\n"
"	and	r0, r10, r5, lsr #12\n"
"	and	lr, r9, r4, lsr #15\n"
"	add	r0, r0, lr\n"
"	ldrh	r0, [r3, r0]\n"
"	add	r5, r5, r7\n"
"	add	r4, r4, r6\n"
"	subs	r2, r2, #1\n"
"	strh	r0, [r1], #2\n"
"	bne	.LrO1\\sfx\n"
".LrD1\\sfx:\n"

".Lr_next\\sfx:\n"
"	ldr	r0, [sp]\n"
"	ldmia	r0, {r1, r2, r4, r5, r8, r11, ip, lr}\n"  /* L, row, urow, vrow, rows, stride, uy, vy (1.13: one load) */
"	add	r1, r1, #8\n"
"	add	r2, r2, r11\n"
"	add	r4, r4, ip\n"
"	add	r5, r5, lr\n"
"	subs	r8, r8, #1\n"
"	stmia	r0, {r1, r2, r4, r5, r8}\n"
"	bgt	.Lr_row\\sfx\n"
"	ldmfd	sp!, {r0, r4-r11, lr}\n"
"	bx	lr\n"
"	.endm\n"
"	.global r3_rows_tex4\n"
"r3_rows_tex4:\n"
"	ROWS4	c, 1\n"
"	.global r3_rows_tex4n\n"
"r3_rows_tex4n:\n"
"	ROWS4	n, 0\n"
);

#else

void r3_span_tex(const R3Span* s)
{
    unsigned short* d = s->dst;
    const unsigned short* t = s->tex;
    int n = s->count, u = s->u, v = s->v, du = s->du, dv = s->dv, sh = s->shift, wm = s->wmask, hm = s->hmask;
    while (n-- > 0)
    {
        *d++ = t[(((v >> 16) & hm) << sh) | ((u >> 16) & wm)];
        u += du;
        v += dv;
    }
}

#endif

#ifndef FM_ASM
void r3_span_tex4(const R3Span* s) { r3_span_tex(s); }
#endif

#ifndef FM_ASM
void r3_rows_tex4(R3Rows* r);
void r3_rows_tex4n(R3Rows* r) { r3_rows_tex4(r); }
void r3_rows_tex4(R3Rows* r)
{
    /* the same as the assembly, with r3_span_tex (on the PC: the reference) */
    int sh = -1;
    unsigned int vm = r->vmask;
    R3Span s;
    while (!(vm & 1)) { vm >>= 1; sh++; }         /* vmask = hmask << (shift + 1): shift + 1 trailing zeros */
    s.tex = r->tex; s.shift = sh; s.wmask = (int) (r->umask >> 1); s.hmask = (int) vm;
    /* v' = v << (shift - 3) mod 2^32: back to v mod 2^(35 - shift), all the bits the mask takes */
    s.du = (int) r->du; s.dv = (int) (r->dv >> (sh - 3));
    for (; r->rows > 0; r->rows--, r->L += 2, r->row = (unsigned short*) ((char*) r->row + r->stride), r->urow += r->uy, r->vrow += r->vy)
    {
        int xa, xb;
        if (r->L[0] > r->L[1]) continue;
        xa = (r->L[0] + 0xffff) >> 16; xb = r->L[1] >> 16;
        if (xa < 0) xa = 0;
        if (xb > r->wmax) xb = r->wmax;
        if (xa > xb) continue;
        r->pixels += xb - xa + 1;
        s.dst = r->row + xa; s.count = xb - xa + 1;
        s.u = (int) (r->urow + r->du * (unsigned int) xa);
        s.v = (int) ((r->vrow + r->dv * (unsigned int) xa) >> (sh - 3));
        r3_span_tex(&s);
    }
}
#endif
