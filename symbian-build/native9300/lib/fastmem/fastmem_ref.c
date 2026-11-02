/* The C versions of fastmem.h, for every build without FM_ASM (the PC, emulator, THUMB). */

#include "fastmem.h"

#ifndef FM_ASM

void fm_memcpy(void* dst, const void* src, unsigned int n)
{
    char* d = (char*)dst;
    const char* s = (const char*)src;
    while (n--) *d++ = *s++;
}

void fm_memset(void* dst, int c, unsigned int n)
{
    char* d = (char*)dst;
    while (n--) *d++ = (char)c;
}

unsigned int fm_strlen(const char* s)
{
    unsigned int n = 0;
    if (!s) return 0;
    while (s[n]) n++;
    return n;
}

void fm_fill16(unsigned short* dst, unsigned int value, unsigned int count)
{
    while (count--) *dst++ = (unsigned short)value;
}

void fm_fill32(unsigned int* dst, unsigned int value, unsigned int count)
{
    while (count--) *dst++ = value;
}

#endif
