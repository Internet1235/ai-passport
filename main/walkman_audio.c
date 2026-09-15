#include "walkman_audio.h"
#include "walkman_online.h"
#include "bsp_audio.h"
#include "esp_timer.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
_Static_assert(CONFIG_I2S_ISR_IRAM_SAFE, "Audio playback needs IRAM-safe I2S");
// EMBED_FILES exposes this file's bytes without loading the pack into RAM.

typedef struct { unsigned kind, value; } command;
static QueueHandle_t commands;
static wa_state state;
static wm_decoder decoder;
static portMUX_TYPE guard = portMUX_INITIALIZER_UNLOCKED;
wa_state wa_status(void) { portENTER_CRITICAL(&guard); wa_state s=state; portEXIT_CRITICAL(&guard); return s; }
void wa_command(unsigned kind, unsigned value) {
    command c={kind,value};
    if (!commands || xQueueSend(commands,&c,0)!=pdTRUE) {
        portENTER_CRITICAL(&guard); ++state.dropped; portEXIT_CRITICAL(&guard);
    }
}
static void worker(void *arg) {
    (void)arg;
    wm_player player=state.player;
    esp_err_t err=bsp_audio_init();
    if(err==ESP_OK) err=bsp_audio_set_format(WM_RATE,16,1);
    if(err==ESP_OK) bsp_audio_set_volume(85);
    portENTER_CRITICAL(&guard); state.ready=err==ESP_OK; state.failed=err!=ESP_OK; portEXIT_CRITICAL(&guard);
    if(err!=ESP_OK) { vTaskDelete(NULL); return; }
    bool network=false;
    int16_t pcm[256]; int16_t last_sample=0; int64_t previous=0;
    for (;;) {
        command c;
        while(xQueueReceive(commands,&c,0)==pdTRUE) {
            if(player.playing && (c.kind==WA_TOGGLE || c.kind==WA_PREV || c.kind==WA_NEXT || c.kind==WA_NETWORK)) {
                for(unsigned i=0;i<256;++i) pcm[i]=(int16_t)((int)last_sample*(255-(int)i)/256);
                if(bsp_audio_write(pcm,sizeof(pcm))!=ESP_OK) {
                    portENTER_CRITICAL(&guard); state.ready=false; state.failed=true; portEXIT_CRITICAL(&guard);
                    vTaskDelete(NULL); return;
                }
                last_sample=0;
            }
            switch(c.kind) {
            case WA_TOGGLE: player.playing=!player.playing; break;
            case WA_PREV: wm_select(&player,-1); break;
            case WA_NEXT: wm_select(&player,1); break;
            case WA_VOLUME: player.volume=c.value<=4?c.value:4; break;
            case WA_REPEAT: player.repeat=c.value%3; break;
            case WA_NETWORK: network=c.value!=0;player.playing=false;wm_decoder_close(&decoder);player.position=0;break;
            case WA_TIMER: wm_timer(&player,c.value); break;
            }
        }
        if(network) {
            bool ok=wn_audio_step(player.volume);
            portENTER_CRITICAL(&guard);state.player=player;state.network=true;if(!ok)state.failed=true;portEXIT_CRITICAL(&guard);
            previous=0;vTaskDelay(1);continue;
        }
        bsp_audio_set_format(WM_RATE,16,1);
        int64_t now=esp_timer_get_time();
        wm_render(&player,&decoder,pcm,256);
        unsigned render_us=(unsigned)(esp_timer_get_time()-now);
        last_sample=pcm[255];
        unsigned peak=0;
        for(unsigned i=0;i<256;++i) { unsigned v=pcm[i]<0?-pcm[i]:pcm[i]; if(v>peak) peak=v; }
        err=bsp_audio_write(pcm,sizeof(pcm));
        unsigned gap=previous?(unsigned)(now-previous):0; previous=now;
        portENTER_CRITICAL(&guard);
        state.player=player;state.network=false; ++state.blocks; state.peak=peak;
        if(render_us>state.max_render_us) state.max_render_us=render_us;
        state.stack_free=(unsigned)uxTaskGetStackHighWaterMark(NULL);
        if(gap>state.max_gap_us) state.max_gap_us=gap;
        if(err!=ESP_OK || decoder.failed) { state.ready=false; state.failed=true; }
        portEXIT_CRITICAL(&guard);
        if(err!=ESP_OK || decoder.failed) { wm_decoder_close(&decoder); vTaskDelete(NULL); return; }
        // Even a pathological decode must not starve input or the idle watchdog.
        vTaskDelay(1);
    }
}
bool wa_start(const wm_player *initial) {
    state.player=*initial;
    commands=xQueueCreate(24,sizeof(command));
    if(!commands) { state.failed=true; return false; }
    if(xTaskCreate(worker,"walkman_audio",6144,NULL,6,NULL)!=pdPASS) {
        vQueueDelete(commands); commands=NULL; state.failed=true; return false;
    }
    return true;
}
