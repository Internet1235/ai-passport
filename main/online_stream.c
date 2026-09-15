#include "online_stream.h"
#include <string.h>
static bool append(online_stream *s,char c) {
    if(s->used+1>=s->capacity){s->error=ONLINE_STREAM_CAPACITY;return false;}
    s->json[s->used++]=c;return true;
}
static int b64(unsigned char c) {
    if(c>='A' && c<='Z')return c-'A';
    if(c>='a' && c<='z')return c-'a'+26;
    if(c>='0' && c<='9')return c-'0'+52;
    return c=='+'?62:c=='/'?63:c=='='?64:-1;
}
static bool flush(online_stream *s) {
    if(s->pcm_used%2){s->error=ONLINE_STREAM_PCM;return false;}
    if(s->pcm_used && !s->sink(s->pcm,s->pcm_used,s->context)){s->error=ONLINE_STREAM_SINK;return false;}
    s->pcm_used=0;return true;
}
static bool sample(online_stream *s,unsigned char c) {
    int value=b64(c);if(value<0 || s->padded){s->error=ONLINE_STREAM_BASE64;return false;}
    s->quartet[s->quartet_count++]=(uint8_t)value;
    if(s->quartet_count<4)return true;
    uint8_t *q=s->quartet;s->quartet_count=0;
    if(q[0]>=64 || q[1]>=64 || (q[2]==64 && q[3]!=64)){s->error=ONLINE_STREAM_BASE64;return false;}
    s->pcm[s->pcm_used++]=(uint8_t)((q[0]<<2)|(q[1]>>4));
    if(q[2]<64)s->pcm[s->pcm_used++]=(uint8_t)((q[1]<<4)|(q[2]>>2));
    if(q[3]<64)s->pcm[s->pcm_used++]=(uint8_t)((q[2]<<6)|q[3]);
    s->padded=q[3]==64;
    return s->pcm_used<sizeof(s->pcm) || flush(s);
}
void online_stream_begin(online_stream *s,char *json,size_t capacity,online_pcm_sink sink,void *context) {
    memset(s,0,sizeof(*s));s->json=json;s->capacity=capacity;s->sink=sink;s->context=context;
}
static bool byte(online_stream *s,char c) {
    if(s->stripping) {
        if(s->escape){s->escape=false;return c=='/' && sample(s,'/');}
        if(c=='\\'){s->escape=true;return true;}
        if(c=='"') {
            if(s->quartet_count || !flush(s))return false;
            s->stripping=false;s->string=false;s->phase=3;return append(s,c);
        }
        return sample(s,(unsigned char)c);
    }
    if(!append(s,c))return false;
    if(s->string) {
        if(s->escape){s->escape=false;return true;}
        if(c=='\\'){s->escape=true;return true;}
        if(c=='"') {
            s->string=false;
            if(s->depth==1) {
                size_t n=s->used-s->start;
                if(s->phase==0){s->key=n==6 && !memcmp(s->json+s->start,"\"type\"",6)?1:n==7 && !memcmp(s->json+s->start,"\"delta\"",7)?2:0;s->phase=1;}
                else if(s->phase==2){if(s->key==1)s->audio=n==22 && !memcmp(s->json+s->start,"\"response.audio.delta\"",22);s->phase=3;}
            }
        }
        return true;
    }
    if(c=='"') {
        s->string=true;s->start=s->used-1;
        if(s->depth==1 && s->phase==2 && s->key==2 && s->audio){s->stripping=true;s->padded=false;}
    } else if(c=='{' || c=='[') {
        if(++s->depth>8)return false;
    } else if(c=='}' || c==']') {
        if(!s->depth)return false;
        if(--s->depth==1)s->phase=3;
    } else if(s->depth==1) {
        if(c==':' && s->phase==1)s->phase=2;
        else if(c==',')s->phase=0;
        else if(s->phase==2 && c!=' ' && c!='\t' && c!='\r' && c!='\n')s->phase=3;
    }
    return true;
}
bool online_stream_feed(online_stream *s,const char *bytes,size_t length) {
    if(s->failed)return false;
    for(size_t i=0;i<length;++i)if(!byte(s,bytes[i])){if(!s->error)s->error=ONLINE_STREAM_JSON;s->failed=true;return false;}
    return true;
}
bool online_stream_end(online_stream *s) {
    if(s->failed || s->string || s->depth || s->used>=s->capacity){if(!s->error)s->error=ONLINE_STREAM_JSON;return false;}
    s->json[s->used]=0;return true;
}
