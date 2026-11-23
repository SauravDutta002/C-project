/*
 * logger.h - In-memory ring buffer event log, mirrored to data/events.log
 */
#ifndef IO_LOGGER_H
#define IO_LOGGER_H

#include "config.h"
#include "ui/ui.h"

typedef enum { LOG_INFO, LOG_OK, LOG_WARN, LOG_ALERT, LOG_LEVEL_COUNT } LogLevel;

typedef struct {
    int      time;
    LogLevel level;
    char     msg[LOG_MSG_LEN];
} LogEntry;

typedef struct {
    LogEntry entries[MAX_LOG_ENTRIES];
    int      head;     /* next write position          */
    int      count;    /* entries currently stored     */
    long     total;    /* entries ever written         */
} EventLog;

void            log_init(EventLog *log);
void            log_add(EventLog *log, int time, LogLevel level, const char *fmt, ...) UI_PRINTF(4, 5);
const LogEntry *log_get(const EventLog *log, int index);   /* 0 = most recent */
const char     *log_level_name(LogLevel level);

#endif /* IO_LOGGER_H */
