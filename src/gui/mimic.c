/*
 * mimic.c - Signal-box style station diagram and the signal board
 */
#include <stdio.h>
#include <string.h>

#include "gui/app.h"
#include "gui/theme.h"
#include "gui/widgets.h"
#include "core/scheduler.h"
#include "util/utils.h"

#define DOT "  \xB7  "

static Train *next_for_platform(Station *st, int pf)
{
    Train *best = NULL;
    int i;
    for (i = 0; i < st->train_count; i++) {
        Train *t = &st->trains[i];
        if (!train_is_active(t) || t->platform != pf || t->status == TS_AT_PLATFORM) continue;
        if (!best || train_eta(t) < train_eta(best)) best = t;
    }
    return best;
}

static int signal_index(Station *st, const RailSignal *s)
{
    return s ? (int)(s - st->signals) : -1;
}

/* Horizontal signal head; double yellow shows two lamps */
static RECT draw_head(HDC hdc, int cx, int cy, SignalAspect a)
{
    int r = S(5), pad = S(3);
    int lamps = a == SA_DOUBLE_YELLOW ? 2 : 1;
    int w = lamps * 2 * r + (lamps - 1) * S(3) + 2 * pad;
    RECT h;

    SetRect(&h, cx - w / 2, cy - r - pad, cx - w / 2 + w, cy + r + pad + 1);
    draw_round(hdc, &h, S(4), RGBX(0x020617), RGBX(0x334155));
    if (lamps == 2) {
        draw_circle(hdc, h.left + pad + r, cy, r, COL_LAMP_YELLOW, NO_BORDER);
        draw_circle(hdc, h.right - pad - r - 1, cy, r, COL_LAMP_YELLOW, NO_BORDER);
    } else {
        draw_circle(hdc, cx, cy, r, aspect_color(a), NO_BORDER);
    }
    return h;
}

static void place_signal(HDC hdc, Station *st, RailSignal *s, int cx, int track_y)
{
    RECT h;
    if (!s) return;
    h = draw_head(hdc, cx, track_y, s->aspect);       /* head sits on the line it protects */
    if (s->manual) draw_circle(hdc, h.right, h.top, S(3), RGBX(0xA78BFA), NO_BORDER);  /* override marker */
    InflateRect(&h, S(4), S(4));
    hit_add(&h, HIT_SIGNAL, signal_index(st, s));
}

static void legend_item(HDC hdc, int *x, int y, COLORREF c, const char *label, int box)
{
    RECT t;
    if (box) {
        RECT b;
        SetRect(&b, *x, y - S(5), *x + S(16), y + S(6));
        draw_round(hdc, &b, S(3), c, NO_BORDER);
        *x += S(22);
    } else {
        draw_circle(hdc, *x + S(5), y, S(5), c, NO_BORDER);
        *x += S(15);
    }
    SetRect(&t, *x, y - S(9), *x + S(120), y + S(9));
    draw_text(hdc, label, &t, g_theme.sm, COL_M_FAINT, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    *x += text_width(hdc, label, g_theme.sm) + S(16);
}

void paint_mimic(HDC hdc, const RECT *rc, Station *st)
{
    Train *list[MAX_TRAINS];
    RECT in, t;
    int n = st->platform_count, i, k, count;
    int row_h, cy, jx, x_out, px0, px1, ex;
    int prev_left, held_k = 0;
    const int bw = S(58);

    draw_round(hdc, rc, S(10), COL_M_BG, NO_BORDER);

    SetRect(&t, rc->left + S(18), rc->top + S(10), rc->right, rc->top + S(30));
    draw_text(hdc, "STATION MIMIC PANEL", &t, g_theme.small_bold, COL_M_FAINT, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    /* legend (right aligned: measure first) */
    {
        int x = rc->right - S(520), y = rc->top + S(20);
        legend_item(hdc, &x, y, COL_LAMP_RED, "Stop", 0);
        legend_item(hdc, &x, y, COL_LAMP_YELLOW, "Caution", 0);
        legend_item(hdc, &x, y, COL_LAMP_GREEN, "Proceed", 0);
        legend_item(hdc, &x, y, RGBX(0x0E7490), "Approaching", 1);
        legend_item(hdc, &x, y, RGBX(0xB91C1C), "Held", 1);
        legend_item(hdc, &x, y, RGBX(0x15803D), "On platform", 1);
    }

    if (n <= 0) return;
    SetRect(&in, rc->left + S(18), rc->top + S(44), rc->right - S(18), rc->bottom - S(12));
    row_h = (in.bottom - in.top) / n;
    cy    = in.top + row_h * n / 2;
    jx    = in.left + (in.right - in.left) * 23 / 100;
    x_out = jx - S(36);
    px0   = jx + S(60);
    ex    = in.right - S(60);
    px1   = ex - S(60);

    /* approach and exit lines */
    draw_line(hdc, in.left, cy, jx, cy, COL_M_TRACK, S(3));
    draw_line(hdc, ex, cy, in.right, cy, COL_M_TRACK, S(3));
    SetRect(&t, in.left, cy + S(8), in.left + S(120), cy + S(24));
    draw_text(hdc, "ARRIVALS  >>", &t, g_theme.small_bold, COL_M_FAINT, DT_LEFT | DT_SINGLELINE);
    SetRect(&t, ex + S(6), cy + S(8), in.right, cy + S(24));
    draw_text(hdc, "EXIT >>", &t, g_theme.small_bold, COL_M_FAINT, DT_RIGHT | DT_SINGLELINE);

    /* routes: highlighted when the signal is off (not red) */
    for (i = 0; i < n; i++) {
        int y = in.top + row_h * i + row_h / 2;
        RailSignal *h = station_home_signal(st, i + 1);
        RailSignal *s = station_starter_signal(st, i + 1);
        COLORREF in_c = (h && h->aspect != SA_RED) ? aspect_color(h->aspect) : COL_M_TRACK;
        COLORREF out_c = (s && s->aspect != SA_RED) ? aspect_color(s->aspect) : COL_M_TRACK;
        draw_line(hdc, jx, cy, px0, y, in_c, S(2));
        draw_line(hdc, px1, y, ex, cy, out_c, S(2));
    }
    draw_circle(hdc, jx, cy, S(4), COL_M_TRACK, NO_BORDER);
    draw_circle(hdc, ex, cy, S(4), COL_M_TRACK, NO_BORDER);

    /* platform lines */
    for (i = 0; i < n; i++) {
        Platform *p = &st->platforms[i];
        int y = in.top + row_h * i + row_h / 2;
        int tx0 = px0 + S(72), tx1 = px1 - S(30);
        RECT row, lb;
        char buf[96];

        SetRect(&row, px0, y - row_h / 2, px1, y + row_h / 2);
        hit_add(&row, HIT_PLATFORM, p->number);

        draw_line(hdc, px0, y, px1, y, COL_M_TRACK, S(2));

        SetRect(&lb, px0 + S(30), y - S(10), px0 + S(62), y + S(10));
        draw_round(hdc, &lb, S(4), RGBX(0x1E293B), RGBX(0x334155));
        snprintf(buf, sizeof buf, "P%d", p->number);
        draw_text(hdc, buf, &lb, g_theme.mono_bold, RGB(255, 255, 255), DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        if (p->state == PS_MAINTENANCE) {
            RECT mr, tr;
            int w;
            SetRect(&mr, tx0, y - S(9), tx1, y + S(9));
            draw_hatch(hdc, &mr, RGBX(0xB45309), COL_M_BG);
            w = text_width(hdc, "UNDER MAINTENANCE", g_theme.small_bold) + S(16);
            SetRect(&tr, (tx0 + tx1 - w) / 2, y - S(9), (tx0 + tx1 + w) / 2, y + S(9));
            draw_fill(hdc, &tr, COL_M_BG);
            draw_text(hdc, "UNDER MAINTENANCE", &tr, g_theme.small_bold, COL_LAMP_YELLOW,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        } else if (p->state == PS_OCCUPIED) {
            Train *o = station_find_train(st, p->occupant_id);
            if (o) {
                int len = tx1 - tx0;
                int tw = len * o->coaches / (p->max_coaches ? p->max_coaches : 1);
                int overdue = st->clock >= train_etd(o);
                RECT tr, tt;
                if (tw < S(190)) tw = len < S(190) ? len : S(190);
                SetRect(&tr, tx0, y - S(10), tx0 + tw, y + S(10));
                draw_round(hdc, &tr, S(5), overdue ? RGBX(0xB45309) : RGBX(0x15803D), NO_BORDER);
                snprintf(buf, sizeof buf, "%s  %s", o->number, o->name);
                SetRect(&tt, tr.left + S(8), tr.top, tr.right - S(70), tr.bottom);
                draw_text(hdc, buf, &tt, g_theme.small_bold, RGB(255, 255, 255),
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                snprintf(buf, sizeof buf, "dep %s", util_time_str(train_etd(o)));
                SetRect(&tt, tr.right - S(70), tr.top, tr.right - S(8), tr.bottom);
                draw_text(hdc, buf, &tt, g_theme.sm, RGBX(0xDCFCE7), DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
                hit_add(&tr, HIT_TRAIN, o->id);
            }
        } else {
            Train *nx = next_for_platform(st, p->number);
            if (nx) {
                RECT nt;
                int w;
                snprintf(buf, sizeof buf, "next  %s %s  at %s", nx->number, nx->name, util_time_str(train_eta(nx)));
                w = text_width(hdc, buf, g_theme.sm) + S(20);
                if (w > tx1 - tx0) w = tx1 - tx0;
                SetRect(&nt, tx1 - w, y - S(9), tx1, y + S(9));
                draw_round(hdc, &nt, S(9), COL_M_BG, RGBX(0x1E293B));
                InflateRect(&nt, -S(10), 0);
                draw_text(hdc, buf, &nt, g_theme.sm, COL_M_FAINT, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            }
        }

        place_signal(hdc, st, station_home_signal(st, p->number), px0 + S(10), y);
        place_signal(hdc, st, station_starter_signal(st, p->number), px1 - S(12), y);
    }

    /* outer signal */
    {
        RailSignal *o = station_outer_signal(st);
        RECT lt;
        place_signal(hdc, st, o, x_out, cy);
        SetRect(&lt, x_out - S(30), cy + S(26), x_out + S(30), cy + S(40));
        draw_text(hdc, o ? o->code : "OUT", &lt, g_theme.sm, COL_M_FAINT, DT_CENTER | DT_SINGLELINE);
    }

    /* trains on the approach, closest first */
    count = sched_sorted(st, list, MAX_TRAINS, 0);
    prev_left = x_out - S(14) + S(6);
    for (k = 0; k < count; k++) {
        Train *tr = list[k];
        RECT b, lbl;
        int right, lane = 0;
        char buf[32];

        if (tr->status == TS_HELD) {
            right = x_out - S(14) - held_k * (bw + S(6));
            held_k++;
        } else if (tr->status == TS_APPROACHING) {
            int dist = train_eta(tr) - st->clock;
            int span = x_out - S(14) - in.left - bw;
            if (dist < 0) dist = 0;
            if (dist > APPROACH_WINDOW) dist = APPROACH_WINDOW;
            right = x_out - S(14) - dist * span / APPROACH_WINDOW;
        } else continue;

        if (right > prev_left - S(6)) right = prev_left - S(6);
        if (right - bw < in.left) { right = in.left + bw; lane = 1; }
        prev_left = right - bw;

        SetRect(&b, right - bw, cy - S(9) - lane * S(44), right, cy + S(9) - lane * S(44));
        draw_round(hdc, &b, S(4), tr->status == TS_HELD ? RGBX(0xB91C1C) : RGBX(0x0E7490), NO_BORDER);
        draw_text(hdc, tr->number, &b, g_theme.mono_bold, RGB(255, 255, 255), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if (tr->platform) snprintf(buf, sizeof buf, "P%d %s", tr->platform, util_time_str(train_eta(tr)));
        else              snprintf(buf, sizeof buf, "P? %s", util_time_str(train_eta(tr)));
        if (!lane) {
            SetRect(&lbl, b.left - S(6), cy - S(30), b.right + S(6), cy - S(14));
            draw_text(hdc, buf, &lbl, g_theme.sm, tr->platform ? COL_M_TEXT : COL_LAMP_YELLOW,
                      DT_CENTER | DT_SINGLELINE | DT_VCENTER);
        }
        hit_add(&b, HIT_TRAIN, tr->id);
    }
}

/* ------------------------------------------------------------------ */
/* Signal board                                                        */
/* ------------------------------------------------------------------ */

static COLORREF aspect_text_color(SignalAspect a)
{
    if (a == SA_RED)   return COL_RED;
    if (a == SA_GREEN) return COL_GREEN;
    return COL_AMBER;
}

void paint_signal_board(HDC hdc, const RECT *rc, Station *st)
{
    const int gap = S(14);
    int width = rc->right - rc->left, height = rc->bottom - rc->top;
    int n = st->signal_count;
    int cols = (width + gap) / (S(190) + gap);
    int tile_w, tile_h, rows, i;

    if (cols < 1) cols = 1;
    /* add columns until every signal fits on screen */
    for (;;) {
        rows = (n + cols - 1) / cols;
        tile_h = rows ? (height - gap * (rows - 1)) / rows : height;
        if (tile_h >= S(128) || cols >= n) break;
        cols++;
    }
    if (tile_h > S(160)) tile_h = S(160);
    tile_w = (width - gap * (cols - 1)) / cols;

    for (i = 0; i < st->signal_count; i++) {
        RailSignal *s = &st->signals[i];
        int c = i % cols, r = i / cols;
        RECT tile, house, t;
        int lamp, lx, ly0, pitch, lr;
        /* lamp order top->bottom: yellow, green, red, lower yellow */
        int lit[4] = { 0, 0, 0, 0 };
        COLORREF lamp_on[4] = { COL_LAMP_YELLOW, COL_LAMP_GREEN, COL_LAMP_RED, COL_LAMP_YELLOW };
        char buf[64];
        const char *mode;
        COLORREF mbg, mfg;

        SetRect(&tile, rc->left + c * (tile_w + gap), rc->top + r * (tile_h + gap),
                rc->left + c * (tile_w + gap) + tile_w, rc->top + r * (tile_h + gap) + tile_h);
        if (tile.bottom > rc->bottom) break;

        draw_round(hdc, &tile, S(10), COL_CARD, s->aspect == SA_RED ? COL_BORDER : aspect_color(s->aspect));

        if (s->aspect == SA_YELLOW) lit[0] = 1;
        else if (s->aspect == SA_DOUBLE_YELLOW) { lit[0] = 1; lit[3] = 1; }
        else if (s->aspect == SA_GREEN) lit[1] = 1;
        else lit[2] = 1;

        SetRect(&house, tile.left + S(16), tile.top + S(14), tile.left + S(50), tile.bottom - S(14));
        draw_round(hdc, &house, S(9), RGBX(0x0F172A), NO_BORDER);
        pitch = (house.bottom - house.top) / 4;
        lr = pitch * 36 / 100;
        if (lr > S(10)) lr = S(10);
        lx = (house.left + house.right) / 2;
        ly0 = house.top + pitch / 2;
        for (lamp = 0; lamp < 4; lamp++)
            draw_circle(hdc, lx, ly0 + lamp * pitch, lr, lit[lamp] ? lamp_on[lamp] : COL_LAMP_OFF,
                        lit[lamp] ? NO_BORDER : RGBX(0x334155));

        SetRect(&t, tile.left + S(62), tile.top + S(10), tile.right - S(10), tile.top + S(34));
        draw_text(hdc, s->code, &t, g_theme.h2, COL_TEXT, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        if (s->platform) snprintf(buf, sizeof buf, "%s  \xB7  Platform %d", signal_kind_name(s->kind), s->platform);
        else             snprintf(buf, sizeof buf, "%s  \xB7  Approach", signal_kind_name(s->kind));
        SetRect(&t, tile.left + S(62), tile.top + S(34), tile.right - S(10), tile.top + S(52));
        draw_text(hdc, buf, &t, g_theme.sm, COL_MUTED, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        SetRect(&t, tile.left + S(62), tile.top + S(56), tile.right - S(10), tile.top + S(76));
        draw_text(hdc, signal_aspect_name(s->aspect), &t, g_theme.ui_bold, aspect_text_color(s->aspect),
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SetRect(&t, tile.left + S(62), tile.top + S(76), tile.right - S(10), tile.bottom - S(34));
        draw_text(hdc, signal_aspect_meaning(s->aspect), &t, g_theme.sm, COL_MUTED, DT_LEFT | DT_WORDBREAK | DT_END_ELLIPSIS);

        if (!st->auto_signals)  { mode = "MANUAL";   mbg = COL_AMBER_T;  mfg = COL_AMBER; }
        else if (s->manual)     { mode = "OVERRIDE"; mbg = COL_PURPLE_T; mfg = COL_PURPLE; }
        else                    { mode = "AUTO";     mbg = COL_GREEN_T;  mfg = COL_GREEN; }
        draw_pill(hdc, tile.left + S(62), tile.bottom - S(30), mode, mbg, mfg, g_theme.small_bold);

        hit_add(&tile, HIT_SIGNAL, i);
    }
}
