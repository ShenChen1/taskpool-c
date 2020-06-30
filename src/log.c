#include <stdio.h>
#include <stdarg.h>
#include "log.h"

static loglevel_t s_level = LOG_LV_INFO;

int log_setlevel(loglevel_t level)
{
    if (level >= LOG_LV_MAX) {
        return -1;
    }

    s_level = level;
    return 0;
}

int log_printf(loglevel_t level, const char *fmt, ...)
{
    int n;
    va_list ap;

    if (level > s_level) {
        return 0;
    }

    va_start(ap, fmt);
    n = vprintf(fmt, ap);
    va_end(ap);

    return n;
}