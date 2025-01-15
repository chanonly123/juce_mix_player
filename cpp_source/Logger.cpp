#include "Logger.h"

bool enableLogsValue = false;

const char* returnCopyCharDelete(std::string str) {
    return returnCopyCharDelete(str.c_str());
}

const char* returnCopyCharDelete(const char* string) {
    char* copy = (char*)malloc(strlen(string) + 1);
    strcpy(copy, string);

    std::thread thread([copy]{
        std::this_thread::sleep_for(std::chrono::seconds(1));
        free(copy);
    });
    thread.detach();
    return copy;
}

bool setContains(std::unordered_set<int>& set, int val) {
    return set.find(val) != set.end();
}
