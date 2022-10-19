/* -*- mode: c; -*- */

#ifndef TUI_UTILS_H
#define TUI_UTILS_H

#include <stdio.h>

char* vsnprintf_(char* pbuf, size_t len, char const* fmt, va_list ap);
char* snprintf_(char* pbuf, size_t len, char const* fmt, ...);

#endif /* TUI_UTILS_H */
