#pragma once
#include <string>
#include <vector>
#include <utility>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <execution>
#include <string_view>
#include <future>
#include <atomic>

#include "log_duration.h"
#include "document.h"
#include "concurrent_map.h"

const double MAXIMUM_MEASUREMENT_ERROR = 1e-6;
const int MAX_RESULT_DOCUMENT_COUNT = 5;

class SearchServer {
public:

    template<typename StringCollection>
    explicit SearchServer(const StringCollection& stop_words);
    explicit SearchServer(const string_view& stop_words);
    explicit SearchServer(const string& stop_words);

    void SetStopWords(const string_view& text);
    
    void AddDocument(int document_id, const string_view document, DocumentStatus status, const vector<int>& ratings);

    template <typename KeyMapper>
    vector<Document> FindTopDocuments(const string_view& raw_query, KeyMapper key_mapper) const;
    vector<Document> FindTopDocuments(const string_view& raw_query, DocumentStatus status) const;
    vector<Document> FindTopDocuments(const string_view& raw_query) const;

    template <typename KeyMapper>
    vector<Document> FindTopDocuments(execution::sequenced_policy policy, const string_view& raw_query, KeyMapper key_mapper) const;
    vector<Document> FindTopDocuments(execution::sequenced_policy policy, const string_view& raw_query, DocumentStatus status) const;
    vector<Document> FindTopDocuments(execution::sequenced_policy policy, const string_view& raw_query) const;

    template <typename KeyMapper>
    vector<Document> FindTopDocuments(execution::parallel_policy policy, const string_view& raw_query, KeyMapper key_mapper) const;
    vector<Document> FindTopDocuments(execution::parallel_policy policy, const string_view& raw_query, DocumentStatus status) const;
    vector<Document> FindTopDocuments(execution::parallel_policy policy, const string_view& raw_query) const;
    
    tuple<vector<string_view>, DocumentStatus> MatchDocument(const string_view raw_query, int document_id) const;
    tuple<vector<string_view>, DocumentStatus> MatchDocument(execution::sequenced_policy, const string_view raw_query, int document_id) const;
    tuple<vector<string_view>, DocumentStatus> MatchDocument(execution::parallel_policy, const string_view raw_query, int document_id) const;

    void RemoveDocument(int document_id);
    void RemoveDocument(execution::sequenced_policy, int document_id);
    void RemoveDocument(execution::parallel_policy, int document_id);

    int GetDocumentCount() const;

    int GetDocumentId(int index) const;

    vector<int>::const_iterator begin() const;

    vector<int>::const_iterator end() const;

    const map<string_view, double>& GetWordFrequencies(int document_id) const;

private:

    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    struct Query {
        vector<string_view> plus_words;
        vector<string_view> minus_words;
    };
    
    struct QueryWord {
        string_view data;
        bool is_minus;
        bool is_stop;
    };

    set<string, less<>> stop_words_;
    map<string, map<int, double>, less<>> word_to_document_freqs_;
    map<int, DocumentData> documents_;
    vector<int> sequence_of_adding_id_;
    map<int, map<string_view, double>> word_frequencies_;

    template<typename StringCollection>
    void InsertCorrectStopWords(const StringCollection& stop_words);
    
    static bool IsValidWord(const string_view& word);
    
    bool IsStopWord(const string_view& word) const;
    
    vector<string_view> SplitIntoWordsNoStop(const string_view& text) const;
    
    static int ComputeAverageRating(const vector<int>& ratings);
    
    QueryWord ParseQueryWord(string_view word) const;
    
    
    Query ParseQuery(const string_view raw_query, bool skip_sort = false) const;
    
    double ComputeWordInverseDocumentFreq(const string_view& word) const;

    template <typename Predicant>
    vector<Document> FindAllDocuments(const Query& query, Predicant predicant) const;

    template <typename Predicant>
    vector<Document> FindAllDocuments(execution::parallel_policy policy, const Query& query, Predicant predicant) const;
};

template<typename StringCollection>
SearchServer::SearchServer(const StringCollection& stop_words) {
    InsertCorrectStopWords(stop_words);
}

template <typename KeyMapper>
vector<Document> SearchServer::FindTopDocuments(execution::sequenced_policy policy, const string_view& raw_query, KeyMapper key_mapper) const {
    return FindTopDocuments(raw_query, key_mapper);
}

template <typename KeyMapper>
vector<Document> SearchServer::FindTopDocuments(execution::parallel_policy policy, const string_view& raw_query, KeyMapper key_mapper) const {
    Query query = ParseQuery(raw_query, true);

	sort(policy, query.minus_words.begin(), query.minus_words.end());
    sort(policy, query.plus_words.begin(), query.plus_words.end());

    auto end_minus_words = unique(policy, query.minus_words.begin(), query.minus_words.end());
    auto end_plus_words = unique(policy, query.plus_words.begin(), query.plus_words.end());

    auto future_erase_minus_words = async([&query, &end_minus_words]{
		query.minus_words.erase(end_minus_words, query.minus_words.end());
	});
    query.plus_words.erase(end_plus_words, query.plus_words.end());

    future_erase_minus_words.get();

    auto matched_documents = FindAllDocuments(policy, query, key_mapper);
    
    sort(policy, matched_documents.begin(), matched_documents.end(),
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

template <typename KeyMapper>
vector<Document> SearchServer::FindTopDocuments(const string_view& raw_query, KeyMapper key_mapper) const {   
    //LOG_DURATION_STREAM("Operation time"s, cout);         
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
    for (const auto& word_view : stop_words) {
        string word(word_view);
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

    for (const string_view& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(string(word))) {
            if (predicant(document_id, documents_.at(document_id).status, documents_.at(document_id).rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }
 
    for (const string_view& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(string(word))) {
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

template <typename Predicant>
vector<Document> SearchServer::FindAllDocuments(execution::parallel_policy policy, const Query& query, Predicant predicant) const {
    //map<int, double> document_to_relevance;
    ConcurrentMap<int, double> document_to_relevance(100);

    for_each(policy, query.plus_words.begin(), query.plus_words.end(), [&](const string_view& word)
    {
        auto check_is_minus_word = any_of(policy, query.minus_words.begin(), query.minus_words.end(), [&word](const string_view& minus_word) 
        {
            return minus_word == word;
        });
        if (word_to_document_freqs_.count(word) && !check_is_minus_word) {
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for_each(policy, word_to_document_freqs_.at(string(word)).begin(), word_to_document_freqs_.at(string(word)).end(), [&](const auto& pair)
            {
                // pair.first -> document_id
                // pair.second -> term_freq
                if (predicant(pair.first, documents_.at(pair.first).status, documents_.at(pair.first).rating)) {
                    document_to_relevance[pair.first].ref_to_value += pair.second * inverse_document_freq;
                }
            });
        }
    });

    atomic<int> index = 0;
    map<int, double> joint_document_to_relevance = document_to_relevance.BuildOrdinaryMap();
    vector<Document> matched_documents(joint_document_to_relevance.size());

    for_each(policy, joint_document_to_relevance.begin(), joint_document_to_relevance.end(), [&](const auto& pair)
    {
        // pair.first -> document_id
        // pair.second -> term_freq
        matched_documents[index++] = {pair.first, pair.second, documents_.at(pair.first).rating};

    });

    matched_documents.resize(index);

    return matched_documents;
}
