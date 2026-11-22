#include "utils.hpp"
#include "globals.hpp"
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <locale>
#include <sstream>

using namespace std;

vector<string> ParseArguments(string line){
	vector <string> args;
	string arg;
	short arg_index = 0;
	
	char comment_prefix = '#';
	char arg_separator = ' ';

	while(!line.empty() && line.back() == '\r') line.pop_back();
	if(line.empty()) return args;
	if(line.at(0) == comment_prefix) return args;

	if(line.at(0) == '[' && line.at(line.size()-1) == ']'){
		arg_separator = ',';
		line.erase(0,1);
		line.erase(line.size()-1, 1);
	}

	bool inBrackets = false;
	for(int i=0;i<line.size();i++){
		if(line.at(i) == '[') inBrackets = true;
		if(inBrackets && line.at(i) == ']') inBrackets = false;

		if(line.at(i) != arg_separator){
			arg += line[i];
		}

		if(!inBrackets && (line.at(i) == arg_separator || i+1 == line.size())){
			
			if(arg_separator != ' ' || arg_index != 0){
				short len = arg.length();
				for(short i=0;i<len;i++) arg[i] = tolower(arg[i]);
			}

			args.push_back(arg);
			arg_index++;
			arg = "";
		}
	}

	return args;
}

size_t CurlWriteData(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

int ConcatenateNumbers(int n, int a){
	int res = n / 10;
	int exp = 10;
	
	while(res>0){
		res = res / 10;
		exp *= 10;
	}

	return ((exp*a)+n);
}

// GenerateNewId depends on rssManager, which is a global
#include "feed.hpp"
unsigned long GenerateNewId(){
	unsigned long newID = rssManager->parsedEntriesCount;

	struct tm datetime;
	time_t now = time(NULL);
	srand(now);
	datetime = *localtime(&now);

	newID = ConcatenateNumbers(rand()%9999, newID);	
	newID = ConcatenateNumbers((datetime.tm_hour*60+datetime.tm_min)/5, newID);
	return newID;
}

time_t TmToUtcTime(struct tm * t){
	struct tm copy = *t;
	copy.tm_isdst = 0;
#ifdef _WIN32
	return _mkgmtime(&copy);
#else
	return timegm(&copy);
#endif
}

static void UtcTimeToTm(time_t ts, struct tm * out){
	std::memset(out, 0, sizeof(*out));
#ifdef _WIN32
	gmtime_s(out, &ts);
#else
	gmtime_r(&ts, out);
#endif
}

static string TrimWhitespace(const string &s){
	size_t start = 0;
	while(start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) start++;
	size_t end = s.size();
	while(end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) end--;
	return s.substr(start, end - start);
}

static bool ParseTimezoneOffset(const string &tz_raw, int &offset_seconds){
	string tz = TrimWhitespace(tz_raw);
	if(tz.empty() || tz == "Z" || tz == "z" || tz == "GMT" || tz == "UTC" || tz == "UT"){
		offset_seconds = 0;
		return true;
	}
	if(tz[0] == '+' || tz[0] == '-'){
		int sign = (tz[0] == '-') ? -1 : 1;
		string digits;
		for(size_t i = 1; i < tz.size(); i++){
			if(std::isdigit(static_cast<unsigned char>(tz[i]))) digits += tz[i];
			else if(tz[i] != ':') break;
		}
		if(digits.size() >= 2){
			int hh = std::stoi(digits.substr(0, 2));
			int mm = (digits.size() >= 4) ? std::stoi(digits.substr(2, 2)) : 0;
			offset_seconds = sign * (hh * 3600 + mm * 60);
			return true;
		}
		return false;
	}
	struct TzEntry { const char *name; int hours; };
	static const TzEntry kZones[] = {
		{"EDT", -4}, {"EST", -5}, {"CDT", -5}, {"CST", -6},
		{"MDT", -6}, {"MST", -7}, {"PDT", -7}, {"PST", -8},
		{"CEST", 2}, {"CET", 1}
	};
	for(const auto &z : kZones){
		if(tz == z.name){
			offset_seconds = z.hours * 3600;
			return true;
		}
	}
	return false;
}

bool GetDatetime(string &date, string type, struct tm * t){
	std::memset(t, 0, sizeof(*t));
	t->tm_isdst = 0;

	string trimmed = TrimWhitespace(date);
	if(trimmed.empty()) return false;

	int tz_offset_seconds = 0;

	if(type == "UNIX"){
		for(char c : trimmed){
			if(!std::isdigit(static_cast<unsigned char>(c))) return false;
		}
		try{
			time_t ts = static_cast<time_t>(std::stoll(trimmed));
			UtcTimeToTm(ts, t);
			return true;
		}catch(...){
			return false;
		}
	}
	else if(type == "RFC_1123"){
		const char *formats[] = {
			"%a, %d %b %Y %H:%M:%S",
			"%d %b %Y %H:%M:%S",
			"%a, %d %b %Y %H:%M",
			"%d %b %Y %H:%M"
		};
		for(const char *fmt : formats){
			std::memset(t, 0, sizeof(*t));
			std::istringstream ss(trimmed);
			ss.imbue(std::locale::classic());
			if(ss >> std::get_time(t, fmt)){
				string rest;
				std::getline(ss, rest);
				ParseTimezoneOffset(rest, tz_offset_seconds);
				time_t utc_ts = TmToUtcTime(t) - tz_offset_seconds;
				UtcTimeToTm(utc_ts, t);
				return true;
			}
		}
	}
	else if(type == "ISO_INSTANT"){
		const char *formats[] = {
			"%Y-%m-%dT%H:%M:%S",
			"%Y-%m-%d %H:%M:%S",
			"%Y-%m-%dT%H:%M",
			"%Y-%m-%d %H:%M"
		};
		for(const char *fmt : formats){
			std::memset(t, 0, sizeof(*t));
			std::istringstream ss(trimmed);
			ss.imbue(std::locale::classic());
			if(ss >> std::get_time(t, fmt)){
				string rest;
				std::getline(ss, rest);
				// Strip optional fractional seconds (e.g. ".123456") before timezone.
				if(!rest.empty() && (rest[0] == '.' || rest[0] == ',')){
					size_t i = 1;
					while(i < rest.size() && std::isdigit(static_cast<unsigned char>(rest[i]))) i++;
					rest = rest.substr(i);
				}
				ParseTimezoneOffset(rest, tz_offset_seconds);
				time_t utc_ts = TmToUtcTime(t) - tz_offset_seconds;
				UtcTimeToTm(utc_ts, t);
				return true;
			}
		}
	}

	return false;
}

void DebugPrintDate(struct tm * t){
	char buffer[64];
	std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", t);
	std::cout << "	DEBUG_PRINTDATE " << buffer << std::endl; 
}

int ParseAnyDatetime(string &date, struct tm * t){
	if(GetDatetime(date, "RFC_1123", t)) return 0;
	if(GetDatetime(date, "ISO_INSTANT", t)) return 0;
	if(GetDatetime(date, "UNIX", t)) return 0;
	return 1;
}

int CompareDates(struct tm * d1, struct tm * d2){
	if(d1->tm_year>d2->tm_year) return 1;
	if(d1->tm_year<d2->tm_year) return 2;
	if(d1->tm_mon>d2->tm_mon) return 1;
	if(d1->tm_mon<d2->tm_mon) return 2;
	if(d1->tm_mday>d2->tm_mday) return 1;
	if(d1->tm_mday<d2->tm_mday) return 2;
	if(d1->tm_hour>d2->tm_hour) return 1;
	if(d1->tm_hour<d2->tm_hour) return 2;
	if(d1->tm_min>d2->tm_min) return 1;
	if(d1->tm_min<d2->tm_min) return 2;
	if(d1->tm_sec>d2->tm_sec) return 1;
	if(d1->tm_sec<d2->tm_sec) return 2;
	return 0;
}

static void AppendUtf8Codepoint(string &out, unsigned long cp){
	if(cp == 0 || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return;
	if(cp == '\n' || cp == '\r' || cp == 31 || cp == 0xA0){
		out.push_back(' ');
		return;
	}
	if(cp < 0x80){
		out.push_back(static_cast<char>(cp));
	}else if(cp < 0x800){
		out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
		out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
	}else if(cp < 0x10000){
		out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
		out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
		out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
	}else{
		out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
		out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
		out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
		out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
	}
}

/* Decode XML/HTML named and numeric entities (e.g. &amp;, &#8217;, &#x27;) into UTF-8. */
string DecodeHtmlEntities(const string &input){
	struct HtmlEntity { const char *name; const char *utf8; };
	static const HtmlEntity kEntities[] = {
		{"amp", "&"}, {"lt", "<"}, {"gt", ">"}, {"quot", "\""}, {"apos", "'"},
		{"nbsp", " "}, {"ensp", " "}, {"emsp", " "}, {"thinsp", " "},
		{"lsquo", "\xE2\x80\x98"}, {"rsquo", "\xE2\x80\x99"}, {"sbquo", "\xE2\x80\x9A"},
		{"ldquo", "\xE2\x80\x9C"}, {"rdquo", "\xE2\x80\x9D"}, {"bdquo", "\xE2\x80\x9E"},
		{"laquo", "\xC2\xAB"}, {"raquo", "\xC2\xBB"},
		{"lsaquo", "\xE2\x80\xB9"}, {"rsaquo", "\xE2\x80\xBA"},
		{"prime", "\xE2\x80\xB2"}, {"Prime", "\xE2\x80\xB3"},
		{"ndash", "\xE2\x80\x93"}, {"mdash", "\xE2\x80\x94"}, {"minus", "\xE2\x88\x92"},
		{"hellip", "\xE2\x80\xA6"}, {"bull", "\xE2\x80\xA2"}, {"middot", "\xC2\xB7"},
		{"copy", "\xC2\xA9"}, {"reg", "\xC2\xAE"}, {"trade", "\xE2\x84\xA2"},
		{"deg", "\xC2\xB0"}, {"plusmn", "\xC2\xB1"}, {"times", "\xC3\x97"}, {"divide", "\xC3\xB7"},
		{"euro", "\xE2\x82\xAC"}, {"pound", "\xC2\xA3"}, {"yen", "\xC2\xA5"}, {"cent", "\xC2\xA2"},
		{"sect", "\xC2\xA7"}, {"para", "\xC2\xB6"},
		{"rarr", "\xE2\x86\x92"}, {"larr", "\xE2\x86\x90"}, {"uarr", "\xE2\x86\x91"},
		{"darr", "\xE2\x86\x93"}, {"harr", "\xE2\x86\x94"},
		{"aacute", "\xC3\xA1"}, {"eacute", "\xC3\xA9"}, {"iacute", "\xC3\xAD"},
		{"oacute", "\xC3\xB3"}, {"uacute", "\xC3\xBA"}, {"agrave", "\xC3\xA0"},
		{"egrave", "\xC3\xA8"}, {"auml", "\xC3\xA4"}, {"ouml", "\xC3\xB6"},
		{"uuml", "\xC3\xBC"}, {"ntilde", "\xC3\xB1"}, {"ccedil", "\xC3\xA7"},
		{"szlig", "\xC3\x9F"}, {"Aacute", "\xC3\x81"}, {"Eacute", "\xC3\x89"},
		{"Auml", "\xC3\x84"}, {"Ouml", "\xC3\x96"}, {"Uuml", "\xC3\x9C"}
	};

	string out;
	out.reserve(input.size());

	for(size_t i = 0; i < input.size(); ){
		if(input[i] != '&'){
			out.push_back(input[i++]);
			continue;
		}
		size_t semi = input.find(';', i + 1);
		if(semi == string::npos || semi - i > 12 || semi == i + 1){
			out.push_back(input[i++]);
			continue;
		}

		string ent = input.substr(i + 1, semi - i - 1);
		bool decoded = false;

		if(ent[0] == '#'){
			unsigned long cp = 0;
			bool valid_num = false;
			if(ent.size() >= 3 && (ent[1] == 'x' || ent[1] == 'X')){
				valid_num = true;
				for(size_t k = 2; k < ent.size(); k++){
					if(!std::isxdigit(static_cast<unsigned char>(ent[k]))){ valid_num = false; break; }
				}
				if(valid_num) cp = std::strtoul(ent.c_str() + 2, nullptr, 16);
			}else if(ent.size() >= 2){
				valid_num = true;
				for(size_t k = 1; k < ent.size(); k++){
					if(!std::isdigit(static_cast<unsigned char>(ent[k]))){ valid_num = false; break; }
				}
				if(valid_num) cp = std::strtoul(ent.c_str() + 1, nullptr, 10);
			}
			if(valid_num && cp > 0 && cp <= 0x10FFFF && !(cp >= 0xD800 && cp <= 0xDFFF)){
				AppendUtf8Codepoint(out, cp);
				decoded = true;
			}
		}else{
			for(const auto &e : kEntities){
				if(ent == e.name){
					out += e.utf8;
					decoded = true;
					break;
				}
			}
		}

		if(decoded){
			i = semi + 1;
		}else{
			out.push_back(input[i++]);
		}
	}
	return out;
}
