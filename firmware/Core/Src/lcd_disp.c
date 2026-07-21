/**
 * @file    lcd_disp.c
 * @brief   LCD display of RTC time, sensor readings, and light/temp progress bars.
 * @details Per design.md §3.3: avoids full-screen clear-and-redraw at 1Hz
 *          (the main cause of visible flicker on FSMC-parallel LCDs).
 *          Progress bars only repaint the delta region between the
 *          previous and new fill width; text fields use a custom glyph
 *          renderer (below) instead of the driver's built-in ASCII bitmap
 *          font, since the built-in font's max size (24) wasn't large
 *          enough for a clearly-readable display. Date uses the 48pt font
 *          (font.h Font_48); time/temp/light use the 72pt font (Font_72).
 *          Glyph lookup/drawing helpers are private to this module since
 *          lcd_disp is the only consumer -- design.md §3.3 only exposes
 *          lcd_disp_init()/lcd_disp_update() publicly.
 */
#include "lcd_disp.h"
#include "lcd.h"
#include "font.h"

/* --- layout + color constants ------------------------------------------*/

#define COLOR_BG        0x8430  // gray, adjust to match your actual background
#define COLOR_TEXT      0x0000  // black, for the big time display
#define COLOR_BAR_FILL  0x0000  // black, or whatever distinct fill color you want
#define COLOR_BORDER    0x0000  // black, for the outline only

#define LIGHT_BAR_X   90
#define LIGHT_BAR_Y   600
#define LIGHT_BAR_W   300
#define LIGHT_BAR_H   40

#define TEMP_BAR_X    90
#define TEMP_BAR_Y    400
#define TEMP_BAR_W    300
#define TEMP_BAR_H    40

#define DATE_X      100
#define DATE_Y      50

#define TIME_X      70
#define TIME_Y      120

#define TEMP_X      120
#define TEMP_Y      300

#define LIGHT_X     180
#define LIGHT_Y     500

/* Temperature bar normalization range, matches requirements_spec.md §7.1 */
#define TEMP_BAR_MIN_C   -10.0f
#define TEMP_BAR_MAX_C    50.0f

/* --- persistent redraw state (survives across lcd_disp_update() calls) -*/

static uint8_t  last_light_pct   = 0xFF;  // 0xFF sentinel forces first-call draw
static uint8_t  last_temp_pct    = 0xFF;

/* --- big font (48pt) glyph lookup + pixel drawing -----------------------*/

#define BIG_STR_MAX_LEN 16

/**
 * @brief  Per-instance state for delta-redraw tracking. Each independent
 *         big-font text display (time, temp, light) needs its own state
 *         so they don't clobber each other's "last drawn" memory.
 */
typedef struct {
    char last_str[BIG_STR_MAX_LEN + 1];
} BigStringState_t;

static BigStringState_t date_state  = {0};
static BigStringState_t time_state  = {0};
static BigStringState_t temp_state  = {0};
static BigStringState_t light_state = {0};

/**
 * @brief  Look up a character's glyph in a tFont table.
 */
static const tImage *font_find_glyph(const tFont *font, char ch)
{
    for (int i = 0; i < font->length; i++) {
        if (font->chars[i].code == (long)(unsigned char)ch) {
            return font->chars[i].image;
        }
    }
    return NULL;
}

/**
 * @brief  Draw a single glyph, pixel by pixel.
 * @note   Bit polarity: 0 = paint, 1 = background (confirmed against the
 *         LCD-Image-Converter preview output for this font). Row storage:
 *         horizontal, MSB-first, ceil(width/8) bytes per row.
 */
static void draw_glyph(uint16_t x, uint16_t y, const tImage *img, uint16_t color)
{
    uint16_t bytes_per_row = (img->width + 7) / 8;

    // Clear this glyph's full bounding box to background first -- a
    // narrower new character must not leave remnants of a wider old one.
    // See debug_log.md #003 for the ghosting issue this prevents.
    LCD_Fill(x, y, x + img->width, y + img->height, COLOR_BG);

    for (uint16_t row = 0; row < img->height; row++) {
        for (uint16_t col = 0; col < img->width; col++) {
            uint8_t byte = img->data[row * bytes_per_row + col / 8];
            uint8_t bit = (byte >> (7 - (col % 8))) & 0x01;
            if (bit == 0) {
                LCD_Fast_DrawPoint(x + col, y + row, color);
            }
        }
    }
}

/**
 * @brief  Draw a string once using the given font, no delta-tracking.
 *         For static content drawn a single time (e.g. bar min/max labels
 *         in lcd_disp_init()), where there's no "previous value" to
 *         compare against and never will be.
 */
static void draw_string_once(uint16_t x, uint16_t y, const char *str,
                                  uint16_t color, const tFont *font)
{
    uint16_t cursor_x = x;
    while (*str) {
        const tImage *glyph = font_find_glyph(font, *str);
        if (glyph != NULL) {
            draw_glyph(cursor_x, y, glyph, color);
            cursor_x += glyph->width + 2;
        }
        str++;
    }
}

/**
 * @brief  Draw a string using the given font, redrawing only character
 *         positions that changed since the last call for this state
 *         instance (avoids flicker from unconditional clear+redraw).
 * @param  font  Which font table to use (Font_48 or Font_72) -- lets
 *               different display fields use different sizes.
 */
static void draw_string_delta(uint16_t x, uint16_t y, const char *str,
                                   uint16_t color, BigStringState_t *state,
                                   const tFont *font)
{
    uint16_t cursor_x = x;
    int i = 0;

    while (str[i] != '\0' && i < BIG_STR_MAX_LEN) {
        const tImage *glyph = font_find_glyph(font, str[i]);

        if (str[i] != state->last_str[i] && glyph != NULL) {
            draw_glyph(cursor_x, y, glyph, color);
        }
        if (glyph != NULL) {
            cursor_x += glyph->width + 2;
        }
        i++;
    }

    while (state->last_str[i] != '\0' && i < BIG_STR_MAX_LEN) {
        const tImage *old_glyph = font_find_glyph(font, state->last_str[i]);
        if (old_glyph != NULL) {
            LCD_Fill(cursor_x, y, cursor_x + old_glyph->width, y + old_glyph->height, COLOR_BG);
            cursor_x += old_glyph->width + 2;
        }
        i++;
    }

    strncpy(state->last_str, str, BIG_STR_MAX_LEN);
    state->last_str[BIG_STR_MAX_LEN] = '\0';
}
/* --- internal helpers ---------------------------------------------------*/

/**
 * @brief  Draw/update a horizontal bar, repainting only the delta region
 *         between the previous and new fill width (anti-flicker, REQ-NFR-003).
 * @param  x,y,width,height  Bar's fixed position/size (full extent, outline
 *                            drawn once in lcd_disp_init(), not here)
 * @param  new_pct   New value, 0-100
 * @param  last_pct  Pointer to the previously-drawn value; updated in place
 * @param  fg_color  Fill color for the "on" portion -- fixed per bar, no
 *                    longer varies at runtime (temp bar no longer color-codes
 *                    by threshold; both light and temp bars use this the
 *                    same way now).
 */
static void draw_bar_delta(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                            uint8_t new_pct, uint8_t *last_pct, uint16_t fg_color)
{
    if (new_pct == *last_pct) {
        return; // nothing changed, skip entirely -- most direct anti-flicker win
    }

    uint16_t new_fill_w = (uint16_t)((uint32_t)width * new_pct / 100);
    uint16_t old_fill_w = (*last_pct == 0xFF)
                            ? 0
                            : (uint16_t)((uint32_t)width * (*last_pct) / 100);

    if (new_fill_w > old_fill_w) {
        LCD_Fill(x + old_fill_w, y, x + new_fill_w, y + height, fg_color); // grew: paint only the new segment
    } else if (new_fill_w < old_fill_w) {
        LCD_Fill(x + new_fill_w, y, x + old_fill_w, y + height, COLOR_BG); // shrank: erase only the vacated segment
    }

    *last_pct = new_pct;
}

/**
 * @brief  Normalize temperature from [TEMP_BAR_MIN_C, TEMP_BAR_MAX_C] to 0-100
 *         for the bar's fill width. Independent of the REQ-SYS-002 clamp
 *         range (-40..85) -- this is a display-only normalization for the
 *         bar's visual range, not a data-validity clamp.
 */
static uint8_t temp_to_bar_pct(float temp_c)
{
    if (temp_c <= TEMP_BAR_MIN_C) return 0;
    if (temp_c >= TEMP_BAR_MAX_C) return 100;
    return (uint8_t)(100.0f * (temp_c - TEMP_BAR_MIN_C) / (TEMP_BAR_MAX_C - TEMP_BAR_MIN_C));
}

/* --- public API ----------------------------------------------------------*/


void lcd_disp_init(void)
{
    LCD_Init();
    LCD_Clear(COLOR_BG);

    POINT_COLOR = COLOR_BORDER;
    // Draw bar outlines once -- these don't need repainting on every tick
    LCD_DrawRectangle(LIGHT_BAR_X-4, LIGHT_BAR_Y-4, LIGHT_BAR_X + LIGHT_BAR_W+4, LIGHT_BAR_Y + LIGHT_BAR_H+4);
    LCD_DrawRectangle(TEMP_BAR_X-4, TEMP_BAR_Y-4, TEMP_BAR_X + TEMP_BAR_W+4, TEMP_BAR_Y + TEMP_BAR_H+4);

    draw_string_once(TEMP_BAR_X-90, TEMP_BAR_Y-10, "-10", COLOR_TEXT, &Font_48);  
    draw_string_once(TEMP_BAR_X+TEMP_BAR_W+8, TEMP_BAR_Y-10, "50", COLOR_TEXT,  &Font_48);  

    draw_string_once(LIGHT_BAR_X-40, LIGHT_BAR_Y-10, "0", COLOR_TEXT, &Font_48);  
    draw_string_once(LIGHT_BAR_X+LIGHT_BAR_W+8, LIGHT_BAR_Y-10, "100", COLOR_TEXT, &Font_48);  
}

void lcd_disp_update(const char *date, const char *time, const SensorData_t *data)
{
    char buf[16];

    /* --- big font: date --- */
    draw_string_delta(DATE_X, DATE_Y, date, COLOR_TEXT, &date_state, &Font_48);  

    /* --- big font: time --- */
    draw_string_delta(TIME_X, TIME_Y, time, COLOR_TEXT, &time_state, &Font_72);

    /* --- big font: temperature --- */
    sprintf(buf, "%.1f\x83", data->temp_c);
    draw_string_delta(TEMP_X, TEMP_Y, buf, COLOR_TEXT, &temp_state,&Font_72);

    /* --- big font: light --- */
    sprintf(buf, "%d%%", data->light_pct);
    draw_string_delta(LIGHT_X, LIGHT_Y, buf, COLOR_TEXT, &light_state,&Font_72);

    /* --- bars, 不变 --- */
    draw_bar_delta(LIGHT_BAR_X, LIGHT_BAR_Y, LIGHT_BAR_W, LIGHT_BAR_H,
                    data->light_pct, &last_light_pct, COLOR_BAR_FILL);

    uint8_t  temp_bar_pct = temp_to_bar_pct(data->temp_c);
    draw_bar_delta(TEMP_BAR_X, TEMP_BAR_Y, TEMP_BAR_W, TEMP_BAR_H,
                    temp_bar_pct, &last_temp_pct, COLOR_BAR_FILL);
}