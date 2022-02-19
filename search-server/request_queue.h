#pragma once
#include "search_server.h"
#include "document.h"
#include <string>
#include <deque>
#include <vector>

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server);

    template <typename DocumentPredicate>
    vector<Document> AddFindRequest(const string& raw_query, DocumentPredicate document_predicate) {
        QueryResult query;
        auto matched_documents = search_server_.FindTopDocuments(raw_query, document_predicate);
        if (requests_size_ < min_in_day_) {
            ++requests_size_;
            if (matched_documents.empty()) {
                query.request = raw_query;
                query.empty = true;
                requests_.push_back(query);
            } else {
                query.request = raw_query;
                query.empty = false;
                requests_.push_back(query);
            }
        } else {
            requests_.pop_front();
            if (matched_documents.empty()) {
                query.request = raw_query;
                query.empty = true;
                requests_.push_back(query);
            } else {
                query.request = raw_query;
                query.empty = false;
                requests_.push_back(query);
            }
        }
        return matched_documents;
    }

    vector<Document> AddFindRequest(const string& raw_query, DocumentStatus status) {
        auto key_l = [status](int document_id, DocumentStatus compare_status, int rating) { 
            return status == compare_status;  };
        return AddFindRequest(raw_query, key_l);
    }

    vector<Document> AddFindRequest(const string& raw_query) {
        return AddFindRequest(raw_query, DocumentStatus::ACTUAL);
    }

    int GetNoResultRequests() const;

private:
    struct QueryResult {
        string request;
        bool empty;
    };
    deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
    const SearchServer& search_server_;
    int requests_size_;
};