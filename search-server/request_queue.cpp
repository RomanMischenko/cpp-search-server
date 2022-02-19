#include "request_queue.h"

using namespace std;

RequestQueue::RequestQueue(const SearchServer& search_server)
    : search_server_(search_server), requests_size_(0)
    {
    }

int RequestQueue::GetNoResultRequests() const {
    int result = 0;
    for (const auto& i : requests_) {
        if (i.empty) {
            ++result;
        }
    }
    return result;
}
