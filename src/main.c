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
static uint8_t level;
static uint8_t transition_lines;
static uint8_t gravity_delay;
static int8_t lock_delay;
static uint8_t push_down_points;
static uint8_t das;
static uint8_t first_tap;
static uint8_t fall_release;
static uint8_t game_over;
static uint16_t rng_state = 0xACE1u;

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
    } else {
        tile = board[row][col];
        set_bkg_tiles(screen_x, screen_y, 1, 1, &tile);
    }
}

static void redraw_board(void) {
    uint8_t row;
    uint8_t col;

    for (row = 0; row < BOARD_H; row++) {
        for (col = 0; col < BOARD_W; col++) draw_board_cell(row, col);
    }
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
    if (y <= 5) return 18;
    if (y <= 9) return 16;
    if (y <= 13) return 14;
    if (y <= 17) return 12;
    return 10;
}

static void draw_uint32_fixed(uint8_t x, uint8_t y, uint32_t value, uint8_t width) {
    char buf[11];
    int8_t i;

    if (width > 10) width = 10;
    for (i = 9; i >= 0; i--) {
        buf[i] = (char)('0' + (value % 10u));
        value /= 10u;
    }
    buf[10] = '\0';
    draw_text(x, y, &buf[10 - width]);
}

static void update_stats_display(void) {
    draw_uint32_fixed(13, 2, score, 6);
    draw_uint16(12, 5, level, 2);
    draw_uint16(15, 5, lines, 3);
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

static void remove_full_rows(void) {
    int8_t row;
    uint8_t col;

    for (row = BOARD_H - 1; row >= 0; row--) {
        uint8_t full = 1;

        for (col = 0; col < BOARD_W; col++) {
            if (board[row][col] == EMPTY_CELL) {
                full = 0;
                break;
            }
        }

        if (full) {
            int8_t shift_row;
            for (shift_row = row; shift_row > 0; shift_row--) {
                for (col = 0; col < BOARD_W; col++) board[shift_row][col] = board[shift_row - 1][col];
            }
            for (col = 0; col < BOARD_W; col++) board[0][col] = EMPTY_CELL;
            row++;
        }
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

static void apply_line_clears(void) {
    uint8_t rows[4];
    uint8_t count = collect_full_rows(rows);

    if (!count) return;

    lines += count;
    apply_line_score(count);

    if (transition_lines > count) {
        transition_lines -= count;
    } else {
        transition_lines = 10u + transition_lines - count;
        level++;
        update_gravity_delay();
    }

    remove_full_rows();
    update_stats_display();
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
        return;
    }

    draw_current_piece();

    lock_delay = -1;
    push_down_points = 0;
    fall_release = 1;
}

static void move_current_horiz(int8_t delta) {
    Tetromino test = current;
    test.x += delta;
    if (can_place(&test)) {
        current = test;
        draw_current_piece();
    }
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
                move_current_horiz(moving_left ? -1 : 1);
                if (das >= DAS_MAX) das = DAS_MAX - ARR_DELAY;
            }
        } else {
            das++;
        }
        first_tap = 0;
    } else {
        das = 0;
        first_tap = 1;
    }
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
        lock_current_piece();
        apply_line_clears();
        update_stats_display();
        spawn_next_piece();
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
    level = 0;
    transition_lines = 10;
    lock_delay = -1;
    push_down_points = 0;
    das = 0;
    first_tap = 1;
    fall_release = 0;
    game_over = 0;
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
    draw_text(12, 1, "SCORE");
    draw_text(12, 4, "LV");
    draw_text(15, 4, "LN");
    // draw_text(12, 7, "next");
    redraw_board();
    draw_next_piece();
    update_stats_display();
    draw_current_piece();
    SHOW_SPRITES;
}

void main(void) {
    const palette_color_t game_palette[] = {
        RGB8(  0,   0,   0),
        RGB8(  0,  88, 248),
        RGB8( 60, 188, 252),
        RGB8(255, 255, 255)
    };

    DISPLAY_OFF;

    set_bkg_data(font_TILE_START, font_TILE_COUNT, font_tiles);
    set_bkg_data(background_TILE_ORIGIN, background_TILE_COUNT, background_tiles);
    set_bkg_data(tetromino_TILE_ORIGIN, tetromino_TILE_COUNT, tetromino_tiles);
    set_sprite_data(tetromino_TILE_ORIGIN, tetromino_TILE_COUNT, tetromino_tiles);

    set_bkg_palette(0, 1, background_palettes);
    set_bkg_palette(1, 1, game_palette);
    set_sprite_palette(1, 1, game_palette);

    SHOW_BKG;
    SHOW_SPRITES;
    DISPLAY_ON;

    reset_game();

    while (1) {
        joyPadPrevious = joyPadCurrent;
        joyPadCurrent = apply_dpad_diagonal_lock(joypad());

        if (game_over) {
            if (btn_pressed(J_A) || btn_pressed(J_B)) reset_game();
        } else {
            update_game();
        }

        CBTFX_update();
        wait_vbl_done();
    }
}
