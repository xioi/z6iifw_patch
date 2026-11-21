/* SPDX-License-Identifier: MIT */
#include "z62.h"
typedef unsigned char u8;
typedef unsigned short u16;
typedef z62_u32 u32;
#define FN(a, result, ...) ((result (*)(__VA_ARGS__))(a))
#define FS_OK 0x043a0080u
static inline void *allocate(u32 size) { return FN(0x1034c1f1, void *, u32, u32)(size, 0); }
static inline void release(void *p) { FN(0x1034c23f, void, void *)(p); }
static inline void delay(u32 ticks) { FN(0x103dec95, void, u32)(ticks); }
static inline void invalidate(void *p, u32 n) { FN(0x1034bde9, void, void *, u32)(p, n); }
