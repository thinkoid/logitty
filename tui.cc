// -*- mode: c++; -*-

#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <climits>

#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include <linux/vt.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <ncurses.h>
#include <form.h>

#define UNUSED(x) ((void)(x))

static const int tty = 2;
static const char *console = "/dev/console";

static void switch_tty(int tty) {
	FILE* pf = fopen(console, "w");
	if (0 == pf)
                throw std::runtime_error("failed to open console");

	int fd = fileno(pf);

	ioctl(fd, VT_ACTIVATE, tty);
	ioctl(fd, VT_WAITACTIVE, tty);

        fclose(pf);
}

static std::string hostname() {
        char buf[HOST_NAME_MAX + 1] = { 0 };

        if (0 == gethostname(buf, sizeof buf))
                return { buf };

        throw std::runtime_error("failed to get hostname");
}

static void signal_handler(int n)
{
        printf(" --> caught : %d\n", n);
}

static void signals()
{
        for (auto x : { SIGINT, SIGTERM, SIGTSTP, SIGQUIT })
                signal(x, signal_handler);
}

static FIELD *make_field(int h, int w, int y, int x)
{
        FIELD *pf = new_field(h, w, y, x, 0, 0);
        field_opts_off(pf, O_AUTOSKIP);
        return pf;
}

static FIELD *make_login_field()
{
        return make_field(1, 10, 4, 21);
}

static FIELD *make_passwd_field()
{
        FIELD *pf = make_field(1, 10, 6, 21);
        field_opts_off(pf, O_PUBLIC);
        return pf;
}

static FIELD **make_form_fields()
{
        static FIELD *arr[] = { 0 };

        if (0 == arr[0]) {
                arr[0] = make_login_field();
                arr[1] = make_passwd_field();
        };

        return arr;
}

static FORM *make_form(FIELD **fields)
{
        return new_form(fields);
}

int main()
{
        UNUSED(switch_tty);

        signals();

        /* Initialize curses */
        initscr();

        cbreak();
        noecho();

        keypad(stdscr, TRUE);

        FIELD **fields = make_form_fields();
        FORM  *form = make_form(fields);

        post_form(form);
        refresh();

        attron(A_BOLD);
        mvprintw(2, 10, "%s\n", hostname().c_str());
        mvprintw(4, 10, "login    :");
        mvprintw(6, 10, "password :");
        attroff(A_BOLD);

        refresh();

        for(int ch = getch(); ch != KEY_F(1); ch = getch()) {
                switch(ch) {
                case KEY_DOWN: case KEY_BTAB:
                        form_driver(form, REQ_NEXT_FIELD);
                        break;

                case KEY_UP:
                        form_driver(form, REQ_PREV_FIELD);
                        break;

                case KEY_RIGHT:
                        form_driver(form, REQ_NEXT_CHAR);
                        break;

                case KEY_LEFT:
                        form_driver(form, REQ_PREV_CHAR);
                        break;

                case KEY_BACKSPACE:
		case 127:
			form_driver(form, REQ_DEL_PREV);
			break;

		case KEY_DC:
			form_driver(form, REQ_DEL_CHAR);
			break;

                default:
                        form_driver(form, ch);
                        break;
                }
        }

        form_driver(form, REQ_VALIDATION);

        mvprintw(40, 0, "username : ");

        attron(A_BOLD);
        printw("%s\n", field_buffer(fields[0], 0));
        attroff(A_BOLD);

        mvprintw(41, 0, "password : ");

        attron(A_BOLD);
        printw("%s\n", field_buffer(fields[1], 0));
        attroff(A_BOLD);

        refresh();
        getch();

        unpost_form(form);
        free_form(form);

        free_field(fields[0]);
        free_field(fields[1]);

        endwin();

        return 0;
}
