#include "online_queue.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
int main(void) {
    online_queue q={0};online_packet *extra[ONLINE_QUEUE_EXTRA],p={0},out;
    /* Independently allocated, permuted slots must behave as one FIFO. */
    for(unsigned i=0;i<ONLINE_QUEUE_EXTRA;++i){extra[(i*7)%ONLINE_QUEUE_EXTRA]=malloc(sizeof(p));assert(extra[(i*7)%ONLINE_QUEUE_EXTRA]);}
    for(unsigned expanded=0;expanded<2;++expanded) {
        unsigned capacity=32+16*expanded;
        assert(online_queue_extend(&q,expanded?extra:NULL,16*expanded));
        for(unsigned round=0;round<100;++round) {
            for(unsigned i=0;i<capacity;++i){p.epoch=round*capacity+i;p.input=!expanded;p.done=i+1==capacity;assert(online_queue_push(&q,&p));}
            assert(!online_queue_push(&q,&p));assert(!online_queue_extend(&q,NULL,0));
            assert(online_queue_peek(&q,&out) && out.epoch==round*capacity);
            for(unsigned i=0;i<capacity;++i){assert(online_queue_pop(&q,&out));assert(out.epoch==round*capacity+i && out.input==!expanded && out.done==(i+1==capacity));}
            assert(!online_queue_pop(&q,&out));
            // Shift the ring so both storage segments wrap at different points.
            assert(online_queue_push(&q,&p));assert(online_queue_pop(&q,&out));
        }
    }
    for(unsigned i=0;i<48;++i)assert(online_queue_push(&q,&p));
    online_queue_reset(&q);assert(online_queue_extend(&q,NULL,0));assert(!online_queue_peek(&q,&out));
    assert(!online_queue_extend(&q,NULL,16));
    assert(!online_queue_extend(&q,extra,ONLINE_QUEUE_EXTRA+1));
    online_packet *saved=extra[7];extra[7]=NULL;assert(!online_queue_extend(&q,extra,16));extra[7]=saved;
    assert(q.extra_count==0);
    for(unsigned i=0;i<ONLINE_QUEUE_EXTRA;++i){assert(q.extra[i]==NULL);free(extra[i]);}
    puts("Online queue: capture/playback capacities, FIFO across segments, wrap and cancellation PASS");
}
