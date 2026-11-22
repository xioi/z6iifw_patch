/* SPDX-License-Identifier: MIT */
#ifndef Z62_MOVIE_ROTATION_H
#define Z62_MOVIE_ROTATION_H
#include "code_copy.h"

#define MROT_WORD(address) (*(volatile z62_u32 *)(address))
#define MROT_NATIVE(address, result, ...) ((result (*)(__VA_ARGS__))(address))
#define MROT_COUNT(table) (sizeof(table)/sizeof((table)[0]))

enum {
    MROT_TASK_SLOT=0xc1f2d84cu, MROT_TASK_NATIVE=0x104e2141u,
    MROT_CONTROL_SLOT=0xc1811550u, MROT_CONTROL=0xc1f2d858u,
    MROT_LAST_FILE=10, MROT_FILE_BASE=0x3f0, MROT_FILE_STRIDE=0x88,
    MROT_FILE_CONTEXT=0x2c, MROT_OPEN_REPLY=0x160, MROT_SPLIT_REPLY=0x318,
    MROT_MATRIX=0x109
};

/* Accessed only by the file mux task, between its open and close calls. */
static struct {
    z62_u32 context, pose;
} movie_rot_record[MROT_LAST_FILE+1];

static z62_u32 movie_rot_context(z62_u32 file)
{
    return MROT_WORD(MROT_CONTROL+MROT_FILE_BASE+file*MROT_FILE_STRIDE+MROT_FILE_CONTEXT);
}

static z62_u32 movie_rot_latch(z62_u32 file, z62_u32 reply_base)
{
    if (file && file<=MROT_LAST_FILE && MROT_WORD(MROT_CONTROL_SLOT)==MROT_CONTROL) {
        movie_rot_record[file].context=0;
        if (!MROT_WORD(MROT_CONTROL+reply_base+8u*file+4u)) {
            z62_u32 context=movie_rot_context(file);
            if (context) {
                movie_rot_record[file].pose=MROT_NATIVE(0x103d67f3u,z62_u32,void)();
                movie_rot_record[file].context=context;
            }
        }
    }
    return file;
}

static z62_u32 movie_rot_open(void)
{
    return movie_rot_latch(MROT_NATIVE(0x104e1eb1u,z62_u32,void)(),MROT_OPEN_REPLY);
}

static z62_u32 movie_rot_split(void)
{
    return movie_rot_latch(MROT_NATIVE(0x104e1831u,z62_u32,void)(),MROT_SPLIT_REPLY);
}

static z62_u32 movie_rot_pose(z62_u32 context)
{
    if (!context || MROT_WORD(MROT_CONTROL_SLOT)!=MROT_CONTROL) return 0;
    for (z62_u32 file=1;file<=MROT_LAST_FILE;++file) {
        if (movie_rot_record[file].context==context && movie_rot_context(file)==context)
            return movie_rot_record[file].pose;
    }
    return 0;
}

/* Big-endian ISO track matrices, in native pose order. Pose 0 uses stock. */
static const unsigned char movie_rot_matrices[4][36]={
    [1]={[4]=0xff,[5]=0xff,[13]=1,[32]=0x40},                  /* 90 */
    [2]={[5]=1,[12]=0xff,[13]=0xff,[32]=0x40},                 /* 270 */
    [3]={[0]=0xff,[1]=0xff,[16]=0xff,[17]=0xff,[32]=0x40},     /* 180 */
};

static z62_u32 movie_rot_track(z62_u32 context)
{
    z62_u32 result=MROT_NATIVE(0x1053ba7du,z62_u32,z62_u32)(context);
    if (!result) {
        z62_u32 pose=movie_rot_pose(context);
        if (pose && pose<MROT_COUNT(movie_rot_matrices))
            MROT_NATIVE(0x10623e59u,void,z62_u32,const unsigned char *,z62_u32)(
                context+MROT_MATRIX,movie_rot_matrices[pose],sizeof(movie_rot_matrices[pose]));
    }
    return result;
}

#include "movie_rotation_shadows.h"
#endif
