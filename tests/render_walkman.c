#include "walkman.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
const uint8_t *wm_pack;
int main(int argc,char **argv) {
    if(argc!=3) return 2;
    FILE *pack=fopen(argv[1],"rb"); if(!pack) return 2;
    fseek(pack,0,SEEK_END); long bytes=ftell(pack);rewind(pack);
    uint8_t *data=malloc((size_t)bytes);if(!data || fread(data,1,(size_t)bytes,pack)!=(size_t)bytes) return 2;
    fclose(pack);wm_pack=data;
    static wm_decoder decoder; int16_t pcm[256];
    for(unsigned track=0;track<wm_track_count;++track) {
        const wm_track *t=&wm_tracks[track];
        if((uint64_t)t->offset+t->bytes>(uint64_t)bytes || !t->samples) return 3;
        wm_decoder_close(&decoder);
        char path[1024]; snprintf(path,sizeof(path),"%s/track-%02u.raw",argv[2],track);
        FILE *out=fopen(path,"wb");if(!out) return 2;
        for(uint32_t position=0;position<t->samples;) {
            unsigned n=t->samples-position; if(n>256) n=256;
            for(unsigned i=0;i<n;++i) pcm[i]=wm_sample(&decoder,track,position++);
            if(decoder.failed) { fprintf(stderr,"Decode failure at track %u position %lu\n",track,(unsigned long)position);return 4; }
            if(fwrite(pcm,sizeof(int16_t),n,out)!=n) return 2;
        }
        fclose(out);
        printf("track %u: %lu samples PASS\n",track,(unsigned long)t->samples);
    }
    wm_decoder_close(&decoder);free(data);return 0;
}
