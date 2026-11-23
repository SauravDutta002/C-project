/*
 * main.c - RailNet PMS entry point
 *
 * Railway Platform Management System
 *   - incoming train board with live ETA / delay
 *   - platform allocation with conflict detection
 *   - delay management with automatic re-planning
 *   - signal control with interlocking
 *   - minute-by-minute simulation clock
 */
#include <stdio.h>

#include "config.h"
#include "core/station.h"
#include "core/demo_data.h"
#include "io/storage.h"
#include "ui/ui.h"
#include "ui/views.h"
#include "ui/menu.h"
#include "util/utils.h"

static Station g_station;   /* large struct: keep off the stack */

int main(void)
{
    util_ensure_dir(DATA_DIR);
    ui_init();
    view_splash();

    if (util_file_exists(SAVE_FILE) && ui_confirm("A saved session was found. Restore it?") &&
        storage_load(&g_station, SAVE_FILE)) {
        log_add(&g_station.log, g_station.clock, LOG_OK, "Session restored from %s", SAVE_FILE);
    } else {
        if (!util_file_exists(SAVE_FILE)) ui_pause();
        demo_load(&g_station);
    }

    menu_run(&g_station);
    ui_shutdown();
    return 0;
}
