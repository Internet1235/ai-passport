#include "starbridge_sound.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    sb_synth synth; sb_synth_init(&synth);
    int16_t pcm[256];
    sb_synth_render(&synth, pcm, 256);
    for (unsigned i = 0; i < 256; ++i) assert(pcm[i] == 0);
    sb_synth_volume(&synth, 4, true);
    int peak = 0; long long energy = 0, total = 0;
    for (unsigned block = 0; block < 7500; ++block) {
        if (!(block % 19)) sb_synth_effect(&synth, (sb_sound)(1 + (block / 19) % 7));
        sb_synth_render(&synth, pcm, 256);
        for (unsigned i = 0; i < 256; ++i) {
            int value = abs(pcm[i]);
            if (value > peak) peak = value;
            energy += (long long)pcm[i] * pcm[i]; total += pcm[i];
            assert(value < 26000); // Headroom remains at maximum volume with overlapping chimes.
        }
    }
    assert(peak > 1000 && energy > 1000000000LL);
    assert(llabs(total) < 7500LL * 256 * 10); // No significant DC offset.
    sb_synth_volume(&synth, 0, false);
    sb_synth_render(&synth, pcm, 256);
    sb_synth_render(&synth, pcm, 256);
    for (unsigned i = 0; i < 256; ++i) assert(pcm[i] == 0);
    sb_synth_effect(&synth, SB_WIN);
    sb_synth_render(&synth, pcm, 256);
    for (unsigned i = 0; i < 256; ++i) assert(pcm[i] == 0);
    // Rendering remains sample-identical regardless of streaming block boundaries.
    sb_synth a, b; sb_synth_init(&a); sb_synth_init(&b);
    sb_synth_volume(&a, 1, true); sb_synth_volume(&b, 1, true);
    int16_t whole[1024], split[1024];
    sb_synth_render(&a, whole, 1024);
    for (unsigned i = 0; i < 1024; i += 64) sb_synth_render(&b, split + i, 64);
    assert(memcmp(whole, split, sizeof(whole)) == 0);
    printf("Starbridge audio: 120 s mixed playback, headroom, DC, mute and chunking PASS (peak=%d)\n", peak);
    return 0;
}
