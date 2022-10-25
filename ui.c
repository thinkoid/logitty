/* -*- mode: c; -*- */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <linux/vt.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>

#include <ncurses.h>
#include <form.h>

#include "ui.h"
#include "utils.h"

#define UNUSED(x) ((void)(x))

static const int box_width   = 40;
static const int box_height  = 11;
static const int box_padding =  1;

static const int nfields = 9;

static char *hostname(char *buf, size_t len)
{
        struct utsname utsname;

        if (0 > uname(&utsname))
                return 0;

        return snprintf_(buf, len, "%s", utsname.nodename);
}

static FIELD *make_field(int h, int w, int y, int x, const char *label)
{
        FIELD *pf = new_field(h, w, y, x, 0, 0);

        if (pf) {
                if (label && label[0]) {
                        set_field_buffer(pf, 0, label);
                        field_opts_off(pf, O_ACTIVE);
                        field_opts_off(pf, O_EDIT);
                }
                else {
                        field_opts_on(pf, O_ACTIVE);
                        field_opts_on(pf, O_EDIT);
                        field_opts_off(pf, O_AUTOSKIP);
                        field_opts_off(pf, O_STATIC);
                }
        }

        return pf;
}

static FIELD *make_login_label()
{
        return make_field(1, 11, 5, 3, "login    : ");
}

static FIELD *make_login_field()
{
        return make_field(1, 14, 5, 14, 0);
}

static FIELD *make_passwd_label()
{
        return make_field(1, 11, 7, 3, "password : ");
}

static FIELD *make_passwd_field()
{
        FIELD *pf = make_field(1, 1, 7, 14, 0);

        if (pf)
                field_opts_off(pf, O_PUBLIC);

        return pf;
}

static FIELD *make_startup_field(char **labels)
{
        FIELD *pf;

        const char *buf[16], **pbuf = buf;
        size_t i, n, buflen = sizeof buf / sizeof *buf;

        for (n = 0; labels[n]; ++n) ;
        if (n + 1 > buflen) {
                pbuf = malloc((n + 1) * sizeof *buf);
                if (0 == pbuf)
                        return 0;
        }

        pf = make_field(1, 10, 3, 15, 0);
        if (pf) {
                field_opts_off(pf, O_EDIT);

                for (i = 0; i < n; ++i)
                        pbuf[i] = labels[i];

                pbuf[i] = 0;
                set_field_type(pf, TYPE_ENUM, pbuf, 0, 0);
        }

        if (pbuf != buf)
                free(pbuf);

        return pf;
}

static FIELD *make_host_label()
{
        char buf[32], *pbuf = buf;
        size_t n, i;

        FIELD *pf;

        pbuf = hostname(buf, sizeof buf);
        if (0 == pbuf) {
                fprintf(stderr, "could not get host info\n");
                return 0;
        }

        n = strlen(pbuf);
        if (n > 32) {
                for (i = 30; pbuf[i]; ++i) {
                        pbuf[i] = '.';
                }

                pbuf[32] = 0;
                n = 32;
        }

        pf = make_field(1, n, 1, (box_width - n) / 2 - 1, pbuf);

        if (pbuf != buf)
                free(pbuf);

        return pf;
}

static void free_fields(FIELD **pptr, FIELD **ppend)
{
        for (; pptr != ppend; ++pptr)
                free(*pptr);
}

static FIELD **make_fields(char **labels)
{
        FIELD **fields = (FIELD **)malloc(nfields * sizeof(FIELD *));
        if (0 == fields)
                return 0;

        memset(fields, 0, nfields * sizeof(FIELD *));

        fields[0] = make_host_label();
        fields[1] = make_startup_field(labels);

        fields[2] = make_login_label();
        fields[3] = make_login_field();

        fields[4] = make_passwd_label();
        fields[5] = make_passwd_field();

        fields[6] = make_field(1, 1, 3, 13, "<");
        fields[7] = make_field(1, 1, 3, 27, ">");

        fields[8] = 0;

        if (0 == fields[0] || 0 == fields[1] || 0 == fields[2] ||
            0 == fields[3] || 0 == fields[4] || 0 == fields[5] ||
            0 == fields[6] || 0 == fields[7]) {
                free_fields(fields, fields + nfields);
                free(fields);
                return 0;
        }

        return fields;
}

void free_screen(struct screen_t *screen)
{
        if (screen) {
                if (screen->form) {
                        FORM *ptr = screen->form;

                        unpost_form(ptr);
                        free_form(ptr);
                }

                free_fields(screen->fields, screen->fields + nfields);
                free(screen->fields);

                if (screen->sub)
                        delwin(screen->sub);

                if (screen->win)
                        delwin(screen->win);

                free(screen);
        }
}

void draw_screen(struct screen_t *screen)
{
        if (screen) {
                mvprintw(0, 0, "F1 reboot  F2 shutdown");
                refresh();

                box(screen->win, 0, 0);

                wrefresh(screen->win);
                wrefresh(screen->sub);
        }
}

int init_screen()
{
        initscr();
        atexit((void(*)(void))endwin);

        noecho();
        cbreak();

        keypad(stdscr, TRUE);

        if (box_height + 3 > LINES || box_width > COLS) {
                fprintf(stderr, "screen too small (%d x %d)\n",
                        COLS, LINES);
                return 1;
        }

        return 0;
}

struct screen_t *
make_screen(char **labels)
{
        struct screen_t *screen;
        int x, y;

        x = (float)( COLS - box_width)  / 2 + 1;
        y = (float)(LINES - box_height) / 2 + 1;

        screen = (struct screen_t *)malloc(sizeof *screen);
        if (0 == screen) {
                fprintf(stderr, "failed to allocate screen\n");
                goto err;
        }

        screen->win = newwin(box_height, box_width, y, x);
        if (0 == screen->win) {
                fprintf(stderr, "failed to create window\n");
                goto err;
        }

        screen->sub = derwin(
                screen->win,
                box_height - 2 * box_padding,
                box_width - 2 * box_padding,
                box_padding, box_padding);
        if (0 == screen->sub) {
                fprintf(stderr, "failed to create sub-window\n");
                goto err;
        }

        screen->fields = make_fields(labels);
        if (0 == screen->fields) {
                fprintf(stderr, "failed to create form fields\n");
                goto err;
        }

        screen->form = new_form(screen->fields);
        if (0 == screen->form) {
                fprintf(stderr, "failed to create form\n");
                goto err;
        }

        set_form_win(screen->form, screen->win);
        set_form_sub(screen->form, screen->sub);

        post_form(screen->form);
        draw_screen(screen);

        return screen;

err:
        free_screen(screen);
        return 0;
}
