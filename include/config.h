/*
 * config.h - Global configuration constants for RailNet PMS
 */
#ifndef CONFIG_H
#define CONFIG_H

#define APP_NAME            "RailNet PMS"
#define APP_VERSION         "1.0.0"

/* Capacity limits */
#define MAX_TRAINS          64
#define MAX_PLATFORMS       12
#define MAX_SIGNALS         (MAX_PLATFORMS * 2 + 2)
#define MAX_LOG_ENTRIES     200

/* String sizes */
#define NAME_LEN            32
#define CITY_LEN            20
#define TRAIN_NO_LEN        8
#define CODE_LEN            8
#define LOG_MSG_LEN         112

/* Operating rules (all times in minutes) */
#define APPROACH_WINDOW     15   /* train becomes "approaching" this many minutes before ETA */
#define HOME_CLEAR_WINDOW   3    /* home signal clears this many minutes before ETA          */
#define PLATFORM_BUFFER     5    /* minimum gap between two trains on the same platform      */
#define MAX_COACHES         30
#define MAX_HALT            180
#define MAX_DELAY           720
#define MINUTES_PER_DAY     1440

/* Files */
#define DATA_DIR            "data"
#define SAVE_FILE           "data/station.dat"
#define LOG_FILE            "data/events.log"
#define BOARD_FILE          "data/board.txt"
#define STORAGE_VERSION     1

/* Console layout */
#define UI_WIDTH            100

#endif /* CONFIG_H */
