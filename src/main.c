#include <gb/gb.h>
#include <gb/cgb.h>
#include <stdint.h>
#include "utils/cbtfx.h"

#include "gfx/font.h"
#include "gfx/background.h"
#include "gfx/tetromino.h"

#define BOARD_W 10
#define BOARD_H 18
#define VISIBLE_H 18
#define BOARD_BKG_X 1
#define BOARD_BKG_Y 0
#define NEXT_BKG_X 13
#define NEXT_BKG_Y 8
#define EMPTY_CELL 0xFFu

#define DAS_MAX 16
#define ARR_DELAY 5
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
static uint16_t lines;
static uint16_t tetris_lines;
static uint8_t level;
static uint8_t selected_level;
static uint8_t transition_lines;
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

static const uint8_t diff_map[20] = {
    48, 43, 38, 33, 28, 23, 18, 13, 8, 6,
     5,  5,  5,  4,  4,  4,  3,  3, 3, 2
};

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

static void draw_game_ui_text(void) {
    draw_text(12, 1, "SCORE");
    draw_text(12, 4, "LV");
    draw_text(15, 4, "LN");
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

static void draw_level_select(void) {
    draw_text(1, 12, "LEVEL <");
    draw_uint8(8, 12, selected_level, 2);
    draw_text(10, 12, ">");
}

static void init_level_select_screen(void) {
    HIDE_SPRITES;
    hide_current_sprites();
    set_bkg_tiles(0, 0, 20, 18, background_map);
    set_game_area_palette(1);
    set_level_select_stats();
    draw_game_ui_text();
    update_stats_display();
    draw_level_select();
}

static void update_level_select(void) {
    if (btn_pressed(J_LEFT)) {
        selected_level = selected_level ? selected_level - 1u : 29u;
        draw_level_select();
    } else if (btn_pressed(J_RIGHT)) {
        selected_level = (selected_level >= 29u) ? 0u : selected_level + 1u;
        draw_level_select();
    }

    if (btn_pressed(J_START)) reset_game();
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

    if (clear_row_count == 4) start_tetris_flash();
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
        draw_current_piece();
        draw_text(1, 8, "GAME  OVER");
        draw_level_select();
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
                if (das == DAS_MAX) das -= ARR_DELAY;
            }
        } else {
            if (btn_held(J_LEFT)) {
                tuck_t = 0;
                tuck_tweak = -1;
            } else if ((lock_delay == -1) && (tuck_t > 0) && move_current_horiz(1)) {
                tuck_wait = 40;
                if (das == DAS_MAX) das -= ARR_DELAY;
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
        if (first_tap && ((lock_delay == -1) || (das < (DAS_MAX - ARR_DELAY)))) {
            das = 0;
        }

        if (first_tap || (das >= DAS_MAX)) {
            if (lock_delay == -1) {
                if (move_current_horiz(moving_left ? -1 : 1)) {
                    if (das >= DAS_MAX) das = DAS_MAX - ARR_DELAY;
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

    if ((lock_delay != -1)) return;

    test.rot = normalize_rot(test.shape, (int8_t)test.rot + delta);
    if (can_place(&test)) {
        current = test;
        draw_current_piece();
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
    if (level < 10u) {
        transition_lines = (level + 1u) * 10u;
    } else if (level < 16u) {
        transition_lines = 100u;
    } else {
        transition_lines = (level * 10u) - 50u;
    }
    lock_delay = -1;
    push_down_points = 0;
    dht = 1;
    trt = 0;
    das = 0;
    first_tap = 1;
    fall_release = 0;
    game_started = 1;
    game_over = 0;
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
    rng_state ^= ((uint16_t)DIV_REG << 8) | LY_REG;

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
    DISPLAY_OFF;

    set_bkg_data(font_TILE_START, font_TILE_COUNT, font_tiles);
    set_bkg_data(background_TILE_ORIGIN, background_TILE_COUNT, background_tiles);
    set_bkg_data(tetromino_TILE_ORIGIN, tetromino_TILE_COUNT, tetromino_tiles);
    set_sprite_data(tetromino_TILE_ORIGIN, tetromino_TILE_COUNT, tetromino_tiles);

    set_bkg_palette(0, 1, background_palettes);
    set_bkg_palette(1, 1, game_palettes[0]);
    set_bkg_palette(2, 1, game_palettes[0]);
    set_sprite_palette(1, 1, game_palettes[0]);

    SHOW_BKG;
    SHOW_SPRITES;
    DISPLAY_ON;

    init_level_select_screen();

    while (1) {
        joyPadPrevious = joyPadCurrent;
        joyPadCurrent = apply_dpad_diagonal_lock(joypad());

        if (!game_started || game_over) {
            update_level_select();
        } else {
            update_game();
        }

        CBTFX_update();
        wait_vbl_done();
        apply_palette_updates();
    }
}
