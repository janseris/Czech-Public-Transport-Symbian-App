// PC test of jpegdec.cpp: decodes a PNG file and writes the RGB565 pixels (little-endian) to a file.
// Usage: jpegtest in.png out.raw [bg 0xRRGGBB]. Prints width height and the decode time.
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "../inc/jpegdec.h"
int main(int argc, char** argv)
    {
    FILE* f = fopen(argv[1], "rb"); if (!f) return 2;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char* d = (unsigned char*) malloc(n); fread(d, 1, n, f); fclose(f);
    unsigned int bg = argc > 3 ? strtoul(argv[3], 0, 16) : 0xf0ede5;
    JpegHeader h; int e = JpegInfo(d, (int) n, h);
    if (e) { printf("info %d\n", e); return 1; }
    unsigned char* work = (unsigned char*) malloc(h.workBytes);
    unsigned short* out = (unsigned short*) malloc(h.width * h.height * 2);
    clock_t t0 = clock();
    e = JpegDecode565(d, (int) n, work, h.workBytes, out, h.width); (void) bg;
    double ms = (clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
    if (e) { printf("decode %d\n", e); return 1; }
    f = fopen(argv[2], "wb"); fwrite(out, 2, h.width * h.height, f); fclose(f);
    printf("%d %d %.2f\n", h.width, h.height, ms);
    return 0;
    }
