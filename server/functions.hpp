#include <cctype>
#include <string>

inline bool IsNumber(const std::string& s){
    std::string::const_iterator it = s.begin();
    while (it != s.end() && std::isdigit(static_cast<unsigned char>(*it))) ++it;
    return !s.empty() && it == s.end();
}
