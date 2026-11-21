/* SPDX-License-Identifier: MIT */
#include "platform.h"

#define HEADER 64u
#define MAX_FILE 1048576u
#define MAX_MEMORY 4194304u
enum { NO_CARD=1, NO_FILE, BAD_SIZE, ALLOC_FAILED, READ_FAILED, CLOSE_FAILED,
       BAD_HEADER, BAD_CRC, BAD_RELOC, MODULE_FAILED, LOADED };

struct state { u32 task[8]; u8 *image; u32 status; u32 module_result; };
#define ANCHOR ((volatile u32 *)0xc18aedb8)
static struct state *current(void)
{
    u32 p=ANCHOR[0];
    if(!p || (p^ANCHOR[1])!=~0u) return 0;
    return (struct state *)p;
}
static const struct z62_api api = {Z62_ABI, sizeof(api), Z62_TARGET};

static void copy(u8 *dst, const u8 *src, u32 size)
{
    for (u32 i=0; i<size; ++i) dst[i] = src[i];
}

static u32 checksum(const u8 *data, u32 size)
{
    u32 crc = ~0u;
    for (u32 i=0; i<size; ++i) {
        crc ^= i >= 32 && i < 36 ? 0 : data[i];
        for (u32 bit=0; bit<8; ++bit)
            crc = (crc >> 1) ^ ((0u - (crc & 1)) & 0xedb88320u);
    }
    return ~crc;
}

static void executable(u8 *image, u32 size)
{
    for (u32 p=(u32)image; p<(u32)image+size; p+=32)
        __asm__ volatile("mcr p15, 0, %0, c7, c11, 1" :: "r"(p) : "memory");
    u32 zero=0;
    __asm__ volatile("dsb sy\n mcr p15, 0, %0, c7, c1, 0\n dsb sy\n isb sy"
                     :: "r"(zero) : "memory");
}

__attribute__((noinline)) u32 extension_call(u8 *image, u32 entry)
{
    return ((u32 (*)(const struct z62_api *))((u32)image + entry + 1))(&api);
}
__attribute__((noinline)) void extension_result(u32 status, void *image, u32 result)
{
    __asm__ volatile("" :: "r"(status), "r"(image), "r"(result) : "memory");
}

static u32 load(u8 **retained, u32 *module_result, const char *path)
{
    u32 file[10];
    for(u32 i=0;i<10;++i) ((volatile u32 *)file)[i]=0;
    FN(0x104d67a5, void, u32 *, const char *)(file, path);
    if (FN(0x104d67a9, u32, u32 *, u32, u32)(file, 0, 0) != FS_OK)
        return NO_FILE;
    u32 size=file[6], status=BAD_SIZE;
    u8 *raw=0, *body=0;
    if (!file[7] && size>=HEADER+2 && size<=MAX_FILE) {
        raw=allocate(size);
        if (!raw) status=ALLOC_FAILED;
        else {
            _Alignas(64) u8 buffer[576];
            status=0;
            for (u32 offset=0; offset<size;) {
                u32 count=size-offset, actual=0;
                if (count>512) count=512;
                u32 capacity=count+(offset+count==size);
                struct { u32 kind; u8 *buffer; u32 capacity; } request={1,buffer,capacity};
                invalidate(buffer, sizeof(buffer));
                u32 result=FN(0x104d6927,u32,u32 *,void *,u32,u32 *)(file,&request,capacity,&actual);
                invalidate(buffer, sizeof(buffer));
                if (result!=FS_OK || actual!=count) { status=READ_FAILED; break; }
                copy(raw+offset,buffer,count);
                offset+=count;
            }
        }
    }
    if (FN(0x104d6835,u32,u32 *)(file)!=FS_OK) status=CLOSE_FAILED;
    if (status) { if(raw) release(raw); return status; }

    u32 *h=(u32 *)raw, image=h[4], bss=h[5], entry=h[6], relocs=h[7];
    if (h[0]!=0x4d32365a || h[1]!=Z62_ABI || h[2]!=Z62_TARGET || h[3]!=size ||
        !image || image%64 || image>MAX_FILE-HEADER || bss>MAX_MEMORY-image ||
        entry%2 || entry>=image || relocs>(MAX_FILE-HEADER-image)/4 ||
        size!=HEADER+image+4*relocs) status=BAD_HEADER;
    for(u32 i=9;i<16;++i) if(h[i]) status=BAD_HEADER;
    if (!status && checksum(raw,size)!=h[8]) status=BAD_CRC;
    if (status) { release(raw); return status; }

    void *allocation=allocate(image+bss+63);
    if (!allocation) { release(raw); return ALLOC_FAILED; }
    body=(u8 *)(((u32)allocation+63)&~63u);
    copy(body,raw+HEADER,image);
    for(u32 i=0;i<bss;++i) body[image+i]=0;
    u32 *rel=(u32 *)(raw+HEADER+image), previous=0;
    for(u32 i=0;i<relocs;++i) {
        u32 off=rel[i];
        if(off%4 || off>image-4 || (i && off<=previous) || *(u32 *)(body+off)>=image+bss) {
            status=BAD_RELOC; break;
        }
        *(u32 *)(body+off)+=(u32)body;
        previous=off;
    }
    release(raw);
    if(status) { release(allocation); return status; }
    executable(body,image+bss);
    struct state *s=current();
    s->image=body;
    *module_result=extension_call(body,entry);
    if(*module_result) {
        s->image=0;
        release(allocation);
        return MODULE_FAILED;
    }
    *retained=body;
    return LOADED;
}

static void worker(void *task)
{
    (void)task;
    u32 status=NO_CARD, result=0;
    u8 *image=0;
    for(u32 attempt=0;attempt<600;++attempt) {
        const volatile u8 *app=(const volatile u8 *)0xc1811e50;
        if(app[0]==1 && app[16]==3 && app[24]==2) {
            status=load(&image,&result,"B:\\Z62APP.BIN");
            if(status==NO_FILE) status=load(&image,&result,"A:\\Z62APP.BIN");
            struct state *s=current();
            if(s) { s->status=status; s->module_result=result; }
            if(status!=NO_FILE) break;
        }
        delay(100);
    }
    extension_result(status,image,result);
    extension_result(status,image,result);
    for(;;) FN(0x101045f1,void,void)();
}

void extension_boot(void)
{
    FN(0x104e3239,void,void)();
    if(current()) return;
    struct state *s=allocate(sizeof(*s));
    if(!s) return;
    for(u32 i=0;i<sizeof(*s)/4;++i) ((volatile u32 *)s)[i]=0;
    _Alignas(4) u8 descriptor[28];
    const u8 *original=FN(0x103d655b,const u8 *,u32)(0x10005);
    copy(descriptor,original,sizeof(descriptor));
    descriptor[0]=1;
    *(u32 *)(descriptor+8)=8192;
    *(u16 *)(descriptor+26)=0;
    FN(0x103debc7,int,void *,void (*)(void *),void *)(s,worker,descriptor);
    if((int)s->task[0]<=0) { release(s); return; }
    ANCHOR[0]=(u32)s;
    ANCHOR[1]=~(u32)s;
    __asm__ volatile("dmb sy" ::: "memory");
    FN(0x1035c987,int,void *)(s);
}
