/*
 * menu.c - Interactive menus / operator workflows
 */
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "ui/menu.h"
#include "ui/ui.h"
#include "ui/views.h"
#include "core/scheduler.h"
#include "core/demo_data.h"
#include "io/storage.h"
#include "util/utils.h"

#define C(x) col(CLR_##x)

static Station *S;

static const char *DELAY_REASONS[] = {
    "Late running", "Signal failure", "Technical fault", "Crew change",
    "Bad weather", "Track maintenance", "Heavy passenger load"
};
#define REASON_COUNT ((int)(sizeof DELAY_REASONS / sizeof DELAY_REASONS[0]))

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static void screen(const char *title)
{
    ui_clear();
    view_header(S, title);
}

static Train *ask_train(const char *prompt)
{
    char buf[16];
    Train *t;
    if (!ui_prompt_string(prompt, buf, sizeof buf)) return NULL;
    t = station_find_train_by_number(S, buf);
    if (!t) ui_error("Train %s not found.", buf);
    return t;
}

static RailSignal *ask_signal(const char *prompt)
{
    char buf[16];
    RailSignal *s;
    if (!ui_prompt_string(prompt, buf, sizeof buf)) return NULL;
    s = station_find_signal(S, buf);
    if (!s) ui_error("Signal '%s' not found (use codes like OUT-A, H2, S4).", buf);
    return s;
}

static void show_new_events(long since)
{
    ui_section("EVENTS");
    view_events_since(S, since);
}

/* ------------------------------------------------------------------ */
/* Live simulation                                                     */
/* ------------------------------------------------------------------ */

static void run_live(void)
{
    int speed, steps;

    screen("LIVE SIMULATION");
    ui_section("SIMULATION SPEED");
    ui_menu_item('1', "Normal", "1.0 s  = 1 station minute");
    ui_menu_item('2', "Fast",   "0.5 s  = 1 station minute");
    ui_menu_item('3', "Turbo",  "0.2 s  = 1 station minute");
    printf("\n");
    if (!ui_prompt_int("Speed", 1, 3, &speed)) return;
    steps = speed == 1 ? 10 : (speed == 2 ? 5 : 2);

    ui_hide_cursor();
    for (;;) {
        int k, stop = 0;
        sched_tick(S);
        view_dashboard(S);
        printf("\n  %s ● LIVE %s  %ssimulation running - press any key to stop%s\n",
               C(BG_RED), C(RESET), C(GRAY), C(RESET));
        fflush(stdout);
        for (k = 0; k < steps; k++) {
            util_sleep_ms(100);
            if (util_key_pressed()) {
                util_consume_key();
                stop = 1;
                break;
            }
        }
        if (stop) break;
    }
    ui_show_cursor();
}

/* ------------------------------------------------------------------ */
/* 1. Dashboard                                                        */
/* ------------------------------------------------------------------ */

static void screen_dashboard(void)
{
    for (;;) {
        int c;
        view_dashboard(S);
        printf("\n  %s[Enter]%s back   %s[1]%s +1 min   %s[5]%s +5 min   %s[t]%s +15 min   %s[l]%s live mode\n",
               C(YELLOW), C(RESET), C(YELLOW), C(RESET), C(YELLOW), C(RESET), C(YELLOW), C(RESET),
               C(YELLOW), C(RESET));
        c = ui_prompt_choice("Action");
        if (c == '1')      sched_advance(S, 1);
        else if (c == '5') sched_advance(S, 5);
        else if (c == 't') sched_advance(S, 15);
        else if (c == 'l') run_live();
        else return;
    }
}

/* ------------------------------------------------------------------ */
/* 2. Board                                                            */
/* ------------------------------------------------------------------ */

static void screen_board(void)
{
    int all = 0;
    for (;;) {
        int c;
        screen("ARRIVALS & DEPARTURES BOARD");
        view_stats(S);
        ui_section(all ? "ALL TRAINS (incl. departed / cancelled)" : "ACTIVE TRAINS");
        view_train_list(S, all, 0);
        printf("\n  %s[a]%s show %s   %s[d]%s train details   %s[Enter]%s back\n",
               C(YELLOW), C(RESET), all ? "active only" : "all trains",
               C(YELLOW), C(RESET), C(YELLOW), C(RESET));
        c = ui_prompt_choice("Action");
        if (c == 'a') all = !all;
        else if (c == 'd') {
            Train *t = ask_train("Train number");
            if (t) view_train_details(S, t);
            ui_pause();
        } else return;
    }
}

/* ------------------------------------------------------------------ */
/* Platform assignment (shared)                                        */
/* ------------------------------------------------------------------ */

static void assign_manual(Train *t)
{
    int i, pf, cid;
    AssignResult r;

    ui_section("PLATFORM AVAILABILITY");
    printf("  %sTrain %s needs %d coaches, window %s - %s%s\n\n", C(GRAY), t->number, t->coaches,
           util_time_str(train_eta(t)), util_time_str(train_etd(t)), C(RESET));
    for (i = 0; i < S->platform_count; i++) {
        Platform *p = &S->platforms[i];
        r = sched_check_platform(S, t, p->number, &cid);
        printf("    %sP%-2d%s %2dc  %-11s  ", C(BOLD), p->number, C(RESET), p->max_coaches,
               platform_state_name(p->state));
        if (r == ASSIGN_OK) {
            printf("%s✓ AVAILABLE%s\n", C(GREEN), C(RESET));
        } else {
            Train *o = cid ? station_find_train(S, cid) : NULL;
            printf("%s✗ %s%s%s%s\n", C(RED), assign_result_str(r), o ? " with " : "", o ? o->number : "", C(RESET));
        }
    }
    printf("\n");
    if (!ui_prompt_int("Platform number", 1, S->platform_count, &pf)) {
        ui_info("No change made.");
        return;
    }
    r = sched_assign(S, t->id, pf);
    if (r == ASSIGN_OK) ui_success("Train %s assigned to platform %d.", t->number, pf);
    else                ui_error("Cannot assign P%d: %s.", pf, assign_result_str(r));
}

/* ------------------------------------------------------------------ */
/* 3. Train management                                                 */
/* ------------------------------------------------------------------ */

static void train_add(void)
{
    Train t, *tp;
    int v, id, halt, c;

    screen("TRAIN MANAGEMENT  ›  ADD TRAIN");
    if (S->train_count >= MAX_TRAINS) {
        ui_error("Timetable is full (%d trains).", MAX_TRAINS);
        ui_pause();
        return;
    }
    memset(&t, 0, sizeof t);
    ui_info("Leave any field blank to cancel.");
    printf("\n");

    for (;;) {
        if (!ui_prompt_string("Train number (4-6 digits)", t.number, sizeof t.number)) goto cancelled;
        if (!train_valid_number(t.number)) { ui_error("Train number must be 4 to 6 digits."); continue; }
        if (station_find_train_by_number(S, t.number)) { ui_error("Train %s already exists.", t.number); continue; }
        break;
    }
    if (!ui_prompt_string("Train name", t.name, sizeof t.name)) goto cancelled;
    if (!ui_prompt_string("Origin station", t.source, sizeof t.source)) goto cancelled;
    if (!ui_prompt_string("Destination", t.destination, sizeof t.destination)) goto cancelled;

    printf("\n");
    for (v = 0; v < TT_COUNT; v++) {
        char hint[32];
        snprintf(hint, sizeof hint, "priority %d", train_priority((TrainType)v));
        ui_menu_item((char)('1' + v), train_type_name((TrainType)v), hint);
    }
    if (!ui_prompt_int("Train type", 1, TT_COUNT, &v)) goto cancelled;
    t.type = (TrainType)(v - 1);

    if (!ui_prompt_int("Number of coaches", 1, MAX_COACHES, &t.coaches)) goto cancelled;
    if (!ui_prompt_time("Scheduled arrival", &t.sched_arrival)) goto cancelled;
    for (;;) {
        if (!ui_prompt_time("Scheduled departure", &t.sched_departure)) goto cancelled;
        halt = t.sched_departure - t.sched_arrival;
        if (halt < 1 || halt > MAX_HALT) {
            ui_error("Departure must be 1-%d minutes after arrival (same day).", MAX_HALT);
            continue;
        }
        break;
    }

    t.status = TS_SCHEDULED;
    id = station_add_train(S, &t);
    tp = station_find_train(S, id);
    if (!tp) { ui_error("Could not add train."); ui_pause(); return; }
    log_add(&S->log, S->clock, LOG_INFO, "New train %s %s added (arr %s, dep %s)", tp->number, tp->name,
            util_time_str(tp->sched_arrival), util_time_str(tp->sched_departure));
    ui_success("Train %s %s added to the timetable.", tp->number, tp->name);
    if (tp->sched_arrival < S->clock)
        ui_warn("Arrival time is earlier than the station clock - it will be processed immediately.");

    ui_section("PLATFORM ASSIGNMENT");
    ui_menu_item('a', "Auto-assign best platform", "shortest platform that fits");
    ui_menu_item('m', "Choose platform manually", "see availability per platform");
    ui_menu_item('n', "Decide later", "auto-assigned on approach if AUTO-PF is ON");
    c = ui_prompt_choice("Choice");
    if (c == 'a') {
        int pf = sched_auto_assign(S, tp);
        if (pf) ui_success("Assigned to platform %d.", pf);
        else    ui_warn("No platform currently available - assign later.");
    } else if (c == 'm') {
        assign_manual(tp);
    }
    ui_pause();
    return;

cancelled:
    ui_info("Cancelled - nothing was added.");
    ui_pause();
}

static void train_search(void)
{
    char q[NAME_LEN];
    Train *matches[MAX_TRAINS];
    int i, n = 0;

    screen("TRAIN MANAGEMENT  ›  DETAILS / SEARCH");
    if (!ui_prompt_string("Train number or part of name", q, sizeof q)) return;

    for (i = 0; i < S->train_count; i++) {
        Train *t = &S->trains[i];
        if (strcmp(t->number, q) == 0 || util_contains_ci(t->name, q) ||
            util_contains_ci(t->source, q) || util_contains_ci(t->destination, q))
            matches[n++] = &S->trains[i];
    }
    if (n == 0) {
        ui_warn("No train matches '%s'.", q);
    } else if (n == 1) {
        view_train_details(S, matches[0]);
    } else {
        ui_section("SEARCH RESULTS");
        view_train_header();
        for (i = 0; i < n; i++) view_train_row(matches[i]);
        printf("\n");
        ui_info("%d trains match - search by train number for full details.", n);
    }
    ui_pause();
}

static void train_edit(void)
{
    Train *t;
    char label[64];
    int arr, dep, coaches, v;

    screen("TRAIN MANAGEMENT  ›  EDIT SCHEDULE");
    view_train_list(S, 0, 0);
    printf("\n");
    t = ask_train("Train number to edit");
    if (!t) { ui_pause(); return; }
    if (t->status != TS_SCHEDULED && t->status != TS_APPROACHING) {
        ui_error("Only trains that have not yet arrived can be edited (status: %s).", train_status_name(t->status));
        ui_pause();
        return;
    }
    ui_info("Leave a field blank to keep its current value.");
    arr = t->sched_arrival;
    dep = t->sched_departure;
    coaches = t->coaches;

    snprintf(label, sizeof label, "New scheduled arrival (now %s)", util_time_str(arr));
    if (ui_prompt_time(label, &v)) arr = v;
    snprintf(label, sizeof label, "New scheduled departure (now %s)", util_time_str(dep));
    if (ui_prompt_time(label, &v)) dep = v;
    snprintf(label, sizeof label, "Coaches (now %d)", coaches);
    if (ui_prompt_int(label, 1, MAX_COACHES, &v)) coaches = v;

    if (dep - arr < 1 || dep - arr > MAX_HALT) {
        ui_error("Invalid halt of %d min - must be 1 to %d minutes. No changes saved.", dep - arr, MAX_HALT);
        ui_pause();
        return;
    }
    t->sched_arrival = arr;
    t->sched_departure = dep;
    t->coaches = coaches;
    if (t->status == TS_APPROACHING && train_eta(t) - S->clock > APPROACH_WINDOW) t->status = TS_SCHEDULED;
    log_add(&S->log, S->clock, LOG_INFO, "%s schedule edited: arr %s dep %s, %d coaches", t->number,
            util_time_str(arr), util_time_str(dep), coaches);
    {
        int old_pf = t->platform;
        sched_revalidate(S, t);
        ui_success("Schedule updated.");
        if (old_pf != t->platform) {
            if (t->platform) ui_warn("Platform changed P%d -> P%d to avoid a conflict.", old_pf, t->platform);
            else             ui_warn("Train lost P%d and no alternative is free - assign manually.", old_pf);
        }
    }
    ui_pause();
}

static void train_cancel(void)
{
    Train *t;
    screen("TRAIN MANAGEMENT  ›  CANCEL TRAIN");
    view_train_list(S, 0, 0);
    printf("\n");
    t = ask_train("Train number to cancel");
    if (t && ui_confirm("Really cancel this train?")) {
        if (sched_cancel(S, t->id)) ui_success("Train %s cancelled.", t->number);
        else                        ui_error("Train %s is not active.", t->number);
    }
    ui_pause();
}

static void train_remove(void)
{
    Train *t;
    char no[TRAIN_NO_LEN];

    screen("TRAIN MANAGEMENT  ›  REMOVE RECORD");
    view_train_list(S, 1, 0);
    printf("\n");
    t = ask_train("Train number to remove");
    if (!t) { ui_pause(); return; }
    if (t->status == TS_AT_PLATFORM) {
        ui_error("Train is standing at P%d - it must depart or be cancelled first.", t->platform);
        ui_pause();
        return;
    }
    if (ui_confirm("Permanently delete this train from the timetable?")) {
        util_copy(no, t->number, sizeof no);
        station_remove_train(S, t->id);
        log_add(&S->log, S->clock, LOG_INFO, "Train %s removed from timetable", no);
        sched_update_signals(S);
        ui_success("Train %s removed.", no);
    }
    ui_pause();
}

static void menu_trains(void)
{
    for (;;) {
        int c;
        screen("TRAIN MANAGEMENT");
        view_stats(S);
        ui_section("OPTIONS");
        ui_menu_item('1', "View all trains", "complete timetable");
        ui_menu_item('2', "Add new train", "register a train in the timetable");
        ui_menu_item('3', "Train details / search", "by number, name or city");
        ui_menu_item('4', "Edit schedule", "change times or formation");
        ui_menu_item('5', "Cancel a train", "frees its platform");
        ui_menu_item('6', "Remove train record", "delete from timetable");
        ui_menu_item('0', "Back", "");
        printf("\n");
        c = ui_prompt_choice("Select option");
        switch (c) {
        case '1':
            screen("TRAIN MANAGEMENT  ›  ALL TRAINS");
            view_train_list(S, 1, 0);
            ui_pause();
            break;
        case '2': train_add();    break;
        case '3': train_search(); break;
        case '4': train_edit();   break;
        case '5': train_cancel(); break;
        case '6': train_remove(); break;
        case '0': case 0: case UI_EOF: return;
        default: break;
        }
    }
}

/* ------------------------------------------------------------------ */
/* 4. Delay management                                                 */
/* ------------------------------------------------------------------ */

static void delay_report(void)
{
    Train *t;
    int mins, r, old_pf;
    char reason[NAME_LEN];

    screen("DELAY MANAGEMENT  ›  REPORT DELAY");
    view_train_list(S, 0, 0);
    printf("\n");
    t = ask_train("Train number");
    if (!t) { ui_pause(); return; }
    if (!train_is_active(t)) { ui_error("Train %s is %s.", t->number, train_status_name(t->status)); ui_pause(); return; }
    if (!ui_prompt_int("Delay to add (minutes)", 1, MAX_DELAY, &mins)) { ui_info("Cancelled."); ui_pause(); return; }

    printf("\n");
    for (r = 0; r < REASON_COUNT; r++) ui_menu_item((char)('1' + r), DELAY_REASONS[r], "");
    ui_menu_item((char)('1' + REASON_COUNT), "Other (type a reason)", "");
    if (ui_prompt_int("Reason", 1, REASON_COUNT + 1, &r)) {
        if (r <= REASON_COUNT) util_copy(reason, DELAY_REASONS[r - 1], sizeof reason);
        else if (!ui_prompt_string("Reason", reason, sizeof reason)) util_copy(reason, "Unspecified", sizeof reason);
    } else {
        util_copy(reason, "Unspecified", sizeof reason);
    }

    old_pf = t->platform;
    sched_delay(S, t->id, mins, reason);

    printf("\n");
    ui_success("%s now expected arr %s / dep %s - total delay %d min.", t->number,
               util_time_str(train_eta(t)), util_time_str(train_etd(t)), t->delay);
    if (old_pf != t->platform) {
        if (t->platform) ui_warn("Platform re-planned: P%d -> P%d (time-slot conflict).", old_pf, t->platform);
        else             ui_warn("Train lost P%d and no alternative is free - assign manually.", old_pf);
    }
    ui_pause();
}

static void delay_recover(int clear_all)
{
    Train *t;
    int mins;

    screen(clear_all ? "DELAY MANAGEMENT  ›  CLEAR DELAY" : "DELAY MANAGEMENT  ›  RECOVER TIME");
    view_delay_report(S);
    printf("\n");
    t = ask_train("Train number");
    if (!t) { ui_pause(); return; }
    if (!train_is_active(t)) { ui_error("Train %s is not active.", t->number); ui_pause(); return; }
    if (t->delay == 0) { ui_info("Train %s is already on time.", t->number); ui_pause(); return; }

    if (clear_all) mins = t->delay;
    else if (!ui_prompt_int("Minutes recovered", 1, t->delay, &mins)) { ui_pause(); return; }

    sched_delay(S, t->id, -mins, NULL);
    ui_success("%s delay now %d min (ETA %s).", t->number, t->delay, util_time_str(train_eta(t)));
    ui_pause();
}

static void menu_delays(void)
{
    for (;;) {
        int c;
        screen("DELAY MANAGEMENT");
        view_delay_report(S);
        ui_section("OPTIONS");
        ui_menu_item('1', "Report / add delay", "platform re-planned automatically");
        ui_menu_item('2', "Recover time", "train is making up time");
        ui_menu_item('3', "Clear delay", "train back on schedule");
        ui_menu_item('0', "Back", "");
        printf("\n");
        c = ui_prompt_choice("Select option");
        switch (c) {
        case '1': delay_report();    break;
        case '2': delay_recover(0);  break;
        case '3': delay_recover(1);  break;
        case '0': case 0: case UI_EOF: return;
        default: break;
        }
    }
}

/* ------------------------------------------------------------------ */
/* 5. Platform control                                                 */
/* ------------------------------------------------------------------ */

static void platform_assign(void)
{
    Train *t;
    screen("PLATFORM CONTROL  ›  ASSIGN PLATFORM");
    view_train_list(S, 0, 0);
    printf("\n");
    t = ask_train("Train number");
    if (!t) { ui_pause(); return; }
    if (!train_is_active(t))          { ui_error("Train %s is %s.", t->number, train_status_name(t->status)); ui_pause(); return; }
    if (t->status == TS_AT_PLATFORM)  { ui_error("Train is already standing on P%d.", t->platform); ui_pause(); return; }
    assign_manual(t);
    ui_pause();
}

static void platform_auto_all(void)
{
    int failed = 0, ok;
    long since = S->log.total;
    screen("PLATFORM CONTROL  ›  AUTO-ASSIGN");
    ok = sched_auto_assign_all(S, &failed);
    if (ok == 0 && failed == 0) ui_info("Every active train already has a platform.");
    else {
        if (ok)     ui_success("%d train(s) allocated a platform.", ok);
        if (failed) ui_warn("%d train(s) could not be placed - check maintenance or delays.", failed);
        show_new_events(since);
    }
    ui_pause();
}

static void platform_release(void)
{
    Train *t;
    screen("PLATFORM CONTROL  ›  RELEASE ASSIGNMENT");
    view_train_list(S, 0, 0);
    printf("\n");
    t = ask_train("Train number");
    if (!t) { ui_pause(); return; }
    if (t->status == TS_AT_PLATFORM) ui_error("Train is physically standing on P%d - cannot release.", t->platform);
    else if (!t->platform)           ui_info("Train %s has no platform assigned.", t->number);
    else if (sched_release(S, t->id)) ui_success("Platform assignment released for %s.", t->number);
    else                              ui_error("Could not release.");
    ui_pause();
}

static void platform_maintenance(void)
{
    int pf, moved = 0;
    Platform *p;

    screen("PLATFORM CONTROL  ›  MAINTENANCE");
    view_platform_table(S);
    printf("\n");
    if (!ui_prompt_int("Platform number", 1, S->platform_count, &pf)) return;
    p = station_platform(S, pf);

    if (p->state == PS_MAINTENANCE) {
        if (ui_confirm("Return this platform to service?") && sched_set_maintenance(S, pf, 0, NULL))
            ui_success("P%d is back in service.", pf);
    } else if (p->state == PS_OCCUPIED) {
        ui_error("P%d is occupied - wait for the train to depart.", pf);
    } else if (ui_confirm("Take this platform out of service?")) {
        long since = S->log.total;
        sched_set_maintenance(S, pf, 1, &moved);
        ui_success("P%d is now under maintenance. %d booked train(s) re-planned.", pf, moved);
        show_new_events(since);
    }
    ui_pause();
}

static void menu_platforms(void)
{
    for (;;) {
        int c;
        screen("PLATFORM CONTROL");
        view_platform_table(S);
        ui_section("OPTIONS");
        ui_menu_item('1', "Track & signal view", "live platform diagram");
        ui_menu_item('2', "Assign / change platform", "manual allocation with checks");
        ui_menu_item('3', "Auto-assign all unassigned", "best-fit by train priority");
        ui_menu_item('4', "Release a platform booking", "");
        ui_menu_item('5', "Toggle maintenance", "take a platform in/out of service");
        ui_menu_item('6', "Occupancy chart", "next 2.4 hours, per platform");
        ui_menu_item('7', S->auto_assign ? "Turn auto-allocation OFF" : "Turn auto-allocation ON",
                     "assign platforms when trains approach");
        ui_menu_item('0', "Back", "");
        printf("\n");
        c = ui_prompt_choice("Select option");
        switch (c) {
        case '1':
            screen("PLATFORM CONTROL  ›  TRACK VIEW");
            view_track(S);
            ui_pause();
            break;
        case '2': platform_assign();      break;
        case '3': platform_auto_all();    break;
        case '4': platform_release();     break;
        case '5': platform_maintenance(); break;
        case '6':
            screen("PLATFORM CONTROL  ›  OCCUPANCY CHART");
            printf("\n");
            view_occupancy_chart(S);
            ui_pause();
            break;
        case '7':
            S->auto_assign = !S->auto_assign;
            log_add(&S->log, S->clock, LOG_INFO, "Automatic platform allocation %s", S->auto_assign ? "ON" : "OFF");
            break;
        case '0': case 0: case UI_EOF: return;
        default: break;
        }
    }
}

/* ------------------------------------------------------------------ */
/* 6. Signal control                                                   */
/* ------------------------------------------------------------------ */

static void signal_set(void)
{
    RailSignal *s;
    int a;
    const char *why = NULL;

    screen("SIGNAL CONTROL  ›  SET ASPECT");
    view_signal_table(S);
    printf("\n");
    s = ask_signal("Signal code");
    if (!s) { ui_pause(); return; }
    printf("\n");
    for (a = 0; a < SA_COUNT; a++)
        printf("    %s%s[%d]%s %s %s%-14s%s %s%s%s\n", C(BOLD), C(YELLOW), a + 1, C(RESET),
               view_lamp((SignalAspect)a), col(view_aspect_color((SignalAspect)a)),
               signal_aspect_name((SignalAspect)a), C(RESET), C(GRAY),
               signal_aspect_meaning((SignalAspect)a), C(RESET));
    if (!ui_prompt_int("Aspect", 1, SA_COUNT, &a)) { ui_pause(); return; }

    if (sched_set_signal(S, s, (SignalAspect)(a - 1), &why)) {
        ui_success("Signal %s now shows %s.", s->code, signal_aspect_name(s->aspect));
        if (S->auto_signals) ui_info("Signal is under OVERRIDE - release it to return to automatic control.");
    } else {
        ui_error("Request refused - %s.", why ? why : "unknown reason");
    }
    ui_pause();
}

static void signal_release(void)
{
    RailSignal *s;
    screen("SIGNAL CONTROL  ›  RELEASE OVERRIDE");
    view_signal_table(S);
    printf("\n");
    s = ask_signal("Signal code");
    if (s) {
        sched_release_signal(S, s);
        ui_success("Signal %s returned to automatic control (%s).", s->code, signal_aspect_name(s->aspect));
        if (!S->auto_signals) ui_warn("Automatic signalling is OFF - enable it for the signal to update.");
    }
    ui_pause();
}

static void menu_signals(void)
{
    for (;;) {
        int c;
        screen("SIGNAL CONTROL");
        view_signal_table(S);
        ui_section("OPTIONS");
        ui_menu_item('1', "Set signal aspect", "operator override with interlocking");
        ui_menu_item('2', "Release one override", "return signal to automatic");
        ui_menu_item('3', "Release all overrides", "");
        ui_menu_item('4', S->auto_signals ? "Switch to MANUAL signalling" : "Switch to AUTOMATIC signalling", "");
        ui_menu_item('9', "EMERGENCY - all signals RED", "stops every train movement");
        ui_menu_item('0', "Back", "");
        printf("\n");
        c = ui_prompt_choice("Select option");
        switch (c) {
        case '1': signal_set();     break;
        case '2': signal_release(); break;
        case '3':
            sched_release_all_signals(S);
            ui_success("All overrides released.");
            ui_pause();
            break;
        case '4':
            sched_toggle_auto_signals(S);
            if (S->auto_signals) ui_success("Automatic signalling enabled.");
            else ui_warn("Manual signalling - you must clear home (H) and starter (S) signals yourself.");
            ui_pause();
            break;
        case '9':
            if (ui_confirm("Throw ALL signals to RED?")) {
                sched_emergency_stop(S);
                ui_warn("All signals at RED. Use option 3 to release overrides when safe.");
                ui_pause();
            }
            break;
        case '0': case 0: case UI_EOF: return;
        default: break;
        }
    }
}

/* ------------------------------------------------------------------ */
/* 7. Simulation                                                       */
/* ------------------------------------------------------------------ */

static void menu_simulation(void)
{
    for (;;) {
        int c, v;
        long since;
        screen("SIMULATION CLOCK");
        view_stats(S);
        ui_section("OPTIONS");
        ui_menu_item('1', "Advance clock by N minutes", "");
        ui_menu_item('2', "Advance clock to a time", "runs every minute up to HH:MM");
        ui_menu_item('3', "Live mode", "real-time running dashboard");
        ui_menu_item('4', "Reset to demo timetable", "discards current state");
        ui_menu_item('0', "Back", "");
        printf("\n");
        c = ui_prompt_choice("Select option");
        switch (c) {
        case '1':
            if (ui_prompt_int("Minutes to advance", 1, 600, &v)) {
                since = S->log.total;
                sched_advance(S, v);
                ui_success("Clock advanced %d min - now %s.", v, util_time_str(S->clock));
                show_new_events(since);
                ui_pause();
            }
            break;
        case '2':
            if (ui_prompt_time("Advance to", &v)) {
                int diff = ((v - S->clock) % MINUTES_PER_DAY + MINUTES_PER_DAY) % MINUTES_PER_DAY;
                if (diff == 0) { ui_info("Clock is already %s.", util_time_str(v)); ui_pause(); break; }
                since = S->log.total;
                sched_advance(S, diff);
                ui_success("Clock advanced %d min - now %s.", diff, util_time_str(S->clock));
                show_new_events(since);
                ui_pause();
            }
            break;
        case '3': run_live(); break;
        case '4':
            if (ui_confirm("Reset everything to the demo timetable?")) {
                demo_load(S);
                ui_success("Demo timetable loaded.");
                ui_pause();
            }
            break;
        case '0': case 0: case UI_EOF: return;
        default: break;
        }
    }
}

/* ------------------------------------------------------------------ */
/* 8. Event log / 9. Files / Help                                      */
/* ------------------------------------------------------------------ */

static void screen_events(void)
{
    screen("EVENT LOG");
    printf("  %sMost recent first - full history in %s%s\n\n", C(GRAY), LOG_FILE, C(RESET));
    view_events(S, 40);
    ui_pause();
}

static void menu_files(void)
{
    for (;;) {
        int c, n;
        screen("SAVE / LOAD / EXPORT");
        ui_section("OPTIONS");
        ui_menu_item('1', "Save session", SAVE_FILE);
        ui_menu_item('2', "Load session", SAVE_FILE);
        ui_menu_item('3', "Export arrivals board", BOARD_FILE);
        ui_menu_item('0', "Back", "");
        printf("\n");
        c = ui_prompt_choice("Select option");
        switch (c) {
        case '1':
            if (storage_save(S, SAVE_FILE)) {
                log_add(&S->log, S->clock, LOG_OK, "Session saved to %s", SAVE_FILE);
                ui_success("Saved %d trains, %d platforms, %d signals to %s.",
                           S->train_count, S->platform_count, S->signal_count, SAVE_FILE);
            } else ui_error("Could not write %s.", SAVE_FILE);
            ui_pause();
            break;
        case '2':
            if (!util_file_exists(SAVE_FILE)) ui_error("No saved session found at %s.", SAVE_FILE);
            else if (ui_confirm("Replace the current state with the saved session?")) {
                if (storage_load(S, SAVE_FILE)) {
                    log_add(&S->log, S->clock, LOG_OK, "Session restored from %s", SAVE_FILE);
                    ui_success("Session loaded - clock %s, %d trains.", util_time_str(S->clock), S->train_count);
                } else ui_error("Save file is invalid or corrupted.");
            }
            ui_pause();
            break;
        case '3':
            n = storage_export_board(S, BOARD_FILE);
            if (n >= 0) ui_success("Exported %d trains to %s.", n, BOARD_FILE);
            else        ui_error("Could not write %s.", BOARD_FILE);
            ui_pause();
            break;
        case '0': case 0: case UI_EOF: return;
        default: break;
        }
    }
}

static void screen_help(void)
{
    screen("HELP & LEGEND");
    view_help();
    ui_pause();
}

/* ------------------------------------------------------------------ */
/* Main menu                                                           */
/* ------------------------------------------------------------------ */

void menu_run(Station *st)
{
    S = st;
    for (;;) {
        int c;
        screen("MAIN MENU");
        view_stats(S);
        ui_section("CONTROL ROOM");
        ui_menu_item('1', "Live Dashboard", "board + track diagram + events");
        ui_menu_item('2', "Arrivals & Departures", "incoming trains board");
        ui_menu_item('3', "Train Management", "add, edit, search, cancel");
        ui_menu_item('4', "Delay Management", "report delays, recover time");
        ui_menu_item('5', "Platform Control", "assign, maintenance, occupancy chart");
        ui_menu_item('6', "Signal Control", "aspects, overrides, emergency stop");
        ui_menu_item('7', "Simulation Clock", "advance time, live mode");
        ui_menu_item('8', "Event Log", "everything that happened");
        ui_menu_item('9', "Save / Load / Export", "");
        ui_menu_item('h', "Help & Legend", "statuses, signals, rules");
        ui_menu_item('0', "Exit", "");
        printf("\n");
        c = ui_prompt_choice("Select option");
        switch (c) {
        case '1': screen_dashboard(); break;
        case '2': screen_board();     break;
        case '3': menu_trains();      break;
        case '4': menu_delays();      break;
        case '5': menu_platforms();   break;
        case '6': menu_signals();     break;
        case '7': menu_simulation();  break;
        case '8': screen_events();    break;
        case '9': menu_files();       break;
        case 'h': screen_help();      break;
        case '0':
        case UI_EOF:
            if (c != UI_EOF && ui_confirm("Save session before exit?")) {
                if (storage_save(S, SAVE_FILE)) ui_success("Session saved to %s.", SAVE_FILE);
                else                            ui_error("Could not save session.");
            }
            printf("\n  %sGoodbye - RailNet control room signing off.%s\n\n", C(CYAN), C(RESET));
            return;
        default: break;
        }
    }
}
