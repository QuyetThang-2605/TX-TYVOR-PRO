#include "mock_display.h"
#include "tft_lvgl.h"
#include <stdio.h>
static tyvor_tft_mock_t mock;static tyvor_display_pipeline_t pipe;static tyvor_tft_lvgl_t ui;
static uint8_t buffer[TYVOR_DISPLAY_BUFFER_BYTES];
int main(int argc,char **argv) {
    if(argc!=2)return 2;
    tyvor_display_config_t config;if(tyvor_display_config(&config,0))return 1;
    tyvor_tft_mock_init(&mock,&config);if(tyvor_display_pipeline_init(&pipe,&config,tyvor_tft_mock_backend(&mock)))return 1;
    lv_init();if(tyvor_tft_lvgl_init(&ui,&pipe,buffer,sizeof(buffer)))return 1;
    unsigned images=0;
    for(unsigned r=0;r<4;++r) {
        if(tyvor_tft_lvgl_rotate(&ui,r))return 1;
        for(unsigned state=0;state<4;++state)for(unsigned page=0;page<4;++page) {
            tyvor_ui_state_t s;tyvor_tft_view_t v;tyvor_tft_fixture(&s,1000,state);if(tyvor_tft_project(&s,1000,1,65000,&v))return 1;
            while(ui.page!=page)tyvor_tft_lvgl_navigate(&ui,1);
            tyvor_tft_lvgl_update(&ui,&v);tyvor_tft_lvgl_frame(&ui);char path[1024];
            if(snprintf(path,sizeof(path),"%s/ui-r%u-state%u-page%u.ppm",argv[1],r*90,state,page)<0 || tyvor_tft_mock_ppm(&mock,path))return 1;
            ++images;
        }
        for(unsigned pattern=0;pattern<=TYVOR_TFT_DIAG_GEOMETRY;++pattern) {
            tyvor_tft_lvgl_diagnostic(&ui,pattern);tyvor_tft_lvgl_frame(&ui);char path[1024];
            if(snprintf(path,sizeof(path),"%s/diagnostic-r%u-pattern%u.ppm",argv[1],r*90,pattern)<0 || tyvor_tft_mock_ppm(&mock,path))return 1;
            ++images;
        }
    }
    printf("PASS: %u real LVGL RGB565 mock renders; simulated time only; no display hardware\n",images);return 0;
}
