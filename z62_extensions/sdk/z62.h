/* SPDX-License-Identifier: MIT */
#ifndef Z62_H
#define Z62_H
typedef unsigned int z62_u32;
#define Z62_ABI 1u
#define Z62_TARGET 0x01700062u

struct z62_api {
    z62_u32 abi, size, target;
};

z62_u32 z62_main(const struct z62_api *api);
#endif
