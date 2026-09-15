#include "online_upload.h"
#include <string.h>
static void encode(online_upload *s,unsigned count) {
    static const char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    unsigned a=s->carry[0],b=count>1?s->carry[1]:0,c=count>2?s->carry[2]:0;
    s->json[s->used++]=alphabet[a>>2];
    s->json[s->used++]=alphabet[((a&3)<<4)|(b>>4)];
    s->json[s->used++]=count>1?alphabet[((b&15)<<2)|(c>>6)]:'=';
    s->json[s->used++]=count>2?alphabet[c&63]:'=';
    s->carry_used=0;
}
void online_upload_begin(online_upload *s) {
    static const char prefix[]="{\"type\":\"input_audio_buffer.append\",\"audio\":\"";
    s->samples_bytes=0;s->carry_used=0;s->finished=false;
    s->used=sizeof(prefix)-1;memcpy(s->json,prefix,sizeof(prefix)-1);
}
bool online_upload_append(online_upload *s,const uint8_t *pcm,size_t length) {
    if(s->finished || length>ONLINE_UPLOAD_PCM_MAX-s->samples_bytes)return false;
    s->samples_bytes+=length;
    for(size_t i=0;i<length;++i){s->carry[s->carry_used++]=pcm[i];if(s->carry_used==3)encode(s,3);}
    return true;
}
size_t online_upload_finish(online_upload *s) {
    if(!s->samples_bytes || s->samples_bytes%2)return 0;
    if(!s->finished){if(s->carry_used)encode(s,s->carry_used);s->json[s->used++]='"';s->json[s->used++]='}';s->json[s->used]=0;s->finished=true;}
    return s->used;
}
