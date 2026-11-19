#ifndef TYVOR_TFT_LVGL_H
#define TYVOR_TFT_LVGL_H
#include "lvgl.h"
#include "display_contract.h"
#include "tft_presenter.h"
enum {TYVOR_TFT_HOME,TYVOR_TFT_TELEMETRY,TYVOR_TFT_MODEL,TYVOR_TFT_SETTINGS,TYVOR_TFT_PAGES};
enum {TYVOR_TFT_DIAG_RED,TYVOR_TFT_DIAG_GREEN,TYVOR_TFT_DIAG_BLUE,TYVOR_TFT_DIAG_RGB565,TYVOR_TFT_DIAG_GEOMETRY};
typedef struct {
    tyvor_display_pipeline_t *pipeline;lv_display_t *display;
    lv_obj_t *root,*title,*source,*banner,*body,*labels[10],*previous,*next,*page_label;
    tyvor_tft_view_t view;
    unsigned page;uint8_t diagnostic;uint64_t last_frame_us,max_frame_us;uint32_t frames;
} tyvor_tft_lvgl_t;
int tyvor_tft_lvgl_init(tyvor_tft_lvgl_t *,tyvor_display_pipeline_t *,void *draw_buffer,size_t bytes);
void tyvor_tft_lvgl_update(tyvor_tft_lvgl_t *,const tyvor_tft_view_t *);
void tyvor_tft_lvgl_navigate(tyvor_tft_lvgl_t *,int direction);
int tyvor_tft_lvgl_rotate(tyvor_tft_lvgl_t *,unsigned rotation);
void tyvor_tft_lvgl_diagnostic(tyvor_tft_lvgl_t *,unsigned pattern);
void tyvor_tft_lvgl_frame(tyvor_tft_lvgl_t *);
void tyvor_tft_lvgl_destroy(tyvor_tft_lvgl_t *);
#endif
