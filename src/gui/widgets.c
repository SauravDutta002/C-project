/*
 * widgets.c - Flat owner-drawn buttons, list views and click regions
 */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "gui/widgets.h"
#include "gui/theme.h"

#define PROP_STYLE "rn_style"
#define PROP_ON    "rn_on"

/* ------------------------------------------------------------------ */
/* Buttons                                                             */
/* ------------------------------------------------------------------ */

HWND btn_create(HWND parent, int id, const char *text, ButtonStyle style)
{
    HWND b = CreateWindowExA(0, "BUTTON", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                             0, 0, 10, 10, parent, (HMENU)(INT_PTR)id,
                             GetModuleHandleA(NULL), NULL);
    SetPropA(b, PROP_STYLE, (HANDLE)(INT_PTR)style);
    SendMessageA(b, WM_SETFONT, (WPARAM)g_theme.ui_bold, FALSE);
    return b;
}

void btn_set_on(HWND b, int on)
{
    if ((int)(INT_PTR)GetPropA(b, PROP_ON) == on) return;
    SetPropA(b, PROP_ON, (HANDLE)(INT_PTR)on);
    InvalidateRect(b, NULL, FALSE);
}

void btn_set_text(HWND b, const char *text)
{
    char cur[64];
    GetWindowTextA(b, cur, sizeof cur);
    if (strcmp(cur, text) != 0) SetWindowTextA(b, text);
}

int btn_ideal_width(HWND b)
{
    char text[64];
    HDC hdc = GetDC(b);
    int w;
    GetWindowTextA(b, text, sizeof text);
    w = text_width(hdc, text, g_theme.ui_bold) + S(30);
    ReleaseDC(b, hdc);
    return w;
}

void btn_draw(const DRAWITEMSTRUCT *d)
{
    ButtonStyle style = (ButtonStyle)(INT_PTR)GetPropA(d->hwndItem, PROP_STYLE);
    int on = (int)(INT_PTR)GetPropA(d->hwndItem, PROP_ON);
    int pressed = (d->itemState & ODS_SELECTED) != 0;
    int disabled = (d->itemState & ODS_DISABLED) != 0;
    COLORREF bg, fg, border = NO_BORDER, parent = COL_BG;
    char text[64];
    RECT r = d->rcItem;

    switch (style) {
    case BTN_PRIMARY:
        bg = pressed ? RGBX(0x1D4ED8) : COL_BLUE; fg = RGB(255, 255, 255); break;
    case BTN_DANGER:
        bg = pressed ? RGBX(0xB91C1C) : COL_RED; fg = RGB(255, 255, 255); break;
    case BTN_SUCCESS:
        bg = pressed ? RGBX(0x15803D) : COL_GREEN; fg = RGB(255, 255, 255); break;
    case BTN_SIDEBAR:
        parent = COL_SIDEBAR;
        bg = on ? COL_BLUE : (pressed ? RGBX(0x475569) : COL_NAV_ACTIVE);
        fg = on ? RGB(255, 255, 255) : RGBX(0xE2E8F0);
        break;
    default:
        bg = pressed ? COL_BORDER : COL_CARD; fg = COL_TEXT; border = RGBX(0xCBD5E1); break;
    }
    if (GetPropA(d->hwndItem, "rn_dark")) parent = COL_SIDEBAR;
    if (on && style != BTN_SIDEBAR) { bg = COL_BLUE_T; border = COL_BLUE; fg = COL_BLUE; }
    if (disabled) {
        bg = style == BTN_SIDEBAR ? RGBX(0x2B3648) : RGBX(0xEEF2F6);
        fg = style == BTN_SIDEBAR ? RGBX(0x64748B) : COL_FAINT;
        border = style == BTN_SIDEBAR ? NO_BORDER : COL_BORDER;
    }

    draw_fill(d->hDC, &r, parent);
    draw_round(d->hDC, &r, S(6), bg, border);
    GetWindowTextA(d->hwndItem, text, sizeof text);
    if (pressed) OffsetRect(&r, 0, 1);
    draw_text(d->hDC, text, &r, g_theme.ui_bold, fg, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

/* ------------------------------------------------------------------ */
/* List views                                                          */
/* ------------------------------------------------------------------ */

typedef HRESULT (WINAPI *SetWindowThemeFn)(HWND, LPCWSTR, LPCWSTR);

static void apply_explorer_theme(HWND h)
{
    static SetWindowThemeFn fn = NULL;
    static int tried = 0;
    if (!tried) {
        HMODULE ux = LoadLibraryA("uxtheme.dll");
        tried = 1;
        if (ux) fn = (SetWindowThemeFn)(void *)GetProcAddress(ux, "SetWindowTheme");
    }
    if (fn) fn(h, L"Explorer", NULL);
}

HWND lv_create(HWND parent, int id, const char *const *names, const int *widths, int n)
{
    HWND lv = CreateWindowExA(0, "SysListView32", "",
                              WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP |
                              LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER,
                              0, 0, 10, 10, parent, (HMENU)(INT_PTR)id,
                              GetModuleHandleA(NULL), NULL);
    HIMAGELIST il;
    LVCOLUMNA c;
    int i;

    ListView_SetExtendedListViewStyle(lv, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
    apply_explorer_theme(lv);
    SendMessageA(lv, WM_SETFONT, (WPARAM)g_theme.ui, FALSE);

    /* a 1px-wide image list is the classic way to get taller, airier rows */
    il = ImageList_Create(1, S(25), ILC_COLOR, 1, 1);
    ListView_SetImageList(lv, il, LVSIL_SMALL);

    memset(&c, 0, sizeof c);
    c.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM | LVCF_FMT;
    for (i = 0; i < n; i++) {
        c.fmt = LVCFMT_LEFT;
        c.cx = S(widths[i]);
        c.pszText = (char *)names[i];
        c.iSubItem = i;
        SendMessageA(lv, LVM_INSERTCOLUMNA, (WPARAM)i, (LPARAM)&c);
    }
    return lv;
}

void tbl_reset(GuiTable *t, int cols)
{
    t->rows = 0;
    t->cols = cols > TBL_MAX_COLS ? TBL_MAX_COLS : cols;
}

int tbl_add(GuiTable *t, LPARAM key)
{
    int c;
    if (t->rows >= TBL_MAX_ROWS) return -1;
    for (c = 0; c < t->cols; c++) t->cell[t->rows][c][0] = '\0';
    t->key[t->rows] = key;
    return t->rows++;
}

void tbl_set(GuiTable *t, int row, int col, const char *fmt, ...)
{
    va_list ap;
    if (row < 0 || row >= t->rows || col < 0 || col >= t->cols) return;
    va_start(ap, fmt);
    vsnprintf(t->cell[row][col], TBL_CELL, fmt, ap);
    va_end(ap);
}

/* Update rows in place (no flicker, keeps scroll position) */
void lv_sync(HWND lv, const GuiTable *t, LPARAM select_key)
{
    int count = ListView_GetItemCount(lv);
    int r, c;
    LVITEMA it;

    SendMessageA(lv, WM_SETREDRAW, FALSE, 0);
    while (count > t->rows) ListView_DeleteItem(lv, --count);

    for (r = 0; r < t->rows; r++) {
        memset(&it, 0, sizeof it);
        it.iItem   = r;
        it.mask    = LVIF_TEXT | LVIF_PARAM;
        it.pszText = (char *)t->cell[r][0];
        it.lParam  = t->key[r];
        if (select_key != LV_KEEP_SELECTION) {
            it.mask     |= LVIF_STATE;
            it.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
            it.state     = (select_key && t->key[r] == select_key) ? (LVIS_SELECTED | LVIS_FOCUSED) : 0;
        }
        if (r >= count) SendMessageA(lv, LVM_INSERTITEMA, 0, (LPARAM)&it);
        else            SendMessageA(lv, LVM_SETITEMA, 0, (LPARAM)&it);

        for (c = 1; c < t->cols; c++) {
            LVITEMA s;
            memset(&s, 0, sizeof s);
            s.iSubItem = c;
            s.pszText  = (char *)t->cell[r][c];
            SendMessageA(lv, LVM_SETITEMTEXTA, (WPARAM)r, (LPARAM)&s);
        }
    }
    SendMessageA(lv, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(lv, NULL, FALSE);
}

LPARAM lv_key_at(HWND lv, int index)
{
    LVITEMA it;
    if (index < 0) return 0;
    memset(&it, 0, sizeof it);
    it.mask = LVIF_PARAM;
    it.iItem = index;
    if (!SendMessageA(lv, LVM_GETITEMA, 0, (LPARAM)&it)) return 0;
    return it.lParam;
}

LPARAM lv_selected_key(HWND lv)
{
    return lv_key_at(lv, ListView_GetNextItem(lv, -1, LVNI_SELECTED));
}

/* ------------------------------------------------------------------ */
/* Click regions                                                       */
/* ------------------------------------------------------------------ */

typedef struct { RECT rc; int kind, id; } HitRegion;

static HitRegion g_hits[384];
static int g_hit_count = 0;

void hit_clear(void) { g_hit_count = 0; }

void hit_add(const RECT *rc, int kind, int id)
{
    if (g_hit_count >= (int)(sizeof g_hits / sizeof g_hits[0])) return;
    g_hits[g_hit_count].rc = *rc;
    g_hits[g_hit_count].kind = kind;
    g_hits[g_hit_count].id = id;
    g_hit_count++;
}

/* Last added wins: painters add background regions first, details on top */
int hit_test(int x, int y, int *kind, int *id)
{
    int i;
    POINT p;
    p.x = x;
    p.y = y;
    for (i = g_hit_count - 1; i >= 0; i--) {
        if (PtInRect(&g_hits[i].rc, p)) {
            *kind = g_hits[i].kind;
            *id = g_hits[i].id;
            return 1;
        }
    }
    return 0;
}
