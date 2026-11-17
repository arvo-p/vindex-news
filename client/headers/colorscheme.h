#ifndef COLORSCHEME_H
#define COLORSCHEME_H

typedef struct colorscheme{
	int header;
	int tab_title;
	int entries;	
	int urlprefix;
	int url;
	int url_path;
	int seen;
	int ui_walls;
	int entries_nav;
	int background;
	int zebra_enabled;
	int zebra_even_color;
	int zebra_odd_color;
	int zebra_dot_enabled;
	int zebra_dot_color;
	int title_sentence_case;
	int title_prefix_color;
	int title_prefix_maxlen;
	char urlprefix_char;
} colorscheme;

typedef struct config{
	char name[64];
	char type;
	void * value;
	struct config * next;
} config;

extern colorscheme * colorScheme;
extern config * uiConfig;

int initColorscheme();

#endif
