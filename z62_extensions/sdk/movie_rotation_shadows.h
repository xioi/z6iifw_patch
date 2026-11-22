/* SPDX-License-Identifier: MIT */

/* The mux task is registered in RAM. Its close path calls the header builder
 * directly, so the three copies retain native event handling and file writes.
 * Offsets are relative to each copy; untouched PC-relative loads use their
 * original literal pool offsets. The task and close copies include that span. */
enum { MROT_TASK_SIZE=0x418, MROT_CLOSE_SIZE=0x264, MROT_HEADER_SIZE=0x7c,
       MROT_TASK_HASH=0x960f4921u, MROT_CLOSE_HASH=0x12db7d0au, MROT_HEADER_HASH=0x32842c20u };

static const z62_u32 movie_rot_task_calls[][2]={
    {0x022u,0x104e150du}, {0x02au,0x104e29c3u}, {0x036u,0x104e150du}, {0x03eu,0x104e1643u},
    {0x086u,0x104e150du}, {0x08eu,0x104e1d25u}, {0x096u,0x104e29b1u}, {0x0a2u,0x104e150du},
    {0x0aau,0x104e1643u}, {0x0eau,0x104e150du}, {0x0f2u,0x104e1c05u}, {0x0fau,0x104e2921u},
    {0x104u,0x104e299fu}, {0x110u,0x104e150du}, {0x118u,0x104e1643u}, {0x158u,0x104e150du},
    {0x160u,0x104e1b81u}, {0x168u,0x104e2991u}, {0x174u,0x104e150du}, {0x17cu,0x104e1643u},
    {0x1bau,0x104e150du}, {0x1cau,0x104e294fu}, {0x1d6u,0x104e150du}, {0x1deu,0x104e1643u},
    {0x220u,0x104e150du}, {0x23cu,0x104e293fu}, {0x248u,0x104e150du}, {0x256u,0x104e1643u},
    {0x25eu,0x104e1593u}, {0x26eu,0x104e15d5u}, {0x286u,0x104e1593u}, {0x2a2u,0x104e1547u},
    {0x2aeu,0x104e150du},
};
static const z62_u32 movie_rot_close_calls[][2]={
    {0x00cu,0x1035d1c1u}, {0x030u,0x1035d1ecu}, {0x038u,0x1035d1bbu}, {0x042u,0x104e293fu},
    {0x058u,0x104e2921u}, {0x070u,0x105236e1u}, {0x076u,0x104e166bu}, {0x086u,0x10539aabu},
    {0x08cu,0x10523ccfu}, {0x090u,0x104e166bu}, {0x09au,0x10539d6du}, {0x0a2u,0x10539d97u},
    {0x0aau,0x10523cc3u}, {0x0aeu,0x104e166bu}, {0x0b6u,0x10539aabu}, {0x0c2u,0x104e166bu},
    {0x0cau,0x10539ac3u}, {0x0d4u,0x1035d1bbu}, {0x0dau,0x104e293fu}, {0x0f6u,0x10539971u},
    {0x114u,0x104e1547u},
};
static const z62_u32 movie_rot_header_calls[][2]={
    {0x008u,0x1053c38du}, {0x010u,0x1053c1adu}, {0x028u,0x1053b323u}, {0x034u,0x1053ac8bu},
    {0x046u,0x1053d57du}, {0x04eu,0x1053d5edu}, {0x058u,0x1053a8efu}, {0x060u,0x10539fe7u},
    {0x072u,0x10539f41u},
};

static _Alignas(64) unsigned char movie_rot_task[MROT_TASK_SIZE+sizeof(movie_rot_task_calls)+24];
static _Alignas(64) unsigned char movie_rot_close[MROT_CLOSE_SIZE+sizeof(movie_rot_close_calls)+8];
static _Alignas(64) unsigned char movie_rot_header[MROT_HEADER_SIZE+sizeof(movie_rot_header_calls)+8];

static z62_u32 movie_rot_relocate(unsigned char *code, z62_u32 size,
                                  const z62_u32 calls[][2], z62_u32 count)
{
    z62_u32 stub=(z62_u32)code+size;
    for (z62_u32 i=0;i<count;++i,stub+=8u)
        write_call((z62_u32)code+calls[i][0],stub,calls[i][1]);
    return stub;
}

static z62_u32 movie_rot_ready(void)
{
    return MROT_WORD(MROT_CONTROL_SLOT)==MROT_CONTROL &&
           MROT_WORD(MROT_TASK_SLOT)==MROT_TASK_NATIVE &&
           code_hash(MROT_TASK_NATIVE&~1u,MROT_TASK_SIZE)==MROT_TASK_HASH &&
           code_hash(0x104e1694u,MROT_CLOSE_SIZE)==MROT_CLOSE_HASH &&
           code_hash(0x1053c4bcu,MROT_HEADER_SIZE)==MROT_HEADER_HASH;
}

static z62_u32 movie_rot_prepare(void)
{
    if (!movie_rot_ready() ||
        copy_code(movie_rot_task,MROT_TASK_NATIVE&~1u,MROT_TASK_SIZE,MROT_TASK_HASH) ||
        copy_code(movie_rot_close,0x104e1694u,MROT_CLOSE_SIZE,MROT_CLOSE_HASH) ||
        copy_code(movie_rot_header,0x1053c4bcu,MROT_HEADER_SIZE,MROT_HEADER_HASH)) return 0;

    z62_u32 stub=movie_rot_relocate(movie_rot_task,MROT_TASK_SIZE,movie_rot_task_calls,
                                    MROT_COUNT(movie_rot_task_calls));
    write_call((z62_u32)movie_rot_task+0x04au,stub,(z62_u32)movie_rot_open);
    write_call((z62_u32)movie_rot_task+0x1c2u,stub+8u,(z62_u32)movie_rot_split);
    write_call((z62_u32)movie_rot_task+0x22cu,stub+16u,(z62_u32)movie_rot_close|1u);
    stub=movie_rot_relocate(movie_rot_close,MROT_CLOSE_SIZE,movie_rot_close_calls,
                           MROT_COUNT(movie_rot_close_calls));
    write_call((z62_u32)movie_rot_close+0x0beu,stub,(z62_u32)movie_rot_header|1u);
    stub=movie_rot_relocate(movie_rot_header,MROT_HEADER_SIZE,movie_rot_header_calls,
                           MROT_COUNT(movie_rot_header_calls));
    write_call((z62_u32)movie_rot_header+0x01cu,stub,(z62_u32)movie_rot_track);
    return 1;
}

static void movie_rot_install(void)
{
    if (!movie_rot_prepare()) return;
    clean_code(movie_rot_task,sizeof(movie_rot_task));
    clean_code(movie_rot_close,sizeof(movie_rot_close));
    clean_code(movie_rot_header,sizeof(movie_rot_header));
    sync_code();
    MROT_WORD(MROT_TASK_SLOT)=(z62_u32)movie_rot_task|1u;
}
