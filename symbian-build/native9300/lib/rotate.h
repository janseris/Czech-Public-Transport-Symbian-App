// Maps 9300: turning 64K-colour (16-bit) pixels, for the rotated map and the portrait screen
// (see rotate.cpp). Portable C++: tested on a PC (test/rotatetest.cpp).

#ifndef ROTATE_H
#define ROTATE_H

typedef unsigned short TPix;

// A quarter turn: the source (aSw x aSh, a portrait screen drawn upright) onto the destination
// (aSh x aSw, the phone's landscape screen). aDir 1: the source's top goes to the screen's left edge
// (the phone turned clockwise, keyboard on the left); aDir 2: to its right edge (keyboard on the right).
// Strides are in pixels.
void Rotate90(const TPix* aSrc, int aSStride, int aSw, int aSh, TPix* aDst, int aDStride, int aDir);

// The map turned by an angle: destination pixel (x, y) shows the source pixel at
//   (aScx, aScy) + R(-angle) * ((x, y) - (aDcx, aDcy)),
// where aCos and aSin are the angle's cosine and sine * 65536 (clockwise on the screen). Source pixels
// outside the source are aBg. No division and no floating point in the loop.
void RotateBlit(const TPix* aSrc, int aSStride, int aSw, int aSh, int aScx, int aScy,
    TPix* aDst, int aDStride, int aDw, int aDh, int aDcx, int aDcy, int aCos, int aSin, TPix aBg);

#endif
