#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define ONLINE_QUEUE_BASE 32
#define ONLINE_QUEUE_EXTRA 16
typedef struct { unsigned epoch;size_t len;bool done,input;int16_t pcm[320]; } online_packet;
/* The caller serializes access. Extra storage belongs to the caller and may
 * only be changed after clearing the queue. No packet data is allocated here. */
typedef struct {
    online_packet base[ONLINE_QUEUE_BASE],*extra[ONLINE_QUEUE_EXTRA];
    unsigned extra_count,head,tail,count;
} online_queue;
void online_queue_reset(online_queue *q);
bool online_queue_extend(online_queue *q,online_packet *const *extra,unsigned count);
bool online_queue_push(online_queue *q,const online_packet *p);
bool online_queue_pop(online_queue *q,online_packet *p);
bool online_queue_peek(const online_queue *q,online_packet *p);
