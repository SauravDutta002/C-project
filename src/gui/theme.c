/*
 * theme.c - Colours, fonts and GDI drawing primitives
 */
#include <string.h>

#include "gui/theme.h"

Theme g_theme;

int S(int px) { return MulDiv(px, g_theme.dpi ? g_theme.dpi : 96, 96); }

static HFONT make_font(int pt, int weight, const char *face)
{
    return CreateFontA(-MulDiv(pt, g_theme.dpi, 72), 0, 0, 0, weight, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, face);
}

void theme_init(void)
{
    HDC hdc = GetDC(NULL);
    g_theme.dpi = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(NULL, hdc);

    g_theme.ui         = make_font(9,  FW_NORMAL,   "Segoe UI");
    g_theme.ui_bold    = make_font(9,  FW_SEMIBOLD, "Segoe UI");
    g_theme.sm      = make_font(8,  FW_NORMAL,   "Segoe UI");
    g_theme.small_bold = make_font(8,  FW_BOLD,     "Segoe UI");
    g_theme.title      = make_font(14, FW_BOLD,     "Segoe UI");
    g_theme.h2         = make_font(11, FW_SEMIBOLD, "Segoe UI");
    g_theme.big        = make_font(20, FW_BOLD,     "Segoe UI");
    g_theme.clock      = make_font(22, FW_SEMIBOLD, "Segoe UI");
    g_theme.mono       = make_font(9,  FW_NORMAL,   "Consolas");
    g_theme.mono_bold  = make_font(9,  FW_BOLD,     "Consolas");
}

void theme_free(void)
{
    HFONT *f[] = { &g_theme.ui, &g_theme.ui_bold, &g_theme.sm, &g_theme.small_bold, &g_theme.title,
                   &g_theme.h2, &g_theme.big, &g_theme.clock, &g_theme.mono, &g_theme.mono_bold };
    size_t i;
    for (i = 0; i < sizeof f / sizeof f[0]; i++) {
        if (*f[i]) DeleteObject(*f[i]);
        *f[i] = NULL;
    }
}

/* ------------------------------------------------------------------ */

void draw_fill(HDC hdc, const RECT *r, COLORREF c)
{
    HBRUSH b = CreateSolidBrush(c);
    FillRect(hdc, r, b);
    DeleteObject(b);
}

void draw_round(HDC hdc, const RECT *r, int radius, COLORREF fill, COLORREF border)
{
    HBRUSH b = CreateSolidBrush(fill);
    HPEN p = border == NO_BORDER ? (HPEN)GetStockObject(NULL_PEN) : CreatePen(PS_SOLID, 1, border);
    HGDIOBJ ob = SelectObject(hdc, b);
    HGDIOBJ op = SelectObject(hdc, p);
    int extra = border == NO_BORDER ? 1 : 0;

    RoundRect(hdc, r->left, r->top, r->right + extra, r->bottom + extra, radius * 2, radius * 2);
    SelectObject(hdc, ob);
    SelectObject(hdc, op);
    DeleteObject(b);
    if (border != NO_BORDER) DeleteObject(p);
}

void draw_round_outline(HDC hdc, const RECT *r, int radius, COLORREF border, int width)
{
    HPEN p = CreatePen(PS_SOLID, width, border);
    HGDIOBJ op = SelectObject(hdc, p);
    HGDIOBJ ob = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    RoundRect(hdc, r->left, r->top, r->right, r->bottom, radius * 2, radius * 2);
    SelectObject(hdc, ob);
    SelectObject(hdc, op);
    DeleteObject(p);
}

void draw_text(HDC hdc, const char *text, const RECT *r, HFONT font, COLORREF color, UINT fmt)
{
    RECT rc = *r;
    HGDIOBJ of = SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    DrawTextA(hdc, text, -1, &rc, fmt | DT_NOPREFIX);
    SelectObject(hdc, of);
}

int text_width(HDC hdc, const char *text, HFONT font)
{
    SIZE sz;
    HGDIOBJ of = SelectObject(hdc, font);
    GetTextExtentPoint32A(hdc, text, (int)strlen(text), &sz);
    SelectObject(hdc, of);
    return sz.cx;
}

int pill_width(HDC hdc, const char *text, HFONT font)
{
    return text_width(hdc, text, font) + S(18);
}

int draw_pill(HDC hdc, int x, int y, const char *text, COLORREF bg, COLORREF fg, HFONT font)
{
    int w = pill_width(hdc, text, font);
    RECT r;
    r.left = x; r.top = y; r.right = x + w; r.bottom = y + S(20);
    draw_round(hdc, &r, S(10), bg, NO_BORDER);
    draw_text(hdc, text, &r, font, fg, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    return w;
}

void draw_circle(HDC hdc, int cx, int cy, int r, COLORREF fill, COLORREF border)
{
    HBRUSH b = CreateSolidBrush(fill);
    HPEN p = border == NO_BORDER ? (HPEN)GetStockObject(NULL_PEN) : CreatePen(PS_SOLID, 1, border);
    HGDIOBJ ob = SelectObject(hdc, b);
    HGDIOBJ op = SelectObject(hdc, p);
    Ellipse(hdc, cx - r, cy - r, cx + r + 1, cy + r + 1);
    SelectObject(hdc, ob);
    SelectObject(hdc, op);
    DeleteObject(b);
    if (border != NO_BORDER) DeleteObject(p);
}

void draw_line(HDC hdc, int x1, int y1, int x2, int y2, COLORREF c, int width)
{
    LOGBRUSH lb;
    HPEN p;
    HGDIOBJ op;
    lb.lbStyle = BS_SOLID;
    lb.lbColor = c;
    lb.lbHatch = 0;
    p = ExtCreatePen(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_ROUND | PS_JOIN_ROUND, width, &lb, 0, NULL);
    op = SelectObject(hdc, p);
    MoveToEx(hdc, x1, y1, NULL);
    LineTo(hdc, x2, y2);
    SelectObject(hdc, op);
    DeleteObject(p);
}

void draw_hatch(HDC hdc, const RECT *r, COLORREF fg, COLORREF bg)
{
    HBRUSH b = CreateHatchBrush(HS_BDIAGONAL, fg);
    int old = SetBkMode(hdc, OPAQUE);
    COLORREF oc = SetBkColor(hdc, bg);
    FillRect(hdc, r, b);
    SetBkColor(hdc, oc);
    SetBkMode(hdc, old);
    DeleteObject(b);
}

/* ------------------------------------------------------------------ */

COLORREF status_color(TrainStatus s)
{
    switch (s) {
    case TS_SCHEDULED:   return COL_SLATE;
    case TS_APPROACHING: return COL_CYAN;
    case TS_HELD:        return COL_RED;
    case TS_AT_PLATFORM: return COL_GREEN;
    case TS_DEPARTED:    return COL_FAINT;
    case TS_CANCELLED:   return COL_PURPLE;
    default:             return COL_TEXT;
    }
}

COLORREF status_tint(TrainStatus s)
{
    switch (s) {
    case TS_APPROACHING: return RGBX(0xF0FDFF);
    case TS_HELD:        return RGBX(0xFEF2F2);
    case TS_AT_PLATFORM: return RGBX(0xF0FDF4);
    case TS_DEPARTED:    return RGBX(0xFAFAFB);
    case TS_CANCELLED:   return RGBX(0xF8F5FF);
    default:             return COL_CARD;
    }
}

COLORREF aspect_color(SignalAspect a)
{
    if (a == SA_RED)   return COL_LAMP_RED;
    if (a == SA_GREEN) return COL_LAMP_GREEN;
    return COL_LAMP_YELLOW;
}

COLORREF type_color(TrainType t)
{
    switch (t) {
    case TT_SUPERFAST: return RGBX(0x2563EB);
    case TT_EXPRESS:   return RGBX(0x7C3AED);
    case TT_PASSENGER: return RGBX(0x0891B2);
    case TT_LOCAL:     return RGBX(0x16A34A);
    case TT_FREIGHT:   return RGBX(0xA16207);
    default:           return COL_SLATE;
    }
}

COLORREF level_color(LogLevel l)
{
    switch (l) {
    case LOG_OK:    return COL_GREEN;
    case LOG_WARN:  return COL_AMBER;
    case LOG_ALERT: return COL_RED;
    default:        return COL_BLUE;
    }
}
