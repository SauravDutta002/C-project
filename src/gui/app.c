/*
 * app.c - Main window: creation, layout, painting, list refresh, input
 */
#include <stdio.h>
#include <string.h>

#include "gui/app.h"
#include "gui/resource.h"
#include "gui/theme.h"
#include "gui/widgets.h"
#include "core/scheduler.h"
#include "core/demo_data.h"
#include "io/storage.h"
#include "util/utils.h"

#define SIM_TIMER   1
#define CLASS_NAME  "RailNetPMSMain"

App g_app;

static GuiTable g_tbl;
static HBRUSH   g_bg_brush;

typedef struct { HWND h; int page; } PageCtl;    /* page -1 = always visible */
static PageCtl g_ctls[64];
static int     g_nctl = 0;

static HWND tb_trains[6], tb_platforms[1], tb_signals[2], tb_delays[3], tb_log[2];
static HWND btn_step[3];

/* ------------------------------------------------------------------ */
/* Table columns                                                       */
/* ------------------------------------------------------------------ */

static const char *COLS_DASH[]  = { "Train", "Name", "Scheduled", "Expected", "Delay", "PF", "Status" };
static const int   W_DASH[]     = { 62, 150, 70, 76, 70, 40, 110 };
static const char *COLS_EVT[]   = { "Time", "Level", "Event" };
static const int   W_EVT[]      = { 50, 56, 300 };
static const char *COLS_TRAIN[] = { "Train", "Name", "Status", "PF", "ETA", "ETD", "Delay", "Sch. arr", "Sch. dep",
                                    "Type", "From", "To", "Coaches", "Delay reason" };
static const int   W_TRAIN[]    = { 64, 175, 112, 42, 56, 56, 72, 64, 64, 82, 100, 100, 62, 150 };
static const char *COLS_DELAY[] = { "Train", "Name", "Scheduled", "Expected", "Delay", "Reason" };
static const int   W_DELAY[]    = { 64, 160, 74, 72, 72, 130 };
static const char *COLS_LOG[]   = { "Time", "Level", "Event" };
static const int   W_LOG[]      = { 70, 70, 900 };

#define NCOLS(a) ((int)(sizeof(a) / sizeof((a)[0])))

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static HWND reg(HWND h, int page)
{
    if (g_nctl < (int)(sizeof g_ctls / sizeof g_ctls[0])) {
        g_ctls[g_nctl].h = h;
        g_ctls[g_nctl].page = page;
        g_nctl++;
    }
    return h;
}

static HWND make_check(HWND parent, int id, const char *text)
{
    HWND h = CreateWindowExA(0, "BUTTON", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                             0, 0, 10, 10, parent, (HMENU)(INT_PTR)id, g_app.inst, NULL);
    SendMessageA(h, WM_SETFONT, (WPARAM)g_theme.ui, FALSE);
    return h;
}

static void move(HWND h, int x, int y, int w, int ht) { MoveWindow(h, x, y, w, ht, FALSE); }

static int place_toolbar(HWND *btns, int n, int x, int y)
{
    int i;
    for (i = 0; i < n; i++) {
        int w = btn_ideal_width(btns[i]);
        move(btns[i], x, y, w, S(34));
        x += w + S(8);
    }
    return x;
}

static void place_list(HWND lv, const RECT *card)
{
    RECT b = card_body(card);
    HWND hdr = ListView_GetHeader(lv);
    int cols = hdr ? Header_GetItemCount(hdr) : 0;
    move(lv, b.left + S(8), b.top + S(4), b.right - b.left - S(16), b.bottom - b.top - S(4));
    /* let the last column fill the remaining width (vertical scrollbar reserved) */
    if (cols > 0) {
        RECT cr;
        int i, used = 0, avail;
        GetClientRect(lv, &cr);
        avail = b.right - b.left - S(16) - GetSystemMetrics(SM_CXVSCROLL) - S(4);
        for (i = 0; i < cols - 1; i++) used += ListView_GetColumnWidth(lv, i);
        if (avail - used > S(90)) ListView_SetColumnWidth(lv, cols - 1, avail - used);
    }
}

static int is_train_list(HWND h)
{
    return h == g_app.lv_dash || h == g_app.lv_trains || h == g_app.lv_delays;
}

/* ------------------------------------------------------------------ */
/* Controls                                                            */
/* ------------------------------------------------------------------ */

static void create_controls(HWND h)
{
    int i;
    static const char *speeds[4] = { "x1", "x2", "x5", "x10" };
    static const int speed_ids[4] = { IDM_SPEED1, IDM_SPEED2, IDM_SPEED5, IDM_SPEED10 };

    /* sidebar simulation controls */
    g_app.btn_run = reg(btn_create(h, IDM_RUN, "Run simulation", BTN_SUCCESS), -1);
    SetPropA(g_app.btn_run, "rn_dark", (HANDLE)1);
    for (i = 0; i < 4; i++) g_app.btn_speed[i] = reg(btn_create(h, speed_ids[i], speeds[i], BTN_SIDEBAR), -1);
    btn_step[0] = reg(btn_create(h, IDM_STEP1, "+1 min", BTN_SIDEBAR), -1);
    btn_step[1] = reg(btn_create(h, IDM_STEP5, "+5 min", BTN_SIDEBAR), -1);
    btn_step[2] = reg(btn_create(h, IDM_STEP15, "+15 min", BTN_SIDEBAR), -1);

    /* dashboard */
    g_app.lv_dash   = reg(lv_create(h, IDC_LV_DASH, COLS_DASH, W_DASH, NCOLS(COLS_DASH)), PAGE_DASHBOARD);
    g_app.lv_events = reg(lv_create(h, IDC_LV_EVENTS, COLS_EVT, W_EVT, NCOLS(COLS_EVT)), PAGE_DASHBOARD);

    /* trains */
    tb_trains[0] = reg(btn_create(h, IDM_ADD_TRAIN, "+  Add train", BTN_PRIMARY), PAGE_TRAINS);
    tb_trains[1] = reg(btn_create(h, IDM_EDIT_TRAIN, "Edit", BTN_DEFAULT), PAGE_TRAINS);
    tb_trains[2] = reg(btn_create(h, IDM_DELAY_TRAIN, "Report delay", BTN_DEFAULT), PAGE_TRAINS);
    tb_trains[3] = reg(btn_create(h, IDM_ASSIGN_TRAIN, "Assign platform", BTN_DEFAULT), PAGE_TRAINS);
    tb_trains[4] = reg(btn_create(h, IDM_CANCEL_TRAIN, "Cancel train", BTN_DEFAULT), PAGE_TRAINS);
    tb_trains[5] = reg(btn_create(h, IDM_REMOVE_TRAIN, "Remove", BTN_DANGER), PAGE_TRAINS);
    g_app.chk_departed = reg(make_check(h, IDC_CHK_DEPARTED, "Show past trains"), PAGE_TRAINS);
    g_app.search = reg(CreateWindowExA(0, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
                                       0, 0, 10, 10, h, (HMENU)(INT_PTR)IDC_SEARCH, g_app.inst, NULL), PAGE_TRAINS);
    SendMessageA(g_app.search, WM_SETFONT, (WPARAM)g_theme.ui, FALSE);
    SendMessageW(g_app.search, EM_SETCUEBANNER, TRUE, (LPARAM)L"Search number, name or city");
    g_app.lv_trains = reg(lv_create(h, IDC_LV_TRAINS, COLS_TRAIN, W_TRAIN, NCOLS(COLS_TRAIN)), PAGE_TRAINS);

    /* platforms */
    tb_platforms[0] = reg(btn_create(h, IDM_AUTOASSIGN_ALL, "Auto-assign all unassigned", BTN_PRIMARY), PAGE_PLATFORMS);
    g_app.chk_autopf = reg(make_check(h, IDC_CHK_AUTOPF, "Allocate platforms automatically when trains approach"), PAGE_PLATFORMS);

    /* signals */
    tb_signals[0] = reg(btn_create(h, IDM_RELEASE_ALL, "Release all overrides", BTN_DEFAULT), PAGE_SIGNALS);
    tb_signals[1] = reg(btn_create(h, IDM_EMERGENCY, "EMERGENCY STOP", BTN_DANGER), PAGE_SIGNALS);
    g_app.chk_autosig = reg(make_check(h, IDC_CHK_AUTOSIG, "Automatic signalling"), PAGE_SIGNALS);

    /* delays */
    tb_delays[0] = reg(btn_create(h, IDM_DELAY_TRAIN, "Report delay", BTN_PRIMARY), PAGE_DELAYS);
    tb_delays[1] = reg(btn_create(h, IDM_RECOVER_TRAIN, "Recover time", BTN_DEFAULT), PAGE_DELAYS);
    tb_delays[2] = reg(btn_create(h, IDM_CLEAR_DELAY, "Clear delay", BTN_DEFAULT), PAGE_DELAYS);
    g_app.lv_delays = reg(lv_create(h, IDC_LV_DELAYS, COLS_DELAY, W_DELAY, NCOLS(COLS_DELAY)), PAGE_DELAYS);

    /* log */
    tb_log[0] = reg(btn_create(h, IDM_EXPORT, "Export arrivals board", BTN_DEFAULT), PAGE_LOG);
    tb_log[1] = reg(btn_create(h, IDM_OPEN_DATA, "Open data folder", BTN_DEFAULT), PAGE_LOG);
    g_app.lv_log = reg(lv_create(h, IDC_LV_LOG, COLS_LOG, W_LOG, NCOLS(COLS_LOG)), PAGE_LOG);

    g_app.status = CreateWindowExA(0, STATUSCLASSNAMEA, "", WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
                                   0, 0, 0, 0, h, (HMENU)(INT_PTR)IDC_STATUS, g_app.inst, NULL);
    SendMessageA(g_app.status, WM_SETFONT, (WPARAM)g_theme.ui, FALSE);
}

static HMENU build_menu(void)
{
    HMENU bar = CreateMenu();
    HMENU file = CreatePopupMenu(), view = CreatePopupMenu(), trains = CreatePopupMenu();
    HMENU pf = CreatePopupMenu(), sig = CreatePopupMenu(), sim = CreatePopupMenu(), help = CreatePopupMenu();

    AppendMenuA(file, MF_STRING, IDM_SAVE, "&Save session\tCtrl+S");
    AppendMenuA(file, MF_STRING, IDM_LOAD, "&Open saved session\tCtrl+O");
    AppendMenuA(file, MF_SEPARATOR, 0, NULL);
    AppendMenuA(file, MF_STRING, IDM_EXPORT, "&Export arrivals board...");
    AppendMenuA(file, MF_STRING, IDM_OPEN_DATA, "Open &data folder");
    AppendMenuA(file, MF_SEPARATOR, 0, NULL);
    AppendMenuA(file, MF_STRING, IDM_RESET, "&Reset to demo timetable");
    AppendMenuA(file, MF_SEPARATOR, 0, NULL);
    AppendMenuA(file, MF_STRING, IDM_EXIT, "E&xit");

    AppendMenuA(view, MF_STRING, IDM_PAGE_DASH, "&Dashboard\tCtrl+1");
    AppendMenuA(view, MF_STRING, IDM_PAGE_TRAINS, "&Trains\tCtrl+2");
    AppendMenuA(view, MF_STRING, IDM_PAGE_PLATFORMS, "&Platforms\tCtrl+3");
    AppendMenuA(view, MF_STRING, IDM_PAGE_SIGNALS, "&Signals\tCtrl+4");
    AppendMenuA(view, MF_STRING, IDM_PAGE_DELAYS, "D&elays\tCtrl+5");
    AppendMenuA(view, MF_STRING, IDM_PAGE_LOG, "Event &log\tCtrl+6");

    AppendMenuA(trains, MF_STRING, IDM_ADD_TRAIN, "&Add train...\tCtrl+N");
    AppendMenuA(trains, MF_STRING, IDM_EDIT_TRAIN, "&Edit selected train...");
    AppendMenuA(trains, MF_STRING, IDM_DETAILS, "Train &details");
    AppendMenuA(trains, MF_SEPARATOR, 0, NULL);
    AppendMenuA(trains, MF_STRING, IDM_DELAY_TRAIN, "Report &delay...\tCtrl+D");
    AppendMenuA(trains, MF_STRING, IDM_RECOVER_TRAIN, "&Recover time...");
    AppendMenuA(trains, MF_STRING, IDM_CLEAR_DELAY, "&Clear delay");
    AppendMenuA(trains, MF_SEPARATOR, 0, NULL);
    AppendMenuA(trains, MF_STRING, IDM_ASSIGN_TRAIN, "Assign &platform...");
    AppendMenuA(trains, MF_SEPARATOR, 0, NULL);
    AppendMenuA(trains, MF_STRING, IDM_CANCEL_TRAIN, "Ca&ncel train");
    AppendMenuA(trains, MF_STRING, IDM_REMOVE_TRAIN, "Re&move from timetable");

    AppendMenuA(pf, MF_STRING, IDM_AUTOPF, "&Automatic allocation on approach");
    AppendMenuA(pf, MF_STRING, IDM_AUTOASSIGN_ALL, "Auto-assign all &unassigned");

    AppendMenuA(sig, MF_STRING, IDM_AUTOSIG, "&Automatic signalling");
    AppendMenuA(sig, MF_STRING, IDM_RELEASE_ALL, "&Release all overrides");
    AppendMenuA(sig, MF_SEPARATOR, 0, NULL);
    AppendMenuA(sig, MF_STRING, IDM_EMERGENCY, "&EMERGENCY STOP - all signals red");

    AppendMenuA(sim, MF_STRING, IDM_RUN, "&Run simulation\tF5");
    AppendMenuA(sim, MF_SEPARATOR, 0, NULL);
    AppendMenuA(sim, MF_STRING, IDM_STEP1, "Step &1 minute\tF6");
    AppendMenuA(sim, MF_STRING, IDM_STEP5, "Step &5 minutes\tF7");
    AppendMenuA(sim, MF_STRING, IDM_STEP15, "Step 15 &minutes\tF8");
    AppendMenuA(sim, MF_SEPARATOR, 0, NULL);
    AppendMenuA(sim, MF_STRING, IDM_SPEED1, "Speed x1  (1 min / s)");
    AppendMenuA(sim, MF_STRING, IDM_SPEED2, "Speed x2");
    AppendMenuA(sim, MF_STRING, IDM_SPEED5, "Speed x5");
    AppendMenuA(sim, MF_STRING, IDM_SPEED10, "Speed x10");

    AppendMenuA(help, MF_STRING, IDM_LEGEND, "&Legend && operating rules");
    AppendMenuA(help, MF_SEPARATOR, 0, NULL);
    AppendMenuA(help, MF_STRING, IDM_ABOUT, "&About RailNet PMS");

    AppendMenuA(bar, MF_POPUP, (UINT_PTR)file, "&File");
    AppendMenuA(bar, MF_POPUP, (UINT_PTR)view, "&View");
    AppendMenuA(bar, MF_POPUP, (UINT_PTR)trains, "&Trains");
    AppendMenuA(bar, MF_POPUP, (UINT_PTR)pf, "&Platforms");
    AppendMenuA(bar, MF_POPUP, (UINT_PTR)sig, "&Signals");
    AppendMenuA(bar, MF_POPUP, (UINT_PTR)sim, "Si&mulation");
    AppendMenuA(bar, MF_POPUP, (UINT_PTR)help, "&Help");
    return bar;
}

/* ------------------------------------------------------------------ */
/* Layout                                                              */
/* ------------------------------------------------------------------ */

void app_layout(void)
{
    RECT cr, sr;
    int W, H, sh, i, L, T, R, B, x, w;
    int parts[3];

    if (!g_app.hwnd) return;
    GetClientRect(g_app.hwnd, &cr);
    SendMessageA(g_app.status, WM_SIZE, 0, 0);
    GetWindowRect(g_app.status, &sr);
    sh = sr.bottom - sr.top;
    parts[0] = S(300);
    parts[1] = S(720);
    parts[2] = -1;
    SendMessageA(g_app.status, SB_SETPARTS, 3, (LPARAM)parts);

    W = cr.right;
    H = cr.bottom - sh;
    SetRect(&g_app.rc_header, 0, 0, W, S(64));
    SetRect(&g_app.rc_sidebar, 0, S(64), S(224), H);
    SetRect(&g_app.rc_content, S(224), S(64), W, H);
    SetRect(&g_app.rc_sim, S(12), H - S(196), S(224) - S(12), H - S(10));

    /* sidebar simulation controls */
    x = g_app.rc_sim.left + S(8);
    w = g_app.rc_sim.right - g_app.rc_sim.left - S(16);
    move(g_app.btn_run, x, g_app.rc_sim.top + S(28), w, S(38));
    {
        int bw = (w - 3 * S(6)) / 4;
        for (i = 0; i < 4; i++) move(g_app.btn_speed[i], x + i * (bw + S(6)), g_app.rc_sim.top + S(96), bw, S(30));
        bw = (w - 2 * S(6)) / 3;
        for (i = 0; i < 3; i++) move(btn_step[i], x + i * (bw + S(6)), g_app.rc_sim.top + S(154), bw, S(30));
    }

    /* show only the current page's controls */
    for (i = 0; i < g_nctl; i++)
        ShowWindow(g_ctls[i].h, (g_ctls[i].page < 0 || g_ctls[i].page == (int)g_app.page) ? SW_SHOWNA : SW_HIDE);

    L = g_app.rc_content.left + S(20);
    T = g_app.rc_content.top + S(18);
    R = g_app.rc_content.right - S(20);
    B = g_app.rc_content.bottom - S(18);

    switch (g_app.page) {
    case PAGE_DASHBOARD: {
        int mh = B - T - S(92) - S(14) - S(230);    /* leave room for the tables */
        int top;
        if (mh > S(360)) mh = S(360);
        if (mh < S(190)) mh = S(190);
        SetRect(&g_app.rc_a, L, T, R, T + S(80));
        SetRect(&g_app.rc_b, L, T + S(92), R, T + S(92) + mh);
        top = g_app.rc_b.bottom + S(14);
        SetRect(&g_app.rc_c, L, top, L + (R - L) * 60 / 100 - S(7), B);
        SetRect(&g_app.rc_d, g_app.rc_c.right + S(14), top, R, B);
        place_list(g_app.lv_dash, &g_app.rc_c);
        place_list(g_app.lv_events, &g_app.rc_d);
        break;
    }
    case PAGE_TRAINS: {
        int sx;
        x = place_toolbar(tb_trains, 6, L, T) + S(8);
        move(g_app.chk_departed, x, T + S(6), S(150), S(22));
        sx = x + S(160);
        if (sx < R - S(260)) sx = R - S(260);
        move(g_app.search, sx, T + S(4), R - sx, S(26));
        SetRect(&g_app.rc_a, L, T + S(48), R, B);
        place_list(g_app.lv_trains, &g_app.rc_a);
        break;
    }
    case PAGE_PLATFORMS: {
        int cols = (R - L) >= S(980) ? 3 : 2;
        int rows = (g_app.st.platform_count + cols - 1) / cols;
        int ch = rows * S(104) + (rows - 1) * S(14);
        x = place_toolbar(tb_platforms, 1, L, T);
        move(g_app.chk_autopf, x + S(12), T + S(6), S(380), S(22));
        SetRect(&g_app.rc_a, L, T + S(48), R, T + S(48) + ch);
        SetRect(&g_app.rc_b, L, g_app.rc_a.bottom + S(16), R, B);
        break;
    }
    case PAGE_SIGNALS:
        move(g_app.chk_autosig, L, T + S(6), S(170), S(22));
        place_toolbar(tb_signals, 2, L + S(180), T);
        SetRect(&g_app.rc_a, L, T + S(48), R, B);
        break;
    case PAGE_DELAYS:
        SetRect(&g_app.rc_a, L, T, R, T + S(92));
        place_toolbar(tb_delays, 3, L, T + S(106));
        SetRect(&g_app.rc_b, L, T + S(154), L + (R - L) * 58 / 100 - S(7), B);
        SetRect(&g_app.rc_c, g_app.rc_b.right + S(14), T + S(154), R, B);
        place_list(g_app.lv_delays, &g_app.rc_b);
        break;
    case PAGE_LOG:
        place_toolbar(tb_log, 2, L, T);
        SetRect(&g_app.rc_a, L, T + S(48), R, B);
        place_list(g_app.lv_log, &g_app.rc_a);
        break;
    default:
        break;
    }
    InvalidateRect(g_app.hwnd, NULL, FALSE);
    for (i = 0; i < g_nctl; i++) InvalidateRect(g_ctls[i].h, NULL, FALSE);
}

void app_set_page(Page p)
{
    if ((int)p < 0 || p >= PAGE_COUNT) return;
    g_app.page = p;
    app_layout();
    app_refresh();
}

/* ------------------------------------------------------------------ */
/* List content                                                        */
/* ------------------------------------------------------------------ */

static void delay_text(const Train *t, char *buf, int size)
{
    if (t->status == TS_CANCELLED) snprintf(buf, (size_t)size, "-");
    else if (t->delay > 0)         snprintf(buf, (size_t)size, "+%d min", t->delay);
    else                           snprintf(buf, (size_t)size, "On time");
}

static void pf_text(const Train *t, char *buf, int size)
{
    if (t->platform) snprintf(buf, (size_t)size, "P%d", t->platform);
    else             snprintf(buf, (size_t)size, "-");
}

static void refresh_dash(void)
{
    Train *list[MAX_TRAINS];
    int n = sched_sorted(&g_app.st, list, MAX_TRAINS, 0), i;
    char d[16], p[8];

    tbl_reset(&g_tbl, NCOLS(COLS_DASH));
    for (i = 0; i < n; i++) {
        Train *t = list[i];
        int r = tbl_add(&g_tbl, t->id);
        delay_text(t, d, sizeof d);
        pf_text(t, p, sizeof p);
        tbl_set(&g_tbl, r, 0, "%s", t->number);
        tbl_set(&g_tbl, r, 1, "%s", t->name);
        tbl_set(&g_tbl, r, 2, "%s", util_time_str(t->sched_arrival));
        tbl_set(&g_tbl, r, 3, "%s%s", t->status == TS_AT_PLATFORM ? "dep " : "",
                util_time_str(t->status == TS_AT_PLATFORM ? train_etd(t) : train_eta(t)));
        tbl_set(&g_tbl, r, 4, "%s", d);
        tbl_set(&g_tbl, r, 5, "%s", p);
        tbl_set(&g_tbl, r, 6, "%s", train_status_name(t->status));
    }
    lv_sync(g_app.lv_dash, &g_tbl, g_app.sel_train);
}

static void refresh_events(HWND lv, int max)
{
    int i;
    tbl_reset(&g_tbl, 3);
    for (i = 0; i < max; i++) {
        const LogEntry *e = log_get(&g_app.st.log, i);
        int r;
        if (!e) break;
        r = tbl_add(&g_tbl, (LPARAM)e->level + 1);
        tbl_set(&g_tbl, r, 0, "%s", util_time_str(e->time));
        tbl_set(&g_tbl, r, 1, "%s", log_level_name(e->level));
        tbl_set(&g_tbl, r, 2, "%s", e->msg);
    }
    lv_sync(lv, &g_tbl, LV_KEEP_SELECTION);
}

static void refresh_trains(void)
{
    Train *list[MAX_TRAINS];
    int all = SendMessageA(g_app.chk_departed, BM_GETCHECK, 0, 0) == BST_CHECKED;
    int n = sched_sorted(&g_app.st, list, MAX_TRAINS, all), i;
    char q[64], d[16], p[8];

    GetWindowTextA(g_app.search, q, sizeof q);
    util_trim(q);
    tbl_reset(&g_tbl, NCOLS(COLS_TRAIN));
    for (i = 0; i < n; i++) {
        Train *t = list[i];
        int r;
        if (q[0] && !util_contains_ci(t->number, q) && !util_contains_ci(t->name, q) &&
            !util_contains_ci(t->source, q) && !util_contains_ci(t->destination, q))
            continue;
        r = tbl_add(&g_tbl, t->id);
        delay_text(t, d, sizeof d);
        pf_text(t, p, sizeof p);
        tbl_set(&g_tbl, r, 0, "%s", t->number);
        tbl_set(&g_tbl, r, 1, "%s", t->name);
        tbl_set(&g_tbl, r, 2, "%s", train_status_name(t->status));
        tbl_set(&g_tbl, r, 3, "%s", p);
        tbl_set(&g_tbl, r, 4, "%s", util_time_str(train_eta(t)));
        tbl_set(&g_tbl, r, 5, "%s", util_time_str(train_etd(t)));
        tbl_set(&g_tbl, r, 6, "%s", d);
        tbl_set(&g_tbl, r, 7, "%s", util_time_str(t->sched_arrival));
        tbl_set(&g_tbl, r, 8, "%s", util_time_str(t->sched_departure));
        tbl_set(&g_tbl, r, 9, "%s", train_type_name(t->type));
        tbl_set(&g_tbl, r, 10, "%s", t->source);
        tbl_set(&g_tbl, r, 11, "%s", t->destination);
        tbl_set(&g_tbl, r, 12, "%d", t->coaches);
        tbl_set(&g_tbl, r, 13, "%s", t->delay > 0 ? t->delay_reason : "");
    }
    lv_sync(g_app.lv_trains, &g_tbl, g_app.sel_train);
}

static void refresh_delays(void)
{
    Train *list[MAX_TRAINS];
    int n = 0, i, j;
    char d[16];

    for (i = 0; i < g_app.st.train_count; i++) {
        Train *t = &g_app.st.trains[i];
        if (t->status == TS_CANCELLED) continue;
        if (!train_is_active(t) && t->delay == 0) continue;
        list[n++] = t;
    }
    for (i = 1; i < n; i++) {   /* worst delay first, then by ETA */
        Train *k = list[i];
        for (j = i - 1; j >= 0 && (list[j]->delay < k->delay ||
             (list[j]->delay == k->delay && train_eta(list[j]) > train_eta(k))); j--)
            list[j + 1] = list[j];
        list[j + 1] = k;
    }
    tbl_reset(&g_tbl, NCOLS(COLS_DELAY));
    for (i = 0; i < n; i++) {
        Train *t = list[i];
        int r = tbl_add(&g_tbl, t->id);
        delay_text(t, d, sizeof d);
        tbl_set(&g_tbl, r, 0, "%s", t->number);
        tbl_set(&g_tbl, r, 1, "%s", t->name);
        tbl_set(&g_tbl, r, 2, "%s", util_time_str(t->sched_arrival));
        tbl_set(&g_tbl, r, 3, "%s", util_time_str(train_eta(t)));
        tbl_set(&g_tbl, r, 4, "%s", d);
        tbl_set(&g_tbl, r, 5, "%s", t->delay > 0 ? (t->delay_reason[0] ? t->delay_reason : "Unspecified") : "");
    }
    lv_sync(g_app.lv_delays, &g_tbl, g_app.sel_train);
}

static void update_status_bar(void)
{
    Station *st = &g_app.st;
    char buf[160];
    int i, active = 0, onpf = 0, held = 0, delayed = 0;
    const LogEntry *e;

    for (i = 0; i < st->train_count; i++) {
        const Train *t = &st->trains[i];
        if (!train_is_active(t)) continue;
        active++;
        if (t->status == TS_AT_PLATFORM) onpf++;
        if (t->status == TS_HELD) held++;
        if (t->delay > 0) delayed++;
    }
    if (g_app.running) snprintf(buf, sizeof buf, "  Simulation running at x%d  (F5 to pause)", g_app.speed);
    else               snprintf(buf, sizeof buf, "  Simulation paused  (F5 to run, F6 to step)");
    SendMessageA(g_app.status, SB_SETTEXTA, 0, (LPARAM)buf);

    snprintf(buf, sizeof buf, "  %d active trains   |   %d on platform   |   %d held   |   %d delayed",
             active, onpf, held, delayed);
    SendMessageA(g_app.status, SB_SETTEXTA, 1, (LPARAM)buf);

    e = log_get(&st->log, 0);
    if (e) snprintf(buf, sizeof buf, "  %s  %s", util_time_str(e->time), e->msg);
    else   buf[0] = '\0';
    SendMessageA(g_app.status, SB_SETTEXTA, 2, (LPARAM)buf);
}

static void update_menu(void)
{
    HMENU m = g_app.menu;
    int ids[4] = { IDM_SPEED1, IDM_SPEED2, IDM_SPEED5, IDM_SPEED10 };
    int sp[4] = { 1, 2, 5, 10 }, i;

    CheckMenuItem(m, IDM_AUTOSIG, MF_BYCOMMAND | (g_app.st.auto_signals ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(m, IDM_AUTOPF, MF_BYCOMMAND | (g_app.st.auto_assign ? MF_CHECKED : MF_UNCHECKED));
    for (i = 0; i < 4; i++) {
        CheckMenuItem(m, ids[i], MF_BYCOMMAND | (g_app.speed == sp[i] ? MF_CHECKED : MF_UNCHECKED));
        btn_set_on(g_app.btn_speed[i], g_app.speed == sp[i]);
    }
    ModifyMenuA(m, IDM_RUN, MF_BYCOMMAND | MF_STRING, IDM_RUN,
                g_app.running ? "&Pause simulation\tF5" : "&Run simulation\tF5");
}

void app_refresh(void)
{
    g_app.syncing = 1;
    switch (g_app.page) {
    case PAGE_DASHBOARD: refresh_dash(); refresh_events(g_app.lv_events, 60); break;
    case PAGE_TRAINS:    refresh_trains(); break;
    case PAGE_DELAYS:    refresh_delays(); break;
    case PAGE_LOG:       refresh_events(g_app.lv_log, MAX_LOG_ENTRIES); break;
    default: break;
    }
    g_app.syncing = 0;

    SendMessageA(g_app.chk_autopf, BM_SETCHECK, g_app.st.auto_assign ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageA(g_app.chk_autosig, BM_SETCHECK, g_app.st.auto_signals ? BST_CHECKED : BST_UNCHECKED, 0);

    btn_set_text(g_app.btn_run, g_app.running ? "Pause simulation" : "Run simulation");
    SetPropA(g_app.btn_run, "rn_style", (HANDLE)(INT_PTR)(g_app.running ? BTN_DANGER : BTN_SUCCESS));
    InvalidateRect(g_app.btn_run, NULL, FALSE);

    update_menu();
    update_status_bar();
    InvalidateRect(g_app.hwnd, NULL, FALSE);
}

void app_status(const char *msg)
{
    char buf[200];
    snprintf(buf, sizeof buf, "  %s", msg);
    SendMessageA(g_app.status, SB_SETTEXTA, 2, (LPARAM)buf);
}

/* ------------------------------------------------------------------ */
/* Painting                                                            */
/* ------------------------------------------------------------------ */

static void paint_page(HDC hdc)
{
    Station *st = &g_app.st;
    char v[32], sub[64];
    int i;

    if (g_app.page == PAGE_DASHBOARD || g_app.page == PAGE_DELAYS) {
        int total = st->train_count, active = 0, appr = 0, onpf = 0, held = 0, delayed = 0, sumd = 0;
        int freepf = 0, maint = 0, counted = 0, ontime = 0, lost = 0;
        for (i = 0; i < st->train_count; i++) {
            const Train *t = &st->trains[i];
            if (t->status != TS_CANCELLED) {
                counted++;
                if (t->delay == 0) ontime++;
                lost += t->delay;
            }
            if (!train_is_active(t)) continue;
            active++;
            if (t->status == TS_APPROACHING) appr++;
            if (t->status == TS_AT_PLATFORM) onpf++;
            if (t->status == TS_HELD) held++;
            if (t->delay > 0) { delayed++; sumd += t->delay; }
        }
        for (i = 0; i < st->platform_count; i++) {
            if (st->platforms[i].state == PS_FREE) freepf++;
            if (st->platforms[i].state == PS_MAINTENANCE) maint++;
        }

        if (g_app.page == PAGE_DASHBOARD) {
            const int gap = S(14);
            int cw = (g_app.rc_a.right - g_app.rc_a.left - 5 * gap) / 6;
            RECT c;
#define CARD(idx, label, value, sb, accent) \
            SetRect(&c, g_app.rc_a.left + (idx) * (cw + gap), g_app.rc_a.top, \
                    g_app.rc_a.left + (idx) * (cw + gap) + cw, g_app.rc_a.bottom); \
            paint_stat_card(hdc, &c, label, value, sb, accent)

            snprintf(v, sizeof v, "%d", total);   snprintf(sub, sizeof sub, "%d active now", active);
            CARD(0, "TRAINS TODAY", v, sub, COL_SLATE);
            snprintf(v, sizeof v, "%d", appr);    CARD(1, "APPROACHING", v, "within 15 minutes", COL_CYAN);
            snprintf(v, sizeof v, "%d", onpf);    snprintf(sub, sizeof sub, "of %d platforms", st->platform_count);
            CARD(2, "ON PLATFORM", v, sub, COL_GREEN);
            snprintf(v, sizeof v, "%d", held);    CARD(3, "HELD AT SIGNAL", v, held ? "waiting for a route" : "no trains waiting", held ? COL_RED : COL_GREEN);
            snprintf(v, sizeof v, "%d", delayed); snprintf(sub, sizeof sub, "avg %d min late", delayed ? sumd / delayed : 0);
            CARD(4, "DELAYED", v, sub, delayed ? COL_AMBER : COL_GREEN);
            snprintf(v, sizeof v, "%d / %d", freepf, st->platform_count);
            snprintf(sub, sizeof sub, maint ? "%d under maintenance" : "all in service", maint);
            CARD(5, "FREE PLATFORMS", v, sub, COL_BLUE);

            paint_mimic(hdc, &g_app.rc_b, st);
            paint_card(hdc, &g_app.rc_c, "Incoming & active trains", "right-click a train for actions");
            paint_card(hdc, &g_app.rc_d, "Recent events", NULL);
        } else {
            const int gap = S(14);
            int cw = (g_app.rc_a.right - g_app.rc_a.left - 3 * gap) / 4;
            int pct = counted ? ontime * 100 / counted : 100;
            RECT c;
            snprintf(v, sizeof v, "%d%%", pct);
            CARD(0, "PUNCTUALITY", v, "trains running on time", pct >= 80 ? COL_GREEN : (pct >= 60 ? COL_AMBER : COL_RED));
            snprintf(v, sizeof v, "%d", delayed); CARD(1, "DELAYED NOW", v, "active trains behind schedule", COL_AMBER);
            snprintf(v, sizeof v, "%d min", delayed ? sumd / delayed : 0); CARD(2, "AVERAGE DELAY", v, "of delayed trains", COL_PURPLE);
            snprintf(v, sizeof v, "%d min", lost); CARD(3, "TOTAL MINUTES LOST", v, "across the timetable", COL_RED);
#undef CARD
            paint_card(hdc, &g_app.rc_b, "Trains by delay", "select a train, then use the buttons above");
            paint_delay_bars(hdc, &g_app.rc_c, st);
        }
        return;
    }

    switch (g_app.page) {
    case PAGE_TRAINS:
        snprintf(sub, sizeof sub, "%d trains  \xB7  double-click to edit, right-click for actions",
                 ListView_GetItemCount(g_app.lv_trains));
        paint_card(hdc, &g_app.rc_a, "Timetable", sub);
        break;
    case PAGE_PLATFORMS:
        {
            RECT t;
            SetRect(&t, g_app.rc_content.right - S(360), g_app.rc_content.top + S(18),
                    g_app.rc_content.right - S(20), g_app.rc_content.top + S(52));
            draw_text(hdc, "Click a platform to manage it", &t, g_theme.sm, COL_FAINT,
                      DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        }
        paint_platform_cards(hdc, &g_app.rc_a, st);
        paint_gantt(hdc, &g_app.rc_b, st);
        break;
    case PAGE_SIGNALS:
        {
            RECT t;
            SetRect(&t, g_app.rc_content.right - S(420), g_app.rc_content.top + S(18),
                    g_app.rc_content.right - S(20), g_app.rc_content.top + S(52));
            draw_text(hdc, "Click a signal to change its aspect  \xB7  interlocking is enforced", &t,
                      g_theme.sm, COL_FAINT, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        }
        paint_signal_board(hdc, &g_app.rc_a, st);
        break;
    case PAGE_LOG:
        paint_card(hdc, &g_app.rc_a, "Event log", "most recent first  \xB7  full history in data\\events.log");
        break;
    default:
        break;
    }
}

static void on_paint(HWND h)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(h, &ps);
    RECT cr;
    HDC mem;
    HBITMAP bmp;
    HGDIOBJ old;

    GetClientRect(h, &cr);
    mem = CreateCompatibleDC(hdc);
    bmp = CreateCompatibleBitmap(hdc, cr.right, cr.bottom);
    old = SelectObject(mem, bmp);

    hit_clear();
    draw_fill(mem, &cr, COL_BG);
    paint_header(mem, &g_app.rc_header);
    paint_sidebar(mem, &g_app.rc_sidebar);
    paint_page(mem);

    BitBlt(hdc, 0, 0, cr.right, cr.bottom, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
    EndPaint(h, &ps);
}

/* ------------------------------------------------------------------ */
/* Notifications                                                       */
/* ------------------------------------------------------------------ */

static LRESULT on_custom_draw(NMLVCUSTOMDRAW *cd)
{
    HWND lv = cd->nmcd.hdr.hwndFrom;
    int status_col = -1, delay_col = -1, level_col = -1;

    if (lv == g_app.lv_dash)        { status_col = 6;  delay_col = 4; }
    else if (lv == g_app.lv_trains) { status_col = 2;  delay_col = 6; }
    else if (lv == g_app.lv_delays) { status_col = -1; delay_col = 4; }
    else                            { level_col = 1; }

    switch (cd->nmcd.dwDrawStage) {
    case CDDS_PREPAINT:
        return CDRF_NOTIFYITEMDRAW;
    case CDDS_ITEMPREPAINT:
        return CDRF_NOTIFYSUBITEMDRAW;
    case CDDS_ITEMPREPAINT | CDDS_SUBITEM: {
        int sub = cd->iSubItem;
        LPARAM key = cd->nmcd.lItemlParam;
        HFONT f = g_theme.ui;

        cd->clrText = COL_TEXT;
        cd->clrTextBk = COL_CARD;
        if (level_col >= 0) {
            LogLevel lvl = (LogLevel)(key - 1);
            if (lvl == LOG_ALERT)     cd->clrTextBk = RGBX(0xFEF2F2);
            else if (lvl == LOG_WARN) cd->clrTextBk = RGBX(0xFFFBEB);
            if (sub == 0) cd->clrText = COL_MUTED;
            if (sub == level_col) { cd->clrText = level_color(lvl); f = g_theme.small_bold; }
        } else {
            Train *t = station_find_train(&g_app.st, (int)key);
            if (t) {
                int active = train_is_active(t);
                cd->clrTextBk = status_tint(t->status);
                if (!active) cd->clrText = COL_FAINT;
                if (sub == 0) f = g_theme.ui_bold;
                if (sub == status_col) { cd->clrText = status_color(t->status); f = g_theme.small_bold; }
                if (sub == delay_col && active) {
                    cd->clrText = t->delay >= 30 ? COL_RED : (t->delay > 0 ? COL_AMBER : COL_GREEN);
                    f = g_theme.ui_bold;
                }
            }
        }
        SelectObject(cd->nmcd.hdc, f);
        return CDRF_NEWFONT;
    }
    default:
        return CDRF_DODEFAULT;
    }
}

static LRESULT on_notify(NMHDR *nh)
{
    HWND from = nh->hwndFrom;

    if (from != g_app.lv_dash && from != g_app.lv_trains && from != g_app.lv_delays &&
        from != g_app.lv_events && from != g_app.lv_log)
        return 0;

    if (nh->code == (UINT)NM_CUSTOMDRAW) return on_custom_draw((NMLVCUSTOMDRAW *)nh);
    if (!is_train_list(from)) return 0;

    if (nh->code == LVN_ITEMCHANGED && !g_app.syncing) {
        NMLISTVIEW *nl = (NMLISTVIEW *)nh;
        if ((nl->uNewState & LVIS_SELECTED) && !(nl->uOldState & LVIS_SELECTED)) g_app.sel_train = (int)nl->lParam;
    } else if (nh->code == (UINT)NM_DBLCLK || nh->code == (UINT)NM_RCLICK) {
        NMITEMACTIVATE *ia = (NMITEMACTIVATE *)nh;
        Train *t = station_find_train(&g_app.st, (int)lv_key_at(from, ia->iItem));
        if (!t) return 0;
        g_app.sel_train = t->id;
        if (nh->code == (UINT)NM_DBLCLK) {
            if (t->status == TS_SCHEDULED || t->status == TS_APPROACHING) dlg_train(t);
            else show_train_details(t);
            app_refresh();
        } else {
            POINT pt;
            GetCursorPos(&pt);
            menu_train(t, pt);
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Mouse on painted surfaces                                           */
/* ------------------------------------------------------------------ */

static void on_click(int x, int y, int right)
{
    int kind, id;
    POINT pt;
    if (!hit_test(x, y, &kind, &id)) return;
    pt.x = x;
    pt.y = y;
    ClientToScreen(g_app.hwnd, &pt);

    if (kind == HIT_NAV && !right) {
        app_set_page((Page)id);
    } else if (kind == HIT_SIGNAL && id >= 0 && id < g_app.st.signal_count) {
        menu_signal(&g_app.st.signals[id], pt);
    } else if (kind == HIT_TRAIN) {
        Train *t = station_find_train(&g_app.st, id);
        if (t) {
            g_app.sel_train = t->id;
            menu_train(t, pt);
        }
    } else if (kind == HIT_PLATFORM) {
        Platform *p = station_platform(&g_app.st, id);
        if (p) menu_platform(p, pt);
    }
}

static void set_timer(void)
{
    KillTimer(g_app.hwnd, SIM_TIMER);
    if (g_app.running) SetTimer(g_app.hwnd, SIM_TIMER, (UINT)(1000 / (g_app.speed ? g_app.speed : 1)), NULL);
}

/* ------------------------------------------------------------------ */
/* Window procedure                                                    */
/* ------------------------------------------------------------------ */

static LRESULT CALLBACK wnd_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_CREATE:
        g_app.hwnd = h;
        create_controls(h);
        app_set_page(PAGE_DASHBOARD);
        return 0;

    case WM_SIZE:
        app_layout();
        return 0;

    case WM_GETMINMAXINFO: {
        MINMAXINFO *mm = (MINMAXINFO *)l;
        mm->ptMinTrackSize.x = S(1120);
        mm->ptMinTrackSize.y = S(720);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
        on_paint(h);
        return 0;

    case WM_DRAWITEM:
        btn_draw((const DRAWITEMSTRUCT *)l);
        return TRUE;

    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)w;
        SetBkColor(dc, COL_BG);
        SetTextColor(dc, COL_TEXT);
        return (LRESULT)g_bg_brush;
    }

    case WM_COMMAND: {
        int id = LOWORD(w), code = HIWORD(w);
        if (id == IDC_SEARCH) {
            if (code == EN_CHANGE) app_refresh();
            return 0;
        }
        if (id == IDC_CHK_DEPARTED) { app_refresh(); return 0; }
        if (id == IDC_CHK_AUTOPF)   id = IDM_AUTOPF;
        if (id == IDC_CHK_AUTOSIG)  id = IDM_AUTOSIG;
        cmd_handle(id);
        if (id == IDM_RUN || (id >= IDM_SPEED1 && id <= IDM_SPEED10) || id == IDM_RESET || id == IDM_LOAD)
            set_timer();
        return 0;
    }

    case WM_NOTIFY:
        return on_notify((NMHDR *)l);

    case WM_TIMER:
        if (w == SIM_TIMER) {
            sched_tick(&g_app.st);
            app_refresh();
        }
        return 0;

    case WM_MOUSEMOVE: {
        int kind, id, hover = -1;
        if (hit_test((short)LOWORD(l), (short)HIWORD(l), &kind, &id) && kind == HIT_NAV) hover = id;
        if (hover != g_app.hover_nav) {
            TRACKMOUSEEVENT tme;
            g_app.hover_nav = hover;
            InvalidateRect(h, &g_app.rc_sidebar, FALSE);
            tme.cbSize = sizeof tme;
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = h;
            tme.dwHoverTime = 0;
            TrackMouseEvent(&tme);
        }
        return 0;
    }

    case WM_MOUSELEAVE:
        if (g_app.hover_nav != -1) {
            g_app.hover_nav = -1;
            InvalidateRect(h, &g_app.rc_sidebar, FALSE);
        }
        return 0;

    case WM_SETCURSOR:
        if ((HWND)w == h && LOWORD(l) == HTCLIENT) {
            POINT p;
            int kind, id;
            GetCursorPos(&p);
            ScreenToClient(h, &p);
            if (hit_test(p.x, p.y, &kind, &id)) {
                SetCursor(LoadCursor(NULL, IDC_HAND));
                return TRUE;
            }
        }
        break;

    case WM_LBUTTONUP:
        on_click((short)LOWORD(l), (short)HIWORD(l), 0);
        return 0;

    case WM_RBUTTONUP:
        on_click((short)LOWORD(l), (short)HIWORD(l), 1);
        return 0;

    case WM_CLOSE:
        if (confirm_close()) DestroyWindow(h);
        return 0;

    case WM_DESTROY:
        KillTimer(h, SIM_TIMER);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(h, m, w, l);
}

/* ------------------------------------------------------------------ */
/* Entry                                                               */
/* ------------------------------------------------------------------ */

/* Run from the project folder even when started from build\ by double-click */
static void set_working_dir(void)
{
    char path[MAX_PATH];
    char *slash;
    size_t len;

    if (!GetModuleFileNameA(NULL, path, sizeof path)) return;
    slash = strrchr(path, '\\');
    if (!slash) return;
    *slash = '\0';
    SetCurrentDirectoryA(path);
    len = strlen(path);
    if (len >= 6 && util_equals_ci(path + len - 6, "\\build")) SetCurrentDirectoryA("..");
}

int app_run(HINSTANCE inst, int show)
{
    INITCOMMONCONTROLSEX icc;
    WNDCLASSEXA wc;
    RECT wa;
    MSG msg;
    int w, h;

    icc.dwSize = sizeof icc;
    icc.dwICC = ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES | ICC_DATE_CLASSES | ICC_UPDOWN_CLASS | ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icc);

    set_working_dir();
    util_ensure_dir(DATA_DIR);
    theme_init();

    memset(&g_app, 0, sizeof g_app);
    g_app.inst = inst;
    g_app.speed = 1;
    g_app.hover_nav = -1;
    g_app.page = PAGE_DASHBOARD;
    g_app.icon = (HICON)LoadImageA(inst, MAKEINTRESOURCEA(IDI_APP), IMAGE_ICON, 48, 48, 0);
    g_app.icon_small = (HICON)LoadImageA(inst, MAKEINTRESOURCEA(IDI_APP), IMAGE_ICON,
                                         GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    g_bg_brush = CreateSolidBrush(COL_BG);
    demo_load(&g_app.st);

    memset(&wc, 0, sizeof wc);
    wc.cbSize = sizeof wc;
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = inst;
    wc.hIcon = (HICON)LoadImageA(inst, MAKEINTRESOURCEA(IDI_APP), IMAGE_ICON, 32, 32, 0);
    wc.hIconSm = g_app.icon_small;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = CLASS_NAME;
    RegisterClassExA(&wc);

    SystemParametersInfoA(SPI_GETWORKAREA, 0, &wa, 0);
    w = S(1440);
    h = S(900);
    if (w > wa.right - wa.left) w = wa.right - wa.left;
    if (h > wa.bottom - wa.top) h = wa.bottom - wa.top;

    g_app.menu = build_menu();
    CreateWindowExA(0, CLASS_NAME, APP_NAME " - Railway Platform Management System",
                    WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                    wa.left + (wa.right - wa.left - w) / 2, wa.top + (wa.bottom - wa.top - h) / 2, w, h,
                    NULL, g_app.menu, inst, NULL);
    if (!g_app.hwnd) return 1;

    ShowWindow(g_app.hwnd, (wa.right - wa.left) < S(1500) ? SW_SHOWMAXIMIZED : show);
    UpdateWindow(g_app.hwnd);

    if (util_file_exists(SAVE_FILE) &&
        MessageBoxA(g_app.hwnd, "A saved session was found.\n\nDo you want to restore it?", APP_NAME,
                    MB_YESNO | MB_ICONQUESTION) == IDYES) {
        if (storage_load(&g_app.st, SAVE_FILE))
            log_add(&g_app.st.log, g_app.st.clock, LOG_OK, "Session restored from %s", SAVE_FILE);
        else
            MessageBoxA(g_app.hwnd, "The saved session could not be read.\nThe demo timetable is loaded instead.",
                        APP_NAME, MB_OK | MB_ICONWARNING);
        app_layout();
        app_refresh();
    }

    g_app.accel = LoadAcceleratorsA(inst, MAKEINTRESOURCEA(IDR_ACCEL));
    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        if (!TranslateAcceleratorA(g_app.hwnd, g_app.accel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }

    DeleteObject(g_bg_brush);
    theme_free();
    return (int)msg.wParam;
}
