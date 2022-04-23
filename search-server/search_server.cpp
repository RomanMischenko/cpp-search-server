#include "string_processing.h"
#include "search_server.h"
#include "log_duration.h"


using namespace std;

SearchServer::SearchServer(const string& stop_words) 
    : SearchServer(string_view(stop_words))
{}

SearchServer::SearchServer(const string_view& stop_words) 
    : SearchServer(SplitIntoWords(stop_words))
{}

void SearchServer::SetStopWords(const string_view& text) {
        InsertCorrectStopWords(SplitIntoWords(text));
}    

vector<Document> SearchServer::FindTopDocuments(const string_view& raw_query) const { 
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL); 
}

vector<Document> SearchServer::FindTopDocuments(execution::sequenced_policy policy, const string_view& raw_query) const { 
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL); 
}

vector<Document> SearchServer::FindTopDocuments(execution::parallel_policy policy, const string_view& raw_query) const { 
    return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL); 
}

vector<Document> SearchServer::FindTopDocuments(const string_view& raw_query, DocumentStatus status) const {
    auto key_l = [status](int document_id, DocumentStatus compare_status, int rating) { 
        return status == compare_status;  };
    return FindTopDocuments(raw_query, key_l); 
}

vector<Document> SearchServer::FindTopDocuments(execution::sequenced_policy policy, const string_view& raw_query, DocumentStatus status) const {
    auto key_l = [status](int document_id, DocumentStatus compare_status, int rating) { 
        return status == compare_status;  };
    return FindTopDocuments(raw_query, key_l); 
}

vector<Document> SearchServer::FindTopDocuments(execution::parallel_policy policy, const string_view& raw_query, DocumentStatus status) const {
    auto key_l = [status](int document_id, DocumentStatus compare_status, int rating) { 
        return status == compare_status;  };
    return FindTopDocuments(policy, raw_query, key_l); 
}

void SearchServer::AddDocument(int document_id, const string_view document, DocumentStatus status, const vector<int>& ratings) {
    if (document_id < 0 || documents_.count(document_id)) {
        throw invalid_argument("invalid_argument"s);
    }
    const vector<string_view> words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();
    for (const string_view word_view : words) {
        string word(word_view);
        word_to_document_freqs_[word][document_id] += inv_word_count;
        word_frequencies_[document_id][word_to_document_freqs_.find(word)->first] += inv_word_count;
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

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(const string_view raw_query, int document_id) const {
        
    const Query query = ParseQuery(raw_query);

    vector<string_view> matched_words;

    for (const string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(string(word)).count(document_id)) {
            return {matched_words, documents_.at(document_id).status};
        }
    }
    
    for (const string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(string(word)).count(document_id)) {
            matched_words.push_back(word_to_document_freqs_.find(word)->first);
        }
    }


    return {matched_words, documents_.at(document_id).status};
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(execution::sequenced_policy policy, const string_view raw_query, int document_id) const {
    return MatchDocument(raw_query, document_id);
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(execution::parallel_policy policy, const string_view raw_query, int document_id) const {

    Query query = ParseQuery(raw_query, true);

    vector<string_view> matched_words(query.plus_words.size());

    const auto word_checker = [this, document_id](const string_view word){
        const auto it = word_to_document_freqs_.find(word);
        return it != word_to_document_freqs_.end() && it->second.count(document_id);
    };

    if (any_of(policy, query.minus_words.begin(), query.minus_words.end(), word_checker)) {
        matched_words.clear();
        return {matched_words, documents_.at(document_id).status};
    }

    atomic<int> index = 0;

    for_each(policy, query.plus_words.begin(), query.plus_words.end(), [&](const string_view word)
    {
        const auto it = word_frequencies_.at(document_id).find(word);
        if (it != word_frequencies_.at(document_id).end()) {
            matched_words.at(index++) = it->first;
        }
    });

    matched_words.resize(index);
    sort(policy, matched_words.begin(), matched_words.end());
    auto words_end = unique(policy, matched_words.begin(), matched_words.end());
    matched_words.erase(words_end, matched_words.end());

    return {matched_words, documents_.at(document_id).status};
}

int SearchServer::GetDocumentId(int index) const {
    if ((index < 0) || (index >= GetDocumentCount())) {
        throw out_of_range("out_of_range"s);
    }
    return sequence_of_adding_id_[index];
}

bool SearchServer::IsValidWord(const string_view& word) {
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    });
}

bool SearchServer::IsStopWord(const string_view& word) const {
    return stop_words_.count(word) > 0;
}

vector<string_view> SearchServer::SplitIntoWordsNoStop(const string_view& text) const {
    vector<string_view> words;
    for (const string_view& word : SplitIntoWords(text)) {
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

SearchServer::QueryWord SearchServer::ParseQueryWord(string_view word) const {
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

SearchServer::Query SearchServer::ParseQuery(const string_view raw_query, bool skip_sort) const {
    Query query;
    for (const string_view word : SplitIntoWords(raw_query)) {
        const QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                query.minus_words.push_back(query_word.data);
            } else {
                query.plus_words.push_back(query_word.data);
            }
        }
    }

    if (!skip_sort) {
        sort(query.minus_words.begin(), query.minus_words.end());
        auto end_minus_words = unique(query.minus_words.begin(), query.minus_words.end());
        query.minus_words.erase(end_minus_words, query.minus_words.end());

        sort(query.plus_words.begin(), query.plus_words.end());
        auto end_plus_words = unique(query.plus_words.begin(), query.plus_words.end());
        query.plus_words.erase(end_plus_words, query.plus_words.end());
    }

    return query;
}

double SearchServer::ComputeWordInverseDocumentFreq(const string_view& word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(string(word)).size());
}

vector<int>::const_iterator SearchServer::begin() const {
    return sequence_of_adding_id_.begin();
}

vector<int>::const_iterator SearchServer::end() const {
    return sequence_of_adding_id_.end();
}

const map<string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    static const map<string_view, double> empty_map = {};
    if (!word_frequencies_.count(document_id)) {
        return empty_map;
    } else {
        return word_frequencies_.at(document_id);
    }
}

void SearchServer::RemoveDocument(int document_id) {
    if (documents_.count(document_id) == 0) {
        return;
    }

    for (auto it = word_frequencies_.at(document_id).begin(); it != word_frequencies_.at(document_id).end(); ++it) {
        if (word_to_document_freqs_.count(it->first)) {
            if (word_to_document_freqs_.at(string(it->first)).size() == 1) {
                word_to_document_freqs_.erase(string(it->first));
            } else if (word_to_document_freqs_.at(string(it->first)).size() > 1) {
                word_to_document_freqs_.at(string(it->first)).erase(document_id);
            }
        }
    }

    documents_.erase(document_id);
    word_frequencies_.erase(document_id);
    sequence_of_adding_id_.erase(find(sequence_of_adding_id_.begin(), sequence_of_adding_id_.end(), document_id));
}

void SearchServer::RemoveDocument(execution::sequenced_policy policy, int document_id) {
    RemoveDocument(document_id);
}

void SearchServer::RemoveDocument(execution::parallel_policy policy, int document_id) {
    if (documents_.count(document_id) == 0) {
        return;
    }

    vector<string_view> words_to_remove(word_frequencies_.at(document_id).size());
 
    transform(policy, word_frequencies_.at(document_id).begin(), word_frequencies_.at(document_id).end(), words_to_remove.begin(),
        [](const auto& i) {
            return (i.first);
        });

    for_each(policy, words_to_remove.begin(), words_to_remove.end(), [&](string_view word){
        if (word_to_document_freqs_.count(word)) {
            word_to_document_freqs_.at(string(word)).erase(document_id);
        }
    });
    
    documents_.erase(document_id);
    word_frequencies_.erase(document_id);
    sequence_of_adding_id_.erase(find(sequence_of_adding_id_.begin(), sequence_of_adding_id_.end(), document_id));

}