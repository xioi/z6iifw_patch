/* SPDX-License-Identifier: MIT */
#ifndef Z62_RAW12_HOOK_ONLY_H
#define Z62_RAW12_HOOK_ONLY_H
#include "z62.h"

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
