/* SPDX-License-Identifier: MIT */
#ifndef Z62_MOVIE_LIMIT_H
#define Z62_MOVIE_LIMIT_H
#include "z62.h"

#define ML_WORD(a) (*(volatile z62_u32 *)(a))
#define ML_BYTE(a) (*(volatile unsigned char *)(a))
#define ML_CALL(a,r,...) ((r (*)(__VA_ARGS__))(a))
#define ML_STOCK_MS 1799000u
#define ML_MAX_MS 359999000u
#define ML_UI_SLOT 0xc296e254u
#define ML_UI_NATIVE 0x1045aafbu
#define ML_RECORD_SLOT 0xc1f5926cu
#define ML_RECORD_NATIVE 0x104a83f3u

static z62_u32 movie_limit_normal(z62_u32 config)
{
    z62_u32 index=14u*ML_BYTE(config+4)+2u*ML_BYTE(config+5)+ML_BYTE(config+12);
    return !ML_BYTE(0xc188ab6au) && ML_WORD(config)==1 && index<32 &&
        ML_WORD(config+8)!=1 && ML_WORD(config+16)!=1;
}

static z62_u32 movie_limit_capacity(z62_u32 config)
{
    z62_u32 selected=ML_BYTE(config+0x88),volumes=config+0x40;
    if (!movie_limit_normal(config) || selected>1 || !ML_WORD(volumes)) return 0;
    z62_u32 volume=volumes+32u*selected;
    z62_u32 reserve=ML_WORD(config+0x94),files=ML_WORD(config+0xcc);
    if (ML_BYTE(volume+8)!=5 || ML_WORD(volume+0x20)<files+ML_WORD(config+0x90)) return 0;
    z62_u32 lo=ML_WORD(volume+0x10),hi=ML_WORD(volume+0x14);
    if (!hi && lo<=reserve) return 0;
    hi-=lo<reserve;
    lo-=reserve;
    if (!ML_BYTE(config+4) && ML_BYTE(config+6)<=1 && !ML_BYTE(volume+0x1c) && hi) {
        lo=0xffffffffu;
        hi=0;
    }
    z62_u32 request[8]={0,config,lo,hi,ML_WORD(volume+0x18),files,0,0};
    z62_u32 duration=ML_CALL(0x1049261du,z62_u32,z62_u32)((z62_u32)request);
    return duration>ML_MAX_MS ? ML_MAX_MS : duration;
}

static void movie_limit_record(z62_u32 events)
{
    const z62_u32 config=0xc1f598c8u,parent=0xc1f59078u;
    z62_u32 extended=0;
    if ((events&1u) && !ML_BYTE(0xc18117a8u) && !ML_BYTE(config+0x58) &&
        !ML_BYTE(config+0x64) && ML_BYTE(config+0x67)<7 &&
        ML_WORD(config+0x5c)==ML_STOCK_MS && !ML_BYTE(parent+0x64) &&
        !ML_BYTE(parent+0x58) && ML_WORD(parent+0x5c)==ML_STOCK_MS) {
        z62_u32 duration=movie_limit_capacity(0xc296e658u);
        if (duration>ML_STOCK_MS) {
            ML_WORD(config+0x5c)=extended=duration;
            ML_WORD(parent+0x5c)=duration;
        }
    }
    ML_CALL(ML_RECORD_NATIVE,void,z62_u32)(events);
    z62_u32 budget=ML_WORD(0xc18117ecu);
    if (extended && ML_WORD(config+0x70)==1 && ML_BYTE(0xc18117a8u)==1 &&
        (budget==3596 || budget==3749)) {
        const z62_u32 format=0xc1f59958u;
        z62_u32 numerator=ML_WORD(format+4),denominator=ML_WORD(format+8);
        z62_u32 chunk=*(volatile unsigned short *)(format+0x12);
        if (numerator && denominator && chunk) {
            typedef unsigned long long u64;
            u64 divisor=(u64)denominator*chunk*1000u;
            u64 dividend=(u64)(extended+1000u)*numerator+divisor-1;
            u64 frames=ML_CALL(0x10439580u,u64,u64,u64)(dividend,divisor);
            if (frames>budget && frames<=0xffffffffu) ML_WORD(0xc18117ecu)=(z62_u32)frames;
        }
    }
}

static void movie_limit_ui(z62_u32 events)
{
    ML_CALL(ML_UI_NATIVE,void,z62_u32)(events);
    if (ML_BYTE(0xc188ab6au)) return;
    ML_CALL(0x1035d1c1u,void,z62_u32)(0xc188ab70u);
    const z62_u32 configs[2]={0xc296e2a0u,0xc296e658u};
    z62_u32 changed=0;
    for (z62_u32 i=0;i<2;++i) {
        z62_u32 config=configs[i];
        if (ML_WORD(config+0xd0)!=ML_STOCK_MS) continue;
        z62_u32 duration=movie_limit_capacity(config);
        if (duration<=ML_STOCK_MS) continue;
        ML_WORD(config+0xd0)=duration;
        changed|=1u<<i;
    }
    if (changed&1u)
        ML_CALL(0x10459921u,void,z62_u32)(configs[0]);
    ML_CALL(0x1035d1bbu,void,z62_u32)(0xc188ab70u);
    if ((changed&2u) && ML_BYTE(0xc188ab69u))
        ML_CALL(0x1045a253u,void,void)();
}

static z62_u32 movie_limit_ready(void)
{
    return ML_WORD(ML_UI_SLOT)==ML_UI_NATIVE && ML_WORD(ML_RECORD_SLOT)==ML_RECORD_NATIVE;
}

static void movie_limit_install(void)
{
    __asm__ volatile("dmb sy" ::: "memory");
    ML_WORD(ML_RECORD_SLOT)=(z62_u32)movie_limit_record;
    ML_WORD(ML_UI_SLOT)=(z62_u32)movie_limit_ui;
}
#endif
