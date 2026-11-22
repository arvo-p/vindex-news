#ifndef FEED_HPP
#define FEED_HPP

#include <string>
#include <unordered_set>
#include <vector>
#include <fstream>
#include "globals.hpp"
#include "rapidxml-1.13/rapidxml.hpp"

struct Entry{
	unsigned long id;
	std::string title;
	std::string link;
    rss_url rssUrl;
	struct tm date;
	time_t timestamp;
};

class feed{
	public:
		std::ofstream newsOutputStream;
		std::ifstream updateInputStream;
		
		enum ParseStatus{
			IN_PROGRESS,
			FINISHED_UPDATED,
			FINISHED_NOCHANGE,
			FAILED
		};

		feed();
		int Fetch(rss_url &rssUrl);
		int Parse(std::string &buffer, rss_url &rssUrl);
		int RecordEntry(std::string &entry_title, std::string &entry_link, rss_url &rssUrl, std::string &date_str);
		bool VerifyEntryDate(std::string &url_src, std::string &date);
		std::string TypeToString(FeedType type);
		void ParsePrintStatus(rss_url &rssUrl, enum ParseStatus pStatus, int count);
		int ParseAtom(rss_url &rssUrl, rapidxml::xml_node<> *node);
		int ParseRdf(rss_url &rssUrl, rapidxml::xml_node<> *node);
		int ParseRss(rss_url &rssUrl, rapidxml::xml_node<> *node);
		bool CompareToLastUpdate(std::string &urlSrc, struct tm * param_t);
		bool UpdateRecord(std::string &urlSrc, struct tm * t);
		int SaveEntry(Entry &entry);
		int Save();
		int Close();
		void LoadRecentLinks(size_t maxEntries = 50);

		std::vector<Entry> entryList;
		std::unordered_set<std::string> recentLinks;
		int parsedEntriesCount = 0;
};

#endif
