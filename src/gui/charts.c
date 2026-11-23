/*
 * charts.c - Platform cards, occupancy (Gantt) chart and delay bars
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gui/app.h"
#include "gui/theme.h"
#include "gui/widgets.h"
#include "core/scheduler.h"
#include "util/utils.h"

static Train *next_for_platform(Station *st, int pf, int *queued)
{
    Train *best = NULL;
    int i, q = 0;
    for (i = 0; i < st->train_count; i++) {
        Train *t = &st->trains[i];
        if (!train_is_active(t) || t->platform != pf || t->status == TS_AT_PLATFORM) continue;
        q++;
        if (!best || train_eta(t) < train_eta(best)) best = t;
    }
    if (queued) *queued = q;
    return best;
}

/* ------------------------------------------------------------------ */
/* Platform cards                                                      */
/* ------------------------------------------------------------------ */

static void lamp_label(HDC hdc, int x, int y, const char *label, const RailSignal *s)
{
    RECT t;
    char buf[32];
    if (!s) return;
    draw_circle(hdc, x + S(6), y + S(8), S(6), aspect_color(s->aspect), RGBX(0x0F172A));
    snprintf(buf, sizeof buf, "%s %s", label, s->code);
    SetRect(&t, x + S(18), y, x + S(120), y + S(16));
    draw_text(hdc, buf, &t, g_theme.sm, COL_MUTED, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

void paint_platform_cards(HDC hdc, const RECT *rc, Station *st)
{
    const int gap = S(14);
    int n = st->platform_count, i;
    int width = rc->right - rc->left;
    int cols = width >= S(980) ? 3 : 2;
    int rows = (n + cols - 1) / cols;
    int cw, ch;

    if (rows < 1) return;
    cw = (width - gap * (cols - 1)) / cols;
    ch = (rc->bottom - rc->top - gap * (rows - 1)) / rows;

    for (i = 0; i < n; i++) {
        Platform *p = &st->platforms[i];
        Train *o = station_find_train(st, p->occupant_id);
        int queued = 0;
        Train *nx = next_for_platform(st, p->number, &queued);
        RECT card, bar, t;
        COLORREF sc, sbg;
        char buf[96];
        int x, y, w;

        SetRect(&card, rc->left + (i % cols) * (cw + gap), rc->top + (i / cols) * (ch + gap),
                rc->left + (i % cols) * (cw + gap) + cw, rc->top + (i / cols) * (ch + gap) + ch);

        if (p->state == PS_FREE)          { sc = COL_GREEN; sbg = COL_GREEN_T; }
        else if (p->state == PS_OCCUPIED) { sc = COL_BLUE;  sbg = COL_BLUE_T; }
        else                              { sc = COL_AMBER; sbg = COL_AMBER_T; }

        draw_round(hdc, &card, S(10), COL_CARD, COL_BORDER);
        if (p->state == PS_MAINTENANCE) {
            RECT hz;
            SetRect(&hz, card.left + S(1), card.top + S(1), card.right - S(1), card.top + S(6));
            draw_hatch(hdc, &hz, COL_AMBER, COL_AMBER_T);
        }
        SetRect(&bar, card.left + S(14), card.top + S(16), card.left + S(18), card.bottom - S(16));
        draw_round(hdc, &bar, S(2), sc, NO_BORDER);

        x = card.left + S(30);
        y = card.top + S(12);
        snprintf(buf, sizeof buf, "Platform %d", p->number);
        SetRect(&t, x, y, card.right, y + S(24));
        draw_text(hdc, buf, &t, g_theme.h2, COL_TEXT, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        w = text_width(hdc, buf, g_theme.h2);
        snprintf(buf, sizeof buf, "%d coaches", p->max_coaches);
        SetRect(&t, x + w + S(10), y, card.right, y + S(24));
        draw_text(hdc, buf, &t, g_theme.sm, COL_FAINT, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        w = pill_width(hdc, platform_state_name(p->state), g_theme.small_bold);
        draw_pill(hdc, card.right - S(14) - w, y + S(2), platform_state_name(p->state), sbg, sc, g_theme.small_bold);

        y += S(30);
        SetRect(&t, x, y, x + S(90), y + S(18));
        draw_text(hdc, "On platform", &t, g_theme.sm, COL_MUTED, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        if (o) snprintf(buf, sizeof buf, "%s %s  \xB7  dep %s", o->number, o->name, util_time_str(train_etd(o)));
        else   snprintf(buf, sizeof buf, "%s", p->state == PS_MAINTENANCE ? "Out of service" : "-");
        SetRect(&t, x + S(90), y, card.right - S(14), y + S(18));
        draw_text(hdc, buf, &t, o ? g_theme.ui_bold : g_theme.ui, COL_TEXT, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        y += S(20);
        SetRect(&t, x, y, x + S(90), y + S(18));
        draw_text(hdc, "Next arrival", &t, g_theme.sm, COL_MUTED, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        if (nx) snprintf(buf, sizeof buf, "%s %s  \xB7  %s%s", nx->number, nx->name, util_time_str(train_eta(nx)),
                         queued > 1 ? "  (+more)" : "");
        else    snprintf(buf, sizeof buf, "None booked");
        SetRect(&t, x + S(90), y, card.right - S(14), y + S(18));
        draw_text(hdc, buf, &t, g_theme.ui, nx ? COL_TEXT : COL_FAINT, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        y += S(24);
        lamp_label(hdc, x, y, "Home", station_home_signal(st, p->number));
        lamp_label(hdc, x + S(110), y, "Starter", station_starter_signal(st, p->number));
        snprintf(buf, sizeof buf, "%d booked", queued);
        SetRect(&t, card.right - S(120), y, card.right - S(14), y + S(16));
        draw_text(hdc, buf, &t, g_theme.sm, COL_MUTED, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

        hit_add(&card, HIT_PLATFORM, p->number);
    }
}

/* ------------------------------------------------------------------ */
/* Occupancy (Gantt) chart                                             */
/* ------------------------------------------------------------------ */

void paint_gantt(HDC hdc, const RECT *rc, Station *st)
{
    RECT body, plot, t;
    int n = st->platform_count, i, tm, t0, t1, row_h, x, inset, now_x;
    char buf[32];

    paint_card(hdc, rc, "Platform occupancy", "3.5-hour window  \xB7  click a booking for actions");
    body = card_body(rc);
    SetRect(&plot, body.left + S(74), body.top + S(34), body.right - S(20), body.bottom - S(32));
    if (n <= 0 || plot.bottom <= plot.top) return;

    t0 = st->clock - 30;
    t0 -= ((t0 % 15) + 15) % 15;
    t1 = t0 + 210;
    row_h = (plot.bottom - plot.top) / n;
    inset = row_h / 5 > S(7) ? S(7) : (row_h / 5 < 2 ? 2 : row_h / 5);

#define GX(m) (plot.left + (int)((long)((m) - t0) * (plot.right - plot.left) / (t1 - t0)))

    /* rows */
    for (i = 0; i < n; i++) {
        Platform *p = &st->platforms[i];
        RECT row;
        SetRect(&row, plot.left, plot.top + i * row_h, plot.right, plot.top + (i + 1) * row_h);
        if (i % 2) draw_fill(hdc, &row, COL_ROW_ALT);
        if (p->state == PS_MAINTENANCE) {
            RECT m = row;
            InflateRect(&m, 0, -S(4));
            draw_hatch(hdc, &m, RGBX(0xF59E0B), COL_AMBER_T);
        }
        snprintf(buf, sizeof buf, "P%d", p->number);
        SetRect(&t, body.left + S(16), row.top, body.left + S(44), row.bottom);
        draw_text(hdc, buf, &t, g_theme.ui_bold, COL_TEXT, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        snprintf(buf, sizeof buf, "%dc", p->max_coaches);
        SetRect(&t, body.left + S(42), row.top, plot.left - S(6), row.bottom);
        draw_text(hdc, buf, &t, g_theme.sm, COL_FAINT, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        hit_add(&row, HIT_PLATFORM, p->number);
    }

    now_x = GX(st->clock);
    /* grid + time labels */
    for (tm = t0; tm <= t1; tm += 15) {
        x = GX(tm);
        draw_line(hdc, x, plot.top, x, plot.bottom, tm % 60 == 0 ? RGBX(0xD5DDE8) : COL_GRID, 1);
        if (tm % 30 == 0 && abs(x - now_x) > S(44)) {
            SetRect(&t, x - S(24), plot.top - S(24), x + S(24), plot.top - S(6));
            draw_text(hdc, util_time_str(tm), &t, g_theme.sm, COL_MUTED, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
    }
    draw_line(hdc, plot.left, plot.bottom, plot.right, plot.bottom, COL_BORDER, 1);

    /* bookings */
    for (i = 0; i < st->train_count; i++) {
        Train *tr = &st->trains[i];
        int a, b, row;
        RECT br;
        if (tr->status == TS_CANCELLED || tr->platform < 1 || tr->platform > n) continue;
        a = train_eta(tr);
        b = train_etd(tr);
        if (tr->status == TS_AT_PLATFORM && b < st->clock) b = st->clock;
        if (b <= t0 || a >= t1) continue;
        row = tr->platform - 1;

        if (tr->delay > 0 && tr->sched_departure > t0 && tr->sched_arrival < t1) {
            RECT gr;
            SetRect(&gr, GX(tr->sched_arrival > t0 ? tr->sched_arrival : t0), plot.top + row * row_h + inset - 1,
                    GX(tr->sched_departure < t1 ? tr->sched_departure : t1), plot.top + (row + 1) * row_h - inset + 1);
            draw_round_outline(hdc, &gr, S(4), COL_FAINT, 1);
        }

        SetRect(&br, GX(a > t0 ? a : t0), plot.top + row * row_h + inset,
                GX(b < t1 ? b : t1), plot.top + (row + 1) * row_h - inset);
        if (br.right - br.left < S(6)) br.right = br.left + S(6);
        draw_round(hdc, &br, S(4), tr->status == TS_DEPARTED ? RGBX(0xCBD5E1) : type_color(tr->type), NO_BORDER);
        if (tr->status == TS_HELD) draw_round_outline(hdc, &br, S(4), COL_RED, S(2));
        if (br.right - br.left > S(46)) {
            RECT tt = br;
            char lbl[64];
            if (br.right - br.left > S(150)) snprintf(lbl, sizeof lbl, "%s  %s", tr->number, tr->name);
            else                             snprintf(lbl, sizeof lbl, "%s", tr->number);
            InflateRect(&tt, -S(6), 0);
            draw_text(hdc, lbl, &tt, g_theme.small_bold, RGB(255, 255, 255),
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
        hit_add(&br, HIT_TRAIN, tr->id);
    }

    /* now line */
    x = GX(st->clock);
    draw_line(hdc, x, plot.top - S(2), x, plot.bottom, COL_RED, S(2));
    snprintf(buf, sizeof buf, "NOW %s", util_time_str(st->clock));
    {
        int w = pill_width(hdc, buf, g_theme.small_bold);
        draw_pill(hdc, x - w / 2, plot.top - S(26), buf, COL_RED, RGB(255, 255, 255), g_theme.small_bold);
    }

    /* legend */
    {
        int lx = plot.left, ly = plot.bottom + S(10), k;
        for (k = 0; k < TT_COUNT; k++) {
            RECT sw;
            SetRect(&sw, lx, ly + S(2), lx + S(12), ly + S(14));
            draw_round(hdc, &sw, S(3), type_color((TrainType)k), NO_BORDER);
            SetRect(&t, lx + S(18), ly, lx + S(120), ly + S(16));
            draw_text(hdc, train_type_name((TrainType)k), &t, g_theme.sm, COL_MUTED, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            lx += S(26) + text_width(hdc, train_type_name((TrainType)k), g_theme.sm) + S(8);
        }
        SetRect(&t, lx + S(10), ly, plot.right, ly + S(16));
        draw_text(hdc, "Outline = original slot of a delayed train", &t, g_theme.sm, COL_FAINT,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
#undef GX
}

/* ------------------------------------------------------------------ */
/* Delay bars                                                          */
/* ------------------------------------------------------------------ */

void paint_delay_bars(HDC hdc, const RECT *rc, Station *st)
{
    Train *list[MAX_TRAINS];
    RECT body, t;
    int n = 0, i, j, maxd, y, row_h = S(38);

    paint_card(hdc, rc, "Delay by train", "minutes behind schedule");
    body = card_body(rc);

    for (i = 0; i < st->train_count; i++) {
        Train *tr = &st->trains[i];
        if (tr->status != TS_CANCELLED && tr->delay > 0) list[n++] = tr;
    }
    for (i = 1; i < n; i++) {
        Train *k = list[i];
        for (j = i - 1; j >= 0 && list[j]->delay < k->delay; j--) list[j + 1] = list[j];
        list[j + 1] = k;
    }
    if (n == 0) {
        draw_text(hdc, "All trains are running on time", &body, g_theme.h2, COL_GREEN,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }

    maxd = list[0]->delay;
    y = body.top + S(12);
    for (i = 0; i < n && y + row_h <= body.bottom; i++) {
        Train *tr = list[i];
        int x0 = body.left + S(80);
        int span = body.right - S(20) - x0 - S(150);
        int len = span * tr->delay / (maxd ? maxd : 1);
        COLORREF c = tr->delay >= 30 ? COL_RED : COL_AMBER;
        RECT bar, row;
        char buf[64];

        if (len < S(4)) len = S(4);
        SetRect(&t, body.left + S(16), y, x0 - S(6), y + S(20));
        draw_text(hdc, tr->number, &t, g_theme.mono_bold, train_is_active(tr) ? COL_TEXT : COL_FAINT,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SetRect(&bar, x0, y + S(3), x0 + len, y + S(17));
        draw_round(hdc, &bar, S(4), train_is_active(tr) ? c : RGBX(0xCBD5E1), NO_BORDER);
        snprintf(buf, sizeof buf, "+%d min", tr->delay);
        SetRect(&t, bar.right + S(8), y, body.right - S(10), y + S(20));
        draw_text(hdc, buf, &t, g_theme.ui_bold, c, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SetRect(&t, x0, y + S(19), body.right - S(10), y + S(35));
        snprintf(buf, sizeof buf, "%s  \xB7  %s", tr->name, tr->delay_reason[0] ? tr->delay_reason : "no reason given");
        draw_text(hdc, buf, &t, g_theme.sm, COL_FAINT, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        SetRect(&row, body.left, y, body.right, y + row_h);
        hit_add(&row, HIT_TRAIN, tr->id);
        y += row_h;
    }
}
