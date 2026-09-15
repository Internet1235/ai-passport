#include "online_state.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    online_config_t c={.version=1,.ssid="test-network",.password="test-only",.url="wss://dashscope.aliyuncs.com/api-ws/v1/realtime?model=qwen-audio-3.0-realtime-plus",.token="test-placeholder-not-a-key"};
    assert(online_config_valid(&c));
    c.password[3]=0;assert(!online_config_valid(&c));c.password[0]=0;assert(online_config_valid(&c));
    c.url[6]='x';assert(!online_config_valid(&c));
    size_t used=0;assert(online_fragment(&used,100,0,25,60));assert(online_fragment(&used,100,25,35,60));
    assert(!online_fragment(&used,100,0,100,100));assert(!online_fragment(&used,100,2,5,7));
    char lines[4][ONLINE_LINE_BYTES];assert(online_caption_lines("一二三四五六七八九十甲乙丙丁戊",lines,4)==2);
    assert(!strcmp(lines[0],"一二三四五六七八九十甲乙丙") && !strcmp(lines[1],"丁戊"));
    assert(online_caption_lines("你好💗今天也加油",lines,4)==1 && !strcmp(lines[0],"你好今天也加油"));
    assert(online_caption_lines("",lines,4)==0);assert(online_caption_lines("hello",lines,0)==0);
    const char partial[]={'a',(char)0xe4,(char)0xbd,0};assert(online_caption_lines(partial,lines,4)==1 && !strcmp(lines[0],"a"));
    char audio_json[]={"{\"type\":\"response.audio.delta\",\"delta\":\"AQIDBA==\",\"output_index\":0}"};
    char *audio;size_t audio_length;
    assert(online_audio_event(audio_json,strlen(audio_json),&audio,&audio_length)==1 && audio_length==8 && !memcmp(audio,"AQIDBA==",8));
    char swapped[]={"{\"delta\":\"AA\\/A\",\"meta\":{\"delta\":\"bad\"},\"type\":\"response.audio.delta\"}"};
    assert(online_audio_event(swapped,strlen(swapped),&audio,&audio_length)==1 && audio_length==4 && !memcmp(audio,"AA/A",4));
    char invalid[]={"{\"type\":\"response.audio.delta\",\"delta\":\"?!==\"}"};
    assert(online_audio_event(invalid,strlen(invalid),&audio,&audio_length)==-1);
    char truncated[]={"{\"type\":\"response.audio.delta\",\"delta\":\"AAAA"};
    assert(online_audio_event(truncated,strlen(truncated),&audio,&audio_length)==-1);
    char other[]={"{\"type\":\"response.audio.done\"}"};
    assert(online_audio_event(other,strlen(other),&audio,&audio_length)==0);
    static char large[30000];const char *prefix="{\"delta\":\"";size_t prefix_size=strlen(prefix);memcpy(large,prefix,prefix_size);memset(large+prefix_size,'A',25800);strcpy(large+prefix_size+25800,"\",\"type\":\"response.audio.delta\"}");
    assert(online_audio_event(large,strlen(large),&audio,&audio_length)==1 && audio_length==25800);
    puts("Online config, bounded fragments, UTF-8 and orphan captions PASS");
}
