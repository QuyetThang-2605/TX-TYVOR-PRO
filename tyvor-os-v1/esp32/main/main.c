#include <inttypes.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "tyvor_async_tx.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_psram.h"
#include "esp_task_wdt.h"
#include "esp_random.h"
#include "esp_system.h"
#include "bootloader_random.h"
#include "tyvor_ui_endpoint.h"
#include "tyvor_diagnostics.h"
#define BRIDGE_UART UART_NUM_1
static const char *TAG="TYVOR-UI";
static tyvor_ui_endpoint_t ui_endpoint;
/* Bridge is sole endpoint/cache owner. UI receives an atomic value copy through
 * a one-element overwrite queue; rendering/logging never owns a bridge lock. */
typedef struct {
    tyvor_ui_state_t screen;tyvor_diagnostics_t diagnostics;tyvor_snapshot_t snapshot;
    tyvor_protocol_stats_t counters;tyvor_link_stats_t session;
    uint32_t local_epoch,peer_epoch;unsigned ready;
    uint32_t tx_full,tx_expired,tx_timeouts,tx_errors,tx_high_water;
    uint64_t max_bridge_us,max_bridge_gap_ms;
} ui_publication_t;
static QueueHandle_t publications;
static tyvor_async_tx_t tx_queue;
static const uint8_t *tx_bytes;
static uint16_t tx_count,tx_offset;
static QueueHandle_t uart_events;
static TaskHandle_t bridge_handle;
static bool uart_ready;
static tyvor_bench_transport_t uart_transport;
static tyvor_diagnostics_t diagnostics;
static int receive_ui(void *ctx,const uint8_t *b,size_t n,uint64_t now) {return tyvor_ui_endpoint_feed(ctx,b,n,now);}
static uint64_t millis(void) {return (uint64_t)esp_timer_get_time()/1000;}
static bool init_uart(void) {
#if !CONFIG_TYVOR_UART_PINOUT_VERIFIED
    ESP_LOGW(TAG,"UART DISABLED: board pinout unverified");return false;
#endif
    int tx=CONFIG_TYVOR_UART_TX_GPIO,rx=CONFIG_TYVOR_UART_RX_GPIO;
    if (tx<0 || rx<0 || tx==rx || !GPIO_IS_VALID_OUTPUT_GPIO(tx) || !GPIO_IS_VALID_GPIO(rx)) {
        ESP_LOGE(TAG,"Verified gate set but UART GPIO configuration invalid");return false;
    }
    uart_config_t conf={.baud_rate=115200,.data_bits=UART_DATA_8_BITS,.parity=UART_PARITY_DISABLE,
        .stop_bits=UART_STOP_BITS_1,.flow_ctrl=UART_HW_FLOWCTRL_DISABLE,.source_clk=UART_SCLK_DEFAULT};
    ESP_ERROR_CHECK(uart_param_config(BRIDGE_UART,&conf));
    ESP_ERROR_CHECK(uart_set_pin(BRIDGE_UART,tx,rx,UART_PIN_NO_CHANGE,UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(BRIDGE_UART,2048,0,16,&uart_events,0));
    return true;
}
static int tx_start(void *ctx,const uint8_t *bytes,uint16_t n) {
    (void)ctx;tx_bytes=bytes;tx_count=n;tx_offset=0;return uart_ready?0:-1;
}
static int tx_state(void *ctx) {
    (void)ctx;
    if(tx_offset==tx_count)return 1;
    int n=uart_tx_chars(BRIDGE_UART,(const char *)tx_bytes+tx_offset,tx_count-tx_offset);
    if(n<0 || n>tx_count-tx_offset)return -1;
    tx_offset+=(uint16_t)n;return tx_offset==tx_count?1:0;
}
static void tx_abort(void *ctx) {(void)ctx;tx_bytes=NULL;tx_count=tx_offset=0;}
static int write_uart(void *ctx,const uint8_t *bytes,uint16_t n) {
    (void)ctx;
    if(!uart_ready)return -1;
    return tyvor_async_tx_write(&tx_queue,bytes,n,millis());
}
static uint64_t fresh_boot_id(void) {
    uint64_t id=0;
    /* No ADC/Wi-Fi/BT task is active here. Enable SDK internal entropy briefly,
       then disable before any other service starts. No fixed-ID fallback. */
    bootloader_random_enable();
    for (unsigned i=0;i<4 && !id;++i) esp_fill_random(&id,sizeof(id));
    bootloader_random_disable();
    if (!id) abort();
    return id;
}
static void bridge_task(void *ctx) {
    (void)ctx;ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    uint64_t next_request=0;
    uint64_t last_start=millis(),max_gap=0,max_work=0;
    for (;;) {
        uint64_t now=millis();
        uint64_t work_start=(uint64_t)esp_timer_get_time(),gap=now-last_start;
        last_start=now;if(gap>max_gap)max_gap=gap;
        tyvor_async_tx_poll(&tx_queue,now);
        if (uart_ready) {
            if(uxQueueMessagesWaiting(uart_events)>=16) {
                /* An error event may have been lost in a saturated event queue. */
                tyvor_bench_transport_error(&uart_transport,&ui_endpoint.link,1);
                ESP_ERROR_CHECK(uart_flush_input(BRIDGE_UART));xQueueReset(uart_events);
            }
            uart_event_t event;
            for (unsigned i=0;i<16 && xQueueReceive(uart_events,&event,0)==pdTRUE;++i) {
                bool overflow=event.type==UART_FIFO_OVF || event.type==UART_BUFFER_FULL;
                if (overflow || event.type==UART_FRAME_ERR || event.type==UART_PARITY_ERR || event.type==UART_BREAK) {
                    tyvor_bench_transport_error(&uart_transport,&ui_endpoint.link,overflow);
                    ESP_ERROR_CHECK(uart_flush_input(BRIDGE_UART));
                    xQueueReset(uart_events);break;
                }
            }
            for (unsigned chunk=0;chunk<4;++chunk) {
                uint8_t bytes[64];int n=uart_read_bytes(BRIDGE_UART,bytes,sizeof(bytes),0);
                if (n<0) {tyvor_bench_transport_error(&uart_transport,&ui_endpoint.link,0);break;}
                if (!n) break;
                (void)tyvor_bench_transport_receive(&uart_transport,bytes,(size_t)n,millis(),receive_ui,&ui_endpoint);
            }
        }
        now=millis();tyvor_ui_endpoint_poll(&ui_endpoint,now);
        if (ui_endpoint.link.ready && uart_ready && now>=next_request) {
            (void)tyvor_ui_endpoint_request(&ui_endpoint,TYVOR_REQ_MODEL_INFO,0);
            next_request=now+1000;
        }
        tyvor_async_tx_poll(&tx_queue,millis());
        static ui_publication_t publication;
        tyvor_diagnostics_update(&diagnostics,&ui_endpoint.link,&uart_transport,millis());
        publication.screen=ui_endpoint.state;publication.diagnostics=diagnostics;
        publication.snapshot=ui_endpoint.link.snapshot;publication.counters=ui_endpoint.link.parser.stats;
        publication.session=ui_endpoint.link.stats;publication.local_epoch=ui_endpoint.link.local_epoch;
        publication.peer_epoch=ui_endpoint.link.peer_epoch;publication.ready=ui_endpoint.link.ready;
        publication.tx_full=tx_queue.full;publication.tx_expired=tx_queue.expired;
        publication.tx_timeouts=tx_queue.timeouts;publication.tx_errors=tx_queue.errors;publication.tx_high_water=tx_queue.high_water;
        uint64_t work=(uint64_t)esp_timer_get_time()-work_start;if(work>max_work)max_work=work;
        publication.max_bridge_us=max_work;publication.max_bridge_gap_ms=max_gap;
        xQueueOverwrite(publications,&publication); /* bounded fixed-size copy, no UI-owned lock */
        ESP_ERROR_CHECK(esp_task_wdt_reset());vTaskDelay(pdMS_TO_TICKS(10));
    }
}
static void diagnostics_task(void *ctx) {
    (void)ctx;unsigned page=0;
    for (;;) {
        tyvor_ui_state_t screen;tyvor_diagnostics_t local_diag;
        tyvor_snapshot_t snapshot;tyvor_protocol_stats_t counters;tyvor_link_stats_t session;
        uint32_t local_epoch,peer_epoch;unsigned ready;
        static ui_publication_t publication; /* UI-task-owned; avoids duplicate stack projection */
        if(xQueuePeek(publications,&publication,0)!=pdTRUE) {vTaskDelay(pdMS_TO_TICKS(10));continue;}
        local_diag=publication.diagnostics;screen=publication.screen;
        snapshot=publication.snapshot;counters=publication.counters;session=publication.session;
        local_epoch=publication.local_epoch;peer_epoch=publication.peer_epoch;ready=publication.ready;
        uint64_t now=millis();(void)tyvor_snapshot_poll(&snapshot,now,TYVOR_STALE_MS);
        tyvor_ui_view_t view;
        tyvor_ui_state_poll(&screen,now);
        if (!tyvor_ui_core_view(page,&screen,&view)) {
            ESP_LOGI(TAG,"%s | %s",view.title,view.banner);
            for(unsigned row=0;row<TYVOR_UI_ROWS;++row)ESP_LOGI(TAG,"%s",view.rows[row]);
        }
        page=(page+1)%7;
        ESP_LOGI(TAG,"TX pending_high=%" PRIu32 " full=%" PRIu32 " expired=%" PRIu32 " timeout=%" PRIu32 " error=%" PRIu32 " bridge_work_before_publish_max_us=%" PRIu64 " bridge_gap_max_ms=%" PRIu64,
            publication.tx_high_water,publication.tx_full,publication.tx_expired,publication.tx_timeouts,publication.tx_errors,publication.max_bridge_us,publication.max_bridge_gap_ms);
        ESP_LOGI(TAG,"protocol=%u ready=%u epochs=%" PRIu32 "/%" PRIu32
            " sessions=%" PRIu32 " timeout=%" PRIu32 " old_session=%" PRIu32 " collision=%" PRIu32,
            TYVOR_VERSION,ready,local_epoch,peer_epoch,session.sessions,session.handshake_timeouts,
            session.old_session,session.collisions);
        ESP_LOGI(TAG,"age=%" PRIu64 "ms frames=%" PRIu32 " crc=%" PRIu32 " frame=%" PRIu32
            " semantic=%" PRIu32 " timeout=%" PRIu32 " overflow=%" PRIu32 " duplicate=%" PRIu32
            " sequence=%" PRIu32 " missing=%" PRIu32 " tx_error=%" PRIu32 " partial=%" PRIu32,
            snapshot.received_once && now>=snapshot.received_ms?now-snapshot.received_ms:0,
            counters.frames,counters.crc_errors,counters.frame_errors,counters.semantic_errors,
            counters.timeouts,counters.overflows,counters.duplicates,counters.sequence_errors,counters.missing,
            session.tx_errors,session.tx_partial);
        ESP_LOGI(TAG,"heap_internal=%u min_internal=%u largest_internal=%u psram_free=%u stack_bridge=%u stack_diag=%u (bytes on ESP-IDF)",
            (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
            (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
            (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
            (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
            (unsigned)uxTaskGetStackHighWaterMark(bridge_handle),(unsigned)uxTaskGetStackHighWaterMark(NULL));
#if TYVOR_DIAGNOSTICS
        local_diag.heap_free=(uint32_t)heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
        local_diag.heap_min=(uint32_t)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
        local_diag.heap_largest=(uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);local_diag.heap_valid=1;
        local_diag.stack_free=(uint32_t)uxTaskGetStackHighWaterMark(bridge_handle);local_diag.stack_reserve=4096;local_diag.stack_valid=1;
        local_diag.watchdog_status_valid=1;local_diag.watchdog_subscribed=esp_task_wdt_status(bridge_handle)==ESP_OK;
        ESP_LOGI(TAG,"DIAG fw=%s protocol=%u boot_reason=%" PRIu32 " resets_observed=%" PRIu32 " lifetime=UNVERIFIED WDT=%u UART_gate=%u",
            TYVOR_FIRMWARE_VERSION,local_diag.protocol,local_diag.boot_reason,local_diag.resets_observed,local_diag.watchdog_subscribed,local_diag.uart_gate);
        ESP_LOGI(TAG,"DIAG heap=%" PRIu32 " min=%" PRIu32 " largest=%" PRIu32 " bridge_stack_free=%" PRIu32 " status_seen=%u age=%" PRIu64,
            local_diag.heap_free,local_diag.heap_min,local_diag.heap_largest,local_diag.stack_free,local_diag.status_age_valid,local_diag.status_age_ms);
#endif
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
void app_main(void) {
    ESP_LOGW(TAG,"PHASE 2C TIMING BENCH / NO RF / NO PANEL / NO TOUCH");
    ESP_LOGI(TAG,"PSRAM initialized=%d detected=%u bytes (hardware remains unverified)",
        esp_psram_is_initialized(),(unsigned)esp_psram_get_size());
    uint64_t id=fresh_boot_id();
    publications=xQueueCreate(1,sizeof(ui_publication_t));if(!publications)abort();
    uart_ready=init_uart();
    tyvor_async_tx_init(&tx_queue,tx_start,tx_state,tx_abort,NULL);
    tyvor_bench_transport_init(&uart_transport,write_uart,NULL,uart_ready);
    tyvor_diagnostics_init(&diagnostics,TYVOR_ROLE_UI,(uint32_t)esp_reset_reason(),0,1);
    diagnostics.watchdog_configured=CONFIG_ESP_TASK_WDT_EN;
    if (tyvor_ui_endpoint_init(&ui_endpoint,id,tyvor_bench_transport_write,&uart_transport,millis())) abort();
    if (xTaskCreate(bridge_task,"tyvor_bridge",4096,NULL,5,&bridge_handle)!=pdPASS) abort();
    if (xTaskCreate(diagnostics_task,"tyvor_diag",4096,NULL,2,NULL)!=pdPASS) abort();
}
