/* SPDX-License-Identifier: MIT */
#ifndef Z62_RAW12_HOOK_ONLY_H
#define Z62_RAW12_HOOK_ONLY_H
#include "z62.h"

#ifndef Z62_RAW12_SINGLE_FRAME
#define Z62_RAW12_SINGLE_FRAME 0
#endif
#ifndef Z62_RAW12_SINGLE_EXPORT
#define Z62_RAW12_SINGLE_EXPORT 0
#endif
#ifndef Z62_RAW12_DUAL_FRAME
#define Z62_RAW12_DUAL_FRAME 0
#endif
#ifndef Z62_RAW12_RAWOUT_SINGLE
#define Z62_RAW12_RAWOUT_SINGLE 0
#endif
#ifndef Z62_RAW12_DMA_SINGLE
#define Z62_RAW12_DMA_SINGLE 0
#endif
#ifndef Z62_RAW12_DMA_DUAL
#define Z62_RAW12_DMA_DUAL 0
#endif
#ifndef Z62_RAW12_DMA_BURST4
#define Z62_RAW12_DMA_BURST4 0
#endif
#ifndef Z62_RAW12_DMA_STREAM8
#define Z62_RAW12_DMA_STREAM8 0
#endif
#ifndef Z62_RAW12_DMA_STREAM16
#define Z62_RAW12_DMA_STREAM16 0
#endif
#if Z62_RAW12_DMA_STREAM16
static void raw12_probe_event(z62_u32 status);
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
static z62_u32 raw12_hook_frames;
static z62_u32 raw12_hook_last_code;
static z62_u32 raw12_hook_last_status;
static z62_u32 raw12_hook_last_movie;
static z62_u32 raw12_hook_worker_iterations;
#if Z62_RAW12_RAWOUT_SINGLE
static z62_u32 raw12_rawout_armed;
static z62_u32 raw12_rawout_done;
static z62_u32 raw12_rawout_ready_sequence;
static z62_u32 raw12_rawout_sequence;
static z62_u32 raw12_rawout_seen_ctl;
static z62_u32 raw12_rawout_seen_address;
static z62_u32 raw12_rawout_bad_row;
static z62_u32 raw12_rawout_bad_word;
#endif
#if Z62_RAW12_DMA_SINGLE || Z62_RAW12_DMA_DUAL || Z62_RAW12_DMA_BURST4 || Z62_RAW12_DMA_STREAM8 || Z62_RAW12_DMA_STREAM16
static z62_u32 raw12_dma_go;
static z62_u32 raw12_dma_stat;
static z62_u32 raw12_dma_bad_row;
static z62_u32 raw12_dma_bad_word;
#endif
#if Z62_RAW12_DMA_STREAM8 || Z62_RAW12_DMA_STREAM16
static z62_u32 raw12_stream_write_index;
#endif
#if Z62_RAW12_DMA_STREAM16
static z62_u32 raw12_stream_dropped;
#endif

static z62_u32 raw12_hook_rom_function(z62_u32 address)
{
    return (address&1u) && (address&~1u)>=0x10020000u && (address&~1u)<0x12520000u;
}

static z62_u32 raw12_hook_event(z62_u32 code,z62_u32 status)
{
    z62_u32 result=RH_CALL(raw12_hook_native,z62_u32,z62_u32,z62_u32)(code,status);
    ++raw12_hook_notifications;
    if(status&0x1000u) ++raw12_hook_frames;
#if Z62_RAW12_DMA_STREAM16
    if(RH_BYTE(RH_MOVIE_ACTIVE)==1) raw12_probe_event(status);
#endif
#if Z62_RAW12_RAWOUT_SINGLE
    if((status&0x1000u) && raw12_rawout_armed && (RH_WORD(0x203803e8u)&1u)) {
        raw12_rawout_seen_ctl=RH_WORD(0x203803e8u);
        raw12_rawout_seen_address=RH_WORD(0x203803ecu);
        RH_WORD(0x203803e8u)=raw12_rawout_seen_ctl&~1u;
        raw12_rawout_sequence=raw12_hook_frames;
        raw12_rawout_armed=0;
        __asm__ volatile("dmb sy" ::: "memory");
        raw12_rawout_done=1;
    }
#endif
    raw12_hook_last_code=code;
    raw12_hook_last_status=status;
    raw12_hook_last_movie=RH_BYTE(RH_MOVIE_ACTIVE);
    return result;
}

#if Z62_RAW12_SINGLE_FRAME
static unsigned char *raw12_single_allocation;
static unsigned char *raw12_single_buffer;
static unsigned char *raw12_dual_allocation;
static unsigned char *raw12_dual_buffer;
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
#if Z62_RAW12_SINGLE_EXPORT
static z62_u32 raw12_single_movie_width;
static z62_u32 raw12_single_movie_height;
static z62_u32 raw12_single_fps_code;
static z62_u32 raw12_single_fps_numerator;
static z62_u32 raw12_single_fps_denominator;
static z62_u32 raw12_single_export_wait;
static z62_u32 raw12_single_export_attempted;
static z62_u32 raw12_single_exported;
static z62_u32 raw12_single_export_errors;
static z62_u32 raw12_export_next_name;
static z62_u32 raw12_single_seen_recording;
static z62_u32 raw12_single_status_written;
static z62_u32 raw12_single_status_result;
static z62_u32 raw12_export_last_open;
static z62_u32 raw12_export_last_write;
static z62_u32 raw12_export_last_close;
static z62_u32 raw12_status_next_name;
static _Alignas(64) z62_u32 raw12_export_header[16];
static _Alignas(64) z62_u32 raw12_export_frame[4];
static z62_u32 raw12_dual_sequences[2];
static z62_u32 raw12_dual_sources[2];
static z62_u32 raw12_dual_retries;
static z62_u32 raw12_dual_error;
static z62_u32 raw12_dual_allocation_results[2];
static z62_u32 raw12_dual_allocation_attempts;
static z62_u32 raw12_dual_after_sequence;
static z62_u32 raw12_dual_after_source;
static z62_u32 raw12_dual_pool_status[2];
#endif

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
#if Z62_RAW12_SINGLE_EXPORT
    raw12_single_movie_width=movie_width;
    raw12_single_movie_height=movie_height;
    raw12_single_fps_code=fps;
    raw12_single_fps_denominator=(fps==23 || fps==29) ? 1001 : 1;
    raw12_single_fps_numerator=fps==23 ? 24000 : fps==29 ? 30000 : fps;
#endif
    return raw12_single_frame_bytes && raw12_single_frame_bytes<=(4u<<20);
}

static void raw12_single_release(void)
{
    if(raw12_single_allocation) RH_CALL(0x1034c23f,void,void *)(raw12_single_allocation);
    if(raw12_dual_allocation) RH_CALL(0x1034c23f,void,void *)(raw12_dual_allocation);
    raw12_single_allocation=0;
    raw12_single_buffer=0;
    raw12_dual_allocation=0;
    raw12_dual_buffer=0;
}

#if Z62_RAW12_SINGLE_EXPORT
static z62_u32 raw12_export_exists(const char *path)
{
    z62_u32 file[10];
    for(z62_u32 i=0;i<10;++i) ((volatile z62_u32 *)file)[i]=0;
    RH_CALL(0x104d67a5,void,z62_u32 *,const char *)(file,path);
    z62_u32 result=RH_CALL(0x104d67a9,z62_u32,z62_u32 *,z62_u32,z62_u32)(file,0,0);
    if(result!=0x043a0080u) return 0;
    RH_CALL(0x104d6835,z62_u32,z62_u32 *)(file);
    return 1;
}

static int raw12_export_open_unique(void)
{
    char path[16]={'B',':','\\','R','A','W','1','2','0','0','0','.','Z','1','2',0};
    z62_u32 start=raw12_export_next_name;
    for(z62_u32 n=0;n<1000;++n) {
        z62_u32 index=(start+n)%1000u;
        path[8]=(unsigned char)('0'+index/100);
        path[9]=(unsigned char)('0'+index/10%10);
        path[10]=(unsigned char)('0'+index%10);
        for(z62_u32 volume=0;volume<2;++volume) {
            path[0]=volume ? 'A' : 'B';
            if(raw12_export_exists(path)) continue;
            int file=RH_CALL(0x105c1897,int,const char *,z62_u32)(path,0);
            if(file<0) continue;
            raw12_export_next_name=index+1;
            return file;
        }
    }
    return -1;
}

static z62_u32 raw12_export_write(int file,void *buffer,z62_u32 size)
{
    z62_u32 offset=0;
    while(offset<size) {
        z62_u32 count=size-offset,actual=0;
        z62_u32 limit=65536u;
#if Z62_RAW12_DMA_STREAM16
        limit=262144u;
#endif
        if(count>limit) count=limit;
        struct { z62_u32 kind; void *buffer; z62_u32 capacity; } request={1,(unsigned char *)buffer+offset,count};
        RH_CALL(0x1034bdad,void,void *,z62_u32)(request.buffer,count);
        z62_u32 result=RH_CALL(0x105c17e5,z62_u32,int,void *,z62_u32,z62_u32 *)(file,&request,count,&actual);
        if(result || actual!=count) return result ? result : 1;
        offset+=count;
    }
    return 0;
}

static void raw12_dual_export_frames(void)
{
    int file=raw12_export_open_unique();
    raw12_export_last_open=(z62_u32)file;
    if(file<0) {
        ++raw12_single_export_errors;
        return;
    }
    for(z62_u32 i=0;i<16;++i) raw12_export_header[i]=0;
    raw12_export_header[0]=0x5236325au;
    raw12_export_header[1]=1;
    raw12_export_header[2]=64;
    raw12_export_header[3]=raw12_single_width;
    raw12_export_header[4]=raw12_single_height;
    raw12_export_header[5]=12;
    raw12_export_header[6]=raw12_single_pitch;
    raw12_export_header[7]=raw12_single_frame_bytes;
    raw12_export_header[8]=raw12_single_fps_numerator;
    raw12_export_header[9]=raw12_single_fps_denominator;
    raw12_export_header[10]=raw12_single_pitch;
    raw12_export_header[11]=raw12_single_movie_width;
    raw12_export_header[12]=raw12_single_movie_height;
    raw12_export_header[13]=raw12_single_fps_code;
    raw12_export_header[15]=~0x5236325au;
    z62_u32 result=raw12_export_write(file,raw12_export_header,64);
    for(z62_u32 i=0;i<2 && !result;++i) {
        raw12_export_frame[0]=0x46323152u;
        raw12_export_frame[1]=raw12_dual_sequences[i];
        raw12_export_frame[2]=raw12_single_frame_bytes;
        raw12_export_frame[3]=raw12_dual_sources[i];
        result=raw12_export_write(file,raw12_export_frame,16);
        if(!result)
            result=raw12_export_write(file,i ? raw12_dual_buffer : raw12_single_buffer,raw12_single_frame_bytes);
    }
    z62_u32 closed=RH_CALL(0x105c1749,z62_u32,int)(file);
    raw12_export_last_write=result;
    raw12_export_last_close=closed;
    if(result || closed) ++raw12_single_export_errors;
    else ++raw12_single_exported;
}

static void raw12_export_write_frame(void)
{
    if(Z62_RAW12_DUAL_FRAME || Z62_RAW12_DMA_DUAL) {
        raw12_dual_export_frames();
        return;
    }
    int file=raw12_export_open_unique();
    raw12_export_last_open=(z62_u32)file;
    if(file<0) {
        ++raw12_single_export_errors;
        return;
    }
    for(z62_u32 i=0;i<16;++i) raw12_export_header[i]=0;
    raw12_export_header[0]=0x5236325au;
    raw12_export_header[1]=1;
    raw12_export_header[2]=64;
    raw12_export_header[3]=raw12_single_width;
    raw12_export_header[4]=raw12_single_height;
    raw12_export_header[5]=12;
    raw12_export_header[6]=raw12_single_pitch;
    raw12_export_header[7]=raw12_single_frame_bytes;
    raw12_export_header[8]=raw12_single_fps_numerator;
    raw12_export_header[9]=raw12_single_fps_denominator;
    raw12_export_header[10]=raw12_single_pitch;
    raw12_export_header[11]=raw12_single_movie_width;
    raw12_export_header[12]=raw12_single_movie_height;
    raw12_export_header[13]=raw12_single_fps_code;
    raw12_export_header[15]=~0x5236325au;
    raw12_export_frame[0]=0x46323152u;
    raw12_export_frame[1]=raw12_single_sequence;
    raw12_export_frame[2]=raw12_single_frame_bytes;
    raw12_export_frame[3]=raw12_single_source;
    z62_u32 result=raw12_export_write(file,raw12_export_header,64);
    if(!result) result=raw12_export_write(file,raw12_export_frame,16);
    if(!result) result=raw12_export_write(file,raw12_single_buffer,raw12_single_frame_bytes);
    z62_u32 closed=RH_CALL(0x105c1749,z62_u32,int)(file);
    raw12_export_last_write=result;
    raw12_export_last_close=closed;
    if(result || closed) ++raw12_single_export_errors;
    else ++raw12_single_exported;
}
static int raw12_status_open_unique(z62_u32 probe)
{
    char path[16]={'B',':','\\','R','1','2','S','0','0','0','.','B','I','N',0,0};
    if(probe) path[3]='P';
    z62_u32 start=raw12_status_next_name;
    for(z62_u32 n=0;n<1000;++n) {
        z62_u32 index=(start+n)%1000u;
        path[7]=(unsigned char)('0'+index/100);
        path[8]=(unsigned char)('0'+index/10%10);
        path[9]=(unsigned char)('0'+index%10);
        for(z62_u32 volume=0;volume<2;++volume) {
            path[0]=volume ? 'A' : 'B';
            if(raw12_export_exists(path)) continue;
            int file=RH_CALL(0x105c1897,int,const char *,z62_u32)(path,0);
            if(file<0) continue;
            raw12_status_next_name=index+1;
            return file;
        }
    }
    return -1;
}

static z62_u32 raw12_write_status(void)
{
    _Alignas(64) z62_u32 data[32];
    for(z62_u32 i=0;i<32;++i) data[i]=0;
    data[0]=0x53323152u;
    data[1]=1;
    data[2]=raw12_single_state;
    data[3]=raw12_single_captured;
    data[4]=raw12_single_torn;
    data[5]=raw12_single_rejected;
    data[6]=raw12_single_width;
    data[7]=raw12_single_height;
    data[8]=raw12_single_pitch;
    data[9]=raw12_single_frame_bytes;
    data[10]=raw12_hook_notifications;
    data[11]=raw12_hook_installs;
    data[12]=raw12_hook_last_code;
    data[13]=raw12_hook_last_status;
    data[14]=raw12_hook_last_movie;
    data[15]=raw12_hook_worker_iterations;
    data[16]=(z62_u32)raw12_single_allocation;
    data[17]=(z62_u32)raw12_single_buffer;
    if(Z62_RAW12_DUAL_FRAME || Z62_RAW12_DMA_DUAL) {
        data[16]=raw12_dual_allocation_results[0];
        data[17]=raw12_dual_allocation_results[1];
    }
    data[18]=raw12_single_export_wait;
#if Z62_RAW12_DMA_STREAM8 || Z62_RAW12_DMA_STREAM16
    if(Z62_RAW12_DMA_STREAM8 || Z62_RAW12_DMA_STREAM16) data[18]=raw12_stream_write_index;
#endif
    data[19]=raw12_single_export_attempted;
    data[20]=raw12_single_exported;
    data[21]=raw12_single_export_errors;
    data[22]=raw12_export_last_open;
    if(Z62_RAW12_DUAL_FRAME || Z62_RAW12_RAWOUT_SINGLE || Z62_RAW12_DMA_SINGLE || Z62_RAW12_DMA_DUAL || Z62_RAW12_DMA_BURST4 || Z62_RAW12_DMA_STREAM8 || Z62_RAW12_DMA_STREAM16) data[22]=raw12_dual_error;
    data[23]=raw12_export_last_write;
    data[24]=raw12_export_last_close;
#if Z62_RAW12_RAWOUT_SINGLE
    if(Z62_RAW12_RAWOUT_SINGLE && raw12_single_state!=2) {
        data[23]=raw12_dual_error==0x2306 ? raw12_rawout_bad_row : raw12_rawout_seen_ctl;
        data[24]=raw12_dual_error==0x2306 ? raw12_rawout_bad_word : raw12_rawout_seen_address;
    }
#endif
#if Z62_RAW12_DMA_SINGLE || Z62_RAW12_DMA_DUAL || Z62_RAW12_DMA_BURST4 || Z62_RAW12_DMA_STREAM8 || Z62_RAW12_DMA_STREAM16
    if((Z62_RAW12_DMA_SINGLE || Z62_RAW12_DMA_DUAL || Z62_RAW12_DMA_BURST4 || Z62_RAW12_DMA_STREAM8 || Z62_RAW12_DMA_STREAM16) && raw12_single_state!=2) {
        z62_u32 sentinel=raw12_dual_error==0x2406 || raw12_dual_error==0x2408 || (raw12_dual_error&0xff00u)==0x2500u;
        data[23]=sentinel ? raw12_dma_bad_row : raw12_dma_go;
#if Z62_RAW12_DMA_STREAM16
        if(!sentinel) data[23]=raw12_stream_dropped;
#endif
        data[24]=sentinel ? raw12_dma_bad_word : raw12_dma_stat;
    }
#endif
#if Z62_RAW12_DMA_STREAM16
    if(Z62_RAW12_DMA_STREAM16 && raw12_single_state==2) data[23]=raw12_stream_dropped;
#endif
    if(Z62_RAW12_DUAL_FRAME && raw12_single_state==3) {
        data[23]=raw12_dual_after_sequence;
        data[24]=raw12_dual_after_source;
    } else if(Z62_RAW12_DUAL_FRAME && raw12_single_state!=2) {
        data[23]=raw12_dual_pool_status[0];
        data[24]=raw12_dual_pool_status[1];
    }
    data[25]=raw12_single_source;
    if(Z62_RAW12_DUAL_FRAME) data[25]=raw12_dual_retries|(raw12_dual_allocation_attempts<<16);
    if(Z62_RAW12_DMA_DUAL || Z62_RAW12_DMA_BURST4 || Z62_RAW12_DMA_STREAM8 || Z62_RAW12_DMA_STREAM16) data[25]=raw12_dual_allocation_attempts<<16;
    data[26]=raw12_single_sequence;
    data[27]=raw12_single_fps_code;
    data[28]=RH_BYTE(RH_MOVIE_ACTIVE);
    data[29]=Z62_TARGET;
    data[30]=~0x53323152u;
    data[31]=raw12_hook_frames;
    int file=raw12_status_open_unique(0);
    raw12_export_last_open=(z62_u32)file;
    if(file<0) return 1;
    z62_u32 result=raw12_export_write(file,data,sizeof(data));
    z62_u32 closed=RH_CALL(0x105c1749,z62_u32,int)(file);
    raw12_export_last_write=result;
    raw12_export_last_close=closed;
    return result ? result : closed;
}
#endif

static void raw12_dual_step(void)
{
    if(RH_BYTE(RH_MOVIE_ACTIVE)!=1) {
#if Z62_RAW12_SINGLE_EXPORT
        if(raw12_single_seen_recording && !raw12_single_status_written) {
            if(++raw12_single_export_wait<200) return;
            if(raw12_single_buffer && raw12_single_state==2 && !raw12_single_export_attempted) {
                raw12_single_export_attempted=1;
                raw12_export_write_frame();
            }
            raw12_single_status_result=raw12_write_status();
            raw12_single_status_written=1;
        }
#endif
        raw12_single_release();
        raw12_single_state=0;
        raw12_single_wait=0;
        return;
    }
    if(raw12_single_state>=2) return;
    if(raw12_single_state==0) {
#if Z62_RAW12_SINGLE_EXPORT
        raw12_single_seen_recording=1;
        raw12_single_status_written=0;
        raw12_single_export_wait=0;
        raw12_single_export_attempted=0;
#endif
        raw12_single_captured=0;
        raw12_single_torn=0;
        raw12_single_rejected=0;
        raw12_dual_retries=0;
        raw12_dual_error=0;
        raw12_dual_allocation_attempts=0;
        raw12_dual_after_sequence=0;
        raw12_dual_after_source=0;
        if(!raw12_single_geometry()) {
            if(++raw12_single_wait>=100) {
                raw12_dual_error=0x2101;
                raw12_single_state=4;
                ++raw12_single_rejected;
            }
            return;
        }
        ++raw12_dual_allocation_attempts;
        if(!raw12_single_allocation)
            raw12_single_allocation=RH_CALL(0x1034c1f1,void *,z62_u32,z62_u32)(raw12_single_frame_bytes+63u,0);
        if(!raw12_dual_allocation)
            raw12_dual_allocation=RH_CALL(0x1034c1f1,void *,z62_u32,z62_u32)(raw12_single_frame_bytes+63u,1);
        raw12_dual_allocation_results[0]=(z62_u32)raw12_single_allocation;
        raw12_dual_allocation_results[1]=(z62_u32)raw12_dual_allocation;
        if(!raw12_single_allocation || !raw12_dual_allocation) {
            if(++raw12_single_wait<100) return;
            raw12_dual_pool_status[0]=RH_CALL(0x1034c2b7,z62_u32,z62_u32)(0);
            raw12_dual_pool_status[1]=RH_CALL(0x1034c2d9,z62_u32,z62_u32)(0);
            raw12_dual_error=raw12_single_allocation ? 0x2112 : 0x2102;
            raw12_single_state=4;
            ++raw12_single_rejected;
            return;
        }
        raw12_single_buffer=(unsigned char *)(((z62_u32)raw12_single_allocation+63u)&~63u);
        raw12_dual_buffer=(unsigned char *)(((z62_u32)raw12_dual_allocation+63u)&~63u);
        raw12_single_wait=0;
        raw12_single_sequence=raw12_hook_frames;
        raw12_single_state=1;
        return;
    }
    if(raw12_single_sequence==raw12_hook_frames) return;
    z62_u32 sequence=raw12_hook_frames;
    z62_u32 source=RH_WORD(0x20380310u);
    z62_u32 dimensions=RH_WORD(0x20380048u);
    if(!raw12_single_ram(source,raw12_single_frame_bytes) || dimensions!=(raw12_single_width<<16|raw12_single_height) ||
       (RH_WORD(0x2038030cu)&0xffffu)!=raw12_single_pitch) {
        ++raw12_dual_retries;
        raw12_single_sequence=sequence;
        if(++raw12_single_wait>=100) {
            raw12_dual_error=0x2103;
            raw12_single_state=4;
            ++raw12_single_rejected;
        }
        return;
    }
    z62_u32 index=raw12_single_captured;
    if(index>=2) return;
    raw12_single_source=source;
    RH_CALL(0x1034bde9,void,void *,z62_u32)((unsigned char *)source,raw12_single_frame_bytes);
    RH_CALL(0x1035d254,void *,void *,const void *,z62_u32)(index ? raw12_dual_buffer : raw12_single_buffer,
        (unsigned char *)source,raw12_single_frame_bytes);
    raw12_dual_after_sequence=raw12_hook_frames;
    raw12_dual_after_source=RH_WORD(0x20380310u);
    if(sequence!=raw12_dual_after_sequence || source!=raw12_dual_after_source) {
        raw12_dual_error=sequence!=raw12_dual_after_sequence ? 0x2201 : 0x2202;
        raw12_single_state=3;
        ++raw12_single_torn;
        return;
    }
    if(index==1 && sequence!=raw12_dual_sequences[0]+1) {
        raw12_dual_error=0x2203;
        raw12_single_state=3;
        ++raw12_single_torn;
        return;
    }
    raw12_dual_sequences[index]=sequence;
    raw12_dual_sources[index]=source;
    raw12_single_sequence=sequence;
    ++raw12_single_captured;
    raw12_single_state=index+1;
}

#if Z62_RAW12_RAWOUT_SINGLE
static z62_u32 raw12_rawout_sentinel(z62_u32 row,z62_u32 word)
{
    return 0xa5000000u|((row&0xffffu)<<4)|word;
}

static void raw12_rawout_disarm(void)
{
    if(raw12_rawout_armed) {
        RH_WORD(0x203803e8u)=RH_WORD(0x203803e8u)&~1u;
        raw12_rawout_armed=0;
        __asm__ volatile("dmb sy" ::: "memory");
    }
}

static void raw12_rawout_complete(void)
{
    if(!raw12_rawout_done) return;
    raw12_rawout_done=0;
    if(!(raw12_rawout_seen_ctl&1u) || raw12_rawout_seen_address!=(z62_u32)raw12_single_buffer) {
        raw12_dual_error=0x2301;
    } else if(raw12_rawout_sequence!=raw12_single_sequence+1) {
        raw12_dual_error=0x2302;
    } else if(RH_WORD(0x20380048u)!=(raw12_single_width<<16|raw12_single_height) ||
              (RH_WORD(0x2038030cu)&0xffffu)!=raw12_single_pitch) {
        raw12_dual_error=0x2303;
    }
    if(raw12_dual_error) {
        raw12_single_state=4;
        ++raw12_single_rejected;
        return;
    }
    raw12_single_sequence=raw12_rawout_sequence;
    raw12_single_state=2;
    raw12_single_captured=1;
}

static void raw12_rawout_step(void)
{
    if(RH_BYTE(RH_MOVIE_ACTIVE)!=1) {
        raw12_rawout_disarm();
        raw12_rawout_ready_sequence=0;
        if(raw12_single_state==1) raw12_rawout_complete();
        if(raw12_single_seen_recording && !raw12_single_status_written) {
            if(++raw12_single_export_wait<200) return;
            if(raw12_single_buffer && raw12_single_state==2 && !raw12_single_export_attempted) {
                RH_CALL(0x1034bde9,void,void *,z62_u32)(raw12_single_buffer,raw12_single_frame_bytes);
                for(z62_u32 row=0;row<raw12_single_height && raw12_rawout_bad_row==~0u;++row)
                    for(z62_u32 word=0;word<4;++word)
                        if(RH_WORD((z62_u32)raw12_single_buffer+row*raw12_single_pitch+word*4)==raw12_rawout_sentinel(row,word)) {
                            raw12_rawout_bad_row=row;
                            raw12_rawout_bad_word=RH_WORD((z62_u32)raw12_single_buffer+row*raw12_single_pitch+word*4);
                            break;
                        }
                if(raw12_rawout_bad_row!=~0u) {
                    raw12_dual_error=0x2306;
                    raw12_single_state=4;
                    ++raw12_single_rejected;
                } else {
                    raw12_single_export_attempted=1;
                    raw12_export_write_frame();
                }
            }
            raw12_single_status_result=raw12_write_status();
            raw12_single_status_written=1;
        }
        raw12_single_release();
        raw12_single_state=0;
        raw12_single_wait=0;
        return;
    }
    if(raw12_single_state>=2) return;
    if(raw12_single_state==0) {
        raw12_single_seen_recording=1;
        raw12_single_status_written=0;
        raw12_single_export_wait=0;
        raw12_single_export_attempted=0;
        raw12_single_captured=0;
        raw12_single_torn=0;
        raw12_single_rejected=0;
        raw12_dual_error=0;
        if(!raw12_rawout_ready_sequence) raw12_rawout_ready_sequence=raw12_hook_frames+4;
        if(raw12_hook_frames<raw12_rawout_ready_sequence) return;
        if(!raw12_single_geometry()) {
            if(++raw12_single_wait>=100) {
                raw12_dual_error=0x2101;
                raw12_single_state=4;
                ++raw12_single_rejected;
            }
            return;
        }
        raw12_single_allocation=RH_CALL(0x1034c1f1,void *,z62_u32,z62_u32)(raw12_single_frame_bytes+63u,0);
        if(!raw12_single_allocation) {
            raw12_dual_error=0x2102;
            raw12_single_state=4;
            ++raw12_single_rejected;
            return;
        }
        raw12_single_buffer=(unsigned char *)(((z62_u32)raw12_single_allocation+63u)&~63u);
        raw12_single_source=RH_WORD(0x20380310u);
        if(!raw12_single_ram(raw12_single_source,raw12_single_frame_bytes) || (RH_WORD(0x203803e8u)&1u)) {
            raw12_dual_error=0x2304;
            raw12_single_state=4;
            ++raw12_single_rejected;
            return;
        }
        raw12_rawout_done=0;
        raw12_rawout_seen_ctl=0;
        raw12_rawout_seen_address=0;
        raw12_rawout_bad_row=~0u;
        raw12_rawout_bad_word=0;
        for(z62_u32 row=0;row<raw12_single_height;++row)
            for(z62_u32 word=0;word<4;++word)
                RH_WORD((z62_u32)raw12_single_buffer+row*raw12_single_pitch+word*4)=raw12_rawout_sentinel(row,word);
        RH_CALL(0x1034bdad,void,void *,z62_u32)(raw12_single_buffer,raw12_single_frame_bytes);
        RH_WORD(0x203803ecu)=(z62_u32)raw12_single_buffer;
        RH_WORD(0x203803f0u)=raw12_single_pitch;
        raw12_single_sequence=raw12_hook_frames;
        raw12_single_wait=0;
        raw12_single_state=1;
        raw12_rawout_armed=1;
        __asm__ volatile("dmb sy" ::: "memory");
        RH_WORD(0x203803e8u)=RH_WORD(0x203803e8u)|1u;
        return;
    }
    raw12_rawout_complete();
    if(raw12_single_state==1 && ++raw12_single_wait>=100) {
        raw12_rawout_disarm();
        raw12_dual_error=0x2305;
        raw12_single_state=4;
        ++raw12_single_rejected;
    }
}
#endif

#include "raw12_dma_single.h"
#include "raw12_dma_dual.h"
#include "raw12_dma_burst4.h"
#include "raw12_dma_stream8.h"

static void raw12_single_step(void)
{
#if Z62_RAW12_DMA_STREAM8 || Z62_RAW12_DMA_STREAM16
    raw12_dma_stream8_step();
    return;
#endif
#if Z62_RAW12_DMA_BURST4
    raw12_dma_burst4_step();
    return;
#endif
#if Z62_RAW12_DMA_DUAL
    raw12_dma_dual_step();
    return;
#endif
#if Z62_RAW12_DMA_SINGLE
    raw12_dma_single_step();
    return;
#endif
#if Z62_RAW12_RAWOUT_SINGLE
    raw12_rawout_step();
    return;
#endif
    if(Z62_RAW12_DUAL_FRAME) {
        raw12_dual_step();
        return;
    }
    if(RH_BYTE(RH_MOVIE_ACTIVE)!=1) {
#if Z62_RAW12_SINGLE_EXPORT
        if(raw12_single_seen_recording && !raw12_single_status_written) {
            if(++raw12_single_export_wait<200) return;
            if(raw12_single_buffer && raw12_single_state==2 && !raw12_single_export_attempted) {
                raw12_single_export_attempted=1;
                raw12_export_write_frame();
            }
            raw12_single_status_result=raw12_write_status();
            raw12_single_status_written=1;
        }
#endif
        raw12_single_release();
        raw12_single_state=0;
        raw12_single_wait=0;
        return;
    }
    if(raw12_single_state>=2) return;
    if(raw12_single_state==0) {
#if Z62_RAW12_SINGLE_EXPORT
        raw12_single_seen_recording=1;
        raw12_single_status_written=0;
        raw12_single_export_wait=0;
        raw12_single_export_attempted=0;
#endif
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
        raw12_single_sequence=raw12_hook_frames;
        raw12_single_state=1;
        return;
    }
    if(raw12_single_sequence==raw12_hook_frames) return;
    z62_u32 sequence=raw12_hook_frames;
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
    if(sequence==raw12_hook_frames && source==RH_WORD(0x20380310u)) {
        raw12_single_sequence=sequence;
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
        z62_u32 delay=10;
#if Z62_RAW12_DMA_SINGLE || Z62_RAW12_DMA_DUAL || Z62_RAW12_DMA_BURST4 || Z62_RAW12_DMA_STREAM8 || Z62_RAW12_DMA_STREAM16
        if(raw12_single_state==1) delay=1;
#endif
        RH_CALL(0x103dec95,void,z62_u32)(delay);
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
#if Z62_RAW12_DMA_STREAM8 || Z62_RAW12_DMA_STREAM16
    for(z62_u32 i=0;i<28;++i) raw12_stream_descriptor[i]=original[i];
    raw12_stream_descriptor[0]=1;
    *(z62_u32 *)(raw12_stream_descriptor+8)=8192;
    *(unsigned short *)(raw12_stream_descriptor+26)=0;
    RH_CALL(0x103debc7,int,void *,void (*)(void *),void *)(raw12_stream_task,raw12_stream_writer,raw12_stream_descriptor);
    if((int)raw12_stream_task[0]<=0) return 3;
    if(RH_CALL(0x1035c987,int,void *)(raw12_stream_task)) return 4;
#endif
    if(RH_CALL(0x1035c987,int,void *)(raw12_hook_task)) return 2;
    return 0;
}
#endif
