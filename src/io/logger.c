/*
 * logger.c - Event log implementation
 */
#include <stdio.h>
#include <stdarg.h>

#include "io/logger.h"
#include "util/utils.h"

static const char *LEVEL_NAMES[LOG_LEVEL_COUNT] = { "INFO", "OK", "WARN", "ALERT" };

void log_init(EventLog *log)
{
    log->head  = 0;
    log->count = 0;
    log->total = 0;
}

void log_add(EventLog *log, int time, LogLevel level, const char *fmt, ...)
{
    LogEntry *e = &log->entries[log->head];
    va_list ap;
    FILE *f;

    e->time  = time;
    e->level = level;
    va_start(ap, fmt);
    vsnprintf(e->msg, sizeof e->msg, fmt, ap);
    va_end(ap);

    log->head = (log->head + 1) % MAX_LOG_ENTRIES;
    if (log->count < MAX_LOG_ENTRIES) log->count++;
    log->total++;

    f = fopen(LOG_FILE, "a");
    if (f) {
        fprintf(f, "[%s] %-5s %s\n", util_time_str(time), log_level_name(level), e->msg);
        fclose(f);
    }
}

const LogEntry *log_get(const EventLog *log, int index)
{
    int idx;
    if (index < 0 || index >= log->count) return NULL;
    idx = (log->head - 1 - index + 2 * MAX_LOG_ENTRIES) % MAX_LOG_ENTRIES;
    return &log->entries[idx];
}

const char *log_level_name(LogLevel level)
{
    return ((int)level >= 0 && (int)level < LOG_LEVEL_COUNT) ? LEVEL_NAMES[level] : "?";
}
