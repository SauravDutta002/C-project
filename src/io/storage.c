/*
 * storage.c - Text based ('|' separated) save format
 *
 *   RAILNET|<version>
 *   STATION|name|code|clock|auto_signals|auto_assign|next_id
 *   PLATFORM|number|max_coaches|state|occupant
 *   SIGNAL|id|code|kind|platform|aspect|manual
 *   TRAIN|id|number|name|from|to|type|status|coaches|arr|dep|delay|platform|held|reason
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "io/storage.h"
#include "core/scheduler.h"
#include "util/utils.h"

#define MAX_FIELDS 20

static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

int storage_save(const Station *st, const char *path)
{
    FILE *f = fopen(path, "w");
    int i;

    if (!f) return 0;

    fprintf(f, "RAILNET|%d\n", STORAGE_VERSION);
    fprintf(f, "STATION|%s|%s|%d|%d|%d|%d\n", st->name, st->code, st->clock,
            st->auto_signals, st->auto_assign, st->next_train_id);

    for (i = 0; i < st->platform_count; i++) {
        const Platform *p = &st->platforms[i];
        fprintf(f, "PLATFORM|%d|%d|%d|%d\n", p->number, p->max_coaches, (int)p->state, p->occupant_id);
    }
    for (i = 0; i < st->signal_count; i++) {
        const RailSignal *s = &st->signals[i];
        fprintf(f, "SIGNAL|%d|%s|%d|%d|%d|%d\n", s->id, s->code, (int)s->kind, s->platform,
                (int)s->aspect, s->manual);
    }
    for (i = 0; i < st->train_count; i++) {
        const Train *t = &st->trains[i];
        fprintf(f, "TRAIN|%d|%s|%s|%s|%s|%d|%d|%d|%d|%d|%d|%d|%d|%s\n",
                t->id, t->number, t->name, t->source, t->destination, (int)t->type, (int)t->status,
                t->coaches, t->sched_arrival, t->sched_departure, t->delay, t->platform,
                t->held_minutes, t->delay_reason);
    }

    fclose(f);
    return 1;
}

/* Split in place on '|', keeping empty fields */
static int split(char *line, char **fields, int max)
{
    int n = 0;
    fields[n++] = line;
    for (; *line && n < max; line++) {
        if (*line == '|') {
            *line = '\0';
            fields[n++] = line + 1;
        }
    }
    return n;
}

int storage_load(Station *st, const char *path)
{
    FILE *f = fopen(path, "r");
    char line[512];
    char *fl[MAX_FIELDS];
    Station *tmp;
    int valid = 0;

    if (!f) return 0;
    tmp = (Station *)calloc(1, sizeof *tmp);
    if (!tmp) { fclose(f); return 0; }
    log_init(&tmp->log);
    tmp->next_train_id = 1;

    while (fgets(line, sizeof line, f)) {
        int n;
        line[strcspn(line, "\r\n")] = '\0';
        if (!line[0]) continue;
        n = split(line, fl, MAX_FIELDS);

        if (strcmp(fl[0], "RAILNET") == 0) {
            valid = 1;
        } else if (strcmp(fl[0], "STATION") == 0 && n >= 7) {
            util_copy(tmp->name, fl[1], sizeof tmp->name);
            util_copy(tmp->code, fl[2], sizeof tmp->code);
            tmp->clock         = atoi(fl[3]);
            tmp->auto_signals  = atoi(fl[4]) != 0;
            tmp->auto_assign   = atoi(fl[5]) != 0;
            tmp->next_train_id = atoi(fl[6]);
        } else if (strcmp(fl[0], "PLATFORM") == 0 && n >= 5 && tmp->platform_count < MAX_PLATFORMS) {
            Platform *p = &tmp->platforms[tmp->platform_count++];
            p->number      = tmp->platform_count;
            p->max_coaches = clampi(atoi(fl[2]), 1, MAX_COACHES);
            p->state       = (PlatformState)clampi(atoi(fl[3]), 0, PS_COUNT - 1);
            p->occupant_id = atoi(fl[4]);
        } else if (strcmp(fl[0], "SIGNAL") == 0 && n >= 7 && tmp->signal_count < MAX_SIGNALS) {
            RailSignal *s = &tmp->signals[tmp->signal_count++];
            s->id       = atoi(fl[1]);
            util_copy(s->code, fl[2], sizeof s->code);
            s->kind     = (SignalKind)clampi(atoi(fl[3]), 0, SG_COUNT - 1);
            s->platform = atoi(fl[4]);
            s->aspect   = (SignalAspect)clampi(atoi(fl[5]), 0, SA_COUNT - 1);
            s->manual   = atoi(fl[6]) != 0;
        } else if (strcmp(fl[0], "TRAIN") == 0 && n >= 15 && tmp->train_count < MAX_TRAINS) {
            Train *t = &tmp->trains[tmp->train_count++];
            t->id              = atoi(fl[1]);
            util_copy(t->number, fl[2], sizeof t->number);
            util_copy(t->name, fl[3], sizeof t->name);
            util_copy(t->source, fl[4], sizeof t->source);
            util_copy(t->destination, fl[5], sizeof t->destination);
            t->type            = (TrainType)clampi(atoi(fl[6]), 0, TT_COUNT - 1);
            t->status          = (TrainStatus)clampi(atoi(fl[7]), 0, TS_COUNT - 1);
            t->coaches         = clampi(atoi(fl[8]), 1, MAX_COACHES);
            t->sched_arrival   = atoi(fl[9]);
            t->sched_departure = atoi(fl[10]);
            t->delay           = clampi(atoi(fl[11]), 0, 10 * MAX_DELAY);
            t->platform        = atoi(fl[12]);
            t->held_minutes    = atoi(fl[13]);
            util_copy(t->delay_reason, fl[14], sizeof t->delay_reason);
            if (t->id >= tmp->next_train_id) tmp->next_train_id = t->id + 1;
        }
    }
    fclose(f);

    if (!valid || tmp->platform_count == 0) {
        free(tmp);
        return 0;
    }
    if (tmp->signal_count == 0) station_build_signals(tmp);

    *st = *tmp;
    free(tmp);
    return 1;
}

int storage_export_board(Station *st, const char *path)
{
    Train *list[MAX_TRAINS];
    FILE *f = fopen(path, "w");
    int n, i;

    if (!f) return -1;
    n = sched_sorted(st, list, MAX_TRAINS, 1);

    fprintf(f, "%s v%s - %s (%s)\n", APP_NAME, APP_VERSION, st->name, st->code);
    fprintf(f, "Arrivals & departures board at station time %s\n\n", util_time_str(st->clock));
    fprintf(f, "%-7s %-24s %-12s %-12s %-5s %-5s %-5s %-6s %-4s %-12s %s\n",
            "TRAIN", "NAME", "FROM", "TO", "SCH", "ETA", "ETD", "DELAY", "PF", "STATUS", "REASON");
    for (i = 0; i < 110; i++) fputc('-', f);
    fputc('\n', f);

    for (i = 0; i < n; i++) {
        Train *t = list[i];
        char pf[8];
        if (t->platform) snprintf(pf, sizeof pf, "P%d", t->platform);
        else             strcpy(pf, "--");
        fprintf(f, "%-7s %-24s %-12s %-12s %-5s %-5s %-5s %-6d %-4s %-12s %s\n",
                t->number, t->name, t->source, t->destination,
                util_time_str(t->sched_arrival), util_time_str(train_eta(t)), util_time_str(train_etd(t)),
                t->delay, pf, train_status_name(t->status), t->delay_reason);
    }

    fprintf(f, "\nPlatforms\n");
    for (i = 0; i < st->platform_count; i++) {
        Platform *p = &st->platforms[i];
        Train *o = station_find_train(st, p->occupant_id);
        fprintf(f, "  P%-2d  %2d coaches  %-11s %s\n", p->number, p->max_coaches,
                platform_state_name(p->state), o ? o->number : "");
    }
    fclose(f);
    return n;
}
