/* Maps 9300 (0.75): the renderer's per-corner geometry (r3dgeo.h): ARM assembly on the phone, C
 * elsewhere (the reference: the same arithmetic as R3D::Poly up to 0.74). */

#include "fastmem.h"
#include "r3dgeo.h"
#include "r3dspan.h"

#ifdef FM_ASM

__asm__(
"	.text\n"
"	.align 2\n"
"	.global r3_xform\n"
"r3_xform:\n"
"	stmfd	sp!, {r4-r11, lr}\n"
"	mov	ip, #0\n"
".Lx_loop:\n"
"	ldmia	r0!, {r4-r8}\n"              /* x y z u v */
"	cmp	r5, #0\n"
"	orrne	ip, ip, #256\n"
"	ldmia	r3, {r9, r10, r11}\n"        /* the camera */
"	sub	r4, r4, r9\n"
"	sub	r5, r5, r10\n"
"	sub	r6, r6, r11\n"
"	ldr	r9, [r3, #28]\n"             /* 30000 */
"	ldr	r10, [r3, #32]\n"            /* 60000 */
"	add	r11, r4, r9\n"
"	cmp	r11, r10\n"
"	bhi	.Lx_out\n"
"	add	r11, r6, r9\n"
"	cmp	r11, r10\n"
"	bhi	.Lx_out\n"
"	add	r11, r5, r9\n"
"	cmp	r11, r10\n"
"	bhi	.Lx_out\n"
"	str	r7, [r2, #12]\n"
"	str	r8, [r2, #16]\n"
"	add	lr, r3, #12\n"
"	ldmia	lr, {r9, r10, r11, lr}\n"    /* yaw cos, sin, pitch cos, sin */
"	mul	r7, r4, r10\n"
"	mla	r7, r6, r9, r7\n"
"	mov	r7, r7, asr #14\n"           /* zr = (dx * ys + dz * yc) >> 14 */
"	mul	r8, r4, r9\n"
"	mul	r4, r6, r10\n"
"	sub	r8, r8, r4\n"
"	mov	r8, r8, asr #14\n"           /* x = (dx * yc - dz * ys) >> 14 */
"	mul	r4, r5, r11\n"
"	mul	r6, r7, lr\n"
"	sub	r4, r4, r6\n"
"	mov	r4, r4, asr #14\n"           /* y = (dy * pc - zr * ps) >> 14 */
"	mul	r6, r5, lr\n"
"	mla	r6, r7, r11, r6\n"
"	mov	r6, r6, asr #14\n"           /* z = (dy * ps + zr * pc) >> 14 */
"	str	r8, [r2, #0]\n"
"	str	r4, [r2, #4]\n"
"	str	r6, [r2, #8]\n"
"	add	r2, r2, #20\n"
"	cmp	r6, #4\n"
"	addge	ip, ip, #1\n"
"	subs	r1, r1, #1\n"
"	bne	.Lx_loop\n"
"	mov	r0, ip\n"
"	ldmfd	sp!, {r4-r11, lr}\n"
"	bx	lr\n"
".Lx_out:\n"
"	mvn	r0, #0\n"
"	ldmfd	sp!, {r4-r11, lr}\n"
"	bx	lr\n"

/* r3_project: r0 in, r1 n, r2 sx[] (sy +32, sz +64, su +96, sv +128 bytes), r3 prm, [sp] box.
 * r4 minx r5 maxx r6 miny r7 maxy r8 zsum; r9-r11, ip, lr temps. */
"	.global r3_project\n"
"r3_project:\n"
"	stmfd	sp!, {r4-r11, lr}\n"
"	mov	r4, #0x40000000\n"           /* minx = 1 << 30 */
"	rsb	r5, r4, #0\n"                /* maxx = -(1 << 30) */
"	mov	r6, r4\n"
"	mov	r7, r5\n"
"	mov	r8, #0\n"
".Lp_loop:\n"
"	ldr	r9, [r0, #8]\n"              /* z */
"	cmp	r9, #8192\n"
"	bge	.Lp_out\n"
"	add	r8, r8, r9\n"
"	str	r9, [r2, #64]\n"             /* sz */
"	ldr	r10, [r0, #12]\n"
"	str	r10, [r2, #96]\n"            /* su */
"	ldr	r10, [r0, #16]\n"
"	str	r10, [r2, #128]\n"           /* sv */
"	ldr	r10, [r3, #12]\n"            /* the reciprocals */
"	ldr	r9, [r10, r9, lsl #2]\n"     /* inv = 2^30 / z */
"	ldr	r10, [r3]\n"                 /* focal */
"	ldr	r11, [r0]\n"
"	mul	ip, r10, r11\n"              /* focal * x */
"	smull	r11, lr, ip, r9\n"
"	mov	r11, r11, lsr #30\n"
"	orr	r11, r11, lr, lsl #2\n"      /* (focal * x * inv) >> 30 */
"	ldr	ip, [r3, #4]\n"
"	add	r11, ip, r11\n"              /* sx = cx + ... */
"	ldr	ip, [r0, #4]\n"
"	mul	lr, r10, ip\n"               /* focal * y */
"	smull	ip, r10, lr, r9\n"
"	mov	ip, ip, lsr #30\n"
"	orr	ip, ip, r10, lsl #2\n"
"	ldr	r10, [r3, #8]\n"
"	sub	ip, r10, ip\n"               /* sy = cy - ... */
"	mov	r9, #16000\n"
"	cmp	r11, r9\n"
"	movgt	r11, r9\n"
"	cmn	r11, r9\n"
"	rsblt	r11, r9, #0\n"
"	cmp	ip, r9\n"
"	movgt	ip, r9\n"
"	cmn	ip, r9\n"
"	rsblt	ip, r9, #0\n"
"	str	r11, [r2, #0]\n"
"	str	ip, [r2, #32]\n"
"	cmp	r11, r4\n"
"	movlt	r4, r11\n"
"	cmp	r11, r5\n"
"	movgt	r5, r11\n"
"	cmp	ip, r6\n"
"	movlt	r6, ip\n"
"	cmp	ip, r7\n"
"	movgt	r7, ip\n"
"	add	r0, r0, #20\n"
"	add	r2, r2, #4\n"
"	subs	r1, r1, #1\n"
"	bne	.Lp_loop\n"
"	ldr	r9, [sp, #36]\n"             /* box */
"	stmia	r9, {r4-r8}\n"
"	mov	r0, #0\n"
"	ldmfd	sp!, {r4-r11, lr}\n"
"	bx	lr\n"
".Lp_out:\n"
"	mvn	r0, #0\n"
"	ldmfd	sp!, {r4-r11, lr}\n"
"	bx	lr\n"

/* 0.92: r3_verts: r0 xyz, r1 n, r2 base, r3 out, [sp] cam, [sp, #4] prm (36 bytes more after the
 * push). Per corner: ip the flags; r4-r6 dx dy dz, then x y; lr z. */
"	.global r3_verts\n"
"r3_verts:\n"
"	stmfd	sp!, {r4-r11, lr}\n"
"	cmp	r1, #0\n"
"	ble	.Lv_done\n"
".Lv_loop:\n"
"	ldrsh	r4, [r0], #2\n"             /* x y z (1.13: 16 bits) */
"	ldrsh	r5, [r0], #2\n"
"	ldrsh	r6, [r0], #2\n"
"	subs	r5, r5, r2\n"                /* y - base */
"	movne	ip, #2\n"
"	moveq	ip, #0\n"
"	ldr	lr, [sp, #36]\n"             /* the camera */
"	ldmia	lr, {r7, r8, r9}\n"
"	sub	r4, r4, r7\n"
"	sub	r5, r5, r8\n"
"	sub	r6, r6, r9\n"
"	ldr	r7, [lr, #28]\n"             /* 30000 */
"	ldr	r8, [lr, #32]\n"             /* 60000 */
"	add	r9, r4, r7\n"
"	cmp	r9, r8\n"
"	bhi	.Lv_next\n"
"	add	r9, r6, r7\n"
"	cmp	r9, r8\n"
"	bhi	.Lv_next\n"
"	add	r9, r5, r7\n"
"	cmp	r9, r8\n"
"	bhi	.Lv_next\n"
"	add	r7, lr, #12\n"
"	ldmia	r7, {r7, r8, r9, r10}\n"     /* yaw cos, sin, pitch cos, sin */
"	mul	r11, r4, r8\n"
"	mla	r11, r6, r7, r11\n"
"	mov	r11, r11, asr #14\n"         /* zr = (dx * ys + dz * yc) >> 14 */
"	mul	lr, r5, r10\n"
"	mla	lr, r11, r9, lr\n"
"	mov	lr, lr, asr #14\n"           /* z = (dy * ps + zr * pc) >> 14 */
"	strh	lr, [r3, #4]\n"
"	cmp	lr, #4\n"
"	orrlt	ip, ip, #4\n"                /* behind */
"	blt	.Lv_next\n"
"	cmp	lr, #8192\n"
"	bge	.Lv_next\n"                  /* 512 m or more: Poly */
"	mul	r4, r7, r4\n"
"	mul	r6, r8, r6\n"
"	sub	r4, r4, r6\n"
"	mov	r4, r4, asr #14\n"           /* x = (dx * yc - dz * ys) >> 14 */
"	mul	r5, r9, r5\n"
"	mul	r6, r11, r10\n"
"	sub	r5, r5, r6\n"
"	mov	r5, r5, asr #14\n"           /* y = (dy * pc - zr * ps) >> 14 */
"	ldr	r6, [sp, #40]\n"             /* prm: focal, cx, cy, recip */
"	ldr	r7, [r6, #12]\n"
"	ldr	r7, [r7, lr, lsl #2]\n"      /* inv = 2^30 / z */
"	ldr	r8, [r6]\n"                  /* focal */
"	mul	r9, r8, r4\n"
"	smull	r10, r11, r9, r7\n"
"	mov	r10, r10, lsr #30\n"
"	orr	r10, r10, r11, lsl #2\n"
"	ldr	r11, [r6, #4]\n"
"	add	r10, r11, r10\n"             /* sx */
"	mul	r9, r8, r5\n"
"	smull	r4, r11, r9, r7\n"
"	mov	r4, r4, lsr #30\n"
"	orr	r4, r4, r11, lsl #2\n"
"	ldr	r11, [r6, #8]\n"
"	sub	r4, r11, r4\n"               /* sy */
"	mov	r9, #16000\n"
"	cmp	r10, r9\n"
"	movgt	r10, r9\n"
"	cmn	r10, r9\n"
"	rsblt	r10, r9, #0\n"
"	cmp	r4, r9\n"
"	movgt	r4, r9\n"
"	cmn	r4, r9\n"
"	rsblt	r4, r9, #0\n"
"	strh	r10, [r3]\n"
"	strh	r4, [r3, #2]\n"
"	orr	ip, ip, #1\n"
".Lv_next:\n"
"	strh	ip, [r3, #6]\n"
"	add	r3, r3, #8\n"
"	subs	r1, r1, #1\n"
"	bne	.Lv_loop\n"
".Lv_done:\n"
"	mov	r0, #0\n"
"	ldmfd	sp!, {r4-r11, lr}\n"
"	bx	lr\n"

/* 0.92: r3_tris: r0 the parameters (R3TriPrm: idx 0, nf 4, mv 8, xyz 12, skip 16, nsk 20, w 24,
 * h 28, out 32, skipped 36). r4 them, r0 idx, r1 f, r2 mv, r3 out; r5-r7 the corners. */
/* 1.17: r3_tris's loop as a macro: with the chunk test (fch not NULL, at 40) or without */
"	.macro	TRIS sfx, chk, sk\n"
".Lt_loop\\sfx:\n"
"	ldrh	r5, [r0], #2\n"
"	ldrh	r6, [r0], #2\n"
"	ldrh	r7, [r0], #2\n"
"	.if \\chk\n"
"	ldr	r8, [r4, #40]\n"             /* 1.17: the chunks: this face's off the screen? */
"	ldrb	r8, [r8, r1]\n"
"	ldr	r9, [r4, #44]\n"
"	ldrb	r9, [r9, r8]\n"
"	cmp	r9, #0\n"
"	beq	.Lt_next\\sfx\n"
"	.endif\n"
"	.if \\sk\n"
"	ldr	r8, [r4, #20]\n"             /* skip squares? */
"	cmp	r8, #0\n"
"	bne	.Lt_skip\\sfx\n"
"	.endif\n"
".Lt_back\\sfx:\n"
"	add	r5, r2, r5, lsl #3\n"
"	add	r6, r2, r6, lsl #3\n"
"	add	r7, r2, r7, lsl #3\n"
"	ldrh	r8, [r5, #6]\n"
"	ldrh	r9, [r6, #6]\n"
"	ldrh	r10, [r7, #6]\n"
"	and	r11, r8, r9\n"
"	and	r11, r11, r10\n"
"	tst	r11, #1\n"
"	beq	.Lt_slow\\sfx\n"
"	ldrsh	r8, [r5]\n"                 /* ax ay */
"	ldrsh	r10, [r6]\n"                /* bx by */
"	ldrsh	ip, [r7]\n"                 /* cx cy */
"	ldrsh	r9, [r5, #2]\n"
"	ldrsh	r11, [r6, #2]\n"
"	ldrsh	lr, [r7, #2]\n"
"	sub	r5, r8, ip\n"
"	sub	r6, r11, lr\n"
"	mul	r7, r5, r6\n"                /* (ax - cx) * (by - cy) */
"	sub	r5, r10, ip\n"
"	sub	r6, r9, lr\n"
"	mul	r5, r6, r5\n"                /* (bx - cx) * (ay - cy) */
"	subs	r7, r7, r5\n"                /* >= 0: a back face (or flat; fits 32 bits) */
"	bge	.Lt_next\\sfx\n"
"	mov	r5, r8\n"                    /* min x */
"	mov	r6, r8\n"                    /* max x */
"	cmp	r10, r5\n"
"	movlt	r5, r10\n"
"	cmp	r10, r6\n"
"	movgt	r6, r10\n"
"	cmp	ip, r5\n"
"	movlt	r5, ip\n"
"	cmp	ip, r6\n"
"	movgt	r6, ip\n"
"	cmp	r6, #0\n"
"	blt	.Lt_next\\sfx\n"
"	ldr	r7, [r4, #24]\n"
"	cmp	r5, r7\n"
"	bge	.Lt_next\\sfx\n"
"	mov	r5, r9\n"                    /* min y */
"	mov	r6, r9\n"                    /* max y */
"	cmp	r11, r5\n"
"	movlt	r5, r11\n"
"	cmp	r11, r6\n"
"	movgt	r6, r11\n"
"	cmp	lr, r5\n"
"	movlt	r5, lr\n"
"	cmp	lr, r6\n"
"	movgt	r6, lr\n"
"	cmp	r6, #0\n"
"	blt	.Lt_next\\sfx\n"
"	ldr	r7, [r4, #28]\n"
"	cmp	r5, r7\n"
"	bge	.Lt_next\\sfx\n"
"	cmp	r5, r6\n"
"	beq	.Lt_next\\sfx\n"
"	str	r1, [r3], #4\n"
".Lt_next\\sfx:\n"
"	add	r1, r1, #1\n"
"	ldr	r8, [r4, #4]\n"
"	cmp	r1, r8\n"
"	blt	.Lt_loop\\sfx\n"
".Lt_done\\sfx:\n"
"	ldr	r5, [r4, #32]\n"
"	sub	r0, r3, r5\n"
"	mov	r0, r0, asr #2\n"
"	ldmfd	sp!, {r4-r11, lr}\n"
"	bx	lr\n"
".Lt_slow\\sfx:\n"                        /* a corner not projected: all behind dropped, else Poly */
"	tst	r11, #4\n"
"	bne	.Lt_next\\sfx\n"
"	orr	r5, r1, #0x80000000\n"
"	str	r5, [r3], #4\n"
"	b	.Lt_next\\sfx\n"
".Lt_skip\\sfx:\n"                        /* the middle * 3 in a skip square: left out (1.13: xyz 16 bits) */
"	ldr	r9, [r4, #12]\n"             /* xyz */
"	add	r10, r5, r5, lsl #1\n"
"	add	r10, r9, r10, lsl #1\n"
"	ldrsh	r11, [r10]\n"
"	ldrsh	ip, [r10, #4]\n"
"	add	r10, r6, r6, lsl #1\n"
"	add	r10, r9, r10, lsl #1\n"
"	ldrsh	lr, [r10]\n"
"	add	r11, r11, lr\n"
"	ldrsh	lr, [r10, #4]\n"
"	add	ip, ip, lr\n"
"	add	r10, r7, r7, lsl #1\n"
"	add	r10, r9, r10, lsl #1\n"
"	ldrsh	lr, [r10]\n"
"	add	r11, r11, lr\n"
"	ldrsh	lr, [r10, #4]\n"
"	add	ip, ip, lr\n"
"	ldr	r9, [r4, #16]\n"
".Lt_sq\\sfx:\n"
"	ldr	r10, [r9]\n"
"	cmp	r11, r10\n"
"	blt	.Lt_sqnext\\sfx\n"
"	ldr	r10, [r9, #8]\n"
"	cmp	r11, r10\n"
"	bge	.Lt_sqnext\\sfx\n"
"	ldr	r10, [r9, #4]\n"
"	cmp	ip, r10\n"
"	blt	.Lt_sqnext\\sfx\n"
"	ldr	r10, [r9, #12]\n"
"	cmp	ip, r10\n"
"	blt	.Lt_skipped\\sfx\n"
".Lt_sqnext\\sfx:\n"
"	add	r9, r9, #16\n"
"	subs	r8, r8, #1\n"
"	bne	.Lt_sq\\sfx\n"
"	b	.Lt_back\\sfx\n"
".Lt_skipped\\sfx:\n"
"	ldr	r8, [r4, #36]\n"
"	add	r8, r8, #1\n"
"	str	r8, [r4, #36]\n"
"	b	.Lt_next\\sfx\n"
"	.endm\n"
"	.global r3_tris\n"
"r3_tris:\n"
"	stmfd	sp!, {r4-r11, lr}\n"
"	mov	r4, r0\n"
"	ldr	r0, [r4]\n"
"	ldr	r2, [r4, #8]\n"
"	ldr	r3, [r4, #32]\n"
"	mov	r1, #0\n"
"	ldr	r8, [r4, #4]\n"
"	cmp	r8, #0\n"
"	ble	.Lt_donen\n"
"	ldr	r8, [r4, #40]\n"            /* 1.17: 4 loops: with the chunks or not, with skip squares or not */
"	ldr	r9, [r4, #20]\n"
"	cmp	r8, #0\n"
"	bne	.Lt_chk\n"
"	cmp	r9, #0\n"
"	bne	.Lt_loops\n"
"	TRIS	n, 0, 0\n"
"	TRIS	s, 0, 1\n"
".Lt_chk:\n"
"	cmp	r9, #0\n"
"	bne	.Lt_loopks\n"
"	TRIS	k, 1, 0\n"
"	TRIS	ks, 1, 1\n"

/* 0.92: r3_texgrad: r0 the polygon (sx +0, sy +32, su +96, sv +128 bytes), r1 the reciprocals, r2 out.
 * r5 x1, r6 y1, r7 x2, r8 y2 (from corner 0), r10 2^30 / d (signed), r11 / ip the texture deltas;
 * x0, y0 kept in out[4], out[5] meanwhile. */
"	.global r3_texgrad\n"
"r3_texgrad:\n"
"	stmfd	sp!, {r4-r11, lr}\n"
"	ldr	r3, [r0]\n"
"	ldr	r4, [r0, #32]\n"
"	ldr	r5, [r0, #4]\n"
"	sub	r5, r5, r3\n"
"	ldr	r6, [r0, #36]\n"
"	sub	r6, r6, r4\n"
"	ldr	r7, [r0, #8]\n"
"	sub	r7, r7, r3\n"
"	ldr	r8, [r0, #40]\n"
"	sub	r8, r8, r4\n"
"	mul	r9, r5, r8\n"
"	mul	r10, r7, r6\n"
"	subs	r9, r9, r10\n"               /* d (fits 32 bits) */
"	moveq	r0, #1\n"
"	beq	.Lg_ret\n"
"	rsblt	r10, r9, #0\n"
"	movge	r10, r9\n"
"	cmp	r10, #8192\n"
"	movge	r0, #2\n"
"	bge	.Lg_ret\n"
"	ldr	r10, [r1, r10, lsl #2]\n"
"	cmp	r9, #0\n"
"	rsblt	r10, r10, #0\n"              /* inv */
"	str	r3, [r2, #16]\n"
"	str	r4, [r2, #20]\n"
"	ldr	r9, [r0, #96]\n"
"	ldr	r11, [r0, #100]\n"
"	sub	r11, r11, r9\n"              /* u1 */
"	ldr	ip, [r0, #104]\n"
"	sub	ip, ip, r9\n"                /* u2 */
"	smull	r1, r3, r11, r8\n"
"	smull	r4, r9, ip, r6\n"
"	subs	r1, r1, r4\n"
"	sbc	r3, r3, r9\n"
"	umull	r4, r9, r1, r10\n"
"	mla	r9, r3, r10, r9\n"
"	cmp	r10, #0\n"
"	sublt	r9, r9, r1\n"
"	mov	r4, r4, lsr #22\n"
"	orr	r4, r4, r9, lsl #10\n"
"	str	r4, [r2, #0]\n"
"	smull	r1, r3, ip, r5\n"
"	smull	r4, r9, r11, r7\n"
"	subs	r1, r1, r4\n"
"	sbc	r3, r3, r9\n"
"	umull	r4, r9, r1, r10\n"
"	mla	r9, r3, r10, r9\n"
"	cmp	r10, #0\n"
"	sublt	r9, r9, r1\n"
"	mov	r4, r4, lsr #22\n"
"	orr	r4, r4, r9, lsl #10\n"
"	str	r4, [r2, #4]\n"
"	ldr	lr, [r0, #96]\n"
"	mov	lr, lr, lsl #8\n"
"	ldr	r3, [r2, #16]\n"
"	ldr	r4, [r2, #0]\n"
"	mul	r9, r4, r3\n"
"	sub	lr, lr, r9\n"
"	ldr	r3, [r2, #20]\n"
"	ldr	r4, [r2, #4]\n"
"	mul	r9, r4, r3\n"
"	sub	lr, lr, r9\n"                /* u00 */
"	ldr	r9, [r0, #128]\n"
"	ldr	r11, [r0, #132]\n"
"	sub	r11, r11, r9\n"              /* v1 */
"	ldr	ip, [r0, #136]\n"
"	sub	ip, ip, r9\n"                /* v2 */
"	smull	r1, r3, r11, r8\n"
"	smull	r4, r9, ip, r6\n"
"	subs	r1, r1, r4\n"
"	sbc	r3, r3, r9\n"
"	umull	r4, r9, r1, r10\n"
"	mla	r9, r3, r10, r9\n"
"	cmp	r10, #0\n"
"	sublt	r9, r9, r1\n"
"	mov	r4, r4, lsr #22\n"
"	orr	r4, r4, r9, lsl #10\n"
"	str	r4, [r2, #8]\n"
"	smull	r1, r3, ip, r5\n"
"	smull	r4, r9, r11, r7\n"
"	subs	r1, r1, r4\n"
"	sbc	r3, r3, r9\n"
"	umull	r4, r9, r1, r10\n"
"	mla	r9, r3, r10, r9\n"
"	cmp	r10, #0\n"
"	sublt	r9, r9, r1\n"
"	mov	r4, r4, lsr #22\n"
"	orr	r4, r4, r9, lsl #10\n"
"	str	r4, [r2, #12]\n"
"	ldr	r1, [r0, #128]\n"
"	mov	r1, r1, lsl #8\n"
"	ldr	r3, [r2, #16]\n"
"	ldr	r4, [r2, #8]\n"
"	mul	r9, r4, r3\n"
"	sub	r1, r1, r9\n"
"	ldr	r3, [r2, #20]\n"
"	ldr	r4, [r2, #12]\n"
"	mul	r9, r4, r3\n"
"	sub	r1, r1, r9\n"                /* v00 */
"	str	lr, [r2, #16]\n"
"	str	r1, [r2, #20]\n"
"	mov	r0, #0\n"
".Lg_ret:\n"
"	ldmfd	sp!, {r4-r11, lr}\n"
"	bx	lr\n"

/* 1.07: r3_edges: r0 the parameters (R3EdgePrm: sx 0, n 4, down 8, L 12, h 16, recip 20). r1 sx, r2 n,
 * r3 sy, r7 h - 1, r6 the edge; per edge r9 ya, r10 yb, r11 x, r12 dx, lr the side. */
"	.global r3_edges\n"
"r3_edges:\n"
"	stmfd	sp!, {r4-r11, lr}\n"
"	ldr	r1, [r0]\n"
"	ldr	r2, [r0, #4]\n"
"	add	r3, r1, #32\n"
"	ldr	r4, [r3]\n"                  /* y0 */
"	mov	r5, r4\n"                    /* y1 */
"	mov	r6, #1\n"
".Le_mm:\n"
"	cmp	r6, r2\n"
"	bge	.Le_mmd\n"
"	ldr	r7, [r3, r6, lsl #2]\n"
"	cmp	r7, r4\n"
"	movlt	r4, r7\n"
"	cmp	r7, r5\n"
"	movgt	r5, r7\n"
"	add	r6, r6, #1\n"
"	b	.Le_mm\n"
".Le_mmd:\n"
"	sub	r6, r5, r4\n"
"	cmp	r6, #8192\n"
"	mvnge	r0, #0\n"                   /* -1: too tall, Raster divides */
"	bge	.Le_ret\n"
"	cmp	r4, #0\n"
"	movlt	r4, #0\n"
"	ldr	r7, [r0, #16]\n"
"	sub	r7, r7, #1\n"
"	cmp	r5, r7\n"
"	movgt	r5, r7\n"
"	cmp	r4, r5\n"
"	mvngt	r0, #1\n"                   /* -2: no rows */
"	bgt	.Le_ret\n"
"	stmfd	sp!, {r4, r5}\n"
"	mov	r6, #0\n"
".Le_edge:\n"
"	add	r8, r6, #1\n"
"	cmp	r8, r2\n"
"	moveq	r8, #0\n"
"	ldr	r9, [r3, r6, lsl #2]\n"     /* ya */
"	ldr	r10, [r3, r8, lsl #2]\n"    /* yb */
"	cmp	r9, r10\n"
"	beq	.Le_next\n"
"	ldr	r11, [r1, r6, lsl #2]\n"    /* xa */
"	ldr	r12, [r1, r8, lsl #2]\n"    /* xb */
"	ldr	lr, [r0, #8]\n"             /* side */
"	movgt	r4, r9\n"                   /* ya > yb: swapped, the other side */
"	movgt	r9, r10\n"
"	movgt	r10, r4\n"
"	movgt	r4, r11\n"
"	movgt	r11, r12\n"
"	movgt	r12, r4\n"
"	eorgt	lr, lr, #1\n"
"	sub	r4, r10, r9\n"               /* dy */
"	ldr	r5, [r0, #20]\n"
"	ldr	r4, [r5, r4, lsl #2]\n"      /* 2^30 / dy */
"	sub	r5, r12, r11\n"
"	smull	r12, r8, r5, r4\n"
"	mov	r12, r12, lsr #14\n"
"	orr	r12, r12, r8, lsl #18\n"     /* dx = ((xb - xa) * rec) >> 14 */
"	mov	r11, r11, lsl #16\n"         /* x */
"	cmp	r9, #0\n"
"	rsblt	r4, r9, #0\n"
"	mlalt	r11, r12, r4, r11\n"        /* above the screen: x += dx * -ys */
"	movlt	r9, #0\n"
"	cmp	r10, r7\n"
"	movgt	r10, r7\n"
"	ldr	r4, [r0, #12]\n"
"	add	r5, lr, r9, lsl #1\n"
"	add	r4, r4, r5, lsl #2\n"        /* &L[ys * 2 + side] */
"	subs	r5, r10, r9\n"
"	blt	.Le_next\n"
"	subs	r5, r5, #1\n"                /* 1.17: 2 rows a step */
"	blt	.Le_one\n"
".Le_fill:\n"
"	str	r11, [r4], #8\n"
"	add	r11, r11, r12\n"
"	str	r11, [r4], #8\n"
"	add	r11, r11, r12\n"
"	subs	r5, r5, #2\n"
"	bge	.Le_fill\n"
"	cmn	r5, #2\n"
"	beq	.Le_next\n"
".Le_one:\n"
"	str	r11, [r4], #8\n"
".Le_next:\n"
"	add	r6, r6, #1\n"
"	cmp	r6, r2\n"
"	blt	.Le_edge\n"
"	ldmfd	sp!, {r4, r5}\n"
"	orr	r0, r4, r5, lsl #16\n"
".Le_ret:\n"
"	ldmfd	sp!, {r4-r11, lr}\n"
"	bx	lr\n"

/* 1.07: r3_build (1.13: SPoly 208 bytes: n 0, down 4, tex 8, next 12, depth 16, color 20, the corners
 * from 48; tex: the mesh's shared copy): r11 the parameters (R3BuildPrm: list 0, i 4, kept 8, idx 12, uv 16, shade 20, mv 24,
 * polys 28, count 32, max 36, bucket 40, tex 44, 48: 16 + shr - shl, 52: shl (1.16), third 56); r1 i, r4 f, r5 the polygon,
 * lr the z sum, ip the corners' flags. */
"	.global r3_build\n"
"r3_build:\n"
"	stmfd	sp!, {r4-r11, lr}\n"
"	mov	r11, r0\n"
"	ldr	r1, [r11, #4]\n"
".Lb_loop:\n"
"	ldr	r2, [r11, #8]\n"
"	cmp	r1, r2\n"
"	bge	.Lb_done\n"
"	ldr	r2, [r11, #32]\n"
"	ldr	r3, [r11, #36]\n"
"	cmp	r2, r3\n"
"	bge	.Lb_done\n"
"	ldr	r3, [r11]\n"
"	ldr	r4, [r3, r1, lsl #2]\n"       /* f */
"	cmp	r4, #0\n"
"	blt	.Lb_done\n"
"	ldr	r5, [r11, #28]\n"
"	add	r6, r2, r2, lsl #2\n"
"	add	r6, r6, r2, lsl #3\n"         /* count * 13 */
"	add	r5, r5, r6, lsl #4\n"         /* the polygon: polys + count * 208 */
"	ldr	r6, [r11, #12]\n"
"	add	r7, r4, r4, lsl #1\n"
"	add	r6, r6, r7, lsl #1\n"         /* &idx[3 f] */
"	ldrh	r7, [r6]\n"
"	ldrh	r8, [r6, #2]\n"
"	ldrh	r9, [r6, #4]\n"
"	ldr	r6, [r11, #24]\n"
"	add	r7, r6, r7, lsl #3\n"
"	add	r8, r6, r8, lsl #3\n"
"	add	r9, r6, r9, lsl #3\n"
"	ldrsh	r0, [r7]\n"                  /* sx sy z flags */
"	ldrsh	r3, [r7, #2]\n"
"	ldrh	r6, [r7, #4]\n"
"	ldrh	r10, [r7, #6]\n"
"	str	r0, [r5, #48]\n"
"	str	r3, [r5, #80]\n"
"	str	r6, [r5, #112]\n"
"	mov	lr, r6\n"
"	mov	ip, r10\n"
"	ldrsh	r0, [r8]\n"
"	ldrsh	r3, [r8, #2]\n"
"	ldrh	r6, [r8, #4]\n"
"	ldrh	r10, [r8, #6]\n"
"	str	r0, [r5, #52]\n"
"	str	r3, [r5, #84]\n"
"	str	r6, [r5, #116]\n"
"	add	lr, lr, r6\n"
"	orr	ip, ip, r10\n"
"	ldrsh	r0, [r9]\n"
"	ldrsh	r3, [r9, #2]\n"
"	ldrh	r6, [r9, #4]\n"
"	ldrh	r10, [r9, #6]\n"
"	str	r0, [r5, #56]\n"
"	str	r3, [r5, #88]\n"
"	str	r6, [r5, #120]\n"
"	add	lr, lr, r6\n"
"	orr	ip, ip, r10\n"
"	ldr	r6, [r11, #16]\n"
"	add	r7, r4, r4, lsl #1\n"
"	add	r6, r6, r7, lsl #2\n"         /* &uv[6 f] */
"	ldr	r7, [r11, #48]\n"             /* 1.16: 16 + shr - shl */
"	ldr	r8, [r11, #52]\n"             /* 1.16: shl (1: the bit from the low half cleared) */
"	ldmia	r6, {r0, r3, r6}\n"          /* 1.16: the 6 halfwords as 3 words (uv is word-aligned) */
"	mov	r10, r0, lsl #16\n"
"	mov	r10, r10, lsr r7\n"
"	str	r10, [r5, #144]\n"
"	mov	r10, r0, lsr r7\n"
"	bic	r10, r10, r8\n"
"	str	r10, [r5, #176]\n"
"	mov	r10, r3, lsl #16\n"
"	mov	r10, r10, lsr r7\n"
"	str	r10, [r5, #148]\n"
"	mov	r10, r3, lsr r7\n"
"	bic	r10, r10, r8\n"
"	str	r10, [r5, #180]\n"
"	mov	r10, r6, lsl #16\n"
"	mov	r10, r10, lsr r7\n"
"	str	r10, [r5, #152]\n"
"	mov	r10, r6, lsr r7\n"
"	bic	r10, r10, r8\n"
"	str	r10, [r5, #184]\n"
"	mov	r0, #3\n"
"	mov	r3, #0\n"
"	ldr	r7, [r11, #44]\n"              /* the mesh's texture (1.13: shared, R3D::iMeshTex) */
"	stmia	r5, {r0, r3, r7}\n"           /* n, down, tex */
"	ldr	r0, [r11, #56]\n"
"	umull	r3, r6, lr, r0\n"
"	mov	r6, r6, lsr #1\n"             /* depth = z sum / 3 */
"	str	r6, [r5, #16]\n"
"	ldr	r0, [r11, #20]\n"
"	mov	r3, r4, lsl #1\n"
"	ldrh	r0, [r0, r3]\n"
"	strh	r0, [r5, #20]\n"              /* color */
"	mov	r0, r6, lsr #4\n"
"	mov	r3, #1024\n"
"	sub	r3, r3, #1\n"
"	cmp	r0, r3\n"
"	movgt	r0, r3\n"
"	sub	r0, r3, r0\n"
"	tst	ip, #2\n"
"	addne	r0, r0, #1024\n"              /* the bucket (not the ground: the second half) */
"	ldr	r3, [r11, #40]\n"
"	ldr	r7, [r3, r0, lsl #2]\n"
"	str	r7, [r5, #12]\n"              /* next */
"	ldr	r2, [r11, #32]\n"
"	str	r2, [r3, r0, lsl #2]\n"
"	add	r2, r2, #1\n"
"	str	r2, [r11, #32]\n"
"	add	r1, r1, #1\n"
"	b	.Lb_loop\n"
".Lb_done:\n"
"	str	r1, [r11, #4]\n"
"	ldmfd	sp!, {r4-r11, lr}\n"
"	bx	lr\n"

/* 1.12: r3_raster_tex (R3Rows as r3dspan.h 1.12): r11 the parameters (R3RasterPrm: sx 0, n 4, down 8, L 12, h 16, recip 20, wmax 24,
 * frame 28, strideBytes 32, tex 36, wShift 40, hShift 44, pixels 48); on the stack the planes (24 bytes)
 * then the R3Rows (60, at +24); r10 the rows (y0 | y1 << 16) across r3_texgrad. */
"	.global r3_raster_tex\n"
"r3_raster_tex:\n"
"	stmfd	sp!, {r4-r11, lr}\n"
"	sub	sp, sp, #88\n"
"	mov	r11, r0\n"
"	bl	r3_edges\n"                    /* r0: the parameters start as R3EdgePrm */
"	cmn	r0, #1\n"
"	beq	.Lrt_fb\n"
"	cmn	r0, #2\n"
"	beq	.Lrt_ok\n"
"	mov	r10, r0\n"
"	ldr	r0, [r11]\n"
"	ldr	r1, [r11, #20]\n"
"	mov	r2, sp\n"
"	bl	r3_texgrad\n"
"	cmp	r0, #1\n"
"	beq	.Lrt_ok\n"
"	cmp	r0, #2\n"
"	beq	.Lrt_fb\n"
"	mov	r4, r10, lsl #16\n"
"	mov	r4, r4, lsr #16\n"            /* y0 */
"	mov	r5, r10, lsr #16\n"            /* y1 */
"	ldmia	sp, {r6, r7, r8, r9, r10, ip}\n"   /* ux uy vx vy u00 v00 */
"	ldr	lr, [r11, #40]\n"
"	sub	lr, lr, #3\n"                  /* s = wShift - 3 */
"	ldr	r0, [r11, #12]\n"
"	add	r0, r0, r4, lsl #3\n"          /* L + y0 * 2 */
"	str	r0, [sp, #24]\n"
"	ldr	r1, [r11, #28]\n"
"	ldr	r2, [r11, #32]\n"
"	mla	r1, r2, r4, r1\n"              /* frame + y0 * stride */
"	str	r1, [sp, #28]\n"
"	str	r2, [sp, #44]\n"
"	sub	r3, r5, r4\n"
"	add	r3, r3, #1\n"
"	str	r3, [sp, #40]\n"               /* rows */
"	ldr	r0, [r11, #24]\n"
"	str	r0, [sp, #56]\n"               /* wmax */
"	mla	r0, r7, r4, r10\n"
"	str	r0, [sp, #32]\n"               /* urow = u00 + uy * y0 */
"	str	r7, [sp, #48]\n"               /* uy */
"	mla	r0, r9, r4, ip\n"
"	mov	r0, r0, lsl lr\n"
"	str	r0, [sp, #36]\n"               /* vrow = (v00 + vy * y0) << s */
"	mov	r0, r9, lsl lr\n"
"	str	r0, [sp, #52]\n"               /* vy << s */
"	ldr	r0, [r11, #36]\n"
"	str	r0, [sp, #60]\n"               /* tex */
"	str	r6, [sp, #64]\n"               /* du */
"	mov	r0, r8, lsl lr\n"
"	str	r0, [sp, #68]\n"               /* dv = vx << s */
"	ldr	r2, [r11, #40]\n"
"	mov	r0, #1\n"
"	mov	r0, r0, lsl r2\n"
"	sub	r0, r0, #1\n"
"	mov	r0, r0, lsl #1\n"
"	str	r0, [sp, #72]\n"               /* umask */
"	ldr	r3, [r11, #44]\n"
"	mov	r0, #1\n"
"	mov	r0, r0, lsl r3\n"
"	sub	r0, r0, #1\n"
"	add	r2, r2, #1\n"
"	mov	r0, r0, lsl r2\n"
"	str	r0, [sp, #76]\n"               /* vmask */
"	mov	r0, #0\n"
"	str	r0, [sp, #80]\n"               /* pixels */
"	ldr	r1, [r11]\n"                 /* 1.16: a triangle with its 3 corners' x in 0..wmax (unsigned: */
"	ldr	r5, [r11, #4]\n"             /* below 0 is above wmax): the rows unclipped (r3_rows_tex4n) */
"	ldmia	r1, {r2, r3, r4}\n"
"	ldr	r7, [r11, #24]\n"
"	add	r0, sp, #24\n"
"	cmp	r5, #3\n"
"	bne	.Lrt_clip\n"
"	cmp	r2, r7\n"
"	cmpls	r3, r7\n"
"	cmpls	r4, r7\n"
"	bhi	.Lrt_clip\n"
"	bl	r3_rows_tex4n\n"
"	b	.Lrt_rows\n"
".Lrt_clip:\n"
"	bl	r3_rows_tex4\n"
".Lrt_rows:\n"
"	ldr	r0, [sp, #80]\n"
"	ldr	r1, [r11, #48]\n"
"	add	r1, r1, r0\n"
"	str	r1, [r11, #48]\n"
".Lrt_ok:\n"
"	mov	r0, #0\n"
"	b	.Lrt_ret\n"
".Lrt_fb:\n"
"	mov	r0, #1\n"
".Lrt_ret:\n"
"	add	sp, sp, #88\n"
"	ldmfd	sp!, {r4-r11, lr}\n"
"	bx	lr\n"

/* 1.13: r3_draw: r11 the parameters (R3DrawPrm: polys 0, bucket 4, b 8, i 12, nb 16, drawn 20, rp 24);
 * r4 polys, r5 bucket, r6 b, r7 i, r8 nb, r9 drawn, r10 &rp. Per polygon: r0-r3 its texture (pix,
 * wShift, hShift, stride), ip the polygon. */
"	.global r3_draw\n"
"r3_draw:\n"
"	stmfd	sp!, {r4-r11, lr}\n"
"	mov	r11, r0\n"
"	ldmia	r11, {r4, r5, r6, r7, r8, r9}\n"
"	add	r10, r11, #24\n"
".Ld_loop:\n"
"	cmp	r7, #0\n"
"	bge	.Ld_poly\n"
"	add	r6, r6, #1\n"
"	cmp	r6, r8\n"
"	bge	.Ld_end\n"
"	ldr	r7, [r5, r6, lsl #2]\n"
"	b	.Ld_loop\n"
".Ld_poly:\n"
"	add	ip, r7, r7, lsl #2\n"
"	add	ip, ip, r7, lsl #3\n"          /* i * 13 */
"	add	ip, r4, ip, lsl #4\n"          /* the polygon (208 bytes) */
"	ldr	r0, [ip, #8]\n"
"	cmp	r0, #0\n"
"	beq	.Ld_slow\n"
"	ldmia	r0, {r0, r1, r2, r3}\n"
"	mov	lr, #1\n"
"	cmp	r3, lr, lsl r1\n"
"	bne	.Ld_slow\n"
"	cmp	r1, #3\n"
"	blt	.Ld_slow\n"
"	add	r3, r1, r2\n"
"	cmp	r3, #19\n"
"	bgt	.Ld_slow\n"
"	str	r0, [r10, #36]\n"
"	str	r1, [r10, #40]\n"
"	str	r2, [r10, #44]\n"
"	add	r0, ip, #48\n"
"	ldr	r1, [ip]\n"
"	ldr	r2, [ip, #4]\n"
"	stmia	r10, {r0, r1, r2}\n"         /* sx, n, down */
"	mov	r0, r10\n"
"	bl	r3_raster_tex\n"
"	cmp	r0, #0\n"
"	bne	.Ld_slow\n"
"	add	r9, r9, #1\n"
"	add	ip, r7, r7, lsl #2\n"               /* the polygon again (r0-r3, ip not kept) */
"	add	ip, ip, r7, lsl #3\n"
"	add	ip, r4, ip, lsl #4\n"
"	ldr	r7, [ip, #12]\n"               /* next */
"	b	.Ld_loop\n"
".Ld_slow:\n"
"	mov	r0, #1\n"
"	b	.Ld_ret\n"
".Ld_end:\n"
"	mov	r0, #0\n"
".Ld_ret:\n"
"	str	r6, [r11, #8]\n"
"	str	r7, [r11, #12]\n"
"	str	r9, [r11, #20]\n"
"	ldmfd	sp!, {r4-r11, lr}\n"
"	bx	lr\n"
);

#endif

/* the C versions: on the PC the only ones; on the phone the drawing test's comparison */
int r3_xform_c(const int* aIn, int aN, int* aOut, const int* aCam)
{
    int i, front = 0, notGround = 0;
    for (i = 0; i < aN; i++, aIn += 5, aOut += 5)
    {
        int dx = aIn[0] - aCam[0], dy = aIn[1] - aCam[1], dz = aIn[2] - aCam[2], zr, z;
        if ((unsigned int) (dx + 30000) > 60000u || (unsigned int) (dz + 30000) > 60000u || (unsigned int) (dy + 30000) > 60000u) return -1;
        if (aIn[1] != 0) notGround = 256;
        zr = (dx * aCam[4] + dz * aCam[3]) >> 14;
        aOut[0] = (dx * aCam[3] - dz * aCam[4]) >> 14;
        aOut[1] = (dy * aCam[5] - zr * aCam[6]) >> 14;
        z = (dy * aCam[6] + zr * aCam[5]) >> 14;
        aOut[2] = z; aOut[3] = aIn[3]; aOut[4] = aIn[4];
        if (z >= 4) front++;
    }
    return front + notGround;
}

int r3_project_c(const int* aIn, int aN, int* aS, const R3ProjPrm* aPrm, int* aBox)
{
    int i, minx = 1 << 30, maxx = -(1 << 30), miny = 1 << 30, maxy = -(1 << 30), zsum = 0;
    const int* recip = aPrm->recip;
    for (i = 0; i < aN; i++, aIn += 5)
    {
        int z = aIn[2], inv, sx, sy;
        if (z >= 8192) return -1;
        inv = recip[z];
        sx = aPrm->cx + (int) (((long long) (aPrm->focal * aIn[0]) * inv) >> 30);
        sy = aPrm->cy - (int) (((long long) (aPrm->focal * aIn[1]) * inv) >> 30);
        if (sx > 16000) sx = 16000; if (sx < -16000) sx = -16000;
        if (sy > 16000) sy = 16000; if (sy < -16000) sy = -16000;
        aS[i] = sx; aS[i + 8] = sy; aS[i + 16] = z; aS[i + 24] = aIn[3]; aS[i + 32] = aIn[4];
        if (sx < minx) minx = sx;
        if (sx > maxx) maxx = sx;
        if (sy < miny) miny = sy;
        if (sy > maxy) maxy = sy;
        zsum += z;
    }
    aBox[0] = minx; aBox[1] = maxx; aBox[2] = miny; aBox[3] = maxy; aBox[4] = zsum;
    return 0;
}

int r3_verts_c(const short* aXyz, int aN, int aBase, short* aOut, const int* aCam, const R3ProjPrm* aPrm)
{
    int i, inFront = 0;
    for (i = 0; i < aN; i++, aXyz += 3, aOut += 4)
    {
        int y = aXyz[1] - aBase;
        int dx = aXyz[0] - aCam[0], dy = y - aCam[1], dz = aXyz[2] - aCam[2], zr, x, z, inv, sx, sy;
        int f = y != 0 ? 2 : 0;
        aOut[3] = (short) f;
        if ((unsigned int) (dx + 30000) > 60000u || (unsigned int) (dz + 30000) > 60000u || (unsigned int) (dy + 30000) > 60000u) continue;
        zr = (dx * aCam[4] + dz * aCam[3]) >> 14;
        z = (dy * aCam[6] + zr * aCam[5]) >> 14;
        aOut[2] = (short) z;
        if (z < 4) { aOut[3] = (short) (f | 4); continue; }
        x = (dx * aCam[3] - dz * aCam[4]) >> 14;
        y = (dy * aCam[5] - zr * aCam[6]) >> 14;
        if (z >= 8192) continue;           /* 512 m or more: Poly (the assembly has no division) */
        inv = aPrm->recip[z];
        sx = aPrm->cx + (int) (((long long) (aPrm->focal * x) * inv) >> 30);
        sy = aPrm->cy - (int) (((long long) (aPrm->focal * y) * inv) >> 30);
        if (sx > 16000) sx = 16000; if (sx < -16000) sx = -16000;
        if (sy > 16000) sy = 16000; if (sy < -16000) sy = -16000;
        aOut[0] = (short) sx; aOut[1] = (short) sy; aOut[3] = (short) (f | 1);
        inFront++;
    }
    return inFront;
}

int r3_tris_c(R3TriPrm* aP)
{
    const unsigned short* idx = aP->idx;
    const short* mv = aP->mv;
    int* out = aP->out;
    int f;
    for (f = 0; f < aP->nf; f++, idx += 3)
    {
        int ia = idx[0], ib = idx[1], ic = idx[2], fl, ax, ay, bx, by, cx, cy, minx, maxx, miny, maxy;
        const short *a, *b, *c;
        if (aP->fch && !aP->vis[aP->fch[f]]) continue;     /* 1.17: its chunk off the screen */
        if (aP->nsk > 0)
        {
            const short* w = aP->xyz;
            int mx = w[ia * 3] + w[ib * 3] + w[ic * 3], mz = w[ia * 3 + 2] + w[ib * 3 + 2] + w[ic * 3 + 2], s;
            const int* k = aP->skip;
            for (s = 0; s < aP->nsk; s++, k += 4) if (mx >= k[0] && mx < k[2] && mz >= k[1] && mz < k[3]) break;
            if (s < aP->nsk) { aP->skipped++; continue; }
        }
        a = mv + ia * 4; b = mv + ib * 4; c = mv + ic * 4;
        fl = a[3] & b[3] & c[3];
        if (!(fl & 1))
        {
            if (!(fl & 4)) *out++ = f | 0x80000000;
            continue;
        }
        ax = a[0]; ay = a[1]; bx = b[0]; by = b[1]; cx = c[0]; cy = c[1];
        if ((ax - cx) * (by - cy) - (bx - cx) * (ay - cy) >= 0) continue;
        minx = maxx = ax; miny = maxy = ay;
        if (bx < minx) minx = bx; if (bx > maxx) maxx = bx;
        if (cx < minx) minx = cx; if (cx > maxx) maxx = cx;
        if (maxx < 0 || minx >= aP->w) continue;
        if (by < miny) miny = by; if (by > maxy) maxy = by;
        if (cy < miny) miny = cy; if (cy > maxy) maxy = cy;
        if (maxy < 0 || miny >= aP->h || maxy == miny) continue;
        *out++ = f;
    }
    return out - aP->out;
}

int r3_texgrad_c(const int* aS, const int* aRecip, unsigned int* aOut)
{
    int x0 = aS[0], yA = aS[8], x1 = aS[1] - x0, y1d = aS[9] - yA, x2 = aS[2] - x0, y2d = aS[10] - yA;
    int u1 = aS[25] - aS[24], u2 = aS[26] - aS[24], v1 = aS[33] - aS[32], v2 = aS[34] - aS[32];
    int d = x1 * y2d - x2 * y1d, inv;
    unsigned int ux, uy, vx, vy;
    if (d == 0) return 1;
    if (d < 0 ? -d >= 8192 : d >= 8192) return 2;
    inv = d < 0 ? -aRecip[-d] : aRecip[d];
    ux = (unsigned int) (((long long) u1 * y2d - (long long) u2 * y1d) * inv >> 22);
    uy = (unsigned int) (((long long) u2 * x1 - (long long) u1 * x2) * inv >> 22);
    vx = (unsigned int) (((long long) v1 * y2d - (long long) v2 * y1d) * inv >> 22);
    vy = (unsigned int) (((long long) v2 * x1 - (long long) v1 * x2) * inv >> 22);
    aOut[0] = ux; aOut[1] = uy; aOut[2] = vx; aOut[3] = vy;
    aOut[4] = ((unsigned int) aS[24] << 8) - ux * x0 - uy * yA;
    aOut[5] = ((unsigned int) aS[32] << 8) - vx * x0 - vy * yA;
    return 0;
}

int r3_edges_c(const R3EdgePrm* aP)
{
    const int* sx = aP->sx;
    const int* sy = aP->sx + 8;
    int n = aP->n, y0 = sy[0], y1 = sy[0], i;
    for (i = 1; i < n; i++) { if (sy[i] < y0) y0 = sy[i]; if (sy[i] > y1) y1 = sy[i]; }
    if (y1 - y0 >= 8192) return -1;
    if (y0 < 0) y0 = 0;
    if (y1 > aP->h - 1) y1 = aP->h - 1;
    if (y0 > y1) return -2;
    for (i = 0; i < n; i++)
    {
        int j = i + 1 < n ? i + 1 : 0;
        int ya = sy[i], yb = sy[j], xa = sx[i], xb = sx[j], side = aP->down, x, dx, ys, ye, *r;
        if (ya == yb) continue;
        if (ya > yb) { int t = ya; ya = yb; yb = t; t = xa; xa = xb; xb = t; side ^= 1; }
        x = xa << 16;
        dx = (int) (((long long) (xb - xa) * aP->recip[yb - ya]) >> 14);
        ys = ya; ye = yb;
        if (ys < 0) { x += dx * -ys; ys = 0; }
        if (ye > aP->h - 1) ye = aP->h - 1;
        r = aP->L + ys * 2 + side;
        for (; ys <= ye; ys++, r += 2, x += dx) *r = x;
    }
    return y0 | (y1 << 16);
}

int r3_raster_tex_c(R3RasterPrm* aP)
{
    unsigned int g[6];
    int er = r3_edges_c((const R3EdgePrm*) aP), gr, y0, y1, s;
    R3Rows rw;
    if (er == -1) return 1;
    if (er == -2) return 0;
    gr = r3_texgrad_c(aP->sx, aP->recip, g);
    if (gr == 1) return 0;
    if (gr == 2) return 1;
    y0 = er & 0xffff; y1 = er >> 16; s = aP->wShift - 3;
    rw.L = aP->L + y0 * 2; rw.row = (unsigned short*) ((char*) aP->frame + y0 * aP->strideBytes); rw.stride = aP->strideBytes;
    rw.rows = y1 - y0 + 1; rw.wmax = aP->wmax;
    rw.urow = g[4] + g[1] * (unsigned int) y0; rw.uy = g[1];
    rw.vrow = (g[5] + g[3] * (unsigned int) y0) << s; rw.vy = g[3] << s;
    rw.tex = aP->tex; rw.du = g[0]; rw.dv = g[2] << s;
    rw.umask = (unsigned int) ((1 << aP->wShift) - 1) << 1;
    rw.vmask = (unsigned int) ((1 << aP->hShift) - 1) << (aP->wShift + 1);
    rw.pixels = 0;
    r3_rows_tex4(&rw);
    aP->pixels += rw.pixels;
    return 0;
}

typedef struct { const unsigned short* pix; int wShift, hShift, stride; } R3TexC;     /* R3Tex (r3d.h) */

int r3_draw_c(R3DrawPrm* aP)
{
    for (;;)
    {
        const char* p;
        const R3TexC* t;
        while (aP->i < 0)
        {
            if (++aP->b >= aP->nb) return 0;
            aP->i = aP->bucket[aP->b];
        }
        p = aP->polys + aP->i * 208;
        t = *(const R3TexC* const*) (p + 8);
        if (!t || t->stride != 1 << t->wShift || t->wShift < 3 || t->wShift + t->hShift > 19) return 1;
        aP->rp.tex = t->pix; aP->rp.wShift = t->wShift; aP->rp.hShift = t->hShift;
        aP->rp.sx = (const int*) (p + 48); aP->rp.n = *(const int*) p; aP->rp.down = *(const int*) (p + 4);
        if (r3_raster_tex_c(&aP->rp)) return 1;
        aP->drawn++;
        aP->i = *(const int*) (p + 12);
    }
}

#if !defined(FM_ASM)
int r3_draw(R3DrawPrm* aP) { return r3_draw_c(aP); }
int r3_raster_tex(R3RasterPrm* aP) { return r3_raster_tex_c(aP); }
void r3_build(R3BuildPrm* aP) { (void) aP; }     /* the PC: R3D::MeshTris's C loop (iGeoMode 0 there too) */
int r3_edges(const R3EdgePrm* aP) { return r3_edges_c(aP); }
int r3_texgrad(const int* aS, const int* aRecip, unsigned int* aOut) { return r3_texgrad_c(aS, aRecip, aOut); }
int r3_verts(const short* aXyz, int aN, int aBase, short* aOut, const int* aCam, const R3ProjPrm* aPrm) { return r3_verts_c(aXyz, aN, aBase, aOut, aCam, aPrm); }
int r3_tris(R3TriPrm* aP) { return r3_tris_c(aP); }
int r3_xform(const int* aIn, int aN, int* aOut, const int* aCam) { return r3_xform_c(aIn, aN, aOut, aCam); }
int r3_project(const int* aIn, int aN, int* aS, const R3ProjPrm* aPrm, int* aBox) { return r3_project_c(aIn, aN, aS, aPrm, aBox); }
#endif
