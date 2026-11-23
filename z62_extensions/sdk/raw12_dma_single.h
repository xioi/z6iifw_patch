/* SPDX-License-Identifier: MIT */
#ifndef Z62_RAW12_DMA_SINGLE_H
#define Z62_RAW12_DMA_SINGLE_H

#if Z62_RAW12_DMA_SINGLE || Z62_RAW12_DMA_DUAL || Z62_RAW12_DMA_BURST4 || Z62_RAW12_DMA_STREAM8 || Z62_RAW12_DMA_STREAM16
#define RD_XDACS 0x04251000u
#define RD_CHANNEL (0x04251000u+3u*0x30u)
#define RD_LEN (RD_CHANNEL+0x10u)
#define RD_SRC (RD_CHANNEL+0x14u)
#define RD_DST (RD_CHANNEL+0x18u)
#define RD_CONFIG0 (RD_CHANNEL+0x1cu)
#define RD_CONFIG1 (RD_CHANNEL+0x20u)
#define RD_CONFIG2 (RD_CHANNEL+0x24u)
#define RD_GO (RD_CHANNEL+0x28u)
#define RD_STAT (RD_CHANNEL+0x30u)
#define RD_REGISTRATION 0xc180675cu
#define RD_RESULT (RD_REGISTRATION+0x18u)

static z62_u32 raw12_dma_active;
static z62_u32 raw12_dma_ready_sequence;
static z62_u32 raw12_dma_saved[7];
static unsigned char *raw12_dma_destination;

static z62_u32 raw12_dma_sentinel(z62_u32 row,z62_u32 word)
{
    return 0xa5000000u|((row&0xffffu)<<4)|word;
}

static void raw12_dma_restore(void)
{
    if(!raw12_dma_active) return;
    raw12_dma_go=RH_WORD(RD_GO);
    raw12_dma_stat=RH_WORD(RD_STAT);
    RH_WORD(RD_GO)=0;
    RH_WORD(RD_LEN)=raw12_dma_saved[0];
    RH_WORD(RD_SRC)=raw12_dma_saved[1];
    RH_WORD(RD_DST)=raw12_dma_saved[2];
    RH_WORD(RD_GO)=raw12_dma_saved[3];
    RH_WORD(RD_CONFIG0)=raw12_dma_saved[4];
    RH_WORD(RD_CONFIG1)=raw12_dma_saved[5];
    RH_WORD(RD_CONFIG2)=raw12_dma_saved[6];
    RH_WORD(RD_STAT)=0;
    raw12_dma_active=0;
}

static void raw12_dma_reject(z62_u32 error)
{
    raw12_dual_error=error;
    raw12_single_state=4;
    ++raw12_single_rejected;
}

static void raw12_dma_capture(void)
{
    z62_u32 sequence=raw12_hook_frames;
    z62_u32 source=RH_WORD(0x20380310u);
    raw12_single_source=source;
    if(!raw12_single_ram(source,raw12_single_frame_bytes) ||
       RH_WORD(0x20380048u)!=(raw12_single_width<<16|raw12_single_height) ||
       (RH_WORD(0x2038030cu)&0xffffu)!=raw12_single_pitch) {
        raw12_dma_reject(0x2404);
        return;
    }
    if(sequence!=raw12_single_sequence+1) {
#if Z62_RAW12_DMA_STREAM16
        if(sequence>raw12_single_sequence) {
            raw12_stream_dropped+=sequence-raw12_single_sequence-1;
            raw12_single_sequence=sequence-1;
        } else
#endif
        {
            raw12_dma_reject(0x2405);
            return;
        }
    }
    raw12_dma_go=RH_WORD(RD_GO);
    raw12_dma_stat=RH_WORD(RD_STAT);
    if(raw12_dma_go || !(RH_WORD(RD_XDACS)&0x10000000u)) {
        raw12_dma_reject(0x2401);
        return;
    }
    if((RH_WORD(RD_REGISTRATION)&0xffffu)!=0x13du || RH_WORD(RD_REGISTRATION+0xcu)!=0x10295fa7u) {
        raw12_dma_reject(0x2407);
        return;
    }
    raw12_dma_saved[0]=RH_WORD(RD_LEN);
    raw12_dma_saved[1]=RH_WORD(RD_SRC);
    raw12_dma_saved[2]=RH_WORD(RD_DST);
    raw12_dma_saved[3]=raw12_dma_go;
    raw12_dma_saved[4]=RH_WORD(RD_CONFIG0);
    raw12_dma_saved[5]=RH_WORD(RD_CONFIG1);
    raw12_dma_saved[6]=RH_WORD(RD_CONFIG2);
    raw12_dma_active=1;
    z62_u32 half=raw12_single_frame_bytes/2u;
    RH_WORD(RD_CONFIG0)=0x30f00u;
    RH_WORD(RD_CONFIG1)=0x30f00u;
    RH_WORD(RD_CONFIG2)=0;
    RH_WORD(RD_LEN)=half-1u;
    RH_WORD(RD_SRC)=source+half;
    RH_WORD(RD_DST)=(z62_u32)raw12_dma_destination+half;
    RH_WORD(RD_STAT)=0;
    RH_BYTE(RD_RESULT)=0;
    RH_WORD(RD_GO)=0x11109003u;
    RH_CALL(0x1034bde9,void,void *,z62_u32)((unsigned char *)source,half);
    RH_CALL(0x1035d254,void *,void *,const void *,z62_u32)(raw12_dma_destination,(const void *)source,half);
    z62_u32 error=0;
    if(sequence!=raw12_hook_frames || source!=RH_WORD(0x20380310u)) {
        error=sequence!=raw12_hook_frames ? 0x2211 : 0x2221;
    }
    for(z62_u32 wait=0;!error && (RH_WORD(RD_GO)&0x10000000u);++wait) {
        if(wait>=100 || sequence!=raw12_hook_frames || source!=RH_WORD(0x20380310u) || RH_BYTE(RH_MOVIE_ACTIVE)!=1) {
            error=sequence!=raw12_hook_frames ? 0x2212 : source!=RH_WORD(0x20380310u) ? 0x2222 : 0x2403;
            break;
        }
        RH_CALL(0x103dec95,void,z62_u32)(1);
    }
    raw12_dma_go=RH_WORD(RD_GO);
    raw12_dma_stat=RH_WORD(RD_STAT);
    if(!error && ((raw12_dma_go&0x10000000u) || (!(raw12_dma_stat&8u) && !(RH_BYTE(RD_RESULT)&8u)))) error=0x2402;
    if(!error && (sequence!=raw12_hook_frames || source!=RH_WORD(0x20380310u)))
        error=sequence!=raw12_hook_frames ? 0x2213 : 0x2223;
    raw12_dma_restore();
    if(error) {
        raw12_dual_error=error;
        if((error&0xff00u)==0x2200u) {
            raw12_single_state=3;
            ++raw12_single_torn;
        } else {
            raw12_single_state=4;
            ++raw12_single_rejected;
        }
        return;
    }
    raw12_single_sequence=sequence;
#if Z62_RAW12_DMA_SINGLE
    raw12_single_captured=1;
    raw12_single_state=2;
#endif
}

#if Z62_RAW12_DMA_SINGLE
static void raw12_dma_single_step(void)
{
    if(RH_BYTE(RH_MOVIE_ACTIVE)!=1) {
        raw12_dma_restore();
        raw12_dma_ready_sequence=0;
        if(raw12_single_seen_recording && !raw12_single_status_written) {
            if(++raw12_single_export_wait<200) return;
            if(raw12_single_buffer && raw12_single_state==2 && !raw12_single_export_attempted) {
                RH_CALL(0x1034bde9,void,void *,z62_u32)(raw12_single_buffer,raw12_single_frame_bytes);
                raw12_dma_bad_row=~0u;
                for(z62_u32 row=0;row<raw12_single_height && raw12_dma_bad_row==~0u;++row)
                    for(z62_u32 word=0;word<4;++word)
                        if(RH_WORD((z62_u32)raw12_single_buffer+row*raw12_single_pitch+word*4)==raw12_dma_sentinel(row,word)) {
                            raw12_dma_bad_row=row;
                            raw12_dma_bad_word=RH_WORD((z62_u32)raw12_single_buffer+row*raw12_single_pitch+word*4);
                            break;
                        }
                if(raw12_dma_bad_row!=~0u) {
                    raw12_dma_reject(0x2406);
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
        if(!raw12_dma_ready_sequence) raw12_dma_ready_sequence=raw12_hook_frames+4;
        if(raw12_hook_frames<raw12_dma_ready_sequence) return;
        if(!raw12_single_geometry()) {
            if(++raw12_single_wait>=100) raw12_dma_reject(0x2101);
            return;
        }
        raw12_single_allocation=RH_CALL(0x1034c1f1,void *,z62_u32,z62_u32)(raw12_single_frame_bytes+63u,0);
        if(!raw12_single_allocation) {
            raw12_dma_reject(0x2102);
            return;
        }
        raw12_single_buffer=(unsigned char *)(((z62_u32)raw12_single_allocation+63u)&~63u);
        raw12_dma_destination=raw12_single_buffer;
        for(z62_u32 row=0;row<raw12_single_height;++row)
            for(z62_u32 word=0;word<4;++word)
                RH_WORD((z62_u32)raw12_single_buffer+row*raw12_single_pitch+word*4)=raw12_dma_sentinel(row,word);
        RH_CALL(0x1034bdad,void,void *,z62_u32)(raw12_single_buffer,raw12_single_frame_bytes);
        raw12_single_sequence=raw12_hook_frames;
        raw12_single_state=1;
        raw12_single_wait=0;
        return;
    }
    if(raw12_single_sequence!=raw12_hook_frames) raw12_dma_capture();
}
#endif
#endif
#endif
