/*
 * ui.h - Low level console UI toolkit: colours, boxes, prompts, messages
 */
#ifndef UI_UI_H
#define UI_UI_H

#include "config.h"

#if defined(__GNUC__)
  #define UI_PRINTF(a, b) __attribute__((format(printf, a, b)))
#else
  #define UI_PRINTF(a, b)
#endif

#define UI_EOF (-1)

typedef enum {
    CLR_RESET, CLR_BOLD, CLR_DIM,
    CLR_RED, CLR_GREEN, CLR_YELLOW, CLR_BLUE, CLR_MAGENTA, CLR_CYAN, CLR_WHITE, CLR_GRAY,
    CLR_HEADER, CLR_BG_RED, CLR_BG_GREEN, CLR_BG_YELLOW, CLR_BG_CYAN,
    CLR_COUNT
} UiColor;

/* Lifecycle */
void ui_init(void);
void ui_shutdown(void);

/* Screen */
const char *col(UiColor c);
void ui_clear(void);
void ui_hide_cursor(void);
void ui_show_cursor(void);

/* Drawing */
int  ui_visible_width(const char *s);
void ui_repeat(const char *s, int n);
void ui_pad(const char *s, int width);
void ui_banner(const char *left, const char *right);
void ui_subtitle(const char *text);
void ui_section(const char *title);
void ui_menu_item(char key, const char *label, const char *hint);

/* Messages */
void ui_success(const char *fmt, ...) UI_PRINTF(1, 2);
void ui_error(const char *fmt, ...)   UI_PRINTF(1, 2);
void ui_warn(const char *fmt, ...)    UI_PRINTF(1, 2);
void ui_info(const char *fmt, ...)    UI_PRINTF(1, 2);

/* Input (blank input = cancel, returns 0) */
int  ui_read_line(char *buf, int size);
int  ui_prompt_choice(const char *prompt);              /* lower-case char, 0 = blank, UI_EOF */
int  ui_prompt_int(const char *prompt, int min, int max, int *out);
int  ui_prompt_time(const char *prompt, int *out);
int  ui_prompt_string(const char *prompt, char *buf, int size);
int  ui_confirm(const char *prompt);
void ui_pause(void);
int  ui_is_eof(void);

#endif /* UI_UI_H */
