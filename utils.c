/* -*- mode: c; -*- */

#include <utils.h>

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

char* vsnprintf_(char* pbuf, size_t len, char const* fmt, va_list ap)
{
        int n;
        char buf[256], *psave;

        va_list tmp_va;

        if (0 == fmt || 0 == fmt [0])
                return 0;

        psave = pbuf;

        if (0 == pbuf || 0 == len) {
                pbuf = buf;
                len = sizeof buf;
        }

        for (;;) {
                va_copy(tmp_va, ap);

                n = vsnprintf(pbuf, len, fmt, tmp_va);
                va_end (tmp_va);

                if (0 > n) {
                        pbuf [0] = 0;
                        break;
                }

                if (0 == n)
                        len *= 2;
                else if (n >= (int)len) {
                        len = n + 1;
                }
                else
                        break;

                if (pbuf != buf && pbuf != psave) {
                        free(pbuf);
                        pbuf = 0;
                }

                pbuf = malloc(len);
        }

        if (pbuf == buf) {
                pbuf = malloc(strlen (pbuf) + 1);
                if (pbuf)
                        strcpy (pbuf, buf);
        }

        return pbuf;
}

char* snprintf_(char* pbuf, size_t len, char const* fmt, ...)
{
        char *ptr;
        va_list ap;

        va_start (ap, fmt);
        ptr = vsnprintf_(pbuf, len, fmt, ap);
        va_end (ap);

        return ptr;
}

char *wstrim(const char *from, char *to, size_t len)
{
        size_t n;
        const char *pbeg, *pend;

        for (pbeg = from; *pbeg && *pbeg == ' '; ++pbeg) ;
        if (0 == *pbeg) {
                *to = 0;
                return to;
        }

        n = strlen(pbeg);

        for (pend = pbeg + n - 1; pend != pbeg && *pend == ' '; --pend) ;
        ++pend;

        n = pend - pbeg;
        if (n >= len) {
                to = (char *)malloc(n + 1);
                if (0 == to)
                        return 0;
        }

        memcpy(to, pbeg, pend - pbeg);
        to[n] = 0;

        return to;
}
