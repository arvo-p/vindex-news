#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <time.h>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#include <shellapi.h>
#include <direct.h>
#include <io.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/wait.h>
#endif

#include "headers/main.h"
#include "headers/nav.h"
#include "headers/ui.h"
#include "headers/cmd.h"
#include "headers/tabs.h"
#include "headers/colorscheme.h"
#include "headers/loadfiles.h"
                                 
PublicLoad * Load = NULL;

cat_group * initial_group = NULL;
entry * initial_entry = NULL;
entry_parent * initial_parent = NULL;

int winSZ[4] = {0,0,0,0};
int display_mode = 1;
int server_is_running = 0;

int entry_view(entry * entry);

static int contains_icase(const char * haystack, const char * needle){
	if(!haystack || !needle || !*needle) return 0;
	size_t nlen = strlen(needle);
	for(const char * p = haystack; *p; p++){
		size_t j = 0;
		while(j < nlen && p[j] && tolower((unsigned char)p[j]) == tolower((unsigned char)needle[j])){
			j++;
		}
		if(j == nlen) return 1;
	}
	return 0;
}

static int is_entry_bookmarked(const entry * e, void * ctx){
	(void)ctx;
	return e->bookmarked;
}

static int is_entry_matching_query(const entry * e, void * ctx){
	const char * query = (const char *)ctx;
	return contains_icase(e->title, query) || contains_icase(e->url, query);
}

static void free_temp_group(cat_group * grp){
	if(!grp) return;
	g_member * mem = grp->first_member;
	while(mem){
		g_member * nxt = mem->next;
		free(mem);
		mem = nxt;
	}
	free(grp);
}

/* Filter entries from the current tab into a new TAB_GROUP tab. Returns 1 if opened. */
static int open_filtered_tab(const char * tab_title, int (*predicate)(const entry *, void *), void * ctx, const char * empty_msg){
	if(selected_tab->tab_mode == TAB_WEBFEEDS) return 0;

	cat_group * filtered_group = calloc(1, sizeof(cat_group));
	if(!filtered_group) return 0;
	snprintf(filtered_group->name, sizeof(filtered_group->name), "%s", tab_title);

	global_e iter;
	iter.entry = NULL; iter.child = NULL; iter.parent = NULL; iter.group_member = NULL;

	if(selected_tab->tab_mode == TAB_SIMPLE){
		setGlobalEntry(&iter, initial_entry);
	}else if(selected_tab->tab_mode == TAB_GROUP){
		if(selected_tab->category && selected_tab->category->first_member){
			iter.group_member = selected_tab->category->first_member;
			setGlobalEntry(&iter, selected_tab->category->first_member->entry);
		}
	}else if(selected_tab->tab_mode == TAB_URLBASE){
		if(selected_tab->offset->parent && selected_tab->offset->parent->first_child){
			child_entry * first = (child_entry *)selected_tab->offset->parent->first_child;
			iter.child = first;
			setGlobalEntry(&iter, first->entry);
		}
	}

	g_member * last_member = NULL;
	while(iter.entry != NULL){
		if(predicate(iter.entry, ctx)){
			g_member * new_mem = calloc(1, sizeof(g_member));
			new_mem->entry = iter.entry;
			if(!filtered_group->first_member){
				filtered_group->first_member = new_mem;
			}else{
				last_member->next = new_mem;
				new_mem->previous = last_member;
			}
			last_member = new_mem;
			filtered_group->count++;
		}

		if(selected_tab->tab_mode == TAB_GROUP){
			g_member * nxt = getNextGroupEntry(iter.group_member, 0);
			if(!nxt) break;
			iter.entry = nxt->entry;
			iter.group_member = nxt;
		}else{
			if(setGlobalEntry(&iter, getNextEntry(&iter, 0))) break;
		}
	}

	if(filtered_group->count > 0){
		tab * ntab = tabs_newtab((char *)tab_title, filtered_group->first_member->entry, TAB_GROUP);
		if(!ntab){
			free_temp_group(filtered_group);
			snprintf(statusbar_notify, sizeof(statusbar_notify), "Maximum tab limit reached.");
			draw_statusbar();
			return 0;
		}
		ntab->category = filtered_group;
		ntab->offset->group_member = filtered_group->first_member;
		selected_tab = ntab;
		tabs_updateDisplayMode(selected_tab);
		return 1;
	}

	free_temp_group(filtered_group);
	snprintf(statusbar_notify, sizeof(statusbar_notify), "%s", empty_msg);
	draw_statusbar();
	return 0;
}

enum SpecialKey {
	KEY_SPECIAL_NONE = 0,
	KEY_SPECIAL_UP = 72,
	KEY_SPECIAL_DOWN = 80,
	KEY_SPECIAL_PAGE_UP = 73,
	KEY_SPECIAL_PAGE_DOWN = 81
};

static void reset_terminal_colors(void){
	printf("\e[0m\e[39;49m\e[H\e[2J\e[?1049l\e[0m\e[39;49m\e[?25h");
	fflush(stdout);
}

#ifndef _WIN32
static struct termios orig_termios;
static int termios_saved = 0;

/* Restore terminal attributes, colors, and cursor visibility on exit or signal. */
static void restore_terminal(void){
	if(termios_saved){
		tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
		termios_saved = 0;
	}
	reset_terminal_colors();
}

static void handle_exit_signal(int sig){
	(void)sig;
	restore_terminal();
	_exit(0);
}

static void enable_raw_mode(void){
	if(tcgetattr(STDIN_FILENO, &orig_termios) == 0){
		termios_saved = 1;
		atexit(restore_terminal);
		signal(SIGINT, handle_exit_signal);
		signal(SIGTERM, handle_exit_signal);

		struct termios raw = orig_termios;
		raw.c_iflag &= ~(ICRNL | IXON);
		raw.c_lflag &= ~(ICANON | ECHO | IEXTEN);
		raw.c_cc[VMIN] = 1;
		raw.c_cc[VTIME] = 0;
		tcsetattr(STDIN_FILENO, TCSANOW, &raw);
	}
}

static int kbhit_pending(void){
	struct timeval tv = {0, 0};
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(STDIN_FILENO, &fds);
	return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}

static int wait_input_ms(int timeout_ms){
	struct timeval tv;
	tv.tv_sec = timeout_ms / 1000;
	tv.tv_usec = (timeout_ms % 1000) * 1000;
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(STDIN_FILENO, &fds);
	int r;
	while((r = select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv)) < 0 && errno == EINTR){
		FD_ZERO(&fds);
		FD_SET(STDIN_FILENO, &fds);
	}
	return r > 0;
}

static int read_raw_byte(void){
	unsigned char ch;
	ssize_t n;
	while((n = read(STDIN_FILENO, &ch, 1)) < 0){
		if(errno == EINTR) continue;
		return EOF;
	}
	if(n == 0) return EOF;
	return (int)ch;
}
#else
static int kbhit_pending(void){
	return _kbhit() != 0;
}
#endif

/* Read a single key event, normalizing Windows scan codes and Linux ANSI escapes. */
static int read_key_event(int *is_special, int *key_out){
	*is_special = 0;
#ifdef _WIN32
	int input = getch();
	if(input == 224 || input == 0 || input == -32){
		*is_special = 1;
		*key_out = getch();
		return 0;
	}
	*key_out = input;
	return 0;
#else
	int input = read_raw_byte();
	if(input == EOF) return -1;

	if(input == 27){
		if(!wait_input_ms(25)){
			*is_special = 1;
			*key_out = KEY_SPECIAL_NONE;
			return 0;
		}
		int next = read_raw_byte();
		if(next == '[' || next == 'O'){
			int code = read_raw_byte();
			*is_special = 1;
			if(code == 'A'){
				*key_out = KEY_SPECIAL_UP;
			}else if(code == 'B'){
				*key_out = KEY_SPECIAL_DOWN;
			}else if(code == '5' || code == '6'){
				int tail = wait_input_ms(10) ? read_raw_byte() : 0;
				if(tail == '~'){
					*key_out = (code == '5') ? KEY_SPECIAL_PAGE_UP : KEY_SPECIAL_PAGE_DOWN;
				}else{
					*key_out = KEY_SPECIAL_NONE;
				}
			}else{
				// Drain remaining bytes of unhandled CSI sequences so they aren't treated as typed text.
				while(code >= 0 && (code < '@' || code > '~') && wait_input_ms(10)){
					code = read_raw_byte();
				}
				*key_out = KEY_SPECIAL_NONE;
			}
			return 0;
		}
		*is_special = 1;
		*key_out = KEY_SPECIAL_NONE;
		return 0;
	}

	if(input == '\n') input = 13;
	else if(input == 127) input = 8;

	*key_out = input;
	return 0;
#endif
}

void * server_thread(void * arg){
#ifdef _WIN32
	FILE *pipe = _popen("..\\server\\vindexServer.exe 2>&1", "r");
#else
	char cmd_buf[PATH_MAX + 32];
	const char *server_cmd = "vindexServer 2>&1";
	if(access("./vindexServer", X_OK) == 0){
		server_cmd = "./vindexServer 2>&1";
	}else if(access("./server/vindexServer", X_OK) == 0){
		server_cmd = "./server/vindexServer 2>&1";
	}else if(access("../server/vindexServer", X_OK) == 0){
		server_cmd = "../server/vindexServer 2>&1";
	}else{
		char exe_path[PATH_MAX];
		ssize_t exe_len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
		if(exe_len > 0){
			exe_path[exe_len] = '\0';
			char *slash = strrchr(exe_path, '/');
			if(slash){
				*slash = '\0';
				snprintf(cmd_buf, sizeof(cmd_buf), "\"%s/vindexServer\" 2>&1", exe_path);
				server_cmd = cmd_buf;
			}
		}
	}
	FILE *pipe = popen(server_cmd, "r");
#endif
	if (pipe){
		char buf[256];
		int pos = 0;
		int ch;
		while ((ch = fgetc(pipe)) != EOF){
			if (ch == '\n' || pos >= 255){
				buf[pos] = '\0';
				if(pos > 0){
					strncpy(statusbar_notify, buf, 255);
					draw_statusbar();
					ui_flush();
				}
				pos = 0;
			} else if (ch != '\r'){
				buf[pos++] = ch;
			}
		}
#ifdef _WIN32
		_pclose(pipe);
#else
		pclose(pipe);
#endif
	}
	Load->ReloadEntries();
	strncpy(statusbar_notify, "Refresh complete.", 255);
	draw_update(true);
	server_is_running = 0;
	return NULL;
}

int main(int argc, char * argv[]){
	pthread_t tWinResize;
	
	int highlighted_entry_id = -1; 
	if(argc == 2) highlighted_entry_id = atoi(argv[1]);

	Load = malloc(sizeof(PublicLoad));
	if(LOAD_INIT(Load) != 0) exit(0);
	initColorscheme();
	
	tabs[0] = tabs_newtab("Main", initial_entry, TAB_SIMPLE);
	selected_tab = tabs[0];

#ifdef _WIN32
	SetConsoleOutputCP(CP_UTF8);
#else
	enable_raw_mode();
#endif
	srand(time(NULL));  
	
	get_terminal_size(&winSZ[0], &winSZ[1]);
	winSZ[2] = winSZ[0];
	winSZ[3] = winSZ[1];

	if (winSZ[0] > displayThreshold) display_mode = 0;
	else display_mode = 1;
	
	printf("\e[?1049h\e[?25l");
	fflush(stdout);
	draw_update(true);

	pthread_create(&tWinResize, NULL, &winResize_Loop, NULL);
	while(!winSZ[0]); //wait for winresize

	int input;
	int is_special = 0;
	int scroll_ret = 0;
	int redraw = false;
	int g_pressed = 0;

	printf("\e[?25l"); //invisible cursor
	fflush(stdout);
	while(1){
		redraw = false;
		if(read_key_event(&is_special, &input) != 0) break;
		
		if(is_special){
			int scan_code = input;
			
			if (cmd_mode){
				if (scan_code == KEY_SPECIAL_UP){ // Up arrow
					cHelper_selected_cmd--;
					run_cmd(1);
				} else if (scan_code == KEY_SPECIAL_DOWN){ // Down arrow
					cHelper_selected_cmd++;
					run_cmd(1);
				}
				draw_update(false);
				continue;
			}

			int pageSize = (display_mode == 0) ? (winSZ[1] - 8) : (winSZ[1] - 5);
			if(pageSize < 1) pageSize = 1;

			if(scan_code == KEY_SPECIAL_PAGE_UP){ // Page Up
				list_selector_move(-pageSize);
			}else if(scan_code == KEY_SPECIAL_PAGE_DOWN){ // Page Down
				list_selector_move(pageSize);
			}
			draw_update(false);
			g_pressed = 0;
			continue;
		}

		if(!cmd_mode && kbhit_pending()) continue; // Prevent input spamming in normal mode
		
		selected_tab->selPrevious = selected_tab->sel;
		
		if(cError[0] || statusbar_notify[0]){
			statusbar_notify[0] = 0;
			cError[0] = 0;
			ClearLastLine();
		}
	
		if(cmd_mode){
			int is_search = (cmd_buffer[0] == '/');
			switch(input){
				case 13:
					exit_command_mode();
					cHelper = NULL;
					if(is_search){
						if(cmd_count > 1){
							char tab_title[128];
							snprintf(tab_title, sizeof(tab_title), "/%.19s", cmd_buffer + 1);
							redraw = open_filtered_tab(tab_title, is_entry_matching_query, cmd_buffer + 1, "No matches found.");
							if(redraw){
								snprintf(selected_tab->search_query, sizeof(selected_tab->search_query), "%s", cmd_buffer + 1);
							}
						}
					}else{
						if(run_cmd(0)!=0) draw_cError();
					}
					memset(cmd_buffer, 0, sizeof(cmd_buffer));
					cmd_count = 0;
					cHelper_selected_cmd = 0;
					break;
				case 8:
					if(cmd_count>0) cmd_buffer[--cmd_count] = 0;
					if(cmd_count == 0) exit_command_mode();
					cHelper_selected_cmd = 0;
					if(!is_search){
						if(!cHelper){
							if (!kbhit_pending()) run_cmd(1);
						} else if(cHelper_len) cHelper_len--;
						else cHelper = NULL;
					}
					break;
				case 9:
					if(!is_search) cmd_autocomplete();
					break;
				default:
					if(cmd_count < (int)sizeof(cmd_buffer) - 1) cmd_buffer[cmd_count++] = input;
					cHelper_selected_cmd = 0;
					break;
			}
			
			// If characters are pasting in quickly, just buffer them and skip expensive renders/autocomplete until they finish!
			if(cmd_mode && kbhit_pending()) continue; 
			
			if (cmd_mode && !is_search) run_cmd(1);
			if(!cmd_mode) printf("\e[?25h"); //visible cursor
			
			draw_update(redraw);	
			g_pressed = 0;
			continue;
		}

		if(input != 'g') g_pressed = 0;

		switch(input){
			case 'g':
				if(g_pressed){
					list_selector_goto_top();
					g_pressed = 0;
				}else g_pressed = 1;
				break;
			case 'r':
				if (!server_is_running) {
					server_is_running = 1;
					pthread_t tServer;
					pthread_create(&tServer, NULL, &server_thread, NULL);
					pthread_detach(tServer);
				}
				break;
			case 'T': 
				//save local copy
				break;
			case 13:
				if (selected_tab->tab_mode != TAB_WEBFEEDS){
					entry_view(selected_tab->old_entry->entry);
				}
				break;
			case 'f':
				if (selected_tab->tab_mode != TAB_WEBFEEDS && selected_tab->old_entry->entry) {
					selected_tab->old_entry->entry->bookmarked = !selected_tab->old_entry->entry->bookmarked;
					Load->UpdateInteractionInformation(selected_tab->old_entry->entry);
					redraw = true;
				}
				break;
			case 'F':
				if (selected_tab->tab_mode != TAB_WEBFEEDS && strncmp(selected_tab->title, "Favs:", 5) != 0) {
					char fav_title[21];
					snprintf(fav_title, sizeof(fav_title), "Favs: %.13s", selected_tab->title);
					if (open_filtered_tab(fav_title, is_entry_bookmarked, NULL, "No favorites found in this view.")) {
						redraw = true;
					}
				}
				break;
			case 'j':
				scroll_ret = list_selector_move(1);
				if(scroll_ret == 2) continue;
				break;
			case 'k':
				scroll_ret = list_selector_move(-1);
				if(scroll_ret == 1) continue;
				break;
			case 'u':
				if (selected_tab->tab_mode != TAB_WEBFEEDS){
					tabs_NewURLBASE(TAB_URLBASE);
					redraw = true;
				}
				break;
			case 'h':
				tabs_switchPrevious(selected_tab);
				redraw = true;
				break;
			case 'l':
				tabs_switchNext(selected_tab);
				redraw = true;
				break;
			case 'd':
				tabs_close(selected_tab);
				redraw = true;
				break;
			case ':':
				cmd_buffer[cmd_count++] = ':';
				cmd_mode = 1;
				printf("\e[?25h");
				break;
			case '/':
				if(selected_tab->tab_mode != TAB_WEBFEEDS){
					cmd_buffer[cmd_count++] = '/';
					cmd_mode = 1;
					printf("\e[?25h");
				}
				break;
			case 'Q':
#ifdef _WIN32
				reset_terminal_colors();
#endif
				exit(0);
		}
		draw_update(redraw);
	}

	return 0;
}

int entry_view(entry * entry){
	if(!entry) return 0;

	char * url = entry->url;
	entry->seen = 1;
	int is_video = is_video_url(url);

#ifdef _WIN32
	char cmd[1024];
	DWORD creationFlags = DETACHED_PROCESS;

	if(is_video){
		snprintf(cmd, sizeof(cmd), "cmd.exe /c mpv --force-window=immediate  --osd-level=1 \"%s\"", url);
		creationFlags = CREATE_NEW_CONSOLE;
	}else{
		snprintf(cmd, sizeof(cmd), "cmd.exe /c start \"\" \"%s\"", url);
		creationFlags = DETACHED_PROCESS;
	}

	STARTUPINFOA si = { sizeof(si) };
	si.cb = sizeof(si);
	PROCESS_INFORMATION pi;
	if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, creationFlags, NULL, NULL, &si, &pi)){
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}
#else
	// Double-fork so the launched viewer is reparented and never leaves zombies or blocks pclose().
	pid_t pid = fork();
	if(pid == 0){
		pid_t pid2 = fork();
		if(pid2 == 0){
			int devnull = open("/dev/null", O_RDWR);
			if(devnull >= 0){
				dup2(devnull, STDIN_FILENO);
				dup2(devnull, STDOUT_FILENO);
				dup2(devnull, STDERR_FILENO);
				if(devnull > STDERR_FILENO) close(devnull);
			}
			setsid();
			if(is_video){
				execlp("mpv", "mpv", "--force-window=immediate", "--osd-level=1", url, (char *)NULL);
			}else{
				execlp("xdg-open", "xdg-open", url, (char *)NULL);
			}
			_exit(127);
		}
		_exit(0);
	}else if(pid > 0){
		waitpid(pid, NULL, 0);
	}
#endif

	Load->UpdateInteractionInformation(entry); //mark as seen

	return 0;
}
