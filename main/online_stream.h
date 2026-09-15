#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef bool (*online_pcm_sink)(const uint8_t *pcm,size_t length,void *context);
enum { ONLINE_STREAM_OK, ONLINE_STREAM_CAPACITY, ONLINE_STREAM_BASE64,
       ONLINE_STREAM_PCM, ONLINE_STREAM_SINK, ONLINE_STREAM_JSON };
typedef struct {
    char *json;size_t capacity,used,start;
    unsigned depth,phase,key,quartet_count,pcm_used,error;
    bool string,escape,audio,stripping,padded,failed;
    uint8_t quartet[4],pcm[636];
    online_pcm_sink sink;void *context;
} online_stream;
void online_stream_begin(online_stream *s,char *json,size_t capacity,online_pcm_sink sink,void *context);
bool online_stream_feed(online_stream *s,const char *bytes,size_t length);
bool online_stream_end(online_stream *s);
