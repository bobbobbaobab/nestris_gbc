#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>
#include "utils/cbtfx.h"
#include "utils/hUGEDriver.h"

#include "gfx/font.h"
#include "gfx/background.h"
#include "gfx/tetromino.h"
#include "snd/SFX_LEVELUP.h"
#include "snd/SFX_LINECLEAR.h"
#include "snd/SFX_LOCK.h"
#include "snd/SFX_MOVE.h"
#include "snd/SFX_ROTATE.h"
#include "snd/SFX_SELECT.h"
#include "snd/SFX_TETRIS.h"

#define BOARD_W 10
#define BOARD_H 18
#define VISIBLE_H 18
#define BOARD_BKG_X 1
#define BOARD_BKG_Y 0
#define NEXT_BKG_X 13
#define NEXT_BKG_Y 8
#define EMPTY_CELL 0xFFu

#define DAS_DELAY_DEFAULT 16
#define DAS_DELAY_MAX 16
#define ARR_DELAY_DEFAULT 5
#define ARR_DELAY_MAX 5
#define MENU_ITEM_LEVEL 0
#define MENU_ITEM_MUSIC 1
#define MENU_ITEM_DAS 2
#define MENU_ITEM_ARR 3
#define MENU_ITEM_COUNT 4
#define DPAD_LOCK_NONE 0
#define DPAD_LOCK_HORIZONTAL 1
#define DPAD_LOCK_DOWN 2
#define CLEAR_ANIM_FRAME_DELAY 3
#define TETRIS_FLASH_COUNT 5
#define TETRIS_FLASH_INTERVAL 4
#define GAME_PALETTE_ID 1
#define FLASH_PALETTE_ID 2

typedef struct Tetromino {
    int8_t x;
    int8_t y;
    uint8_t shape;
    uint8_t rot;
} Tetromino;

uint8_t joyPadCurrent = 0;
uint8_t joyPadPrevious = 0;
static uint8_t dpad_lock_axis = DPAD_LOCK_NONE;

static uint8_t board[BOARD_H][BOARD_W];
static Tetromino current;
static Tetromino next;
static uint16_t frame_counter;
static uint32_t score;
static uint32_t high_score;
static uint16_t lines;
static uint16_t tetris_lines;
static uint8_t level;
static uint8_t selected_level;
static uint8_t selected_menu_item;
static uint8_t selected_music = 1;
static uint8_t selected_das_delay = DAS_DELAY_DEFAULT;
static uint8_t selected_arr_delay = ARR_DELAY_DEFAULT;
static uint16_t transition_lines;
static uint8_t gravity_delay;
static int8_t lock_delay;
static uint8_t push_down_points;
static uint8_t dht;
static uint8_t trt;
static uint8_t das;
static uint8_t first_tap;
static uint8_t fall_release;
static uint8_t game_started;
static uint8_t game_over;
static uint8_t game_paused;
static int8_t tuck_tweak;
static uint8_t tuck_t;
static uint8_t tuck_wait;
static uint8_t clear_rows[4];
static uint8_t clear_row_count;
static uint8_t clear_anim_phase;
static uint8_t clear_anim_timer;
static uint8_t tetris_flash_count;
static uint8_t tetris_flash_timer;
static uint8_t tetris_flash_white;
static uint8_t tetris_flash_palette_dirty;
static uint8_t game_palette_dirty;
static uint16_t rng_state = 0xACE1u;

static const palette_color_t game_palettes[10][4] = {
    {RGB8(  0,   0,   0), RGB8(  0,  88, 248), RGB8( 60, 188, 252), RGB8(255, 255, 255)},
    {RGB8(  0,   0,   0), RGB8(  0, 168,   0), RGB8(148, 248,  24), RGB8(255, 255, 255)},
    {RGB8(  0,   0,   0), RGB8(216,   0, 204), RGB8(248, 120, 248), RGB8(255, 255, 255)},
    {RGB8(  0,   0,   0), RGB8(  0,  88, 248), RGB8( 88, 216,  84), RGB8(255, 255, 255)},
    {RGB8(  0,   0,   0), RGB8(228,   0,  88), RGB8( 88, 248, 152), RGB8(255, 255, 255)},
    {RGB8(  0,   0,   0), RGB8( 88, 248, 152), RGB8(104, 136, 252), RGB8(255, 255, 255)},
    {RGB8(  0,   0,   0), RGB8(248,  56,   0), RGB8(124, 124, 124), RGB8(255, 255, 255)},
    {RGB8(  0,   0,   0), RGB8(104,  68, 252), RGB8(168,   0,  32), RGB8(255, 255, 255)},
    {RGB8(  0,   0,   0), RGB8(  0,  88, 248), RGB8(248,  56,   0), RGB8(255, 255, 255)},
    {RGB8(  0,   0,   0), RGB8(248,  56,   0), RGB8(252, 160,  68), RGB8(255, 255, 255)}
};

static const palette_color_t flash_palette[] = {
    RGB8(255, 255, 255),
    RGB8(255, 255, 255),
    RGB8(255, 255, 255),
    RGB8(255, 255, 255)
};

typedef struct SaveData {
    uint16_t magic;
    uint32_t high_score;
    uint16_t checksum;
    uint8_t selected_level;
    uint8_t music_on;
    uint8_t das_delay;
    uint8_t arr_delay;
    uint16_t settings_checksum;
} SaveData;

extern SaveData save_data;
extern const hUGESong_t song_descriptor;

#define SAVE_MAGIC 0x4E54u

static const uint8_t diff_map[20] = {
    48, 43, 38, 33, 28, 23, 18, 13, 8, 6,
     5,  5,  5,  4,  4,  4,  3,  3, 3, 2
};

static uint8_t music_active;
static uint8_t music_paused;

/* Classic Picotris shape order: O, T, I, S, Z, L, J. */
static const int8_t shape_offsets[7][4][4][2] = {
    {
        {{ 0, 0}, {-1, 0}, { 0, 1}, {-1, 1}},
        {{ 0, 0}, {-1, 0}, { 0, 1}, {-1, 1}},
        {{ 0, 0}, {-1, 0}, { 0, 1}, {-1, 1}},
        {{ 0, 0}, {-1, 0}, { 0, 1}, {-1, 1}}
    },
    {
        {{ 0, 0}, {-1, 0}, { 1, 0}, { 0, 1}},
        {{ 0, 0}, {-1, 0}, { 0,-1}, { 0, 1}},
        {{ 0, 0}, {-1, 0}, { 1, 0}, { 0,-1}},
        {{ 0, 0}, { 0,-1}, { 0, 1}, { 1, 0}}
    },
    {
        {{ 0, 0}, {-1, 0}, {-2, 0}, { 1, 0}},
        {{ 0, 0}, { 0, 1}, { 0,-1}, { 0,-2}},
        {{ 0, 0}, {-1, 0}, {-2, 0}, { 1, 0}},
        {{ 0, 0}, { 0, 1}, { 0,-1}, { 0,-2}}
    },
    {
        {{ 0, 0}, { 0, 1}, {-1, 1}, { 1, 0}},
        {{ 0, 0}, { 0,-1}, { 1, 0}, { 1, 1}},
        {{ 0, 0}, { 0, 1}, {-1, 1}, { 1, 0}},
        {{ 0, 0}, { 0,-1}, { 1, 0}, { 1, 1}}
    },
    {
        {{ 0, 1}, { 0, 0}, {-1, 0}, { 1, 1}},
        {{ 0, 1}, { 0, 0}, { 1, 0}, { 1,-1}},
        {{ 0, 1}, { 0, 0}, {-1, 0}, { 1, 1}},
        {{ 0, 1}, { 0, 0}, { 1, 0}, { 1,-1}}
    },
    {
        {{ 0, 0}, {-1, 0}, { 1, 0}, {-1, 1}},
        {{ 0, 0}, { 0,-1}, { 0, 1}, {-1,-1}},
        {{ 0, 0}, {-1, 0}, { 1, 0}, { 1,-1}},
        {{ 0, 0}, { 0,-1}, { 0, 1}, { 1, 1}}
    },
    {
        {{ 0, 0}, {-1, 0}, { 1, 0}, { 1, 1}},
        {{ 0, 0}, { 0,-1}, { 0, 1}, {-1, 1}},
        {{ 0, 0}, {-1, 0}, { 1, 0}, {-1,-1}},
        {{ 0, 0}, { 0, 1}, { 0,-1}, { 1,-1}}
    }
};

static const uint8_t piece_tile[7] = {
    0, /* O */
    0, /* T */
    0, /* I */
    2, /* S */
    1, /* Z */
    1, /* L */
    2  /* J */
};

static void spawn_next_piece(void);
static void reset_game(void);
static void apply_palette_updates(void);

static void play_sfx_select(void) {
    CRITICAL {
        CBTFX_PLAY_SFX_SELECT;
    }
}

static void play_sfx_move(void) {
    CRITICAL {
        CBTFX_PLAY_SFX_MOVE;
    }
}

static void play_sfx_rotate(void) {
    CRITICAL {
        CBTFX_PLAY_SFX_ROTATE;
    }
}

static void play_sfx_levelup(void) {
    CRITICAL {
        CBTFX_PLAY_SFX_LEVELUP;
    }
}

static void play_sfx_lock(void) {
    CRITICAL {
        CBTFX_PLAY_SFX_LOCK;
    }
}

static void play_sfx_lineclear(void) {
    CRITICAL {
        CBTFX_PLAY_SFX_LINECLEAR;
    }
}

static void play_sfx_tetris(void) {
    CRITICAL {
        CBTFX_PLAY_SFX_TETRIS;
    }
}

static void silence_music_channels(void) {
    NR10_REG = NR11_REG = NR12_REG = NR13_REG = NR14_REG = 0;
    NR21_REG = NR22_REG = NR23_REG = NR24_REG = 0;
    NR30_REG = NR31_REG = NR32_REG = NR33_REG = NR34_REG = 0;
    NR41_REG = NR42_REG = NR43_REG = NR44_REG = 0;
}

static void mute_music_channels(uint8_t mute) {
    hUGE_mute_channel(HT_CH1, mute);
    hUGE_mute_channel(HT_CH2, mute);
    hUGE_mute_channel(HT_CH3, mute);
    hUGE_mute_channel(HT_CH4, mute);
}

static void start_music(void) {
    if (!selected_music) {
        music_active = 0;
        music_paused = 0;
        silence_music_channels();
        return;
    }

    CRITICAL {
        hUGE_init(&song_descriptor);
        mute_music_channels(HT_CH_PLAY);
        music_paused = 0;
        music_active = 1;
    }
}

static void pause_music(void) {
    if (!music_active || music_paused) return;

    CRITICAL {
        music_paused = 1;
        mute_music_channels(HT_CH_MUTE);
        silence_music_channels();
    }
}

static void resume_music(void) {
    if (!music_active || !music_paused) return;

    CRITICAL {
        mute_music_channels(HT_CH_PLAY);
        music_paused = 0;
    }
}

static void stop_music(void) {
    if (!music_active && !music_paused) return;

    CRITICAL {
        music_active = 0;
        music_paused = 0;
        mute_music_channels(HT_CH_MUTE);
        silence_music_channels();
    }
}

static void audio_vblank(void) {
    apply_palette_updates();
    if (music_active && !music_paused) hUGE_dosound();
    CBTFX_update();
}

static void start_tuck_tweak(uint8_t side) {
    uint8_t window = gravity_delay + 3u;

    tuck_tweak = (int8_t)side;
    tuck_t = (window < 8u) ? window : 8u;
}

static uint8_t btn_pressed(uint8_t mask) {
    return (joyPadCurrent & mask) && !(joyPadPrevious & mask);
}

static uint8_t btn_held(uint8_t mask) {
    return joyPadCurrent & mask;
}

static uint8_t reset_combo_pressed(void) {
    const uint8_t combo = J_A | J_B | J_START | J_SELECT;

    return ((joyPadCurrent & combo) == combo) && ((joyPadPrevious & combo) != combo);
}

static uint8_t repeat_reset_value(void) {
    return (selected_arr_delay < selected_das_delay) ? (selected_das_delay - selected_arr_delay) : 0u;
}

static uint8_t apply_dpad_diagonal_lock(uint8_t input) {
    uint8_t has_horizontal = input & (J_LEFT | J_RIGHT);
    uint8_t has_down = input & J_DOWN;

    if (has_horizontal && has_down) {
        if (dpad_lock_axis == DPAD_LOCK_DOWN) {
            input &= (uint8_t)~(J_LEFT | J_RIGHT);
        } else {
            dpad_lock_axis = DPAD_LOCK_HORIZONTAL;
            input &= (uint8_t)~J_DOWN;
        }
    } else if (has_horizontal) {
        dpad_lock_axis = DPAD_LOCK_HORIZONTAL;
    } else if (has_down) {
        dpad_lock_axis = DPAD_LOCK_DOWN;
    } else {
        dpad_lock_axis = DPAD_LOCK_NONE;
    }

    return input;
}

static uint8_t normalize_rot(uint8_t shape, int8_t rot) {
    if (shape == 0) return 0;
    if ((shape == 2) || (shape == 3) || (shape == 4)) return rot & 1;
    return rot & 3;
}

static void set_bkg_palette_area(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t palette_id) {
    uint8_t row[10];
    uint8_t i;
    uint8_t yy;
    uint8_t attr = palette_id & 0x07u;

    for (i = 0; i < w; i++) row[i] = attr;

    VBK_REG = 1;
    for (yy = 0; yy < h; yy++) set_bkg_tiles(x, y + yy, w, 1, row);
    VBK_REG = 0;
}

static void set_bkg_palette_cell(uint8_t x, uint8_t y, uint8_t palette_id) {
    uint8_t attr = palette_id & 0x07u;

    VBK_REG = 1;
    set_bkg_tiles(x, y, 1, 1, &attr);
    VBK_REG = 0;
}

static void set_game_area_palette(uint8_t palette_id) {
    uint8_t i;

    set_bkg_palette_area(1, 0, 10, 18, palette_id);
    set_bkg_palette_area(13, 8, 4, 2, palette_id);

    for (i = 0; i < 8; i++) set_sprite_prop(i, palette_id);
}

static uint8_t background_tile_at(uint8_t x, uint8_t y) {
    return background_map[(uint16_t)y * 20u + x];
}

static void restore_background_cell(uint8_t x, uint8_t y) {
    uint8_t tile = background_tile_at(x, y);
    set_bkg_tiles(x, y, 1, 1, &tile);
}

static void draw_board_cell(uint8_t row, uint8_t col) {
    uint8_t screen_y;
    uint8_t screen_x;
    uint8_t tile;

    screen_y = BOARD_BKG_Y + row;
    screen_x = BOARD_BKG_X + col;

    if (board[row][col] == EMPTY_CELL) {
        restore_background_cell(screen_x, screen_y);
        set_bkg_palette_cell(screen_x, screen_y, FLASH_PALETTE_ID);
    } else {
        tile = board[row][col];
        set_bkg_tiles(screen_x, screen_y, 1, 1, &tile);
        set_bkg_palette_cell(screen_x, screen_y, GAME_PALETTE_ID);
    }
}

static void redraw_board(void) {
    uint8_t row;
    uint8_t col;

    for (row = 0; row < BOARD_H; row++) {
        for (col = 0; col < BOARD_W; col++) draw_board_cell(row, col);
    }
}

static void set_tetris_flash_white(uint8_t white) {
    tetris_flash_white = white;
    tetris_flash_palette_dirty = 1;
}

static void mark_game_palette_dirty(void) {
    game_palette_dirty = 1;
    if (!tetris_flash_white) tetris_flash_palette_dirty = 1;
}

static void apply_palette_updates(void) {
    const palette_color_t *current_game_palette = game_palettes[level % 10u];

    if (game_palette_dirty) {
        set_bkg_palette(GAME_PALETTE_ID, 1, current_game_palette);
        set_sprite_palette(GAME_PALETTE_ID, 1, current_game_palette);
        game_palette_dirty = 0;
    }

    if (!tetris_flash_palette_dirty) return;
    set_bkg_palette(FLASH_PALETTE_ID, 1, tetris_flash_white ? flash_palette : current_game_palette);
    tetris_flash_palette_dirty = 0;
}

static void start_tetris_flash(void) {
    tetris_flash_count = TETRIS_FLASH_COUNT;
    tetris_flash_timer = 0;
    set_tetris_flash_white(1);
    tetris_flash_count--;
}

static void update_tetris_flash(void) {
    if (!tetris_flash_white && !tetris_flash_count) return;

    if (tetris_flash_white) {
        set_tetris_flash_white(0);
        if (tetris_flash_count) tetris_flash_timer = TETRIS_FLASH_INTERVAL - 1u;
        return;
    }

    if (tetris_flash_timer > 0) tetris_flash_timer--;
    if (tetris_flash_timer == 0) {
        set_tetris_flash_white(1);
        tetris_flash_count--;
    }
}

static void stop_tetris_flash(void) {
    tetris_flash_count = 0;
    tetris_flash_timer = 0;
    set_tetris_flash_white(0);
}

static void clear_next_box(void) {
    uint8_t x;
    uint8_t y;

    for (y = 0; y < 2; y++) {
        for (x = 0; x < 4; x++) restore_background_cell(NEXT_BKG_X + x, NEXT_BKG_Y + y);
    }
}

static void draw_next_piece(void) {
    uint8_t i;
    uint8_t tile = piece_tile[next.shape];
    uint8_t x;
    uint8_t y;

    clear_next_box();

    for (i = 0; i < 4; i++) {
        switch (next.shape) {
            case 0:
                x = (i & 1u) + 1u;
                y = (i >> 1);
                break;
            case 2:
                x = i;
                y = 1;
                break;
            default:
                x = (uint8_t)(shape_offsets[next.shape][0][i][0] + 1);
                y = (uint8_t)shape_offsets[next.shape][0][i][1];
                break;
        }
        set_bkg_tiles(NEXT_BKG_X + x, NEXT_BKG_Y + y, 1, 1, &tile);
    }
}

static void hide_current_sprites(void) {
    uint8_t i;
    for (i = 0; i < 4; i++) move_sprite(i, 0, 0);
}

static void draw_current_piece(void) {
    uint8_t i;
    int8_t row;
    int8_t col;
    uint8_t tile = piece_tile[current.shape];

    for (i = 0; i < 4; i++) {
        col = current.x + shape_offsets[current.shape][current.rot][i][0];
        row = current.y + shape_offsets[current.shape][current.rot][i][1];
        set_sprite_tile(i, tile);
        set_sprite_prop(i, 1);

        if ((row >= 0) && (row < BOARD_H) && (col >= 0) && (col < BOARD_W)) {
            move_sprite(
                i,
                (uint8_t)((BOARD_BKG_X + col) * 8 + 8),
                (uint8_t)((BOARD_BKG_Y + row) * 8 + 16)
            );
        } else {
            move_sprite(i, 0, 0);
        }
    }
}

static uint16_t next_rand(void) {
    uint8_t lsb = rng_state & 1u;
    rng_state >>= 1;
    if (lsb) rng_state ^= 0xB400u;
    rng_state += DIV_REG;
    return rng_state;
}

static uint8_t random_shape(void) {
    return (uint8_t)(next_rand() % 7u);
}

static Tetromino make_piece(uint8_t shape) {
    Tetromino piece;
    piece.x = 5;
    piece.y = 0;
    piece.shape = shape;
    piece.rot = 0;
    return piece;
}

static uint8_t can_place(const Tetromino *piece) {
    uint8_t i;
    int8_t row;
    int8_t col;

    for (i = 0; i < 4; i++) {
        col = piece->x + shape_offsets[piece->shape][piece->rot][i][0];
        row = piece->y + shape_offsets[piece->shape][piece->rot][i][1];

        if ((col < 0) || (col >= BOARD_W) || (row >= BOARD_H)) return 0;
        if ((row >= 0) && (board[row][col] != EMPTY_CELL)) return 0;
    }
    return 1;
}

static uint8_t piece_blocked_down(void) {
    Tetromino test = current;
    test.y++;
    return !can_place(&test);
}

static void update_gravity_delay(void) {
    if (level < 20) gravity_delay = diff_map[level];
    else if (level >= 29) gravity_delay = 1;
    else gravity_delay = 2;
}

static uint8_t lock_delay_for_y(int8_t y) {
    if (y <= 4) return 18;
    if (y <= 8) return 16;
    if (y <= 12) return 14;
    if (y <= 16) return 12;
    return 10;
}

static void update_stats_display(void) {
    draw_uint32(12, 2, score, 7);
    draw_uint8(12, 5, level, 2);
    draw_uint16(15, 5, lines, 3);
    draw_uint8(12, 13, dht, 3);
    draw_uint8(12, 16, trt, 3);
}

static uint16_t score_checksum(uint32_t value) {
    return (uint16_t)(SAVE_MAGIC ^ (uint16_t)value ^ (uint16_t)(value >> 16));
}

static uint16_t settings_checksum(uint8_t saved_level, uint8_t music_on, uint8_t das_delay, uint8_t arr_delay) {
    return (uint16_t)(SAVE_MAGIC ^ 0x5345u ^ saved_level ^ ((uint16_t)music_on << 4) ^ ((uint16_t)das_delay << 8) ^ ((uint16_t)arr_delay << 12));
}

static void set_default_settings(void) {
    selected_level = 0;
    selected_music = 1;
    selected_das_delay = DAS_DELAY_DEFAULT;
    selected_arr_delay = ARR_DELAY_DEFAULT;
    selected_menu_item = MENU_ITEM_LEVEL;
}

static void load_high_score(void) {
    ENABLE_RAM;
    SWITCH_RAM(0);
    if ((save_data.magic == SAVE_MAGIC) && (save_data.checksum == score_checksum(save_data.high_score))) {
        high_score = save_data.high_score;
    } else {
        high_score = 0;
    }
    DISABLE_RAM;
}

static void load_settings(void) {
    set_default_settings();

    ENABLE_RAM;
    SWITCH_RAM(0);
    if (
        (save_data.magic == SAVE_MAGIC) &&
        (save_data.settings_checksum == settings_checksum(save_data.selected_level, save_data.music_on, save_data.das_delay, save_data.arr_delay)) &&
        (save_data.selected_level <= 19u) &&
        (save_data.music_on <= 1u) &&
        (save_data.das_delay <= DAS_DELAY_MAX) &&
        (save_data.arr_delay <= ARR_DELAY_MAX)
    ) {
        selected_level = save_data.selected_level;
        selected_music = save_data.music_on;
        selected_das_delay = save_data.das_delay;
        selected_arr_delay = save_data.arr_delay;
    }
    DISABLE_RAM;
}

static void save_settings(void) {
    ENABLE_RAM;
    SWITCH_RAM(0);
    save_data.magic = SAVE_MAGIC;
    save_data.high_score = high_score;
    save_data.checksum = score_checksum(high_score);
    save_data.selected_level = selected_level;
    save_data.music_on = selected_music;
    save_data.das_delay = selected_das_delay;
    save_data.arr_delay = selected_arr_delay;
    save_data.settings_checksum = settings_checksum(selected_level, selected_music, selected_das_delay, selected_arr_delay);
    DISABLE_RAM;
}

static void save_high_score(void) {
    ENABLE_RAM;
    SWITCH_RAM(0);
    save_data.magic = SAVE_MAGIC;
    save_data.high_score = high_score;
    save_data.checksum = score_checksum(high_score);
    DISABLE_RAM;
}

static void update_high_score(void) {
    if (score > high_score) {
        high_score = score;
        save_high_score();
    }
}

static void draw_top_score(void) {
    draw_text(2, 1, "TOP");
    draw_uint32(2, 2, high_score, 7);
    draw_text(1, 8, "NESTRISgbc");
}

static void draw_game_ui_text(void) {
    draw_text(12, 1, "SCORE");
    draw_text(12, 4, "LV");
    draw_text(15, 4, "LNS");
    draw_text(12, 12, "DHT");
    draw_text(12, 15, "TRT");
    draw_text(15, 16, "%");
}

static void set_level_select_stats(void) {
    score = 0;
    lines = 0;
    tetris_lines = 0;
    level = selected_level;
    dht = 1;
    trt = 0;
}

static void draw_menu_value(uint8_t y, uint8_t value, uint8_t selected) {
    draw_text(7, y, selected ? "<" : " ");
    draw_uint8(8, y, value, 2);
    draw_text(10, y, selected ? ">" : " ");
}

static void draw_level_select(void) {
    draw_text(1, 13, "LEVEL ");
    draw_menu_value(13, selected_level, selected_menu_item == MENU_ITEM_LEVEL);

    draw_text(1, 14, "MUSIC ");
    draw_text(7, 14, (selected_menu_item == MENU_ITEM_MUSIC) ? "<" : " ");
    draw_text(8, 14, selected_music ? "ON" : "--");
    draw_text(10, 14, (selected_menu_item == MENU_ITEM_MUSIC) ? ">" : " ");

    draw_text(1, 15, (selected_das_delay == DAS_DELAY_DEFAULT) ? "DAS   " : "DAS*  ");
    draw_menu_value(15, selected_das_delay, selected_menu_item == MENU_ITEM_DAS);

    draw_text(1, 16, (selected_arr_delay == ARR_DELAY_DEFAULT) ? "ARR   " : "ARR*  ");
    draw_menu_value(16, selected_arr_delay, selected_menu_item == MENU_ITEM_ARR);
}

static void init_level_select_screen(void) {
    HIDE_SPRITES;
    hide_current_sprites();
    set_bkg_tiles(0, 0, 20, 18, background_map);
    set_game_area_palette(1);
    selected_menu_item = MENU_ITEM_LEVEL;
    set_level_select_stats();
    draw_top_score();
    draw_game_ui_text();
    update_stats_display();
    draw_level_select();
}

static void soft_reset_game(void) {
    stop_music();
    game_started = 0;
    game_over = 0;
    game_paused = 0;
    lock_delay = -1;
    push_down_points = 0;
    clear_row_count = 0;
    clear_anim_phase = 0;
    clear_anim_timer = 0;
    tetris_flash_count = 0;
    tetris_flash_timer = 0;
    tetris_flash_white = 0;
    tetris_flash_palette_dirty = 1;
    dpad_lock_axis = DPAD_LOCK_NONE;

    init_level_select_screen();
    mark_game_palette_dirty();
}

static void update_level_select(void) {
    if (btn_pressed(J_UP)) {
        selected_menu_item = selected_menu_item ? selected_menu_item - 1u : (MENU_ITEM_COUNT - 1u);
        draw_level_select();
        play_sfx_select();
    } else if (btn_pressed(J_DOWN)) {
        selected_menu_item = (selected_menu_item >= (MENU_ITEM_COUNT - 1u)) ? 0u : selected_menu_item + 1u;
        draw_level_select();
        play_sfx_select();
    }

    if (btn_pressed(J_LEFT)) {
        switch (selected_menu_item) {
            case MENU_ITEM_MUSIC:
                selected_music = !selected_music;
                break;
            case MENU_ITEM_DAS:
                selected_das_delay = selected_das_delay ? selected_das_delay - 1u : DAS_DELAY_MAX;
                break;
            case MENU_ITEM_ARR:
                selected_arr_delay = selected_arr_delay ? selected_arr_delay - 1u : ARR_DELAY_MAX;
                break;
            default:
                selected_level = selected_level ? selected_level - 1u : 19u;
                break;
        }
        draw_level_select();
        play_sfx_select();
    } else if (btn_pressed(J_RIGHT)) {
        switch (selected_menu_item) {
            case MENU_ITEM_MUSIC:
                selected_music = !selected_music;
                break;
            case MENU_ITEM_DAS:
                selected_das_delay = (selected_das_delay >= DAS_DELAY_MAX) ? 0u : selected_das_delay + 1u;
                break;
            case MENU_ITEM_ARR:
                selected_arr_delay = (selected_arr_delay >= ARR_DELAY_MAX) ? 0u : selected_arr_delay + 1u;
                break;
            default:
                selected_level = (selected_level >= 19u) ? 0u : selected_level + 1u;
                break;
        }
        draw_level_select();
        play_sfx_select();
    }

    if (btn_pressed(J_START)) reset_game();
}

static void pause_game(void) {
    game_paused = 1;
    pause_music();
    hide_current_sprites();
    draw_text(3, 8, "PAUSED");
}

static void resume_game(void) {
    game_paused = 0;
    resume_music();
    redraw_board();
    if (clear_row_count) {
        hide_current_sprites();
    } else {
        draw_current_piece();
    }
}

static void update_pause(void) {
    if (btn_pressed(J_SELECT)) {
        reset_game();
    } else if (btn_pressed(J_START)) {
        resume_game();
    }
}

static void update_dht_for_locked_piece(void) {
    if (current.shape == 2) {
        dht = 1;
    } else if (dht < 255) {
        dht++;
    }
}

static void update_trt(uint8_t clear_count) {
    uint16_t total_lines;
    uint32_t rounded_rate;

    if (clear_count == 4) tetris_lines += 4;

    total_lines = lines + clear_count;
    if (!total_lines) {
        trt = 0;
        return;
    }

    rounded_rate = (((uint32_t)tetris_lines * 100u) + (total_lines >> 1)) / total_lines;
    trt = (rounded_rate > 100u) ? 100u : (uint8_t)rounded_rate;
}

static void lock_current_piece(void) {
    uint8_t i;
    int8_t row;
    int8_t col;
    uint8_t tile = piece_tile[current.shape];

    for (i = 0; i < 4; i++) {
        col = current.x + shape_offsets[current.shape][current.rot][i][0];
        row = current.y + shape_offsets[current.shape][current.rot][i][1];
        if ((row >= 0) && (row < BOARD_H) && (col >= 0) && (col < BOARD_W)) {
            board[row][col] = tile;
            draw_board_cell((uint8_t)row, (uint8_t)col);
        }
    }
    hide_current_sprites();
}

static uint8_t collect_full_rows(uint8_t *rows) {
    int8_t row;
    uint8_t col;
    uint8_t count = 0;

    for (row = BOARD_H - 1; row >= 0; row--) {
        uint8_t full = 1;
        for (col = 0; col < BOARD_W; col++) {
            if (board[row][col] == EMPTY_CELL) {
                full = 0;
                break;
            }
        }
        if (full) rows[count++] = (uint8_t)row;
    }
    return count;
}

static uint8_t is_clear_row(uint8_t row) {
    uint8_t i;

    for (i = 0; i < clear_row_count; i++) {
        if (clear_rows[i] == row) return 1;
    }
    return 0;
}

static void remove_cleared_rows(void) {
    int8_t row;
    int8_t dst = BOARD_H - 1;
    uint8_t col;

    for (row = BOARD_H - 1; row >= 0; row--) {
        if (!is_clear_row((uint8_t)row)) {
            if (dst != row) {
                for (col = 0; col < BOARD_W; col++) board[dst][col] = board[row][col];
            }
            dst--;
        }
    }

    while (dst >= 0) {
        for (col = 0; col < BOARD_W; col++) board[dst][col] = EMPTY_CELL;
        dst--;
    }

    redraw_board();
}

static void apply_line_score(uint8_t clear_count) {
    uint16_t base = 40;

    if (clear_count == 2) base = 100;
    else if (clear_count == 3) base = 300;
    else if (clear_count == 4) base = 1200;

    score += (uint32_t)base * (uint32_t)(level + 1u);
}

static void finish_line_clears(void) {
    stop_tetris_flash();

    update_trt(clear_row_count);
    lines += clear_row_count;
    apply_line_score(clear_row_count);

    if (transition_lines > clear_row_count) {
        transition_lines -= clear_row_count;
    } else {
        transition_lines = 10u + transition_lines - clear_row_count;
        if (level < 99) {
            level++;
            mark_game_palette_dirty();
            play_sfx_levelup();
        }
        update_gravity_delay();
    }

    remove_cleared_rows();
    update_stats_display();
    clear_row_count = 0;
    clear_anim_phase = 0;
    clear_anim_timer = 0;
    spawn_next_piece();
}

static void start_line_clear_animation(void) {
    clear_row_count = collect_full_rows(clear_rows);
    clear_anim_phase = 0;
    clear_anim_timer = 0;

    if (clear_row_count == 4) {
        play_sfx_tetris();
        start_tetris_flash();
    } else if (clear_row_count) {
        play_sfx_lineclear();
    }
}

static void update_line_clear_animation(void) {
    uint8_t i;
    uint8_t left_col;
    uint8_t right_col;

    if (!clear_row_count) return;

    clear_anim_timer++;
    if (clear_anim_timer < CLEAR_ANIM_FRAME_DELAY) return;
    clear_anim_timer = 0;

    if (clear_anim_phase < 5) {
        left_col = 4u - clear_anim_phase;
        right_col = 5u + clear_anim_phase;

        for (i = 0; i < clear_row_count; i++) {
            board[clear_rows[i]][left_col] = EMPTY_CELL;
            board[clear_rows[i]][right_col] = EMPTY_CELL;
            draw_board_cell(clear_rows[i], left_col);
            draw_board_cell(clear_rows[i], right_col);
        }
    }

    clear_anim_phase++;
    if (clear_anim_phase >= 5) finish_line_clears();
}

static void spawn_next_piece(void) {
    uint8_t new_shape;

    current = next;
    new_shape = random_shape();
    if (new_shape == current.shape) new_shape = random_shape();
    next = make_piece(new_shape);
    draw_next_piece();

    if (!can_place(&current)) {
        game_over = 1;
        stop_music();
        draw_current_piece();
        update_high_score();
        draw_top_score();
        draw_text(1, 8, "GAME  OVER");
        selected_menu_item = MENU_ITEM_LEVEL;
        draw_level_select();
        game_paused = 0;
        return;
    }

    draw_current_piece();

    lock_delay = -1;
    push_down_points = 0;
    fall_release = 1;
    tuck_t = 0;
    tuck_tweak = -1;
}

static uint8_t move_current_horiz(int8_t delta) {
    Tetromino test = current;
    test.x += delta;
    if (can_place(&test)) {
        current = test;
        draw_current_piece();
        play_sfx_move();
        return 1;
    }
    return 0;
}

static void update_tuck_tweak(void) {
    if ((tuck_tweak != -1) && (tuck_wait == 0)) {
        if (tuck_tweak == 0) {
            if (btn_held(J_RIGHT)) {
                tuck_t = 0;
                tuck_tweak = -1;
            } else if ((lock_delay == -1) && (tuck_t > 0) && move_current_horiz(-1)) {
                tuck_wait = 40;
                if (das >= selected_das_delay) das = repeat_reset_value();
            }
        } else {
            if (btn_held(J_LEFT)) {
                tuck_t = 0;
                tuck_tweak = -1;
            } else if ((lock_delay == -1) && (tuck_t > 0) && move_current_horiz(1)) {
                tuck_wait = 40;
                if (das >= selected_das_delay) das = repeat_reset_value();
            }
        }
    }

    if (tuck_t > 0) {
        tuck_t--;
    } else {
        tuck_tweak = -1;
    }

    if (tuck_wait > 0) tuck_wait--;
}

static void handle_horizontal_input(void) {
    uint8_t moving_left = btn_held(J_LEFT) && !btn_held(J_RIGHT);
    uint8_t moving_right = btn_held(J_RIGHT) && !btn_held(J_LEFT);

    if (moving_left || moving_right) {
        if (first_tap && ((lock_delay == -1) || (das < repeat_reset_value()))) {
            das = 0;
        }

        if (first_tap || (das >= selected_das_delay)) {
            if (lock_delay == -1) {
                if (move_current_horiz(moving_left ? -1 : 1)) {
                    if (das >= selected_das_delay) das = repeat_reset_value();
                } else {
                    start_tuck_tweak(moving_left ? 0u : 1u);
                }
            }
        } else {
            das++;
        }
        first_tap = 0;
    } else {
        das = 0;
        first_tap = 1;
    }

    update_tuck_tweak();
}

static void rotate_current(int8_t delta) {
    Tetromino test = current;
    uint8_t next_rot;

    if ((lock_delay != -1)) return;

    next_rot = normalize_rot(test.shape, (int8_t)test.rot + delta);
    if (next_rot == current.rot) {
        if (current.shape == 0) play_sfx_rotate();
        return;
    }

    test.rot = next_rot;
    if (can_place(&test)) {
        current = test;
        draw_current_piece();
        play_sfx_rotate();
    }
}

static void step_down(uint8_t soft_drop) {
    Tetromino test = current;
    test.y++;

    if (can_place(&test)) {
        current = test;
        if (soft_drop) push_down_points++;
        draw_current_piece();
    } else if (lock_delay == -1) {
        lock_delay = (int8_t)lock_delay_for_y(current.y);
    }
}

static void update_game(void) {
    uint8_t soft_drop = 0;
    uint8_t active_delay;

    frame_counter++;
    update_tetris_flash();

    if (clear_row_count) {
        update_line_clear_animation();
        handle_horizontal_input();
        return;
    }

    if (btn_held(J_DOWN) && (frame_counter < 50)) frame_counter = 50;

    handle_horizontal_input();

    if (btn_pressed(J_A)) rotate_current(1);
    if (btn_pressed(J_B)) rotate_current(-1);

    if (btn_held(J_DOWN) && !fall_release && (lock_delay == -1)) soft_drop = 1;
    if (!btn_held(J_DOWN)) fall_release = 0;

    active_delay = soft_drop ? 2 : gravity_delay;
    if ((lock_delay == -1) && (frame_counter > 50) && ((frame_counter % active_delay) == 0)) {
        step_down(soft_drop);
    }

    if (lock_delay > 0) {
        lock_delay--;
    } else if (lock_delay == 0) {
        score += push_down_points;
        update_dht_for_locked_piece();
        lock_current_piece();
        start_line_clear_animation();
        if (!clear_row_count) {
            play_sfx_lock();
            update_stats_display();
            spawn_next_piece();
        }
    }
}

static void reset_game(void) {
    uint8_t row;
    uint8_t col;
    uint8_t first_shape;
    uint8_t second_shape;

    HIDE_SPRITES;
    hide_current_sprites();

    for (row = 0; row < BOARD_H; row++) {
        for (col = 0; col < BOARD_W; col++) board[row][col] = EMPTY_CELL;
    }

    frame_counter = 0;
    score = 0;
    lines = 0;
    tetris_lines = 0;
    level = selected_level;
    transition_lines = ((uint16_t)level + 1u) * 10u;
    lock_delay = -1;
    push_down_points = 0;
    dht = 1;
    trt = 0;
    das = 0;
    first_tap = 1;
    fall_release = 0;
    game_started = 1;
    game_over = 0;
    game_paused = 0;
    tuck_tweak = -1;
    tuck_t = 0;
    tuck_wait = 0;
    clear_row_count = 0;
    clear_anim_phase = 0;
    clear_anim_timer = 0;
    tetris_flash_count = 0;
    tetris_flash_timer = 0;
    tetris_flash_white = 0;
    tetris_flash_palette_dirty = 1;
    mark_game_palette_dirty();
    dpad_lock_axis = DPAD_LOCK_NONE;
    joyPadPrevious = joyPadCurrent;
    save_settings();
    rng_state ^= ((uint16_t)DIV_REG << 8) | LY_REG;
    start_music();

    update_gravity_delay();

    first_shape = random_shape();
    second_shape = random_shape();
    if (second_shape == first_shape) second_shape = random_shape();

    current = make_piece(first_shape);
    next = make_piece(second_shape);

    set_bkg_tiles(0, 0, 20, 18, background_map);
    set_game_area_palette(1);
    draw_game_ui_text();
    // draw_text(12, 7, "next");
    redraw_board();
    draw_next_piece();
    update_stats_display();
    draw_current_piece();
    SHOW_SPRITES;
}

void main(void) {
    
    NR52_REG = 0x80; //开启声音
    NR51_REG = 0xFF; //开启左右所有声道
    NR50_REG = 0x77; //左右声道音量设置为最大

    DISPLAY_OFF;

    set_bkg_data(font_TILE_START, font_TILE_COUNT, font_tiles);
    set_bkg_data(background_TILE_ORIGIN, background_TILE_COUNT, background_tiles);
    set_bkg_data(tetromino_TILE_ORIGIN, tetromino_TILE_COUNT, tetromino_tiles);
    set_sprite_data(tetromino_TILE_ORIGIN, tetromino_TILE_COUNT, tetromino_tiles);

    set_bkg_palette(0, 1, background_palettes);
    set_bkg_palette(1, 1, game_palettes[0]);
    set_bkg_palette(2, 1, game_palettes[0]);
    set_sprite_palette(1, 1, game_palettes[0]);

    CRITICAL {
        add_VBL(audio_vblank);
    }
    set_interrupts(VBL_IFLAG);
    enable_interrupts();

    SHOW_BKG;
    SHOW_SPRITES;
    DISPLAY_ON;

    load_high_score();
    load_settings();
    init_level_select_screen();

    while (1) {
        joyPadPrevious = joyPadCurrent;
        joyPadCurrent = apply_dpad_diagonal_lock(joypad());

        if (reset_combo_pressed()) {
            soft_reset_game();
        } else if (!game_started || game_over) {
            update_level_select();
        } else if (game_paused) {
            update_pause();
        } else if (btn_pressed(J_START)) {
            pause_game();
        } else {
            update_game();
        }

        wait_vbl_done();
    }
}
