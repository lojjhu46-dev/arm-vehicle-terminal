/**
 * @file freetype_lcd.c
 * @brief 使用 FreeType 将 UTF-8 文本渲染到 LCD 显存。
 */

#include <stddef.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "freetype_lcd.h"

/* LCD 每行的像素数，用于将二维坐标换算为显存下标。 */
#define LCD_WIDTH 800

/* LCD 显存首地址，由 freetype_set_lcd() 设置。 */
static unsigned int *lcd_mp = NULL;

/* FreeType 库对象和当前字体对象。 */
static FT_Library library = NULL;
static FT_Face face = NULL;

/**
 * @brief 将一个 UTF-8 字符转换为 Unicode 码点。
 *
 * 本项目主要显示 ASCII 和常用汉字，因此处理 1～3 字节 UTF-8 编码。
 * 返回的字节数可用于让字符串指针移动到下一个字符。
 *
 * @param text 当前 UTF-8 字符的首字节地址。
 * @param code 用于接收转换后的 Unicode 码点。
 * @return 成功时返回当前字符占用的字节数；不支持的编码返回 -1。
 */
static int utf8_to_unicode(const unsigned char *text, unsigned int *code)
{
    /* 单字节编码：ASCII，Unicode 范围为 U+0000～U+007F。 */
    if (text[0] < 0x80) {
        *code = text[0];
        return 1;
    }

    /* 双字节编码：首字节格式为 110xxxxx。 */
    if ((text[0] & 0xE0) == 0xC0) {
        *code = ((text[0] & 0x1F) << 6) |
                (text[1] & 0x3F);
        return 2;
    }

    /* 三字节编码：首字节格式为 1110xxxx，常用汉字位于此范围。 */
    if ((text[0] & 0xF0) == 0xE0) {
        *code = ((text[0] & 0x0F) << 12) |
                ((text[1] & 0x3F) << 6) |
                (text[2] & 0x3F);
        return 3;
    }

    return -1;
}

/**
 * @brief 保存 LCD 显存的首地址。
 *
 * 后续字符绘制操作会通过该地址直接修改 LCD 像素数据。本函数只保存
 * 指针，不申请、复制或释放显存；显存的生命周期由调用者负责管理。
 *
 * @param lcd 由 mmap() 等方式获得的 LCD 显存首地址。
 */
void freetype_set_lcd(unsigned int *lcd)
{
    lcd_mp = lcd;
}

/**
 * @brief 初始化 FreeType 库并加载指定字体。
 *
 * 首先创建 FreeType 库对象，然后从 font_path 指定的字体文件创建字体
 * 对象。如果字体加载失败，会立即释放已经创建的库对象，避免资源泄漏。
 *
 * @param font_path 字体文件路径。
 * @return 初始化和字体加载均成功时返回 0，否则返回 -1。
 */
int freetype_init(const char *font_path)
{
    /* 创建 FreeType 库对象。 */
    if (FT_Init_FreeType(&library) != 0) {
        return -1;
    }

    /* 从字体文件创建字体对象；参数 0 表示选择字体集合中的第一个字体。 */
    if (FT_New_Face(library, font_path, 0, &face) != 0) {
        FT_Done_FreeType(library);
        library = NULL;
        return -1;
    }

    return 0;
}

/**
 * @brief 设置字体的像素高度。
 *
 * 宽度参数设置为 0，表示让 FreeType 按照字体原始宽高比例自动计算
 * 字符宽度。设置结果将用于后续加载和渲染的所有字形。
 *
 * @param size 字体像素高度，应为大于 0 的整数。
 */
void freetype_set_size(int size)
{
    /* 宽度传 0，让 FreeType 根据字体比例自动计算字符宽度。 */
    FT_Set_Pixel_Sizes(face, 0, (FT_UInt)size);
}

/**
 * @brief 将单个 Unicode 字符绘制到 LCD 显存。
 *
 * FreeType 生成的位图是 8 位灰度图。每个灰度值代表当前像素的覆盖率：
 * 0 表示完全透明，255 表示完全使用指定字体颜色，中间值用于抗锯齿。
 *
 * @param code  Unicode 码点。
 * @param x     字符基线起点的横坐标。
 * @param y     字符基线起点的纵坐标。
 * @param color 字体颜色，格式为 0xRRGGBB。
 */
static void draw_char(unsigned int code, int x, int y, unsigned int color)
{
    FT_UInt glyph_index;
    FT_Bitmap *bitmap;
    int draw_x;
    int draw_y;
    unsigned char red;
    unsigned char green;
    unsigned char blue;

    /* 将 Unicode 码点映射为当前字体内部的字形索引。 */
    glyph_index = FT_Get_Char_Index(face, code);

    /* 加载字形轮廓，然后将轮廓栅格化为抗锯齿灰度位图。 */
    if (FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT) != 0 ||
        FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL) != 0) {
        return;
    }

    bitmap = &face->glyph->bitmap;

    /*
     * bitmap_left 是位图左边缘相对基线起点的水平偏移。
     * bitmap_top 是位图上边缘相对基线的向上距离；LCD 的 y 轴向下，
     * 因此计算纵坐标时需要使用减法。
     */
    draw_x = x + face->glyph->bitmap_left;
    draw_y = y - face->glyph->bitmap_top;

    /* 从 0xRRGGBB 中拆分三个颜色分量。 */
    red = (unsigned char)((color >> 16) & 0xFF);
    green = (unsigned char)((color >> 8) & 0xFF);
    blue = (unsigned char)(color & 0xFF);

    for (int row = 0; row < (int)bitmap->rows; ++row) {
        for (int column = 0; column < (int)bitmap->width; ++column) {
            unsigned char gray = bitmap->buffer[row * bitmap->pitch + column];

            if (gray != 0) {
                /* 按字形覆盖率缩放 RGB 分量，实现边缘抗锯齿效果。 */
                unsigned char output_red = (unsigned char)(red * gray / 255);
                unsigned char output_green = (unsigned char)(green * gray / 255);
                unsigned char output_blue = (unsigned char)(blue * gray / 255);

                lcd_mp[(draw_y + row) * LCD_WIDTH + draw_x + column] =
                    ((unsigned int)output_red << 16) |
                    ((unsigned int)output_green << 8) |
                    output_blue;
            }
        }
    }
}

/**
 * @brief 从指定基线位置开始绘制 UTF-8 字符串。
 *
 * 函数逐个解析 UTF-8 字符，将其转换为 Unicode 码点并调用 draw_char()
 * 绘制。每绘制一个字符后，根据该字形的水平推进量更新 x 坐标，使下
 * 一个字符排列在正确位置。遇到不支持的编码时停止绘制。
 *
 * @param x     首字符基线起点的横坐标。
 * @param y     首字符基线起点的纵坐标。
 * @param text  以空字符结尾的 UTF-8 字符串。
 * @param color 字体颜色，格式为 0xRRGGBB。
 */
void lcd_draw_text(int x, int y, const char *text, unsigned int color)
{
    const unsigned char *current = (const unsigned char *)text;

    while (*current != '\0') {
        unsigned int code;
        int length = utf8_to_unicode(current, &code);

        /* 遇到不支持的 UTF-8 编码时停止，避免错误解析后续字节。 */
        if (length < 0) {
            break;
        }

        draw_char(code, x, y, color);

        /*
         * advance.x 表示绘制下一个字符时基线起点应前进的距离。
         * FreeType 使用 26.6 定点格式保存该值，右移 6 位后得到像素数。
         */
        FT_UInt glyph_index = FT_Get_Char_Index(face, code);
        if (FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT) == 0) {
            x += face->glyph->advance.x >> 6;
        }

        current += length;
    }
}

/**
 * @brief 释放本模块持有的全部 FreeType 资源。
 *
 * 按照“先字体对象、后库对象”的顺序释放资源，并将对应指针恢复为
 * NULL。释放前会检查指针，因此尚未成功初始化或重复调用时不会再次
 * 释放同一个对象。
 */
void freetype_exit(void)
{
    if (face != NULL) {
        FT_Done_Face(face);
        face = NULL;
    }

    if (library != NULL) {
        FT_Done_FreeType(library);
        library = NULL;
    }
}
