#include "walkman_decoder.h"
#include "walkman.h"
#include <string.h>
void wm_decoder_close(wm_decoder *d) {
    if(d->handle) MP3FreeDecoder(d->handle);
    memset(d,0,sizeof(*d));
}
int16_t wm_sample(wm_decoder *d,unsigned track,uint32_t position) {
    if(!d->initialized || d->track!=track || d->position!=position) {
        wm_decoder_close(d); d->handle=MP3InitDecoder(); d->track=track; d->initialized=true;
        if(!d->handle || position) { d->failed=true; return 0; }
    }
    const wm_track *t=&wm_tracks[track];
    if(d->cursor>=d->available) {
        if(d->offset>=t->bytes) { d->failed=true; return 0; }
        unsigned char *input=(unsigned char *)(wm_pack+t->offset+d->offset);
        int remaining=(int)(t->bytes-d->offset), before=remaining;
        int err=MP3Decode(d->handle,&input,&remaining,d->pcm,0);
        if(err || remaining>=before || remaining<0) { d->failed=true; return 0; }
        MP3FrameInfo info; MP3GetLastFrameInfo(d->handle,&info);
        if(info.samprate!=WM_RATE || info.nChans!=1 || info.outputSamps<=0 || info.outputSamps>MAX_NCHAN*MAX_NGRAN*MAX_NSAMP) { d->failed=true; return 0; }
        d->offset+=(unsigned)(before-remaining);d->available=(unsigned)info.outputSamps;d->cursor=0;
    }
    ++d->position;return d->pcm[d->cursor++];
}
