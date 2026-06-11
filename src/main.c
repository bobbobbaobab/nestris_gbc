#include <gb/gb.h>
#include <gb/cgb.h>
#include "utils/cbtfx.h"

#include "gfx/font.h"
#include "gfx/background.h"
#include "gfx/tetromino.h"
// #include "gfx/circlesprite8x8.h"
#include "gfx/character8x16.h"
#include "snd/SFX_LINECLEAR.h"

uint8_t joyPadCurrent = 0;
uint8_t joyPadPrevious = 0;

void set_bkg_palette_area(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t palette_id) {
    uint8_t row[10];   // 最大区域宽度是 10
    uint8_t i;
    uint8_t yy;
    uint8_t attr;

    attr = palette_id & 0x07; // GBC BG palette 只有 0~7

    for(i = 0; i < w; i++) {
        row[i] = attr;
    }

    VBK_REG = 1; // 写背景属性层

    for(yy = 0; yy < h; yy++) {
        set_bkg_tiles(x, y + yy, w, 1, row);
    }

    VBK_REG = 0; // 切回 tile map 层
}

void set_game_area_palette(uint8_t palette_id) {
    set_bkg_palette_area(1, 0, 10, 18, palette_id);
    set_bkg_palette_area(13, 8, 4, 2, palette_id);
    set_sprite_prop(0, palette_id);
    set_sprite_prop(1, palette_id);
    set_sprite_prop(2, palette_id);
    set_sprite_prop(3, palette_id);
}

void main(void)
{
    NR52_REG = 0x80; //开启声音
    NR51_REG = 0xFF; //开启左右所有声道
    NR50_REG = 0x77; //左右声道音量设置为最大

    SHOW_BKG;    // 开启显示
    SHOW_SPRITES;    // 开启显示

    set_bkg_data(font_TILE_START, font_TILE_COUNT, font_tiles); // 加载字体图块
    set_bkg_data(background_TILE_ORIGIN,background_TILE_COUNT,background_tiles); // 加载背景图块
    set_bkg_data(tetromino_TILE_ORIGIN,tetromino_TILE_COUNT,tetromino_tiles); // 加载锁定方块图块
    set_sprite_data(tetromino_TILE_ORIGIN,tetromino_TILE_COUNT,tetromino_tiles); // 加载精灵方块图块


    set_bkg_palette(0,1,background_palettes); // 定义0号背景调色板

    const palette_color_t palette1[] = {
        RGB8(  0,   0,   0),
        RGB8( 0,  88,  248),
        RGB8(60, 188, 252),
        RGB8(255, 255, 255)
    };
    set_bkg_palette(1, 1, palette1);    // 定义1号背景调色板
    set_sprite_palette(1, 1, palette1); // 定义1号精灵调色板


    set_bkg_tiles(0,0,20,18,background_map); // 显示背景图片
    
    set_game_area_palette(1); // 对游戏区域（游戏主区域和next区域）的背景和四个tetromino精灵 应用1号调色板


    draw_text(12, 1, "score"); // 显示界面文字
    draw_uint16(14, 2, 0, 5); // 显示分数，初始为0，宽度5位（不足补0）
    draw_text(12, 4, "lv"); // 显示界面文字
    draw_uint8(12, 5, 0, 2); // 显示等级，初始为0，宽度2位（不足补0）
    draw_text(15, 4, "ln"); // 显示界面文字
    draw_uint8(15, 5, 0, 3); // 显示消行数，初始为0，宽度3位（不足补0）
    draw_text(12, 7, "next"); // 显示界面文字
    //下一个方块的显示区域是 (13,8) ~ (16,9)，共4x2个字符格


    while(1) {

        joyPadPrevious = joyPadCurrent;
        joyPadCurrent = joypad();





        



        
        CBTFX_update();
        wait_vbl_done();
    }
}