/* SPDX-License-Identifier: MIT */
#ifndef Z62_CODE_COPY_H
#define Z62_CODE_COPY_H
#include "z62.h"

static z62_u32 code_hash(z62_u32 start, z62_u32 size)
{
    const volatile unsigned char *source=(const volatile unsigned char *)start;
    z62_u32 hash=2166136261u;
    for (z62_u32 i=0;i<size;++i)
        hash=(hash^source[i])*16777619u;
    return hash;
}

static z62_u32 copy_code(unsigned char *destination, z62_u32 start, z62_u32 size, z62_u32 expected)
{
    const volatile unsigned char *source=(const volatile unsigned char *)start;
    if (code_hash(start,size)!=expected) return 1;
    for (z62_u32 i=0;i<size;++i)
        destination[i]=source[i];
    return 0;
}

static void write_stub(z62_u32 stub, z62_u32 target)
{
    *(volatile z62_u32 *)stub=0xf000f8dfu;
    *(volatile z62_u32 *)(stub+4)=target;
}

static void write_call(z62_u32 site, z62_u32 stub, z62_u32 target)
{
    z62_u32 delta=stub-site-4u;
    write_stub(stub,target);
    /* All stubs are in the positive BL range with S=0 and J1=J2=1. */
    *(volatile unsigned short *)site=(unsigned short)(0xf000u|(delta>>12&0x3ffu));
    *(volatile unsigned short *)(site+2)=(unsigned short)(0xf800u|(delta>>1&0x7ffu));
}

static void clean_code(unsigned char *bytes, z62_u32 size)
{
    for (z62_u32 p=(z62_u32)bytes;p<(z62_u32)bytes+size;p+=32)
        __asm__ volatile("mcr p15,0,%0,c7,c11,1" :: "r"(p) : "memory");
}

static void sync_code(void)
{
    z62_u32 zero=0;
    __asm__ volatile("dsb sy\n mcr p15,0,%0,c7,c1,0\n dsb sy\n isb sy"
                     :: "r"(zero) : "memory");
}
#endif
