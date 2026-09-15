#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "walkman_decoder.h"
#define WM_RATE 22050u
#define WM_SAVE_SIZE 8

typedef struct { const char *title, *caption; uint32_t offset, samples, bytes; unsigned cue_start, cue_count; } wm_track;
typedef struct { unsigned ms; const char *text; } wm_cue;
extern const wm_cue wm_cues[];
extern const wm_track wm_tracks[];
int wm_active_cue(unsigned track, uint32_t position);
extern const unsigned wm_track_count;
#ifdef WM_HOST_PACK_POINTER
extern const uint8_t *wm_pack;
#elif defined(ESP_PLATFORM)
extern const uint8_t wm_pack[] asm("_binary_audio_bin_start");
#else
extern const uint8_t wm_pack[];
#endif

typedef struct {
    unsigned track, volume, repeat, timer_minutes;
    bool playing;
    uint32_t position, timer_left;
    int predictor, step_index;
    int gain;
} wm_player;
void wm_init(wm_player *p);
void wm_select(wm_player *p, int direction);
void wm_timer(wm_player *p, unsigned minutes);
void wm_render(wm_player *p, wm_decoder *decoder, int16_t *pcm, size_t count);
int16_t wm_adpcm(unsigned code, int *predictor, int *index);
void wm_save(const wm_player *p, uint8_t data[WM_SAVE_SIZE]);
bool wm_load(wm_player *p, const uint8_t *data, size_t length);
