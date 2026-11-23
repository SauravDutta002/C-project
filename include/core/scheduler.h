/*
 * scheduler.h - Operations engine: platform allocation, delays,
 *               signal interlocking and the simulation clock.
 */
#ifndef CORE_SCHEDULER_H
#define CORE_SCHEDULER_H

#include "core/station.h"

typedef enum {
    ASSIGN_OK,
    ASSIGN_ERR_NO_TRAIN,
    ASSIGN_ERR_INACTIVE,
    ASSIGN_ERR_AT_PLATFORM,
    ASSIGN_ERR_NO_PLATFORM,
    ASSIGN_ERR_MAINTENANCE,
    ASSIGN_ERR_TOO_SHORT,
    ASSIGN_ERR_CONFLICT
} AssignResult;

const char *assign_result_str(AssignResult r);

/* Platform allocation */
AssignResult sched_check_platform(Station *st, const Train *t, int pf, int *conflict_id);
AssignResult sched_assign(Station *st, int train_id, int pf);
int  sched_auto_assign(Station *st, Train *t);             /* returns platform or 0 */
int  sched_auto_assign_all(Station *st, int *failed);      /* returns count assigned */
int  sched_release(Station *st, int train_id);
void sched_revalidate(Station *st, Train *t);              /* fix conflicts after a change */
int  sched_set_maintenance(Station *st, int pf, int on, int *moved);

/* Train operations */
int  sched_delay(Station *st, int train_id, int minutes, const char *reason);
int  sched_cancel(Station *st, int train_id);

/* Signalling */
void sched_update_signals(Station *st);
int  sched_set_signal(Station *st, RailSignal *s, SignalAspect a, const char **why);
void sched_release_signal(Station *st, RailSignal *s);
void sched_release_all_signals(Station *st);
void sched_emergency_stop(Station *st);
void sched_toggle_auto_signals(Station *st);

/* Simulation */
void sched_process(Station *st);                 /* evaluate state at current time */
void sched_tick(Station *st);                    /* advance one minute            */
void sched_advance(Station *st, int minutes);

/* Queries */
int  sched_sorted(Station *st, Train **out, int max, int include_inactive);

#endif /* CORE_SCHEDULER_H */
