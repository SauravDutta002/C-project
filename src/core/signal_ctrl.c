/*
 * signal_ctrl.c - Railway signal entity helpers
 */
#include "core/signal_ctrl.h"
#include "util/utils.h"

static const char *ASPECT_NAMES[SA_COUNT]  = { "RED", "YELLOW", "DOUBLE YELLOW", "GREEN" };
static const char *ASPECT_SHORT[SA_COUNT]  = { "RED", "YELLOW", "D-YEL", "GREEN" };
static const char *ASPECT_MEANING[SA_COUNT] = {
    "STOP - danger, do not pass",
    "CAUTION - route set, stop at platform",
    "ATTENTION - route being prepared",
    "PROCEED - line clear"
};
static const char *KIND_NAMES[SG_COUNT] = { "OUTER", "HOME", "STARTER" };

void signal_init(RailSignal *s, int id, const char *code, SignalKind kind, int platform)
{
    s->id       = id;
    util_copy(s->code, code, sizeof s->code);
    s->kind     = kind;
    s->platform = platform;
    s->aspect   = SA_RED;   /* fail-safe default */
    s->manual   = 0;
}

const char *signal_aspect_name(SignalAspect a)
{
    return ((int)a >= 0 && (int)a < SA_COUNT) ? ASPECT_NAMES[a] : "?";
}

const char *signal_aspect_short(SignalAspect a)
{
    return ((int)a >= 0 && (int)a < SA_COUNT) ? ASPECT_SHORT[a] : "?";
}

const char *signal_aspect_meaning(SignalAspect a)
{
    return ((int)a >= 0 && (int)a < SA_COUNT) ? ASPECT_MEANING[a] : "";
}

const char *signal_kind_name(SignalKind k)
{
    return ((int)k >= 0 && (int)k < SG_COUNT) ? KIND_NAMES[k] : "?";
}
