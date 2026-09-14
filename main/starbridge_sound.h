#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SB_SOUND_RATE 16000
// Keep diagnostic IDs stable across firmware revisions.
typedef enum { SB_SILENT = 0, SB_TURN = 2, SB_CONNECT = 3,
               SB_WIN = 7, SB_SOUND_COUNT = 8 } sb_sound;
typedef struct {
    uint32_t phase, increment, age;
    int32_t envelope, gain;
} sb_voice;
typedef struct {
    sb_voice voices[6];
    uint32_t position, effect_age;
    unsigned effect_step;
    sb_sound effect;
    int master, target, background, background_target;
} sb_synth;

void sb_synth_init(sb_synth *s);
void sb_synth_volume(sb_synth *s, unsigned level, bool playing);
void sb_synth_effect(sb_synth *s, sb_sound effect);
void sb_synth_render(sb_synth *s, int16_t *pcm, size_t samples);
