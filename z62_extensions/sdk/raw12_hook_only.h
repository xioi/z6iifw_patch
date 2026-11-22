/* SPDX-License-Identifier: MIT */
#ifndef Z62_RAW12_HOOK_ONLY_H
#define Z62_RAW12_HOOK_ONLY_H
#include "z62.h"

#ifndef Z62_RAW12_SINGLE_FRAME
#define Z62_RAW12_SINGLE_FRAME 0
#endif

#define RH_WORD(a) (*(volatile z62_u32 *)(a))
#define RH_BYTE(a) (*(volatile unsigned char *)(a))
#define RH_CALL(a,r,...) ((r (*)(__VA_ARGS__))(a))
#define RH_MIPU_SLOT 0xc1801ed4u
#define RH_MOVIE_ACTIVE 0xc18117a8u

static z62_u32 raw12_hook_task[8];
static unsigned char raw12_hook_descriptor[28];
static z62_u32 raw12_hook_native;
static z62_u32 raw12_hook_installs;
static z62_u32 raw12_hook_notifications;
static z62_u32 raw12_hook_last_code;
static z62_u32 raw12_hook_last_status;
static z62_u32 raw12_hook_last_movie;
static z62_u32 raw12_hook_worker_iterations;

static z62_u32 raw12_hook_rom_function(z62_u32 address)
{
    return (address&1u) && (address&~1u)>=0x10020000u && (address&~1u)<0x12520000u;
}

static z62_u32 raw12_hook_event(z62_u32 code,z62_u32 status)
{
    z62_u32 result=RH_CALL(raw12_hook_native,z62_u32,z62_u32,z62_u32)(code,status);
    ++raw12_hook_notifications;
    raw12_hook_last_code=code;
    raw12_hook_last_status=status;
    raw12_hook_last_movie=RH_BYTE(RH_MOVIE_ACTIVE);
    return result;
}

#if Z62_RAW12_SINGLE_FRAME
static unsigned char *raw12_single_allocation;
static unsigned char *raw12_single_buffer;
static z62_u32 raw12_single_state;
static z62_u32 raw12_single_wait;
static z62_u32 raw12_single_width;
static z62_u32 raw12_single_height;
static z62_u32 raw12_single_pitch;
static z62_u32 raw12_single_frame_bytes;
static z62_u32 raw12_single_sequence;
static z62_u32 raw12_single_source;
static z62_u32 raw12_single_captured;
static z62_u32 raw12_single_torn;
static z62_u32 raw12_single_rejected;

static z62_u32 raw12_single_ram(z62_u32 address,z62_u32 size)
{
    return address>=0x80000000u && size && address+size>=address;
}

static z62_u32 raw12_single_fps(z62_u32 code)
{
    return code==23 || code==25 || code==29 || code==30;
}

static z62_u32 raw12_single_geometry(void)
{
    z62_u32 movie_width=RH_WORD(0x24070e00u)&0x7fffu;
    z62_u32 movie_height=RH_WORD(0x24070e04u)&0x1fffu;
    z62_u32 fps=RH_WORD(0x24070e0cu)&0xffffu;
    if(movie_width<16 || movie_width>1920 || movie_height<16 || movie_height>1080 ||
       !raw12_single_fps(fps) || (RH_WORD(0x20380300u)&255u)!=2 || (RH_WORD(0x203800a0u)&2u)==0) return 0;
    z62_u32 dimensions=RH_WORD(0x20380048u);
    raw12_single_width=dimensions>>16;
    raw12_single_height=dimensions&0xffffu;
    raw12_single_pitch=RH_WORD(0x2038030cu)&0xffffu;
    if(raw12_single_width<movie_width || raw12_single_width>2100 ||
       raw12_single_height<movie_height || raw12_single_height>1200 || (raw12_single_width&1u)) return 0;
    z62_u32 packed=(raw12_single_width*12u+7u)>>3;
    if(raw12_single_pitch<packed || raw12_single_pitch>8192) return 0;
    raw12_single_frame_bytes=raw12_single_pitch*raw12_single_height;
    return raw12_single_frame_bytes && raw12_single_frame_bytes<=(4u<<20);
}

static void raw12_single_release(void)
{
    if(raw12_single_allocation) RH_CALL(0x1034c23f,void,void *)(raw12_single_allocation);
    raw12_single_allocation=0;
    raw12_single_buffer=0;
}

static void raw12_single_step(void)
{
    if(RH_BYTE(RH_MOVIE_ACTIVE)!=1) {
        raw12_single_release();
        raw12_single_state=0;
        raw12_single_wait=0;
        return;
    }
    if(raw12_single_state>=2) return;
    if(raw12_single_state==0) {
        if(!raw12_single_geometry()) {
            if(++raw12_single_wait>=100) {
                raw12_single_state=4;
                ++raw12_single_rejected;
            }
            return;
        }
        raw12_single_allocation=RH_CALL(0x1034c1f1,void *,z62_u32,z62_u32)(raw12_single_frame_bytes+63u,0);
        if(!raw12_single_allocation) {
            raw12_single_state=4;
            ++raw12_single_rejected;
            return;
        }
        raw12_single_buffer=(unsigned char *)(((z62_u32)raw12_single_allocation+63u)&~63u);
        raw12_single_sequence=raw12_hook_notifications;
        raw12_single_state=1;
        return;
    }
    if(raw12_single_sequence==raw12_hook_notifications) return;
    z62_u32 sequence=raw12_hook_notifications;
    z62_u32 source=RH_WORD(0x20380310u);
    z62_u32 dimensions=RH_WORD(0x20380048u);
    if(!raw12_single_ram(source,raw12_single_frame_bytes) || dimensions!=(raw12_single_width<<16|raw12_single_height) ||
       (RH_WORD(0x2038030cu)&0xffffu)!=raw12_single_pitch) {
        raw12_single_state=4;
        ++raw12_single_rejected;
        return;
    }
    raw12_single_source=source;
    RH_CALL(0x1034bde9,void,void *,z62_u32)((unsigned char *)source,raw12_single_frame_bytes);
    RH_CALL(0x1035d254,void *,void *,const void *,z62_u32)(raw12_single_buffer,(unsigned char *)source,raw12_single_frame_bytes);
    if(sequence==raw12_hook_notifications && source==RH_WORD(0x20380310u)) {
        raw12_single_state=2;
        ++raw12_single_captured;
    } else {
        raw12_single_state=3;
        ++raw12_single_torn;
    }
}
#endif

static void raw12_hook_worker(void *ignored)
{
    (void)ignored;
    for(;;) {
        ++raw12_hook_worker_iterations;
        z62_u32 callback=RH_WORD(RH_MIPU_SLOT);
        if(callback!=(z62_u32)raw12_hook_event && raw12_hook_rom_function(callback)) {
            raw12_hook_native=callback;
            __asm__ volatile("dmb sy" ::: "memory");
            RH_WORD(RH_MIPU_SLOT)=(z62_u32)raw12_hook_event;
            ++raw12_hook_installs;
        }
#if Z62_RAW12_SINGLE_FRAME
        if(RH_WORD(RH_MIPU_SLOT)==(z62_u32)raw12_hook_event) raw12_single_step();
#endif
        RH_CALL(0x103dec95,void,z62_u32)(10);
    }
}

static z62_u32 raw12_hook_only_install(void)
{
    const unsigned char *original=(const unsigned char *)RH_CALL(0x103d655b,z62_u32,z62_u32)(0x10005);
    for(z62_u32 i=0;i<28;++i) raw12_hook_descriptor[i]=original[i];
    raw12_hook_descriptor[0]=1;
    *(z62_u32 *)(raw12_hook_descriptor+8)=8192;
    *(unsigned short *)(raw12_hook_descriptor+26)=0;
    RH_CALL(0x103debc7,int,void *,void (*)(void *),void *)(raw12_hook_task,raw12_hook_worker,raw12_hook_descriptor);
    if((int)raw12_hook_task[0]<=0) return 1;
    if(RH_CALL(0x1035c987,int,void *)(raw12_hook_task)) return 2;
    return 0;
}
#endif
