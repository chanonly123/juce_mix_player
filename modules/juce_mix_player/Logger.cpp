#include "Logger.h"

bool enableLogsValue = false;

const char* returnCopyCharDelete(std::string str) {
    return returnCopyCharDelete(str.c_str());
}

const char* returnCopyCharDelete(const char* string) {
    char* copy = (char*)malloc(strlen(string) + 1);
    strcpy(copy, string);

    std::thread thread([copy]{
        std::this_thread::sleep_for(std::chrono::seconds(5));
        free(copy);
    });
    thread.detach();
    return copy;
}

std::string toLower(std::string str) {
    for (auto& e: str) e = std::tolower(e);
    return str;
}

bool setContains(std::unordered_set<int>& set, int val) {
    return set.find(val) != set.end();
}
