/*
 * utils.h - General purpose helpers: time, strings, OS portability
 */
#ifndef UTIL_UTILS_H
#define UTIL_UTILS_H

/* Time helpers (minutes since midnight <-> "HH:MM") */
void        util_fmt_time(int minutes, char *buf);      /* buf must hold 6 chars */
const char *util_time_str(int minutes);                 /* rotating static buffer */
int         util_parse_time(const char *s, int *out);   /* accepts HH:MM or HHMM */

/* String helpers */
void util_trim(char *s);
void util_sanitize(char *s);
void util_copy(char *dst, const char *src, int size);
int  util_equals_ci(const char *a, const char *b);
int  util_contains_ci(const char *hay, const char *needle);
int  util_is_digits(const char *s);

/* OS helpers */
void util_sleep_ms(int ms);
int  util_key_pressed(void);
void util_consume_key(void);
int  util_ensure_dir(const char *path);
int  util_file_exists(const char *path);

#endif /* UTIL_UTILS_H */
