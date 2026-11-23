/* SPDX-License-Identifier: MIT */
#ifndef Z62_RAW12_WRITE_BENCH_H
#define Z62_RAW12_WRITE_BENCH_H

#if Z62_RAW12_WRITE_BENCH
#define RB_CASES 3u
#define RB_BYTES (64u<<20)
#define RB_ABORT 0x00574241u

static z62_u32 raw12_bench_done;
static z62_u32 raw12_bench_wait;
static z62_u32 raw12_bench_next_name;
static z62_u32 raw12_bench_last_volume;
static _Alignas(64) z62_u32 raw12_bench_records[RB_CASES][16];

static int raw12_bench_open_unique(z62_u32 forced_volume)
{
    char path[16]={'B',':','\\','B','E','N','C','H','0','0','0','.','B','I','N',0};
    z62_u32 start=raw12_bench_next_name;
    raw12_bench_last_volume=0;
    for(z62_u32 n=0;n<1000;++n) {
        z62_u32 index=(start+n)%1000u;
        path[8]=(unsigned char)('0'+index/100);
        path[9]=(unsigned char)('0'+index/10%10);
        path[10]=(unsigned char)('0'+index%10);
        for(z62_u32 volume=0;volume<2;++volume) {
            path[0]=volume ? 'A' : 'B';
            if(forced_volume && path[0]!=forced_volume) continue;
            if(raw12_export_exists(path)) continue;
            int file=RH_CALL(0x105c1897,int,const char *,z62_u32)(path,0);
            if(file<0) continue;
            raw12_bench_next_name=index+1;
            raw12_bench_last_volume=path[0];
            return file;
        }
    }
    return -1;
}

static z62_u32 raw12_bench_write(int file,unsigned char *buffer,z62_u32 size,z62_u32 write_limit,z62_u32 clean_limit,
                                 z62_u32 *clean_ticks,z62_u32 *write_ticks,z62_u32 *written,
                                 z62_u32 *write_calls,z62_u32 *clean_calls)
{
    z62_u32 offset=0;
    while(offset<size) {
        if(RH_BYTE(RH_MOVIE_ACTIVE)==1) return RB_ABORT;
        z62_u32 count=size-offset;
        if(count>write_limit) count=write_limit;
        z62_u32 buffer_offset=offset&((4u<<20)-1u);
        for(z62_u32 clean_offset=0;clean_offset<count;clean_offset+=clean_limit) {
            z62_u32 clean_count=count-clean_offset;
            if(clean_count>clean_limit) clean_count=clean_limit;
            z62_u32 start=RH_WORD(0x04266008u);
            RH_CALL(0x1034bdad,void,void *,z62_u32)(buffer+buffer_offset+clean_offset,clean_count);
            *clean_ticks+=RH_WORD(0x04266008u)-start;
            ++*clean_calls;
        }
        struct { z62_u32 kind; void *buffer; z62_u32 capacity; } request={1,buffer+buffer_offset,count};
        z62_u32 actual=0,start=RH_WORD(0x04266008u);
        z62_u32 result=RH_CALL(0x105c17e5,z62_u32,int,void *,z62_u32,z62_u32 *)(file,&request,count,&actual);
        *write_ticks+=RH_WORD(0x04266008u)-start;
        ++*write_calls;
        if(result || actual!=count) return result ? result : 1;
        offset+=count;
        *written+=count;
    }
    return 0;
}

static void raw12_bench_write_status(z62_u32 stage,z62_u32 completed,z62_u32 total_written)
{
    _Alignas(64) z62_u32 data[32];
    for(z62_u32 i=0;i<32;++i) data[i]=0;
    z62_u32 *first=raw12_bench_records[0];
    data[0]=0x42323152u;
    data[1]=2;
    data[2]=stage;
    data[3]=first[3];
    data[4]=first[4];
    data[5]=first[5];
    data[6]=first[6];
    data[7]=first[7];
    data[8]=48000000;
    data[9]=first[1];
    data[10]=64;
    data[11]=(z62_u32)raw12_single_allocation;
    data[12]=first[14];
    data[13]=first[14];
    data[14]=first[14]+first[3];
    data[15]=first[15];
    data[16]=first[8];
    data[17]=first[9];
    data[18]=completed;
    data[19]=RB_CASES;
    data[20]=total_written;
    data[21]=first[11];
    data[22]=first[10];
    data[23]=first[12];
    data[24]=first[13];
    data[31]=~0x42323152u;
    int file=raw12_status_open_unique(2);
    if(file<0) return;
    raw12_export_write(file,data,sizeof(data));
    raw12_export_write(file,raw12_bench_records,sizeof(raw12_bench_records));
    RH_CALL(0x105c1749,z62_u32,int)(file);
}

static void raw12_write_bench_step(void)
{
    static const z62_u32 write_limits[RB_CASES]={262144u,1u<<20,4u<<20};
    if(raw12_bench_done) return;
    if(RH_BYTE(RH_MOVIE_ACTIVE)==1) {
        raw12_bench_wait=0;
        return;
    }
    if(++raw12_bench_wait<500) return;
    raw12_bench_done=1;
    raw12_single_allocation=RH_CALL(0x1034c1f1,void *,z62_u32,z62_u32)((4u<<20)+63u,0);
    if(!raw12_single_allocation) {
        raw12_bench_write_status(1,0,0);
        return;
    }
    unsigned char *buffer=(unsigned char *)(((z62_u32)raw12_single_allocation+63u)&~63u);
    for(z62_u32 i=0;i<(4u<<20)/4u;++i)
        ((z62_u32 *)buffer)[i]=(i&((1u<<20)/4u-1u))*0x9e3779b9u;
    z62_u32 stage=0,completed=0,total_written=0;
    for(z62_u32 index=0;index<RB_CASES;++index) {
        z62_u32 *record=raw12_bench_records[index];
        record[0]=index;
        record[1]=write_limits[index];
        record[2]=262144u;
        record[14]=RH_WORD(0x04266008u);
        int file=raw12_bench_open_unique(Z62_RAW12_BENCH_VOLUME);
        record[5]=(z62_u32)file;
        record[9]=raw12_bench_last_volume;
        if(file<0) {
            stage=2;
            record[15]=RH_WORD(0x04266008u);
            raw12_bench_write_status(stage,completed,total_written);
            break;
        }
        z62_u32 data_start=RH_WORD(0x04266008u);
        z62_u32 result=raw12_bench_write(file,buffer,RB_BYTES,record[1],record[2],
                                         &record[10],&record[11],&record[8],&record[12],&record[13]);
        z62_u32 data_end=RH_WORD(0x04266008u);
        z62_u32 closed=RH_CALL(0x105c1749,z62_u32,int)(file);
        z62_u32 total_end=RH_WORD(0x04266008u);
        record[3]=data_end-data_start;
        record[4]=total_end-record[14];
        record[6]=result;
        record[7]=closed;
        record[15]=total_end;
        if(result) stage=result==RB_ABORT ? 5 : 3;
        else if(closed) stage=4;
        else {
            ++completed;
            total_written+=record[8];
        }
        raw12_bench_write_status(stage,completed,total_written);
        if(stage) break;
    }
    raw12_single_release();
}
#endif
#endif
