#include "files_path.hpp"

#ifndef _WIN32
#include <cstdlib>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

using namespace std;

string files_path::GetHomeDirectory(){
#ifdef _WIN32
	char pth_Appdata[MAX_PATH];
	SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, pth_Appdata);
	string retstr(pth_Appdata);
	return retstr;			
#else
	const char* xdg = getenv("XDG_DATA_HOME");
	string baseDir;
	if(xdg && xdg[0] != '\0'){
		baseDir = xdg;
	}else{
		const char* home = getenv("HOME");
		if(!home || home[0] == '\0'){
			struct passwd* pw = getpwuid(getuid());
			home = pw ? pw->pw_dir : ".";
		}
		string localDir = string(home) + "/.local";
		mkdir(localDir.c_str(), 0755);
		baseDir = localDir + "/share";
	}
	mkdir(baseDir.c_str(), 0755);
	string appDir = baseDir + "/" + rootDir;
	mkdir(appDir.c_str(), 0755);
	return baseDir;
#endif
}

int files_path::AddPath(string &dest, string &tAdd){
#ifdef _WIN32
	const char sep = '\\';
#else
	const char sep = '/';
#endif
	if(!dest.empty() && dest.back() != sep) dest += sep;
	dest = dest + tAdd;

	return 0;
}

int files_path::AddRootToPath(string &dest, string &tAdd){
	string ret = tAdd;
	AddPath(ret, dest);
	dest = ret;

	return 0;
}

void files_path::ApplyRootToPaths(){
	string appdata = GetHomeDirectory();

	AddRootToPath(rootDir, appdata);
	AddRootToPath(entriesFilePath, rootDir);
	AddRootToPath(feedListFilePath, rootDir);
	AddRootToPath(blacklistFilePath, rootDir);
	AddRootToPath(groupsFilePath, rootDir);
	AddRootToPath(updateRecordFilePath, rootDir);
}

files_path::files_path(){
	ApplyRootToPaths();
}
