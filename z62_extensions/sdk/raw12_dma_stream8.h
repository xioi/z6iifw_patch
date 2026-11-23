/* SPDX-License-Identifier: MIT */
#ifndef Z62_RAW12_DMA_STREAM8_H
#define Z62_RAW12_DMA_STREAM8_H

#if Z62_RAW12_DMA_STREAM8 || Z62_RAW12_DMA_STREAM16
#define RS_LOAD(p) __atomic_load_n(p,__ATOMIC_ACQUIRE)
#define RS_STORE(p,v) __atomic_store_n(p,v,__ATOMIC_RELEASE)
#define RS_SLOTS 8u
#define RS_TOTAL (Z62_RAW12_DMA_STREAM16 ? 16u : 8u)

static z62_u32 raw12_stream_task[8];
static unsigned char raw12_stream_descriptor[28];
static unsigned char *raw12_stream_allocations[6];
static unsigned char *raw12_stream_buffers[RS_SLOTS];
static z62_u32 raw12_stream_sequences[RS_SLOTS];
static z62_u32 raw12_stream_sources[RS_SLOTS];
static z62_u32 raw12_stream_slots[RS_SLOTS];
static z62_u32 raw12_stream_capture_done;
static z62_u32 raw12_stream_batch_paused;
static z62_u32 raw12_stream_writer_done;
static z62_u32 raw12_stream_writer_error;
static int raw12_stream_file=-1;

static void raw12_stream_release(void)
{
    for(z62_u32 i=0;i<6;++i) {
        if(raw12_stream_allocations[i]) RH_CALL(0x1034c23f,void,void *)(raw12_stream_allocations[i]);
        raw12_stream_allocations[i]=0;
    }
    for(z62_u32 i=0;i<RS_SLOTS;++i) raw12_stream_buffers[i]=0;
    raw12_single_release();
}

static z62_u32 raw12_stream_verify(unsigned char *buffer)
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

static z62_u32 raw12_stream_header(void)
{
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
    return raw12_export_write(raw12_stream_file,raw12_export_header,64);
}

static void raw12_stream_finish(void)
{
    if(RS_LOAD(&raw12_stream_writer_done)) return;
    z62_u32 closed=0;
    if(raw12_stream_file>=0) closed=RH_CALL(0x105c1749,z62_u32,int)(raw12_stream_file);
    raw12_export_last_close=closed;
    raw12_stream_file=-1;
    if(RS_LOAD(&raw12_stream_writer_error) || closed) {
        ++raw12_single_export_errors;
    } else if(RS_LOAD(&raw12_stream_write_index)==RS_TOTAL) {
        ++raw12_single_exported;
    }
    RS_STORE(&raw12_stream_writer_done,1);
}

static void raw12_stream_fail(z62_u32 error)
{
    raw12_dual_error=error;
    raw12_single_state=4;
    ++raw12_single_rejected;
    RS_STORE(&raw12_stream_writer_error,1);
}

static void raw12_stream_writer(void *ignored)
{
    (void)ignored;
    for(;;) {
        if(RS_LOAD(&raw12_stream_writer_error)) {
            raw12_stream_finish();
            RH_CALL(0x103dec95,void,z62_u32)(2);
            continue;
        }
        z62_u32 index=RS_LOAD(&raw12_stream_write_index);
        z62_u32 captured=RS_LOAD(&raw12_single_captured);
        z62_u32 done=RS_LOAD(&raw12_stream_capture_done);
        if(index>=RS_TOTAL || (done && index>=captured)) {
            raw12_stream_finish();
            RH_CALL(0x103dec95,void,z62_u32)(2);
            continue;
        }
#if Z62_RAW12_DMA_STREAM16
        if(!captured || index>=captured) {
            RH_CALL(0x103dec95,void,z62_u32)(1);
            continue;
        }
#else
        if(captured<4 || index>=captured) {
            RH_CALL(0x103dec95,void,z62_u32)(1);
            continue;
        }
#endif
        if(raw12_stream_file<0) {
            raw12_stream_file=raw12_export_open_unique();
            raw12_export_last_open=(z62_u32)raw12_stream_file;
            if(raw12_stream_file<0) {
                raw12_stream_fail(0x2604);
                continue;
            }
            raw12_single_export_attempted=1;
            z62_u32 result=raw12_stream_header();
            if(result) {
                raw12_export_last_write=result;
                raw12_stream_fail(result);
                continue;
            }
        }
        z62_u32 slot=index&7u;
        if(RS_LOAD(&raw12_stream_slots[slot])!=2) {
            RH_CALL(0x103dec95,void,z62_u32)(1);
            continue;
        }
        RS_STORE(&raw12_stream_slots[slot],3);
#if Z62_RAW12_DMA_STREAM16
        if(index==0 && !raw12_stream_verify(raw12_stream_buffers[slot])) {
            raw12_stream_fail(0x2500u|slot);
            continue;
        }
#else
        if(!raw12_stream_verify(raw12_stream_buffers[slot])) {
            raw12_stream_fail(0x2500u|slot);
            continue;
        }
#endif
        raw12_export_frame[0]=0x46323152u;
        raw12_export_frame[1]=raw12_stream_sequences[slot];
        raw12_export_frame[2]=raw12_single_frame_bytes;
        raw12_export_frame[3]=raw12_stream_sources[slot];
        z62_u32 result=raw12_export_write(raw12_stream_file,raw12_export_frame,16);
        if(!result) result=raw12_export_write(raw12_stream_file,raw12_stream_buffers[slot],raw12_single_frame_bytes);
        raw12_export_last_write=result;
        if(result) {
            raw12_stream_fail(result);
            continue;
        }
        RS_STORE(&raw12_stream_slots[slot],0);
        RS_STORE(&raw12_stream_write_index,index+1);
    }
}

#if Z62_RAW12_DMA_STREAM16
static z62_u32 raw12_probe_events[32][16];
static z62_u32 raw12_probe_copies[16][16];
static z62_u32 raw12_probe_event_count;
static z62_u32 raw12_probe_copy_count;

static void raw12_probe_event(z62_u32 status)
{
    z62_u32 index=raw12_probe_event_count;
    if(index>=32 || !(status&0x1000u)) return;
    for(z62_u32 i=0;i<16;++i) raw12_probe_events[index][i]=0;
    raw12_probe_events[index][0]=raw12_hook_frames;
    raw12_probe_events[index][1]=RH_WORD(0x04266008u);
    raw12_probe_events[index][2]=RH_WORD(0x20380310u);
    raw12_probe_events[index][3]=status;
    raw12_probe_event_count=index+1;
}

static void raw12_probe_capture(void)
{
    z62_u32 index=raw12_probe_copy_count;
    if(index>=16) return;
    for(z62_u32 i=0;i<16;++i) raw12_probe_copies[index][i]=0;
    z62_u32 source=RH_WORD(0x20380310u);
    raw12_probe_copies[index][0]=raw12_hook_frames;
    raw12_probe_copies[index][1]=RH_WORD(0x04266008u);
    raw12_probe_copies[index][2]=source;
    raw12_probe_copies[index][3]=raw12_hook_last_status;
    for(z62_u32 group=0;group<3;++group) {
        z62_u32 offset=group==0 ? 0 : group==1 ? raw12_single_frame_bytes/2u : raw12_single_frame_bytes-16u;
        RH_CALL(0x1034bde9,void,void *,z62_u32)((unsigned char *)source+offset,16);
        for(z62_u32 word=0;word<4;++word)
            raw12_probe_copies[index][4+group*4+word]=RH_WORD(source+offset+word*4);
    }
    raw12_probe_copy_count=index+1;
}

static void raw12_probe_write(void)
{
    if(!raw12_probe_event_count) return;
    _Alignas(64) z62_u32 header[16];
    for(z62_u32 i=0;i<16;++i) header[i]=0;
    header[0]=0x50323152u;
    header[1]=1;
    header[2]=raw12_probe_event_count;
    header[3]=raw12_probe_copy_count;
    header[4]=raw12_single_frame_bytes;
    header[5]=48000000;
    header[6]=raw12_single_captured;
    header[7]=raw12_dual_error;
    header[15]=~0x50323152u;
    int file=raw12_status_open_unique(1);
    if(file<0) return;
    z62_u32 result=raw12_export_write(file,header,64);
    if(!result) result=raw12_export_write(file,raw12_probe_events,sizeof(raw12_probe_events));
    if(!result) result=raw12_export_write(file,raw12_probe_copies,sizeof(raw12_probe_copies));
    raw12_export_last_write=result;
    raw12_export_last_close=RH_CALL(0x105c1749,z62_u32,int)(file);
}
#endif

static void raw12_dma_stream8_step(void)
{
    if(RH_BYTE(RH_MOVIE_ACTIVE)!=1) {
        raw12_dma_restore();
        raw12_dma_ready_sequence=0;
        RS_STORE(&raw12_stream_capture_done,1);
        if(raw12_single_seen_recording && !raw12_single_status_written) {
            if(!RS_LOAD(&raw12_stream_writer_done)) {
                if(++raw12_single_export_wait<1000) return;
                if(!raw12_dual_error) raw12_stream_fail(0x2602);
                else RS_STORE(&raw12_stream_writer_error,1);
                return;
            }
#if Z62_RAW12_DMA_STREAM16
            raw12_probe_write();
#endif
            raw12_single_status_result=raw12_write_status();
            raw12_single_status_written=1;
        }
        if(RS_LOAD(&raw12_stream_writer_done)) {
            raw12_stream_release();
            raw12_single_state=0;
            raw12_single_wait=0;
        }
        return;
    }
    if(RS_LOAD(&raw12_stream_writer_error)) {
        RS_STORE(&raw12_stream_capture_done,1);
        return;
    }
#if Z62_RAW12_DMA_STREAM16
    if(raw12_stream_batch_paused) {
        if(RS_LOAD(&raw12_stream_write_index)<8) return;
        raw12_stream_dropped+=raw12_hook_frames-raw12_single_sequence;
        raw12_single_sequence=raw12_hook_frames;
        raw12_stream_batch_paused=0;
    }
#endif
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
        RS_STORE(&raw12_stream_capture_done,0);
        RS_STORE(&raw12_stream_batch_paused,0);
        RS_STORE(&raw12_stream_writer_done,0);
        RS_STORE(&raw12_stream_writer_error,0);
        RS_STORE(&raw12_stream_write_index,0);
#if Z62_RAW12_DMA_STREAM16
        raw12_stream_dropped=0;
        raw12_probe_event_count=0;
        raw12_probe_copy_count=0;
#endif
        for(z62_u32 i=0;i<RS_SLOTS;++i) RS_STORE(&raw12_stream_slots[i],0);
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
        for(z62_u32 i=0;i<6;++i) {
            if(!raw12_stream_allocations[i])
                raw12_stream_allocations[i]=RH_CALL(0x1034c1f1,void *,z62_u32,z62_u32)(raw12_single_frame_bytes+63u,i&1u);
        }
        if(!raw12_single_allocation || !raw12_dual_allocation || !raw12_stream_allocations[0] || !raw12_stream_allocations[1] ||
           !raw12_stream_allocations[2] || !raw12_stream_allocations[3] || !raw12_stream_allocations[4] || !raw12_stream_allocations[5]) {
            if(++raw12_single_wait<100) return;
            raw12_dma_reject(0x2102);
            RS_STORE(&raw12_stream_capture_done,1);
            RS_STORE(&raw12_stream_writer_done,1);
            return;
        }
        raw12_stream_buffers[0]=(unsigned char *)(((z62_u32)raw12_single_allocation+63u)&~63u);
        raw12_stream_buffers[1]=(unsigned char *)(((z62_u32)raw12_dual_allocation+63u)&~63u);
        for(z62_u32 i=0;i<6;++i)
            raw12_stream_buffers[i+2]=(unsigned char *)(((z62_u32)raw12_stream_allocations[i]+63u)&~63u);
        for(z62_u32 frame=0;frame<RS_SLOTS;++frame) {
            unsigned char *buffer=raw12_stream_buffers[frame];
            for(z62_u32 row=0;row<raw12_single_height;++row)
                for(z62_u32 word=0;word<4;++word)
                    RH_WORD((z62_u32)buffer+row*raw12_single_pitch+word*4)=raw12_dma_sentinel(row,word);
            RH_CALL(0x1034bdad,void,void *,z62_u32)(buffer,raw12_single_frame_bytes);
        }
        raw12_stream_file=raw12_export_open_unique();
        raw12_export_last_open=(z62_u32)raw12_stream_file;
        if(raw12_stream_file<0) {
            raw12_stream_fail(0x2604);
            RS_STORE(&raw12_stream_capture_done,1);
            return;
        }
        raw12_single_export_attempted=1;
        z62_u32 result=raw12_stream_header();
        if(result) {
            raw12_export_last_write=result;
            raw12_stream_fail(result);
            RS_STORE(&raw12_stream_capture_done,1);
            return;
        }
        raw12_single_sequence=raw12_hook_frames;
        raw12_single_state=1;
        raw12_single_wait=0;
        return;
    }
    z62_u32 index=RS_LOAD(&raw12_single_captured);
    if(index>=RS_TOTAL || raw12_single_sequence==raw12_hook_frames) return;
    z62_u32 slot=index&7u;
    if(RS_LOAD(&raw12_stream_slots[slot])!=0) {
        if(++raw12_single_wait>=200) {
            raw12_dual_error=0x2601;
            raw12_single_state=4;
            ++raw12_single_rejected;
            RS_STORE(&raw12_stream_capture_done,1);
        }
        return;
    }
    raw12_single_wait=0;
    RS_STORE(&raw12_stream_slots[slot],1);
    raw12_dma_destination=raw12_stream_buffers[slot];
#if Z62_RAW12_DMA_STREAM16
    raw12_probe_capture();
#endif
    raw12_dma_capture();
    if(raw12_single_state>=3) {
        RS_STORE(&raw12_stream_slots[slot],0);
        RS_STORE(&raw12_stream_capture_done,1);
        return;
    }
    raw12_stream_sequences[slot]=raw12_single_sequence;
    raw12_stream_sources[slot]=raw12_single_source;
    RS_STORE(&raw12_stream_slots[slot],2);
    RS_STORE(&raw12_single_captured,index+1);
#if Z62_RAW12_DMA_STREAM16
    if(index==7) {
        raw12_stream_batch_paused=1;
        return;
    }
#endif
    if(index==RS_TOTAL-1) {
        raw12_single_state=2;
        RS_STORE(&raw12_stream_capture_done,1);
    }
}
#endif
#endif
