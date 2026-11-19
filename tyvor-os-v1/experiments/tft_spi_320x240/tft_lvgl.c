#include "tft_lvgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const uint32_t background=0x0b1220,foreground=0xe8efff,muted=0x94a6c0,accent=0x2dd4bf;
static const char *pages[]={"HOME","TELEMETRY","MODEL","SETTINGS"};
static lv_obj_t *label(lv_obj_t *parent,int x,int y,int w,const char *text,const lv_font_t *font,uint32_t color) {
    lv_obj_t *l=lv_label_create(parent);lv_label_set_text(l,text);lv_obj_set_pos(l,x,y);lv_obj_set_width(l,w);
    lv_obj_set_style_text_font(l,font,0);lv_obj_set_style_text_color(l,lv_color_hex(color),0);
    lv_label_set_long_mode(l,LV_LABEL_LONG_WRAP);return l;
}
static void text(lv_obj_t *l,const char *s) {if(l && strcmp(lv_label_get_text(l),s))lv_label_set_text(l,s);}
static void flush(lv_display_t *display,const lv_area_t *area,uint8_t *pixels) {
    tyvor_tft_lvgl_t *u=lv_display_get_user_data(display);
    tyvor_display_region_t r={area->x1,area->y1,area->x2+1,area->y2+1};
    size_t n=(size_t)(r.x2-r.x1)*(size_t)(r.y2-r.y1)*2;
    if(!tyvor_display_region_valid(&u->pipeline->config,&r,n))abort();
    tyvor_rgb565_wire(pixels,n/2);
    /* One persistent DMA-capable draw buffer. This task waits; bridge task never
       shares this buffer/lock. ISR signals only a semaphore, never LVGL objects. */
    if(tyvor_display_begin(u->pipeline,&r,pixels,n) || tyvor_display_complete(u->pipeline))abort();
    lv_display_flush_ready(display); /* only after DMA relinquishes ownership */
}
static void navigation(lv_event_t *e) {
    tyvor_tft_lvgl_t *u=lv_event_get_user_data(e);
    tyvor_tft_lvgl_navigate(u,lv_event_get_target(e)==u->next?1:-1);
}
static void body_create(tyvor_tft_lvgl_t *u) {
    lv_obj_clean(u->body);memset(u->labels,0,sizeof(u->labels));
    int w=u->pipeline->config.width,h=u->pipeline->config.height,landscape=h<300;
    int body_w=w-24;
    if(u->page==TYVOR_TFT_HOME) {
        u->labels[0]=label(u->body,0,0,landscape?170:body_w,"MODEL",&lv_font_montserrat_12,muted);
        u->labels[1]=label(u->body,0,18,landscape?170:body_w,"--",&lv_font_montserrat_20,foreground);
        lv_label_set_long_mode(u->labels[1],LV_LABEL_LONG_DOT);
        u->labels[2]=label(u->body,landscape?180:0,landscape?0:55,landscape?body_w-180:body_w,"Battery --",&lv_font_montserrat_14,accent);
        u->labels[3]=label(u->body,landscape?180:0,landscape?30:82,landscape?body_w-180:body_w,"LQ --",&lv_font_montserrat_14,foreground);
        u->labels[4]=label(u->body,0,landscape?60:116,body_w,"Timer --:--",&lv_font_montserrat_20,foreground);
        u->labels[5]=label(u->body,0,landscape?96:155,body_w,"ARM: UNKNOWN / RO",&lv_font_montserrat_12,muted);
    } else if(u->page==TYVOR_TFT_TELEMETRY) {
        u->labels[0]=label(u->body,0,0,body_w,"Battery --",&lv_font_montserrat_20,foreground);
        u->labels[1]=label(u->body,0,34,landscape?135:body_w,"LQ --",&lv_font_montserrat_14,accent);
        u->labels[2]=label(u->body,landscape?150:0,landscape?34:62,landscape?body_w-150:body_w,"RSSI --",&lv_font_montserrat_14,foreground);
        u->labels[3]=label(u->body,0,landscape?64:94,body_w,"GPS --",&lv_font_montserrat_12,muted);
    } else if(u->page==TYVOR_TFT_MODEL) {
        u->labels[0]=label(u->body,0,0,body_w,"--",&lv_font_montserrat_20,foreground);
        u->labels[1]=label(u->body,0,40,body_w,"Safety UNKNOWN",&lv_font_montserrat_14,accent);
        u->labels[2]=label(u->body,0,landscape?75:90,body_w,"STM32 owns model / safety\nView only",&lv_font_montserrat_12,muted);
    } else {
        char geometry[48];(void)snprintf(geometry,sizeof(geometry),"TFT %ux%u / RGB565",u->pipeline->config.width,u->pipeline->config.height);
        u->labels[0]=label(u->body,0,0,body_w,geometry,&lv_font_montserrat_14,foreground);
        u->labels[1]=label(u->body,0,32,body_w,"UART v2 / hardware gate OFF",&lv_font_montserrat_12,muted);
        u->labels[2]=label(u->body,0,60,body_w,"Touch: IC UNVERIFIED",&lv_font_montserrat_12,muted);
        u->labels[3]=label(u->body,0,88,body_w,"No aircraft controls",&lv_font_montserrat_12,muted);
    }
    char page[40];(void)snprintf(page,sizeof(page),"%s %u/4",pages[u->page],u->page+1);text(u->page_label,page);
}
static void ui_create(tyvor_tft_lvgl_t *u) {
    lv_display_set_default(u->display);u->root=lv_display_get_screen_active(u->display);lv_obj_clean(u->root);
    int w=u->pipeline->config.width,h=u->pipeline->config.height;
    lv_obj_remove_flag(u->root,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_style_bg_color(u->root,lv_color_hex(background),0);
    lv_obj_set_style_bg_opa(u->root,LV_OPA_COVER,0);
    u->title=label(u->root,12,9,w-110,"TYVOR OS",&lv_font_montserrat_20,foreground);
    u->source=label(u->root,w-96,15,88,"SIMULATOR",&lv_font_montserrat_12,accent);
    u->banner=label(u->root,12,42,w-24,"DISCONNECTED",&lv_font_montserrat_12,foreground);
    lv_obj_set_style_bg_color(u->banner,lv_color_hex(0x172b42),0);lv_obj_set_style_bg_opa(u->banner,LV_OPA_COVER,0);
    lv_obj_set_style_radius(u->banner,5,0);lv_obj_set_style_pad_all(u->banner,6,0);
    u->body=lv_obj_create(u->root);lv_obj_remove_style_all(u->body);lv_obj_set_pos(u->body,12,80);lv_obj_set_size(u->body,w-24,h-124);
    lv_obj_remove_flag(u->body,LV_OBJ_FLAG_SCROLLABLE);
    u->previous=lv_button_create(u->root);lv_obj_set_pos(u->previous,8,h-34);lv_obj_set_size(u->previous,50,28);
    lv_obj_set_style_bg_color(u->previous,lv_color_hex(0x19334c),0);lv_obj_set_style_bg_opa(u->previous,LV_OPA_COVER,0);lv_obj_set_style_radius(u->previous,5,0);
    label(u->previous,7,5,40,"PREV",&lv_font_montserrat_12,foreground);lv_obj_add_event_cb(u->previous,navigation,LV_EVENT_CLICKED,u);
    u->next=lv_button_create(u->root);lv_obj_set_pos(u->next,w-58,h-34);lv_obj_set_size(u->next,50,28);
    lv_obj_set_style_bg_color(u->next,lv_color_hex(0x19334c),0);lv_obj_set_style_bg_opa(u->next,LV_OPA_COVER,0);lv_obj_set_style_radius(u->next,5,0);
    label(u->next,7,5,40,"NEXT",&lv_font_montserrat_12,foreground);lv_obj_add_event_cb(u->next,navigation,LV_EVENT_CLICKED,u);
    u->page_label=label(u->root,61,h-27,w-122,"HOME 1/4",&lv_font_montserrat_12,muted);
    lv_obj_set_style_text_align(u->page_label,LV_TEXT_ALIGN_CENTER,0);body_create(u);u->diagnostic=0;
}
int tyvor_tft_lvgl_init(tyvor_tft_lvgl_t *u,tyvor_display_pipeline_t *p,void *buffer,size_t n) {
    if(!u || !p || !buffer || n<TYVOR_DISPLAY_BUFFER_BYTES)return -1;
    memset(u,0,sizeof(*u));u->pipeline=p;u->display=lv_display_create(p->config.width,p->config.height);if(!u->display)return -1;
    lv_display_set_color_format(u->display,LV_COLOR_FORMAT_RGB565);lv_display_set_user_data(u->display,u);lv_display_set_flush_cb(u->display,flush);
    lv_display_set_buffers(u->display,buffer,NULL,TYVOR_DISPLAY_BUFFER_BYTES,LV_DISPLAY_RENDER_MODE_PARTIAL);
    ui_create(u);return 0;
}
void tyvor_tft_lvgl_update(tyvor_tft_lvgl_t *u,const tyvor_tft_view_t *v) {
    if(!u || !v)return;
    if(u->diagnostic)ui_create(u);
    u->view=*v;text(u->source,v->mock?"SIMULATOR":"STM32 LINK");
    char banner[64];
    if(v->quality==TYVOR_TFT_VALID)(void)snprintf(banner,sizeof(banner),"%s | %s",v->connection,v->safety);
    else (void)snprintf(banner,sizeof(banner),"%s",v->connection);
    text(u->banner,banner);
    lv_obj_set_style_text_color(u->banner,lv_color_hex(v->quality==TYVOR_TFT_VALID?accent:v->quality==TYVOR_TFT_STALE?0xfbbf24:0xfb7185),0);
    if(u->page==TYVOR_TFT_HOME) {text(u->labels[1],v->model);text(u->labels[2],v->battery);text(u->labels[3],v->lq);text(u->labels[4],v->flight_timer);text(u->labels[5],v->aircraft_arm);}
    else if(u->page==TYVOR_TFT_TELEMETRY) {text(u->labels[0],v->battery);text(u->labels[1],v->lq);text(u->labels[2],v->rssi);text(u->labels[3],v->gps);}
    else if(u->page==TYVOR_TFT_MODEL) {text(u->labels[0],v->model);text(u->labels[1],v->safety);}
}
void tyvor_tft_lvgl_navigate(tyvor_tft_lvgl_t *u,int direction) {
    if(!u || !direction)return;
    if(u->diagnostic)ui_create(u);
    u->page=(u->page+(direction>0?1:TYVOR_TFT_PAGES-1))%TYVOR_TFT_PAGES;body_create(u);
    tyvor_tft_view_t v=u->view;tyvor_tft_lvgl_update(u,&v);
}
int tyvor_tft_lvgl_rotate(tyvor_tft_lvgl_t *u,unsigned rotation) {
    if(!u || tyvor_display_rotate(u->pipeline,rotation))return -1;
    lv_display_set_resolution(u->display,u->pipeline->config.width,u->pipeline->config.height);
    ui_create(u);tyvor_tft_view_t v=u->view;tyvor_tft_lvgl_update(u,&v);return 0;
}
void tyvor_tft_lvgl_diagnostic(tyvor_tft_lvgl_t *u,unsigned pattern) {
    if(!u || pattern>TYVOR_TFT_DIAG_GEOMETRY)return;
    lv_obj_clean(u->root);memset(u->labels,0,sizeof(u->labels));u->diagnostic=1;
    int w=u->pipeline->config.width,h=u->pipeline->config.height;
    uint32_t color=pattern==TYVOR_TFT_DIAG_RED?0xff0000:pattern==TYVOR_TFT_DIAG_GREEN?0x00ff00:pattern==TYVOR_TFT_DIAG_BLUE?0x0000ff:background;
    lv_obj_set_style_bg_color(u->root,lv_color_hex(color),0);
    if(pattern==TYVOR_TFT_DIAG_RGB565)for(unsigned i=0;i<3;++i) {
        lv_obj_t *bar=lv_obj_create(u->root);lv_obj_remove_style_all(bar);lv_obj_set_pos(bar,(int)i*w/3,50);lv_obj_set_size(bar,w/3,h-90);
        lv_obj_set_style_bg_color(bar,lv_color_hex(i==0?0xff0000:i==1?0x00ff00:0x0000ff),0);lv_obj_set_style_bg_opa(bar,LV_OPA_COVER,0);
    }
    if(pattern==TYVOR_TFT_DIAG_GEOMETRY) {
        lv_obj_t *box=lv_obj_create(u->root);lv_obj_remove_style_all(box);lv_obj_set_pos(box,8,50);lv_obj_set_size(box,w-16,h-80);
        lv_obj_set_style_border_color(box,lv_color_hex(accent),0);lv_obj_set_style_border_width(box,3,0);
        label(u->root,18,70,w-36,"RECTANGLE\nRGB565 / region test",&lv_font_montserrat_14,foreground);
        label(u->root,14,54,24,"TL",&lv_font_montserrat_12,accent);
        label(u->root,w-36,54,24,"TR",&lv_font_montserrat_12,accent);
        label(u->root,14,h-49,24,"BL",&lv_font_montserrat_12,accent);
        label(u->root,w-36,h-49,24,"BR",&lv_font_montserrat_12,accent);
    }
    label(u->root,12,12,w-24,"TYVOR OS",&lv_font_montserrat_20,pattern==TYVOR_TFT_DIAG_GREEN?0x000000:0xffffff);
    char caption[80];(void)snprintf(caption,sizeof(caption),"R%u %ux%u / TFT DIAGNOSTIC",u->pipeline->config.rotation*90,w,h);
    label(u->root,12,h-26,w-24,caption,&lv_font_montserrat_12,pattern==TYVOR_TFT_DIAG_GREEN?0x000000:0xffffff);
}
void tyvor_tft_lvgl_frame(tyvor_tft_lvgl_t *u) {
    if(!u)return;
    uint64_t start=u->pipeline->backend.micros(u->pipeline->backend.context);lv_refr_now(u->display);
    uint64_t end=u->pipeline->backend.micros(u->pipeline->backend.context);u->last_frame_us=end>=start?end-start:0;
    if(u->last_frame_us>u->max_frame_us)u->max_frame_us=u->last_frame_us;
    ++u->frames;
}
void tyvor_tft_lvgl_destroy(tyvor_tft_lvgl_t *u) {if(u && u->display && !u->pipeline->busy){lv_display_delete(u->display);u->display=NULL;}}
