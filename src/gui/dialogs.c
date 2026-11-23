/*
 * dialogs.c - Add/Edit train and Delay dialogs
 */
#include <stdio.h>
#include <string.h>

#include "gui/app.h"
#include "gui/resource.h"
#include "core/scheduler.h"
#include "util/utils.h"

static const char *REASONS[] = {
    "Late running", "Signal failure", "Technical fault", "Crew change",
    "Bad weather", "Track maintenance", "Heavy passenger load", "Late incoming rake"
};
#define REASON_COUNT ((int)(sizeof REASONS / sizeof REASONS[0]))

/* Pause the simulation clock while a modal dialog is open */
static int hold_sim(void)
{
    int was = g_app.running;
    if (was) KillTimer(g_app.hwnd, 1);
    return was;
}

static void resume_sim(int was)
{
    if (was && g_app.running) SetTimer(g_app.hwnd, 1, (UINT)(1000 / (g_app.speed ? g_app.speed : 1)), NULL);
}

static int warn(HWND d, int focus_id, const char *msg)
{
    MessageBoxA(d, msg, APP_NAME, MB_OK | MB_ICONWARNING);
    if (focus_id) SetFocus(GetDlgItem(d, focus_id));
    return 0;
}

static void set_time_ctl(HWND d, int id, int minutes)
{
    SYSTEMTIME s;
    GetLocalTime(&s);
    minutes = ((minutes % MINUTES_PER_DAY) + MINUTES_PER_DAY) % MINUTES_PER_DAY;
    s.wHour = (WORD)(minutes / 60);
    s.wMinute = (WORD)(minutes % 60);
    s.wSecond = 0;
    s.wMilliseconds = 0;
    SendDlgItemMessageA(d, id, DTM_SETFORMATA, 0, (LPARAM)"HH':'mm");
    SendDlgItemMessageA(d, id, DTM_SETSYSTEMTIME, GDT_VALID, (LPARAM)&s);
}

static int get_time_ctl(HWND d, int id)
{
    SYSTEMTIME s;
    if (SendDlgItemMessageA(d, id, DTM_GETSYSTEMTIME, 0, (LPARAM)&s) != GDT_VALID) return -1;
    return s.wHour * 60 + s.wMinute;
}

static int get_text(HWND d, int id, char *buf, int size)
{
    GetDlgItemTextA(d, id, buf, size);
    util_trim(buf);
    util_sanitize(buf);
    return buf[0] != '\0';
}

/* ------------------------------------------------------------------ */
/* Train dialog                                                        */
/* ------------------------------------------------------------------ */

static Train *g_edit;

static void train_init(HWND d)
{
    Station *st = &g_app.st;
    int i, arr, dep, coaches = 18, type = TT_EXPRESS;
    char buf[64];

    SetWindowTextA(d, g_edit ? "Edit train schedule" : "Add new train");
    for (i = 0; i < TT_COUNT; i++) SendDlgItemMessageA(d, IDC_T_TYPE, CB_ADDSTRING, 0, (LPARAM)train_type_name((TrainType)i));
    SendDlgItemMessageA(d, IDC_T_NUMBER, EM_LIMITTEXT, 6, 0);
    SendDlgItemMessageA(d, IDC_T_NAME, EM_LIMITTEXT, NAME_LEN - 1, 0);
    SendDlgItemMessageA(d, IDC_T_FROM, EM_LIMITTEXT, CITY_LEN - 1, 0);
    SendDlgItemMessageA(d, IDC_T_TO, EM_LIMITTEXT, CITY_LEN - 1, 0);
    SendDlgItemMessageA(d, IDC_T_COACHES_SPIN, UDM_SETRANGE, 0, MAKELPARAM(MAX_COACHES, 1));

    arr = st->clock + 30;
    arr += (5 - arr % 5) % 5;
    dep = arr + 10;

    if (g_edit) {
        Train *t = g_edit;
        SetDlgItemTextA(d, IDC_T_NUMBER, t->number);
        EnableWindow(GetDlgItem(d, IDC_T_NUMBER), FALSE);
        SetDlgItemTextA(d, IDC_T_NAME, t->name);
        SetDlgItemTextA(d, IDC_T_FROM, t->source);
        SetDlgItemTextA(d, IDC_T_TO, t->destination);
        coaches = t->coaches;
        type = t->type;
        arr = t->sched_arrival;
        dep = t->sched_departure;
        if (t->platform) snprintf(buf, sizeof buf, "Keep current (platform %d)", t->platform);
        else             snprintf(buf, sizeof buf, "Keep current (not assigned)");
        SendDlgItemMessageA(d, IDC_T_PLATFORM, CB_ADDSTRING, 0, (LPARAM)buf);
        SendDlgItemMessageA(d, IDC_T_PLATFORM, CB_ADDSTRING, 0, (LPARAM)"Auto-assign best platform");
        SendDlgItemMessageA(d, IDC_T_PLATFORM, CB_ADDSTRING, 0, (LPARAM)"Unassign");
        SetDlgItemTextA(d, IDC_T_HINT, "Changing times or formation re-checks the platform for clashes; "
                                       "the train is moved automatically if needed.");
    } else {
        SendDlgItemMessageA(d, IDC_T_PLATFORM, CB_ADDSTRING, 0, (LPARAM)"Auto-assign best platform");
        SendDlgItemMessageA(d, IDC_T_PLATFORM, CB_ADDSTRING, 0, (LPARAM)"Assign later");
        SetDlgItemTextA(d, IDC_T_HINT, "Times are 24-hour station time. The halt must be between 1 and 180 minutes.");
    }
    for (i = 0; i < st->platform_count; i++) {
        snprintf(buf, sizeof buf, "Platform %d  (%d coaches)", i + 1, st->platforms[i].max_coaches);
        SendDlgItemMessageA(d, IDC_T_PLATFORM, CB_ADDSTRING, 0, (LPARAM)buf);
    }
    SendDlgItemMessageA(d, IDC_T_PLATFORM, CB_SETCURSEL, 0, 0);
    SendDlgItemMessageA(d, IDC_T_TYPE, CB_SETCURSEL, (WPARAM)type, 0);
    SetDlgItemInt(d, IDC_T_COACHES, (UINT)coaches, FALSE);
    set_time_ctl(d, IDC_T_ARR, arr);
    set_time_ctl(d, IDC_T_DEP, dep);
}

static int train_apply(HWND d)
{
    Station *st = &g_app.st;
    Train tmp;
    BOOL ok;
    int coaches, arr, dep, type, choice, first_pf;
    char buf[200];

    memset(&tmp, 0, sizeof tmp);
    if (!g_edit) {
        if (!get_text(d, IDC_T_NUMBER, tmp.number, sizeof tmp.number) || !train_valid_number(tmp.number))
            return warn(d, IDC_T_NUMBER, "The train number must be 4 to 6 digits.");
        if (station_find_train_by_number(st, tmp.number))
            return warn(d, IDC_T_NUMBER, "A train with this number already exists.");
    }
    if (!get_text(d, IDC_T_NAME, tmp.name, sizeof tmp.name))         return warn(d, IDC_T_NAME, "Please enter the train name.");
    if (!get_text(d, IDC_T_FROM, tmp.source, sizeof tmp.source))     return warn(d, IDC_T_FROM, "Please enter the origin station.");
    if (!get_text(d, IDC_T_TO, tmp.destination, sizeof tmp.destination)) return warn(d, IDC_T_TO, "Please enter the destination.");

    coaches = (int)GetDlgItemInt(d, IDC_T_COACHES, &ok, FALSE);
    if (!ok || coaches < 1 || coaches > MAX_COACHES) {
        snprintf(buf, sizeof buf, "Coaches must be between 1 and %d.", MAX_COACHES);
        return warn(d, IDC_T_COACHES, buf);
    }
    arr = get_time_ctl(d, IDC_T_ARR);
    dep = get_time_ctl(d, IDC_T_DEP);
    if (arr < 0 || dep < 0) return warn(d, IDC_T_ARR, "Please enter valid times.");
    if (dep - arr < 1 || dep - arr > MAX_HALT)
        return warn(d, IDC_T_DEP, "Departure must be 1 to 180 minutes after arrival (same day).");

    type = (int)SendDlgItemMessageA(d, IDC_T_TYPE, CB_GETCURSEL, 0, 0);
    if (type < 0) type = TT_EXPRESS;
    choice = (int)SendDlgItemMessageA(d, IDC_T_PLATFORM, CB_GETCURSEL, 0, 0);

    if (!g_edit) {
        Train *t;
        int id;
        tmp.type = (TrainType)type;
        tmp.status = TS_SCHEDULED;
        tmp.coaches = coaches;
        tmp.sched_arrival = arr;
        tmp.sched_departure = dep;
        id = station_add_train(st, &tmp);
        t = station_find_train(st, id);
        if (!t) return warn(d, 0, "The timetable is full.");
        g_app.sel_train = t->id;
        log_add(&st->log, st->clock, LOG_INFO, "New train %s %s added (arr %s, dep %s)", t->number, t->name,
                util_time_str(arr), util_time_str(dep));

        first_pf = 2;
        if (choice == 0) {
            if (!sched_auto_assign(st, t))
                MessageBoxA(d, "The train was added, but no platform is free for its time slot.\n"
                               "Assign one later from the Platforms page.", APP_NAME, MB_OK | MB_ICONINFORMATION);
        } else if (choice >= first_pf) {
            AssignResult r = sched_assign(st, t->id, choice - first_pf + 1);
            if (r != ASSIGN_OK) {
                snprintf(buf, sizeof buf, "The train was added, but platform %d is not available:\n%s.",
                         choice - first_pf + 1, assign_result_str(r));
                MessageBoxA(d, buf, APP_NAME, MB_OK | MB_ICONINFORMATION);
            }
        }
    } else {
        Train *t = g_edit;
        int old_pf = t->platform;
        if (t->status != TS_SCHEDULED && t->status != TS_APPROACHING)
            return warn(d, 0, "This train has already arrived and can no longer be edited.");

        util_copy(t->name, tmp.name, sizeof t->name);
        util_copy(t->source, tmp.source, sizeof t->source);
        util_copy(t->destination, tmp.destination, sizeof t->destination);
        t->type = (TrainType)type;
        t->coaches = coaches;
        t->sched_arrival = arr;
        t->sched_departure = dep;
        if (t->status == TS_APPROACHING && train_eta(t) - st->clock > APPROACH_WINDOW) t->status = TS_SCHEDULED;
        log_add(&st->log, st->clock, LOG_INFO, "%s schedule edited: arr %s dep %s, %d coaches", t->number,
                util_time_str(arr), util_time_str(dep), coaches);

        first_pf = 3;
        if (choice == 0) {
            sched_revalidate(st, t);
        } else if (choice == 1) {
            t->platform = 0;
            sched_auto_assign(st, t);
        } else if (choice == 2) {
            if (t->platform) sched_release(st, t->id);
        } else {
            AssignResult r = sched_assign(st, t->id, choice - first_pf + 1);
            if (r != ASSIGN_OK) {
                snprintf(buf, sizeof buf, "Schedule saved, but platform %d is not available:\n%s.",
                         choice - first_pf + 1, assign_result_str(r));
                MessageBoxA(d, buf, APP_NAME, MB_OK | MB_ICONINFORMATION);
                sched_revalidate(st, t);
            }
        }
        if (choice == 0 && old_pf != t->platform) {
            if (t->platform) snprintf(buf, sizeof buf, "To avoid a clash the train was moved from platform %d to platform %d.", old_pf, t->platform);
            else             snprintf(buf, sizeof buf, "The train lost platform %d and no alternative is free.\nAssign one manually.", old_pf);
            MessageBoxA(d, buf, APP_NAME, MB_OK | MB_ICONINFORMATION);
        }
    }
    sched_update_signals(st);
    return 1;
}

static INT_PTR CALLBACK train_proc(HWND d, UINT msg, WPARAM w, LPARAM l)
{
    (void)l;
    switch (msg) {
    case WM_INITDIALOG:
        train_init(d);
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(w) == IDOK) {
            if (train_apply(d)) EndDialog(d, 1);
            return TRUE;
        }
        if (LOWORD(w) == IDCANCEL) {
            EndDialog(d, 0);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

int dlg_train(Train *edit)
{
    int was = hold_sim(), r;
    g_edit = edit;
    r = (int)DialogBoxParamA(g_app.inst, MAKEINTRESOURCEA(IDD_TRAIN), g_app.hwnd, train_proc, 0);
    resume_sim(was);
    app_refresh();
    return r == 1;
}

/* ------------------------------------------------------------------ */
/* Delay dialog                                                        */
/* ------------------------------------------------------------------ */

static Train *g_dtrain;
static int g_drecover;

static void delay_preview(HWND d)
{
    Train *t = g_dtrain;
    BOOL ok;
    int m = (int)GetDlgItemInt(d, IDC_D_MIN, &ok, FALSE);
    int add = IsDlgButtonChecked(d, IDC_D_ADD) == BST_CHECKED;
    int nd;
    char buf[160];

    EnableWindow(GetDlgItem(d, IDC_D_REASON), add);
    if (!ok) { SetDlgItemTextA(d, IDC_D_PREVIEW, "Enter the number of minutes."); return; }
    nd = add ? t->delay + m : (t->delay - m < 0 ? 0 : t->delay - m);
    if (t->status == TS_AT_PLATFORM)
        snprintf(buf, sizeof buf, "New departure %s   \xB7   total delay %d min",
                 util_time_str(t->sched_departure + nd), nd);
    else
        snprintf(buf, sizeof buf, "New arrival %s   \xB7   departure %s   \xB7   total delay %d min",
                 util_time_str(t->sched_arrival + nd), util_time_str(t->sched_departure + nd), nd);
    SetDlgItemTextA(d, IDC_D_PREVIEW, buf);
}

static void delay_init(HWND d)
{
    Train *t = g_dtrain;
    char buf[200];
    int i;

    SetWindowTextA(d, g_drecover ? "Recover time" : "Report delay");
    snprintf(buf, sizeof buf, "%s  %s\nScheduled arr %s / dep %s   \xB7   currently %s",
             t->number, t->name, util_time_str(t->sched_arrival), util_time_str(t->sched_departure),
             t->delay ? "" : "on time");
    if (t->delay) {
        size_t len = strlen(buf);
        snprintf(buf + len, sizeof buf - len, "%d min late", t->delay);
    }
    SetDlgItemTextA(d, IDC_D_TRAIN, buf);

    CheckRadioButton(d, IDC_D_ADD, IDC_D_RECOVER, g_drecover ? IDC_D_RECOVER : IDC_D_ADD);
    EnableWindow(GetDlgItem(d, IDC_D_RECOVER), t->delay > 0);
    SendDlgItemMessageA(d, IDC_D_SPIN, UDM_SETRANGE, 0, MAKELPARAM(MAX_DELAY, 1));
    SetDlgItemInt(d, IDC_D_MIN, (UINT)(g_drecover ? t->delay : 10), FALSE);

    for (i = 0; i < REASON_COUNT; i++) SendDlgItemMessageA(d, IDC_D_REASON, CB_ADDSTRING, 0, (LPARAM)REASONS[i]);
    SetDlgItemTextA(d, IDC_D_REASON, t->delay_reason[0] ? t->delay_reason : REASONS[0]);
    delay_preview(d);
}

static int delay_apply(HWND d)
{
    Train *t = g_dtrain;
    BOOL ok;
    int m = (int)GetDlgItemInt(d, IDC_D_MIN, &ok, FALSE);
    int add = IsDlgButtonChecked(d, IDC_D_ADD) == BST_CHECKED;
    int old_pf = t->platform;
    char reason[NAME_LEN], buf[200];

    if (!ok || m < 1 || m > MAX_DELAY) return warn(d, IDC_D_MIN, "Enter between 1 and 720 minutes.");
    if (!train_is_active(t)) return warn(d, 0, "This train is no longer active.");

    if (add) {
        if (!get_text(d, IDC_D_REASON, reason, sizeof reason)) util_copy(reason, "Unspecified", sizeof reason);
        sched_delay(&g_app.st, t->id, m, reason);
    } else {
        if (t->delay == 0) return warn(d, 0, "This train is already on time.");
        sched_delay(&g_app.st, t->id, -(m > t->delay ? t->delay : m), NULL);
    }

    if (old_pf != t->platform) {
        if (t->platform) snprintf(buf, sizeof buf, "Because of the new timing, train %s was re-planned "
                                  "from platform %d to platform %d.", t->number, old_pf, t->platform);
        else             snprintf(buf, sizeof buf, "Train %s lost platform %d and no alternative is free.\n"
                                  "Assign a platform manually.", t->number, old_pf);
        MessageBoxA(d, buf, "Platform re-planned", MB_OK | MB_ICONINFORMATION);
    }
    return 1;
}

static INT_PTR CALLBACK delay_proc(HWND d, UINT msg, WPARAM w, LPARAM l)
{
    (void)l;
    switch (msg) {
    case WM_INITDIALOG:
        delay_init(d);
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(w)) {
        case IDOK:
            if (delay_apply(d)) EndDialog(d, 1);
            return TRUE;
        case IDCANCEL:
            EndDialog(d, 0);
            return TRUE;
        case IDC_D_ADD:
        case IDC_D_RECOVER:
            if (HIWORD(w) == BN_CLICKED) delay_preview(d);
            return TRUE;
        case IDC_D_MIN:
            if (HIWORD(w) == EN_CHANGE) delay_preview(d);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

int dlg_delay(Train *t, int recover)
{
    int was = hold_sim(), r;
    g_dtrain = t;
    g_drecover = recover;
    r = (int)DialogBoxParamA(g_app.inst, MAKEINTRESOURCEA(IDD_DELAY), g_app.hwnd, delay_proc, 0);
    resume_sim(was);
    app_refresh();
    return r == 1;
}
