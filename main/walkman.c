#include "walkman.h"
#include <string.h>
static const int steps[89] = {7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767};
static const int delta[8] = {-1,-1,-1,-1,2,4,6,8};
int16_t wm_adpcm(unsigned code, int *pred, int *index) {
    code &= 15;
    int step = steps[*index];
    int d = step / 8 + ((code & 1) ? step / 4 : 0) + ((code & 2) ? step / 2 : 0) + ((code & 4) ? step : 0);
    *pred += (code & 8) ? -d : d;
    if (*pred > 32767) *pred = 32767;
    if (*pred < -32768) *pred = -32768;
    *index += delta[code & 7];
    if (*index < 0) *index = 0;
    if (*index > 88) *index = 88;
    return (int16_t)*pred;
}
static void rewind_track(wm_player *p) { p->position = 0; p->predictor = p->step_index = p->gain = 0; }
void wm_init(wm_player *p) { memset(p, 0, sizeof(*p)); p->volume = 3; }
void wm_select(wm_player *p,int direction) {
    p->track=direction<0?(p->track+wm_track_count-1)%wm_track_count:(p->track+1)%wm_track_count;
    rewind_track(p);
}
void wm_timer(wm_player *p, unsigned minutes) {
    p->timer_minutes = minutes <= 60 ? minutes : 60;
    p->timer_left = p->timer_minutes * 60 * WM_RATE;
}
void wm_render(wm_player *p, wm_decoder *decoder, int16_t *pcm, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        if (!p->playing) { pcm[i] = 0; p->gain = 0; continue; }
        if (p->timer_minutes && !p->timer_left) {
            p->playing = false; p->timer_minutes = 0; pcm[i] = 0; continue;
        }
        const wm_track *t = &wm_tracks[p->track];
        if (p->position >= t->samples) {
            if (p->repeat == 2 && p->track+1==wm_track_count) {
                p->playing = false; rewind_track(p); pcm[i] = 0; continue;
            }
            if (p->repeat == 1) rewind_track(p); else wm_select(p, 1);
            t = &wm_tracks[p->track];
        }
        int sample = wm_sample(decoder, p->track, p->position);
        if (decoder->failed) { p->playing = false; pcm[i] = 0; continue; }
        ++p->position;
        int target = (int)p->volume * 256;
        if (p->gain < target) p->gain += 1;
        if (p->gain > target) p->gain -= 1;
        // Short fades at both track boundaries and the sleep timer deadline.
        uint32_t fade = WM_RATE / 20, remaining = t->samples - p->position;
        if (remaining < fade) sample = sample * (int)remaining / (int)fade;
        if (p->timer_minutes) {
            if (p->timer_left < WM_RATE) sample = sample * (int)p->timer_left / (int)WM_RATE;
            --p->timer_left;
        }
        pcm[i] = (int16_t)(sample * p->gain / 1024);
    }
}
void wm_save(const wm_player *p, uint8_t d[WM_SAVE_SIZE]) {
    d[0]='W'; d[1]='M'; d[2]=1; d[3]=(uint8_t)p->track; d[4]=(uint8_t)p->volume; d[5]=(uint8_t)p->repeat; d[6]=0;
    d[7]=0; for(unsigned i=0;i<7;++i) d[7]^=d[i];
}
bool wm_load(wm_player *p, const uint8_t *d, size_t n) {
    if (n != WM_SAVE_SIZE) return false;
    unsigned sum=0; for(unsigned i=0;i<8;++i) sum^=d[i];
    if(sum || d[0]!='W' || d[1]!='M' || d[2]!=1 || d[3]>=wm_track_count || d[4]>4 || d[5]>2 || d[6]!=0) return false;
    wm_init(p); p->track=d[3]; p->volume=d[4]; p->repeat=d[5];
    return true;
}

int wm_active_cue(unsigned track,uint32_t position) {
    if(track>=wm_track_count || !wm_tracks[track].cue_count) return -1;
    const wm_track *t=&wm_tracks[track];unsigned ms=(unsigned)((uint64_t)position*1000/WM_RATE),selected=0;
    for(unsigned i=1;i<t->cue_count;++i) { if(wm_cues[t->cue_start+i].ms>ms) break; selected=i; }
    return (int)selected;
}
