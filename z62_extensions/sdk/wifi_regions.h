/* SPDX-License-Identifier: MIT */
#ifndef Z62_WIFI_REGIONS_H
#define Z62_WIFI_REGIONS_H

/* These indexes are persisted in Z62APP.CFG. Append rather than reorder. */
enum {
    Z62_WIFI_REGION_MAINLAND=0,
    Z62_WIFI_REGION_US,
    Z62_WIFI_REGION_BODY,
    Z62_WIFI_REGION_COUNT
};
#define Z62_WIFI_REGION_MASK 3u
#define Z62_WIFI_80MHZ_ON 4u
#define Z62_WIFI_COUNTRIES {"CN","US"}
#define Z62_WIFI_COUNTRY_REVISIONS {38u,0u}
#endif
