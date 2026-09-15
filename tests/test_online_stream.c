#include "online_stream.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t decoded[40000];static size_t decoded_size;
static bool collect(const uint8_t *pcm,size_t n,void *unused) {
    (void)unused;assert(n%2==0 && decoded_size+n<=sizeof(decoded));memcpy(decoded+decoded_size,pcm,n);decoded_size+=n;return true;
}
static bool congested(const uint8_t *pcm,size_t n,void *unused) {
    (void)pcm;(void)n;(void)unused;return false;
}
static void check(const char *raw,const char *expected,size_t bytes) {
    for(size_t step=1;step<=2048;step=step<8?step+1:step*2) {
        char compact[1024];online_stream s;decoded_size=0;online_stream_begin(&s,compact,sizeof(compact),collect,NULL);
        for(size_t at=0,n=strlen(raw);at<n;){size_t take=n-at;if(take>step)take=step;assert(online_stream_feed(&s,raw+at,take));at+=take;}
        assert(online_stream_end(&s));assert(!strcmp(compact,expected));assert(decoded_size==bytes);
    }
}
int main(void) {
    check("{\"type\":\"response.audio.delta\",\"delta\":\"AQIDBA==\"}","{\"type\":\"response.audio.delta\",\"delta\":\"\"}",4);
    assert(decoded[0]==1 && decoded[1]==2 && decoded[2]==3 && decoded[3]==4);
    check("{\"meta\":{\"delta\":\"ignore\"},\"type\":\"response.audio.delta\",\"delta\":\"AA\\/AAA==\",\"index\":0}","{\"meta\":{\"delta\":\"ignore\"},\"type\":\"response.audio.delta\",\"delta\":\"\",\"index\":0}",4);
    const char *text="{\"type\":\"response.audio_transcript.delta\",\"delta\":\"你好，\\\"加油\\\"\"}";check(text,text,0);
    static char large[35000];const char *prefix="{\"type\":\"response.audio.delta\",\"delta\":\"";size_t n=strlen(prefix);memcpy(large,prefix,n);memset(large+n,'A',32000);strcpy(large+n+32000,"\"}");
    check(large,"{\"type\":\"response.audio.delta\",\"delta\":\"\"}",24000);
    char compact[1024];online_stream s;online_stream_begin(&s,compact,sizeof(compact),collect,NULL);
    assert(!online_stream_feed(&s,"{\"type\":\"response.audio.delta\",\"delta\":\"????\"}",strlen("{\"type\":\"response.audio.delta\",\"delta\":\"????\"}")));
    assert(s.error==ONLINE_STREAM_BASE64);
    online_stream_begin(&s,compact,sizeof(compact),congested,NULL);
    const char *valid="{\"type\":\"response.audio.delta\",\"delta\":\"AQIDBA==\"}";
    assert(!online_stream_feed(&s,valid,strlen(valid)) && s.error==ONLINE_STREAM_SINK);
    /* A receiver stall must remain distinguishable from invalid cloud data. */
    assert(!online_stream_end(&s) && s.error==ONLINE_STREAM_SINK);
    online_stream_begin(&s,compact,20,collect,NULL);
    assert(!online_stream_feed(&s,text,strlen(text)) && s.error==ONLINE_STREAM_CAPACITY);
    online_stream_begin(&s,compact,sizeof(compact),collect,NULL);
    const char *odd="{\"type\":\"response.audio.delta\",\"delta\":\"AQ==\"}";
    assert(!online_stream_feed(&s,odd,strlen(odd)) && s.error==ONLINE_STREAM_PCM);
    online_stream_begin(&s,compact,sizeof(compact),collect,NULL);assert(online_stream_feed(&s,"{\"type\":",8));assert(!online_stream_end(&s));
    puts("Online streaming: arbitrary fragments, large audio, escaped slash, captions and malformed data PASS");
}
