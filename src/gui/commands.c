/*
 * commands.c - Command handlers and context menus
 */
#include <stdio.h>
#include <string.h>

#include "gui/app.h"
#include "gui/resource.h"
#include "gui/widgets.h"
#include "core/scheduler.h"
#include "core/demo_data.h"
#include "io/storage.h"
#include "util/utils.h"

#include <shellapi.h>

enum {
    POP_DETAILS = 1, POP_EDIT, POP_RECOVER, POP_CLEAR, POP_AUTO, POP_RELEASE, POP_CANCEL, POP_REMOVE,
    POP_DELAY_CUSTOM, POP_SIG_RELEASE, POP_MAINT,
    POP_DELAY_BASE  = 100,   /* + minutes  */
    POP_PF_BASE     = 200,   /* + platform */
    POP_ASPECT_BASE = 300,   /* + aspect   */
    POP_TRAIN_BASE  = 1000   /* + train id */
};

static int info(const char *text, UINT flags)
{
    return MessageBoxA(g_app.hwnd, text, APP_NAME, flags);
}

static int editable(const Train *t)
{
    return t->status == TS_SCHEDULED || t->status == TS_APPROACHING;
}

static int popup(HMENU m, POINT pt)
{
    int cmd;
    SetForegroundWindow(g_app.hwnd);
    cmd = (int)TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_LEFTALIGN, pt.x, pt.y, 0, g_app.hwnd, NULL);
    DestroyMenu(m);
    return cmd;
}

Train *app_current_train(int warn)
{
    HWND lv = NULL;
    int id = 0;
    Train *t;

    if (g_app.page == PAGE_DASHBOARD)   lv = g_app.lv_dash;
    else if (g_app.page == PAGE_TRAINS) lv = g_app.lv_trains;
    else if (g_app.page == PAGE_DELAYS) lv = g_app.lv_delays;
    if (lv) id = (int)lv_selected_key(lv);
    if (!id) id = g_app.sel_train;
    t = id ? station_find_train(&g_app.st, id) : NULL;
    if (!t && warn)
        info("Select a train first.\n\nClick a row in the list, or click a train on the diagram.",
             MB_OK | MB_ICONINFORMATION);
    return t;
}

void show_train_details(Train *t)
{
    char buf[1024];
    char delay[96], pf[64];

    if (t->delay > 0) snprintf(delay, sizeof delay, "%d min late (%s)", t->delay,
                               t->delay_reason[0] ? t->delay_reason : "no reason given");
    else              snprintf(delay, sizeof delay, "On time");
    if (t->platform) {
        Platform *p = station_platform(&g_app.st, t->platform);
        snprintf(pf, sizeof pf, "P%d  (%d-coach platform)", t->platform, p ? p->max_coaches : 0);
    } else snprintf(pf, sizeof pf, "Not assigned");

    snprintf(buf, sizeof buf,
             "%s   %s\n%s train,  priority %d\n\n"
             "Route:\t\t%s  ->  %s\n"
             "Formation:\t%d coaches\n"
             "Scheduled:\tarr %s   dep %s   (halt %d min)\n"
             "Expected:\tarr %s   dep %s\n"
             "Delay:\t\t%s\n"
             "Platform:\t%s\n"
             "Status:\t\t%s",
             t->number, t->name, train_type_name(t->type), train_priority(t->type),
             t->source, t->destination, t->coaches,
             util_time_str(t->sched_arrival), util_time_str(t->sched_departure), train_halt(t),
             util_time_str(train_eta(t)), util_time_str(train_etd(t)),
             delay, pf, train_status_name(t->status));
    MessageBoxA(g_app.hwnd, buf, "Train details", MB_OK | MB_ICONINFORMATION);
}

int confirm_close(void)
{
    int r = info("Save the current session before closing?", MB_YESNOCANCEL | MB_ICONQUESTION);
    if (r == IDCANCEL) return 0;
    if (r == IDYES && !storage_save(&g_app.st, SAVE_FILE)) {
        info("The session could not be saved.", MB_OK | MB_ICONERROR);
        return 0;
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* Actions                                                             */
/* ------------------------------------------------------------------ */

static void assign_to(Train *t, int pf)
{
    AssignResult r = sched_assign(&g_app.st, t->id, pf);
    if (r != ASSIGN_OK) {
        char buf[200];
        snprintf(buf, sizeof buf, "Train %s cannot use platform %d:\n%s.", t->number, pf, assign_result_str(r));
        info(buf, MB_OK | MB_ICONWARNING);
    }
}

static void auto_assign(Train *t)
{
    if (t->status == TS_AT_PLATFORM) return;
    t->platform = 0;
    if (!sched_auto_assign(&g_app.st, t))
        info("No platform is free for this train's time slot.\nTry a different time, or free a platform.",
             MB_OK | MB_ICONWARNING);
}

static void cancel_train(Train *t)
{
    char buf[160];
    snprintf(buf, sizeof buf, "Cancel train %s %s?", t->number, t->name);
    if (info(buf, MB_YESNO | MB_ICONWARNING) == IDYES) sched_cancel(&g_app.st, t->id);
}

static void remove_train(Train *t)
{
    char buf[160], no[TRAIN_NO_LEN];
    if (t->status == TS_AT_PLATFORM) {
        info("This train is standing at a platform.\nIt must depart or be cancelled before it can be removed.",
             MB_OK | MB_ICONWARNING);
        return;
    }
    snprintf(buf, sizeof buf, "Permanently remove train %s %s from the timetable?", t->number, t->name);
    if (info(buf, MB_YESNO | MB_ICONWARNING) != IDYES) return;
    util_copy(no, t->number, sizeof no);
    station_remove_train(&g_app.st, t->id);
    g_app.sel_train = 0;
    log_add(&g_app.st.log, g_app.st.clock, LOG_INFO, "Train %s removed from timetable", no);
    sched_update_signals(&g_app.st);
}

static void add_platform_items(HMENU m, Train *t)
{
    int pf, cid;
    for (pf = 1; pf <= g_app.st.platform_count; pf++) {
        Platform *p = &g_app.st.platforms[pf - 1];
        AssignResult r = sched_check_platform(&g_app.st, t, pf, &cid);
        char label[128];
        UINT flags = MF_STRING;
        if (r == ASSIGN_OK) {
            snprintf(label, sizeof label, "Platform %d   (%d coaches)\tavailable", pf, p->max_coaches);
        } else {
            Train *o = cid ? station_find_train(&g_app.st, cid) : NULL;
            snprintf(label, sizeof label, "Platform %d   (%d coaches)\t%s%s%s", pf, p->max_coaches,
                     assign_result_str(r), o ? " with " : "", o ? o->number : "");
            flags |= MF_GRAYED;
        }
        if (t->platform == pf) flags |= MF_CHECKED;
        AppendMenuA(m, flags, POP_PF_BASE + pf, label);
    }
}

static void platform_choice(Train *t)
{
    HMENU m;
    POINT pt;
    int cmd;
    if (!train_is_active(t) || t->status == TS_AT_PLATFORM) {
        info("Platform can only be changed for trains that have not yet arrived.", MB_OK | MB_ICONINFORMATION);
        return;
    }
    m = CreatePopupMenu();
    add_platform_items(m, t);
    AppendMenuA(m, MF_SEPARATOR, 0, NULL);
    AppendMenuA(m, MF_STRING, POP_AUTO, "Auto-assign best platform");
    GetCursorPos(&pt);
    cmd = popup(m, pt);
    if (cmd > POP_PF_BASE && cmd <= POP_PF_BASE + MAX_PLATFORMS) assign_to(t, cmd - POP_PF_BASE);
    else if (cmd == POP_AUTO) auto_assign(t);
}

/* ------------------------------------------------------------------ */
/* Context menus                                                       */
/* ------------------------------------------------------------------ */

void menu_train(Train *t, POINT pt)
{
    HMENU m = CreatePopupMenu(), dm = CreatePopupMenu(), pm = CreatePopupMenu();
    int active = train_is_active(t), cmd, id = t->id;
    int can_pf = active && t->status != TS_AT_PLATFORM;
    char title[96];
    static const int quick[] = { 5, 10, 15, 30, 60 };
    size_t i;

    snprintf(title, sizeof title, "%s  %s  -  %s", t->number, t->name, train_status_name(t->status));
    AppendMenuA(m, MF_STRING | MF_GRAYED, 0, title);
    AppendMenuA(m, MF_SEPARATOR, 0, NULL);
    AppendMenuA(m, MF_STRING, POP_DETAILS, "Train &details...");
    AppendMenuA(m, MF_STRING | (editable(t) ? 0 : MF_GRAYED), POP_EDIT, "&Edit schedule...");
    AppendMenuA(m, MF_SEPARATOR, 0, NULL);

    for (i = 0; i < sizeof quick / sizeof quick[0]; i++) {
        char lbl[32];
        snprintf(lbl, sizeof lbl, "+%d minutes", quick[i]);
        AppendMenuA(dm, MF_STRING, POP_DELAY_BASE + quick[i], lbl);
    }
    AppendMenuA(dm, MF_SEPARATOR, 0, NULL);
    AppendMenuA(dm, MF_STRING, POP_DELAY_CUSTOM, "Custom delay with reason...");
    AppendMenuA(m, MF_POPUP | (active ? 0 : MF_GRAYED), (UINT_PTR)dm, "Report &delay");
    AppendMenuA(m, MF_STRING | (active && t->delay > 0 ? 0 : MF_GRAYED), POP_RECOVER, "&Recover time...");
    AppendMenuA(m, MF_STRING | (active && t->delay > 0 ? 0 : MF_GRAYED), POP_CLEAR, "C&lear delay");
    AppendMenuA(m, MF_SEPARATOR, 0, NULL);

    if (can_pf) add_platform_items(pm, t);
    AppendMenuA(m, MF_POPUP | (can_pf ? 0 : MF_GRAYED), (UINT_PTR)pm, "Assign &platform");
    AppendMenuA(m, MF_STRING | (can_pf ? 0 : MF_GRAYED), POP_AUTO, "&Auto-assign best platform");
    AppendMenuA(m, MF_STRING | (can_pf && t->platform ? 0 : MF_GRAYED), POP_RELEASE, "Release platform &booking");
    AppendMenuA(m, MF_SEPARATOR, 0, NULL);
    AppendMenuA(m, MF_STRING | (active ? 0 : MF_GRAYED), POP_CANCEL, "&Cancel train");
    AppendMenuA(m, MF_STRING | (t->status != TS_AT_PLATFORM ? 0 : MF_GRAYED), POP_REMOVE, "Re&move from timetable");

    cmd = popup(m, pt);
    t = station_find_train(&g_app.st, id);     /* re-fetch: the timer may have run */
    if (!t || !cmd) { app_refresh(); return; }

    if (cmd == POP_DETAILS)                   show_train_details(t);
    else if (cmd == POP_EDIT)                 dlg_train(t);
    else if (cmd == POP_DELAY_CUSTOM)         dlg_delay(t, 0);
    else if (cmd == POP_RECOVER)              dlg_delay(t, 1);
    else if (cmd == POP_CLEAR)                sched_delay(&g_app.st, t->id, -t->delay, NULL);
    else if (cmd == POP_AUTO)                 auto_assign(t);
    else if (cmd == POP_RELEASE)              sched_release(&g_app.st, t->id);
    else if (cmd == POP_CANCEL)               cancel_train(t);
    else if (cmd == POP_REMOVE)               remove_train(t);
    else if (cmd > POP_PF_BASE && cmd <= POP_PF_BASE + MAX_PLATFORMS) assign_to(t, cmd - POP_PF_BASE);
    else if (cmd > POP_DELAY_BASE && cmd < POP_PF_BASE)
        sched_delay(&g_app.st, t->id, cmd - POP_DELAY_BASE, t->delay_reason[0] ? t->delay_reason : "Late running");
    app_refresh();
}

void menu_signal(RailSignal *s, POINT pt)
{
    HMENU m = CreatePopupMenu();
    char title[96];
    int a, cmd, idx = (int)(s - g_app.st.signals);

    if (s->platform)
        snprintf(title, sizeof title, "Signal %s  -  %s, platform %d", s->code, signal_kind_name(s->kind), s->platform);
    else
        snprintf(title, sizeof title, "Signal %s  -  %s", s->code, signal_kind_name(s->kind));
    AppendMenuA(m, MF_STRING | MF_GRAYED, 0, title);
    AppendMenuA(m, MF_SEPARATOR, 0, NULL);
    for (a = 0; a < SA_COUNT; a++) {
        char lbl[96];
        snprintf(lbl, sizeof lbl, "Set %s\t%s", signal_aspect_name((SignalAspect)a),
                 signal_aspect_meaning((SignalAspect)a));
        AppendMenuA(m, MF_STRING | (s->aspect == (SignalAspect)a ? MF_CHECKED : 0), POP_ASPECT_BASE + a, lbl);
    }
    AppendMenuA(m, MF_SEPARATOR, 0, NULL);
    AppendMenuA(m, MF_STRING | (s->manual ? 0 : MF_GRAYED), POP_SIG_RELEASE, "Return to &automatic control");

    cmd = popup(m, pt);
    s = &g_app.st.signals[idx];
    if (cmd >= POP_ASPECT_BASE && cmd < POP_ASPECT_BASE + SA_COUNT) {
        const char *why = NULL;
        if (!sched_set_signal(&g_app.st, s, (SignalAspect)(cmd - POP_ASPECT_BASE), &why)) {
            char buf[200];
            snprintf(buf, sizeof buf, "Signal %s cannot be cleared.\n\n%s.", s->code, why ? why : "Refused");
            info(buf, MB_OK | MB_ICONWARNING);
        }
    } else if (cmd == POP_SIG_RELEASE) {
        sched_release_signal(&g_app.st, s);
    }
    app_refresh();
}

void menu_platform(Platform *p, POINT pt)
{
    HMENU m = CreatePopupMenu(), tm = CreatePopupMenu();
    char title[96];
    int i, cmd, eligible = 0, pf = p->number;

    snprintf(title, sizeof title, "Platform %d  -  %d coaches  -  %s", p->number, p->max_coaches,
             platform_state_name(p->state));
    AppendMenuA(m, MF_STRING | MF_GRAYED, 0, title);
    AppendMenuA(m, MF_SEPARATOR, 0, NULL);
    if (p->state == PS_MAINTENANCE)
        AppendMenuA(m, MF_STRING, POP_MAINT, "Return platform to &service");
    else
        AppendMenuA(m, MF_STRING | (p->state == PS_OCCUPIED ? MF_GRAYED : 0), POP_MAINT,
                    "Take out of service for &maintenance");

    for (i = 0; i < g_app.st.train_count; i++) {
        Train *t = &g_app.st.trains[i];
        char lbl[128];
        if (!train_is_active(t) || t->status == TS_AT_PLATFORM || t->platform == pf) continue;
        if (sched_check_platform(&g_app.st, t, pf, NULL) != ASSIGN_OK) continue;
        snprintf(lbl, sizeof lbl, "%s  %s\tETA %s%s", t->number, t->name, util_time_str(train_eta(t)),
                 t->platform ? "" : "  (unassigned)");
        AppendMenuA(tm, MF_STRING, POP_TRAIN_BASE + t->id, lbl);
        eligible++;
    }
    if (!eligible) AppendMenuA(tm, MF_STRING | MF_GRAYED, 0, "No eligible trains");
    AppendMenuA(m, MF_POPUP | (p->state == PS_MAINTENANCE ? MF_GRAYED : 0), (UINT_PTR)tm, "&Assign a train here");

    cmd = popup(m, pt);
    if (cmd == POP_MAINT) {
        Platform *cur = station_platform(&g_app.st, pf);
        if (cur->state == PS_MAINTENANCE) {
            sched_set_maintenance(&g_app.st, pf, 0, NULL);
        } else {
            int moved = 0;
            char buf[200];
            snprintf(buf, sizeof buf, "Take platform %d out of service?\n\nTrains booked on it will be moved "
                     "to other platforms where possible.", pf);
            if (info(buf, MB_YESNO | MB_ICONQUESTION) == IDYES) {
                if (!sched_set_maintenance(&g_app.st, pf, 1, &moved))
                    info("The platform is occupied - wait for the train to depart.", MB_OK | MB_ICONWARNING);
            }
        }
    } else if (cmd >= POP_TRAIN_BASE) {
        Train *t = station_find_train(&g_app.st, cmd - POP_TRAIN_BASE);
        if (t) assign_to(t, pf);
    }
    app_refresh();
}

/* ------------------------------------------------------------------ */
/* Command dispatcher                                                  */
/* ------------------------------------------------------------------ */

static void open_path(const char *path)
{
    ShellExecuteA(g_app.hwnd, "open", path, NULL, NULL, SW_SHOWNORMAL);
}

void cmd_handle(int id)
{
    Station *st = &g_app.st;
    Train *t;
    char buf[256];

    switch (id) {
    case IDM_SAVE:
        if (storage_save(st, SAVE_FILE)) {
            log_add(&st->log, st->clock, LOG_OK, "Session saved to %s", SAVE_FILE);
            app_refresh();
            app_status("Session saved to data\\station.dat");
            return;
        }
        info("Could not write data\\station.dat.", MB_OK | MB_ICONERROR);
        break;

    case IDM_LOAD:
        if (!util_file_exists(SAVE_FILE)) { info("No saved session found yet. Use File > Save first.", MB_OK | MB_ICONINFORMATION); break; }
        if (info("Replace the current state with the saved session?", MB_YESNO | MB_ICONQUESTION) != IDYES) break;
        if (storage_load(st, SAVE_FILE)) {
            g_app.sel_train = 0;
            log_add(&st->log, st->clock, LOG_OK, "Session restored from %s", SAVE_FILE);
            app_layout();
        } else info("The saved session is invalid or corrupted.", MB_OK | MB_ICONERROR);
        break;

    case IDM_EXPORT: {
        int n = storage_export_board(st, BOARD_FILE);
        if (n < 0) { info("Could not write data\\board.txt.", MB_OK | MB_ICONERROR); break; }
        snprintf(buf, sizeof buf, "Exported %d trains to data\\board.txt.\n\nOpen it now?", n);
        if (info(buf, MB_YESNO | MB_ICONINFORMATION) == IDYES) open_path(BOARD_FILE);
        break;
    }

    case IDM_OPEN_DATA:
        open_path(DATA_DIR);
        break;

    case IDM_RESET:
        if (info("Reset everything to the demo timetable?\nUnsaved changes will be lost.",
                 MB_YESNO | MB_ICONWARNING) == IDYES) {
            g_app.running = 0;
            g_app.sel_train = 0;
            demo_load(st);
            app_layout();
        }
        break;

    case IDM_EXIT:
        PostMessageA(g_app.hwnd, WM_CLOSE, 0, 0);
        break;

    case IDM_RUN:     g_app.running = !g_app.running; break;
    case IDM_STEP1:   sched_advance(st, 1); break;
    case IDM_STEP5:   sched_advance(st, 5); break;
    case IDM_STEP15:  sched_advance(st, 15); break;
    case IDM_SPEED1:  g_app.speed = 1; break;
    case IDM_SPEED2:  g_app.speed = 2; break;
    case IDM_SPEED5:  g_app.speed = 5; break;
    case IDM_SPEED10: g_app.speed = 10; break;

    case IDM_AUTOSIG:     sched_toggle_auto_signals(st); break;
    case IDM_RELEASE_ALL: sched_release_all_signals(st); break;
    case IDM_EMERGENCY:
        if (info("EMERGENCY STOP\n\nThrow every signal to RED? All train movements will stop.",
                 MB_YESNO | MB_ICONEXCLAMATION) == IDYES)
            sched_emergency_stop(st);
        break;

    case IDM_AUTOPF:
        st->auto_assign = !st->auto_assign;
        log_add(&st->log, st->clock, LOG_INFO, "Automatic platform allocation %s", st->auto_assign ? "ON" : "OFF");
        break;

    case IDM_AUTOASSIGN_ALL: {
        int failed = 0, ok = sched_auto_assign_all(st, &failed);
        if (!ok && !failed) snprintf(buf, sizeof buf, "Every active train already has a platform.");
        else snprintf(buf, sizeof buf, "%d train(s) were given a platform.%s", ok,
                      failed ? "\n\nSome trains could not be placed - check maintenance or delays in the Event Log." : "");
        app_refresh();
        info(buf, MB_OK | (failed ? MB_ICONWARNING : MB_ICONINFORMATION));
        break;
    }

    case IDM_ADD_TRAIN:
        dlg_train(NULL);
        break;

    case IDM_EDIT_TRAIN:
        if ((t = app_current_train(1)) != NULL) {
            if (editable(t)) dlg_train(t);
            else info("Only trains that have not yet arrived can be edited.", MB_OK | MB_ICONINFORMATION);
        }
        break;

    case IDM_DETAILS:
        if ((t = app_current_train(1)) != NULL) show_train_details(t);
        break;

    case IDM_DELAY_TRAIN:
        if ((t = app_current_train(1)) != NULL) {
            if (train_is_active(t)) dlg_delay(t, 0);
            else info("This train has already departed or is cancelled.", MB_OK | MB_ICONINFORMATION);
        }
        break;

    case IDM_RECOVER_TRAIN:
        if ((t = app_current_train(1)) != NULL) {
            if (train_is_active(t) && t->delay > 0) dlg_delay(t, 1);
            else info("This train is running on time.", MB_OK | MB_ICONINFORMATION);
        }
        break;

    case IDM_CLEAR_DELAY:
        if ((t = app_current_train(1)) != NULL) {
            if (train_is_active(t) && t->delay > 0) sched_delay(st, t->id, -t->delay, NULL);
            else info("This train is running on time.", MB_OK | MB_ICONINFORMATION);
        }
        break;

    case IDM_ASSIGN_TRAIN:
        if ((t = app_current_train(1)) != NULL) platform_choice(t);
        break;

    case IDM_CANCEL_TRAIN:
        if ((t = app_current_train(1)) != NULL) {
            if (train_is_active(t)) cancel_train(t);
            else info("This train is not active.", MB_OK | MB_ICONINFORMATION);
        }
        break;

    case IDM_REMOVE_TRAIN:
        if ((t = app_current_train(1)) != NULL) remove_train(t);
        break;

    case IDM_LEGEND:
        MessageBoxA(g_app.hwnd,
            "TRAIN STATUS\n"
            "  Scheduled  -  in the timetable, more than 15 minutes away\n"
            "  Approaching  -  within 15 minutes; a platform is allocated now\n"
            "  Held at signal  -  waiting at the outer signal (no route / platform busy)\n"
            "  On platform  -  standing at its platform\n"
            "  Departed / Cancelled\n\n"
            "SIGNALS\n"
            "  OUT-A  outer signal guarding the approach\n"
            "  H<n>  home signal admitting a train to platform n\n"
            "  S<n>  starter signal letting a train leave platform n\n\n"
            "  Red = stop,  Yellow = caution (route set),\n"
            "  Double yellow = attention,  Green = proceed\n\n"
            "OPERATING RULES\n"
            "  - A home signal cannot clear onto an occupied or closed platform.\n"
            "  - A starter signal cannot clear unless a train is standing there.\n"
            "  - Two trains on one platform need a 5-minute gap.\n"
            "  - Delays that cause a clash are re-planned automatically.\n"
            "  - Automatic allocation picks the shortest platform that fits,\n"
            "    giving higher-priority trains first choice.\n"
            "  - Each minute a train waits at a red signal adds 1 minute of delay.\n\n"
            "TIPS\n"
            "  Right-click any train, signal or platform for actions.\n"
            "  F5 runs/pauses the simulation; F6/F7/F8 step 1/5/15 minutes.",
            "Legend & operating rules", MB_OK | MB_ICONINFORMATION);
        break;

    case IDM_ABOUT:
        MessageBoxA(g_app.hwnd,
            APP_NAME " " APP_VERSION "\nRailway Platform Management System\n\n"
            "Train tracking, platform allocation, delay management\n"
            "and signal interlocking with a live station simulation.\n\n"
            "Written in C using the Win32 API.",
            "About " APP_NAME, MB_OK | MB_ICONINFORMATION);
        break;

    case IDM_PAGE_DASH:      app_set_page(PAGE_DASHBOARD); return;
    case IDM_PAGE_TRAINS:    app_set_page(PAGE_TRAINS); return;
    case IDM_PAGE_PLATFORMS: app_set_page(PAGE_PLATFORMS); return;
    case IDM_PAGE_SIGNALS:   app_set_page(PAGE_SIGNALS); return;
    case IDM_PAGE_DELAYS:    app_set_page(PAGE_DELAYS); return;
    case IDM_PAGE_LOG:       app_set_page(PAGE_LOG); return;

    default:
        return;
    }
    app_refresh();
}
