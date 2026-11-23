# RailNet PMS — Railway Platform Management System (C)

A console-based railway station control room written in pure C (C99).
It tracks incoming trains, allocates platforms, handles delays, operates
signals with interlocking, and simulates the station minute by minute.

```
╔══════════════════════════════════════════════════════════════════════════════════╗
║  ◆ RAILNET PMS  │  Central Junction (CJN)     CLOCK 08:05  │  SIGNALS AUTO  │ ...║
╚══════════════════════════════════════════════════════════════════════════════════╝
  P1  24c  H● ──────────────────────────────────────────── ● S  FREE   next 22691 08:25
  P2  24c  H● ═════[ 12627 Coastal Superf dep 08:12 ]═════ ● S  OCCUPIED
```

## Features

| Area | What it does |
|------|--------------|
| **Live dashboard** | Incoming trains board, track diagram with signal lamps, recent events, real-time "live mode" |
| **Arrivals board** | All trains sorted by ETA: scheduled/expected times, delay, platform, status |
| **Train management** | Add, search, view details, edit schedule, cancel, remove |
| **Delay management** | Report delays (with reasons), recover time, punctuality report with bar graph |
| **Platform control** | Manual assignment with per-platform availability check, best-fit auto-allocation by priority, release, maintenance, occupancy (Gantt) chart |
| **Signal control** | Outer / home / starter signals, automatic or manual signalling, operator overrides, **interlocking**, emergency all-red |
| **Simulation** | Advance N minutes, advance to a time, live mode (1 s = 1 min) |
| **Persistence** | Save/restore session, export the board to text, event log file |

### Operating rules
* Trains become **APPROACHING** 15 min before ETA; a platform is auto-allocated then (if AUTO-PF is on).
* A train enters only if its platform is free **and** its home signal is not RED; otherwise it is
  **HELD** at the outer signal and gains 1 minute of delay for every minute it waits.
* Two trains on one platform need a 5-minute gap. Delays that cause conflicts are re-planned automatically.
* Interlocking: a home signal cannot clear onto an occupied or closed platform; a starter
  signal cannot clear unless a train is standing at the platform.

## Project layout

```
C project/
├── include/                 header files (public interfaces)
│   ├── config.h             global constants
│   ├── core/                domain model
│   │   ├── train.h          Train entity
│   │   ├── platform.h       Platform entity
│   │   ├── signal_ctrl.h    Signal entity
│   │   ├── station.h        Station aggregate
│   │   ├── scheduler.h      operations engine
│   │   └── demo_data.h      sample timetable
│   ├── io/
│   │   ├── logger.h         event log
│   │   └── storage.h        save / load / export
│   ├── ui/
│   │   ├── ui.h             console toolkit (colours, boxes, prompts)
│   │   ├── views.h          screens & widgets
│   │   └── menu.h           interactive menus
│   └── util/
│       └── utils.h          time, string, OS helpers
├── src/                     implementations (same structure)
│   ├── main.c
│   ├── core/  io/  ui/  util/
├── data/                    runtime files (station.dat, events.log, board.txt)
├── build/                   compiler output
├── Makefile
└── build.bat
```

## Build & run

**Windows (MinGW)**

```bat
build.bat          :: build
build.bat run      :: build and run
```

or with make:

```bat
mingw32-make
mingw32-make run
```

**Linux / macOS**

```sh
make && ./build/railnet
```

Run it from the project folder so `data/` is found. It works best in Windows Terminal or the
VS Code terminal at 100+ columns wide.

## Quick tour
1. Start the program and press Enter. It loads the demo timetable at 07:50.
2. **1 → l**: start live mode and watch trains approach, arrive and depart.
3. **4 → 1**: delay a train by 30 min. Its platform is re-planned if it now conflicts.
4. **5 → 5**: close a platform for maintenance. Booked trains move to other platforms.
5. **6 → 9**: emergency stop. Trains are held at the outer signal. **6 → 3** releases it.
6. **5 → 6**: occupancy chart for the next 2.4 hours.
