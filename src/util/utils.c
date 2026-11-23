/*
 * utils.c - General purpose helpers
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "config.h"
#include "util/utils.h"

#ifdef _WIN32
  #include <windows.h>
  #include <conio.h>
  #include <direct.h>
#else
  #include <unistd.h>
  #include <sys/select.h>
  #include <sys/stat.h>
  #include <sys/time.h>
  #include <sys/types.h>
#endif

/* ------------------------------------------------------------------ */
/* Time                                                                */
/* ------------------------------------------------------------------ */

void util_fmt_time(int minutes, char *buf)
{
    int m = ((minutes % MINUTES_PER_DAY) + MINUTES_PER_DAY) % MINUTES_PER_DAY;
    sprintf(buf, "%02d:%02d", m / 60, m % 60);
}

const char *util_time_str(int minutes)
{
    static char ring[8][8];
    static int next = 0;
    char *buf = ring[next];
    next = (next + 1) % 8;
    util_fmt_time(minutes, buf);
    return buf;
}

int util_parse_time(const char *s, int *out)
{
    int h = -1, m = -1;
    char tail;

    if (!s) return 0;
    while (isspace((unsigned char)*s)) s++;

    if (strchr(s, ':')) {
        if (sscanf(s, "%d:%d %c", &h, &m, &tail) != 2) return 0;
    } else {
        size_t len = strlen(s);
        int n;
        if ((len != 3 && len != 4) || !util_is_digits(s)) return 0;
        n = atoi(s);
        h = n / 100;
        m = n % 100;
    }
    if (h < 0 || h > 23 || m < 0 || m > 59) return 0;
    *out = h * 60 + m;
    return 1;
}

/* ------------------------------------------------------------------ */
/* Strings                                                             */
/* ------------------------------------------------------------------ */

void util_trim(char *s)
{
    char *start = s;
    size_t len;

    while (*start && isspace((unsigned char)*start)) start++;
    if (start != s) memmove(s, start, strlen(start) + 1);

    len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) s[--len] = '\0';
}

/* Remove characters that would break the '|' separated save format */
void util_sanitize(char *s)
{
    for (; *s; s++)
        if (*s == '|' || *s == '\t' || *s == '\r' || *s == '\n') *s = '/';
}

void util_copy(char *dst, const char *src, int size)
{
    int i = 0;
    if (size <= 0) return;
    if (src)
        for (; i < size - 1 && src[i]; i++) dst[i] = src[i];
    dst[i] = '\0';
}

int util_equals_ci(const char *a, const char *b)
{
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++;
        b++;
    }
    return *a == *b;
}

int util_contains_ci(const char *hay, const char *needle)
{
    size_t n = strlen(needle);
    if (n == 0) return 1;
    for (; *hay; hay++) {
        size_t i = 0;
        while (i < n && hay[i] &&
               tolower((unsigned char)hay[i]) == tolower((unsigned char)needle[i]))
            i++;
        if (i == n) return 1;
    }
    return 0;
}

int util_is_digits(const char *s)
{
    if (!*s) return 0;
    for (; *s; s++)
        if (!isdigit((unsigned char)*s)) return 0;
    return 1;
}

/* ------------------------------------------------------------------ */
/* OS portability                                                      */
/* ------------------------------------------------------------------ */

void util_sleep_ms(int ms)
{
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    usleep((useconds_t)ms * 1000);
#endif
}

int util_key_pressed(void)
{
#ifdef _WIN32
    return _kbhit();
#else
    fd_set set;
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    FD_ZERO(&set);
    FD_SET(0, &set);
    return select(1, &set, NULL, NULL, &tv) > 0;
#endif
}

void util_consume_key(void)
{
#ifdef _WIN32
    while (_kbhit()) (void)_getch();
#else
    int c;
    while ((c = getchar()) != '\n' && c != EOF) { }
#endif
}

int util_ensure_dir(const char *path)
{
#ifdef _WIN32
    if (_mkdir(path) == 0) return 1;
#else
    if (mkdir(path, 0755) == 0) return 1;
#endif
    return errno == EEXIST;
}

int util_file_exists(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    fclose(f);
    return 1;
}
