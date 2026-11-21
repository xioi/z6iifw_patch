/* SPDX-License-Identifier: MIT */
#include "movie_limit.h"

z62_u32 z62_main(const struct z62_api *api)
{
    if (!api || api->abi!=Z62_ABI || api->target!=Z62_TARGET) return 1;
    if (!movie_limit_ready()) return 2;
    movie_limit_install();
    return 0;
}
