#include "starbridge.h"
#include "starbridge_audio.h"
#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_battery.h"
#include "bsp_i2c.h"
#include "bsp_pins.h"
#include "lvgl.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

LV_FONT_DECLARE(starbridge_16);
LV_FONT_DECLARE(starbridge_12);

enum { HOME, PLAY, PAUSE, LEVELS, HELP, VICTORY };
enum { CMD_KEY, CMD_STATUS, CMD_CAPTURE, CMD_SAVE, CMD_TEST_BEGIN, CMD_TEST_END, CMD_SOUND };
typedef struct { int kind; bsp_btn_t key; bsp_btn_ev_t event; bool physical; } input_t;
static const char *TAG = "starbridge";
static sb_game game, view;
static int mode = HOME, view_mode = HOME, selection, battery = -1;
static unsigned physical_seen;
static uint8_t sound_level = 3;
static QueueHandle_t inputs;
static lv_obj_t *screen;
static lv_display_t *display;
static nvs_handle_t storage;
static bool storage_ready, dirty, save_failed;
static int64_t save_due;
#define CAPTURE_LINES 20
#define CAPTURE_STRIPES ((BSP_LCD_H + CAPTURE_LINES - 1) / CAPTURE_LINES)
static uint16_t **capture_pixels;
static unsigned capture_rows;

#define BG 0x081425
#define PANEL 0x12253a
#define MUTED 0x9bafc2
#define CYAN 0x58ead0
#define GOLD 0xffcf70
#define WHITE 0xe8f4ff

static void rectangle(lv_layer_t *layer, int x, int y, int w, int h,
                      uint32_t color, uint32_t border, int radius) {
    lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d);
    d.bg_color = lv_color_hex(color); d.radius = radius;
    d.border_color = lv_color_hex(border); d.border_width = border ? 2 : 0;
    lv_area_t area = {x, y, x + w - 1, y + h - 1}; lv_draw_rect(layer, &d, &area);
}

static void line(lv_layer_t *layer, int x, int y, int ex, int ey, uint32_t color, int width) {
    lv_draw_line_dsc_t d; lv_draw_line_dsc_init(&d);
    d.p1.x = x; d.p1.y = y; d.p2.x = ex; d.p2.y = ey;
    d.color = lv_color_hex(color); d.width = width; d.round_start = 1; d.round_end = 1;
    lv_draw_line(layer, &d);
}

static void draw(lv_event_t *e) {
    lv_layer_t *layer = lv_event_get_layer(e);
    if (view_mode != PLAY && view_mode != VICTORY) {
        if (view_mode == HOME) {
            line(layer, 65, 95, 120, 95, CYAN, 6);
            line(layer, 120, 95, 120, 132, CYAN, 6);
            line(layer, 120, 132, 175, 132, CYAN, 6);
            rectangle(layer, 52, 82, 27, 27, GOLD, 0, 14);
            rectangle(layer, 107, 82, 27, 27, PANEL, CYAN, 8);
            rectangle(layer, 162, 119, 27, 27, CYAN, 0, 14);
        }
        return;
    }
    int cell = 192 / view.size, left = (BSP_LCD_W - 192) / 2, top = 72;
    for (unsigned i = 0; i < view.size * view.size; ++i) {
        int x = left + (i % view.size) * cell, y = top + (i / view.size) * cell;
        bool powered = view.powered & (1u << i);
        rectangle(layer, x + 2, y + 2, cell - 4, cell - 4,
                  i == view.cursor ? 0x243b52 : PANEL,
                  i == view.cursor ? GOLD : 0x20364c, 8);
        int cx = x + cell / 2, cy = y + cell / 2;
        uint32_t wire = powered ? CYAN : MUTED;
        if (view.tiles[i] & 1) line(layer, cx, cy, cx, y + 1, wire, 6);
        if (view.tiles[i] & 2) line(layer, cx, cy, x + cell - 1, cy, wire, 6);
        if (view.tiles[i] & 4) line(layer, cx, cy, cx, y + cell - 1, wire, 6);
        if (view.tiles[i] & 8) line(layer, cx, cy, x + 1, cy, wire, 6);
        rectangle(layer, cx - 7, cy - 7, 15, 15, i == 0 ? GOLD : wire, 0, 8);
        if (i == 0) rectangle(layer, cx - 3, cy - 3, 7, 7, BG, 0, 3);
    }
}

static lv_obj_t *label(const char *text, int x, int y, int width, bool small, uint32_t color) {
    lv_obj_t *o = lv_label_create(screen);
    lv_obj_set_style_text_font(o, small ? &starbridge_12 : &starbridge_16, 0);
    lv_obj_set_style_text_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(o, x, y); lv_obj_set_width(o, width); lv_label_set_text(o, text);
    return o;
}

static void button(const char *text, int y, bool selected) {
    lv_obj_t *o = label(text, 28, y, 184, false, selected ? BG : WHITE);
    lv_obj_set_style_bg_color(o, lv_color_hex(selected ? CYAN : PANEL), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0); lv_obj_set_style_radius(o, 9, 0);
    lv_obj_set_style_pad_ver(o, 10, 0);
}

static void refresh(void) {
    if (!bsp_lvgl_lock(1000)) return;
    view = game; view_mode = mode;
    lv_obj_clean(screen);
    label("星桥接线", 20, 14, 112, false, WHITE);
    char text[100];
    if (battery >= 0) snprintf(text, sizeof(text), "%d%%", battery);
    else snprintf(text, sizeof(text), "--%%");
    label(text, 172, 18, 47, true, MUTED);
    if (mode == HOME) {
        label("让沉睡的星点重新发光", 20, 49, 200, true, MUTED);
        snprintf(text, sizeof(text), "第 %02u 关  /  30    星星 %u / 90", game.level + 1, sb_total_stars(&game));
        label(text, 15, 164, 210, true, GOLD);
        button(game.moves || game.level ? "继续旅程" : "开始接线", 191, true);
        label("上下键选关   确定键开始", 20, 250, 200, true, WHITE);
        label("长按确定查看玩法", 24, 281, 192, true, MUTED);
    } else if (mode == PLAY || mode == VICTORY) {
        snprintf(text, sizeof(text), "%02u / 30     点亮 %u / %u     步数 %u", game.level + 1,
                 sb_powered_count(&game), game.size * game.size, game.moves);
        label(text, 16, 44, 208, true, CYAN);
        if (mode == VICTORY) {
            snprintf(text, sizeof(text), "接通成功！  %u 星  ·  确定继续", game.stars[game.level]);
            label(text, 18, 274, 204, true, GOLD);
            label(game.level == SB_LEVELS - 1 ? "全部完成！确定返回选关" : "长按确定返回菜单", 20, 295, 200, true, MUTED);
        } else {
            label("上下选择   确定旋转   长按菜单", 15, 274, 210, true, WHITE);
            if (save_failed) snprintf(text, sizeof(text), "保存失败：请保留电源并重试");
            else if (dirty) snprintf(text, sizeof(text), "保存中  ·  长按上提示 / 下撤销");
            else snprintf(text, sizeof(text), "长按上提示 / 下撤销  ·  目标 %u 步", game.par);
            label(text, 14, 295, 212, true, save_failed ? GOLD : MUTED);
        }
    } else if (mode == PAUSE) {
        label("稍作停留", 20, 54, 200, false, GOLD);
        snprintf(text, sizeof(text), "音量：%u%%", sound_level * 25);
        const char *items[] = {"继续接线", "重新挑战", "选择关卡", "玩法说明", text};
        for (int i = 0; i < 5; ++i) button(items[i], 85 + 39 * i, selection == i);
        label("上下选择   确定进入", 20, 290, 200, true, MUTED);
    } else if (mode == LEVELS) {
        label("星桥航图", 20, 55, 200, false, GOLD);
        snprintf(text, sizeof(text), "第 %02d 关", selection + 1);
        button(text, 111, true);
        snprintf(text, sizeof(text), "%s  ·  已获 %u 星", selection < 10 ? "启程  3 × 3" : "深空  4 × 4", game.stars[selection]);
        label(text, 20, 171, 200, false, WHITE);
        snprintf(text, sizeof(text), "已解锁 %u / 30 关", game.unlocked + 1);
        label(text, 20, 219, 200, true, MUTED);
        label("上下选关   确定出发", 20, 270, 200, true, WHITE);
        label("长按确定返回", 20, 292, 200, true, MUTED);
    } else {
        label("接通每一颗星", 20, 59, 200, false, GOLD);
        label("金色圆点是能量起点\n旋转线路，让所有星点变绿\n上下键逐格移动，跨行循环\n确定键顺时针旋转一次\n长按上：帮你接好一格\n长按下：撤销上一次旋转\n长按确定：打开暂停菜单", 18, 98, 204, true, WHITE);
        label("目标步数内无提示：三星\n超出步数：两星   使用提示：一星\n自动保存，断电后继续", 16, 219, 208, true, MUTED);
        button("确定出发", 271, true);
    }
    lv_obj_invalidate(screen);
    bsp_lvgl_unlock();
}

static void status(void) {
    sb_audio_stats audio = sb_audio_status();
    printf("SB_STATE {\"mode\":%d,\"level\":%u,\"cursor\":%u,\"moves\":%u,\"par\":%u,\"hints\":%u,\"powered\":%u,\"won\":%s,\"unlocked\":%u,\"stars\":%u,\"dirty\":%s,\"save_ok\":%s,\"physical\":%u,\"heap\":%lu,\"min_heap\":%lu,\"tiles\":[",
           mode, game.level, game.cursor, game.moves, game.par, game.hints, sb_powered_count(&game),
           game.won ? "true" : "false", game.unlocked, sb_total_stars(&game), dirty ? "true" : "false",
           storage_ready && !save_failed ? "true" : "false", physical_seen,
           (unsigned long)esp_get_free_heap_size(), (unsigned long)esp_get_minimum_free_heap_size());
    for (unsigned i = 0; i < game.size * game.size; ++i) printf("%s%u", i ? "," : "", game.tiles[i]);
    printf("],\"sound\":%u,\"audio\":{\"ready\":%s,\"failed\":%s,\"volume\":%u,\"blocks\":%u,\"max_gap_us\":%u,\"max_render_us\":%u,\"late_feeds\":%u,\"dropped\":%u,\"peak\":%u,\"effects\":[",
           sound_level, audio.ready ? "true" : "false", audio.failed ? "true" : "false",
           audio.volume, audio.blocks, audio.max_gap_us, audio.max_render_us, audio.late_feeds, audio.dropped, audio.peak);
    for (unsigned i = 0; i < SB_SOUND_COUNT; ++i) printf("%s%u", i ? "," : "", audio.effects[i]);
    printf("]}}\n"); fflush(stdout);
}

static void save(void) {
    if (!dirty) return;
    uint8_t bytes[SB_SAVE_SIZE]; sb_encode(&game, bytes);
    esp_err_t error = storage_ready ? nvs_set_blob(storage, "progress", bytes, sizeof(bytes)) : ESP_FAIL;
    if (error == ESP_OK) error = nvs_set_u8(storage, "sound", sound_level);
    if (error == ESP_OK) error = nvs_commit(storage);
    save_failed = error != ESP_OK;
    if (!save_failed) { dirty = false; ESP_LOGI(TAG, "SAVE_OK level=%u moves=%u", game.level, game.moves); }
    else { ESP_LOGW(TAG, "SAVE_FAILED %s", esp_err_to_name(error)); save_due = esp_timer_get_time() + 5000000; }
}

static void mark_dirty(void) { dirty = true; save_due = esp_timer_get_time() + 1000000; }

static void toggle_sound(void) {
    sound_level = (sound_level + 1) % 5;
    sb_audio_update(sound_level, mode == PLAY, SB_SILENT);
    mark_dirty(); save();
}

// Local USB QA keeps a persistent checkpoint so a reboot test never loses the player's progress.
static void test_checkpoint(bool restore) {
    uint8_t bytes[SB_SAVE_SIZE]; size_t length = sizeof(bytes);
    if (!storage_ready) { puts("SB_TEST_ERROR storage"); return; }
    esp_err_t error = nvs_get_blob(storage, "qa_restore", bytes, &length);
    if (!restore) {
        if (error != ESP_ERR_NVS_NOT_FOUND) { puts("SB_TEST_ERROR checkpoint_exists"); return; }
        sb_encode(&game, bytes);
        error = nvs_set_blob(storage, "qa_restore", bytes, sizeof(bytes));
        if (error == ESP_OK) error = nvs_set_u8(storage, "qa_sound", sound_level);
        if (error == ESP_OK) error = nvs_commit(storage);
        if (error != ESP_OK) { puts("SB_TEST_ERROR checkpoint_write"); return; }
        sb_new(&game); mode = HOME; mark_dirty(); save(); refresh();
        puts("SB_TEST_BEGIN_OK");
    } else {
        sb_game restored;
        if (error != ESP_OK || !sb_decode(&restored, bytes, length)) { puts("SB_TEST_ERROR checkpoint_invalid"); return; }
        uint8_t restored_sound = sound_level;
        (void)nvs_get_u8(storage, "qa_sound", &restored_sound);
        if (restored_sound <= 4) sound_level = restored_sound;
        sb_audio_update(sound_level, false, SB_SILENT);
        game = restored; mode = HOME; mark_dirty(); save();
        if (dirty) { puts("SB_TEST_ERROR restore_write"); return; }
        error = nvs_erase_key(storage, "qa_restore");
        (void)nvs_erase_key(storage, "qa_sound");
        if (error == ESP_OK) error = nvs_commit(storage);
        refresh(); puts(error == ESP_OK ? "SB_TEST_END_OK" : "SB_TEST_ERROR checkpoint_cleanup");
    }
}

static void key(bsp_btn_t btn, bsp_btn_ev_t event) {
    if (event != BSP_BTN_CLICK && event != BSP_BTN_LONG && event != BSP_BTN_DOUBLE) return;
    // A double click represents two clicks suppressed by the underlying button component.
    if (event == BSP_BTN_DOUBLE) { key(btn, BSP_BTN_CLICK); key(btn, BSP_BTN_CLICK); return; }
    bool longpress = event == BSP_BTN_LONG;
    if (btn == BSP_BTN_OK && longpress) {
        if (mode == HOME) mode = HELP;
        else if (mode == PAUSE || mode == HELP || mode == LEVELS) mode = game.won ? VICTORY : PLAY;
        else { mode = PAUSE; selection = 0; }
        save(); return;
    }
    if (mode == PLAY) {
        bool changed = false;
        if (longpress) changed = btn == BSP_BTN_UP ? sb_hint(&game) : sb_undo(&game);
        else if (btn == BSP_BTN_UP) sb_move(&game, -1);
        else if (btn == BSP_BTN_DOWN) sb_move(&game, 1);
        else changed = sb_turn(&game);
        if (changed) mark_dirty();
        if (game.won) { mode = VICTORY; save(); }
    } else if (longpress) return;
    else if (mode == HOME) {
        if (btn == BSP_BTN_OK) mode = game.moves || game.level ? (game.won ? VICTORY : PLAY) : HELP;
        else { mode = LEVELS; selection = game.level; }
    } else if (mode == HELP) {
        if (btn == BSP_BTN_OK) mode = game.won ? VICTORY : PLAY;
    } else if (mode == VICTORY) {
        if (btn == BSP_BTN_OK) {
            if (game.level == SB_LEVELS - 1) { mode = LEVELS; selection = 0; }
            else { sb_load_level(&game, game.level + 1); mode = PLAY; mark_dirty(); save(); }
        }
    } else if (mode == PAUSE) {
        if (btn == BSP_BTN_UP) selection = (selection + 4) % 5;
        else if (btn == BSP_BTN_DOWN) selection = (selection + 1) % 5;
        else if (selection == 0) mode = game.won ? VICTORY : PLAY;
        else if (selection == 1) { sb_load_level(&game, game.level); mode = PLAY; mark_dirty(); save(); }
        else if (selection == 2) { mode = LEVELS; selection = game.level; }
        else if (selection == 3) mode = HELP;
        else toggle_sound();
    } else if (mode == LEVELS) {
        int n = game.unlocked + 1;
        if (btn == BSP_BTN_UP) selection = (selection + n - 1) % n;
        else if (btn == BSP_BTN_DOWN) selection = (selection + 1) % n;
        else { sb_load_level(&game, (unsigned)selection); mode = PLAY; mark_dirty(); save(); }
    }
}

static void capture_event(lv_event_t *e) {
    if (!capture_pixels) return;
    const lv_area_t *area = lv_event_get_param(e);
    lv_draw_buf_t *buf = lv_display_get_buf_active(display);
    if (!area || !buf || !buf->data) return;
    for (int y = area->y1; y <= area->y2; ++y) {
        if (y < 0 || y >= BSP_LCD_H) continue;
        for (int x = area->x1; x <= area->x2; ++x) {
            if (x < 0 || x >= BSP_LCD_W) continue;
            uint16_t pixel;
            memcpy(&pixel, buf->data + (y - area->y1) * buf->header.stride + (x - area->x1) * 2, 2);
            capture_pixels[y / CAPTURE_LINES][(y % CAPTURE_LINES) * BSP_LCD_W + x] = pixel;
        }
        if (area->x1 == 0 && area->x2 == BSP_LCD_W - 1) ++capture_rows;
    }
}

static void capture(void) {
    uint16_t *pixels[CAPTURE_STRIPES] = {0};
    for (unsigned i = 0; i < CAPTURE_STRIPES; ++i) {
        pixels[i] = calloc(BSP_LCD_W * CAPTURE_LINES, sizeof(uint16_t));
        if (!pixels[i]) {
            for (unsigned j = 0; j < i; ++j) free(pixels[j]);
            puts("SB_CAPTURE_ERROR memory"); return;
        }
    }
    if (!bsp_lvgl_lock(2000)) {
        for (unsigned i = 0; i < CAPTURE_STRIPES; ++i) free(pixels[i]);
        puts("SB_CAPTURE_ERROR lock"); return;
    }
    capture_rows = 0; capture_pixels = pixels;
    lv_obj_invalidate(screen); lv_refr_now(display);
    capture_pixels = NULL; bsp_lvgl_unlock();
    printf("SB_FRAME_BEGIN %d %d %u\n", BSP_LCD_W, BSP_LCD_H, capture_rows);
    static const char hex[] = "0123456789abcdef";
    char row[BSP_LCD_W * 4 + 1];
    for (int y = 0; y < BSP_LCD_H; ++y) {
        printf("SB_ROW %03d ", y);
        for (int x = 0; x < BSP_LCD_W; ++x) {
            uint16_t p = pixels[y / CAPTURE_LINES][(y % CAPTURE_LINES) * BSP_LCD_W + x];
            for (unsigned n = 0; n < 4; ++n) row[x * 4 + n] = hex[(p >> (12 - n * 4)) & 15];
        }
        row[BSP_LCD_W * 4] = 0; puts(row);
        // Yield between rows so USB transport and audio DMA both make progress.
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    puts("SB_FRAME_END"); fflush(stdout);
    for (unsigned i = 0; i < CAPTURE_STRIPES; ++i) free(pixels[i]);
}

static void on_button(bsp_btn_t btn, bsp_btn_ev_t event, void *user) {
    (void)user;
    input_t in = {.kind = CMD_KEY, .key = btn, .event = event, .physical = true};
    (void)xQueueSend(inputs, &in, 0);
}

static void serial_task(void *arg) {
    (void)arg;
    for (;;) {
        int c = getchar();
        if (c == EOF) { clearerr(stdin); vTaskDelay(pdMS_TO_TICKS(20)); continue; }
        input_t in = {.kind = CMD_KEY, .event = BSP_BTN_CLICK};
        if (c == 'u' || c == 'U') in.key = BSP_BTN_UP;
        else if (c == 'd' || c == 'D') in.key = BSP_BTN_DOWN;
        else if (c == 'o' || c == 'O') in.key = BSP_BTN_OK;
        else if (c == '?') in.kind = CMD_STATUS;
        else if (c == 'c') in.kind = CMD_CAPTURE;
        else if (c == 's') in.kind = CMD_SAVE;
        else if (c == 'b') in.kind = CMD_TEST_BEGIN;
        else if (c == 'e') in.kind = CMD_TEST_END;
        else if (c == 'm') in.kind = CMD_SOUND;
        else continue;
        if (c == 'U' || c == 'D' || c == 'O') in.event = BSP_BTN_LONG;
        xQueueSend(inputs, &in, portMAX_DELAY);
    }
}

void app_main(void) {
    // The default polling console drops bytes after 50 ms of backpressure.
    // Buffer USB output so full display captures can coexist with music playback.
    usb_serial_jtag_driver_config_t usb = {.tx_buffer_size = 8192, .rx_buffer_size = 256};
    if (usb_serial_jtag_driver_install(&usb) == ESP_OK) usb_serial_jtag_vfs_use_driver();
    ESP_LOGI(TAG, "STARBRIDGE 1.0.0 BOOT");
    sb_new(&game);
    esp_err_t err = nvs_flash_init();
    if (err == ESP_OK) err = nvs_open("starbridge", NVS_READWRITE, &storage);
    storage_ready = err == ESP_OK;
    if (storage_ready) {
        uint8_t saved_sound;
        if (nvs_get_u8(storage, "sound", &saved_sound) == ESP_OK && saved_sound <= 4) sound_level = saved_sound;
        uint8_t bytes[SB_SAVE_SIZE]; size_t length = sizeof(bytes);
        if (nvs_get_blob(storage, "progress", bytes, &length) == ESP_OK && sb_decode(&game, bytes, length))
            ESP_LOGI(TAG, "RESTORE_OK level=%u moves=%u", game.level, game.moves);
    } else { save_failed = true; ESP_LOGW(TAG, "NVS unavailable; existing data preserved"); }
    ESP_ERROR_CHECK(bsp_i2c_init());
    ESP_ERROR_CHECK(bsp_display_init());
    display = bsp_lvgl_init(); if (!display) { ESP_LOGE(TAG, "LVGL failed"); return; }
    if (bsp_battery_init() == ESP_OK) battery = bsp_battery_soc();
    inputs = xQueueCreate(24, sizeof(input_t)); if (!inputs) abort();
    if (!bsp_lvgl_lock(1000)) abort();
    screen = lv_obj_create(NULL); lv_obj_remove_style_all(screen);
    lv_obj_set_size(screen, BSP_LCD_W, BSP_LCD_H);
    lv_obj_set_style_bg_color(screen, lv_color_hex(BG), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(screen, draw, LV_EVENT_DRAW_MAIN, NULL);
    lv_display_add_event_cb(display, capture_event, LV_EVENT_FLUSH_START, NULL);
    lv_screen_load(screen); bsp_lvgl_unlock();
    refresh();
    if (bsp_lvgl_lock(1000)) { lv_refr_now(display); bsp_lvgl_unlock(); }
    bsp_display_backlight(70);
    if (!sb_audio_start(sound_level)) ESP_LOGW(TAG, "Audio worker unavailable; game remains playable");
    ESP_ERROR_CHECK(bsp_button_init(on_button, NULL));
    if (xTaskCreate(serial_task, "sb_serial", 3072, NULL, 3, NULL) != pdPASS) abort();
    ESP_LOGI(TAG, "READY display=1 buttons=1 storage=%d battery=%d", storage_ready, battery);
    status();
    int64_t battery_due = esp_timer_get_time() + 30000000;
    for (;;) {
        input_t in;
        if (xQueueReceive(inputs, &in, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (in.kind == CMD_KEY) {
                if (in.physical && in.event != BSP_BTN_PRESS) {
                    physical_seen |= 1u << (unsigned)in.key;
                    ESP_LOGI(TAG, "PHYSICAL key=%d event=%d mv=%d", in.key, in.event, bsp_button_read_mv());
                }
                int before_mode = mode;
                unsigned before_moves = game.moves, before_hints = game.hints;
                unsigned before_power = sb_powered_count(&game);
                key(in.key, in.event);
                if (in.event != BSP_BTN_PRESS) {
                    sb_sound effect = SB_SILENT;
                    if (mode == VICTORY && before_mode != VICTORY) effect = SB_WIN;
                    else if (before_mode == PLAY && game.hints == before_hints && game.moves != before_moves)
                        effect = sb_powered_count(&game) > before_power ? SB_CONNECT : SB_TURN;
                    sb_audio_update(sound_level, mode == PLAY, effect);
                }
                if (in.event != BSP_BTN_PRESS) refresh();
            } else if (in.kind == CMD_CAPTURE) capture();
            else if (in.kind == CMD_SAVE) { save(); refresh(); }
            else if (in.kind == CMD_TEST_BEGIN) test_checkpoint(false);
            else if (in.kind == CMD_TEST_END) test_checkpoint(true);
            else if (in.kind == CMD_SOUND) { toggle_sound(); refresh(); }
            if (in.event != BSP_BTN_PRESS || in.kind != CMD_KEY) status();
        }
        int64_t now = esp_timer_get_time();
        if (dirty && now >= save_due) { save(); refresh(); }
        if (now >= battery_due) { battery = bsp_battery_soc(); battery_due = now + 30000000; refresh(); }
    }
}
