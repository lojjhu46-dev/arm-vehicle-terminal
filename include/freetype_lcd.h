/**
 * @file freetype_lcd.h
 * @brief FreeType 字体加载与 LCD 文本绘制接口。
 *
 * 本模块负责加载字体文件、设置字体像素大小，并将 UTF-8 字符串
 * 渲染到调用者提供的 LCD 显存中。颜色格式为 0xRRGGBB。
 */

#ifndef FREETYPE_LCD_H
#define FREETYPE_LCD_H

/**
 * @brief 初始化 FreeType 并加载一个字体文件。
 *
 * 使用其他字体接口前必须先调用本函数；使用结束后应调用
 * freetype_exit() 释放资源。
 *
 * @param font_path 字体文件路径，例如 "/usr/share/fonts/test.ttf"。
 *                  路径在函数返回后无需继续保持有效。
 * @return 成功返回 0；FreeType 初始化或字体加载失败时返回 -1。
 */
int freetype_init(const char *font_path);

/**
 * @brief 设置字体的像素高度。
 *
 * 该设置会影响之后所有 lcd_draw_text() 调用。常用值有 32、40、48。
 * 调用本函数前必须成功执行 freetype_init()。
 *
 * @param size 字体像素高度，应为大于 0 的整数。
 */
void freetype_set_size(int size);

/**
 * @brief 设置 LCD 显存的首地址。
 *
 * 显存通常由 mmap() 获得。调用者负责保证该内存在绘制期间有效，
 * 且容量足以覆盖绘制区域。
 *
 * @param lcd LCD 显存首地址，每个像素使用一个 unsigned int 保存。
 */
void freetype_set_lcd(unsigned int *lcd);

/**
 * @brief 在 LCD 显存中绘制一段 UTF-8 文本。
 *
 * 坐标 (x, y) 表示首字符的基线起点，而不是字符位图的左上角。
 * 当前模块不执行屏幕边界裁剪，因此调用者应确保文本位于有效区域内。
 *
 * @param x     文本基线起点的横坐标，单位为像素。
 * @param y     文本基线起点的纵坐标，单位为像素。
 * @param text  以 '\0' 结尾的 UTF-8 字符串。
 * @param color 字体颜色，格式为 0xRRGGBB，例如黑色为 0x000000，
 *              红色为 0xFF0000。
 */
void lcd_draw_text(int x, int y, const char *text, unsigned int color);

/**
 * @brief 释放字体对象和 FreeType 库资源。
 *
 * 释放后如需再次绘制文字，应重新调用 freetype_init()。
 */
void freetype_exit(void);

#endif /* FREETYPE_LCD_H */
