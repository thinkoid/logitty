/* -*- mode: c; -*- */

#ifndef TUI_UI_H
#define TUI_UI_H

struct screen_t {
        FORM *form;
        FIELD **fields;
        WINDOW *win, *sub;
};

struct screen_t *make_screen(char **labels);
void free_screen(struct screen_t *screen);

void draw_screen(struct screen_t *screen);

#endif /* TUI_UI_H */
