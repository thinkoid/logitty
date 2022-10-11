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

#include <utils.h>

#define UNUSED(x) ((void)(x))

struct screen_t {
        FORM *form;
        FIELD **fields;
        WINDOW *win, *sub;
};

static const char *console = "/dev/console";
static const int tty = 2;

static const int width = 40, height = 11;
static const int padding = 1;

static const struct {
        const char *label, *cmd;
} choices[] = {
        { "shell", "/bin/zsh" },
        { "dwl", "dwl" }
};

static int switch_tty(int tty)
{
        int fd = open(console, O_WRONLY);
        if (0 > fd) {
                perror("console");
                return 1;
        }

        ioctl(fd, VT_ACTIVATE, tty);
        ioctl(fd, VT_WAITACTIVE, tty);

        close(fd);

        return 0;
}

static char *hostname(char *buf, size_t len)
{
        struct utsname utsname;

        if (0 > uname(&utsname))
                return 0;

        return snprintf_(buf, len, "%s", utsname.nodename);
}

static void signal_handler(int n)
{
        printf(" --> caught : %d\n", n);
}

static void signals()
{
        static int arr[] = { SIGINT, SIGTERM, SIGTSTP, SIGQUIT };
        for (size_t i = 0; i < sizeof arr / sizeof *arr; ++i)
                signal(arr[1], signal_handler);
}

static char *wstrim(const char *from, char *to, size_t len)
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

static FIELD *make_target_field()
{
        const char *buf[16], **pbuf = buf;
        size_t i, n, buflen = sizeof buf / sizeof *buf;

        FIELD *pf;

        n = sizeof choices / sizeof *choices;
        if (n > buflen) {
                pbuf = malloc(n * sizeof *buf);
                if (0 == pbuf)
                        return 0;
        }

        pf = make_field(1, 10, 3, 15, 0);
        if (pf) {
                field_opts_off(pf, O_EDIT);

                for (i = 0; i < n; ++i)
                        pbuf[i] = choices[i].label;

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

        pf = make_field(1, n, 1, (width - n) / 2 - 1, pbuf);

        if (pbuf != buf)
                free(pbuf);

        return pf;
}

static void free_fields(FIELD **pptr)
{
        if (pptr) {
                for (FIELD **pp = pptr; *pp; ++pp)
                        free(*pp);
        }
}

static FIELD **make_fields()
{
        FIELD **fields = (FIELD **)malloc(9 * sizeof(FIELD *));
        if (0 == fields)
                return 0;

        fields[0] = make_host_label();
        fields[1] = make_target_field();

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
                free_fields(fields);
                return 0;
        }

        return fields;
}

static void free_screen(struct screen_t *screen)
{
        if (screen) {
                if (screen->form) {
                        FORM *ptr = screen->form;

                        unpost_form(ptr);
                        free_form(ptr);
                }

                free_fields(screen->fields);

                if (screen->sub)
                        delwin(screen->sub);

                if (screen->win)
                        delwin(screen->win);

                free(screen);
        }
}

static void initialize()
{
        initscr();

        noecho();
        cbreak();

        keypad(stdscr, TRUE);

        if (height + 3 > LINES || width > COLS) {
                fprintf(stderr, "screen too small (%d x %d)\n",
                        COLS, LINES);
                endwin();
                exit(1);
        }

        signals();
}

static void draw_screen(struct screen_t *screen)
{
        if (screen) {
                mvprintw(0, 0, "F1 quit  F2 print");
                refresh();

                box(screen->win, 0, 0);

                wrefresh(screen->win);
                wrefresh(screen->sub);
        }
}

static struct screen_t *make_screen()
{
        struct screen_t *screen;
        int x, y;

        x = (float)( COLS -  width) / 2 + 1;
        y = (float)(LINES - height) / 2 + 1;

        screen = (struct screen_t *)malloc(sizeof *screen);
        if (0 == screen) {
                fprintf(stderr, "failed to allocate screen\n");
                goto err;
        }

        screen->win = newwin(height, width, y, x);
        if (0 == screen->win) {
                fprintf(stderr, "failed to create window\n");
                goto err;
        }

        screen->sub = derwin(
                screen->win, height - 2 * padding, width - 2 * padding,
                padding, padding);
        if (0 == screen->sub) {
                fprintf(stderr, "failed to create sub-window\n");
                goto err;
        }

        screen->fields = make_fields();
        if (0 == screen->fields) {
                fprintf(stderr, "failed to create form fields\n");
                goto err;
        }

        screen->form = new_form(screen->fields);
        if (0 == screen->form) {
                fprintf(stderr, "failed to create form\n");
                goto err;
        }

        return screen;

err:
        free_screen(screen);
        return 0;
}

int main()
{
        int c;
        char buf[256], *pbuf;

        FORM *f = 0;
        FIELD **fs = 0;

        struct screen_t *screen = 0;

        UNUSED(tty);
        UNUSED(hostname);
        UNUSED(switch_tty);

        initialize();

        screen = make_screen();
        if (0 == screen) {
                endwin();
                exit(1);
        }

        set_form_win(screen->form, screen->win);
        set_form_sub(screen->form, screen->sub);

        post_form(screen->form);
        draw_screen(screen);

        f = screen->form;
        fs = screen->fields;

        form_driver(f, REQ_NEXT_CHOICE);
        draw_screen(screen);

        for (c = getch(); c != KEY_F(1); c = getch()) {
                switch (c) {
                case KEY_F(2):
                        form_driver(f, REQ_VALIDATION);

                        form_driver(f, REQ_NEXT_FIELD);
                        form_driver(f, REQ_PREV_FIELD);

                        move(LINES - 3, 2);

                        pbuf = wstrim(field_buffer(fs[1], 0), buf, sizeof buf);
                        printw("[%s] : ", pbuf);

                        if (pbuf != buf)
                                free(pbuf);

                        pbuf = wstrim(field_buffer(fs[3], 0), buf, sizeof buf);
                        printw("[%s] : ", pbuf);

                        if (pbuf != buf)
                                free(pbuf);

                        pbuf = wstrim(field_buffer(fs[5], 0), buf, sizeof buf);
                        printw("[%s]", pbuf);

                        if (pbuf != buf)
                                free(pbuf);

                        refresh();

                        pos_form_cursor(f);
                        break;

                case '\t':
                        form_driver(f, REQ_NEXT_FIELD);
                        form_driver(f, REQ_END_FIELD);
                        break;

                case KEY_LEFT:
                        if (fs[1] == current_field(f)) {
                                form_driver(f, REQ_PREV_CHOICE);
                        }
                        else {
                                form_driver(f, REQ_PREV_CHAR);
                        }
                        break;

                case KEY_RIGHT:
                        if (fs[1] == current_field(f)) {
                                form_driver(f, REQ_NEXT_CHOICE);
                        }
                        else {
                                form_driver(f, REQ_NEXT_CHAR);
                        }
                        break;

                case KEY_BACKSPACE:
                case 127:
                        /* Delete the char before cursor */
                        form_driver(f, REQ_DEL_PREV);
                        break;

                case KEY_DC:
                        /* Delete the char under the cursor */
                        form_driver(f, REQ_DEL_CHAR);
                        break;

                default:
                        form_driver(f, c);
                        break;
                }

                wrefresh(screen->win);
        }

        free_screen(screen);
        endwin();

        return 0;
}
