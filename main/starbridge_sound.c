#include "starbridge_sound.h"
#include <string.h>

// Original 20-second music-box miniature, 96 BPM. No recordings or third-party music.
// Integer synthesis keeps the ESP32-C3 producer bounded without floating point.
static const int16_t sine[256] = {0, 804, 1608, 2410, 3212, 4011, 4808, 5602, 6393, 7179, 7962, 8739, 9512, 10278, 11039, 11793, 12539, 13279, 14010, 14732, 15446, 16151, 16846, 17530, 18204, 18868, 19519, 20159, 20787, 21403, 22005, 22594, 23170, 23731, 24279, 24811, 25329, 25832, 26319, 26790, 27245, 27683, 28105, 28510, 28898, 29268, 29621, 29956, 30273, 30571, 30852, 31113, 31356, 31580, 31785, 31971, 32137, 32285, 32412, 32521, 32609, 32678, 32728, 32757, 32767, 32757, 32728, 32678, 32609, 32521, 32412, 32285, 32137, 31971, 31785, 31580, 31356, 31113, 30852, 30571, 30273, 29956, 29621, 29268, 28898, 28510, 28105, 27683, 27245, 26790, 26319, 25832, 25329, 24811, 24279, 23731, 23170, 22594, 22005, 21403, 20787, 20159, 19519, 18868, 18204, 17530, 16846, 16151, 15446, 14732, 14010, 13279, 12539, 11793, 11039, 10278, 9512, 8739, 7962, 7179, 6393, 5602, 4808, 4011, 3212, 2410, 1608, 804, 0, -804, -1608, -2410, -3212, -4011, -4808, -5602, -6393, -7179, -7962, -8739, -9512, -10278, -11039, -11793, -12539, -13279, -14010, -14732, -15446, -16151, -16846, -17530, -18204, -18868, -19519, -20159, -20787, -21403, -22005, -22594, -23170, -23731, -24279, -24811, -25329, -25832, -26319, -26790, -27245, -27683, -28105, -28510, -28898, -29268, -29621, -29956, -30273, -30571, -30852, -31113, -31356, -31580, -31785, -31971, -32137, -32285, -32412, -32521, -32609, -32678, -32728, -32757, -32767, -32757, -32728, -32678, -32609, -32521, -32412, -32285, -32137, -31971, -31785, -31580, -31356, -31113, -30852, -30571, -30273, -29956, -29621, -29268, -28898, -28510, -28105, -27683, -27245, -26790, -26319, -25832, -25329, -24811, -24279, -23731, -23170, -22594, -22005, -21403, -20787, -20159, -19519, -18868, -18204, -17530, -16846, -16151, -15446, -14732, -14010, -13279, -12539, -11793, -11039, -10278, -9512, -8739, -7962, -7179, -6393, -5602, -4808, -4011, -3212, -2410, -1608, -804};
static const uint32_t pitch[37] = {35114789u, 37202823u, 39415018u, 41758757u, 44241862u, 46872620u, 49659811u, 52612737u, 55741253u, 59055800u, 62567441u, 66287895u, 70229578u, 74405646u, 78830036u, 83517514u, 88483724u, 93745240u, 99319622u, 105225474u, 111482506u, 118111601u, 125134882u, 132575789u, 140459156u, 148811292u, 157660072u, 167035027u, 176967447u, 187490479u, 198639243u, 210450947u, 222965012u, 236223201u, 250269764u, 265151578u, 280918312u};
static const uint8_t melody[64] = {
    72,0,76,0,79,0,76,0, 74,0,76,79,81,0,79,0,
    76,0,74,0,72,0,69,0, 74,0,76,0,79,0,0,0,
    81,0,79,0,76,0,79,0, 74,0,72,74,76,0,0,0,
    69,0,72,0,74,0,76,0, 74,0,72,0,0,0,0,0
};
static const uint8_t chords[8][4] = {
    {48,60,64,67}, {55,62,67,71}, {53,60,65,69}, {55,62,67,71},
    {57,60,64,69}, {52,59,64,67}, {53,60,65,69}, {48,60,64,67}
};
static const uint8_t effects[SB_SOUND_COUNT][5] = {
    [SB_TURN] = {72,79,0},
    [SB_CONNECT] = {76,79,84,0},
    [SB_WIN] = {72,76,79,84,0}
};

static void note(sb_voice *v, unsigned midi, int gain) {
    if (midi < 48 || midi > 84) return;
    *v = (sb_voice){.increment = pitch[midi - 48], .envelope = 32767, .gain = gain};
}

void sb_synth_init(sb_synth *s) { memset(s, 0, sizeof(*s)); }

void sb_synth_volume(sb_synth *s, unsigned level, bool playing) {
    s->target = (int)(level > 4 ? 4 : level) * 64;
    s->background_target = playing ? 256 : 112;
}

void sb_synth_effect(sb_synth *s, sb_sound effect) {
    if ((effect != SB_TURN && effect != SB_CONNECT && effect != SB_WIN) || !s->target) return;
    s->effect = effect; s->effect_age = 0; s->effect_step = 0;
}

static int sample(sb_voice *v) {
    if (!v->envelope) return 0;
    unsigned index = v->phase >> 24;
    int wave = sine[index] + sine[(index * 2) & 255] / 5;
    int envelope = v->envelope;
    if (v->age < 128) envelope = envelope * (int)v->age / 128;
    int value = ((wave * envelope) / 32768) * v->gain / 32768;
    v->phase += v->increment;
    if (v->age < 128) ++v->age;
    v->envelope = v->envelope * 65524 / 65536;
    return value;
}

void sb_synth_render(sb_synth *s, int16_t *pcm, size_t samples) {
    for (size_t i = 0; i < samples; ++i) {
        if (s->position % 5000 == 0) {
            unsigned step = s->position / 5000;
            const uint8_t *chord = chords[step / 8];
            if (melody[step]) note(&s->voices[0], melody[step], 1450);
            if (!(step % 2)) note(&s->voices[1], chord[1 + (step / 2) % 3], 600);
            if (!(step % 8)) note(&s->voices[2], chord[0], 800);
        }
        if (s->effect && s->effect_age % 2000 == 0) {
            unsigned midi = effects[s->effect][s->effect_step];
            if (!midi) s->effect = SB_SILENT;
            else {
                note(&s->voices[3 + s->effect_step % 3], midi,
                     s->effect == SB_TURN ? 588 : 2400);
                ++s->effect_step;
            }
        }
        if (s->effect) ++s->effect_age;
        int background = 0, foreground = 0;
        for (unsigned v = 0; v < 3; ++v) background += sample(&s->voices[v]);
        for (unsigned v = 3; v < 6; ++v) foreground += sample(&s->voices[v]);
        // Fade gain changes in 16 ms and duck music under the short chimes.
        s->master += (s->master < s->target) - (s->master > s->target);
        int desired = s->background_target / (s->effect ? 2 : 1);
        s->background += (s->background < desired) - (s->background > desired);
        background = background * s->background / 256;
        int value = (background + foreground) * s->master * 3 / 256;
        pcm[i] = (int16_t)(value > 30000 ? 30000 : value < -30000 ? -30000 : value);
        s->position = (s->position + 1) % 320000;
    }
}
