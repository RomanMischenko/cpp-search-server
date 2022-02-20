#include "string_processing.h"
#include "search_server.h"

using namespace std;

SearchServer::SearchServer(const string& stop_words) 
    : SearchServer(SplitIntoWords(stop_words))
{}

void SearchServer::SetStopWords(const string& text) {
        InsertCorrectStopWords(SplitIntoWords(text));
}    

vector<Document> SearchServer::FindTopDocuments(const string& raw_query) const { 
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL); 
}

vector<Document> SearchServer::FindTopDocuments(const string& raw_query, DocumentStatus status) const { 
    auto key_l = [status](int document_id, DocumentStatus compare_status, int rating) { 
        return status == compare_status;  };
    return FindTopDocuments(raw_query, key_l); 
}

void SearchServer::AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
    if (document_id < 0 || documents_.count(document_id)) {
        throw invalid_argument("invalid_argument"s);
    }
    const vector<string> words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();
    for (const string& word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
    }
    documents_.emplace(document_id, 
        DocumentData{
            ComputeAverageRating(ratings), 
            status
        });
    sequence_of_adding_id_.push_back(document_id);
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

tuple<vector<string>, DocumentStatus> SearchServer::MatchDocument(const string& raw_query, int document_id) const {
    
    const Query query = ParseQuery(raw_query);

    vector<string> matched_words;
    for (const string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    for (const string& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.clear();
            break;
        }
    }
    return {matched_words, documents_.at(document_id).status};
}

int SearchServer::GetDocumentId(int index) const {
    if ((index < 0) || (index >= GetDocumentCount())) {
        throw out_of_range("out_of_range"s);
    }
    return sequence_of_adding_id_[index];
}

bool SearchServer::IsValidWord(const string& word) {
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}

bool SearchServer::IsStopWord(const string& word) const {
    return stop_words_.count(word) > 0;
}

vector<string> SearchServer::SplitIntoWordsNoStop(const string& text) const {
    vector<string> words;
    for (const string& word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw invalid_argument("invalid_argument"s);
        } else if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(string word) const {
    bool is_minus = false;
    if (word[0] == '-') {
        is_minus = true;
        word = word.substr(1);
    }
    // Проверка на отсутствие в поисковом запросе текста после символа «минус», 
    // на отсутствие спецсимволов и на на указание в поисковом запросе 
    // более чем одного минуса перед словами
    if ((word.size() == 0) || (!IsValidWord(word)) || (word[0] == '-')) {
        throw invalid_argument("invalid_argument"s);
    }
    return {
        word,
        is_minus,
        IsStopWord(word)
    };
}

SearchServer::Query SearchServer::ParseQuery(const string& raw_query) const {
    Query query;
    for (const string& word : SplitIntoWords(raw_query)) {
        const QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                query.minus_words.insert(query_word.data);
            } else {
                query.plus_words.insert(query_word.data);
            }
        }
    }
    return query;
}

double SearchServer::ComputeWordInverseDocumentFreq(const string& word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}