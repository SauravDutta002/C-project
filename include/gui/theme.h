/*
 * theme.h - Colours, fonts and GDI drawing primitives
 */
#ifndef GUI_THEME_H
#define GUI_THEME_H

#include "gui/win.h"
#include "core/train.h"
#include "core/signal_ctrl.h"
#include "io/logger.h"

#define RGBX(h) RGB(((h) >> 16) & 0xFF, ((h) >> 8) & 0xFF, (h) & 0xFF)
#define NO_BORDER CLR_INVALID

/* Chrome */
#define COL_HEADER       RGBX(0x0F172A)
#define COL_HEADER_SUB   RGBX(0x94A3B8)
#define COL_SIDEBAR      RGBX(0x1E293B)
#define COL_NAV_HOVER    RGBX(0x293548)
#define COL_NAV_ACTIVE   RGBX(0x334155)
#define COL_NAV_TEXT     RGBX(0xCBD5E1)
#define COL_ACCENT       RGBX(0x3B82F6)

/* Content */
#define COL_BG           RGBX(0xF1F5F9)
#define COL_CARD         RGBX(0xFFFFFF)
#define COL_BORDER       RGBX(0xE2E8F0)
#define COL_TEXT         RGBX(0x0F172A)
#define COL_MUTED        RGBX(0x64748B)
#define COL_FAINT        RGBX(0x94A3B8)
#define COL_ROW_ALT      RGBX(0xF8FAFC)
#define COL_GRID         RGBX(0xE9EEF5)

/* Semantic */
#define COL_GREEN        RGBX(0x16A34A)
#define COL_GREEN_T      RGBX(0xDCFCE7)
#define COL_AMBER        RGBX(0xD97706)
#define COL_AMBER_T      RGBX(0xFEF3C7)
#define COL_RED          RGBX(0xDC2626)
#define COL_RED_T        RGBX(0xFEE2E2)
#define COL_BLUE         RGBX(0x2563EB)
#define COL_BLUE_T       RGBX(0xDBEAFE)
#define COL_CYAN         RGBX(0x0891B2)
#define COL_CYAN_T       RGBX(0xCFFAFE)
#define COL_PURPLE       RGBX(0x7C3AED)
#define COL_PURPLE_T     RGBX(0xEDE9FE)
#define COL_SLATE        RGBX(0x475569)
#define COL_SLATE_T      RGBX(0xF1F5F9)

/* Mimic (signal-box) panel */
#define COL_M_BG         RGBX(0x0B1220)
#define COL_M_GRID       RGBX(0x16213A)
#define COL_M_TRACK      RGBX(0x5B6B82)
#define COL_M_PLATFORM   RGBX(0x2A3A52)
#define COL_M_TEXT       RGBX(0xCBD5E1)
#define COL_M_FAINT      RGBX(0x64748B)
#define COL_LAMP_OFF     RGBX(0x1F2937)
#define COL_LAMP_RED     RGBX(0xEF4444)
#define COL_LAMP_YELLOW  RGBX(0xFACC15)
#define COL_LAMP_GREEN   RGBX(0x22C55E)

typedef struct {
    int   dpi;
    HFONT ui, ui_bold, sm, small_bold, title, h2, big, clock, mono, mono_bold;
} Theme;

extern Theme g_theme;

int  S(int px);                      /* scale 96-dpi pixels to the screen */
void theme_init(void);
void theme_free(void);

/* Primitives */
void draw_fill(HDC hdc, const RECT *r, COLORREF c);
void draw_round(HDC hdc, const RECT *r, int radius, COLORREF fill, COLORREF border);
void draw_round_outline(HDC hdc, const RECT *r, int radius, COLORREF border, int width);
void draw_text(HDC hdc, const char *text, const RECT *r, HFONT font, COLORREF color, UINT fmt);
int  text_width(HDC hdc, const char *text, HFONT font);
int  pill_width(HDC hdc, const char *text, HFONT font);
int  draw_pill(HDC hdc, int x, int y, const char *text, COLORREF bg, COLORREF fg, HFONT font);
void draw_circle(HDC hdc, int cx, int cy, int r, COLORREF fill, COLORREF border);
void draw_line(HDC hdc, int x1, int y1, int x2, int y2, COLORREF c, int width);
void draw_hatch(HDC hdc, const RECT *r, COLORREF fg, COLORREF bg);

/* Semantic colours */
COLORREF status_color(TrainStatus s);
COLORREF status_tint(TrainStatus s);
COLORREF aspect_color(SignalAspect a);
COLORREF type_color(TrainType t);
COLORREF level_color(LogLevel l);

#endif /* GUI_THEME_H */
