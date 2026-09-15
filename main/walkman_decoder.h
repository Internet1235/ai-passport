#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "vendor/helix/mp3dec.h"
typedef struct {
    HMP3Decoder handle;
    int16_t pcm[MAX_NCHAN*MAX_NGRAN*MAX_NSAMP];
    unsigned track, offset, available, cursor;
    uint32_t position;
    bool initialized, failed;
} wm_decoder;
int16_t wm_sample(wm_decoder *d, unsigned track, uint32_t position);
void wm_decoder_close(wm_decoder *d);
