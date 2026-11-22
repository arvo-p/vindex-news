#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <ctype.h>
#ifdef _WIN32
#include <windows.h>
#define strncasecmp _strnicmp
#else
#include <strings.h>
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

static int get_date_bucket(time_t published_at, time_t now){
	if(!colorScheme->date_separators_enabled || published_at <= 0) return -1;
	if(now <= published_at) return 0;
	double age = difftime(now, published_at);
	if(age < 86400.0) return 0;
	if(age < 7.0 * 86400.0) return 1;
	if(age < 30.0 * 86400.0) return 2;
	if(age < 365.0 * 86400.0) return 3;
	if(age < 2.0 * 365.0 * 86400.0) return 4;
	if(age < 5.0 * 365.0 * 86400.0) return 5;
	return 6;
}

static const char * get_date_bucket_label(int bucket){
	switch(bucket){
		case 1: return " --- One day ago --- ";
		case 2: return " --- One week ago --- ";
		case 3: return " --- One month ago --- ";
		case 4: return " --- One year ago --- ";
		case 5: return " --- Two years ago --- ";
		case 6: return " --- Five years ago --- ";
		default: return NULL;
	}
}

static int get_prev_entry_bucket(global_e * offset_pos, time_t now){
	if(!colorScheme->date_separators_enabled) return -1;
	entry * prev_e = NULL;
	if(selected_tab->tab_mode == TAB_GROUP){
		g_member * gp = getNextGroupEntry(offset_pos->group_member, PREVIOUS);
		if(gp) prev_e = gp->entry;
	}else{
		prev_e = getNextEntry(offset_pos, PREVIOUS);
	}
	if(!prev_e) return 0;
	return get_date_bucket(prev_e->published_at, now);
}

int ui_count_rows_for_sel(global_e * start_pos, int target_sel){
	if(!start_pos || !start_pos->entry || target_sel < 0) return 0;

	global_e iter = *start_pos;
	time_t now = time(NULL);
	int prev_bucket = get_prev_entry_bucket(&iter, now);
	int rows = 0;

	for(int i = 0; i <= target_sel && iter.entry != NULL; i++){
		if(iter.entry->is_separator){
			rows++;
		}
		if(colorScheme->date_separators_enabled){
			int bucket = get_date_bucket(iter.entry->published_at, now);
			if(bucket > 0 && bucket > prev_bucket){
				rows++;
				prev_bucket = bucket;
			}
		}
		rows++;

		if(selected_tab->tab_mode == TAB_GROUP){
			g_member * nxt = getNextGroupEntry(iter.group_member, NEXT);
			if(!nxt) break;
			iter.entry = nxt->entry;
			iter.group_member = nxt;
		}else{
			if(setGlobalEntry(&iter, getNextEntry(&iter, NEXT))) break;
		}
	}
	return rows;
}

static void ui_print_date_separator(int expanded_mode, const char * label){
	if(!label) return;
	ui_reset_with_bg();
	ui_printf("\e[2K");
	if(expanded_mode){
		ui_printf("\e[38;5;%dm\e[%dG\u2502 ", colorScheme->ui_walls, 5);
	}else{
		ui_printf("  ");
	}
	ui_printf("\e[38;5;%dm%s", colorScheme->date_separators_color, label);
	ui_reset_with_bg();
	newline();
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

static int find_keyword_match_at(const char * title, size_t total_len, size_t pos);

static const char * active_search_query(void){
	if(selected_tab && selected_tab->search_query[0] && colorScheme->search_color >= 0){
		return selected_tab->search_query;
	}
	return NULL;
}

static int find_search_match_at(const char * str, size_t total_len, size_t pos, const char * query, size_t qlen){
	if(!query || qlen == 0 || pos + qlen > total_len) return 0;
	return (strncasecmp(str + pos, query, qlen) == 0) ? (int)qlen : 0;
}

/* Render text split at split_idx (first_fg before, second_fg after), overriding search/keyword matches. */
static void ui_print_segmented(const char * str, int len, int split_idx, int first_fg, int second_fg, int allow_keywords){
	size_t total_len = strlen(str);
	const char * search_q = active_search_query();
	size_t qlen = search_q ? strlen(search_q) : 0;
	int do_keywords = (allow_keywords && colorScheme->title_highlight_color >= 0 && highlighted_words_count > 0);

	int cur_fg = -1;
	int i = 0;
	while(i < len){
		int search_len = search_q ? find_search_match_at(str, total_len, (size_t)i, search_q, qlen) : 0;
		if(search_len > 0){
			int print_len = (i + search_len > len) ? (len - i) : search_len;
			if(cur_fg != colorScheme->search_color){
				cur_fg = colorScheme->search_color;
				ui_printf("\e[38;5;%dm", cur_fg);
			}
			ui_printf("%.*s", print_len, str + i);
			i += print_len;
			continue;
		}

		int kw_len = do_keywords ? find_keyword_match_at(str, total_len, (size_t)i) : 0;
		if(kw_len > 0){
			int print_len = (i + kw_len > len) ? (len - i) : kw_len;
			if(cur_fg != colorScheme->title_highlight_color){
				cur_fg = colorScheme->title_highlight_color;
				ui_printf("\e[38;5;%dm", cur_fg);
			}
			ui_printf("%.*s", print_len, str + i);
			i += print_len;
			continue;
		}

		int next_fg = (i < split_idx) ? first_fg : second_fg;
		if(cur_fg != next_fg){
			cur_fg = next_fg;
			ui_printf("\e[38;5;%dm", cur_fg);
		}
		int run_start = i;
		i++;
		while(i < len){
			int fg_here = (i < split_idx) ? first_fg : second_fg;
			if(fg_here != next_fg) break;
			if(search_q && find_search_match_at(str, total_len, (size_t)i, search_q, qlen) > 0) break;
			if(do_keywords && find_keyword_match_at(str, total_len, (size_t)i) > 0) break;
			i++;
		}
		ui_printf("%.*s", i - run_start, str + run_start);
	}
	if(cur_fg != second_fg){
		ui_printf("\e[38;5;%dm", second_fg);
	}
}

static int ui_print_url(const char * raw_url, int max_len){
	if(max_len <= 0) return 0;
	const char * p = strip_url_prefix(raw_url);
	int total_len = (int)strlen(p);
	int print_len = (total_len > max_len) ? max_len : total_len;
	int domain_len = (int)strcspn(p, "/?#");
	ui_print_segmented(p, print_len, domain_len, colorScheme->url, colorScheme->url_path, 0);
	return print_len;
}

static void format_title_sentence_case(const char * src, char * dst, size_t dst_sz){
	if(dst_sz == 0) return;
	snprintf(dst, dst_sz, "%s", src ? src : "");
	if(!src || !*src) return;

	int total_letters = 0;
	int total_upper = 0;
	int alpha_words = 0;
	int in_word = 0;
	for(size_t i = 0; dst[i]; i++){
		unsigned char c = (unsigned char)dst[i];
		int is_alpha = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
		if(is_alpha){
			total_letters++;
			if(c >= 'A' && c <= 'Z') total_upper++;
			if(!in_word){
				alpha_words++;
				in_word = 1;
			}
		}else if(!(c >= '0' && c <= '9')){
			in_word = 0;
		}
	}

	int is_shouting = (alpha_words >= 3 && total_letters > 0 && (total_upper * 100 / total_letters) >= 80 && total_upper == total_letters);
	int seen_first_letter = 0;

	size_t i = 0;
	while(dst[i]){
		unsigned char c = (unsigned char)dst[i];
		int is_alnum = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
		if(!is_alnum){
			i++;
			continue;
		}

		size_t start = i;
		int has_digit = 0;
		int up_cnt = 0;
		int lo_cnt = 0;
		int has_internal_upper = 0;
		while(dst[i]){
			unsigned char wc = (unsigned char)dst[i];
			if(wc >= '0' && wc <= '9'){
				has_digit = 1;
			}else if(wc >= 'A' && wc <= 'Z'){
				up_cnt++;
				if(i > start) has_internal_upper = 1;
			}else if(wc >= 'a' && wc <= 'z'){
				lo_cnt++;
			}else{
				break;
			}
			i++;
		}
		size_t wlen = i - start;

		if(has_digit || wlen == 0){
			if(wlen > 0) seen_first_letter = 1;
			continue;
		}

		if(!seen_first_letter){
			seen_first_letter = 1;
			if(is_shouting){
				for(size_t j = start + 1; j < i; j++){
					if(dst[j] >= 'A' && dst[j] <= 'Z') dst[j] = (char)(dst[j] + ('a' - 'A'));
				}
			}
			continue;
		}

		/* Preserve standalone "I" and contractions like "I'm". */
		if(wlen == 1 && dst[start] == 'I') continue;

		/* Preserve mixed-case identifiers (e.g. IoT, JavaScript, GitHub, iOS) unless shouting. */
		if(!is_shouting && has_internal_upper && lo_cnt > 0) continue;

		/* Preserve short all-caps acronyms (e.g. AI, HN, CSS, NSA) unless shouting. */
		if(!is_shouting && up_cnt == (int)wlen && wlen <= 5) continue;

		if(is_shouting || (up_cnt >= 1 && lo_cnt > 0) || (up_cnt == (int)wlen && wlen > 5)){
			for(size_t j = start; j < i; j++){
				if(dst[j] >= 'A' && dst[j] <= 'Z') dst[j] = (char)(dst[j] + ('a' - 'A'));
			}
		}
	}
}

static int find_title_prefix_len(const char * title){
	if(!title || colorScheme->title_prefix_color < 0 || colorScheme->title_prefix_maxlen <= 0) return 0;
	for(int i = 0; title[i] && i < colorScheme->title_prefix_maxlen; i++){
		if(title[i] == ':' && title[i + 1] == ' ' && i > 0){
			return i + 1;
		}
	}
	return 0;
}

int url_has_ext(const char * url, const char * ext){
	if(!url || !ext) return 0;
	size_t ext_len = strlen(ext);
	size_t path_len = strcspn(url, "?#");
	if(path_len < ext_len) return 0;
	return strncasecmp(url + path_len - ext_len, ext, ext_len) == 0;
}

int is_video_url(const char * url){
	if(!url) return 0;
	return (strstr(url, "youtube.com") != NULL ||
			strstr(url, "youtu.be") != NULL ||
			strstr(url, "odysee.com") != NULL ||
			url_has_ext(url, ".mp4") ||
			url_has_ext(url, ".webm") ||
			url_has_ext(url, ".mkv"));
}

int is_pdf_url(const char * url){
	return url_has_ext(url, ".pdf");
}

static void ui_print_media_prefix(const char * url){
	if(is_video_url(url) && colorScheme->media_video_color >= 0){
		ui_printf("\e[27;38;5;%dm\u25B6 ", colorScheme->media_video_color);
	}else if(is_pdf_url(url) && colorScheme->media_pdf_color >= 0){
		ui_printf("\e[27;38;5;%dm\u25C8 ", colorScheme->media_pdf_color);
	}else{
		ui_printf("\e[27;38;5;%dm%c ", colorScheme->urlprefix, colorScheme->urlprefix_char);
	}
}

static int find_keyword_match_at(const char * title, size_t total_len, size_t pos){
	int best_len = 0;
	for(int k = 0; k < highlighted_words_count; k++){
		const char * kw = highlighted_words[k];
		size_t kw_len = strlen(kw);
		if(kw_len == 0 || pos + kw_len > total_len) continue;
		if((int)kw_len <= best_len) continue;
		if(strncasecmp(title + pos, kw, kw_len) != 0) continue;

		unsigned char first_c = (unsigned char)kw[0];
		unsigned char last_c = (unsigned char)kw[kw_len - 1];
		if(isalnum(first_c) && pos > 0 && isalnum((unsigned char)title[pos - 1])) continue;
		if(isalnum(last_c) && (pos + kw_len < total_len) && isalnum((unsigned char)title[pos + kw_len])) continue;

		best_len = (int)kw_len;
	}
	return best_len;
}

static void ui_print_title_text(const char * title, int len, int seen){
	int base_fg = seen ? colorScheme->seen : colorScheme->entries;
	int prefix_len = seen ? 0 : find_title_prefix_len(title);
	ui_print_segmented(title, len, prefix_len, colorScheme->title_prefix_color, base_fg, !seen);
}

int the_entry_print(entry * entry_item, int expanded_mode, int maxUrl, int title_maxlen, int i, short refresh){
	int shorted = 0;
	int is_selected = (i == selected_tab->sel);
	int zebra_bg = get_row_zebra_bg(i);
	int zebra_dot = should_zebra_dot(i);

	char formatted_title[sizeof(entry_item->title)];
	const char * title_ptr = entry_item->title;
	if(colorScheme->title_sentence_case){
		format_title_sentence_case(entry_item->title, formatted_title, sizeof(formatted_title));
		title_ptr = formatted_title;
	}
	
	int len = strlen(title_ptr);
	if(len>=title_maxlen){
		len = title_maxlen-1;
		shorted = 1;
	}

	ui_reset_with_bg();
	ui_printf("\e[2K");

	if(expanded_mode){
		if(entry_item->bookmarked) ui_printf("\e[38;5;%dm\e[3G\u2691", colorScheme->bookmark_color);
		ui_printf("\e[38;5;%dm\e[%dG\u2502", colorScheme->ui_walls, 5);
	} else {
		if(entry_item->bookmarked) ui_printf("\e[38;5;%dm\u2691", colorScheme->bookmark_color);
		else ui_printf(" ");
	}
	if(zebra_bg >= 0) ui_printf("\e[48;5;%dm", zebra_bg);
	if(zebra_dot){
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

	ui_print_title_text(title_ptr, len, entry_item->seen);
	if(shorted) ui_printf("...");
		
	if(entry_item->downloaded) ui_printf(" 🌠");

	if(is_selected) ui_printf("\e[27m");
	ui_printf("\e[0K");

	if(expanded_mode){
		ui_printf("\e[7G"); // cursor pos 7 :: title
		if(is_selected && shorted){
			ui_printf("\e[7m");
			ui_print_title_text(title_ptr, (int)strlen(title_ptr), entry_item->seen);
			ui_printf(" \e[27m");
		} 
		if(zebra_bg >= 0){
			ui_printf("\e[107G");
			ui_apply_bg();
			ui_printf("\e[0K");
		}
		ui_printf("\e[108G"); // cursor pos 108 :: url
		ui_print_media_prefix(entry_item->url);
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
			ui_print_media_prefix(entry_item->url);
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
	
	time_t now = time(NULL);
	int prev_bucket = get_prev_entry_bucket(huidig, now);
	int rows_used = 0;
	int i;
	for(i=0; rows_used < maxHeight; i++){
		if(huidig->entry->is_separator){
			if(rows_used + 1 >= maxHeight && i > selected_tab->sel) break;
			ui_print_date_separator(expanded_mode, " --- Caught up to here --- ");
			rows_used++;
		}
		if(colorScheme->date_separators_enabled){
			int bucket = get_date_bucket(huidig->entry->published_at, now);
			if(bucket > 0 && bucket > prev_bucket){
				if(rows_used + 1 >= maxHeight && i > selected_tab->sel) break;
				ui_print_date_separator(expanded_mode, get_date_bucket_label(bucket));
				rows_used++;
				prev_bucket = bucket;
			}
		}

		the_entry_print(huidig->entry, expanded_mode, maxUrl, title_maxlen, i, refresh);
		rows_used++;

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
