#include "request_queue.h"

using namespace std;

RequestQueue::RequestQueue(const SearchServer& search_server)
    : search_server_(search_server), requests_size_(0)
{}

int RequestQueue::GetNoResultRequests() const {
    int result = 0;
    for (const auto& i : requests_) {
        if (i.empty) {
            ++result;
        }
    }
    return result;
}

vector<Document> RequestQueue::AddFindRequest(const string_view& raw_query, DocumentStatus status) {
    auto key_l = [status](int document_id, DocumentStatus compare_status, int rating) { 
        return status == compare_status;  };
    return AddFindRequest(raw_query, key_l);
}

vector<Document> RequestQueue::AddFindRequest(const string_view& raw_query) {
    return AddFindRequest(raw_query, DocumentStatus::ACTUAL);
}
