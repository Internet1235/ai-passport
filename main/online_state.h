#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct { uint32_t version; char ssid[33], password[65], url[256], token[193]; } online_config_t;
bool online_config_valid(const online_config_t *config);
bool online_fragment(size_t *used,size_t capacity,size_t offset,size_t length,size_t total);
void online_setup_password(uint32_t random_value,char out[9]);
#define ONLINE_LINE_BYTES 43
unsigned online_caption_lines(const char *text,char (*lines)[ONLINE_LINE_BYTES],unsigned capacity);
/* In-place extraction for the provider's large base64 audio events; no heap allocation.
 * Returns 1 for audio, 0 for another event, and -1 for malformed input. */
int online_audio_event(char *json,size_t length,char **audio,size_t *audio_length);
