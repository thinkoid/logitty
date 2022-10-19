/* -*- mode: c; -*- */

#ifndef TUI_UI_H
#define TUI_UI_H

struct screen_t;

void initialize();

struct screen_t *make_screen();
void free_screen(struct screen_t *screen);

void draw_screen(struct screen_t *screen);
void refresh_window(struct screen_t *screen);

FORM *screen_form(struct screen_t *screen);
FIELD **screen_fields(struct screen_t *screen);

char **startup_argv(const char *startup);

#endif /* TUI_UI_H */
