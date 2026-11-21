/* SPDX-License-Identifier: MIT */
#define Z62_GRID_WITH_PORTRAIT 1
#define Z62_PORTRAIT_TRACE 0
#define Z62_PORTRAIT_NO_MENU 1
#define Z62_EVF_PORTRAIT 1
#include "movie_limit.h"
#include "portrait_menu.c"

z62_u32 z62_main(const struct z62_api *api)
{
    if (!api || api->abi!=Z62_ABI || api->target!=Z62_TARGET) return 1;
    if (!movie_limit_ready()) return 2;
    if (!histogram_horizon_ready(api)) return 3;
    z62_u32 result=portrait_init(api);
    if (result) return result;
    movie_limit_install();
    return 0;
}
