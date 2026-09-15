#include "online_state.h"
#include <stdio.h>
#include <string.h>
static bool bounded(const char *s,size_t n) {return memchr(s,0,n)!=NULL;}
bool online_config_valid(const online_config_t *c) {
    static const char prefix[]="wss://dashscope.aliyuncs.com/api-ws/v1/realtime?model=qwen-";
    if(!c || c->version!=1 || !bounded(c->ssid,sizeof(c->ssid)) || !bounded(c->password,sizeof(c->password)) ||
       !bounded(c->url,sizeof(c->url)) || !bounded(c->token,sizeof(c->token)) || !c->ssid[0])return false;
    size_t pass=strlen(c->password);if(pass && (pass<8 || pass>63))return false;
    if(strncmp(c->url,prefix,sizeof(prefix)-1) || strlen(c->token)<12 || strpbrk(c->token,"\r\n\t "))return false;
    const char *model=c->url+sizeof(prefix)-1;
    return *model && strspn(model,"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-.")==strlen(model);
}
bool online_fragment(size_t *used,size_t cap,size_t off,size_t len,size_t total) {
    if(!off)*used=0;
    if(total>=cap || off!=*used || off>total || len>total-off){*used=0;return false;}
    *used+=len;return true;
}
void online_setup_password(uint32_t random_value,char out[9]) {
    snprintf(out,9,"%08lu",(unsigned long)(random_value%100000000u));
}
unsigned online_caption_lines(const char *text,char (*lines)[ONLINE_LINE_BYTES],unsigned cap) {
    if(!cap)return 0;
    unsigned row=0,chars=0,bytes=0;memset(lines,0,cap*ONLINE_LINE_BYTES);
    for(const unsigned char *p=(const unsigned char*)text;*p;) {
        unsigned n=*p<128?1:(*p&0xe0)==0xc0?2:(*p&0xf0)==0xe0?3:4;
        bool complete=true;for(unsigned i=1;i<n;i++)if(!p[i] || (p[i]&0xc0)!=0x80){complete=false;break;}
        if(!complete)break;
        if(n==4){p+=n;continue;}
        if(*p=='\n' || chars==14) {
            if(chars){if(++row>=cap)return cap;bytes=chars=0;}
            if(*p=='\n'){++p;continue;}
        }
        memcpy(lines[row]+bytes,p,n);bytes+=n;lines[row][bytes]=0;++chars;p+=n;
    }
    unsigned count=row+(chars?1:0);
    // A dangling final character shares the preceding line after moving one glyph.
    if(count>=2 && chars==1) {
        char *prev=lines[count-2],*last=lines[count-1];size_t len=strlen(prev),at=len;
        if(at){do{--at;}while(at && ((unsigned char)prev[at]&0xc0)==0x80);
            size_t n=len-at;memmove(last+n,last,strlen(last)+1);memcpy(last,prev+at,n);prev[at]=0;}
    }
    return count;
}

static void json_space(const char **p,const char *end) {
    while(*p<end && (**p==' ' || **p=='\t' || **p=='\r' || **p=='\n'))++*p;
}
static bool json_string(const char **p,const char *end) {
    if(*p==end || *(*p)++!='"')return false;
    while(*p<end) {
        unsigned char c=(unsigned char)*(*p)++;
        if(c=='"')return true;
        if(c<32)return false;
        if(c=='\\') {
            if(*p==end)return false;
            char escaped=*(*p)++;
            if(escaped=='u') {
                for(unsigned i=0;i<4;++i){if(*p==end || !strchr("0123456789abcdefABCDEF",**p))return false;++*p;}
            } else if(!strchr("\"\\/bfnrt",escaped))return false;
        }
    }
    return false;
}
static bool json_value(const char **p,const char *end,unsigned depth) {
    if(depth>8 || *p==end)return false;
    if(**p=='"')return json_string(p,end);
    if(**p=='{' || **p=='[') {
        bool object=**p=='{';char close=object?'}':']';++*p;json_space(p,end);
        if(*p<end && **p==close){++*p;return true;}
        for(;;) {
            if(object){if(!json_string(p,end))return false;json_space(p,end);if(*p==end || *(*p)++!=':')return false;json_space(p,end);}
            if(!json_value(p,end,depth+1))return false;
            json_space(p,end);if(*p==end)return false;
            char c=*(*p)++;if(c==close)return true;if(c!=',')return false;json_space(p,end);
        }
    }
    const char *start=*p;
    while(*p<end && !strchr(" \t\r\n,}]",**p))++*p;
    return *p>start;
}
int online_audio_event(char *json,size_t length,char **audio,size_t *audio_length) {
    const char *p=json,*end=json+length,*delta=NULL;size_t delta_length=0;bool is_audio=false,seen_type=false,seen_delta=false;
    *audio=NULL;*audio_length=0;json_space(&p,end);if(p==end || *p++!='{')return -1;json_space(&p,end);
    if(p<end && *p=='}')return 0;
    for(;;) {
        const char *key=p;if(!json_string(&p,end))return -1;size_t key_length=(size_t)(p-key);
        json_space(&p,end);if(p==end || *p++!=':')return -1;json_space(&p,end);
        const char *value=p;if(!json_value(&p,end,0))return -1;size_t value_length=(size_t)(p-value);
        if(key_length==6 && !memcmp(key,"\"type\"",6)) {
            if(seen_type)return -1;
            seen_type=true;is_audio=value_length==22 && !memcmp(value,"\"response.audio.delta\"",22);
        } else if(key_length==7 && !memcmp(key,"\"delta\"",7)) {
            if(seen_delta)return -1;
            seen_delta=true;if(value_length>=2 && *value=='"'){delta=value+1;delta_length=value_length-2;}
        }
        json_space(&p,end);if(p==end)return -1;char c=*p++;
        if(c=='}')break;
        if(c!=',')return -1;
        json_space(&p,end);
    }
    json_space(&p,end);if(p!=end)return -1;
    if(!is_audio)return 0;
    if(!delta)return -1;
    char *out=(char*)delta;size_t n=0;
    for(size_t i=0;i<delta_length;++i) {
        unsigned char c=(unsigned char)delta[i];
        if(c=='\\'){if(++i>=delta_length || delta[i]!='/')return -1;c='/';}
        if(!((c>='A' && c<='Z') || (c>='a' && c<='z') || (c>='0' && c<='9') || c=='+' || c=='/' || c=='='))return -1;
        out[n++]=(char)c;
    }
    if(n%4)return -1;
    *audio=out;*audio_length=n;return 1;
}
