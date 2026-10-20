/**
 * MiniMotoNav LVGL configuration (works for the SDL simulator and ESP32-S3).
 */
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16

/* Use the C library allocator: no fixed LV_MEM_SIZE pool. */
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB

#define LV_USE_OS LV_OS_NONE

/* Aim one refresh opportunity per 60 Hz frame. */
#define LV_DEF_REFR_PERIOD 8

#define LV_USE_DRAW_SW 1
#define LV_DRAW_SW_DRAW_UNIT_CNT 1

/* Float angles + precise lines keep the gauge and map free of angle quantisation. */
#define LV_USE_FLOAT 1
#define LV_USE_MATRIX 1

#define LV_USE_LOG 0
#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1
#define LV_USE_ASSERT_OBJ 0

#define LV_USE_SYSMON 0
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR 0

#define LV_USE_CANVAS 0
#define LV_USE_SNAPSHOT 0
#define LV_USE_SDL 0
#define LV_USE_TFT_ESPI 0

#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/* Widgets used by the simulator control sheet. */
#define LV_USE_SLIDER 1
#define LV_USE_SWITCH 1
#define LV_USE_BUTTON 1
#define LV_USE_LABEL 1

#endif /* LV_CONF_H */
