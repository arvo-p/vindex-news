#ifndef UI_H
#define UI_H

#include "main.h"

#define NEXT 0
#define PREVIOUS 1

int getEntriesNum();
int list_selector_move(int step);

void draw_update(short refresh);
void draw_cError();
void draw_statusbar();
void ClearLastLine();
void ui_flush();
void get_terminal_size(int *cols, int *rows);
int ui_count_rows_for_sel(global_e * start_pos, int target_sel);
int url_has_ext(const char * url, const char * ext);
int is_video_url(const char * url);
int is_pdf_url(const char * url);
void * winResize_Loop(void * arg);

extern int displayThreshold;
extern char statusbar_notify[256];
extern int entries_sz;

#endif
