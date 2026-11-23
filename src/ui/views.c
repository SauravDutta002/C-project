/*
 * views.c - Screens and widgets
 */
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "ui/ui.h"
#include "ui/views.h"
#include "core/scheduler.h"
#include "util/utils.h"

#define C(x)      col(CLR_##x)
#define TRACK_LEN 44

/* ------------------------------------------------------------------ */
/* Widgets                                                             */
/* ------------------------------------------------------------------ */

UiColor view_status_color(TrainStatus s)
{
    switch (s) {
    case TS_SCHEDULED:   return CLR_WHITE;
    case TS_APPROACHING: return CLR_CYAN;
    case TS_HELD:        return CLR_RED;
    case TS_AT_PLATFORM: return CLR_GREEN;
    case TS_DEPARTED:    return CLR_GRAY;
    case TS_CANCELLED:   return CLR_MAGENTA;
    default:             return CLR_WHITE;
    }
}

UiColor view_aspect_color(SignalAspect a)
{
    if (a == SA_RED)   return CLR_RED;
    if (a == SA_GREEN) return CLR_GREEN;
    return CLR_YELLOW;
}

/* Signal lamp, always 2 columns wide */
const char *view_lamp(SignalAspect a)
{
    static char ring[8][32];
    static int k = 0;
    char *b = ring[k];
    k = (k + 1) % 8;
    snprintf(b, 32, "%s%s%s%s", C(BOLD), col(view_aspect_color(a)),
             a == SA_DOUBLE_YELLOW ? "●●" : "● ", C(RESET));
    return b;
}

static UiColor level_color(LogLevel l)
{
    switch (l) {
    case LOG_OK:    return CLR_GREEN;
    case LOG_WARN:  return CLR_YELLOW;
    case LOG_ALERT: return CLR_RED;
    default:        return CLR_CYAN;
    }
}

/* ------------------------------------------------------------------ */
/* Splash & header                                                     */
/* ------------------------------------------------------------------ */

void view_splash(void)
{
    static const char *art[] = {
        "     ______________    _____________________      _____________________",
        "    |  RAILNET  |o|   |  []  []  []  []  [] |    |  []  []  []  []  [] |",
        "    |___________|_|===|_____________________|====|_____________________|",
        "      (O)    (O)        (O)             (O)        (O)             (O)"
    };
    char left[128], right[64];
    int i;

    ui_clear();
    printf("\n\n");
    for (i = 0; i < 4; i++)
        printf("   %s%s%s%s\n", C(BOLD), i == 1 ? C(YELLOW) : C(CYAN), art[i], C(RESET));
    printf("   %s", C(GRAY));
    ui_repeat("═", 76);
    printf("%s\n\n", C(RESET));

    snprintf(left, sizeof left, "  %s%s◆ RAILNET PMS%s  -  Railway Platform Management System",
             C(BOLD), C(YELLOW), C(RESET));
    snprintf(right, sizeof right, "%sv%s%s  ", C(GRAY), APP_VERSION, C(RESET));
    ui_banner(left, right);

    printf("\n     %sTrain tracking  ·  Platform allocation  ·  Delay control  ·  Signal interlocking%s\n",
           C(WHITE), C(RESET));
    printf("     %sLive simulation  ·  Occupancy charts  ·  Event log  ·  Save / restore — written in pure C%s\n\n",
           C(GRAY), C(RESET));
}

void view_header(Station *st, const char *subtitle)
{
    char left[192], right[192];

    snprintf(left, sizeof left, "  %s%s◆ RAILNET PMS%s  %s│%s  %s%s%s (%s)",
             C(BOLD), C(YELLOW), C(RESET), C(GRAY), C(RESET),
             C(BOLD), st->name, C(RESET), st->code);

    snprintf(right, sizeof right,
             "%sCLOCK%s %s%s%s%s  %s│%s  %sSIGNALS%s %s%s%s  %s│%s  %sAUTO-PF%s %s%s%s  ",
             C(GRAY), C(RESET), C(BOLD), C(GREEN), util_time_str(st->clock), C(RESET),
             C(GRAY), C(RESET),
             C(GRAY), C(RESET), st->auto_signals ? C(GREEN) : C(YELLOW),
             st->auto_signals ? "AUTO" : "MANUAL", C(RESET),
             C(GRAY), C(RESET),
             C(GRAY), C(RESET), st->auto_assign ? C(GREEN) : C(YELLOW),
             st->auto_assign ? "ON" : "OFF", C(RESET));

    ui_banner(left, right);
    ui_subtitle(subtitle);
}

static void stat_item(const char *label, int value, UiColor c)
{
    printf("%s%s%s %s%s%d%s   ", C(GRAY), label, C(RESET), C(BOLD), col(c), value, C(RESET));
}

void view_stats(Station *st)
{
    int i, total = st->train_count, appr = 0, onpf = 0, held = 0, delayed = 0, departed = 0, freepf = 0;

    for (i = 0; i < st->train_count; i++) {
        const Train *t = &st->trains[i];
        if (t->status == TS_APPROACHING) appr++;
        if (t->status == TS_AT_PLATFORM) onpf++;
        if (t->status == TS_HELD)        held++;
        if (t->status == TS_DEPARTED)    departed++;
        if (train_is_active(t) && t->delay > 0) delayed++;
    }
    for (i = 0; i < st->platform_count; i++)
        if (st->platforms[i].state == PS_FREE) freepf++;

    printf("  ");
    stat_item("TRAINS", total, CLR_WHITE);
    stat_item("APPROACHING", appr, CLR_CYAN);
    stat_item("ON PLATFORM", onpf, CLR_GREEN);
    stat_item("HELD", held, held ? CLR_RED : CLR_GREEN);
    stat_item("DELAYED", delayed, delayed ? CLR_YELLOW : CLR_GREEN);
    stat_item("DEPARTED", departed, CLR_GRAY);
    printf("%sFREE PF%s %s%s%d/%d%s\n", C(GRAY), C(RESET), C(BOLD),
           freepf ? C(GREEN) : C(RED), freepf, st->platform_count, C(RESET));
}

/* ------------------------------------------------------------------ */
/* Train tables                                                        */
/* ------------------------------------------------------------------ */

void view_train_header(void)
{
    printf("%s %-7s %-19s %-10s %-10s %-4s %-5s %-5s %-5s %-7s %-3s %-12s %s\n", C(HEADER),
           "TRAIN", "NAME", "FROM", "TO", "TYPE", "SCH", "ETA", "ETD", "DELAY", "PF", "STATUS", C(RESET));
}

void view_train_row(const Train *t)
{
    char delay[16], pf[8];
    int active = train_is_active(t);
    const char *dim = active ? "" : C(GRAY);
    const char *eta_c, *delay_c, *pf_c;

    if (t->status == TS_CANCELLED) snprintf(delay, sizeof delay, "--");
    else if (t->delay > 0)         snprintf(delay, sizeof delay, "+%dm", t->delay);
    else                           snprintf(delay, sizeof delay, "ON TIME");

    if (t->platform) snprintf(pf, sizeof pf, "P%d", t->platform);
    else             snprintf(pf, sizeof pf, "--");

    eta_c   = !active ? C(GRAY) : (t->delay > 0 ? C(YELLOW) : C(GREEN));
    delay_c = !active ? C(GRAY) : (t->delay >= 30 ? C(RED) : (t->delay > 0 ? C(YELLOW) : C(GREEN)));
    pf_c    = !active ? C(GRAY) : (t->platform ? C(CYAN) : C(YELLOW));

    printf(" %s%s%-7s%s ", dim, active ? C(BOLD) : "", t->number, C(RESET));
    printf("%s%-19.19s %-10.10s %-10.10s%s ", dim, t->name, t->source, t->destination, C(RESET));
    printf("%s%-4s%s ", C(GRAY), train_type_code(t->type), C(RESET));
    printf("%s%-5s%s ", dim, util_time_str(t->sched_arrival), C(RESET));
    printf("%s%-5s %-5s%s ", eta_c, util_time_str(train_eta(t)), util_time_str(train_etd(t)), C(RESET));
    printf("%s%-7s%s ", delay_c, delay, C(RESET));
    printf("%s%s%-3s%s ", C(BOLD), pf_c, pf, C(RESET));
    printf("%s%s%-12s%s\n", t->status == TS_HELD ? C(BOLD) : "", col(view_status_color(t->status)),
           train_status_name(t->status), C(RESET));
}

int view_train_list(Station *st, int include_inactive, int limit)
{
    Train *list[MAX_TRAINS];
    int n = sched_sorted(st, list, MAX_TRAINS, include_inactive);
    int i;

    view_train_header();
    if (n == 0) printf("   %sNo trains to show.%s\n", C(GRAY), C(RESET));
    for (i = 0; i < n && (limit <= 0 || i < limit); i++) view_train_row(list[i]);
    if (limit > 0 && n > limit)
        printf("   %s... and %d more train(s)%s\n", C(GRAY), n - limit, C(RESET));
    return n;
}

void view_train_details(Station *st, const Train *t)
{
    UiColor sc = view_status_color(t->status);
    const Platform *p = t->platform ? station_platform(st, t->platform) : NULL;

    ui_section("TRAIN DETAILS");
    printf("\n    %s%s%s%s  %s%s%s   %s[%s]%s\n\n", C(BOLD), C(YELLOW), t->number, C(RESET),
           C(BOLD), t->name, C(RESET), C(CYAN), train_type_name(t->type), C(RESET));
    printf("    %-14s %s  →  %s\n", "Route", t->source, t->destination);
    printf("    %-14s %d coaches\n", "Formation", t->coaches);
    printf("    %-14s arr %s   dep %s   (halt %d min)\n", "Scheduled",
           util_time_str(t->sched_arrival), util_time_str(t->sched_departure), train_halt(t));
    printf("    %-14s %sarr %s   dep %s%s\n", "Expected", t->delay ? C(YELLOW) : C(GREEN),
           util_time_str(train_eta(t)), util_time_str(train_etd(t)), C(RESET));
    if (t->delay > 0)
        printf("    %-14s %s+%d min%s  %s(%s)%s\n", "Delay", C(RED), t->delay, C(RESET),
               C(GRAY), t->delay_reason[0] ? t->delay_reason : "no reason given", C(RESET));
    else
        printf("    %-14s %sON TIME%s\n", "Delay", C(GREEN), C(RESET));
    if (p)
        printf("    %-14s %sP%d%s  (%d-coach platform, %s)\n", "Platform", C(CYAN), p->number, C(RESET),
               p->max_coaches, platform_state_name(p->state));
    else
        printf("    %-14s %sNot assigned%s\n", "Platform", C(YELLOW), C(RESET));
    printf("    %-14s %s%s%s%s\n", "Status", C(BOLD), col(sc), train_status_name(t->status), C(RESET));
    printf("    %-14s %d %s(1 = highest)%s\n", "Priority", train_priority(t->type), C(GRAY), C(RESET));

    if (t->status == TS_SCHEDULED || t->status == TS_APPROACHING)
        printf("\n    %s▸ Arriving in %d min%s\n", C(CYAN), train_eta(t) - st->clock, C(RESET));
    else if (t->status == TS_HELD)
        printf("\n    %s▸ Held at signal for %d min%s\n", C(RED), t->held_minutes, C(RESET));
    else if (t->status == TS_AT_PLATFORM)
        printf("\n    %s▸ Departing in %d min%s\n", C(GREEN), train_etd(t) - st->clock, C(RESET));
}

/* ------------------------------------------------------------------ */
/* Track & platform diagrams                                           */
/* ------------------------------------------------------------------ */

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

static void centered_track(const char *fill, UiColor fill_c, const char *label, UiColor label_c)
{
    int len = (int)strlen(label);
    int left = (TRACK_LEN - len) / 2;
    int right = TRACK_LEN - len - left;
    if (left < 0) left = 0;
    if (right < 0) right = 0;
    printf("%s", col(fill_c));
    ui_repeat(fill, left);
    printf("%s%s%s%s%s", C(RESET), C(BOLD), col(label_c), label, C(RESET));
    printf("%s", col(fill_c));
    ui_repeat(fill, right);
    printf("%s", C(RESET));
}

static void track_body(Station *st, const Platform *p)
{
    char label[64];

    if (p->state == PS_MAINTENANCE) {
        centered_track("░", CLR_YELLOW, " UNDER MAINTENANCE ", CLR_YELLOW);
    } else if (p->state == PS_OCCUPIED) {
        Train *o = station_find_train(st, p->occupant_id);
        if (o) snprintf(label, sizeof label, "[ %s %-14.14s dep %s ]", o->number, o->name,
                        util_time_str(train_etd(o)));
        else   snprintf(label, sizeof label, "[ OCCUPIED ]");
        centered_track("═", CLR_GREEN, label, CLR_WHITE);
    } else {
        printf("%s", C(GRAY));
        ui_repeat("─", TRACK_LEN);
        printf("%s", C(RESET));
    }
}

void view_track(Station *st)
{
    Train *list[MAX_TRAINS];
    RailSignal *outer = station_outer_signal(st);
    int i, n, shown = 0;
    char pfs[8];

    printf("  %sAPPROACH%s  %s%s%s %s", C(BOLD), C(RESET), C(GRAY), outer ? outer->code : "OUT",
           C(RESET), outer ? view_lamp(outer->aspect) : "  ");

    n = sched_sorted(st, list, MAX_TRAINS, 0);
    for (i = 0; i < n && shown < 4; i++) {
        Train *t = list[i];
        if (t->status != TS_APPROACHING && t->status != TS_HELD) continue;
        if (t->platform) snprintf(pfs, sizeof pfs, "P%d", t->platform);
        else             snprintf(pfs, sizeof pfs, "P?");
        if (t->status == TS_HELD)
            printf("  %s%s%s HELD→%s%s", C(BOLD), C(RED), t->number, pfs, C(RESET));
        else
            printf("  %s%s→%s %s%s", t->platform ? C(CYAN) : C(YELLOW), t->number, pfs,
                   util_time_str(train_eta(t)), C(RESET));
        shown++;
    }
    if (!shown) printf("  %sline clear - no trains approaching%s", C(GRAY), C(RESET));
    printf("\n\n");

    for (i = 0; i < st->platform_count; i++) {
        Platform *p = &st->platforms[i];
        RailSignal *h = station_home_signal(st, p->number);
        RailSignal *s = station_starter_signal(st, p->number);
        Train *nx = next_for_platform(st, p->number);
        UiColor stc = p->state == PS_FREE ? CLR_GREEN : (p->state == PS_OCCUPIED ? CLR_RED : CLR_YELLOW);

        printf("  %sP%-2d%s %s%2dc%s  %sH%s%s", C(BOLD), p->number, C(RESET), C(GRAY), p->max_coaches,
               C(RESET), C(GRAY), C(RESET), h ? view_lamp(h->aspect) : "  ");
        track_body(st, p);
        printf(" %s%sS%s", s ? view_lamp(s->aspect) : "  ", C(GRAY), C(RESET));
        printf("  %s%-11s%s", col(stc), platform_state_name(p->state), C(RESET));
        if (nx)
            printf(" %snext%s %s%s %s%s", C(GRAY), C(RESET), C(CYAN), nx->number,
                   util_time_str(train_eta(nx)), C(RESET));
        printf("\n");
    }
}

void view_platform_table(Station *st)
{
    int i, j;

    printf("%s %-4s %-6s %-12s %-27s %-9s %-9s %-24s%s\n", C(HEADER),
           "PF", "LENGTH", "STATE", "OCCUPIED BY", "HOME", "STARTER", "NEXT ARRIVAL", C(RESET));

    for (i = 0; i < st->platform_count; i++) {
        Platform *p = &st->platforms[i];
        RailSignal *h = station_home_signal(st, p->number);
        RailSignal *s = station_starter_signal(st, p->number);
        Train *o = station_find_train(st, p->occupant_id);
        Train *nx = next_for_platform(st, p->number);
        UiColor stc = p->state == PS_FREE ? CLR_GREEN : (p->state == PS_OCCUPIED ? CLR_RED : CLR_YELLOW);
        char len[8], occ[48], nxt[48];
        int queued = 0;

        for (j = 0; j < st->train_count; j++) {
            Train *t = &st->trains[j];
            if (train_is_active(t) && t->platform == p->number && t->status != TS_AT_PLATFORM) queued++;
        }
        snprintf(len, sizeof len, "%dc", p->max_coaches);
        if (o) snprintf(occ, sizeof occ, "%s %s", o->number, o->name);
        else   snprintf(occ, sizeof occ, "-");
        if (nx) snprintf(nxt, sizeof nxt, "%s @%s (%d queued)", nx->number,
                         util_time_str(train_eta(nx)), queued);
        else    snprintf(nxt, sizeof nxt, "-");

        printf(" %sP%-3d%s %-6s %s%-12s%s %-27.27s ", C(BOLD), p->number, C(RESET), len,
               col(stc), platform_state_name(p->state), C(RESET), occ);
        if (h) printf("%s%s%-7s%s ", view_lamp(h->aspect), col(view_aspect_color(h->aspect)),
                      signal_aspect_short(h->aspect), C(RESET));
        else   printf("%-9s ", "-");
        if (s) printf("%s%s%-7s%s ", view_lamp(s->aspect), col(view_aspect_color(s->aspect)),
                      signal_aspect_short(s->aspect), C(RESET));
        else   printf("%-9s ", "-");
        printf("%s%s%s\n", nx ? C(CYAN) : C(GRAY), nxt, C(RESET));
    }
}

void view_signal_table(Station *st)
{
    int i;

    printf("%s %-7s %-8s %-4s %-17s %-9s %-48s%s\n", C(HEADER),
           "CODE", "TYPE", "PF", "ASPECT", "MODE", "MEANING", C(RESET));

    for (i = 0; i < st->signal_count; i++) {
        RailSignal *s = &st->signals[i];
        char pf[8];
        const char *mode;
        UiColor mc;

        if (s->platform) snprintf(pf, sizeof pf, "P%d", s->platform);
        else             snprintf(pf, sizeof pf, "-");

        if (!st->auto_signals)  { mode = "MANUAL";   mc = CLR_YELLOW; }
        else if (s->manual)     { mode = "OVERRIDE"; mc = CLR_MAGENTA; }
        else                    { mode = "AUTO";     mc = CLR_GREEN; }

        printf(" %s%-7s%s %-8s %-4s %s%s%-15s%s %s%-9s%s %s%.48s%s\n",
               C(BOLD), s->code, C(RESET), signal_kind_name(s->kind), pf,
               view_lamp(s->aspect), col(view_aspect_color(s->aspect)), signal_aspect_name(s->aspect), C(RESET),
               col(mc), mode, C(RESET), C(GRAY), signal_aspect_meaning(s->aspect), C(RESET));
        if (s->kind == SG_STARTER && i + 1 < st->signal_count)
            printf("%s%s%s\n", C(GRAY), "", C(RESET));
    }
}

/* ------------------------------------------------------------------ */
/* Occupancy (Gantt) chart                                             */
/* ------------------------------------------------------------------ */

void view_occupancy_chart(Station *st)
{
    enum { COLS = 72, STEP = 2 };
    static const UiColor palette[] = { CLR_GREEN, CLR_CYAN, CLR_MAGENTA, CLR_YELLOW, CLR_BLUE };
    int start = st->clock - st->clock % 10;
    int end = start + COLS * STEP;
    int now_col = (st->clock - start) / STEP;
    char ruler[COLS + 1];
    int c, i, j;

    memset(ruler, ' ', COLS);
    ruler[COLS] = '\0';
    for (c = 0; c + 5 <= COLS; c += 15) memcpy(ruler + c, util_time_str(start + c * STEP), 5);

    printf("        %s%s%s\n", C(GRAY), ruler, C(RESET));
    printf("        ");
    for (c = 0; c < COLS; c++) {
        if (c == now_col)     printf("%s%s▼%s", C(BOLD), C(YELLOW), C(RESET));
        else if (c % 15 == 0) printf("%s|%s", C(GRAY), C(RESET));
        else if (c % 5 == 0)  printf("%s'%s", C(GRAY), C(RESET));
        else                  printf(" ");
    }
    printf("\n");

    for (i = 0; i < st->platform_count; i++) {
        Platform *p = &st->platforms[i];
        int legend = 0, extra = 0;

        printf("  %sP%-2d%s %s│%s", C(BOLD), p->number, C(RESET), C(GRAY), C(RESET));
        for (c = 0; c < COLS; c++) {
            int tm = start + c * STEP + 1, found = -1;
            if (p->state == PS_MAINTENANCE) {
                printf("%s░%s", C(YELLOW), C(RESET));
                continue;
            }
            for (j = 0; j < st->train_count; j++) {
                const Train *t = &st->trains[j];
                if (t->status == TS_CANCELLED || t->platform != p->number) continue;
                if (train_eta(t) <= tm && tm < train_etd(t)) { found = j; break; }
            }
            if (found >= 0) {
                const Train *t = &st->trains[found];
                UiColor cc = t->status == TS_DEPARTED ? CLR_GRAY : palette[found % 5];
                printf("%s█%s", col(cc), C(RESET));
            } else {
                printf("%s%s%s", C(GRAY), c == now_col ? "┆" : "·", C(RESET));
            }
        }
        printf("%s│%s", C(GRAY), C(RESET));

        for (j = 0; j < st->train_count; j++) {
            const Train *t = &st->trains[j];
            if (t->status == TS_CANCELLED || t->platform != p->number) continue;
            if (train_etd(t) <= start || train_eta(t) >= end) continue;
            if (legend < 3) {
                printf(" %s%s%s", col(t->status == TS_DEPARTED ? CLR_GRAY : palette[j % 5]), t->number, C(RESET));
                legend++;
            } else extra++;
        }
        if (extra) printf(" %s+%d%s", C(GRAY), extra, C(RESET));
        printf("\n");
    }

    printf("\n  %s█%s booked/occupied   %s█%s departed   %s░%s maintenance   %s·%s free   "
           "%s▼%s now   %s(1 column = %d min, window %s - %s)%s\n",
           C(GREEN), C(RESET), C(GRAY), C(RESET), C(YELLOW), C(RESET), C(GRAY), C(RESET),
           C(YELLOW), C(RESET), C(GRAY), STEP, util_time_str(start), util_time_str(end), C(RESET));
}

/* ------------------------------------------------------------------ */
/* Delay report                                                        */
/* ------------------------------------------------------------------ */

void view_delay_report(Station *st)
{
    Train *list[MAX_TRAINS];
    int n = 0, i, j, counted = 0, ontime = 0, total_delay = 0;

    for (i = 0; i < st->train_count; i++) {
        Train *t = &st->trains[i];
        if (t->status == TS_CANCELLED) continue;
        counted++;
        if (t->delay > 0) { list[n++] = t; total_delay += t->delay; }
        else ontime++;
    }
    /* insertion sort: worst delay first */
    for (i = 1; i < n; i++) {
        Train *k = list[i];
        for (j = i - 1; j >= 0 && list[j]->delay < k->delay; j--) list[j + 1] = list[j];
        list[j + 1] = k;
    }

    {
        int pct = counted ? ontime * 100 / counted : 100;
        UiColor pc = pct >= 80 ? CLR_GREEN : (pct >= 60 ? CLR_YELLOW : CLR_RED);
        printf("  %sPUNCTUALITY%s %s%s%d%%%s   %sDELAYED%s %s%d%s   %sAVG DELAY%s %s%d min%s   %sTOTAL LOST%s %s%d min%s\n\n",
               C(GRAY), C(RESET), C(BOLD), col(pc), pct, C(RESET),
               C(GRAY), C(RESET), C(BOLD), n, C(RESET),
               C(GRAY), C(RESET), C(BOLD), n ? total_delay / n : 0, C(RESET),
               C(GRAY), C(RESET), C(BOLD), total_delay, C(RESET));
    }

    printf("%s %-7s %-20s %-5s %-5s %-7s %-22s %-26s%s\n", C(HEADER),
           "TRAIN", "NAME", "SCH", "ETA", "DELAY", "REASON", "IMPACT", C(RESET));
    if (n == 0) printf("   %sAll trains are running on time.%s\n", C(GREEN), C(RESET));

    for (i = 0; i < n; i++) {
        Train *t = list[i];
        UiColor dc = t->delay >= 30 ? CLR_RED : CLR_YELLOW;
        int bars = t->delay / 3 + 1;
        char d[16];
        if (bars > 20) bars = 20;
        snprintf(d, sizeof d, "+%d min", t->delay);
        printf(" %s%-7s%s %-20.20s %-5s %s%-5s%s %s%s%-7s%s %-22.22s %s",
               C(BOLD), t->number, C(RESET), t->name, util_time_str(t->sched_arrival),
               C(YELLOW), util_time_str(train_eta(t)), C(RESET),
               C(BOLD), col(dc), d, C(RESET),
               t->delay_reason[0] ? t->delay_reason : "-", col(dc));
        ui_repeat("█", bars);
        printf("%s %s%s%s\n", C(RESET), col(view_status_color(t->status)),
               train_is_active(t) ? "" : "(done)", C(RESET));
    }
}

/* ------------------------------------------------------------------ */
/* Events                                                              */
/* ------------------------------------------------------------------ */

static void event_line(const LogEntry *e)
{
    printf("  %s%s%s  %s%-5s%s  %s\n", C(GRAY), util_time_str(e->time), C(RESET),
           col(level_color(e->level)), log_level_name(e->level), C(RESET), e->msg);
}

void view_events(Station *st, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        const LogEntry *e = log_get(&st->log, i);
        if (!e) break;
        event_line(e);
    }
    if (i == 0) printf("  %sNo events yet.%s\n", C(GRAY), C(RESET));
}

void view_events_since(Station *st, long since)
{
    int fresh = (int)(st->log.total - since);
    int i, cap = 40;

    if (fresh > st->log.count) fresh = st->log.count;
    if (fresh <= 0) {
        printf("  %sNo new events.%s\n", C(GRAY), C(RESET));
        return;
    }
    if (fresh > cap) {
        printf("  %s(%d events - showing the latest %d, see Event Log for all)%s\n",
               C(GRAY), fresh, cap, C(RESET));
        fresh = cap;
    }
    for (i = fresh - 1; i >= 0; i--) event_line(log_get(&st->log, i));
}

/* ------------------------------------------------------------------ */
/* Help                                                                */
/* ------------------------------------------------------------------ */

void view_help(void)
{
    int i;

    ui_section("TRAIN STATUS");
    for (i = 0; i < TS_COUNT; i++) {
        static const char *desc[TS_COUNT] = {
            "In the timetable, more than 15 minutes away",
            "Within 15 minutes of the station - platform is allocated now",
            "Waiting at the outer signal (no platform / platform busy / red home signal)",
            "Standing on its platform, starter signal clears 1 min before departure",
            "Has left the station",
            "Cancelled by the operator"
        };
        printf("    %s%s%-12s%s %s\n", C(BOLD), col(view_status_color((TrainStatus)i)),
               train_status_name((TrainStatus)i), C(RESET), desc[i]);
    }

    ui_section("SIGNAL ASPECTS");
    for (i = 0; i < SA_COUNT; i++)
        printf("    %s %s%-14s%s %s\n", view_lamp((SignalAspect)i), col(view_aspect_color((SignalAspect)i)),
               signal_aspect_name((SignalAspect)i), C(RESET), signal_aspect_meaning((SignalAspect)i));

    ui_section("SIGNALS AT THIS STATION");
    printf("    %sOUT-A%s   Outer signal - guards the approach into the station\n", C(BOLD), C(RESET));
    printf("    %sH<n>%s    Home signal  - admits a train onto platform n\n", C(BOLD), C(RESET));
    printf("    %sS<n>%s    Starter      - lets a train depart from platform n\n", C(BOLD), C(RESET));

    ui_section("INTERLOCKING & RULES");
    printf("    • A home signal cannot clear if its platform is occupied or under maintenance.\n");
    printf("    • A starter signal cannot clear unless a train is standing at the platform.\n");
    printf("    • Two trains on one platform need a %d-minute gap; conflicts are re-planned automatically.\n",
           PLATFORM_BUFFER);
    printf("    • Auto allocation picks the shortest platform that fits (best-fit), by train priority.\n");
    printf("    • Every minute a train waits at a red signal adds 1 minute of delay.\n");
}

/* ------------------------------------------------------------------ */
/* Dashboard                                                           */
/* ------------------------------------------------------------------ */

void view_dashboard(Station *st)
{
    ui_clear();
    view_header(st, "LIVE DASHBOARD");
    view_stats(st);
    ui_section("INCOMING & ACTIVE TRAINS");
    view_train_list(st, 0, 7);
    ui_section("TRACK & SIGNAL VIEW");
    view_track(st);
    ui_section("RECENT EVENTS");
    view_events(st, 4);
}
