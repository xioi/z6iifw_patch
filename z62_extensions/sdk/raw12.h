/* SPDX-License-Identifier: MIT */
#ifndef Z62_RAW12_H
#define Z62_RAW12_H
#include "z62.h"
#include "menu.h"

#define R12_WORD(a) (*(volatile z62_u32 *)(a))
#define R12_BYTE(a) (*(volatile unsigned char *)(a))
#define R12_CALL(a,r,...) ((r (*)(__VA_ARGS__))(a))
#define R12_FS_OK 0x043a0080u
#define R12_MIPU_BASE 0x20380000u
#define R12_MIPU_SLOT 0xc1801ed4u
#define R12_H264_BASE 0x24070000u
#define R12_MOVIE_ACTIVE 0xc18117a8u
#define R12_FILE_MAGIC 0x5236325au
#define R12_FRAME_MAGIC 0x46323152u
#define R12_MAX_FRAME_BYTES (4u<<20)
#define R12_WRITE_CHUNK 65536u
#define R12_SLOTS 3u

static struct z62_menu_storage raw12_menu;
static z62_u32 raw12_task[8];
static unsigned char raw12_task_descriptor[28];
static z62_u32 raw12_writer_task[8];
static unsigned char raw12_writer_descriptor[28];
static z62_u32 raw12_mipu_native;
static z62_u32 raw12_enabled;
static z62_u32 raw12_irq_sequence;
static z62_u32 raw12_mipu_notifications;
static z62_u32 raw12_last_movie_state;
static z62_u32 raw12_last_sequence;
static z62_u32 raw12_session;
static z62_u32 raw12_rejected;
static z62_u32 raw12_geometry_failures;
static z62_u32 raw12_next_name;
static int raw12_file=-1;
static unsigned char *raw12_allocation;
static unsigned char *raw12_buffer;
static z62_u32 raw12_width;
static z62_u32 raw12_height;
static z62_u32 raw12_source_pitch;
static z62_u32 raw12_row_bytes;
static z62_u32 raw12_frame_bytes;
static z62_u32 raw12_frames;
static z62_u32 raw12_dropped;
static z62_u32 raw12_write_errors;
static z62_u32 raw12_sessions;
static z62_u32 raw12_slot_state[R12_SLOTS];
static z62_u32 raw12_slot_sequence[R12_SLOTS];
static z62_u32 raw12_slot_source[R12_SLOTS];
static z62_u32 raw12_pending;
static z62_u32 raw12_capture_slot;
static z62_u32 raw12_write_slot;
static z62_u32 raw12_writer_error;
static z62_u32 raw12_last_error;
static z62_u32 raw12_menu_installed;
static _Alignas(64) z62_u32 raw12_header[16];
static _Alignas(64) z62_u32 raw12_frame_header[4];

static z62_u32 raw12_load(const z62_u32 *p)
{
    return __atomic_load_n(p,__ATOMIC_ACQUIRE);
}

static void raw12_store(z62_u32 *p,z62_u32 value)
{
    __atomic_store_n(p,value,__ATOMIC_RELEASE);
}

static void raw12_delay(z62_u32 ms)
{
    R12_CALL(0x103dec95,void,z62_u32)(ms);
}

static void *raw12_allocate(z62_u32 size)
{
    return R12_CALL(0x1034c1f1,void *,z62_u32,z62_u32)(size,0);
}

static void raw12_release(void *p)
{
    R12_CALL(0x1034c23f,void,void *)(p);
}

static void raw12_clean(void *p,z62_u32 size)
{
    R12_CALL(0x1034bdad,void,void *,z62_u32)(p,size);
}

static void raw12_invalidate(void *p,z62_u32 size)
{
    R12_CALL(0x1034bde9,void,void *,z62_u32)(p,size);
}

static void raw12_copy(void *dst,const void *src,z62_u32 size)
{
    R12_CALL(0x1035d254,void *,void *,const void *,z62_u32)(dst,src,size);
}

static void raw12_publish_enabled(z62_u32 enabled)
{
    raw12_store(&raw12_enabled,enabled);
}

static z62_u32 raw12_menu_enable(z62_u32 a,z62_u32 b,z62_u32 c,z62_u32 d)
{
    (void)a; (void)b; (void)c; (void)d;
    raw12_publish_enabled(1);
    return 0;
}

static z62_u32 raw12_menu_disable(z62_u32 a,z62_u32 b,z62_u32 c,z62_u32 d)
{
    (void)a; (void)b; (void)c; (void)d;
    raw12_publish_enabled(0);
    return 0;
}

static z62_u32 raw12_exists(const char *path)
{
    z62_u32 file[10];
    for(z62_u32 i=0;i<10;++i) ((volatile z62_u32 *)file)[i]=0;
    R12_CALL(0x104d67a5,void,z62_u32 *,const char *)(file,path);
    z62_u32 result=R12_CALL(0x104d67a9,z62_u32,z62_u32 *,z62_u32,z62_u32)(file,0,0);
    if(result!=R12_FS_OK) return 0;
    R12_CALL(0x104d6835,z62_u32,z62_u32 *)(file);
    return 1;
}

static int raw12_open_unique(void)
{
    char path[16]={'B',':','\\','R','A','W','1','2','0','0','0','.','Z','1','2',0};
    z62_u32 start=raw12_next_name;
    for(z62_u32 n=0;n<1000;++n) {
        z62_u32 index=(start+n)%1000u;
        path[8]=(unsigned char)('0'+index/100);
        path[9]=(unsigned char)('0'+index/10%10);
        path[10]=(unsigned char)('0'+index%10);
        for(z62_u32 volume=0;volume<2;++volume) {
            path[0]=volume ? 'A' : 'B';
            if(raw12_exists(path)) continue;
            int file=R12_CALL(0x105c1897,int,const char *,z62_u32)(path,0);
            if(file<0) continue;
            raw12_next_name=index+1;
            return file;
        }
    }
    return -1;
}

static z62_u32 raw12_write_all(int file,void *buffer,z62_u32 size)
{
    z62_u32 offset=0;
    while(offset<size) {
        z62_u32 count=size-offset,actual=0;
        if(count>R12_WRITE_CHUNK) count=R12_WRITE_CHUNK;
        struct { z62_u32 kind; void *buffer; z62_u32 capacity; } request={1,(unsigned char *)buffer+offset,count};
        raw12_clean(request.buffer,count);
        z62_u32 result=R12_CALL(0x105c17e5,z62_u32,int,void *,z62_u32,z62_u32 *)(file,&request,count,&actual);
        if(result || actual!=count) return result ? result : 1;
        offset+=count;
    }
    return 0;
}

static z62_u32 raw12_fps(z62_u32 code,z62_u32 *numerator,z62_u32 *denominator)
{
    *denominator=1;
    if(code==23) { *numerator=24000; *denominator=1001; return 1; }
    if(code==25) { *numerator=25; return 1; }
    if(code==29) { *numerator=30000; *denominator=1001; return 1; }
    if(code==30) { *numerator=30; return 1; }
    return 0;
}

static z62_u32 raw12_ram(z62_u32 address,z62_u32 size)
{
    return address>=0x80000000u && size && address+size>=address;
}

static z62_u32 raw12_geometry(z62_u32 *fps_numerator,z62_u32 *fps_denominator)
{
    z62_u32 movie_width=R12_WORD(R12_H264_BASE+0xe00)&0x7fffu;
    z62_u32 movie_height=R12_WORD(R12_H264_BASE+0xe04)&0x1fffu;
    z62_u32 fps_code=R12_WORD(R12_H264_BASE+0xe0c)&0xffffu;
    if(movie_width<16 || movie_width>1920 || movie_height<16 || movie_height>1080 ||
       !raw12_fps(fps_code,fps_numerator,fps_denominator)) return 0;
    if((R12_WORD(R12_MIPU_BASE+0x300)&255u)!=2 || (R12_WORD(R12_MIPU_BASE+0xa0)&2u)==0) return 0;
    z62_u32 dimensions=R12_WORD(R12_MIPU_BASE+0x48);
    raw12_width=dimensions>>16;
    raw12_height=dimensions&0xffffu;
    raw12_source_pitch=R12_WORD(R12_MIPU_BASE+0x30c)&0xffffu;
    if(raw12_width< movie_width || raw12_width>2100 || raw12_height<movie_height || raw12_height>1200 ||
       (raw12_width&1u) || raw12_height<2) return 0;
    raw12_row_bytes=(raw12_width*12u+7u)>>3;
    if(raw12_source_pitch<raw12_row_bytes || raw12_source_pitch>8192) return 0;
    raw12_row_bytes=raw12_source_pitch;
    raw12_frame_bytes=raw12_row_bytes*raw12_height;
    if(!raw12_frame_bytes || raw12_frame_bytes>R12_MAX_FRAME_BYTES) return 0;
    z62_u32 source=R12_WORD(R12_MIPU_BASE+0x310);
    z62_u32 span=raw12_frame_bytes;
    if(!raw12_ram(source,span)) return 0;
    raw12_header[11]=movie_width;
    raw12_header[12]=movie_height;
    raw12_header[13]=fps_code;
    return 1;
}

static void raw12_close_session(void)
{
    if(raw12_file>=0) {
        z62_u32 result=R12_CALL(0x105c1749,z62_u32,int)(raw12_file);
        if(result && !raw12_last_error) raw12_last_error=result;
    }
    raw12_file=-1;
    if(raw12_allocation) raw12_release(raw12_allocation);
    raw12_allocation=0;
    raw12_buffer=0;
    raw12_session=0;
    raw12_pending=0;
    for(z62_u32 i=0;i<R12_SLOTS;++i) raw12_slot_state[i]=0;
}

static z62_u32 raw12_open_session(void)
{
    z62_u32 numerator=0,denominator=0;
    if(!raw12_geometry(&numerator,&denominator)) {
        raw12_last_error=0x1001;
        return 0;
    }
    raw12_allocation=raw12_allocate(raw12_frame_bytes*R12_SLOTS+63u);
    if(!raw12_allocation) {
        raw12_last_error=0x1002;
        return 0;
    }
    raw12_buffer=(unsigned char *)(((z62_u32)raw12_allocation+63u)&~63u);
    raw12_file=raw12_open_unique();
    if(raw12_file<0) {
        raw12_last_error=0x1003;
        raw12_close_session();
        return 0;
    }
    raw12_header[0]=R12_FILE_MAGIC;
    raw12_header[1]=1;
    raw12_header[2]=64;
    raw12_header[3]=raw12_width;
    raw12_header[4]=raw12_height;
    raw12_header[5]=12;
    raw12_header[6]=raw12_row_bytes;
    raw12_header[7]=raw12_frame_bytes;
    raw12_header[8]=numerator;
    raw12_header[9]=denominator;
    raw12_header[10]=raw12_source_pitch;
    raw12_header[15]=~R12_FILE_MAGIC;
    z62_u32 result=raw12_write_all(raw12_file,raw12_header,64);
    if(result) {
        raw12_last_error=result;
        ++raw12_write_errors;
        raw12_close_session();
        return 0;
    }
    raw12_pending=0;
    raw12_capture_slot=0;
    raw12_write_slot=0;
    raw12_writer_error=0;
    for(z62_u32 i=0;i<R12_SLOTS;++i) raw12_slot_state[i]=0;
    raw12_frames=0;
    raw12_dropped=0;
    raw12_last_sequence=raw12_load(&raw12_irq_sequence);
    raw12_session=1;
    ++raw12_sessions;
    return 1;
}

static z62_u32 raw12_source_valid(z62_u32 source)
{
    z62_u32 span=raw12_frame_bytes;
    z62_u32 dimensions=R12_WORD(R12_MIPU_BASE+0x48);
    return raw12_ram(source,span) && dimensions==(raw12_width<<16|raw12_height) &&
        (R12_WORD(R12_MIPU_BASE+0x300)&255u)==2 &&
        (R12_WORD(R12_MIPU_BASE+0x30c)&0xffffu)==raw12_source_pitch;
}

static void raw12_capture(z62_u32 sequence)
{
    if(sequence!=raw12_last_sequence+1 && sequence>raw12_last_sequence)
        raw12_dropped+=sequence-raw12_last_sequence-1;
    z62_u32 slot=raw12_capture_slot;
    z62_u32 found=0;
    for(z62_u32 n=0;n<R12_SLOTS;++n) {
        slot=(raw12_capture_slot+n)%R12_SLOTS;
        if(raw12_load(&raw12_slot_state[slot])==0) { found=1; break; }
    }
    if(!found) {
        ++raw12_dropped;
        raw12_last_sequence=sequence;
        return;
    }
    raw12_store(&raw12_slot_state[slot],2);
    z62_u32 source=R12_WORD(R12_MIPU_BASE+0x310);
    if(!raw12_source_valid(source)) {
        raw12_store(&raw12_slot_state[slot],0);
        raw12_last_error=0x1004;
        raw12_rejected=1;
        return;
    }
    unsigned char *destination=raw12_buffer+slot*raw12_frame_bytes;
    raw12_invalidate((unsigned char *)source,raw12_frame_bytes);
    raw12_copy(destination,(unsigned char *)source,raw12_frame_bytes);
    z62_u32 after=raw12_load(&raw12_irq_sequence);
    if(after!=sequence || source!=R12_WORD(R12_MIPU_BASE+0x310)) {
        raw12_store(&raw12_slot_state[slot],0);
        ++raw12_dropped;
        raw12_last_sequence=after ? after-1 : 0;
        return;
    }
    raw12_slot_sequence[slot]=sequence;
    raw12_slot_source[slot]=source;
    raw12_capture_slot=(slot+1)%R12_SLOTS;
    raw12_last_sequence=sequence;
    __atomic_add_fetch(&raw12_pending,1,__ATOMIC_ACQ_REL);
    raw12_store(&raw12_slot_state[slot],1);
}

static void raw12_writer_worker(void *ignored)
{
    (void)ignored;
    for(;;) {
        if(!raw12_session || !raw12_load(&raw12_pending)) {
            raw12_delay(2);
            continue;
        }
        z62_u32 slot=raw12_write_slot,found=0;
        for(z62_u32 n=0;n<R12_SLOTS;++n) {
            slot=(raw12_write_slot+n)%R12_SLOTS;
            if(raw12_load(&raw12_slot_state[slot])==1) { found=1; break; }
        }
        if(!found) {
            raw12_delay(1);
            continue;
        }
        z62_u32 result=0;
        if(!raw12_writer_error) {
            raw12_frame_header[0]=R12_FRAME_MAGIC;
            raw12_frame_header[1]=raw12_slot_sequence[slot];
            raw12_frame_header[2]=raw12_frame_bytes;
            raw12_frame_header[3]=raw12_slot_source[slot];
            result=raw12_write_all(raw12_file,raw12_frame_header,16);
            if(!result)
                result=raw12_write_all(raw12_file,raw12_buffer+slot*raw12_frame_bytes,raw12_frame_bytes);
            if(result) {
                raw12_last_error=result;
                ++raw12_write_errors;
                raw12_writer_error=1;
                raw12_publish_enabled(0);
            } else ++raw12_frames;
        }
        raw12_write_slot=(slot+1)%R12_SLOTS;
        raw12_store(&raw12_slot_state[slot],0);
        __atomic_sub_fetch(&raw12_pending,1,__ATOMIC_ACQ_REL);
    }
}

static z62_u32 raw12_rom_function(z62_u32 address)
{
    return (address&1u) && (address&~1u)>=0x10020000u && (address&~1u)<0x12520000u;
}

static z62_u32 raw12_mipu_event(z62_u32 code,z62_u32 status);

static void raw12_worker(void *ignored)
{
    (void)ignored;
    for(;;) {
        if(!raw12_menu_installed) {
            const struct z62_menu_row rows[2]={
                {"RAW12 video: enable",raw12_menu_enable,0xd7},
                {"RAW12 video: disable",raw12_menu_disable,0xd7}
            };
            if(!z62_menu_prepare_rows(&raw12_menu,rows,2)) {
                z62_menu_publish_rows(&raw12_menu,2);
                raw12_menu_installed=1;
            }
        }
        z62_u32 callback=R12_WORD(R12_MIPU_SLOT);
        if(callback!=(z62_u32)raw12_mipu_event && raw12_rom_function(callback)) {
            raw12_mipu_native=callback;
            __asm__ volatile("dmb sy" ::: "memory");
            R12_WORD(R12_MIPU_SLOT)=(z62_u32)raw12_mipu_event;
        }
        if(R12_WORD(R12_MIPU_SLOT)!=(z62_u32)raw12_mipu_event) {
            raw12_delay(5);
            continue;
        }
        z62_u32 active=raw12_load(&raw12_enabled) && R12_BYTE(R12_MOVIE_ACTIVE)==1;
        if(!active) {
            if(raw12_load(&raw12_pending)) {
                raw12_delay(1);
                continue;
            }
            raw12_close_session();
            raw12_rejected=0;
            raw12_geometry_failures=0;
            raw12_delay(5);
            continue;
        }
        if(raw12_rejected) {
            if(raw12_load(&raw12_pending)) {
                raw12_delay(2);
                continue;
            }
            raw12_close_session();
            raw12_delay(20);
            continue;
        }
        if(!raw12_session && !raw12_open_session()) {
            if(++raw12_geometry_failures>=20) raw12_rejected=1;
            else raw12_delay(50);
            continue;
        }
        raw12_geometry_failures=0;
        z62_u32 sequence=raw12_load(&raw12_irq_sequence);
        if(raw12_session && sequence!=raw12_last_sequence) raw12_capture(sequence);
        else raw12_delay(1);
    }
}

static z62_u32 raw12_mipu_event(z62_u32 code,z62_u32 status)
{
    z62_u32 result=R12_CALL(raw12_mipu_native,z62_u32,z62_u32,z62_u32)(code,status);
    ++raw12_mipu_notifications;
    raw12_last_movie_state=R12_BYTE(R12_MOVIE_ACTIVE);
    if(raw12_load(&raw12_enabled) && R12_BYTE(R12_MOVIE_ACTIVE)==1)
        raw12_store(&raw12_irq_sequence,raw12_irq_sequence+1);
    return result;
}

static z62_u32 raw12_install(void)
{
    const unsigned char *original=(const unsigned char *)R12_CALL(0x103d655b,z62_u32,z62_u32)(0x10005);
    for(z62_u32 i=0;i<28;++i) {
        raw12_task_descriptor[i]=original[i];
        raw12_writer_descriptor[i]=original[i];
    }
    raw12_task_descriptor[0]=1;
    raw12_writer_descriptor[0]=1;
    *(z62_u32 *)(raw12_task_descriptor+8)=16384;
    *(z62_u32 *)(raw12_writer_descriptor+8)=16384;
    *(unsigned short *)(raw12_task_descriptor+26)=0;
    *(unsigned short *)(raw12_writer_descriptor+26)=0;
    R12_CALL(0x103debc7,int,void *,void (*)(void *),void *)(raw12_task,raw12_worker,raw12_task_descriptor);
    R12_CALL(0x103debc7,int,void *,void (*)(void *),void *)(raw12_writer_task,raw12_writer_worker,raw12_writer_descriptor);
    if((int)raw12_task[0]<=0 || (int)raw12_writer_task[0]<=0) return 3;
    if(R12_CALL(0x1035c987,int,void *)(raw12_writer_task)) return 4;
    if(R12_CALL(0x1035c987,int,void *)(raw12_task)) {
        raw12_last_error=0x2001;
        return 0;
    }
    return 0;
}
#endif
