/*
 * app.h - Main window state shared across the GUI modules
 */
#ifndef GUI_APP_H
#define GUI_APP_H

#include "gui/win.h"
#include "core/station.h"

typedef enum {
    PAGE_DASHBOARD, PAGE_TRAINS, PAGE_PLATFORMS, PAGE_SIGNALS, PAGE_DELAYS, PAGE_LOG,
    PAGE_COUNT
} Page;

enum { HIT_NAV = 1, HIT_SIGNAL, HIT_TRAIN, HIT_PLATFORM };

typedef struct {
    HINSTANCE inst;
    HWND      hwnd, status;
    HACCEL    accel;
    HICON     icon, icon_small;
    HMENU     menu;

    Station   st;

    Page      page;
    int       hover_nav;
    int       running;
    int       speed;          /* simulated minutes per second */
    int       sel_train;      /* selected train id, 0 = none  */
    int       syncing;        /* suppress selection events while refreshing lists */

    HWND lv_dash, lv_events, lv_trains, lv_delays, lv_log;
    HWND search, chk_departed, chk_autopf, chk_autosig;
    HWND btn_run, btn_speed[4];

    /* layout */
    RECT rc_header, rc_sidebar, rc_content, rc_sim;
    RECT rc_a, rc_b, rc_c, rc_d;    /* page-specific areas */
} App;

extern App g_app;

/* app.c */
void app_refresh(void);
void app_set_page(Page p);
void app_layout(void);
void app_status(const char *msg);
int  app_run(HINSTANCE inst, int show);

/* commands.c */
void   cmd_handle(int id);
Train *app_current_train(int warn);
void   menu_train(Train *t, POINT screen_pt);
void   menu_signal(RailSignal *s, POINT screen_pt);
void   menu_platform(Platform *p, POINT screen_pt);
void   show_train_details(Train *t);
int    confirm_close(void);

/* dialogs.c */
int dlg_train(Train *edit);            /* NULL = add new */
int dlg_delay(Train *t, int recover);

/* panels.c */
void paint_header(HDC hdc, const RECT *r);
void paint_sidebar(HDC hdc, const RECT *r);
void paint_stat_card(HDC hdc, const RECT *r, const char *label, const char *value,
                     const char *sub, COLORREF accent);
void paint_card(HDC hdc, const RECT *r, const char *title, const char *subtitle);
RECT card_body(const RECT *card);

/* mimic.c */
void paint_mimic(HDC hdc, const RECT *r, Station *st);
void paint_signal_board(HDC hdc, const RECT *r, Station *st);

/* charts.c */
void paint_platform_cards(HDC hdc, const RECT *r, Station *st);
void paint_gantt(HDC hdc, const RECT *r, Station *st);
void paint_delay_bars(HDC hdc, const RECT *r, Station *st);

#endif /* GUI_APP_H */
