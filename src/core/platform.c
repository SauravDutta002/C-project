/*
 * platform.c - Platform entity helpers
 */
#include "core/platform.h"

static const char *STATE_NAMES[PS_COUNT] = { "FREE", "OCCUPIED", "MAINTENANCE" };

void platform_init(Platform *p, int number, int max_coaches)
{
    p->number      = number;
    p->max_coaches = max_coaches;
    p->state       = PS_FREE;
    p->occupant_id = 0;
}

const char *platform_state_name(PlatformState s)
{
    return ((int)s >= 0 && (int)s < PS_COUNT) ? STATE_NAMES[s] : "UNKNOWN";
}
