/* SPDX-License-Identifier: MIT */
#ifndef Z62_RAW12_WRITE_BENCH_H
#define Z62_RAW12_WRITE_BENCH_H

#if Z62_RAW12_WRITE_BENCH
static z62_u32 raw12_bench_done;
static z62_u32 raw12_bench_wait;
static z62_u32 raw12_bench_next_name;

static int raw12_bench_open_unique(void)
{
    char path[16]={'B',':','\\','B','E','N','C','H','0','0','0','.','B','I','N',0};
    z62_u32 start=raw12_bench_next_name;
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
            raw12_bench_next_name=index+1;
            return file;
        }
    }
    return -1;
}

static void raw12_bench_write_status(z62_u32 stage,z62_u32 open_result,z62_u32 write_result,z62_u32 close_result,
                                     z62_u32 start,z62_u32 data_start,z62_u32 data_end,z62_u32 total_end,z62_u32 written)
{
    _Alignas(64) z62_u32 data[32];
    for(z62_u32 i=0;i<32;++i) data[i]=0;
    data[0]=0x42323152u;
    data[1]=1;
    data[2]=stage;
    data[3]=data_end-data_start;
    data[4]=total_end-start;
    data[5]=open_result;
    data[6]=write_result;
    data[7]=close_result;
    data[8]=48000000;
    data[9]=262144;
    data[10]=64;
    data[11]=(z62_u32)raw12_single_allocation;
    data[12]=start;
    data[13]=data_start;
    data[14]=data_end;
    data[15]=total_end;
    data[16]=written;
    data[31]=~0x42323152u;
    int file=raw12_status_open_unique(2);
    if(file<0) return;
    raw12_export_write(file,data,sizeof(data));
    RH_CALL(0x105c1749,z62_u32,int)(file);
}

static void raw12_write_bench_step(void)
{
    if(raw12_bench_done) return;
    if(RH_BYTE(RH_MOVIE_ACTIVE)==1) {
        raw12_bench_wait=0;
        return;
    }
    if(++raw12_bench_wait<500) return;
    raw12_bench_done=1;
    z62_u32 start=RH_WORD(0x04266008u);
    raw12_single_allocation=RH_CALL(0x1034c1f1,void *,z62_u32,z62_u32)((1u<<20)+63u,0);
    if(!raw12_single_allocation) {
        raw12_bench_write_status(1,0,0,0,start,start,start,start,0);
        return;
    }
    unsigned char *buffer=(unsigned char *)(((z62_u32)raw12_single_allocation+63u)&~63u);
    for(z62_u32 i=0;i<(1u<<20)/4u;++i) ((z62_u32 *)buffer)[i]=i*0x9e3779b9u;
    int file=raw12_bench_open_unique();
    if(file<0) {
        raw12_bench_write_status(2,(z62_u32)file,0,0,start,start,start,start,0);
        raw12_single_release();
        return;
    }
    z62_u32 data_start=RH_WORD(0x04266008u);
    z62_u32 result=0,written=0,stage=0;
    for(z62_u32 i=0;i<64;++i) {
        if(RH_BYTE(RH_MOVIE_ACTIVE)==1) {
            stage=5;
            break;
        }
        result=raw12_export_write(file,buffer,1u<<20);
        if(result) {
            stage=3;
            break;
        }
        written+=1u<<20;
    }
    z62_u32 data_end=RH_WORD(0x04266008u);
    z62_u32 closed=RH_CALL(0x105c1749,z62_u32,int)(file);
    if(closed && !stage) stage=4;
    z62_u32 total_end=RH_WORD(0x04266008u);
    raw12_bench_write_status(stage,(z62_u32)file,result,closed,start,data_start,data_end,total_end,written);
    raw12_single_release();
}
#endif
#endif
