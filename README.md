# RailNet PMS — Railway Platform Management System (C)

A Windows desktop application written in pure C (C99 + Win32 API) for running a railway
station control room: incoming trains, platform allocation, delays, signals with
interlocking, and a live minute-by-minute simulation.

A console (terminal) edition built on the same engine is included as well.

## Features

| Page | What you can do |
|------|-----------------|
| **Dashboard** | KPI cards, a live **station mimic panel** (track diagram with signal lamps, trains on the approach, trains on platforms), incoming trains table, recent events |
| **Trains** | Timetable with colour-coded status, search, add / edit / cancel / remove, double-click to edit, right-click for every action |
| **Platforms** | Platform cards (occupant, next arrival, signal lamps), **occupancy (Gantt) chart** for 3.5 hours, maintenance, best-fit auto-allocation |
| **Signals** | Signal board with 4-lamp signal heads; click a signal to set its aspect; automatic / manual signalling; emergency all-red |
| **Delays** | Punctuality KPIs, trains by delay, delay bar chart, report / recover / clear delay |
| **Event Log** | Every operation and movement, colour-coded, plus export of the arrivals board |

Also included:
- **Simulation:** run / pause (F5), speed x1–x10, step 1 / 5 / 15 minutes.
- **Dialogs:** native dialogs with validation, a time picker and a live delay preview.
- **Context menus:** right-click any train, signal or platform (in a table *or* on the diagrams).
- **Persistence:** save / restore the session, export the arrivals board, and an event log file.
- **Look:** high-DPI aware, modern visual styles, custom app icon and version info.

### Operating rules (enforced by the engine)
- Trains become **Approaching** 15 minutes before arrival, and a platform is allocated then.
- A train enters only if its platform is free and its home signal is not red. Otherwise it is
  **Held at signal** and loses 1 minute for every minute it waits.
- Two trains on one platform need a 5-minute gap. A delay that causes a clash re-plans the platform automatically.
- **Interlocking:**
  - A home signal cannot clear onto an occupied or closed platform.
  - A starter signal cannot clear unless a train is standing at the platform.

## Project layout

```
C project/
├── include/                    header files (public interfaces)
│   ├── config.h                global constants
│   ├── core/                   domain model + engine (UI independent)
│   │   ├── train.h  platform.h  signal_ctrl.h  station.h
│   │   ├── scheduler.h         allocation, delays, interlocking, simulation
│   │   └── demo_data.h
│   ├── io/                     logger.h, storage.h
│   ├── util/                   utils.h
│   ├── gui/                    desktop application (Win32)
│   │   ├── win.h               Windows include + compatibility
│   │   ├── resource.h          resource / command IDs
│   │   ├── app.h               main window state
│   │   ├── theme.h             colours, fonts, drawing primitives
│   │   └── widgets.h           flat buttons, tables, click regions
│   └── ui/                     console edition (ui.h, views.h, menu.h)
├── src/
│   ├── core/  io/  util/       engine implementation (shared)
│   ├── gui/                    winmain.c app.c commands.c dialogs.c
│   │                           panels.c mimic.c charts.c theme.c widgets.c
│   ├── ui/                     console edition
│   └── main.c                  console entry point
├── resources/                  app.rc (dialogs, menus, version), app.manifest, app.ico
├── tools/make_icon.ps1         regenerates the icon
├── data/                       runtime files: station.dat, events.log, board.txt
├── Makefile
└── build.bat
```

## Build & run (Windows, MinGW)

```bat
build.bat          :: builds build\RailNetPMS.exe (desktop) and build\railnet_console.exe
build.bat run      :: build and launch the desktop app
```

or with make:

```bat
mingw32-make            :: both apps
mingw32-make run        :: build and launch the desktop app
```

You can also double-click `build\RailNetPMS.exe`. It always uses the project's `data\` folder.

## Keyboard shortcuts

| Key | Action |
|-----|--------|
| F5 | Run / pause simulation |
| F6 / F7 / F8 | Step 1 / 5 / 15 minutes |
| Ctrl+N | Add train |
| Ctrl+D | Report delay for selected train |
| Ctrl+S / Ctrl+O | Save / open session |
| Ctrl+1 … Ctrl+6 | Switch page |

## Quick tour
1. Launch the app. The demo timetable starts at 07:50.
2. Press **F5** and set the speed to **x5**. Watch trains approach, get a platform, arrive and depart on the mimic panel.
3. Right-click a train and choose **Report delay → +30 minutes**. Its platform is re-planned if it clashes.
4. On **Platforms**, click a platform and choose **Take out of service**. Its booked trains move elsewhere.
5. On **Signals**, press **EMERGENCY STOP**. Trains are held at the outer signal. **Release all overrides** clears it.
