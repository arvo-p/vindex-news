#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <conio.h>
#include <windows.h>
#include <shellapi.h>
#include <direct.h>
#include <io.h>
#include <time.h>

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

void * server_thread(void * arg){
	FILE *pipe = _popen("..\\server\\koalaServer.exe 2>&1", "r");
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
		_pclose(pipe);
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

	SetConsoleOutputCP(CP_UTF8);
	srand(time(NULL));  
	
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
	winSZ[0] = csbi.srWindow.Right - csbi.srWindow.Left + 1;
	winSZ[1] = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
	winSZ[2] = winSZ[0];
	winSZ[3] = winSZ[1];

	if (winSZ[0] > displayThreshold) display_mode = 0;
	else display_mode = 1;
	
	draw_update(true);

	pthread_create(&tWinResize, NULL, &winResize_Loop, NULL);
	while(!winSZ[0]); //wait for winresize

	int input;
	int scroll_ret = 0;
	int redraw = false;
	int g_pressed = 0;

	printf("\e[?25l"); //invisible cursor
	while(1){
		redraw = false;
		input = getch();
		
		if(input == 224 || input == 0 || input == -32){
			int scan_code = getch();
			
			if (cmd_mode){
				if (scan_code == 72){ // Up arrow
					cHelper_selected_cmd--;
					run_cmd(1);
				} else if (scan_code == 80){ // Down arrow
					cHelper_selected_cmd++;
					run_cmd(1);
				}
				draw_update(false);
				continue;
			}

			int pageSize = (display_mode == 0) ? (winSZ[1] - 13) : (winSZ[1] - 5);
			if(pageSize < 1) pageSize = 1;

			if(scan_code == 73){ // Page Up
				list_selector_move(-pageSize);
			}else if(scan_code == 81){ // Page Down
				list_selector_move(pageSize);
			}
			draw_update(false);
			g_pressed = 0;
			continue;
		}

		if(!cmd_mode && _kbhit() != 0) continue; // Prevent input spamming in normal mode
		
		selected_tab->selPrevious = selected_tab->sel;
		
		if(cError[0] || statusbar_notify[0]){
			statusbar_notify[0] = 0;
			cError[0] = 0;
			ClearLastLine();
		}
	
		if(cmd_mode){
			switch(input){
				case 13:
					exit_command_mode();
					cHelper = NULL;
					if(run_cmd(0)!=0) draw_cError();
					memset(cmd_buffer, 0, cmd_count);
					cmd_count = 0;
					cHelper_selected_cmd = 0;
					break;
				case 8:
					if(cmd_count>0) cmd_buffer[--cmd_count] = 0;
					if(cmd_count == 0) exit_command_mode();
					cHelper_selected_cmd = 0;
					if(!cHelper){
						if (_kbhit() == 0) run_cmd(1);
					} else if(cHelper_len) cHelper_len--;
					else cHelper = NULL;
					break;
				case 9:
					cmd_autocomplete();
					break;
				default:
					if(cmd_count < 100) cmd_buffer[cmd_count++] = input;
					cHelper_selected_cmd = 0;
					break;
			}
			
			// If characters are pasting in quickly, just buffer them and skip expensive renders/autocomplete until they finish!
			if(cmd_mode && _kbhit() != 0) continue; 
			
			if (cmd_mode) run_cmd(1);
			if(!cmd_mode) printf("\e[?25h"); //visible cursor
			
			draw_update(false);	
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
					cat_group * fav_group = calloc(1, sizeof(cat_group));
					snprintf(fav_group->name, sizeof(fav_group->name), "Favs: %.13s", selected_tab->title);
					
					global_e iter;
					iter.entry = NULL; iter.child = NULL; iter.parent = NULL; iter.group_member = NULL;
					
					// Setup iterator start
					if (selected_tab->tab_mode == TAB_SIMPLE) {
						setGlobalEntry(&iter, initial_entry);
					} else if (selected_tab->tab_mode == TAB_GROUP) {
						if(selected_tab->category && selected_tab->category->first_member){
							iter.group_member = selected_tab->category->first_member;
							setGlobalEntry(&iter, selected_tab->category->first_member->entry);
						}
					} else if (selected_tab->tab_mode == TAB_URLBASE) {
						if(selected_tab->offset->parent && selected_tab->offset->parent->first_child){
							child_entry * first = (child_entry *)selected_tab->offset->parent->first_child;
							iter.child = first;
							setGlobalEntry(&iter, first->entry);
						}
					}

					g_member * last_member = NULL;
					while (iter.entry != NULL) {
						if (iter.entry->bookmarked) {
							g_member * new_mem = calloc(1, sizeof(g_member));
							new_mem->entry = iter.entry;
							if (!fav_group->first_member) {
								fav_group->first_member = new_mem;
							} else {
								last_member->next = new_mem;
								new_mem->previous = last_member;
							}
							last_member = new_mem;
							fav_group->count++;
						}
						
						if (selected_tab->tab_mode == TAB_GROUP) {
							g_member * nxt = getNextGroupEntry(iter.group_member, 0);
							if (!nxt) break;
							iter.entry = nxt->entry;
							iter.group_member = nxt;
						} else {
							if (setGlobalEntry(&iter, getNextEntry(&iter, 0))) break;
						}
					}
					
					if (fav_group->count > 0) {
						tab * ntab = tabs_newtab(fav_group->name, fav_group->first_member->entry, TAB_GROUP);
						ntab->category = fav_group;
						ntab->offset->group_member = fav_group->first_member;
						selected_tab = ntab;
						tabs_updateDisplayMode(selected_tab);
						redraw = true;
					} else {
						free(fav_group);
						strncpy(statusbar_notify, "No favorites found in this view.", 255);
						draw_statusbar();
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
			case 'Q':
				printf("\e[0m\e[?25h");
				exit(0);
		}
		draw_update(redraw);
	}

	return 0;
}

int entry_view(entry * entry){
	char * url = entry->url;
	short szUrl = strlen(url);
	entry->seen = 1;

	char cmd[1024];
	DWORD creationFlags = DETACHED_PROCESS;

	if(strstr(url, "youtube.com") || strstr(url, "youtu.be") || strstr(url+szUrl-4,".mp4")){
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

	Load->UpdateInteractionInformation(entry); //mark as seen

	return 0;
}
