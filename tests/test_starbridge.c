#include "starbridge.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    for (unsigned mask = 0; mask < 16; ++mask) {
        uint8_t t = (uint8_t)mask;
        for (int n = 0; n < 4; ++n) t = sb_rotate_mask(t);
        assert(t == mask);
    }
    sb_game g; sb_new(&g);
    for (unsigned level = 0; level < SB_LEVELS; ++level) {
        sb_load_level(&g, level);
        assert(!g.won && g.par > 0 && g.size == (level < 10 ? 3 : 4));
        unsigned par = g.par;
        sb_game opening = g;
        sb_move(&g, -1); assert(g.cursor == g.size * g.size - 1);
        sb_move(&g, 1); assert(g.cursor == 0);
        for (unsigned i = 0; i < g.size * g.size; ++i) {
            g.cursor = (uint8_t)i;
            for (unsigned turn = 0; turn < 4; ++turn) {
                if (g.tiles[i] == g.solution[i]) break;
                assert(sb_turn(&g));
            }
        }
        assert(g.won && g.moves <= par && g.stars[level] == 3);
        assert(sb_powered_count(&g) == g.size * g.size);
        assert(g.unlocked == (level == SB_LEVELS - 1 ? level : level + 1));
        assert(!sb_turn(&g) && !sb_hint(&g) && !sb_undo(&g));
        uint8_t saved[SB_SAVE_SIZE]; sb_encode(&g, saved);
        sb_game recovered; assert(sb_decode(&recovered, saved, sizeof(saved)));
        assert(recovered.won && recovered.moves == g.moves && recovered.par == g.par);
        assert(!memcmp(recovered.tiles, g.tiles, sizeof(g.tiles)));
        for (unsigned i = 0; i < sizeof(saved); ++i) {
            saved[i] ^= 0x40; assert(!sb_decode(&recovered, saved, sizeof(saved))); saved[i] ^= 0x40;
        }
        assert(!sb_decode(&recovered, saved, sizeof(saved) - 1));
        assert(!sb_decode(&recovered, NULL, sizeof(saved)));
        sb_game hints = opening;
        memset(hints.stars, 0, sizeof(hints.stars));
        unsigned n = 0;
        while (!hints.won) { assert(sb_hint(&hints)); assert(++n <= SB_CELLS); }
        assert(hints.stars[level] == 1 && hints.hints > 0);
        sb_game undo = opening;
        assert(sb_turn(&undo)); assert(sb_undo(&undo));
        assert(!memcmp(undo.tiles, opening.tiles, sizeof(undo.tiles)));
        assert(!sb_undo(&undo));
        for (unsigned i = 0; i < 1000; ++i) { sb_move(&undo, 1); if (undo.won) break; sb_turn(&undo); }
        assert(undo.history_count <= 128);
        sb_encode(&undo, saved); assert(sb_decode(&recovered, saved, sizeof(saved)));
        assert(!memcmp(recovered.tiles, undo.tiles, sizeof(undo.tiles)));
        assert(recovered.history_count == 0); // Undo history intentionally stays in RAM.
    }
    assert(sb_total_stars(&g) == 90);
    puts("Starbridge: 30 solvable levels, rotation, wrap, scoring, hints, undo and save integrity PASS");
    return 0;
}
