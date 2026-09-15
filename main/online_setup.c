#include "online_setup.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "nvs.h"
#include "cJSON.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static char nonce[33];
static bool saved;
static esp_timer_handle_t reboot_timer;
bool online_load_config(online_config_t *c) {
    nvs_handle_t n; size_t size=sizeof(*c); memset(c,0,size);
    if(nvs_open("wm_online",NVS_READWRITE,&n)!=ESP_OK) return false;
    uint8_t setup=0; nvs_get_u8(n,"setup",&setup);
    if(setup) { nvs_erase_key(n,"setup"); nvs_commit(n); }
    esp_err_t e=nvs_get_blob(n,"config",c,&size); nvs_close(n);
    return !setup && e==ESP_OK && size==sizeof(*c) && online_config_valid(c);
}
static void reboot(void *arg) { (void)arg;esp_restart(); }
#define NETWORK_LIMIT 32
/* This list is only needed in the separate provisioning boot. */
static wifi_ap_record_t *nearby;
static uint16_t nearby_count;
static bool scan_ok;
static esp_err_t networks(httpd_req_t *r) {
    char query[32]={0};
    if(httpd_req_get_url_query_str(r,query,sizeof(query))==ESP_OK && strstr(query,"refresh=1")) {
        scan_ok=esp_wifi_scan_start(NULL,true)==ESP_OK;
        if(scan_ok){nearby_count=NETWORK_LIMIT;if(esp_wifi_scan_get_ap_records(&nearby_count,nearby)!=ESP_OK){nearby_count=0;scan_ok=false;}}
    }
    cJSON *j=cJSON_CreateObject(),*list=cJSON_AddArrayToObject(j,"networks");
    cJSON_AddBoolToObject(j,"scanned",scan_ok);
    for(unsigned i=0;i<nearby_count;i++) {
        const char *ssid=(const char *)nearby[i].ssid;if(!*ssid)continue;
        bool duplicate=false;
        for(unsigned k=0;k<i;k++)if(!strcmp((const char *)nearby[k].ssid,ssid))duplicate=true;
        if(duplicate)continue;
        cJSON *item=cJSON_CreateObject();cJSON_AddStringToObject(item,"ssid",ssid);
        cJSON_AddNumberToObject(item,"rssi",nearby[i].rssi);
        cJSON_AddBoolToObject(item,"secure",nearby[i].authmode!=WIFI_AUTH_OPEN);cJSON_AddItemToArray(list,item);
    }
    char *raw=cJSON_PrintUnformatted(j);cJSON_Delete(j);
    if(!raw)return httpd_resp_send_err(r,HTTPD_500_INTERNAL_SERVER_ERROR,"读取列表失败");
    httpd_resp_set_type(r,"application/json; charset=utf-8");httpd_resp_set_hdr(r,"Cache-Control","no-store");
    esp_err_t e=httpd_resp_sendstr(r,raw);free(raw);return e;
}
static bool default_key(char *out,size_t capacity) {
    nvs_handle_t n;size_t size=capacity;out[0]=0;
    if(nvs_open("wm_online",NVS_READONLY,&n)!=ESP_OK)return false;
    esp_err_t e=nvs_get_str(n,"default_key",out,&size);nvs_close(n);
    return e==ESP_OK && strlen(out)>=12;
}
static esp_err_t home(httpd_req_t *r) {
    char secret[193];bool configured=default_key(secret,sizeof(secret));memset(secret,0,sizeof(secret));
    char *html=malloc(8192);if(!html)return ESP_ERR_NO_MEM;
    int length=snprintf(html,8192,
        "<!doctype html><html lang=zh-CN><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
        "<title>元气随身听配网</title><style>body{font:17px system-ui;max-width:480px;margin:24px auto;padding:20px;background:#fff7f5;color:#454157}input,select,button{box-sizing:border-box;width:100%%;padding:12px;margin:8px 0 20px;font:inherit}button{background:#f3c6d0;border:0;border-radius:12px}small{display:block;color:#81788a}.secondary{background:#eee9f2}</style>"
        "<h1>元气随身听</h1><p>连接 Wi-Fi，开启语音陪伴。</p><form id=f>"
        "<label>附近的 2.4 GHz Wi-Fi<select id=wifi required><option value=''>正在搜索附近 Wi-Fi…</option></select></label>"
        "<button class=secondary type=button id=scan>重新搜索</button><small id=hint></small>"
        "<label id=manualRow hidden>隐藏网络名称<input id=manual maxlength=32 disabled></label>"
        "<label>Wi-Fi 密码<input name=password type=password maxlength=63 autocomplete=new-password></label>"
        "<label>千问 API Key<input name=token type=password maxlength=192 autocomplete=new-password placeholder='%s' %s></label>"
        "<small>%s</small><p>配置仅保存在这台设备。主动开始说话时，语音将发送至千问服务。</p>"
        "<button>保存并连接</button></form><p id=s></p><script>"
        "const f=document.getElementById('f'),s=document.getElementById('s'),wifi=document.getElementById('wifi'),manual=document.getElementById('manual'),scan=document.getElementById('scan'),hint=document.getElementById('hint');let networks=[];"
        "wifi.onchange=()=>{const yes=wifi.value==='manual';document.getElementById('manualRow').hidden=!yes;manual.disabled=!yes;manual.required=yes;};"
        "async function load(refresh){scan.disabled=true;hint.textContent='正在搜索，请稍等…';try{const r=await fetch('/networks'+(refresh?'?refresh=1':''));const d=await r.json();networks=d.networks||[];wifi.replaceChildren(new Option('请选择 Wi-Fi',''));networks.forEach((n,i)=>wifi.add(new Option(n.ssid+' · '+(n.rssi>=-60?'信号强':n.rssi>=-75?'信号中':'信号弱'),String(i))));wifi.add(new Option('隐藏网络 / 手动输入','manual'));wifi.onchange();hint.textContent=networks.length?'已找到 '+networks.length+' 个网络':'暂未找到网络，请靠近路由器后重新搜索';}catch(e){hint.textContent='搜索中断，请重新搜索';}finally{scan.disabled=false}}scan.onclick=()=>load(true);load(false);"
        "f.onsubmit=async e=>{e.preventDefault();const b=f.querySelector('button:not([type=button])');b.disabled=true;const data=Object.fromEntries(new FormData(f));data.ssid=wifi.value==='manual'?manual.value:networks[Number(wifi.value)].ssid;try{const r=await fetch('/save',{method:'POST',headers:{'Content-Type':'application/json','X-Setup-Nonce':'%s'},body:JSON.stringify(data)});s.textContent=await r.text();if(!r.ok)b.disabled=false}catch(e){s.textContent='请查看设备是否正在重启';b.disabled=false}};</script></html>",
        configured?"已配置默认密钥，留空即可":"填写千问 API Key",configured?"":"required",
        configured?"已配置默认密钥。留空继续使用，填写新密钥即可替换。":"首次连接需要填写密钥。",nonce);
    if(length<0 || length>=8192){free(html);return ESP_FAIL;}
    httpd_resp_set_type(r,"text/html; charset=utf-8");httpd_resp_set_hdr(r,"Cache-Control","no-store");httpd_resp_set_hdr(r,"X-Frame-Options","DENY");
    esp_err_t e=httpd_resp_send(r,html,length);free(html);return e;
}
static bool field(cJSON *j,const char *key,char *out,size_t cap) {
    cJSON *v=cJSON_GetObjectItemCaseSensitive(j,key);
    if(!cJSON_IsString(v) || strlen(v->valuestring)>=cap) return false;
    strcpy(out,v->valuestring); return true;
}
static esp_err_t save(httpd_req_t *r) {
    char provided[40];
    if(httpd_req_get_hdr_value_str(r,"X-Setup-Nonce",provided,sizeof(provided))!=ESP_OK || strcmp(provided,nonce))
        return httpd_resp_send_err(r,HTTPD_403_FORBIDDEN,"请重新打开配置页");
    if(saved || r->content_len<=0 || r->content_len>1200) return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"配置无效");
    char body[1201];size_t used=0;
    while(used<(size_t)r->content_len) {
        int n=httpd_req_recv(r,body+used,r->content_len-used);
        if(n<=0) return ESP_FAIL;
        used+=n;
    }
    body[used]=0;cJSON *j=cJSON_Parse(body);online_config_t c={.version=1};
    snprintf(c.url,sizeof(c.url),"wss://dashscope.aliyuncs.com/api-ws/v1/realtime?model=qwen-audio-3.0-realtime-plus");
    bool valid=j && field(j,"ssid",c.ssid,sizeof(c.ssid)) && field(j,"password",c.password,sizeof(c.password)) && field(j,"token",c.token,sizeof(c.token));
    if(valid && !c.token[0])valid=default_key(c.token,sizeof(c.token));
    valid=valid && online_config_valid(&c);
    cJSON_Delete(j);memset(body,0,sizeof(body));
    if(!valid) {memset(&c,0,sizeof(c));return httpd_resp_send_err(r,HTTPD_400_BAD_REQUEST,"请检查 Wi-Fi 名称、密码和千问 API Key");}
    nvs_handle_t n;esp_err_t e=nvs_open("wm_online",NVS_READWRITE,&n);
    if(e==ESP_OK) {e=nvs_set_blob(n,"config",&c,sizeof(c));if(e==ESP_OK)e=nvs_set_str(n,"default_key",c.token);if(e==ESP_OK)e=nvs_commit(n);nvs_close(n);}
    memset(&c,0,sizeof(c));
    if(e!=ESP_OK) return httpd_resp_send_err(r,HTTPD_500_INTERNAL_SERVER_ERROR,"保存失败，请重试");
    saved=true;httpd_resp_set_type(r,"text/plain; charset=utf-8");
    httpd_resp_sendstr(r,"已保存，设备即将重启。请让手机重新连接原来的 Wi-Fi。");
    esp_timer_start_once(reboot_timer,1500000);return ESP_OK;
}
esp_err_t online_start_setup(char *description,size_t size) {
    nearby=calloc(NETWORK_LIMIT,sizeof(*nearby));
    if(!nearby)return ESP_ERR_NO_MEM;
    wifi_config_t ap={0}; char password[9];
    online_setup_password(esp_random(),password);
    snprintf(nonce,sizeof(nonce),"%08lx%08lx%08lx%08lx",(unsigned long)esp_random(),(unsigned long)esp_random(),(unsigned long)esp_random(),(unsigned long)esp_random());
    snprintf((char *)ap.ap.ssid,sizeof(ap.ap.ssid),"Sunshine-%04lx",(unsigned long)(esp_random()&65535));
    strcpy((char *)ap.ap.password,password);ap.ap.ssid_len=strlen((char *)ap.ap.ssid);
    ap.ap.authmode=WIFI_AUTH_WPA2_PSK;ap.ap.max_connection=1;ap.ap.channel=1;
    if(!esp_netif_create_default_wifi_sta() || !esp_netif_create_default_wifi_ap()) return ESP_FAIL;
    wifi_init_config_t cfg=WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t e=esp_wifi_init(&cfg); if(e!=ESP_OK)return e;
    if((e=esp_wifi_set_storage(WIFI_STORAGE_RAM))!=ESP_OK || (e=esp_wifi_set_mode(WIFI_MODE_STA))!=ESP_OK ||
       (e=esp_wifi_start())!=ESP_OK)return e;
    /* Blocking scan belongs to this boot worker and happens before AP startup. */
    scan_ok=esp_wifi_scan_start(NULL,true)==ESP_OK;
    if(scan_ok) {
        nearby_count=NETWORK_LIMIT;
        if(esp_wifi_scan_get_ap_records(&nearby_count,nearby)!=ESP_OK){nearby_count=0;scan_ok=false;}
    } else esp_wifi_clear_ap_list();
    ESP_LOGI("bean_setup","pre-hotspot scan: %u networks, success=%d",nearby_count,scan_ok);
    if((e=esp_wifi_stop())!=ESP_OK || (e=esp_wifi_set_mode(WIFI_MODE_APSTA))!=ESP_OK ||
       (e=esp_wifi_set_config(WIFI_IF_AP,&ap))!=ESP_OK || (e=esp_wifi_start())!=ESP_OK)return e;
    esp_timer_create_args_t ta={.callback=reboot,.name="setup_reboot"};
    if((e=esp_timer_create(&ta,&reboot_timer))!=ESP_OK)return e;
    httpd_config_t hc=HTTPD_DEFAULT_CONFIG();hc.stack_size=6144;hc.max_open_sockets=2;hc.lru_purge_enable=true;
    httpd_handle_t server=NULL;if((e=httpd_start(&server,&hc))!=ESP_OK)return e;
    httpd_uri_t get={.uri="/",.method=HTTP_GET,.handler=home};
    httpd_uri_t post={.uri="/save",.method=HTTP_POST,.handler=save};
    httpd_uri_t list={.uri="/networks",.method=HTTP_GET,.handler=networks};
    if((e=httpd_register_uri_handler(server,&get))!=ESP_OK || (e=httpd_register_uri_handler(server,&post))!=ESP_OK || (e=httpd_register_uri_handler(server,&list))!=ESP_OK)return e;
    snprintf(description,size,"%s\n密码 %s",ap.ap.ssid,password);
    return ESP_OK;
}
