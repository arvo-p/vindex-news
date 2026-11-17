#include "feed.hpp"
#include "files_path.hpp"
#include "blacklist.hpp"
#include "utils.hpp"
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <curl/curl.h>
#include <cstring>
#include <ctime>

using namespace std;

feed::feed(){
	newsOutputStream.open(pathManager->entriesFilePath, std::ios_base::app);
	if(!newsOutputStream.is_open()){
		cout << "Could not open " << pathManager->entriesFilePath << endl;
		exit(1);
	}
}

int feed::Save(){
	cout << "Sorting entries" << endl;
	std::stable_sort(entryList.begin(), entryList.end(), [](const Entry& a, const Entry& b){
		return a.timestamp < b.timestamp;
	});

	cout << "Saving entries" << endl;
	for(auto& entry: entryList)
		SaveEntry(entry);
	return 0;
}

int feed::Close(){
	cout << "Exited normally" << endl;
	entryList.clear();
	newsOutputStream.close();
	return 0;
}

int feed::SaveEntry(Entry &entry){
	uint8_t unit_separator = 31;
	newsOutputStream << std::hex << entry.id << std::dec << unit_separator << entry.title << unit_separator << entry.link << unit_separator;
	
	int sz = entry.rssUrl.groups.size();
	for(int i=0;i<sz;i++){
		if(i > 0) newsOutputStream << " ";
		newsOutputStream << entry.rssUrl.groups[i];
	}
	newsOutputStream << unit_separator << static_cast<long long>(entry.timestamp) << endl;
	return 0;
}

int feed::RecordEntry(string &entry_title, string &entry_link, rss_url &rssUrl, string &date_str){
	Entry entry;
	entry.id = GenerateNewId();
	entry.title = entry_title;
	entry.link = entry_link;
	entry.rssUrl = rssUrl;
	
	struct tm date = {};
	if(ParseAnyDatetime(date_str, &date) == 0){
		entry.date = date;
		entry.timestamp = TmToUtcTime(&date);
	}else{
		entry.date = date;
		entry.timestamp = 0;
	}

	entryList.push_back(entry);
	return 0;
}

bool feed::UpdateRecord(string &urlSrc, struct tm * t){
	ofstream fw;
	ifstream fr;
	ostringstream filetext;
	string line;
	char output_date[20];
	char unit_separator = 31;

	fr.open(pathManager->updateRecordFilePath);
	strftime(output_date, 20, "%Y-%m-%dT%H:%M:%S", t);

	filetext << urlSrc << unit_separator << output_date << endl;
	if(fr.is_open()){
		while(getline(fr, line)){	
			int sep_pos = line.find(unit_separator);
			if(sep_pos != string::npos && line.substr(0, sep_pos) != urlSrc){
				filetext << line << endl;
			} else if (sep_pos == string::npos) {
				filetext << line << endl;
			}
		}		
	}
	fr.close();

	fw.open(pathManager->updateRecordFilePath);
	fw << filetext.str();
	fw.close();

	return true;
}

bool feed::CompareToLastUpdate(string &urlSrc, struct tm * param_t){
	ifstream f (pathManager->updateRecordFilePath);
	string line;
	string saved_date_str;
	struct tm saved_date;
	bool found = false;
	char unit_separator = 31;

	if(f.is_open()){
		while(getline(f, line)){
			int sep_pos = line.find(unit_separator);
			if(sep_pos != string::npos && line.substr(0, sep_pos) == urlSrc){
				saved_date_str = line.substr(sep_pos + 1);
				found = true;
			}
		}
	}

	if(!found) return true;
	if(!GetDatetime(saved_date_str, "ISO_INSTANT", &saved_date)) return false;
	if(CompareDates(param_t, &saved_date) == 1) return true;

	return false;
}

std::string feed::TypeToString(FeedType type) {
    switch (type) {
        case RSS:   return "RSS";
        case RDF:   return "RDF";
        case ATM:  return "ATOM";
        default:    return "UNDEFINED";
    }
}

void feed::ParsePrintStatus(rss_url &rssUrl, enum ParseStatus pStatus, int count){
	
	cout << rssUrl.url << " : ";

	switch(pStatus){
		case FAILED:
			cout << "failed";
			break;
		case IN_PROGRESS:
			cout << "fetching " << feed::TypeToString(rssUrl.feedType) << " | " << count << " new entries"; 
			break;
		case FINISHED_UPDATED:
			cout << "up to date | " << count << " new entries";
			break;
		case FINISHED_NOCHANGE:
			cout << "already up to date | no changes";
			break;
		default:
			break;
	}
	cout << endl;
}

bool feed::VerifyEntryDate(string &url_src, string &date){
	struct tm dt_entry;
	int ret = ParseAnyDatetime(date, &dt_entry);
	if(ret == 1){
		cout << "Failed to parse a date." << endl;
		return false;
	}
	return feed::CompareToLastUpdate(url_src, &dt_entry); 
}

int feed::Parse(string &buffer, rss_url &rssUrl){
	int ret = 0;
	char * parsed = new char[buffer.length()+1];
	strcpy(parsed, buffer.c_str());

	rapidxml::xml_document<> doc;
	rapidxml::xml_node<> *node;
	
	rssUrl.feedType = UNDEF; 

	try{
   		doc.parse<0>(parsed);
	}catch(const exception& e){
		cout << "Parsing failed. Buffer size: " << buffer.length() << " Content: " << buffer.substr(0, min((int)buffer.length(), 200)) << endl;
		feed::ParsePrintStatus(rssUrl, FAILED, 0);
		delete[] parsed;
		return 1;
	}

	node = doc.first_node("rss");
	if(node == NULL){
		node = doc.first_node("feed");
		if(node == NULL){
			node = doc.first_node("rdf:RDF");
			if(node == NULL){
				feed::ParsePrintStatus(rssUrl, FAILED, 0);
				ret = 1;
			}else{
				rssUrl.feedType = RDF;
				ret = feed::ParseRdf(rssUrl, node);
			}
		}else{
			rssUrl.feedType = ATM;
			ret = feed::ParseAtom(rssUrl, node);
		}
	}else{
		rssUrl.feedType = RSS;
		ret = feed::ParseRss(rssUrl, node);
	}

	delete[] parsed;
	return ret;
}

int feed::ParseRss(rss_url &rssUrl, rapidxml::xml_node<> *node){
	rapidxml::xml_node<> *item, *title_node, *link_node, *pdate_node;
	string channel_date_str = "";

	node = node ? node->first_node("channel") : NULL;
	if(node == NULL){
		feed::ParsePrintStatus(rssUrl, FAILED, 0);
		return 1;
	}
	pdate_node = node->first_node("lastBuildDate");
	if(pdate_node == NULL) pdate_node = node->first_node("pubDate");
	if(pdate_node == NULL) pdate_node = node->first_node("published");
	if(pdate_node == NULL) pdate_node = node->first_node("dc:date");
	
	if(pdate_node != NULL) {
		channel_date_str = pdate_node->value();
		if(feed::VerifyEntryDate(rssUrl.url, channel_date_str) == false){
			feed::ParsePrintStatus(rssUrl, FINISHED_NOCHANGE, 0); 
			return 1;
		}
	}

	item = node->first_node("item");

	int countNewEntries = 0;
	struct tm tmNew = {};
	bool tmNewInit = false;

	while(item){
		string link_str, title_str, pubdate_str;
		link_node = item->first_node("link");
		title_node = item->first_node("title");
		pdate_node = item->first_node("pubDate");
		if(!pdate_node) pdate_node = item->first_node("dc:date");
		if(!link_node || !title_node || !pdate_node){
			item = item->next_sibling("item");
			continue;
		}

		link_str = link_node->value();
		title_str = title_node->value();
		pubdate_str = pdate_node->value();

		if(title_str.size()==0){
			title_node = title_node->first_node();
			if(title_node) title_str = title_node->value();
			else title_str="NO TITLE";
		}
		
		struct tm itemDatetime;
		if(ParseAnyDatetime(pubdate_str, &itemDatetime) == 0) {
			if(!tmNewInit) {
				tmNew = itemDatetime;
				tmNewInit = true;
			} else if (CompareDates(&itemDatetime, &tmNew) == 1) {
				tmNew = itemDatetime;
			}
		}

		bool blacklisted = blacklistManager->Check(title_str,link_str); 
		bool isEntryNew = feed::VerifyEntryDate(rssUrl.url,pubdate_str);

		if(!blacklisted && isEntryNew){
			rssManager->RecordEntry(title_str, link_str, rssUrl, pubdate_str);
			countNewEntries++;
			feed::ParsePrintStatus(rssUrl, IN_PROGRESS, countNewEntries);
			rssManager->parsedEntriesCount++;
		}
		item = item->next_sibling("item");
	}

	if(tmNewInit) {
		feed::UpdateRecord(rssUrl.url, &tmNew);
	} else if(channel_date_str.length() > 0) {
		struct tm t;
		if(ParseAnyDatetime(channel_date_str, &t) == 0) {
			feed::UpdateRecord(rssUrl.url, &t);
		}
	}

	if(countNewEntries == 0)
		feed::ParsePrintStatus(rssUrl, FINISHED_NOCHANGE, countNewEntries);
	else
		feed::ParsePrintStatus(rssUrl, FINISHED_UPDATED, countNewEntries);

	return 0;
}

int feed::ParseRdf(rss_url &rssUrl, rapidxml::xml_node<> *node){
	if(node == NULL){
		feed::ParsePrintStatus(rssUrl, FAILED, 0);
		return 1;
	}
	struct tm t = {}, tmNew = {};
	rapidxml::xml_node<> *item, *title_node, *link_node, *pdate_node;
	string pubdate_str;

	item = node->first_node("item");

	int i=0;
	int countNewEntries = 0;
	while(item){
		string title_str, link_str, pubdate_str;
		title_node = item->first_node("title");
		link_node = item->first_node("link");
		pdate_node = item->first_node("dc:date");
		if(!title_node || !link_node || !pdate_node){
			item = item->next_sibling("item");
			continue;
		}

		title_str = title_node->value();
		link_str = link_node->value();
		pubdate_str = pdate_node->value();
	
		if(ParseAnyDatetime(pubdate_str, &t) == 0){
			if(i++ == 0){
				tmNew = t;
			}else if(CompareDates(&t, &tmNew) == 1) tmNew = t;
		}

		bool blacklisted = blacklistManager->Check(title_str,link_str); 
		bool isEntryNew = feed::VerifyEntryDate(rssUrl.url,pubdate_str);

		if(!blacklisted && isEntryNew){
			rssManager->RecordEntry(title_str, link_str, rssUrl, pubdate_str);
			countNewEntries++;
			feed::ParsePrintStatus(rssUrl, IN_PROGRESS, countNewEntries);
			rssManager->parsedEntriesCount++;
		}
		item = item->next_sibling("item");
	}

	if(countNewEntries == 0)
		feed::ParsePrintStatus(rssUrl, FINISHED_NOCHANGE, countNewEntries);
	else
		feed::ParsePrintStatus(rssUrl, FINISHED_UPDATED, countNewEntries);

	if(i > 0) feed::UpdateRecord(rssUrl.url, &tmNew);

	return 0;
}

int feed::ParseAtom(rss_url &rssUrl, rapidxml::xml_node<> *node){
	if(node == NULL){
		feed::ParsePrintStatus(rssUrl, FAILED, 0);
		return 1;
	}
	rapidxml::xml_node<> *item, *title_node, *link_node, *pdate_node, *headerdate_node;
	string pubdate_str;

	headerdate_node = node->first_node("updated");
	if(!headerdate_node) headerdate_node = node->first_node("lastBuildDate");

	struct tm headerDatetime = {};
	bool headerDefined = false;

	if(headerdate_node){
		pubdate_str = headerdate_node->value();
		if(ParseAnyDatetime(pubdate_str, &headerDatetime) == 0) headerDefined = true;
		if(feed::VerifyEntryDate(rssUrl.url, pubdate_str) == false){
			feed::ParsePrintStatus(rssUrl, FINISHED_NOCHANGE, 0); 
			return 1;
		}
	}

	item = node->first_node("entry");
	int countNewEntries = 0;
	while(item){
		string title_str, link_str, pubdate_str;
		title_node = item->first_node("title");
		link_node = item->first_node("link");

		pdate_node = item->first_node("updated");
		if(!pdate_node) pdate_node = item->first_node("published");

		rapidxml::xml_attribute<> *href_attr = link_node ? link_node->first_attribute("href") : NULL;
		if(!title_node || !href_attr || !pdate_node){
			item = item->next_sibling("entry");
			continue;
		}

		title_str = title_node->value();
		link_str = href_attr->value();
		pubdate_str = pdate_node->value();

		struct tm itemDatetime = {};
		if(ParseAnyDatetime(pubdate_str,&itemDatetime) == 0){
			/* I noticed the YouTube header only has the <published> tag, which corresponds to the date of channel creation and
			 * does not actually tell us the last update time. So I need to get from the item by doing a comparison.*/

			if(!headerDefined || CompareDates(&headerDatetime, &itemDatetime) == 2){
				headerDatetime = itemDatetime;    
				headerDefined = true;
			}
		}
	
		bool blacklisted = blacklistManager->Check(title_str,link_str); 
		bool isEntryNew = feed::VerifyEntryDate(rssUrl.url,pubdate_str);

		if(!blacklisted && isEntryNew){
			rssManager->RecordEntry(title_str, link_str, rssUrl, pubdate_str);
			countNewEntries++;
			feed::ParsePrintStatus(rssUrl, IN_PROGRESS, countNewEntries);
			rssManager->parsedEntriesCount++;
		}
		item = item->next_sibling("entry");
		//DebugPrintDate(&itemDatetime);
	}
	
	if(countNewEntries == 0)
		feed::ParsePrintStatus(rssUrl, FINISHED_NOCHANGE, countNewEntries);
	else
		feed::ParsePrintStatus(rssUrl, FINISHED_UPDATED, countNewEntries);
	
	if(headerDefined) feed::UpdateRecord(rssUrl.url, &headerDatetime);
	//DebugPrintDate(&headerDatetime);
	
	return 0;
}


int feed::Fetch(rss_url &rssUrl){
	CURL * curl;
	CURLcode res;
	string &url = rssUrl.url;
	curl = curl_easy_init();
	if(!curl) return 1;

	string readBuffer;
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYSTATUS, 0);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/114.0.0.0 Safari/537.36");
   	curl_easy_setopt(curl, CURLOPT_AUTOREFERER, 1);
#ifdef _WIN32
	curl_easy_setopt(curl, CURLOPT_CAINFO, "../server/cacert.pem");
#endif
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteData);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
	
	struct curl_slist *headers = NULL;
	headers = curl_slist_append(headers, "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7");
	headers = curl_slist_append(headers, "Accept-Language: en-US,en;q=0.9");
	headers = curl_slist_append(headers, "Upgrade-Insecure-Requests: 1");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

#ifdef CURL_HTTP_VERSION_2_0
	curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);
#endif

	res = curl_easy_perform(curl);
 	if(res != CURLE_OK){
		fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		feed::ParsePrintStatus(rssUrl, FAILED, 0);
		curl_easy_cleanup(curl);
		curl_slist_free_all(headers);
		return 1;
	}
	long http_code = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
	if(http_code >= 400){
		feed::ParsePrintStatus(rssUrl, FAILED, 0);
		curl_easy_cleanup(curl);
		curl_slist_free_all(headers);
		return 1;
	}
	rssManager->Parse(readBuffer, rssUrl);
    curl_easy_cleanup(curl);
	curl_slist_free_all(headers);
	return 0;
}
