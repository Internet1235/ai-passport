#include "starbridge_audio.h"
#include "sdkconfig.h"
#include "bsp_audio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

_Static_assert(CONFIG_I2S_ISR_IRAM_SAFE, "Continuous audio requires I2S IRAM-safe interrupts");

typedef struct { unsigned volume; bool playing; sb_sound effect; } audio_command;
static QueueHandle_t commands;
static sb_audio_stats stats;
static portMUX_TYPE stats_lock = portMUX_INITIALIZER_UNLOCKED;
static unsigned initial_volume;

sb_audio_stats sb_audio_status(void) {
    portENTER_CRITICAL(&stats_lock);
    sb_audio_stats copy = stats;
    portEXIT_CRITICAL(&stats_lock);
    return copy;
}

static void audio_worker(void *arg) {
    (void)arg;
    sb_synth synth; sb_synth_init(&synth);
    sb_synth_volume(&synth, initial_volume, false);
    esp_err_t result = bsp_audio_init();
    if (result == ESP_OK) result = bsp_audio_set_format(SB_SOUND_RATE, 16, 1);
    if (result == ESP_OK) bsp_audio_set_volume(85);
    portENTER_CRITICAL(&stats_lock);
    stats.ready = result == ESP_OK; stats.failed = result != ESP_OK;
    stats.volume = initial_volume;
    portEXIT_CRITICAL(&stats_lock);
    if (result != ESP_OK) { vTaskDelete(NULL); return; }

    int16_t pcm[256];
    int64_t previous = 0;
    for (;;) {
        audio_command command;
        while (xQueueReceive(commands, &command, 0) == pdTRUE) {
            sb_synth_volume(&synth, command.volume, command.playing);
            sb_synth_effect(&synth, command.effect);
            portENTER_CRITICAL(&stats_lock);
            stats.volume = command.volume;
            if (command.volume && command.effect > SB_SILENT && command.effect < SB_SOUND_COUNT)
                ++stats.effects[command.effect];
            portEXIT_CRITICAL(&stats_lock);
        }
        int64_t begin = esp_timer_get_time();
        sb_synth_render(&synth, pcm, 256);
        int64_t rendered = esp_timer_get_time();
        unsigned peak = 0;
        for (unsigned i = 0; i < 256; ++i) {
            unsigned magnitude = (unsigned)(pcm[i] < 0 ? -pcm[i] : pcm[i]);
            if (magnitude > peak) peak = magnitude;
        }
        // Blocking DMA writes pace the worker; no UI lock or allocation in this loop.
        result = bsp_audio_write(pcm, sizeof(pcm));
        unsigned gap = previous ? (unsigned)(begin - previous) : 0;
        previous = begin;
        portENTER_CRITICAL(&stats_lock);
        ++stats.blocks;
        stats.peak = peak;
        if (gap > stats.max_gap_us) stats.max_gap_us = gap;
        if (gap > 60000) ++stats.late_feeds;
        if ((unsigned)(rendered - begin) > stats.max_render_us) stats.max_render_us = (unsigned)(rendered - begin);
        if (result != ESP_OK) { stats.failed = true; stats.ready = false; }
        portEXIT_CRITICAL(&stats_lock);
        if (result != ESP_OK) { vTaskDelete(NULL); return; }
    }
}

bool sb_audio_start(unsigned volume) {
    initial_volume = volume;
    commands = xQueueCreate(16, sizeof(audio_command));
    if (!commands) { stats.failed = true; return false; }
    if (xTaskCreate(audio_worker, "sb_audio", 4096, NULL, 6, NULL) != pdPASS) {
        vQueueDelete(commands); commands = NULL; stats.failed = true; return false;
    }
    return true;
}

void sb_audio_update(unsigned volume, bool playing, sb_sound effect) {
    audio_command command = {.volume = volume, .playing = playing, .effect = effect};
    if (commands && xQueueSend(commands, &command, 0) != pdTRUE) {
        portENTER_CRITICAL(&stats_lock); ++stats.dropped; portEXIT_CRITICAL(&stats_lock);
    }
}
