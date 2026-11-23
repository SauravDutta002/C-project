/*
 * station.h - The station aggregate: trains, platforms, signals, clock, log
 */
#ifndef CORE_STATION_H
#define CORE_STATION_H

#include "config.h"
#include "core/train.h"
#include "core/platform.h"
#include "core/signal_ctrl.h"
#include "io/logger.h"

typedef struct Station {
    char       name[NAME_LEN];
    char       code[CODE_LEN];
    int        clock;                /* station time, minutes since midnight */

    Train      trains[MAX_TRAINS];
    int        train_count;
    int        next_train_id;

    Platform   platforms[MAX_PLATFORMS];
    int        platform_count;

    RailSignal signals[MAX_SIGNALS];
    int        signal_count;

    int        auto_signals;         /* automatic signalling on/off          */
    int        auto_assign;          /* auto platform allocation on approach */

    EventLog   log;
} Station;

void station_init(Station *st, const char *name, const char *code,
                  int platform_count, const int *platform_lengths);
void station_build_signals(Station *st);

int    station_add_train(Station *st, const Train *t);   /* returns new id or -1 */
int    station_remove_train(Station *st, int id);

Train      *station_find_train(Station *st, int id);
Train      *station_find_train_by_number(Station *st, const char *number);
Platform   *station_platform(Station *st, int number);
RailSignal *station_find_signal(Station *st, const char *code);
RailSignal *station_home_signal(Station *st, int platform);
RailSignal *station_starter_signal(Station *st, int platform);
RailSignal *station_outer_signal(Station *st);

#endif /* CORE_STATION_H */
