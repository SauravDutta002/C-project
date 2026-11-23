/*
 * widgets.h - Flat owner-drawn buttons, list views and click regions
 */
#ifndef GUI_WIDGETS_H
#define GUI_WIDGETS_H

#include "gui/win.h"

/* ---- Buttons ---- */
typedef enum { BTN_DEFAULT, BTN_PRIMARY, BTN_DANGER, BTN_SUCCESS, BTN_SIDEBAR } ButtonStyle;

HWND btn_create(HWND parent, int id, const char *text, ButtonStyle style);
void btn_set_on(HWND b, int on);
void btn_set_text(HWND b, const char *text);
int  btn_ideal_width(HWND b);
void btn_draw(const DRAWITEMSTRUCT *dis);

/* ---- List views ---- */
#define TBL_MAX_ROWS 256
#define TBL_MAX_COLS 14
#define TBL_CELL     80
#define LV_KEEP_SELECTION ((LPARAM)-1)

typedef struct {
    int    rows, cols;
    char   cell[TBL_MAX_ROWS][TBL_MAX_COLS][TBL_CELL];
    LPARAM key[TBL_MAX_ROWS];
} GuiTable;

HWND   lv_create(HWND parent, int id, const char *const *names, const int *widths, int n);
void   tbl_reset(GuiTable *t, int cols);
int    tbl_add(GuiTable *t, LPARAM key);
void   tbl_set(GuiTable *t, int row, int col, const char *fmt, ...);
void   lv_sync(HWND lv, const GuiTable *t, LPARAM select_key);
LPARAM lv_selected_key(HWND lv);
LPARAM lv_key_at(HWND lv, int index);

/* ---- Click regions on painted surfaces ---- */
void hit_clear(void);
void hit_add(const RECT *rc, int kind, int id);
int  hit_test(int x, int y, int *kind, int *id);

#endif /* GUI_WIDGETS_H */
