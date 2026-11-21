/* SPDX-License-Identifier: MIT */
#ifndef Z62_MENU_H
#define Z62_MENU_H
#include "z62.h"

/* C 1.70 platform binding for owned rows prepended to the setup menu. */
#define Z62_SETUP_MENU 0xb1853e64u
#ifndef Z62_MENU_SLOT
#define Z62_MENU_SLOT 255u /* Grid labels use slots 250 through 254. */
#endif
#define Z62_MENU_ROW_ID 0x2fffu
#define Z62_MENU_ROWS 42u
#define Z62_MENU_EVENTS 61u
#ifndef Z62_MENU_EXTRA_ROWS
#define Z62_MENU_EXTRA_ROWS 2u
#endif
typedef z62_u32 (*z62_menu_callback)(z62_u32,z62_u32,z62_u32,z62_u32);
struct z62_menu_row {
    const char *label; /* NULL when the caller assigns a localized ID before publishing. */
    z62_menu_callback callback;
    z62_u32 value;
};
struct z62_menu_storage {
    z62_u32 rows[Z62_MENU_ROWS+Z62_MENU_EXTRA_ROWS][7];
    z62_u32 maps[Z62_MENU_ROWS+Z62_MENU_EXTRA_ROWS];
    z62_u32 events[Z62_MENU_EXTRA_ROWS][Z62_MENU_EVENTS][4];
};

static inline void z62_menu_set_label(z62_u32 row,const char *label)
{
    ((void (*)(const char *,z62_u32,z62_u32))0x10639c5b)(label,Z62_MENU_SLOT+row,0);
}

/* Storage and callbacks must remain valid until power-off. */
static inline z62_u32 z62_menu_prepare_rows(struct z62_menu_storage *storage,
                                           const struct z62_menu_row *added,z62_u32 count)
{
    volatile z62_u32 *header=(volatile z62_u32 *)Z62_SETUP_MENU;
    const z62_u32 *rows=(const z62_u32 *)0x117d844c;
    const z62_u32 *maps=(const z62_u32 *)0x11810d08;
    const z62_u32 *events=(const z62_u32 *)0x1180b848;
    if(!count || count>Z62_MENU_EXTRA_ROWS || header[3]!=(z62_u32)rows ||
       header[4]!=(z62_u32)maps || header[5]!=0x8002a ||
       events[(Z62_MENU_EVENTS-1)*4]!=0x31a) return 1;
    for(z62_u32 row=0;row<count;++row)
        if(added[row].label && !*(volatile z62_u32 *)(0xc2028884+(Z62_MENU_SLOT+row)*12+4)) return 1;
    for(z62_u32 row=0;row<Z62_MENU_ROWS;++row) {
        for(z62_u32 col=0;col<7;++col) storage->rows[row+count][col]=rows[row*7+col];
        storage->maps[row+count]=maps[row];
    }
    for(z62_u32 row=0;row<count;++row) {
        for(z62_u32 col=0;col<7;++col) storage->rows[row][col]=rows[(Z62_MENU_ROWS-1)*7+col];
        storage->rows[row][0]=Z62_MENU_SLOT+row;
        storage->rows[row][4]=0x1201; /* Native dynamic ASCII label. */
        storage->rows[row][5]=added[row].value;
        storage->rows[row][6]=Z62_MENU_ROW_ID-row;
        for(z62_u32 i=0;i<Z62_MENU_EVENTS;++i) {
            z62_u32 ev=events[i*4], fn=events[i*4+1], pred=events[i*4+2], dest=events[i*4+3];
            if(ev==0x257 || ev==0x25c || ev==0x270 || ev==0x27d) {
                fn=(z62_u32)added[row].callback; pred=0; dest=0x430;
            } else if(fn!=0x1063e0c1 && fn!=0x1063e0cd && fn!=0x1063e0d9 &&
                      fn!=0x1063e0dd && fn!=0x1063e0df && fn!=0x1063e105) {
                fn=0; pred=0; dest=dest==0x431 ? 0x431 : 0x430;
            }
            storage->events[row][i][0]=ev;
            storage->events[row][i][1]=fn;
            storage->events[row][i][2]=pred;
            storage->events[row][i][3]=dest;
        }
        storage->maps[row]=(z62_u32)storage->events[row];
        if(added[row].label) z62_menu_set_label(row,added[row].label);
    }
    return 0;
}

static inline void z62_menu_publish_rows(struct z62_menu_storage *storage,z62_u32 count)
{
    volatile z62_u32 *header=(volatile z62_u32 *)Z62_SETUP_MENU;
    __asm__ volatile("dmb sy" ::: "memory");
    header[3]=(z62_u32)storage->rows;
    header[4]=(z62_u32)storage->maps;
    header[5]=0x8002a+count;
}

static inline z62_u32 z62_menu_install_rows(struct z62_menu_storage *storage,
                                           const struct z62_menu_row *added,z62_u32 count)
{
    z62_u32 result=z62_menu_prepare_rows(storage,added,count);
    if(!result) z62_menu_publish_rows(storage,count);
    return result;
}

static inline z62_u32 z62_menu_install(struct z62_menu_storage *storage,
                                      const char *label,z62_menu_callback callback)
{
    const struct z62_menu_row row={label,callback,0xd7};
    return z62_menu_install_rows(storage,&row,1);
}
#endif
