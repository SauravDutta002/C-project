/*
 * platform.h - Platform entity
 */
#ifndef CORE_PLATFORM_H
#define CORE_PLATFORM_H

typedef enum {
    PS_FREE, PS_OCCUPIED, PS_MAINTENANCE,
    PS_COUNT
} PlatformState;

typedef struct Platform {
    int           number;        /* 1-based                       */
    int           max_coaches;   /* longest train it can hold     */
    PlatformState state;
    int           occupant_id;   /* train id standing here, 0 = none */
} Platform;

void        platform_init(Platform *p, int number, int max_coaches);
const char *platform_state_name(PlatformState s);

#endif /* CORE_PLATFORM_H */
