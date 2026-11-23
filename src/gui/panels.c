/*
 * panels.c - Window chrome: header bar, sidebar navigation, cards
 */
#include <stdio.h>
#include <string.h>

#include "gui/app.h"
#include "gui/theme.h"
#include "gui/widgets.h"
#include "util/utils.h"

static const char *NAV_LABELS[PAGE_COUNT] = {
    "Dashboard", "Trains", "Platforms", "Signals", "Delays", "Event Log"
};

/* ------------------------------------------------------------------ */
/* Header                                                              */
/* ------------------------------------------------------------------ */

void paint_header(HDC hdc, const RECT *r)
{
    Station *st = &g_app.st;
    RECT t;
    int x, y, right;
    char buf[64];

    draw_fill(hdc, r, COL_HEADER);

    /* logo + product name */
    x = r->left + S(18);
    y = r->top + (r->bottom - r->top - S(34)) / 2;
    if (g_app.icon) DrawIconEx(hdc, x, y, g_app.icon, S(34), S(34), 0, NULL, DI_NORMAL);
    x += S(46);

    SetRect(&t, x, r->top + S(10), x + S(300), r->top + S(36));
    draw_text(hdc, "RailNet PMS", &t, g_theme.title, RGB(255, 255, 255), DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    SetRect(&t, x, r->top + S(36), x + S(300), r->bottom - S(8));
    draw_text(hdc, "Railway Platform Management System", &t, g_theme.sm, COL_HEADER_SUB,
              DT_LEFT | DT_SINGLELINE | DT_TOP);

    /* station */
    x += S(250);
    draw_line(hdc, x, r->top + S(14), x, r->bottom - S(14), RGBX(0x334155), 1);
    x += S(20);
    SetRect(&t, x, r->top + S(10), x + S(320), r->top + S(36));
    draw_text(hdc, st->name, &t, g_theme.h2, RGB(255, 255, 255), DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    snprintf(buf, sizeof buf, "Station code %s  \xB7  %d platforms  \xB7  %d trains", st->code,
             st->platform_count, st->train_count);
    SetRect(&t, x, r->top + S(36), x + S(360), r->bottom - S(8));
    draw_text(hdc, buf, &t, g_theme.sm, COL_HEADER_SUB, DT_LEFT | DT_SINGLELINE | DT_TOP);

    /* clock (right) */
    right = r->right - S(20);
    {
        const char *clk = util_time_str(st->clock);
        int w = text_width(hdc, clk, g_theme.clock);
        SetRect(&t, right - w, r->top + S(4), right, r->top + S(42));
        draw_text(hdc, clk, &t, g_theme.clock, RGB(255, 255, 255), DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
        SetRect(&t, right - S(120), r->top + S(40), right, r->bottom - S(6));
        draw_text(hdc, "STATION TIME", &t, g_theme.small_bold, COL_HEADER_SUB, DT_RIGHT | DT_SINGLELINE | DT_TOP);
        right -= (w > S(90) ? w : S(90)) + S(24);
    }

    /* status pills, right to left */
    y = r->top + (r->bottom - r->top - S(20)) / 2;
    {
        const char *pf = st->auto_assign ? "AUTO-PF ON" : "AUTO-PF OFF";
        const char *sg = st->auto_signals ? "SIGNALS AUTO" : "SIGNALS MANUAL";
        char sim[32];
        int w;

        if (g_app.running) snprintf(sim, sizeof sim, "   LIVE  x%d", g_app.speed);
        else               snprintf(sim, sizeof sim, "PAUSED");

        w = pill_width(hdc, pf, g_theme.small_bold);
        right -= w;
        draw_pill(hdc, right, y, pf, st->auto_assign ? RGBX(0x14532D) : RGBX(0x78350F),
                  st->auto_assign ? RGBX(0x86EFAC) : RGBX(0xFCD34D), g_theme.small_bold);
        right -= S(8);

        w = pill_width(hdc, sg, g_theme.small_bold);
        right -= w;
        draw_pill(hdc, right, y, sg, st->auto_signals ? RGBX(0x1E3A8A) : RGBX(0x78350F),
                  st->auto_signals ? RGBX(0x93C5FD) : RGBX(0xFCD34D), g_theme.small_bold);
        right -= S(8);

        w = pill_width(hdc, sim, g_theme.small_bold);
        right -= w;
        draw_pill(hdc, right, y, sim, g_app.running ? RGBX(0x7F1D1D) : RGBX(0x334155),
                  g_app.running ? RGBX(0xFCA5A5) : RGBX(0xCBD5E1), g_theme.small_bold);
        if (g_app.running) draw_circle(hdc, right + S(13), y + S(10), S(3), RGBX(0xF87171), NO_BORDER);
    }
}

/* ------------------------------------------------------------------ */
/* Sidebar                                                             */
/* ------------------------------------------------------------------ */

static void nav_icon(HDC hdc, int page, int x, int y, COLORREF c)
{
    RECT r;
    int s = S(16);
    switch (page) {
    case PAGE_DASHBOARD:
        SetRect(&r, x, y, x + S(7), y + S(7));               draw_round(hdc, &r, S(1), c, NO_BORDER);
        SetRect(&r, x + S(9), y, x + s, y + S(7));           draw_round(hdc, &r, S(1), c, NO_BORDER);
        SetRect(&r, x, y + S(9), x + S(7), y + s);           draw_round(hdc, &r, S(1), c, NO_BORDER);
        SetRect(&r, x + S(9), y + S(9), x + s, y + s);       draw_round(hdc, &r, S(1), c, NO_BORDER);
        break;
    case PAGE_TRAINS:
        SetRect(&r, x, y + S(1), x + s, y + S(12));          draw_round(hdc, &r, S(3), c, NO_BORDER);
        SetRect(&r, x + S(3), y + S(3), x + S(13), y + S(7)); draw_fill(hdc, &r, COL_SIDEBAR);
        draw_circle(hdc, x + S(4), y + S(14), S(2), c, NO_BORDER);
        draw_circle(hdc, x + S(12), y + S(14), S(2), c, NO_BORDER);
        break;
    case PAGE_PLATFORMS:
        draw_line(hdc, x, y + S(3), x + s, y + S(3), c, S(2));
        draw_line(hdc, x, y + S(8), x + s, y + S(8), c, S(2));
        draw_line(hdc, x, y + S(13), x + s, y + S(13), c, S(2));
        break;
    case PAGE_SIGNALS:
        SetRect(&r, x + S(4), y, x + S(12), y + s);          draw_round(hdc, &r, S(3), c, NO_BORDER);
        draw_circle(hdc, x + S(8), y + S(4), S(2), COL_SIDEBAR, NO_BORDER);
        draw_circle(hdc, x + S(8), y + S(11), S(2), COL_SIDEBAR, NO_BORDER);
        break;
    case PAGE_DELAYS:
        draw_circle(hdc, x + S(8), y + S(8), S(8), c, NO_BORDER);
        draw_circle(hdc, x + S(8), y + S(8), S(6), COL_SIDEBAR, NO_BORDER);
        draw_line(hdc, x + S(8), y + S(8), x + S(8), y + S(4), c, S(2));
        draw_line(hdc, x + S(8), y + S(8), x + S(11), y + S(10), c, S(2));
        break;
    default:
        draw_line(hdc, x, y + S(2), x + s, y + S(2), c, S(2));
        draw_line(hdc, x, y + S(7), x + S(12), y + S(7), c, S(2));
        draw_line(hdc, x, y + S(12), x + s, y + S(12), c, S(2));
        break;
    }
}

void paint_sidebar(HDC hdc, const RECT *r)
{
    Station *st = &g_app.st;
    RECT t;
    int i, y, held = 0, delayed = 0, overrides = 0, active = 0;

    for (i = 0; i < st->train_count; i++) {
        if (st->trains[i].status == TS_HELD) held++;
        if (train_is_active(&st->trains[i])) {
            active++;
            if (st->trains[i].delay > 0) delayed++;
        }
    }
    for (i = 0; i < st->signal_count; i++)
        if (st->signals[i].manual) overrides++;

    draw_fill(hdc, r, COL_SIDEBAR);

    SetRect(&t, r->left + S(20), r->top + S(16), r->right, r->top + S(34));
    draw_text(hdc, "CONTROL ROOM", &t, g_theme.small_bold, RGBX(0x64748B), DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    y = r->top + S(40);
    for (i = 0; i < PAGE_COUNT; i++) {
        RECT item, lbl;
        int is_active = (int)g_app.page == i;
        COLORREF fg = is_active ? RGB(255, 255, 255) : COL_NAV_TEXT;
        char badge[16] = "";
        COLORREF bbg = COL_NAV_ACTIVE, bfg = COL_NAV_TEXT;

        SetRect(&item, r->left + S(10), y, r->right - S(10), y + S(40));
        if (is_active)                 draw_round(hdc, &item, S(8), COL_NAV_ACTIVE, NO_BORDER);
        else if (g_app.hover_nav == i) draw_round(hdc, &item, S(8), COL_NAV_HOVER, NO_BORDER);
        if (is_active) {
            RECT bar;
            SetRect(&bar, item.left, item.top + S(10), item.left + S(3), item.bottom - S(10));
            draw_fill(hdc, &bar, COL_ACCENT);
        }
        nav_icon(hdc, i, item.left + S(14), item.top + S(12), is_active ? COL_ACCENT : RGBX(0x94A3B8));

        SetRect(&lbl, item.left + S(42), item.top, item.right - S(8), item.bottom);
        draw_text(hdc, NAV_LABELS[i], &lbl, is_active ? g_theme.ui_bold : g_theme.ui, fg,
                  DT_LEFT | DT_SINGLELINE | DT_VCENTER);

        if (i == PAGE_DASHBOARD && held)   { snprintf(badge, sizeof badge, "%d held", held); bbg = RGBX(0x7F1D1D); bfg = RGBX(0xFCA5A5); }
        if (i == PAGE_TRAINS)              { snprintf(badge, sizeof badge, "%d", active); }
        if (i == PAGE_DELAYS && delayed)   { snprintf(badge, sizeof badge, "%d", delayed); bbg = RGBX(0x78350F); bfg = RGBX(0xFCD34D); }
        if (i == PAGE_SIGNALS && overrides){ snprintf(badge, sizeof badge, "%d manual", overrides); bbg = RGBX(0x4C1D95); bfg = RGBX(0xC4B5FD); }
        if (badge[0]) {
            int w = pill_width(hdc, badge, g_theme.small_bold);
            draw_pill(hdc, item.right - S(10) - w, item.top + S(10), badge, bbg, bfg, g_theme.small_bold);
        }

        hit_add(&item, HIT_NAV, i);
        y += S(44);
    }

    /* simulation panel label */
    SetRect(&t, g_app.rc_sim.left + S(10), g_app.rc_sim.top, g_app.rc_sim.right, g_app.rc_sim.top + S(22));
    draw_text(hdc, "SIMULATION", &t, g_theme.small_bold, RGBX(0x64748B), DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    draw_line(hdc, r->left + S(20), g_app.rc_sim.top - S(14), r->right - S(20), g_app.rc_sim.top - S(14),
              RGBX(0x334155), 1);
    SetRect(&t, g_app.rc_sim.left + S(10), g_app.rc_sim.top + S(72), g_app.rc_sim.right, g_app.rc_sim.top + S(92));
    draw_text(hdc, "Speed (station minutes / second)", &t, g_theme.sm, RGBX(0x64748B),
              DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    SetRect(&t, g_app.rc_sim.left + S(10), g_app.rc_sim.top + S(130), g_app.rc_sim.right, g_app.rc_sim.top + S(150));
    draw_text(hdc, "Step the clock", &t, g_theme.sm, RGBX(0x64748B), DT_LEFT | DT_SINGLELINE | DT_VCENTER);
}

/* ------------------------------------------------------------------ */
/* Cards                                                               */
/* ------------------------------------------------------------------ */

void paint_stat_card(HDC hdc, const RECT *r, const char *label, const char *value,
                     const char *sub, COLORREF accent)
{
    RECT t, bar;

    draw_round(hdc, r, S(10), COL_CARD, COL_BORDER);
    SetRect(&bar, r->left + S(14), r->top + S(14), r->left + S(18), r->bottom - S(14));
    draw_round(hdc, &bar, S(2), accent, NO_BORDER);

    SetRect(&t, r->left + S(28), r->top + S(10), r->right - S(10), r->top + S(26));
    draw_text(hdc, label, &t, g_theme.small_bold, COL_MUTED, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    SetRect(&t, r->left + S(28), r->top + S(24), r->right - S(10), r->top + S(56));
    draw_text(hdc, value, &t, g_theme.big, COL_TEXT, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    if (sub) {
        SetRect(&t, r->left + S(28), r->top + S(55), r->right - S(10), r->bottom - S(4));
        draw_text(hdc, sub, &t, g_theme.sm, COL_FAINT, DT_LEFT | DT_SINGLELINE | DT_TOP | DT_END_ELLIPSIS);
    }
}

void paint_card(HDC hdc, const RECT *r, const char *title, const char *subtitle)
{
    RECT t;
    draw_round(hdc, r, S(10), COL_CARD, COL_BORDER);
    SetRect(&t, r->left + S(16), r->top + S(10), r->right - S(16), r->top + S(34));
    draw_text(hdc, title, &t, g_theme.h2, COL_TEXT, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    if (subtitle) {
        int w = text_width(hdc, title, g_theme.h2);
        SetRect(&t, r->left + S(16) + w + S(10), r->top + S(10), r->right - S(16), r->top + S(34));
        draw_text(hdc, subtitle, &t, g_theme.sm, COL_FAINT, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    }
    draw_line(hdc, r->left + 1, r->top + S(42), r->right - 1, r->top + S(42), COL_BORDER, 1);
}

RECT card_body(const RECT *card)
{
    RECT b;
    SetRect(&b, card->left + S(2), card->top + S(43), card->right - S(2), card->bottom - S(8));
    return b;
}
