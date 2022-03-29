#pragma once
#include "document.h"
#include <string>
#include <vector>
#include <utility>
#include <algorithm>
#include <cmath>
#include <numeric>
#include "log_duration.h"

const double MAXIMUM_MEASUREMENT_ERROR = 1e-6;
const int MAX_RESULT_DOCUMENT_COUNT = 5;

class SearchServer {
public:

    explicit SearchServer(const string& stop_words);

    template<typename StringCollection>
    explicit SearchServer(const StringCollection& stop_words);

    void SetStopWords(const string& text);
    
    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings);

    vector<Document> FindTopDocuments(const string& raw_query) const;

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const;

    template <typename KeyMapper>
    vector<Document> FindTopDocuments(const string& raw_query, KeyMapper key_mapper) const;

    int GetDocumentCount() const;
    
    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const;

    int GetDocumentId(int index) const;

    vector<int>::const_iterator begin() const;

    vector<int>::const_iterator end() const;

    const map<string, double>& GetWordFrequencies(int document_id) const;

    void RemoveDocument(int document_id);
    
private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;
    vector<int> sequence_of_adding_id_;
    map<int, map<string, double>> word_frequencies_;

    template<typename StringCollection>
    void InsertCorrectStopWords(const StringCollection& stop_words);
    
    static bool IsValidWord(const string& word);
    
    bool IsStopWord(const string& word) const;
    
    vector<string> SplitIntoWordsNoStop(const string& text) const;
    
    static int ComputeAverageRating(const vector<int>& ratings);
    
    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };
    
    QueryWord ParseQueryWord(string word) const;
    
    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };
    
    Query ParseQuery(const string& raw_query) const;
    
    double ComputeWordInverseDocumentFreq(const string& word) const;

    template <typename Predicant>
    vector<Document> FindAllDocuments(const Query& query, Predicant predicant) const;
};

template<typename StringCollection>
SearchServer::SearchServer(const StringCollection& stop_words) {
    InsertCorrectStopWords(stop_words);
}

template <typename KeyMapper>
vector<Document> SearchServer::FindTopDocuments(const string& raw_query, KeyMapper key_mapper) const {   
    LOG_DURATION_STREAM("Operation time"s, cout);         
    const Query query = ParseQuery(raw_query);

    auto matched_documents = FindAllDocuments(query, key_mapper);
    
    sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
            if (std::abs(lhs.relevance - rhs.relevance) < MAXIMUM_MEASUREMENT_ERROR) {
                return lhs.rating > rhs.rating;
            } else {
                return lhs.relevance > rhs.relevance;
            }
            });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}

template<typename StringCollection>
void SearchServer::InsertCorrectStopWords(const StringCollection& stop_words) {
    for (const auto& word : stop_words) {
        if (!IsValidWord(word)) {
                throw invalid_argument("invalid_argument"s);
            }
        if (!word.empty()) {
            stop_words_.insert(word);
        }
    }
}

template <typename Predicant>
vector<Document> SearchServer::FindAllDocuments(const Query& query, Predicant predicant) const {
    map<int, double> document_to_relevance;
    for (const string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            if (predicant(document_id, documents_.at(document_id).status, documents_.at(document_id).rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }

    for (const string& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance.erase(document_id);
        }
    }

    vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back({
            document_id,
            relevance,
            documents_.at(document_id).rating
        });
    }
    return matched_documents;
}