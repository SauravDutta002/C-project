/*
 * scheduler.c - Operations engine
 *
 * Train life-cycle driven by the clock:
 *
 *   SCHEDULED --(ETA within 15 min)--> APPROACHING --(ETA reached)--+--> ON PLATFORM --(ETD)--> DEPARTED
 *                                                                   |        ^
 *                                       (no route / red home signal)+--> HELD (+1 min delay each minute)
 *
 * Interlocking rules:
 *   - a HOME signal can only clear when its platform line is free and in service
 *   - a STARTER signal can only clear when a train is standing on that platform
 *   - two trains may not share a platform within PLATFORM_BUFFER minutes
 */
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "core/scheduler.h"
#include "util/utils.h"

#define LOG(st, lvl, ...) log_add(&(st)->log, (st)->clock, (lvl), __VA_ARGS__)

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static int windows_overlap(int a1, int a2, int b1, int b2)
{
    return a1 < b2 + PLATFORM_BUFFER && b1 < a2 + PLATFORM_BUFFER;
}

/* A train overstaying on a platform keeps it blocked until "now" at least */
static int occupancy_end(const Station *st, const Train *o)
{
    int end = train_etd(o);
    if (o->status == TS_AT_PLATFORM && end < st->clock) end = st->clock;
    return end;
}

const char *assign_result_str(AssignResult r)
{
    switch (r) {
    case ASSIGN_OK:              return "OK";
    case ASSIGN_ERR_NO_TRAIN:    return "train not found";
    case ASSIGN_ERR_INACTIVE:    return "train has departed or is cancelled";
    case ASSIGN_ERR_AT_PLATFORM: return "train is already standing at a platform";
    case ASSIGN_ERR_NO_PLATFORM: return "no such platform";
    case ASSIGN_ERR_MAINTENANCE: return "platform under maintenance";
    case ASSIGN_ERR_TOO_SHORT:   return "platform too short for this train";
    case ASSIGN_ERR_CONFLICT:    return "time-slot conflict";
    }
    return "unknown";
}

/* ------------------------------------------------------------------ */
/* Platform allocation                                                 */
/* ------------------------------------------------------------------ */

AssignResult sched_check_platform(Station *st, const Train *t, int pf, int *conflict_id)
{
    const Platform *p;
    int i;

    if (conflict_id) *conflict_id = 0;
    if (!t)                           return ASSIGN_ERR_NO_TRAIN;
    if (!train_is_active(t))          return ASSIGN_ERR_INACTIVE;
    if (t->status == TS_AT_PLATFORM)  return ASSIGN_ERR_AT_PLATFORM;
    if (pf < 1 || pf > st->platform_count) return ASSIGN_ERR_NO_PLATFORM;

    p = &st->platforms[pf - 1];
    if (p->state == PS_MAINTENANCE)   return ASSIGN_ERR_MAINTENANCE;
    if (t->coaches > p->max_coaches)  return ASSIGN_ERR_TOO_SHORT;

    for (i = 0; i < st->train_count; i++) {
        const Train *o = &st->trains[i];
        if (o == t || o->platform != pf || !train_is_active(o)) continue;
        if (windows_overlap(train_eta(t), train_etd(t), train_eta(o), occupancy_end(st, o))) {
            if (conflict_id) *conflict_id = o->id;
            return ASSIGN_ERR_CONFLICT;
        }
    }
    return ASSIGN_OK;
}

/* Best-fit: the shortest valid platform, so long platforms stay free for long trains */
static int pick_platform(Station *st, const Train *t)
{
    int pf, best = 0, best_len = INT_MAX;
    for (pf = 1; pf <= st->platform_count; pf++) {
        int len = st->platforms[pf - 1].max_coaches;
        if (len < best_len && sched_check_platform(st, t, pf, NULL) == ASSIGN_OK) {
            best = pf;
            best_len = len;
        }
    }
    return best;
}

AssignResult sched_assign(Station *st, int train_id, int pf)
{
    Train *t = station_find_train(st, train_id);
    AssignResult r = sched_check_platform(st, t, pf, NULL);

    if (r == ASSIGN_OK) {
        int old = t->platform;
        t->platform = pf;
        if (old && old != pf)
            LOG(st, LOG_INFO, "%s %s moved P%d -> P%d by operator", t->number, t->name, old, pf);
        else if (old != pf)
            LOG(st, LOG_INFO, "%s %s assigned to P%d by operator", t->number, t->name, pf);
        sched_update_signals(st);
    }
    return r;
}

int sched_auto_assign(Station *st, Train *t)
{
    int pf;
    if (!t) return 0;
    pf = pick_platform(st, t);
    if (pf) {
        t->platform = pf;
        LOG(st, LOG_OK, "%s %s auto-assigned to P%d (%d coaches, %dc platform)",
            t->number, t->name, pf, t->coaches, st->platforms[pf - 1].max_coaches);
        sched_update_signals(st);
    }
    return pf;
}

static int cmp_priority(const void *a, const void *b)
{
    const Train *x = *(Train * const *)a;
    const Train *y = *(Train * const *)b;
    int d = train_priority(x->type) - train_priority(y->type);
    if (d) return d;
    return train_eta(x) - train_eta(y);
}

int sched_auto_assign_all(Station *st, int *failed)
{
    Train *list[MAX_TRAINS];
    int n = 0, i, ok = 0, bad = 0;

    for (i = 0; i < st->train_count; i++) {
        Train *t = &st->trains[i];
        if (train_is_active(t) && !t->platform && t->status != TS_AT_PLATFORM)
            list[n++] = t;
    }
    qsort(list, (size_t)n, sizeof list[0], cmp_priority);   /* superfast first */

    for (i = 0; i < n; i++) {
        if (sched_auto_assign(st, list[i])) ok++;
        else {
            bad++;
            LOG(st, LOG_WARN, "No suitable platform for %s %s", list[i]->number, list[i]->name);
        }
    }
    if (failed) *failed = bad;
    return ok;
}

int sched_release(Station *st, int train_id)
{
    Train *t = station_find_train(st, train_id);
    if (!t || !train_is_active(t) || !t->platform || t->status == TS_AT_PLATFORM) return 0;
    LOG(st, LOG_INFO, "%s %s released from P%d", t->number, t->name, t->platform);
    t->platform = 0;
    sched_update_signals(st);
    return 1;
}

void sched_revalidate(Station *st, Train *t)
{
    int pf, cid, i;

    if (!t || !t->platform || !train_is_active(t)) return;
    pf = t->platform;

    if (t->status != TS_AT_PLATFORM) {
        AssignResult r = sched_check_platform(st, t, pf, &cid);
        if (r != ASSIGN_OK) {
            Train *o = cid ? station_find_train(st, cid) : NULL;
            t->platform = 0;
            LOG(st, LOG_WARN, "%s lost P%d: %s%s%s", t->number, pf, assign_result_str(r),
                o ? " with " : "", o ? o->number : "");
            if (!sched_auto_assign(st, t))
                LOG(st, LOG_ALERT, "No platform available for %s - operator action needed", t->number);
        }
    } else {
        /* Train is standing here and now leaves later: bump whoever comes next */
        for (i = 0; i < st->train_count; i++) {
            Train *o = &st->trains[i];
            if (o == t || !train_is_active(o) || o->platform != pf || o->status == TS_AT_PLATFORM)
                continue;
            if (windows_overlap(train_eta(t), occupancy_end(st, t), train_eta(o), train_etd(o))) {
                o->platform = 0;
                LOG(st, LOG_WARN, "%s moved off P%d - %s still occupying", o->number, pf, t->number);
                if (!sched_auto_assign(st, o))
                    LOG(st, LOG_ALERT, "No platform available for %s - operator action needed", o->number);
            }
        }
    }
}

int sched_set_maintenance(Station *st, int pf, int on, int *moved)
{
    Platform *p = station_platform(st, pf);
    int i, m = 0;

    if (moved) *moved = 0;
    if (!p) return 0;

    if (on) {
        if (p->state == PS_OCCUPIED || p->state == PS_MAINTENANCE) return 0;
        p->state = PS_MAINTENANCE;
        p->occupant_id = 0;
        LOG(st, LOG_ALERT, "P%d taken OUT OF SERVICE for maintenance", pf);
        for (i = 0; i < st->train_count; i++) {
            Train *t = &st->trains[i];
            if (!train_is_active(t) || t->platform != pf || t->status == TS_AT_PLATFORM) continue;
            t->platform = 0;
            m++;
            if (!sched_auto_assign(st, t))
                LOG(st, LOG_ALERT, "%s has no platform after P%d closure", t->number, pf);
        }
    } else {
        if (p->state != PS_MAINTENANCE) return 0;
        p->state = PS_FREE;
        LOG(st, LOG_OK, "P%d back in service", pf);
    }
    if (moved) *moved = m;
    sched_update_signals(st);
    return 1;
}

/* ------------------------------------------------------------------ */
/* Train operations                                                    */
/* ------------------------------------------------------------------ */

int sched_delay(Station *st, int train_id, int minutes, const char *reason)
{
    Train *t = station_find_train(st, train_id);
    if (!t || !train_is_active(t) || minutes == 0) return 0;

    t->delay += minutes;
    if (t->delay < 0) t->delay = 0;
    if (reason && *reason) util_copy(t->delay_reason, reason, sizeof t->delay_reason);
    if (t->delay == 0) t->delay_reason[0] = '\0';

    if (minutes > 0) {
        if (t->status == TS_AT_PLATFORM)
            LOG(st, LOG_WARN, "%s %s departure delayed +%d min (%s) - ETD %s", t->number, t->name,
                minutes, t->delay_reason[0] ? t->delay_reason : "unspecified", util_time_str(train_etd(t)));
        else
            LOG(st, LOG_WARN, "%s %s delayed +%d min (%s) - ETA %s", t->number, t->name,
                minutes, t->delay_reason[0] ? t->delay_reason : "unspecified", util_time_str(train_eta(t)));
    } else {
        LOG(st, LOG_OK, "%s %s recovered %d min - delay now %d min", t->number, t->name, -minutes, t->delay);
    }

    if (t->status == TS_HELD && train_eta(t) > st->clock) {
        t->status = TS_APPROACHING;     /* new ETA in the future: no longer waiting */
        t->held_minutes = 0;
    }
    if (t->status == TS_APPROACHING && train_eta(t) - st->clock > APPROACH_WINDOW)
        t->status = TS_SCHEDULED;

    sched_revalidate(st, t);
    sched_update_signals(st);
    return 1;
}

int sched_cancel(Station *st, int train_id)
{
    Train *t = station_find_train(st, train_id);
    if (!t || !train_is_active(t)) return 0;

    if (t->status == TS_AT_PLATFORM) {
        Platform *p = station_platform(st, t->platform);
        if (p) { p->state = PS_FREE; p->occupant_id = 0; }
    }
    t->status = TS_CANCELLED;
    t->platform = 0;
    t->held_minutes = 0;
    LOG(st, LOG_ALERT, "%s %s CANCELLED", t->number, t->name);
    sched_update_signals(st);
    return 1;
}

/* ------------------------------------------------------------------ */
/* Signalling                                                          */
/* ------------------------------------------------------------------ */

static SignalAspect compute_aspect(Station *st, const RailSignal *s)
{
    int j;
    Platform *p;

    if (s->kind == SG_OUTER) {
        int held = 0, imminent = 0, approaching = 0;
        for (j = 0; j < st->train_count; j++) {
            const Train *t = &st->trains[j];
            if (t->status == TS_HELD) held = 1;
            else if (t->status == TS_APPROACHING) {
                approaching = 1;
                if (train_eta(t) - st->clock <= HOME_CLEAR_WINDOW + 2) imminent = 1;
            }
        }
        if (held)        return SA_RED;
        if (imminent)    return SA_GREEN;
        if (approaching) return SA_YELLOW;
        return SA_RED;
    }

    p = station_platform(st, s->platform);
    if (!p) return SA_RED;

    if (s->kind == SG_HOME) {
        SignalAspect best = SA_RED;
        if (p->state != PS_FREE) return SA_RED;
        for (j = 0; j < st->train_count; j++) {
            const Train *t = &st->trains[j];
            if (t->platform != s->platform) continue;
            if (t->status != TS_APPROACHING && t->status != TS_HELD) continue;
            if (train_eta(t) - st->clock <= HOME_CLEAR_WINDOW) return SA_YELLOW;
            best = SA_DOUBLE_YELLOW;
        }
        return best;
    }

    /* Starter: clear one minute before booked departure */
    if (p->state == PS_OCCUPIED) {
        const Train *o = station_find_train(st, p->occupant_id);
        if (o && st->clock >= train_etd(o) - 1) return SA_GREEN;
    }
    return SA_RED;
}

void sched_update_signals(Station *st)
{
    int i;
    if (!st->auto_signals) return;
    for (i = 0; i < st->signal_count; i++) {
        RailSignal *s = &st->signals[i];
        if (!s->manual) s->aspect = compute_aspect(st, s);
    }
}

int sched_set_signal(Station *st, RailSignal *s, SignalAspect a, const char **why)
{
    Platform *p = s->platform ? station_platform(st, s->platform) : NULL;
    const char *reason = NULL;

    if (a != SA_RED && p) {
        if (s->kind == SG_HOME) {
            if (p->state == PS_MAINTENANCE)   reason = "INTERLOCK: platform under maintenance";
            else if (p->state == PS_OCCUPIED) reason = "INTERLOCK: platform line is occupied";
        } else if (s->kind == SG_STARTER && p->state != PS_OCCUPIED) {
            reason = "INTERLOCK: no train standing at the platform";
        }
    }
    if (why) *why = reason;
    if (reason) {
        LOG(st, LOG_WARN, "Signal %s -> %s refused (%s)", s->code, signal_aspect_name(a), reason);
        return 0;
    }
    s->aspect = a;
    s->manual = 1;
    LOG(st, LOG_INFO, "Signal %s set to %s by operator", s->code, signal_aspect_name(a));
    return 1;
}

void sched_release_signal(Station *st, RailSignal *s)
{
    s->manual = 0;
    LOG(st, LOG_INFO, "Signal %s returned to automatic control", s->code);
    sched_update_signals(st);
}

void sched_release_all_signals(Station *st)
{
    int i;
    for (i = 0; i < st->signal_count; i++) st->signals[i].manual = 0;
    LOG(st, LOG_INFO, "All signal overrides released");
    sched_update_signals(st);
}

void sched_emergency_stop(Station *st)
{
    int i;
    for (i = 0; i < st->signal_count; i++) {
        st->signals[i].aspect = SA_RED;
        st->signals[i].manual = 1;
    }
    LOG(st, LOG_ALERT, "EMERGENCY - all signals thrown to RED");
}

void sched_toggle_auto_signals(Station *st)
{
    st->auto_signals = !st->auto_signals;
    LOG(st, LOG_INFO, "Automatic signalling %s", st->auto_signals ? "ENABLED" : "DISABLED");
    sched_update_signals(st);
}

/* ------------------------------------------------------------------ */
/* Simulation                                                          */
/* ------------------------------------------------------------------ */

static void hold_train(Station *st, Train *t, const char *why)
{
    t->delay++;
    t->held_minutes++;
    if (t->status != TS_HELD || t->held_minutes % 10 == 0)
        LOG(st, LOG_WARN, "%s held at outer signal - %s (%d min)", t->number, why, t->held_minutes);
    t->status = TS_HELD;
}

static void try_arrive(Station *st, Train *t)
{
    const char *why = NULL;
    Platform *p = NULL;
    RailSignal *home = NULL;

    if (!t->platform && st->auto_assign) sched_auto_assign(st, t);

    /* Booked platform blocked? divert to another one if we can */
    if (t->platform && st->auto_assign) {
        p = station_platform(st, t->platform);
        if (p && p->state != PS_FREE) {
            int old = t->platform, alt;
            t->platform = 0;
            alt = pick_platform(st, t);
            if (alt) {
                t->platform = alt;
                LOG(st, LOG_INFO, "%s diverted P%d -> P%d (P%d blocked)", t->number, old, alt, old);
            } else {
                t->platform = old;
            }
        }
    }

    p = t->platform ? station_platform(st, t->platform) : NULL;
    if (!t->platform)                       why = "no platform assigned";
    else if (!p)                            why = "invalid platform";
    else if (p->state == PS_MAINTENANCE)    why = "platform under maintenance";
    else if (p->state == PS_OCCUPIED)       why = "platform occupied";
    else {
        sched_update_signals(st);
        home = station_home_signal(st, t->platform);
        if (home && home->aspect == SA_RED) why = "home signal at RED";
    }

    if (why) {
        hold_train(st, t, why);
        return;
    }

    p->state       = PS_OCCUPIED;
    p->occupant_id = t->id;
    t->status      = TS_AT_PLATFORM;
    t->held_minutes = 0;
    if (home) { home->aspect = SA_RED; home->manual = 0; }  /* signal replaced behind train */

    if (t->delay > 0)
        LOG(st, LOG_OK, "%s %s arrived on P%d (%d min late)", t->number, t->name, t->platform, t->delay);
    else
        LOG(st, LOG_OK, "%s %s arrived on P%d on time", t->number, t->name, t->platform);
    sched_update_signals(st);
}

static void try_depart(Station *st, Train *t)
{
    Platform *p = station_platform(st, t->platform);
    RailSignal *starter;

    sched_update_signals(st);
    starter = station_starter_signal(st, t->platform);

    if (starter && starter->aspect == SA_RED) {
        t->delay++;
        t->held_minutes++;
        if (t->held_minutes == 1 || t->held_minutes % 10 == 0)
            LOG(st, LOG_WARN, "%s waiting on P%d - starter %s at RED (%d min)",
                t->number, t->platform, starter->code, t->held_minutes);
        sched_revalidate(st, t);
        return;
    }

    if (p) { p->state = PS_FREE; p->occupant_id = 0; }
    t->status = TS_DEPARTED;
    t->held_minutes = 0;
    if (starter) { starter->aspect = SA_RED; starter->manual = 0; }
    LOG(st, LOG_OK, "%s %s departed P%d for %s", t->number, t->name, t->platform, t->destination);
    sched_update_signals(st);
}

void sched_process(Station *st)
{
    int i;

    sched_update_signals(st);
    for (i = 0; i < st->train_count; i++) {
        Train *t = &st->trains[i];
        if (!train_is_active(t)) continue;

        if (t->status == TS_SCHEDULED && train_eta(t) - st->clock <= APPROACH_WINDOW) {
            t->status = TS_APPROACHING;
            LOG(st, LOG_INFO, "%s %s approaching - ETA %s", t->number, t->name, util_time_str(train_eta(t)));
            if (!t->platform && st->auto_assign) sched_auto_assign(st, t);
            if (!t->platform)
                LOG(st, LOG_WARN, "%s approaching with NO platform assigned", t->number);
            sched_update_signals(st);
        }

        if ((t->status == TS_APPROACHING || t->status == TS_HELD) && st->clock >= train_eta(t))
            try_arrive(st, t);
        else if (t->status == TS_AT_PLATFORM && st->clock >= train_etd(t))
            try_depart(st, t);
    }
    sched_update_signals(st);
}

void sched_tick(Station *st)
{
    st->clock++;
    sched_process(st);
}

void sched_advance(Station *st, int minutes)
{
    while (minutes-- > 0) sched_tick(st);
}

/* ------------------------------------------------------------------ */
/* Queries                                                             */
/* ------------------------------------------------------------------ */

static int cmp_eta(const void *a, const void *b)
{
    const Train *x = *(Train * const *)a;
    const Train *y = *(Train * const *)b;
    int d = train_eta(x) - train_eta(y);
    if (d) return d;
    return strcmp(x->number, y->number);
}

int sched_sorted(Station *st, Train **out, int max, int include_inactive)
{
    int i, n = 0;
    for (i = 0; i < st->train_count && n < max; i++)
        if (include_inactive || train_is_active(&st->trains[i]))
            out[n++] = &st->trains[i];
    qsort(out, (size_t)n, sizeof *out, cmp_eta);
    return n;
}
