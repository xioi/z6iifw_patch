/* SPDX-License-Identifier: MIT */
#include "z62.h"
#ifdef Z62_PHOTO_ZEBRA
#include "photo_zebra.h"
#endif

typedef z62_u32 u32;
typedef unsigned char u8;
#ifndef WORD
#define WORD(a) (*(volatile u32 *)(a))
#define BYTE(a) (*(volatile u8 *)(a))
#define CALL(a, result, ...) ((result (*)(__VA_ARGS__))(a))
#endif

#define GUI_UPDATE_SLOT 0xc1821b6cu
#define GUI_UPDATE 0x104e9845u
#ifndef DISPLAY_SNAPSHOT
#define DISPLAY_SNAPSHOT 0xc202bf40u
#endif
#define LAST_DISPLAY_SNAPSHOT 0xc202c2c4u
#define DISPLAY_UPDATE_TICK 0xc181ddbcu
#define DISPLAY_SCENE 0xc181dabdu
#define ACTIVE_OUTPUT_BITS 0xc181d3ccu
#define LCD_OUTPUT 1u
#define EVF_OUTPUT 4u
#define LCD_SCENE 0u
#define EVF_SCENE 2u
#define HISTOGRAM_MASKS 0xc181f8dcu
#define HISTOGRAM_CONTROL 0xc181f8a8u
#define HISTOGRAM_STATE 0xc202bd08u
#define HISTOGRAM_BINS 0xc202caccu
#define HISTOGRAM_WIDGET 32u
#define DEFAULT_DRAW 0x106f22a7u
#define HISTOGRAM_DRAW 0x105249a5u
#define EXTRA_UPDATE_INTERVAL 100u

static u32 updating;
static u32 last_extra_update_end;
static _Alignas(64) u32 histogram_samples[256];
static volatile u32 paint_visible;

#ifdef Z62_PORTRAIT_HISTOGRAM
static u32 paint_orientation;
static u32 portrait_histogram_orientation(void);
static u32 draw_portrait_histogram(u32 orientation);
static void clear_portrait_histogram(u32 orientation);
#endif

enum histogram_result {
    NO_TICK, OTHER_PAGE, PREVIOUS_PAGE, WRONG_CONTEXT, MENU_BLOCKED,
    GUI_NOT_READY, UPDATE_BLOCKED, WIDGET_BLOCKED, OUTPUT_INACTIVE,
    INVALID_OBJECT, CONTROL_BUSY, INELIGIBLE, MASK_MISMATCH, SAMPLE_BLOCKED,
    SAMPLE_FAILED, SAMPLE_EMPTY, LAYOUT_HIDDEN, PAINTER_MISMATCH,
    SUBMIT_FAILED, DRAWN, HISTOGRAM_RESULTS
};

#ifdef Z62_HISTOGRAM_DIAGNOSTIC
static void histogram_observe(enum histogram_result result,u32 before,u32 after);
static volatile u32 painter_calls,visible_painter_calls,present_requests;
static volatile u32 painter_object,painter_visibility,painter_scale;
#endif

static u32 histogram_painter(u32 object,u32 next,u32 c,u32 d)
{
    u32 visible=WORD(object+0x28);
    if (visible) paint_visible=1;
#ifdef Z62_HISTOGRAM_DIAGNOSTIC
    ++painter_calls;
    if (visible) ++visible_painter_calls;
    painter_object=object;
    painter_visibility=visible;
    painter_scale=WORD(HISTOGRAM_STATE+0x34);
#endif
#ifdef Z62_PORTRAIT_HISTOGRAM
    if (visible && paint_orientation) {
        paint_visible=draw_portrait_histogram(paint_orientation);
        return 1;
    }
#endif
    return CALL(HISTOGRAM_DRAW,u32,u32,u32,u32,u32)(object,next,c,d);
}

static u32 native_sampling_allowed(void)
{
    u32 (*property)(u32)=CALL(0x1063f9e9,u32,u32);
    if (property(0x168c)==1) return 1;
    return property(0x112c)==1 && !CALL(0x105b326d,u32,void)() &&
           !CALL(0x1062352d,u32,void)();
}

static u32 active_photo_output(void)
{
    if (WORD(SHOOTING_MODE)!=0) return 0;
    u32 scene=BYTE(DISPLAY_SCENE);
    u32 outputs=WORD(ACTIVE_OUTPUT_BITS)&7u;
    if (scene==LCD_SCENE && (outputs&LCD_OUTPUT)) return LCD_OUTPUT;
    /* The stock photo update selects scene 2 for the EVF. Require that
     * scene to be active and leave secondary output routing to firmware. */
    if (scene==EVF_SCENE && outputs==EVF_OUTPUT &&
        CALL(0x10634b9b,u32,void)()==EVF_OUTPUT) return EVF_OUTPUT;
    return 0;
}

static u32 display_work_pending(void)
{
    /* The stock abort callback polls queued display events and the requested
     * output without consuming those events. Check before its render waits. */
    if (CALL(0x106387f5,u32,void)()) return 1;
    /* Match the busy conditions in the native render wait. This optional
     * pass can yield to the next GUI update instead of entering that wait. */
    return WORD(0xc181d3d8) &&
           (WORD(0xc181d3dc) || CALL(0x106314c3,u32,void)());
}

static void layout_histogram(void)
{
    CALL(0x10557d4b,void,u32)(HISTOGRAM_WIDGET);
    CALL(0x10557d17,void,u32)(HISTOGRAM_WIDGET);
    CALL(0x10525c35,void,u32)(HISTOGRAM_WIDGET);
}

#ifdef Z62_FOCAL_LENGTH
#include "focal_length.h"
#endif
#ifdef Z62_BATTERY_PERCENT
#include "battery_percent.h"
#endif

static u32 histogram_masks_match(void)
{
    for (u32 i=0;i<9;++i)
        if (WORD(HISTOGRAM_MASKS+4*i)!=0x444u) return 0;
    return 1;
}

static void set_histogram_masks(u32 mask)
{
    for (u32 i=0;i<9;++i) WORD(HISTOGRAM_MASKS+4*i)=mask;
}

/* Freestanding module builds do not link the compiler's division helper. */
static u32 average(u32 total,u32 count)
{
    u32 quotient=0,remainder=0;
    for (int bit=31;bit>=0;--bit) {
        remainder=(remainder<<1)|((total>>bit)&1);
        if (remainder>=count) {
            remainder-=count;
            quotient|=1u<<bit;
        }
    }
    return quotient;
}

__attribute__((used)) static enum histogram_result update_histogram(void)
{
    if (WORD(SHOOTING_MODE)!=0) return INELIGIBLE;
    u32 page=3;
#ifdef Z62_CUSTOM_DISP
    u32 custom=disp_custom_live();
    if (custom) {
        page=BYTE(DISPLAY_SNAPSHOT+0x19e);
        if ((page!=0 && page!=3) || !(disp_components()&Z62_DISP_HISTOGRAM)) return OTHER_PAGE;
    }
#endif
    if (BYTE(DISPLAY_SNAPSHOT+0x19e)!=page) return OTHER_PAGE;
    if (BYTE(LAST_DISPLAY_SNAPSHOT+0x19e)!=page) return PREVIOUS_PAGE;
    if (BYTE(DISPLAY_SNAPSHOT+0x1a6) || BYTE(0xc1821b4c) || BYTE(0xc202e364))
        return WRONG_CONTEXT;
    if (CALL(0x104aea9d,u32,void)()) return MENU_BLOCKED;
    if (!CALL(0x105260d7,u32,void)()) return GUI_NOT_READY;
    if (CALL(0x10633f73,u32,void)()==1) return UPDATE_BLOCKED;
    if (CALL(0x10620f8d,u32,u32)(12)) return WIDGET_BLOCKED;
    u32 output=active_photo_output();
    if (!output) return OUTPUT_INACTIVE;
    if (display_work_pending()) return UPDATE_BLOCKED;

    u32 object=WORD(HISTOGRAM_CONTROL+0x24);
    if (object<0xc1800000u || object>=0xd0000000u || (object&3)) return INVALID_OBJECT;
    if (BYTE(HISTOGRAM_CONTROL) || BYTE(HISTOGRAM_CONTROL+1)) return CONTROL_BUSY;
    if (!native_sampling_allowed()) return INELIGIBLE;
    if (!histogram_masks_match()) return MASK_MISMATCH;

    _Alignas(4) u8 readiness[0x468];
    CALL(0x106bf313,void,void *)(readiness);
    if (readiness[0x28] || readiness[0x29]) return SAMPLE_BLOCKED;
    if (!CALL(0x10640765,u32,u32 *)(histogram_samples)) return SAMPLE_FAILED;

    u32 peak=0,total=0,count=0;
    for (u32 i=0;i<256;++i) {
        u32 bin=histogram_samples[i];
        if (bin>peak) peak=bin;
        if (bin) { total+=bin; ++count; }
    }
    if (!count) return SAMPLE_EMPTY;
    if (active_photo_output()!=output || display_work_pending()) return UPDATE_BLOCKED;
    u32 scale=average(total,count)<<2;
    if (peak<scale) scale=peak;
    for (u32 i=0;i<256;++i) WORD(HISTOGRAM_BINS+4*i)=histogram_samples[i];
    WORD(HISTOGRAM_STATE+0x34)=scale;
    BYTE(HISTOGRAM_STATE+0x10)=1;

    /* This runs on the GUI task after the stock display pass. The masks only
     * need to be extended during placement of this one widget. */
    BYTE(HISTOGRAM_CONTROL)=1;
    BYTE(HISTOGRAM_CONTROL+1)=1;
#ifdef Z62_CUSTOM_DISP
    if (custom) {
        WORD(HISTOGRAM_MASKS)=0x444u|(0x11u<<page);
        WORD(HISTOGRAM_MASKS+12)=0x444u|(0x11u<<page);
    }
    else
#endif
        set_histogram_masks(0xcccu);
    layout_histogram();
#ifdef Z62_CUSTOM_DISP
    if (custom) {
        WORD(HISTOGRAM_MASKS)=0x444u;
        WORD(HISTOGRAM_MASKS+12)=0x444u;
    }
    else
#endif
        set_histogram_masks(0x444u);
    enum histogram_result result=BYTE(HISTOGRAM_CONTROL+1)==1 ? PAINTER_MISMATCH : LAYOUT_HIDDEN;
    u32 painter=WORD(object+0x4c);
    if (BYTE(HISTOGRAM_CONTROL+1)==1 && WORD(HISTOGRAM_CONTROL+0x24)==object &&
        (painter==DEFAULT_DRAW || painter==HISTOGRAM_DRAW)) {
        CALL(0x10528eb9,void,void)();
        if (WORD(object+0x4c)==HISTOGRAM_DRAW) {
            paint_visible=0;
#ifdef Z62_PORTRAIT_HISTOGRAM
            paint_orientation=portrait_histogram_orientation();
#endif
            CALL(0x1073e301,u32,u32,u32)(object,(u32)histogram_painter);
            u32 drawn=CALL(0x105254ab,u32,void)();
#ifdef Z62_PORTRAIT_HISTOGRAM
            paint_orientation=0;
#endif
            /* The native submit consumes dirty callbacks synchronously.
             * Restore the ordinary painter before another page can use it. */
            if (WORD(HISTOGRAM_CONTROL+0x24)!=object) return INVALID_OBJECT;
            CALL(0x1073e301,u32,u32,u32)(object,DEFAULT_DRAW);
            if (drawn && paint_visible && BYTE(DISPLAY_SNAPSHOT+0x19e)==page &&
                active_photo_output()==output && !CALL(0x106387f5,u32,void)()) {
                /* The stock present routine routes the current output. */
                CALL(0x10524627,void,void)();
#ifdef Z62_HISTOGRAM_DIAGNOSTIC
                ++present_requests;
#endif
                return DRAWN;
            }
            CALL(0x106393df,u32,u32,u32)(object,1);
            if (!drawn) result=SUBMIT_FAILED;
        }
    }

    BYTE(HISTOGRAM_STATE+0x10)=0;
    BYTE(HISTOGRAM_CONTROL)=1;
    BYTE(HISTOGRAM_CONTROL+1)=0;
    layout_histogram();
    return result;
}

static u32 gui_update(u32 a,u32 b,u32 c,u32 d)
{
    u32 (*original)(u32,u32,u32,u32)=CALL(GUI_UPDATE,u32,u32,u32,u32,u32);
#if Z62_DISABLE_HISTOGRAM
    return original(a,b,c,d);
#else
    if (__atomic_exchange_n(&updating,1,__ATOMIC_ACQUIRE)) return original(a,b,c,d);
    u32 before=WORD(DISPLAY_UPDATE_TICK);
    u32 output_before=WORD(ACTIVE_OUTPUT_BITS);
#ifdef Z62_PORTRAIT_HISTOGRAM
    u32 orientation=portrait_histogram_orientation();
    u32 page=BYTE(DISPLAY_SNAPSHOT+0x19e);
    u32 histogram_page=page==2 || page==3;
#ifdef Z62_CUSTOM_DISP
    if (disp_custom_live()) histogram_page=!!(disp_components()&Z62_DISP_HISTOGRAM);
#endif
    clear_portrait_histogram(histogram_page ? orientation : 0);
    /* The stock sampler uses the DISP page directly. Suppress only the
     * landscape widget during this synchronous pass. */
    u32 hide=orientation && (page==2 || BYTE(LAST_DISPLAY_SNAPSHOT+0x19e)==2) &&
             histogram_masks_match();
    if (hide) set_histogram_masks(0);
#endif
#ifdef Z62_BATTERY_PERCENT
    battery_prepare();
#endif
    u32 result=original(a,b,c,d);
#ifdef Z62_FOCAL_LENGTH
    focal_update(output_before&7u);
#endif
#ifdef Z62_BATTERY_PERCENT
    battery_update(output_before&7u,live_orientation);
#endif
#ifdef Z62_PHOTO_ZEBRA
    z62_photo_zebra_update();
#endif
#ifdef Z62_PORTRAIT_HISTOGRAM
    if (hide) set_histogram_masks(0x444u);
    if (current_page()!=LIVE) {
        __atomic_store_n(&updating,0,__ATOMIC_RELEASE);
        return result;
    }
    orientation=portrait_histogram_orientation();
    u32 portrait_normal=hide && orientation && BYTE(DISPLAY_SNAPSHOT+0x19e)==2;
#else
    const u32 portrait_normal=0;
#endif
    u32 after=WORD(DISPLAY_UPDATE_TICK);
    enum histogram_result outcome=NO_TICK;
    u32 extra=portrait_normal || BYTE(DISPLAY_SNAPSHOT+0x19e)==3;
#ifdef Z62_CUSTOM_DISP
    if (disp_custom_live()) extra=!!(disp_components()&Z62_DISP_HISTOGRAM);
#endif
    if (extra &&
        ((after && after!=before) || !CALL(0x1062163d,u32,void)())) {
        u32 now=CALL(0x106317d5,u32,void)();
        if (output_before!=WORD(ACTIVE_OUTPUT_BITS)) {
            last_extra_update_end=now;
        } else if (now && (!last_extra_update_end ||
                           now-last_extra_update_end>=EXTRA_UPDATE_INTERVAL)) {
#ifdef Z62_PORTRAIT_HISTOGRAM
            if (portrait_normal) {
                if (!BYTE(HISTOGRAM_CONTROL+1) && !display_work_pending() &&
                    draw_portrait_histogram(orientation) && portrait_histogram_orientation()==orientation &&
                    !CALL(0x106387f5,u32,void)())
                    CALL(0x10524627,void,void)();
            } else
#endif
                outcome=update_histogram();
            /* Throttle both timestamp paths from completion so a slow
             * sampling or render call cannot trigger an immediate retry. */
            last_extra_update_end=CALL(0x106317d5,u32,void)();
        }
    }
#ifdef Z62_HISTOGRAM_DIAGNOSTIC
    histogram_observe(outcome,before,after);
#else
    (void)outcome;
#endif
    __atomic_store_n(&updating,0,__ATOMIC_RELEASE);
    return result;
#endif
}

static u32 histogram_horizon_ready(const struct z62_api *api)
{
    return api->abi==Z62_ABI && api->target==Z62_TARGET && api->size>=sizeof(*api) &&
           WORD(0xc202f7e4)==0xc1821b50u && WORD(GUI_UPDATE_SLOT)==GUI_UPDATE &&
           WORD(0x1094f84c)==0xc181f8d8u && WORD(0xc181f904)==HISTOGRAM_CONTROL;
}

#ifndef Z62_PORTRAIT_HISTOGRAM
static void histogram_horizon_install(void)
{
    __asm__ volatile("dmb sy" ::: "memory");
    WORD(GUI_UPDATE_SLOT)=(u32)gui_update;
}

#ifndef Z62_HISTOGRAM_WITH_GRID
z62_u32 z62_main(const struct z62_api *api)
{
    if (!histogram_horizon_ready(api)) return 1;
    histogram_horizon_install();
    return 0;
}
#endif
#endif
