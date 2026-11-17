#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/ioctl.h>
#endif
#include "headers/ui.h"
#include "headers/main.h"
#include "headers/tabs.h"
#include "headers/colorscheme.h"
#include "headers/nav.h"
#include "headers/cmd.h"

int displayThreshold = 150;
char statusbar_notify[256] = {0};
int entries_sz=0;

static char *ui_buffer = NULL;
static size_t ui_buffer_size = 0;
static size_t ui_buffer_pos = 0;

void ui_printf(const char *format, ...){
    if (!ui_buffer){
        ui_buffer_size = 1024 * 64;
        ui_buffer = malloc(ui_buffer_size);
    }
    
    va_list args;
    va_start(args, format);
    int needed = vsnprintf(ui_buffer + ui_buffer_pos, ui_buffer_size - ui_buffer_pos, format, args);
    va_end(args);

    if (needed < 0) return;

    if ((size_t)needed >= ui_buffer_size - ui_buffer_pos){
        ui_buffer_size = (ui_buffer_pos + needed) * 2 + 1024;
        ui_buffer = realloc(ui_buffer, ui_buffer_size);
        va_start(args, format);
        vsnprintf(ui_buffer + ui_buffer_pos, ui_buffer_size - ui_buffer_pos, format, args);
        va_end(args);
    }
    
    ui_buffer_pos += needed;
}

void ui_flush(){
    if (ui_buffer && ui_buffer_pos > 0){
        fwrite(ui_buffer, 1, ui_buffer_pos, stdout);
        fflush(stdout);
        ui_buffer_pos = 0;
    }
}

void draw_header();
void draw_command_line();
void draw_cError();
void draw_statusbar();
void draw_tabs();

void newline();
void EmptyScreen();
void SetCursorLastLine();

void get_terminal_size(int *cols, int *rows){
#ifdef _WIN32
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
	*cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
	*rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
#else
	struct winsize w;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0 && w.ws_row > 0) {
		*cols = w.ws_col;
		*rows = w.ws_row;
	} else if (*cols == 0 || *rows == 0) {
		*cols = 80;
		*rows = 24;
	}
#endif
}

void * winResize_Loop(void * arg){
	int m = display_mode * (-2) + 1;
	while(1){
		get_terminal_size(&winSZ[0], &winSZ[1]);

		if(winSZ[0] != winSZ[2] || winSZ[1] != winSZ[3]){
		
			if(winSZ[0]*m > displayThreshold*m){
				display_mode = !display_mode;
				m = display_mode * (-2) + 1;
				tabs_updateDisplayMode(selected_tab);
			}

			draw_update(true);
		}

		winSZ[2] = winSZ[0];
		winSZ[3] = winSZ[1];
#ifdef _WIN32
		Sleep(300);
#else
		usleep(300000);
#endif
	}

	return 0;
}

void draw_header(){
	char * appname = "VINDEX";
	int margin = (winSZ[0] - (int)strlen(appname))/2-8;
	// int expanded = winSZ[0] > displayThreshold; // unused
	ui_printf("\n");
	for(int i=0;i<margin/8;i++) ui_printf("\t");
	for(int i=0;i<margin%8;i++) ui_printf(" ");
	ui_printf("\e[38;5;%dm",colorScheme->tab_title);
	ui_printf("%s\e[0m\n", appname);

	return;
}

int display_urltree(){
	return 0;
}

static void ui_apply_bg(void){
	if(colorScheme->background >= 0){
		ui_printf("\e[48;5;%dm", colorScheme->background);
	}else{
		ui_printf("\e[49m");
	}
}

static void ui_reset_with_bg(void){
	ui_printf("\e[0m");
	ui_apply_bg();
}

static int is_zebra_odd_row(int row_idx){
	return ((row_idx + selected_tab->line_offset) % 2 == 1);
}

static int get_row_zebra_bg(int row_idx){
	if(!colorScheme->zebra_enabled) return -1;
	return is_zebra_odd_row(row_idx)
		? colorScheme->zebra_odd_color
		: colorScheme->zebra_even_color;
}

static int should_zebra_dot(int row_idx){
	return (colorScheme->zebra_dot_enabled && is_zebra_odd_row(row_idx));
}

static const char * strip_url_prefix(const char * url){
	if(!url) return "";
	if(strncmp(url, "https://", 8) == 0) url += 8;
	else if(strncmp(url, "http://", 7) == 0) url += 7;
	if(strncmp(url, "www.", 4) == 0) url += 4;
	return url;
}

static int ui_print_url(const char * raw_url, int max_len){
	if(max_len <= 0) return 0;
	const char * p = strip_url_prefix(raw_url);
	int total_len = (int)strlen(p);
	int domain_len = (int)strcspn(p, "/?#");
	if(domain_len > max_len) domain_len = max_len;

	ui_printf("\e[38;5;%dm%.*s", colorScheme->url, domain_len, p);
	int printed = domain_len;

	if(domain_len < total_len && printed < max_len){
		int rem = max_len - printed;
		int path_len = total_len - domain_len;
		if(path_len > rem) path_len = rem;
		ui_printf("\e[38;5;%dm%.*s", colorScheme->url_path, path_len, p + domain_len);
		printed += path_len;
	}
	return printed;
}

int the_entry_print(entry * entry_item, int expanded_mode, int maxUrl, int title_maxlen, int i, short refresh){
	int shorted = 0;
	int is_selected = (i == selected_tab->sel);
	int zebra_bg = get_row_zebra_bg(i);
	int zebra_dot = should_zebra_dot(i);
	
	int len = strlen(entry_item->title);
	if(len>=title_maxlen){
		len = title_maxlen-1;
		shorted = 1;
	}

	ui_reset_with_bg();
	ui_printf("\e[2K");

	if(expanded_mode){
		ui_printf("\e[38;5;%dm\e[%dG%s", colorScheme->ui_walls, 5, entry_item->is_separator ? "\u251C" : "\u2502");
	} else {
		if(entry_item->is_separator) ui_printf("\e[38;5;%dm\u2500", colorScheme->ui_walls);
		else ui_printf(" ");
	}
	if(zebra_bg >= 0) ui_printf("\e[48;5;%dm", zebra_bg);
	if(entry_item->is_separator){
		ui_printf("\u2500");
	} else if(zebra_dot){
		ui_printf("\e[38;5;%dm\u00B7", colorScheme->zebra_dot_color);
	} else {
		ui_printf(" ");
	}

	if(entry_item->seen) ui_printf("\e[38;5;%dm",colorScheme->seen);
	else ui_printf("\e[38;5;%dm", colorScheme->entries);
	
	if(is_selected){
		setGlobalEntry(selected_tab->old_entry, entry_item);
		ui_printf("\e[7m");
	}

	ui_printf("%.*s",len,entry_item->title);
	if(shorted) ui_printf("...");
		
	if(entry_item->downloaded) ui_printf(" 🌠");
	if(entry_item->bookmarked) ui_printf(" ⭐");

	if(is_selected) ui_printf("\e[27m");
	ui_printf("\e[0K");

	if(expanded_mode){
		ui_printf("\e[7G"); // cursor pos 7 :: title
		if(is_selected && shorted) ui_printf("\e[38;5;%d;7m%s \e[27m", colorScheme->entries, entry_item->title); 
		if(zebra_bg >= 0){
			ui_printf("\e[107G");
			ui_apply_bg();
			ui_printf("\e[0K");
		}
		ui_printf("\e[108G"); // cursor pos 108 :: url
		ui_printf("\e[27;38;5;%dm%c ",colorScheme->urlprefix,colorScheme->urlprefix_char);
		if(is_selected) ui_printf("\e[4m"); // underline open
		ui_print_url(entry_item->url_nohttp ? entry_item->url_nohttp : entry_item->url, maxUrl);
		if(is_selected) ui_printf("\e[24m"); // underline close
	}else{
		if(zebra_bg >= 0 && winSZ[0] > 1){
			ui_printf("\e[%dG", winSZ[0]);
			ui_apply_bg();
			ui_printf("\e[0K");
		}
		if(is_selected){
			ui_printf("\e[27;38;5;%dm", colorScheme->urlprefix);
			newline();
			ui_printf("\e[2K");
			ui_printf(" | ");
			ui_print_url(entry_item->url_nohttp ? entry_item->url_nohttp : entry_item->url, maxUrl);
		}
	}
	ui_reset_with_bg();
	newline();
	return 0;
}

int display_entries(short refresh){
	if (selected_tab->tab_mode == TAB_WEBFEEDS){
		int expanded_mode = winSZ[0]>displayThreshold;
		int maxHeight = expanded_mode?winSZ[1]-8:winSZ[1]-4;
		int maxUrl = winSZ[0]/2 - 15; // Adjusted for wider alias column
		
		if (webfeeds_count == 0){
			ui_reset_with_bg();
			ui_printf("\n    (No webfeeds found)\e[0K\n");
			return 0;
		}

		for(int i=0; i < maxHeight; i++){
			int idx = selected_tab->line_offset + i;
			if (idx >= webfeeds_count){
				ui_reset_with_bg();
				ui_printf("\e[2K\n");
				continue;
			}
			
			int is_selected = (i == selected_tab->sel);
			int zebra_bg = get_row_zebra_bg(i);
			int zebra_dot = should_zebra_dot(i);
			ui_reset_with_bg();
			ui_printf("\e[2K ");
			
			// --- Alias (acts like Title: reversed when selected) ---
			if (zebra_bg >= 0) ui_printf("\e[48;5;%dm", zebra_bg);
			if (zebra_dot) ui_printf("\e[38;5;%dm\u00B7", colorScheme->zebra_dot_color);
			else ui_printf(" ");
			ui_printf("\e[38;5;%dm", colorScheme->entries);
			if (is_selected) ui_printf("\e[7m");
			
			int alias_len = strlen(webfeeds_list[idx].alias);
			if (alias_len > 0){
				ui_printf("%-25.25s", webfeeds_list[idx].alias);
			} else {
				ui_printf("%-25.25s", "");
			}
			
			if (is_selected) ui_printf("\e[27m");
			if (zebra_bg >= 0) ui_apply_bg();
			ui_printf(" ");
			
			// --- Separator ---
			ui_printf("\e[38;5;%dm", colorScheme->ui_walls);
			ui_printf("\u2502 ");
			
			// --- URL (acts like URL: underlined when selected) ---
			if (i == selected_tab->sel) ui_printf("\e[4m");
			
			int printed_url = ui_print_url(webfeeds_list[idx].url, maxUrl);
			for (int pad = printed_url; pad < maxUrl; pad++) ui_printf(" ");
			ui_printf(" ");
			
			if (i == selected_tab->sel) ui_printf("\e[24m");
			
			// --- Separator ---
			ui_printf("\e[38;5;%dm", colorScheme->ui_walls);
			ui_printf("\u2502 ");
			
			// --- Groups ---
			ui_printf("\e[38;5;%dm", colorScheme->entries);
			for (int g = 0; g < webfeeds_list[idx].group_count; g++){
				ui_printf("[%s] ", webfeeds_list[idx].groups[g]->name);
			}
			
			ui_reset_with_bg(); // Reset formatting while preserving background
			newline();
		}
		return 0;
	}

	global_e * huidig = selected_tab->working; 
	if(setGlobalEntry(huidig, selected_tab->offset->entry)){
		ui_reset_with_bg();
		ui_printf("\n    (Empty feed)\e[0K\e[J\n");
		return 0;
	}
	huidig->group_member = selected_tab->offset->group_member;
	
	int expanded_mode = winSZ[0]>displayThreshold;
	int maxHeight = expanded_mode?winSZ[1]-8:winSZ[1]-4; 

	int maxUrl = expanded_mode?winSZ[0]-112:winSZ[0]-3;
	int title_maxlen = expanded_mode?94:winSZ[0]-4;
	
	int i;
	for(i=0;i < maxHeight;i++){		
		the_entry_print(huidig->entry, expanded_mode, maxUrl, title_maxlen, i, refresh);

		if(selected_tab->tab_mode == TAB_GROUP){
			g_member * nextInGroup = getNextGroupEntry(huidig->group_member, NEXT);
			if(!nextInGroup) break;
			huidig->entry = nextInGroup->entry;
			huidig->group_member = nextInGroup;
			continue;
		}
		
		if(setGlobalEntry(huidig, getNextEntry(huidig, NEXT))) break;
	}

	ui_reset_with_bg();
	if(expanded_mode){
		ui_printf("\e[2;38;5;%dm", colorScheme->ui_walls);
		ui_printf("\e[%dG\u2502\e[0K",5);
		ui_printf("\n\e[%dG\u2514\u2500\e[0K",5);
	}else{
		int margin = winSZ[0]-10;
		ui_printf("\e[%dG\e[2K",margin);
	}
	ui_printf("\e[22;38;5;%dm%d\\%d\e[0K",colorScheme->entries_nav, (selected_tab->sel)+(selected_tab->line_offset),getEntriesNum()-1); 
	ui_printf("\e[J");

	return 0;
}

void draw_tabs(){
	int len=0;
	ui_printf("\e[48;5;234m");
	int restore_bg_color = 0;

	// Write something in case draw_tabs() exceeds screen width
	
	for(len=0;tabs[len]!=0;len++){
		if(restore_bg_color){
			restore_bg_color = 0;
			ui_printf("\e[48;5;234m");
		}
		if(tabs[len] == selected_tab){
			ui_printf("\e[48;5;236m");
			restore_bg_color = 1;
		}
		ui_printf("    %s    ", tabs[len]->title);
	}
	ui_reset_with_bg();
	ui_printf("\e[0K");
	newline();
	ui_printf("\e[2K");
	newline();
}

void draw_statusbar(){
	int expanded_mode = winSZ[0]>displayThreshold;
	int sz = (int)strlen(statusbar_notify);
	int space_sz = winSZ[0]-sz;
	char * style = "\e[48;5;233m\e[38;5;188m";
	
	SetCursorLastLine();
	ui_reset_with_bg();

	if(statusbar_notify[0] != 0){
		if(expanded_mode){
			for(int i=0;i<space_sz;i++)ui_printf(" ");
			ui_printf("%s%s", style, statusbar_notify);
		}else{
			ui_printf("\r%s%.50s...\e[0K", style, statusbar_notify);
		}
	}else{
		ui_printf("\e[2K");
	}
}

void draw_cError(){
	SetCursorLastLine();
	ui_reset_with_bg();
	if(cError[0]) ui_printf("\r\e[31m%s\e[0K", cError);
	ui_flush();
}

void EmptyScreen(){
	ui_reset_with_bg();
	ui_printf("\e[H\e[2J");
}

void newline(){
	ui_printf("\n");
}

void draw_command_line(){
	ui_reset_with_bg();
	ui_printf("\e[%d;0H", winSZ[1]);
	ui_printf("\e[2K");

	if(cHelper){
		ui_printf("\r");
		for(int i=0;i<cmd_count-1;i++) ui_printf(" ");
		ui_printf("\e[2m%s\e[22m", cHelper+cHelper_len);
	}
	
	ui_printf("\r");
	ui_reset_with_bg();
	ui_printf("%s", cmd_buffer);
	ui_flush();
}

void draw_update(short refresh){	
	if(cmd_mode){
		draw_command_line();
		return;
	}
	
	if(refresh == 0){
		ui_reset_with_bg();
		ui_printf("\e[3;0H");
	}else{
		EmptyScreen();
		draw_tabs();
	}

	if(selected_tab->tab_mode == TAB_TREE) display_urltree();
	else display_entries(refresh);

	draw_statusbar();

	ui_flush();
	return;
}

void SetCursorLastLine(){
	ui_printf("\e[%d;0H",winSZ[1]);
}

void ClearLastLine(){
	ui_reset_with_bg();
	ui_printf("\e[%d;0H\e[2K",winSZ[1]);
	ui_flush();
}
