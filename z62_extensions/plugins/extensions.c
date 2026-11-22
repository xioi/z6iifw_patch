/* SPDX-License-Identifier: MIT */
#define Z62_GRID_WITH_PORTRAIT 1
#define Z62_PORTRAIT_TRACE 0
#define Z62_PORTRAIT_NO_MENU 1
#define Z62_DISABLE_HISTOGRAM 1
#ifndef Z62_WITH_MOVIE
#define Z62_WITH_MOVIE 1
#endif
#ifndef Z62_WITH_PORTRAIT
#define Z62_WITH_PORTRAIT 1
#endif
#ifndef Z62_WITH_MOVIE_ROTATION
#define Z62_WITH_MOVIE_ROTATION 0
#endif
#ifndef Z62_EVF_PORTRAIT
#define Z62_EVF_PORTRAIT Z62_WITH_PORTRAIT
#endif
#if Z62_WITH_MOVIE
#include "movie_limit.h"
#endif
#if Z62_WITH_MOVIE_ROTATION
#include "movie_rotation.h"
#endif
#if Z62_WITH_PORTRAIT
#include "portrait_menu.c"
#endif

z62_u32 z62_main(const struct z62_api *api)
{
    if (!api || api->abi!=Z62_ABI || api->target!=Z62_TARGET) return 1;
#if Z62_WITH_MOVIE
    if (!movie_limit_ready()) return 2;
#endif
#if Z62_WITH_MOVIE_ROTATION
    if (!movie_rot_ready()) return 4;
#endif
#if Z62_WITH_PORTRAIT
    if (!histogram_horizon_ready(api)) return 3;
    z62_u32 result=portrait_init(api);
    if (result) return result;
#endif
#if Z62_WITH_MOVIE
    movie_limit_install();
#endif
#if Z62_WITH_MOVIE_ROTATION
    movie_rot_install();
#endif
    return 0;
}
