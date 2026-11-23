/* SPDX-License-Identifier: MIT */
#ifndef Z62_RAW12_DMA_DUAL_H
#define Z62_RAW12_DMA_DUAL_H

#if Z62_RAW12_DMA_DUAL
static z62_u32 raw12_dma_verify_frame(unsigned char *buffer)
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

static void raw12_dma_dual_step(void)
{
    if(RH_BYTE(RH_MOVIE_ACTIVE)!=1) {
        raw12_dma_restore();
        raw12_dma_ready_sequence=0;
        if(raw12_single_seen_recording && !raw12_single_status_written) {
            if(++raw12_single_export_wait<200) return;
            if(raw12_single_buffer && raw12_dual_buffer && raw12_single_state==2 && !raw12_single_export_attempted) {
                if(!raw12_dma_verify_frame(raw12_single_buffer)) {
                    raw12_dma_reject(0x2406);
                } else if(!raw12_dma_verify_frame(raw12_dual_buffer)) {
                    raw12_dma_reject(0x2408);
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
        raw12_dual_allocation_results[0]=(z62_u32)raw12_single_allocation;
        raw12_dual_allocation_results[1]=(z62_u32)raw12_dual_allocation;
        if(!raw12_single_allocation || !raw12_dual_allocation) {
            if(++raw12_single_wait<100) return;
            raw12_dma_reject(0x2102);
            return;
        }
        raw12_single_buffer=(unsigned char *)(((z62_u32)raw12_single_allocation+63u)&~63u);
        raw12_dual_buffer=(unsigned char *)(((z62_u32)raw12_dual_allocation+63u)&~63u);
        for(z62_u32 frame=0;frame<2;++frame) {
            unsigned char *buffer=frame ? raw12_dual_buffer : raw12_single_buffer;
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
    if(index>=2) return;
    raw12_dma_destination=index ? raw12_dual_buffer : raw12_single_buffer;
    raw12_dma_capture();
    if(raw12_single_state>=3) return;
    raw12_dual_sequences[index]=raw12_single_sequence;
    raw12_dual_sources[index]=raw12_single_source;
    ++raw12_single_captured;
    if(index==1) raw12_single_state=2;
}
#endif
#endif
