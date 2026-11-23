/*
 * train.c - Train entity helpers
 */
#include <string.h>

#include "core/train.h"
#include "util/utils.h"

static const char *TYPE_NAMES[TT_COUNT] = {
    "Superfast", "Express", "Passenger", "Local", "Freight"
};

static const char *TYPE_CODES[TT_COUNT] = {
    "SF", "EXP", "PASS", "LOC", "FRT"
};

static const char *STATUS_NAMES[TS_COUNT] = {
    "SCHEDULED", "APPROACHING", "HELD AT SIG", "ON PLATFORM", "DEPARTED", "CANCELLED"
};

const char *train_type_name(TrainType t)
{
    return ((int)t >= 0 && (int)t < TT_COUNT) ? TYPE_NAMES[t] : "Unknown";
}

const char *train_type_code(TrainType t)
{
    return ((int)t >= 0 && (int)t < TT_COUNT) ? TYPE_CODES[t] : "?";
}

const char *train_status_name(TrainStatus s)
{
    return ((int)s >= 0 && (int)s < TS_COUNT) ? STATUS_NAMES[s] : "UNKNOWN";
}

/* Enum order already reflects operating priority */
int train_priority(TrainType t) { return (int)t + 1; }

int train_eta(const Train *t)  { return t->sched_arrival + t->delay; }
int train_etd(const Train *t)  { return t->sched_departure + t->delay; }
int train_halt(const Train *t) { return t->sched_departure - t->sched_arrival; }

int train_is_active(const Train *t)
{
    return t->status != TS_DEPARTED && t->status != TS_CANCELLED;
}

int train_valid_number(const char *s)
{
    size_t n = strlen(s);
    return n >= 4 && n <= 6 && util_is_digits(s);
}
