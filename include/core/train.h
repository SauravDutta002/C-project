/*
 * train.h - Train entity
 */
#ifndef CORE_TRAIN_H
#define CORE_TRAIN_H

#include "config.h"

typedef enum {
    TT_SUPERFAST, TT_EXPRESS, TT_PASSENGER, TT_LOCAL, TT_FREIGHT,
    TT_COUNT
} TrainType;

typedef enum {
    TS_SCHEDULED,     /* in timetable, not yet near the station          */
    TS_APPROACHING,   /* within APPROACH_WINDOW minutes of arrival        */
    TS_HELD,          /* waiting at the outer signal (no route / platform)*/
    TS_AT_PLATFORM,   /* standing on its platform                         */
    TS_DEPARTED,
    TS_CANCELLED,
    TS_COUNT
} TrainStatus;

typedef struct Train {
    int         id;
    char        number[TRAIN_NO_LEN];
    char        name[NAME_LEN];
    char        source[CITY_LEN];
    char        destination[CITY_LEN];
    TrainType   type;
    TrainStatus status;
    int         coaches;
    int         sched_arrival;      /* minutes since midnight */
    int         sched_departure;
    int         delay;              /* minutes                */
    int         platform;           /* 1-based, 0 = none      */
    int         held_minutes;       /* minutes waiting at a red signal */
    char        delay_reason[NAME_LEN];
} Train;

const char *train_type_name(TrainType t);
const char *train_type_code(TrainType t);
const char *train_status_name(TrainStatus s);
int         train_priority(TrainType t);      /* 1 = highest */

int train_eta(const Train *t);
int train_etd(const Train *t);
int train_halt(const Train *t);
int train_is_active(const Train *t);
int train_valid_number(const char *s);

#endif /* CORE_TRAIN_H */
