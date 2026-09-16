#include "online_queue.h"
static online_packet *slot(online_queue *q,unsigned at) {
    return at<ONLINE_QUEUE_BASE?&q->base[at]:q->extra[at-ONLINE_QUEUE_BASE];
}
void online_queue_reset(online_queue *q) {q->head=q->tail=q->count=0;}
bool online_queue_extend(online_queue *q,online_packet *const *extra,unsigned count) {
    if(q->count || (count && !extra) || count>ONLINE_QUEUE_EXTRA)return false;
    for(unsigned i=0;i<count;++i)if(!extra[i])return false;
    for(unsigned i=0;i<ONLINE_QUEUE_EXTRA;++i)q->extra[i]=i<count?extra[i]:NULL;
    q->extra_count=count;q->head=q->tail=0;return true;
}
bool online_queue_push(online_queue *q,const online_packet *p) {
    unsigned capacity=ONLINE_QUEUE_BASE+q->extra_count;
    if(q->count>=capacity)return false;
    *slot(q,q->tail)=*p;q->tail=(q->tail+1)%capacity;++q->count;return true;
}
bool online_queue_pop(online_queue *q,online_packet *p) {
    if(!q->count)return false;
    *p=*slot(q,q->head);q->head=(q->head+1)%(ONLINE_QUEUE_BASE+q->extra_count);--q->count;return true;
}
bool online_queue_peek(const online_queue *q,online_packet *p) {
    if(!q->count)return false;
    *p=q->head<ONLINE_QUEUE_BASE?q->base[q->head]:*q->extra[q->head-ONLINE_QUEUE_BASE];return true;
}
