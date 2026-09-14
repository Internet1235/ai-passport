#include "starbridge.h"
#include <string.h>

static uint32_t random_next(uint32_t *state) {
    *state ^= *state << 13; *state ^= *state >> 17; *state ^= *state << 5;
    return *state;
}

uint8_t sb_rotate_mask(uint8_t mask) { return (uint8_t)(((mask << 1) | (mask >> 3)) & 15); }

static int neighbor(unsigned i, unsigned d, unsigned size) {
    if (d == 0) return i >= size ? (int)(i - size) : -1;
    if (d == 1) return i % size + 1 < size ? (int)i + 1 : -1;
    if (d == 2) return i + size < size * size ? (int)(i + size) : -1;
    return i % size ? (int)i - 1 : -1;
}

unsigned sb_powered_count(const sb_game *g) {
    unsigned count = 0;
    for (unsigned i = 0; i < g->size * g->size; ++i) count += !!(g->powered & (1u << i));
    return count;
}

unsigned sb_total_stars(const sb_game *g) {
    unsigned n = 0;
    for (unsigned i = 0; i < SB_LEVELS; ++i) n += g->stars[i];
    return n;
}

void sb_evaluate(sb_game *g) {
    uint8_t queue[SB_CELLS]; unsigned head = 0, tail = 1;
    queue[0] = 0; g->powered = 1;
    while (head < tail) {
        unsigned i = queue[head++];
        for (unsigned d = 0; d < 4; ++d) {
            int j = neighbor(i, d, g->size);
            if (j < 0 || !(g->tiles[i] & (1u << d)) ||
                !(g->tiles[j] & (1u << ((d + 2) % 4))) || (g->powered & (1u << j))) continue;
            g->powered |= (uint16_t)(1u << j); queue[tail++] = (uint8_t)j;
        }
    }
    g->won = tail == g->size * g->size;
    if (g->won) {
        unsigned stars = g->hints ? 1 : (g->moves <= g->par ? 3 : 2);
        if (stars > g->stars[g->level]) g->stars[g->level] = (uint8_t)stars;
        if (g->unlocked < SB_LEVELS - 1 && g->level >= g->unlocked) g->unlocked = g->level + 1;
    }
}

void sb_load_level(sb_game *g, unsigned level) {
    if (level >= SB_LEVELS) level = SB_LEVELS - 1;
    g->level = (uint8_t)level; g->size = level < 10 ? 3 : 4;
    g->cursor = 0; g->moves = 0; g->par = 0; g->hints = 0; g->history_count = 0;
    memset(g->tiles, 0, sizeof(g->tiles)); memset(g->solution, 0, sizeof(g->solution));
    uint32_t rng = 0x57a2b1d3u + level * 0x9e3779b9u;
    uint8_t stack[SB_CELLS] = {0}; unsigned depth = 1; uint16_t visited = 1;
    while (depth) {
        unsigned i = stack[depth - 1], dirs[4], count = 0;
        for (unsigned d = 0; d < 4; ++d) {
            int j = neighbor(i, d, g->size);
            if (j >= 0 && !(visited & (1u << j))) dirs[count++] = d;
        }
        if (!count) { --depth; continue; }
        unsigned d = dirs[random_next(&rng) % count];
        unsigned j = (unsigned)neighbor(i, d, g->size);
        g->solution[i] |= (uint8_t)(1u << d);
        g->solution[j] |= (uint8_t)(1u << ((d + 2) % 4));
        visited |= (uint16_t)(1u << j); stack[depth++] = (uint8_t)j;
    }
    for (unsigned i = 0; i < g->size * g->size; ++i) {
        uint8_t tile = g->solution[i]; unsigned turns = random_next(&rng) % 4;
        while (turns--) tile = sb_rotate_mask(tile);
        g->tiles[i] = tile;
    }
    // The source is a leaf in the generated tree: rotating it guarantees an unsolved opening.
    if (g->tiles[0] == g->solution[0]) g->tiles[0] = sb_rotate_mask(g->tiles[0]);
    for (unsigned i = 0; i < g->size * g->size; ++i) {
        uint8_t tile = g->tiles[i];
        while (tile != g->solution[i]) { ++g->par; tile = sb_rotate_mask(tile); }
    }
    sb_evaluate(g);
}

void sb_new(sb_game *g) { memset(g, 0, sizeof(*g)); sb_load_level(g, 0); }

void sb_move(sb_game *g, int delta) {
    int n = g->size * g->size;
    g->cursor = (uint8_t)(((int)g->cursor + delta % n + n) % n);
}

static void remember(sb_game *g, unsigned cell) {
    if (g->history_count == sizeof(g->history)) {
        memmove(g->history, g->history + 1, sizeof(g->history) - 1); --g->history_count;
    }
    g->history[g->history_count++] = (uint8_t)cell;
}

bool sb_turn(sb_game *g) {
    if (g->won) return false;
    remember(g, g->cursor); g->tiles[g->cursor] = sb_rotate_mask(g->tiles[g->cursor]);
    if (g->moves < UINT16_MAX) ++g->moves;
    sb_evaluate(g); return true;
}

bool sb_undo(sb_game *g) {
    if (g->won || !g->history_count) return false;
    g->cursor = g->history[--g->history_count];
    for (unsigned i = 0; i < 3; ++i) g->tiles[g->cursor] = sb_rotate_mask(g->tiles[g->cursor]);
    // Count all rotations, including retries, so undo cannot manufacture a three-star result.
    sb_evaluate(g); return true;
}

bool sb_hint(sb_game *g) {
    if (g->won) return false;
    for (unsigned i = 0; i < g->size * g->size; ++i) {
        if (g->tiles[i] == g->solution[i]) continue;
        g->cursor = (uint8_t)i;
        if (g->hints < UINT8_MAX) ++g->hints;
        while (g->tiles[i] != g->solution[i]) {
            remember(g, i); g->tiles[i] = sb_rotate_mask(g->tiles[i]);
            if (g->moves < UINT16_MAX) ++g->moves;
        }
        sb_evaluate(g); return true;
    }
    return false;
}

static uint32_t checksum(const uint8_t *p, size_t n) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; ++i) h = (h ^ p[i]) * 16777619u;
    return h;
}

void sb_encode(const sb_game *g, uint8_t out[SB_SAVE_SIZE]) {
    memset(out, 0, SB_SAVE_SIZE); memcpy(out, "SB01", 4);
    out[4] = g->level; out[5] = g->unlocked; out[6] = g->cursor; out[7] = g->hints;
    out[8] = (uint8_t)g->moves; out[9] = (uint8_t)(g->moves >> 8);
    memcpy(out + 10, g->tiles, SB_CELLS); memcpy(out + 26, g->stars, SB_LEVELS);
    uint32_t hash = checksum(out, SB_SAVE_SIZE - 4);
    for (unsigned i = 0; i < 4; ++i) out[SB_SAVE_SIZE - 4 + i] = (uint8_t)(hash >> (8 * i));
}

bool sb_decode(sb_game *g, const uint8_t *data, size_t len) {
    if (!data || len != SB_SAVE_SIZE || memcmp(data, "SB01", 4)) return false;
    uint32_t stored = 0;
    for (unsigned i = 0; i < 4; ++i) stored |= (uint32_t)data[SB_SAVE_SIZE - 4 + i] << (8 * i);
    if (stored != checksum(data, SB_SAVE_SIZE - 4) || data[4] >= SB_LEVELS ||
        data[5] >= SB_LEVELS || data[4] > data[5]) return false;
    sb_game next; sb_new(&next); sb_load_level(&next, data[4]);
    if (data[6] >= next.size * next.size) return false;
    for (unsigned i = 0; i < SB_CELLS; ++i) {
        uint8_t candidate = next.solution[i]; bool valid = false;
        for (unsigned t = 0; t < 4; ++t) { valid |= candidate == data[10 + i]; candidate = sb_rotate_mask(candidate); }
        if (!valid) return false;
    }
    for (unsigned i = 0; i < SB_LEVELS; ++i) if (data[26 + i] > 3) return false;
    next.unlocked = data[5]; next.cursor = data[6]; next.hints = data[7];
    next.moves = (uint16_t)(data[8] | ((uint16_t)data[9] << 8));
    memcpy(next.tiles, data + 10, SB_CELLS); memcpy(next.stars, data + 26, SB_LEVELS);
    sb_evaluate(&next); *g = next; return true;
}
