/* SPDX-License-Identifier: MIT */
#ifndef Z62_RAW12_DMA_BURST4_H
#define Z62_RAW12_DMA_BURST4_H

#if Z62_RAW12_DMA_BURST4
static unsigned char *raw12_burst_allocations[2];
static unsigned char *raw12_burst_buffers[4];
static z62_u32 raw12_burst_sequences[4];
static z62_u32 raw12_burst_sources[4];

static void raw12_burst_release(void)
{
    for(z62_u32 i=0;i<2;++i) {
        if(raw12_burst_allocations[i]) RH_CALL(0x1034c23f,void,void *)(raw12_burst_allocations[i]);
        raw12_burst_allocations[i]=0;
    }
    for(z62_u32 i=0;i<4;++i) raw12_burst_buffers[i]=0;
    raw12_single_release();
}

static z62_u32 raw12_burst_verify(unsigned char *buffer)
{
    RH_CALL(0x1034bde9,void,void *,z62_u32)(buffer,raw12_single_frame_bytes);
    raw12_dma_bad_row=~0u;
    for(z62_u32 row=0;row<raw12_single_height && raw12_dma_bad_row==~0u;++row)
        for(z62_u32 word=0;word<4;++word)
            if(RH_WORD((z62_u32)buffer+row*raw12_single_pitch+word*4)==raw12_dma_sentinel(row,word)) {
                raw12_dma_bad_row=row;
                raw12_dma_bad_word=RH_WORD((z62_u32)buffer+row*raw12_single_pitch+word*4);
                break;
            }
    return raw12_dma_bad_row==~0u;
}

static void raw12_burst_export(void)
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
    for(z62_u32 i=0;i<4 && !result;++i) {
        raw12_export_frame[0]=0x46323152u;
        raw12_export_frame[1]=raw12_burst_sequences[i];
        raw12_export_frame[2]=raw12_single_frame_bytes;
        raw12_export_frame[3]=raw12_burst_sources[i];
        result=raw12_export_write(file,raw12_export_frame,16);
        if(!result) result=raw12_export_write(file,raw12_burst_buffers[i],raw12_single_frame_bytes);
    }
    z62_u32 closed=RH_CALL(0x105c1749,z62_u32,int)(file);
    raw12_export_last_write=result;
    raw12_export_last_close=closed;
    if(result || closed) ++raw12_single_export_errors;
    else ++raw12_single_exported;
}

static void raw12_dma_burst4_step(void)
{
    if(RH_BYTE(RH_MOVIE_ACTIVE)!=1) {
        raw12_dma_restore();
        raw12_dma_ready_sequence=0;
        if(raw12_single_seen_recording && !raw12_single_status_written) {
            if(++raw12_single_export_wait<200) return;
            if(raw12_single_state==2 && !raw12_single_export_attempted) {
                z62_u32 frame=0;
                for(;frame<4;++frame) {
                    if(!raw12_burst_verify(raw12_burst_buffers[frame])) {
                        raw12_dma_reject(0x2500u|frame);
                        break;
                    }
                }
                if(frame==4) {
                    raw12_single_export_attempted=1;
                    raw12_burst_export();
                }
            }
            raw12_single_status_result=raw12_write_status();
            raw12_single_status_written=1;
        }
        raw12_burst_release();
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
        raw12_dual_allocation_attempts=0;
        if(!raw12_dma_ready_sequence) raw12_dma_ready_sequence=raw12_hook_frames+4;
        if(raw12_hook_frames<raw12_dma_ready_sequence) return;
        if(!raw12_single_geometry()) {
            if(++raw12_single_wait>=100) raw12_dma_reject(0x2101);
            return;
        }
        raw12_single_wait=0;
        ++raw12_dual_allocation_attempts;
        if(!raw12_single_allocation)
            raw12_single_allocation=RH_CALL(0x1034c1f1,void *,z62_u32,z62_u32)(raw12_single_frame_bytes+63u,0);
        if(!raw12_dual_allocation)
            raw12_dual_allocation=RH_CALL(0x1034c1f1,void *,z62_u32,z62_u32)(raw12_single_frame_bytes+63u,1);
        for(z62_u32 i=0;i<2;++i) {
            if(!raw12_burst_allocations[i])
                raw12_burst_allocations[i]=RH_CALL(0x1034c1f1,void *,z62_u32,z62_u32)(raw12_single_frame_bytes+63u,i);
        }
        if(!raw12_single_allocation || !raw12_dual_allocation || !raw12_burst_allocations[0] || !raw12_burst_allocations[1]) {
            if(++raw12_single_wait<100) return;
            raw12_dma_reject(0x2102);
            return;
        }
        raw12_burst_buffers[0]=(unsigned char *)(((z62_u32)raw12_single_allocation+63u)&~63u);
        raw12_burst_buffers[1]=(unsigned char *)(((z62_u32)raw12_dual_allocation+63u)&~63u);
        raw12_burst_buffers[2]=(unsigned char *)(((z62_u32)raw12_burst_allocations[0]+63u)&~63u);
        raw12_burst_buffers[3]=(unsigned char *)(((z62_u32)raw12_burst_allocations[1]+63u)&~63u);
        for(z62_u32 frame=0;frame<4;++frame) {
            unsigned char *buffer=raw12_burst_buffers[frame];
            for(z62_u32 row=0;row<raw12_single_height;++row)
                for(z62_u32 word=0;word<4;++word)
                    RH_WORD((z62_u32)buffer+row*raw12_single_pitch+word*4)=raw12_dma_sentinel(row,word);
            RH_CALL(0x1034bdad,void,void *,z62_u32)(buffer,raw12_single_frame_bytes);
        }
        raw12_single_sequence=raw12_hook_frames;
        raw12_single_state=1;
        raw12_single_wait=0;
        return;
    }
    if(raw12_single_sequence==raw12_hook_frames) return;
    z62_u32 index=raw12_single_captured;
    if(index>=4) return;
    raw12_dma_destination=raw12_burst_buffers[index];
    raw12_dma_capture();
    if(raw12_single_state>=3) return;
    raw12_burst_sequences[index]=raw12_single_sequence;
    raw12_burst_sources[index]=raw12_single_source;
    ++raw12_single_captured;
    if(index==3) raw12_single_state=2;
}
#endif
#endif
