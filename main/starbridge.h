#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SB_LEVELS 30
#define SB_CELLS 16
#define SB_SAVE_SIZE 100

typedef struct {
    uint8_t level, size, cursor, unlocked;
    uint8_t tiles[SB_CELLS], solution[SB_CELLS], stars[SB_LEVELS];
    uint8_t history[128], history_count;
    uint16_t moves, par, powered;
    uint8_t hints;
    bool won;
} sb_game;

uint8_t sb_rotate_mask(uint8_t mask);
void sb_new(sb_game *g);
void sb_load_level(sb_game *g, unsigned level);
void sb_move(sb_game *g, int delta);
bool sb_turn(sb_game *g);
bool sb_undo(sb_game *g);
bool sb_hint(sb_game *g);
void sb_evaluate(sb_game *g);
unsigned sb_powered_count(const sb_game *g);
unsigned sb_total_stars(const sb_game *g);
void sb_encode(const sb_game *g, uint8_t out[SB_SAVE_SIZE]);
bool sb_decode(sb_game *g, const uint8_t *data, size_t len);
