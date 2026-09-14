#pragma once
#include "starbridge_sound.h"

typedef struct {
    bool ready, failed;
    unsigned volume, blocks, max_gap_us, max_render_us, late_feeds, dropped, peak;
    unsigned effects[SB_SOUND_COUNT];
} sb_audio_stats;

bool sb_audio_start(unsigned volume);
void sb_audio_update(unsigned volume, bool playing, sb_sound effect);
sb_audio_stats sb_audio_status(void);
