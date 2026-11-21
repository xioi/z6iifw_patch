/* SPDX-License-Identifier: MIT */
#ifndef Z62_PLAYBACK_FILTER_CONFIG_H
#define Z62_PLAYBACK_FILTER_CONFIG_H
#include "z62.h"

enum {
    Z62_FILTER_ENABLED=1, Z62_PROTECTION_SHIFT=1, Z62_MEDIA_SHIFT=3,
    Z62_RATING_SHIFT=5, Z62_DATE_SHIFT=12
};

static inline z62_u32 z62_filter_days_in_month(z62_u32 year,z62_u32 month)
{
    static const unsigned char days[]={31,28,31,30,31,30,31,31,30,31,30,31};
    return days[month-1]+(month==2 && year%4==0 && (year%100!=0 || year%400==0));
}

static inline z62_u32 z62_playback_filter_valid(z62_u32 value)
{
    if ((value>>28) || (value&(1u<<Z62_RATING_SHIFT)) ||
        ((value>>Z62_PROTECTION_SHIFT)&3)==3 || ((value>>Z62_MEDIA_SHIFT)&3)==3)
        return 0;
    z62_u32 date=value>>Z62_DATE_SHIFT;
    if (!date) return 1;
    z62_u32 year=1980+(date>>9),month=(date>>5)&15,day=date&31;
    return year>=2000 && year<=2099 && month>=1 && month<=12 &&
           day>=1 && day<=z62_filter_days_in_month(year,month);
}

#endif
