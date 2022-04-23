#include "string_processing.h"

using namespace std;

vector<string_view> SplitIntoWords(string_view text) {

    vector<string_view> result;
    auto pos_end = text.npos;
    while (true) {
        uint64_t space = text.find(' ');
        result.push_back(text.substr(0, space));
        text.remove_prefix(space + 1);
        if (space == pos_end) {
            break;
        }
    }
    return result;
} 