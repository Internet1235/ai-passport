#pragma once
#include "walkman.h"
enum { WA_TOGGLE, WA_PREV, WA_NEXT, WA_VOLUME, WA_REPEAT, WA_TIMER, WA_NETWORK };
typedef struct { wm_player player; bool ready, failed, network; unsigned blocks, max_gap_us, max_render_us, stack_free, peak, dropped; } wa_state;
bool wa_start(const wm_player *initial);
void wa_command(unsigned kind, unsigned value);
wa_state wa_status(void);
