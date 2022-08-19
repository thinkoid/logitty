// -*- mode: c++; -*-

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <linux/vt.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <ncurses.h>
#include <form.h>

#define UNUSED(x) ((void)(x))

static const char *console = "/dev/console";
static const int tty = 2;

static const int w = 36, h = 10;
static const int padding = 2;

static int switch_tty(int tty)
{
	FILE* pf = fopen(console, "w");
	if (0 == pf) {
                fprintf(stderr, "failed to open console\n");
                return 1;
        }

	int fd = fileno(pf);

	ioctl(fd, VT_ACTIVATE, tty);
	ioctl(fd, VT_WAITACTIVE, tty);

        fclose(pf);

        return 0;
}

struct screen_t {
        FORM *form;
        FIELD **fields;
        WINDOW *win, *sub;
};

static const char *hostname()
{
        static char buf[HOST_NAME_MAX + 1] = { 0 };
        return 0 == gethostname(buf, sizeof buf) ? buf : 0;
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
                }
        }

        return pf;
}

static FIELD *make_login_label()
{
        return make_field(1, 11, 3, 3, "login    : ");
}

static FIELD *make_login_field()
{
        return make_field(1, 14, 3, 14, 0);
}

static FIELD *make_passwd_label()
{
        return make_field(1, 11, 5, 3, "password : ");
}

static FIELD *make_passwd_field()
{
        FIELD *pf = make_field(1, 14, 5, 14, 0);
        field_opts_off(pf, O_PUBLIC);
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
        FIELD **fields = (FIELD **)malloc(5 * sizeof(FIELD *));
        if (0 == fields)
                return 0;

        fields[0] = make_login_label();
        fields[1] = make_login_field();

        fields[2] = make_passwd_label();
        fields[3] = make_passwd_field();

        fields[4] = 0;

        if (0 == fields[0] || 0 == fields[1] || 0 == fields[2] || 0 == fields[3]) {
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

        if (h > LINES + 3 || w > COLS) {
                fprintf(stderr, "screen too small (%d x %d)\n",
                        COLS, LINES + 3);
                endwin();
                exit(1);
        }

        signals();
}

static void redraw_screen(struct screen_t *screen)
{
        if (screen) {
                mvprintw(0, 0, "F1 reboot  F2 shutdown");
                refresh();

                box(screen->win, 0, 0);

                mvwprintw(screen->sub, 1, 13, "< xinitrc   >");
                // box(screen->sub, 0, 0);

                wrefresh(screen->win);
                wrefresh(screen->sub);
        }
}

static struct screen_t *make_screen()
{
        struct screen_t *screen;
        int x, y;

        x =  (COLS - w) / 2;
        y = (LINES - h) / 2;

        screen = (struct screen_t *)malloc(sizeof *screen);
        if (0 == screen) {
                fprintf(stderr, "failed to allocate screen\n");
                goto err;
        }

	screen->win = newwin(h, w, y, x);
        if (0 == screen->win) {
                fprintf(stderr, "failed to create window\n");
                goto err;
        }

	screen->sub = derwin(
                screen->win, h - 2 * padding, w - 2 * padding,
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

	set_form_win(screen->form, screen->win);
	set_form_sub(screen->form, screen->sub);

        post_form(screen->form);

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

        UNUSED(hostname);
        UNUSED(switch_tty);
        UNUSED(redraw_screen);

        initialize();

        screen = make_screen();
        if (0 == screen) {
                endwin();
                exit(1);
        }

        redraw_screen(screen);

        f = screen->form;
        fs = screen->fields;

	for (c = getch(); c != KEY_F(1); c = getch()) {
                switch (c) {
                case KEY_F(2):
                        form_driver(f, REQ_VALIDATION);

                        // Or the current field buffer won't be sync with what is displayed
                        form_driver(f, REQ_NEXT_FIELD);
                        form_driver(f, REQ_PREV_FIELD);

                        move(LINES - 3, 2);

                        pbuf = wstrim(field_buffer(fs[1], 0), buf, sizeof buf);
                        printw("[%s] : ", pbuf);

                        if (pbuf != buf)
                                free(pbuf);

                        pbuf = wstrim(field_buffer(fs[3], 0), buf, sizeof buf);
                        printw("[%s]", pbuf);

                        if (pbuf != buf)
                                free(pbuf);

                        refresh();

                        pos_form_cursor(f);
                        break;

                case KEY_DOWN:
                        form_driver(f, REQ_NEXT_FIELD);
                        form_driver(f, REQ_END_LINE);
                        break;

                case KEY_UP:
                        form_driver(f, REQ_PREV_FIELD);
                        form_driver(f, REQ_END_LINE);
                        break;

                case KEY_LEFT:
                        form_driver(f, REQ_PREV_CHAR);
                        break;

                case KEY_RIGHT:
                        form_driver(f, REQ_NEXT_CHAR);
                        break;

                        // Delete the char before cursor
                case KEY_BACKSPACE:
                case 127:
                        form_driver(f, REQ_DEL_PREV);
                        break;

                        // Delete the char under the cursor
                case KEY_DC:
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
