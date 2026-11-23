/*
 * demo_data.c - Sample station + timetable
 */
#include <string.h>

#include "core/demo_data.h"
#include "core/scheduler.h"
#include "util/utils.h"

#define T(h, m) ((h) * 60 + (m))

static Train *add(Station *st, const char *no, const char *name, const char *from, const char *to,
                  TrainType type, int coaches, int arr, int dep)
{
    Train t;
    int id;

    memset(&t, 0, sizeof t);
    util_copy(t.number, no, sizeof t.number);
    util_copy(t.name, name, sizeof t.name);
    util_copy(t.source, from, sizeof t.source);
    util_copy(t.destination, to, sizeof t.destination);
    t.type            = type;
    t.status          = TS_SCHEDULED;
    t.coaches         = coaches;
    t.sched_arrival   = arr;
    t.sched_departure = dep;

    id = station_add_train(st, &t);
    return station_find_train(st, id);
}

void demo_load(Station *st)
{
    static const int lengths[] = { 24, 24, 22, 18, 26, 12 };
    Train *a, *b, *c, *d, *e, *f;

    station_init(st, "Central Junction", "CJN", 6, lengths);
    st->clock = T(7, 50);

    a = add(st, "12951", "Northern Star Express", "New Delhi",  "Mumbai",    TT_SUPERFAST, 24, T(7, 55), T(8, 5));
    b = add(st, "12627", "Coastal Superfast",     "Chennai",    "Kolkata",   TT_SUPERFAST, 22, T(8, 2),  T(8, 12));
        add(st, "16345", "Valley Passenger",      "Pune",       "Nagpur",    TT_PASSENGER, 18, T(8, 10), T(8, 20));
    c = add(st, "22691", "Deccan Express",        "Bengaluru",  "Hyderabad", TT_EXPRESS,   24, T(8, 15), T(8, 25));
    d = add(st, "06421", "City Local",            "Riverside",  "Old Town",  TT_LOCAL,     12, T(8, 20), T(8, 23));
    e = add(st, "12001", "Capital Shatabdi",      "New Delhi",  "Bhopal",    TT_SUPERFAST, 20, T(8, 30), T(8, 35));
        add(st, "58210", "Hill Passenger",        "Dehradun",   "Haridwar",  TT_PASSENGER, 16, T(8, 35), T(8, 50));
    f = add(st, "19019", "Desert Mail",           "Jaipur",     "Ahmedabad", TT_EXPRESS,   22, T(8, 45), T(8, 55));
        add(st, "40122", "Coal Freight",          "Dhanbad",    "Vizag",     TT_FREIGHT,   26, T(8, 50), T(9, 10));
        add(st, "12259", "Sunrise Duronto",       "Kolkata",    "Mumbai",    TT_SUPERFAST, 24, T(9, 0),  T(9, 10));
        add(st, "06433", "Suburban Local",        "Old Town",   "Riverside", TT_LOCAL,     12, T(9, 5),  T(9, 8));
        add(st, "17230", "River Express",         "Varanasi",   "Patna",     TT_EXPRESS,   21, T(9, 15), T(9, 25));

    log_add(&st->log, st->clock, LOG_INFO, "Control room online - demo timetable loaded (%d trains, %d platforms)",
            st->train_count, st->platform_count);

    /* Pre-planned platform allocations; the rest are allocated on approach */
    sched_assign(st, a->id, 1);
    sched_assign(st, b->id, 2);
    sched_assign(st, c->id, 1);
    sched_assign(st, d->id, 6);
    sched_assign(st, e->id, 3);

    sched_delay(st, c->id, 10, "Late running");
    sched_delay(st, f->id, 25, "Signal failure");

    sched_process(st);
}
