// Maps 9300: turning pixels for the rotated map (Q / E, or with the direction of travel) and
// the portrait screen (200 x 640 on the 640 x 200 display). Both run once per drawn frame: a
// screen is 128 000 pixels, so the inner loops are only adds, shifts, a bounds check and a 16-bit
// copy (the phone has no FPU and no divide instruction). Portable C++: tested on a PC.

#include "rotate.h"

void Rotate90(const TPix* aSrc, int aSStride, int aSw, int aSh, TPix* aDst, int aDStride, int aDir)
    {
    // destination: aSh wide, aSw high
    for (int y = 0; y < aSw; y++)
        {
        TPix* d = aDst + y * aDStride;
        if (aDir == 1)
            {
            // source column aSw-1-y, read downwards
            const TPix* s = aSrc + (aSw - 1 - y);
            for (int x = 0; x < aSh; x++) { d[x] = *s; s += aSStride; }
            }
        else
            {
            // source column y, read upwards
            const TPix* s = aSrc + (aSh - 1) * aSStride + y;
            for (int x = 0; x < aSh; x++) { d[x] = *s; s -= aSStride; }
            }
        }
    }

void RotateBlit(const TPix* aSrc, int aSStride, int aSw, int aSh, int aScx, int aScy,
    TPix* aDst, int aDStride, int aDw, int aDh, int aDcx, int aDcy, int aCos, int aSin, TPix aBg)
    {
    for (int y = 0; y < aDh; y++)
        {
        int dy = y - aDcy, dx = -aDcx;
        // u = scx + dx cos + dy sin, v = scy - dx sin + dy cos (16.16, + half a pixel)
        int u = (aScx << 16) + 32768 + dx * aCos + dy * aSin;
        int v = (aScy << 16) + 32768 - dx * aSin + dy * aCos;
        TPix* d = aDst + y * aDStride;
        for (int x = 0; x < aDw; x++)
            {
            unsigned su = (unsigned) (u >> 16), sv = (unsigned) (v >> 16);
            d[x] = su < (unsigned) aSw && sv < (unsigned) aSh ? aSrc[sv * aSStride + su] : aBg;
            u += aCos;
            v -= aSin;
            }
        }
    }
