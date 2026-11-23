/*
 * station.c - Station aggregate implementation
 */
#include <stdio.h>
#include <string.h>

#include "core/station.h"
#include "util/utils.h"

void station_init(Station *st, const char *name, const char *code,
                  int platform_count, const int *platform_lengths)
{
    int i;

    memset(st, 0, sizeof *st);
    util_copy(st->name, name, sizeof st->name);
    util_copy(st->code, code, sizeof st->code);
    st->clock         = 8 * 60;
    st->next_train_id = 1;
    st->auto_signals  = 1;
    st->auto_assign   = 1;

    if (platform_count > MAX_PLATFORMS) platform_count = MAX_PLATFORMS;
    for (i = 0; i < platform_count; i++)
        platform_init(&st->platforms[i], i + 1, platform_lengths[i]);
    st->platform_count = platform_count;

    station_build_signals(st);
    log_init(&st->log);
}

/* One outer signal for the station plus a home + starter per platform */
void station_build_signals(Station *st)
{
    int i, n = 0;
    char code[CODE_LEN];

    signal_init(&st->signals[n], n + 1, "OUT-A", SG_OUTER, 0);
    n++;
    for (i = 1; i <= st->platform_count; i++) {
        snprintf(code, sizeof code, "H%d", i);
        signal_init(&st->signals[n], n + 1, code, SG_HOME, i);
        n++;
        snprintf(code, sizeof code, "S%d", i);
        signal_init(&st->signals[n], n + 1, code, SG_STARTER, i);
        n++;
    }
    st->signal_count = n;
}

int station_add_train(Station *st, const Train *t)
{
    Train *slot;
    if (st->train_count >= MAX_TRAINS) return -1;
    slot = &st->trains[st->train_count++];
    *slot = *t;
    slot->id = st->next_train_id++;
    return slot->id;
}

int station_remove_train(Station *st, int id)
{
    int i;
    for (i = 0; i < st->train_count; i++) {
        if (st->trains[i].id == id) {
            memmove(&st->trains[i], &st->trains[i + 1],
                    (size_t)(st->train_count - i - 1) * sizeof(Train));
            st->train_count--;
            return 1;
        }
    }
    return 0;
}

Train *station_find_train(Station *st, int id)
{
    int i;
    if (id <= 0) return NULL;
    for (i = 0; i < st->train_count; i++)
        if (st->trains[i].id == id) return &st->trains[i];
    return NULL;
}

Train *station_find_train_by_number(Station *st, const char *number)
{
    int i;
    for (i = 0; i < st->train_count; i++)
        if (strcmp(st->trains[i].number, number) == 0) return &st->trains[i];
    return NULL;
}

Platform *station_platform(Station *st, int number)
{
    if (number < 1 || number > st->platform_count) return NULL;
    return &st->platforms[number - 1];
}

RailSignal *station_find_signal(Station *st, const char *code)
{
    int i;
    for (i = 0; i < st->signal_count; i++)
        if (util_equals_ci(st->signals[i].code, code)) return &st->signals[i];
    return NULL;
}

static RailSignal *find_kind(Station *st, SignalKind kind, int platform)
{
    int i;
    for (i = 0; i < st->signal_count; i++)
        if (st->signals[i].kind == kind && st->signals[i].platform == platform)
            return &st->signals[i];
    return NULL;
}

RailSignal *station_home_signal(Station *st, int platform)    { return find_kind(st, SG_HOME, platform); }
RailSignal *station_starter_signal(Station *st, int platform) { return find_kind(st, SG_STARTER, platform); }
RailSignal *station_outer_signal(Station *st)                 { return find_kind(st, SG_OUTER, 0); }
