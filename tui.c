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

#include "run.h"
#include "ui.h"

#define UNUSED(x) ((void)(x))

static struct {
        char *label;
        char *argv[16];
} startups[] = {
        { "shell", { "/bin/zsh", 0 } },
        { "dwl",   { "/usr/local/bin/dwl", 0 }  }
};

static void signal_handler(int n)
{
        UNUSED(n);
}

static void signals()
{
        static int arr[] = { SIGINT, SIGTERM, SIGTSTP, SIGQUIT };
        for (size_t i = 0; i < sizeof arr / sizeof *arr; ++i)
                signal(arr[1], signal_handler);
}

static char *
field_buffer_trim(FIELD *f)
{
        char buf[256], *pbuf;

        pbuf = wstrim(field_buffer(f, 0), buf, sizeof buf);
        if (pbuf == buf)
                pbuf = strdup(buf);

        return pbuf;
}

static char **
startup_argvs(const char *startup)
{
        size_t i;

        for (i = 0; i < sizeof startups / sizeof *startups; ++i) {
                if (0 == strcmp(startup, startups[i].label)) {
                        return startups[i].argv;
                }
        }

        return 0;
}

static char **
startup_labels(char **ppbuf, size_t len)
{
        size_t i;

        if (len - 1 < sizeof startups / sizeof *startups) {
                len = sizeof startups / sizeof *startups + 1;

                ppbuf = malloc(len * sizeof(char *));
                if (0 == ppbuf)
                        return 0;

                memset(ppbuf, 0, len * sizeof(char *));
        }

        for (i = 0; i < sizeof startups / sizeof *startups; ++i)
                ppbuf[i] = startups[i].label;

        return ppbuf;
}

int main()
{
        int c, cont = 1;

        char *startup, *username, *password, **argv;
        char *labels[16], **plabels = labels;

        struct screen_t *screen = 0;

        FORM *f = 0;
        FIELD **fs = 0;

        signals();

        {
                plabels = labels;
                plabels = startup_labels(labels, sizeof labels / sizeof *labels);

                screen = make_screen(plabels);

                if (plabels != labels)
                        free(plabels);

                if (0 == screen)
                        exit(1);
        }

        f  = screen->form;
        fs = screen->fields;

        form_driver(f, REQ_NEXT_CHOICE);
        draw_screen(screen);

        while(cont && ERR != (c = getch())) {
                switch (c) {
                case KEY_F(1):
                        endwin();
                        execvp("reboot", (char *[]){ "reboot", 0 });
                        break;

                case KEY_F(2):
                        endwin();
                        execvp("shutdown", (char *[]){ "shutdown", 0 });
                        break;

                case '\n': case KEY_ENTER: {
                        form_driver(f, REQ_VALIDATION);

                        form_driver(f, REQ_NEXT_FIELD);
                        form_driver(f, REQ_PREV_FIELD);

                        startup  = field_buffer_trim(fs[1]);
                        if (0 == (argv = startup_argvs(startup))) {
                                fprintf(stderr, "invalid startup label %s\n", startup);
                                free(startup);
                                break;
                        }

                        username = field_buffer_trim(fs[3]);
                        password = field_buffer_trim(fs[5]);

                        endwin();
                        run(username, password, argv);

                        free(startup);
                        free(username);
                        free(password);

                        startup = username = password = 0;

                        draw_screen(screen);
                        pos_form_cursor(f);
                }
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

        return 0;
}
