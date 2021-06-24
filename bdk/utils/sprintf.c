/*
 * Copyright (c) 2019-2020 shchmue
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "sprintf.h"

#include <stdarg.h>

static void _putc(char *buffer, const char c) {
    *buffer = c;
}

static u32 _puts(char *buffer, const char *s) {
    u32 count = 0;
    for (; *s; s++, count++)
        _putc(buffer + count, *s);
    return count;
}

static u32 _putn(char *buffer, u32 v, int base, char fill, int fcnt) {
    char buf[0x121];
    static const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    char *p;
    int c = fcnt;

    if (base > 36)
        return 0;

    p = buf + 0x120;
    *p = 0;
    do {
        c--;
        *--p = digits[v % base];
        v /= base;
    } while (v);

    if (fill != 0) {
        while (c > 0) {
            *--p = fill;
            c--;
        }
    }

    return _puts(buffer, p);
}

u32 s_printf(char *buffer, const char *fmt, ...) {
    va_list ap;
    int fill, fcnt;
    u32 count = 0;

    va_start(ap, fmt);
    while(*fmt) {
        if (*fmt == '%') {
            fmt++;
            fill = 0;
            fcnt = 0;
            if ((*fmt >= '0' && *fmt <= '9') || *fmt == ' ') {
                fcnt = *fmt;
                fmt++;
                if (*fmt >= '0' && *fmt <= '9') {
                    fill = fcnt;
                    fcnt = *fmt - '0';
                    fmt++;
                } else {
                    fill = ' ';
                    fcnt -= '0';
                }
            }
            switch (*fmt) {
            case 'c':
                _putc(buffer + count, va_arg(ap, u32));
                count++;
                break;
            case 's':
                count += _puts(buffer + count, va_arg(ap, char *));
                break;
            case 'd':
                count += _putn(buffer + count, va_arg(ap, u32), 10, fill, fcnt);
                break;
            case 'p':
            case 'P':
            case 'x':
            case 'X':
                count += _putn(buffer + count, va_arg(ap, u32), 16, fill, fcnt);
                break;
            case '%':
                _putc(buffer + count, '%');
                count++;
                break;
            case '\0':
                goto out;
            default:
                _putc(buffer + count, '%');
                count++;
                _putc(buffer + count, *fmt);
                count++;
                break;
            }
        } else {
            _putc(buffer + count, *fmt);
            count++;
        }
        fmt++;
    }

    out:
    buffer[count] = 0;
    va_end(ap);
    return count;
}