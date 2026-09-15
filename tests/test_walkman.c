#include "walkman.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
const wm_track wm_tracks[]={{"A","",0,8,4,0,2},{"B","",4,8,4,0,0}};
const unsigned wm_track_count=2;
const wm_cue wm_cues[]={{0,"first"},{1000,"next"}};
const uint8_t wm_pack[]={0x77,0x77,0xff,0xff,0x12,0x34,0x56,0x78};
int16_t wm_sample(wm_decoder *d,unsigned track,uint32_t position) {
    if(!d->initialized || d->track!=track || d->position!=position) {
        memset(d,0,sizeof(*d)); d->track=track; d->initialized=true;
    }
    ++d->position;
    return (int16_t)((position%4)*1000);
}
int main(void) {
    wm_decoder decoder={0}, other={0};
    assert(wm_active_cue(0,0)==0 && wm_active_cue(0,WM_RATE)==1 && wm_active_cue(1,0)==-1);
    wm_player p; wm_init(&p); assert(!p.playing && p.volume==3);
    wm_select(&p,-1); assert(p.track==1); wm_select(&p,1); assert(p.track==0);
    int pred=0,index=0;
    assert(wm_adpcm(7,&pred,&index)==11 && index==8);
    assert(wm_adpcm(7,&pred,&index)==41 && index==16);
    for(unsigned i=0;i<1000;++i) wm_adpcm(7,&pred,&index);
    assert(pred==32767 && index==88);
    for(unsigned i=0;i<1000;++i) wm_adpcm(15,&pred,&index);
    assert(pred==-32768 && index==88);
    int16_t pcm[64]; p.playing=true; wm_render(&p,&decoder,pcm,9); assert(p.track==1 && p.position==1);
    p.repeat=1; wm_render(&p,&decoder,pcm,16); assert(p.track==1 && p.position==1);
    p.repeat=2; wm_render(&p,&decoder,pcm,16); assert(!p.playing && p.position==0);
    p.volume=0; p.playing=true; p.repeat=0; wm_render(&p,&decoder,pcm,64);
    for(unsigned i=0;i<64;++i) assert(pcm[i]==0);
    wm_timer(&p,15); p.timer_left=20; p.playing=false; wm_render(&p,&decoder,pcm,64); assert(p.timer_left==20);
    p.playing=true; wm_render(&p,&decoder,pcm,64); assert(!p.playing && !p.timer_minutes && !p.timer_left);
    wm_init(&p); p.playing=true; p.volume=4;
    wm_player q=p; int16_t a[64],b[64]; wm_render(&p,&decoder,a,64);
    wm_render(&q,&other,b,11); wm_render(&q,&other,b+11,53); assert(!memcmp(a,b,sizeof(a)) && p.position==q.position);
    uint8_t saved[WM_SAVE_SIZE]; wm_save(&p,saved); assert(wm_load(&q,saved,sizeof(saved)) && !q.playing && q.volume==4);
    for(unsigned i=0;i<WM_SAVE_SIZE;++i) { saved[i]^=1; assert(!wm_load(&q,saved,sizeof(saved))); saved[i]^=1; }
    assert(!wm_load(&q,saved,7));
    for(unsigned i=0;i<100000;++i) { wm_select(&p,(i&1)?-1:1); wm_render(&p,&decoder,pcm,64); assert(p.track<2 && p.step_index<=88 && p.step_index>=0); }
    puts("Walkman: decoder limits, navigation, end modes, pause, mute, timer, chunk invariance, persistence PASS");
}
