#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "headers/colorscheme.h"

extern char *pth_folder_colorscheme;

colorscheme * colorScheme = NULL;
config * uiConfig = NULL;

int setAppDefaultColorscheme(){
	colorScheme->tab_title = 252;
	colorScheme->background = 233;
	
	colorScheme->entries = 250;
	colorScheme->url = 244;
	colorScheme->url_path = 239;
	colorScheme->seen = 239;

	colorScheme->ui_walls=138;
	colorScheme->entries_nav=138;
	colorScheme->urlprefix=182;

	colorScheme->zebra_enabled = 1;
	colorScheme->zebra_even_color = 233;
	colorScheme->zebra_odd_color = 234;
	colorScheme->zebra_dot_enabled = 0;
	colorScheme->zebra_dot_color = 240;
	colorScheme->title_sentence_case = 1;
	colorScheme->title_prefix_color = 138;
	colorScheme->title_prefix_maxlen = 24;

	colorScheme->urlprefix_char = '@';

	return 0;
}

int isValidChar(int c){
	if((c > 32 && c < 127) || c == 46) return 1;
	return 0;
}

int getConfigFromLine(char * line, config * previousConfig, config * newConfig){
	//isn't this too big?

	int len = strlen(line);

	char singleArg[64];
	int singleArg_count=0;
	memset(singleArg, 0, sizeof(singleArg));

	char configName[64];
	char type = 0;
	void * configValue = NULL;

	memset(configName, 0, sizeof(configName));
	
	int arg_num = 0;

	int validInput = 0;
	for(int i=0;i<len;i++){
		if(line[i] == '=' && arg_num == 0){
			singleArg[singleArg_count] = 0;
			
			int configName_count=0;
			for(int j=0;singleArg[j]!=0 && configName_count < (int)sizeof(configName)-1;j++){
				if(singleArg[j] == ' ') continue;
				configName[configName_count] = singleArg[j];
				configName_count++;
			}

			memset(singleArg, 0, sizeof(singleArg));
			singleArg_count = 0;
			arg_num++;
			continue;
		}

		if(line[i] != ';' && isValidChar(line[i]) && singleArg_count < (int)sizeof(singleArg)-1){
			singleArg[singleArg_count] = line[i];
			singleArg_count++;
		}

		if((i == len-1 || line[i] == ';') && arg_num == 1){
			singleArg[singleArg_count] = 0;
			if(singleArg_count==0) break;

			int isNumeric = 1;
			int startIdx = (singleArg[0] == '-' && singleArg_count > 1) ? 1 : 0;
	
			for(int j=startIdx;j<singleArg_count;j++){
				if(singleArg[j] < 48 || singleArg[j] > 57){
					isNumeric = 0;
					break;
				}
			}

			if(isNumeric){
				configValue = (void *)malloc(sizeof(int));
				*(int *)configValue = atoi(singleArg);
				type = 'i';
			}else{
				configValue = (void *)malloc((singleArg_count + 1)*sizeof(char));
				if(singleArg_count == 1){
					((char *)configValue)[0] = singleArg[0];
					type = 'c';
				}else{
					strcpy((char *)configValue, singleArg);
					type = 's';
				}
			}

			validInput = 1;
			break;
		}
	}

	if(validInput){
		strcpy(newConfig->name,configName);
		newConfig->value = configValue;
		newConfig->next = NULL;
		newConfig->type = type; 
		if(previousConfig) previousConfig->next = newConfig;
	}

	return validInput;
}

int getConfigValue(char * configName, void * registerValue, char expectedType){
	config * config = uiConfig;
	int ret = 0;

	while(config){
		if(strcmp(config->name, configName) == 0){
			ret = 1;
			break;
		}
		config = config->next;
	}

	if(ret){
		if(expectedType == 'i' && config->type == 'i'){
			*(int *)registerValue = *(int *)config->value;
			return 1;
		}

		if(expectedType == 'c' && config->type == 'c'){
			*(char *)registerValue = *(char *)config->value;
			return 1;
		}

		if(expectedType == 's' && config->type != 'i'){
			if(config->type == 'c') ((char *)registerValue)[0] = ((char *)config->value)[0];
			else{
				strcpy((char *)registerValue, config->value); //unsafe
			}
			return 1;
		}
	}
	return 0;
}

int loadColorschemeFromFile(char * path){
	char buffer[255];
	
	FILE * f = fopen(path, "rb");
	if(!f) return 1;

	uiConfig = calloc(1, sizeof(struct config));
	config * previousConfig = NULL;
	config * newConfig = uiConfig;
	
	while(fgets(buffer, 255, f)){
		int validConfig = getConfigFromLine(buffer, previousConfig, newConfig);	
		if(validConfig){
			previousConfig = newConfig;
			newConfig = calloc(1, sizeof(struct config));
		}
	}

	if(!previousConfig){
		free(uiConfig);
		uiConfig = NULL;
	}else{
		free(newConfig);
	}

	fclose(f);

	return 0;
}

int initColorscheme(){
	colorScheme = malloc(sizeof(struct colorscheme));
	setAppDefaultColorscheme();

	if(pth_folder_colorscheme){
		char theme_path[1024];
		snprintf(theme_path, sizeof(theme_path), "%slatenight", pth_folder_colorscheme);
		if(loadColorschemeFromFile(theme_path) != 0){
#ifdef _WIN32
			loadColorschemeFromFile("C:/Users/gsppe/wezterm/news/colorschemes/latenight");
#endif
		}
	}

	getConfigValue("ui.background.color", &colorScheme->background, 'i');
	getConfigValue("ui.entries.color", &colorScheme->entries, 'i');
	getConfigValue("ui.url.color", &colorScheme->url, 'i');
	getConfigValue("ui.url.path.color", &colorScheme->url_path, 'i');
	getConfigValue("ui.seen.color", &colorScheme->seen, 'i');
	getConfigValue("ui.walls.color", &colorScheme->ui_walls, 'i');
	getConfigValue("ui.urlprefix.color", &colorScheme->urlprefix, 'i');
	getConfigValue("ui.entries_nav.color", &colorScheme->entries_nav, 'i');
	getConfigValue("ui.zebra.enabled", &colorScheme->zebra_enabled, 'i');
	getConfigValue("ui.zebra.color", &colorScheme->zebra_odd_color, 'i');
	getConfigValue("ui.zebra.even.color", &colorScheme->zebra_even_color, 'i');
	getConfigValue("ui.zebra.odd.color", &colorScheme->zebra_odd_color, 'i');
	getConfigValue("ui.zebra.dot.enabled", &colorScheme->zebra_dot_enabled, 'i');
	getConfigValue("ui.zebra.dot.color", &colorScheme->zebra_dot_color, 'i');
	getConfigValue("ui.title.sentence_case", &colorScheme->title_sentence_case, 'i');
	getConfigValue("ui.title.prefix.color", &colorScheme->title_prefix_color, 'i');
	getConfigValue("ui.title.prefix.maxlen", &colorScheme->title_prefix_maxlen, 'i');
	
	getConfigValue("ui.urlprefix.char", &colorScheme->urlprefix_char, 'c');

	return 0;
}
