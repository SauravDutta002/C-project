/*
 * ui.c - Low level console UI toolkit (ANSI / VT100 + UTF-8 box drawing)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#include "ui/ui.h"
#include "util/utils.h"

#ifdef _WIN32
  #include <windows.h>
  #ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
    #define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
  #endif
  static UINT g_old_cp = 0;
#endif

static int g_color = 1;
static int g_eof = 0;

static const char *CODES[CLR_COUNT] = {
    "\x1b[0m",          /* RESET    */
    "\x1b[1m",          /* BOLD     */
    "\x1b[2m",          /* DIM      */
    "\x1b[91m",         /* RED      */
    "\x1b[92m",         /* GREEN    */
    "\x1b[93m",         /* YELLOW   */
    "\x1b[94m",         /* BLUE     */
    "\x1b[95m",         /* MAGENTA  */
    "\x1b[96m",         /* CYAN     */
    "\x1b[97m",         /* WHITE    */
    "\x1b[90m",         /* GRAY     */
    "\x1b[1;97;44m",    /* HEADER   */
    "\x1b[1;97;41m",    /* BG_RED   */
    "\x1b[1;30;42m",    /* BG_GREEN */
    "\x1b[1;30;43m",    /* BG_YELLOW*/
    "\x1b[1;30;46m"     /* BG_CYAN  */
};

/* ------------------------------------------------------------------ */
/* Lifecycle                                                           */
/* ------------------------------------------------------------------ */

void ui_init(void)
{
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;

    g_old_cp = GetConsoleOutputCP();
    SetConsoleOutputCP(CP_UTF8);
    if (h == INVALID_HANDLE_VALUE || !GetConsoleMode(h, &mode) ||
        !SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
        g_color = 0;
    SetConsoleTitleA(APP_NAME " - Railway Platform Management System");
#endif
    /* Full buffering = flicker-free redraws; prompts flush explicitly */
    setvbuf(stdout, NULL, _IOFBF, 1 << 15);
}

void ui_shutdown(void)
{
    ui_show_cursor();
    fputs(col(CLR_RESET), stdout);
    fflush(stdout);
#ifdef _WIN32
    if (g_old_cp) SetConsoleOutputCP(g_old_cp);
#endif
}

/* ------------------------------------------------------------------ */
/* Screen                                                              */
/* ------------------------------------------------------------------ */

const char *col(UiColor c)
{
    if (!g_color || (int)c < 0 || (int)c >= CLR_COUNT) return "";
    return CODES[c];
}

void ui_clear(void)
{
    if (g_color) fputs("\x1b[H\x1b[2J\x1b[3J", stdout);
    else         fputs("\n\n", stdout);
}

void ui_hide_cursor(void) { if (g_color) fputs("\x1b[?25l", stdout); }
void ui_show_cursor(void) { if (g_color) fputs("\x1b[?25h", stdout); }

/* ------------------------------------------------------------------ */
/* Drawing                                                             */
/* ------------------------------------------------------------------ */

/* Printable width of a string: skips ANSI escapes and UTF-8 continuation bytes */
int ui_visible_width(const char *s)
{
    int w = 0;
    while (*s) {
        unsigned char c = (unsigned char)*s;
        if (c == 0x1b) {
            s++;
            if (*s == '[') {
                s++;
                while (*s && !isalpha((unsigned char)*s)) s++;
                if (*s) s++;
            }
            continue;
        }
        if ((c & 0xC0) != 0x80) w++;
        s++;
    }
    return w;
}

void ui_repeat(const char *s, int n)
{
    while (n-- > 0) fputs(s, stdout);
}

void ui_pad(const char *s, int width)
{
    fputs(s, stdout);
    ui_repeat(" ", width - ui_visible_width(s));
}

void ui_banner(const char *left, const char *right)
{
    int inner = UI_WIDTH - 2;
    int pad = inner - ui_visible_width(left) - ui_visible_width(right);
    if (pad < 1) pad = 1;

    printf("%s╔", col(CLR_CYAN));
    ui_repeat("═", inner);
    printf("╗%s\n", col(CLR_RESET));

    printf("%s║%s%s", col(CLR_CYAN), col(CLR_RESET), left);
    ui_repeat(" ", pad);
    printf("%s%s║%s\n", right, col(CLR_CYAN), col(CLR_RESET));

    printf("%s╚", col(CLR_CYAN));
    ui_repeat("═", inner);
    printf("╝%s\n", col(CLR_RESET));
}

void ui_subtitle(const char *text)
{
    printf(" %s▸ %s%s%s\n", col(CLR_CYAN), col(CLR_BOLD), text, col(CLR_RESET));
}

void ui_section(const char *title)
{
    int used = ui_visible_width(title) + 4;
    printf("\n %s%s▌ %s %s%s", col(CLR_CYAN), col(CLR_BOLD), title, col(CLR_RESET), col(CLR_GRAY));
    ui_repeat("─", UI_WIDTH - used - 1);
    printf("%s\n", col(CLR_RESET));
}

void ui_menu_item(char key, const char *label, const char *hint)
{
    printf("    %s%s[%c]%s  %-30s %s%s%s\n",
           col(CLR_BOLD), col(CLR_YELLOW), key, col(CLR_RESET),
           label, col(CLR_GRAY), hint ? hint : "", col(CLR_RESET));
}

/* ------------------------------------------------------------------ */
/* Messages                                                            */
/* ------------------------------------------------------------------ */

static void ui_msg(UiColor badge, const char *tag, const char *fmt, va_list ap)
{
    printf("  %s %s %s ", col(badge), tag, col(CLR_RESET));
    vprintf(fmt, ap);
    printf("\n");
}

void ui_success(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt); ui_msg(CLR_BG_GREEN, " OK ", fmt, ap); va_end(ap);
}

void ui_error(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt); ui_msg(CLR_BG_RED, "ERROR", fmt, ap); va_end(ap);
}

void ui_warn(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt); ui_msg(CLR_BG_YELLOW, "WARN", fmt, ap); va_end(ap);
}

void ui_info(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt); ui_msg(CLR_BG_CYAN, "INFO", fmt, ap); va_end(ap);
}

/* ------------------------------------------------------------------ */
/* Input                                                               */
/* ------------------------------------------------------------------ */

int ui_is_eof(void) { return g_eof; }

int ui_read_line(char *buf, int size)
{
    size_t len;

    fflush(stdout);
    if (g_eof || !fgets(buf, size, stdin)) {
        g_eof = 1;
        buf[0] = '\0';
        return -1;
    }
    len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[--len] = '\0';
    } else {
        int ch;   /* line longer than buffer: drain the rest */
        while ((ch = getchar()) != '\n' && ch != EOF) { }
    }
    if (len > 0 && buf[len - 1] == '\r') buf[--len] = '\0';
    if (len >= 3 && (unsigned char)buf[0] == 0xEF && (unsigned char)buf[1] == 0xBB &&
        (unsigned char)buf[2] == 0xBF) {          /* strip UTF-8 BOM from piped input */
        memmove(buf, buf + 3, len - 2);
        len -= 3;
    }
    return (int)len;
}

static void prompt_label(const char *prompt)
{
    printf("  %s%s›%s %s: ", col(CLR_BOLD), col(CLR_YELLOW), col(CLR_RESET), prompt);
}

int ui_prompt_choice(const char *prompt)
{
    char buf[64];
    prompt_label(prompt);
    if (ui_read_line(buf, sizeof buf) < 0) return UI_EOF;
    util_trim(buf);
    if (!buf[0]) return 0;
    return tolower((unsigned char)buf[0]);
}

int ui_prompt_int(const char *prompt, int min, int max, int *out)
{
    char buf[64];
    char *end;
    long v;

    for (;;) {
        printf("  %s%s›%s %s %s[%d-%d]%s: ", col(CLR_BOLD), col(CLR_YELLOW), col(CLR_RESET),
               prompt, col(CLR_GRAY), min, max, col(CLR_RESET));
        if (ui_read_line(buf, sizeof buf) < 0) return 0;
        util_trim(buf);
        if (!buf[0]) return 0;
        v = strtol(buf, &end, 10);
        if (*end || v < min || v > max) {
            ui_error("Enter a whole number between %d and %d (blank to cancel).", min, max);
            continue;
        }
        *out = (int)v;
        return 1;
    }
}

int ui_prompt_time(const char *prompt, int *out)
{
    char buf[64];
    for (;;) {
        printf("  %s%s›%s %s %s[HH:MM]%s: ", col(CLR_BOLD), col(CLR_YELLOW), col(CLR_RESET),
               prompt, col(CLR_GRAY), col(CLR_RESET));
        if (ui_read_line(buf, sizeof buf) < 0) return 0;
        util_trim(buf);
        if (!buf[0]) return 0;
        if (!util_parse_time(buf, out)) {
            ui_error("Invalid time. Use 24-hour HH:MM, e.g. 08:45 (blank to cancel).");
            continue;
        }
        return 1;
    }
}

int ui_prompt_string(const char *prompt, char *buf, int size)
{
    char tmp[256];
    prompt_label(prompt);
    if (ui_read_line(tmp, sizeof tmp) < 0) return 0;
    util_trim(tmp);
    if (!tmp[0]) return 0;
    util_sanitize(tmp);
    util_copy(buf, tmp, size);
    return 1;
}

int ui_confirm(const char *prompt)
{
    char buf[16];
    printf("  %s%s?%s %s %s(y/n)%s: ", col(CLR_BOLD), col(CLR_MAGENTA), col(CLR_RESET),
           prompt, col(CLR_GRAY), col(CLR_RESET));
    if (ui_read_line(buf, sizeof buf) < 0) return 0;
    util_trim(buf);
    return tolower((unsigned char)buf[0]) == 'y';
}

void ui_pause(void)
{
    char buf[16];
    printf("\n  %sPress Enter to continue...%s", col(CLR_GRAY), col(CLR_RESET));
    ui_read_line(buf, sizeof buf);
}
