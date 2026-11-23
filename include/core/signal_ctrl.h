/*
 * signal_ctrl.h - Railway signal entity
 * (named signal_ctrl to avoid clashing with the C standard <signal.h>)
 */
#ifndef CORE_SIGNAL_CTRL_H
#define CORE_SIGNAL_CTRL_H

#include "config.h"

typedef enum {
    SA_RED, SA_YELLOW, SA_DOUBLE_YELLOW, SA_GREEN,
    SA_COUNT
} SignalAspect;

typedef enum {
    SG_OUTER,     /* guards the approach into the station   */
    SG_HOME,      /* admits a train onto a platform line    */
    SG_STARTER,   /* lets a train leave the platform        */
    SG_COUNT
} SignalKind;

typedef struct RailSignal {
    int          id;
    char         code[CODE_LEN];   /* e.g. OUT-A, H3, S3 */
    SignalKind   kind;
    int          platform;         /* 0 for outer signal  */
    SignalAspect aspect;
    int          manual;           /* 1 = operator override */
} RailSignal;

void        signal_init(RailSignal *s, int id, const char *code, SignalKind kind, int platform);
const char *signal_aspect_name(SignalAspect a);
const char *signal_aspect_short(SignalAspect a);
const char *signal_aspect_meaning(SignalAspect a);
const char *signal_kind_name(SignalKind k);

#endif /* CORE_SIGNAL_CTRL_H */
