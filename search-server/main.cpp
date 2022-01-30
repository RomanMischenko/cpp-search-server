#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <sstream>
#include <numeric>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double MAXIMUM_MEASUREMENT_ERROR = 1e-6;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}
    
struct Document {
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }    
    
    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
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
    }

    vector<Document> FindTopDocuments(const string& raw_query) const { 
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL); 
    }

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status_1) const { 
        auto key_l = [status_1](int document_id, DocumentStatus status_2, int rating) { 
            return status_1 == status_2;  };
        return FindTopDocuments(raw_query, key_l); 
    }


    template <typename KeyMapper>
    vector<Document> FindTopDocuments(const string& raw_query, KeyMapper keymapper) const {            
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, keymapper);
        
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

    int GetDocumentCount() const {
        return documents_.size();
    }
    
    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
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
    
private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;
    
    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }
    
    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }
    
    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
        return rating_sum / static_cast<int>(ratings.size());
    }
    
    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };
    
    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {
            text,
            is_minus,
            IsStopWord(text)
        };
    }
    
    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };
    
    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
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
    
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename Predicant>
    vector<Document> FindAllDocuments(const Query& query, Predicant predicant) const {
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
};


void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating
         << " }"s << endl;
}

template <typename ElementPair1, typename ElementPair2>
ostream& operator<<(ostream& out, const pair<ElementPair1, ElementPair2>& container) {
    out << container.first << ": " << container.second;
    return out;
}

template <typename Element>
ostream& Print (ostream& out, const Element& container) {
    bool is_first = true;
    for (const auto& element : container) {
        if (!is_first) {
            out << ", "s;
        }
        is_first = false;
        out << element;
    }
    return out;
}


template <typename Element>
ostream& operator<<(ostream& out, const vector<Element>& container) {
    out << "["s;
    Print(out, container);
    out << "]"s;
    return out;
}

template <typename Element>
ostream& operator<<(ostream& out, const set<Element>& container) {
    out << "{"s;
    Print(out, container);
    out << "}"s;
    return out;
}

template <typename Key, typename Value>
ostream& operator<<(ostream& out, const map<Key, Value>& container) {
    out << "{"s;
    Print(out, container);
    out << "}"s;
    return out;
}

ostream& operator<<(ostream& out, const DocumentStatus status) {
    if (status == DocumentStatus::ACTUAL) {
        out << "DocumentStatus::ACTUAL"s;
    } else if (status == DocumentStatus::BANNED) {
        out << "DocumentStatus::BANNED"s;
    } else if (status == DocumentStatus::IRRELEVANT) {
        out << "DocumentStatus::IRRELEVANT"s;
    } else if (status == DocumentStatus::REMOVED) {
        out << "DocumentStatus::REMOVED"s;
    } else {
        cerr << "..." << __LINE__ << " - сюда попасть не должно";
        abort();
    }
    return out;
}

template <typename T, typename U>
void ASSERTEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
                     const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cerr << boolalpha;
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) ASSERTEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) ASSERTEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void ASSERTImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
                const string& hint) {
    if (!value) {
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT(expr) ASSERTImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) ASSERTImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))


// -------- Начало модульных тестов поисковой системы ----------

// ----0----
// Тест из примера.
// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов.
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    // Сначала убеждаемся, что поиск слова, не входящего в список стоп-слов,
    // находит нужный документ
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    // Затем убеждаемся, что поиск этого же слова, входящего в список стоп-слов,
    // возвращает пустой результат
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("in"s).empty());
    }
}

// ----1----
// Добавление документов. 
// Добавленный документ должен находиться по поисковому запросу, который содержит слова из документа.

void TestAddDocuments() {
    SearchServer server;

    ASSERT(server.FindTopDocuments("test"s).empty());

    server.AddDocument(0, "test test_1"s, DocumentStatus::ACTUAL, {1, 2, 3});
    ASSERT_EQUAL(server.GetDocumentCount(), 1);
    ASSERT_EQUAL(server.FindTopDocuments("test"s).size(), 1);
    ASSERT_EQUAL(server.FindTopDocuments("test"s)[0].id, 0);
    server.AddDocument(1, "test"s, DocumentStatus::ACTUAL, {1, 2, 3});
    ASSERT_EQUAL(server.GetDocumentCount(), 2);
    ASSERT_EQUAL(server.FindTopDocuments("test"s).size(), 2);
    ASSERT_EQUAL(server.FindTopDocuments("test"s)[0].id, 0);
    ASSERT_EQUAL(server.FindTopDocuments("test"s)[1].id, 1);
}

// ----2----
// Поддержка стоп-слов. 
// Стоп-слова исключаются из текста документов.
void TestStopWords() {
    const int document_id_1 = 1;
    const int document_id_2 = 2;
    const string document_1 = "test test_1 test_2 test_3"s;
    const string document_2 = "test test_3 test_4"s;
    const DocumentStatus status = DocumentStatus::ACTUAL;
    const vector<int> ratings_document_1 = {1, 2, 3}; // 2
    const vector<int> ratings_document_2 = {5, 5, 5}; // 5

    SearchServer server;
    server.SetStopWords("test test_1 test_2"s);
    server.AddDocument(document_id_1, document_1, status, ratings_document_1);
    server.AddDocument(document_id_2, document_2, status, ratings_document_2);

    const auto find_document_1 = server.FindTopDocuments("test"s);
    const auto find_document_2 = server.FindTopDocuments("test_1"s);
    const auto find_document_3 = server.FindTopDocuments("test_3 test_4"s);
    const auto find_document_4 = server.FindTopDocuments("test_4"s);

    ASSERT(find_document_1.empty());
    ASSERT(find_document_2.empty());
    ASSERT_EQUAL(find_document_3.size(), 2);
    ASSERT_EQUAL(find_document_4.size(), 1);

    ASSERT_EQUAL(find_document_3[0].id, 2);
    ASSERT_EQUAL(find_document_3[1].id, 1);
    ASSERT_EQUAL(find_document_4[0].id, 2);
}

// ----3----
// Поддержка минус-слов. 
// Документы, содержащие минус-слова поискового запроса, не должны включаться
// в результаты поиска.
void TestMinusWords() {
    {
        const int document_id_1 = 1;
        const int document_id_2 = 2;
        const string document_1 = "test test_1 test_2 test_3"s;
        const string document_2 = "test test_3 test_4"s;
        const DocumentStatus status = DocumentStatus::ACTUAL;
        const vector<int> ratings_document_1 = {1, 2, 3}; // 2
        const vector<int> ratings_document_2 = {5, 5, 5}; // 5

        SearchServer server;
        server.AddDocument(document_id_1, document_1, status, ratings_document_1);
        server.AddDocument(document_id_2, document_2, status, ratings_document_2);

        const auto find_document_1 = server.FindTopDocuments("test test"s);
        const auto find_document_2 = server.FindTopDocuments("test_1"s);
        const auto find_document_3 = server.FindTopDocuments("test_3"s);
        const auto find_document_4 = server.FindTopDocuments("test_3 test_2 test_5"s);
        const auto find_document_5 = server.FindTopDocuments("test_3 test_4 test"s);

        ASSERT_EQUAL(find_document_1.size(), 2);
        ASSERT_EQUAL(find_document_1[0].id, 2);
        ASSERT_EQUAL(find_document_1[1].id, 1);

        ASSERT_EQUAL(find_document_2.size(), 1);
        ASSERT_EQUAL(find_document_2[0].id, 1);

        ASSERT_EQUAL(find_document_3.size(), 2);
        ASSERT_EQUAL(find_document_3[0].id, 2);
        ASSERT_EQUAL(find_document_3[1].id, 1);

        ASSERT_EQUAL(find_document_4.size(), 2);
        ASSERT_EQUAL(find_document_4[0].id, 1);
        ASSERT_EQUAL(find_document_4[1].id, 2);

        ASSERT_EQUAL(find_document_5.size(), 2);
        ASSERT_EQUAL(find_document_5[0].id, 2);
        ASSERT_EQUAL(find_document_5[1].id, 1);

        ASSERT(server.FindTopDocuments("-test"s).empty());
        ASSERT(server.FindTopDocuments("-test_5"s).empty());
        ASSERT(server.FindTopDocuments("-test test"s).empty());
        ASSERT(server.FindTopDocuments("-test_1 -test"s).empty());
        ASSERT_EQUAL(server.FindTopDocuments("-test_1 test"s).size(), 1);
        ASSERT_EQUAL(server.FindTopDocuments("-test_1 test"s)[0].id, 2);
    }

    {
        SearchServer server_1, server_2;

        server_1.AddDocument(0, "huge flying green cat"s, DocumentStatus::ACTUAL, {0});
        ASSERT(server_1.FindTopDocuments("cat -cat"s).empty());

        server_2.AddDocument(0, "test test_1 test_2 test_3"s, DocumentStatus::ACTUAL, {0});
        server_2.AddDocument(1, "test test_3 test_4"s, DocumentStatus::ACTUAL, {0});

        ASSERT_EQUAL(server_2.FindTopDocuments("test"s).size(), 2);
        ASSERT_EQUAL(server_2.FindTopDocuments("test"s)[0].id, 0);
        ASSERT_EQUAL(server_2.FindTopDocuments("test"s)[1].id, 1);

        ASSERT_EQUAL(server_2.FindTopDocuments("test -test_4"s).size(), 1);
        ASSERT_EQUAL(server_2.FindTopDocuments("test -test_4"s)[0].id, 0);
    }
}

// ----4----
// Матчинг документов. 
// При матчинге документа по поисковому запросу должны быть возвращены
// все слова из поискового запроса, присутствующие в документе. 
// Если есть соответствие хотя бы по одному минус-слову, 
// должен возвращаться пустой список слов.
void TestMatchDocument() {
    const int document_id_1 = 1;
    const int document_id_2 = 2;
    const string document_1 = "test test_1 test_2 test_3"s;
    const string document_2 = "test test_3 test_4"s;
    const DocumentStatus status = DocumentStatus::ACTUAL;
    const vector<int> ratings_document_1 = {1, 2, 3}; // 2
    const vector<int> ratings_document_2 = {5, 5, 5}; // 5

    SearchServer server;
    server.AddDocument(document_id_1, document_1, status, ratings_document_1);
    server.AddDocument(document_id_2, document_2, status, ratings_document_2);

    const auto find_document_1 = server.MatchDocument("test test_1 test_6"s, document_id_1);
    const auto find_document_2 = server.MatchDocument("test_3 test"s, document_id_2);
    const auto find_document_3 = server.MatchDocument("test -test_1"s, document_id_1);
    const auto find_document_4 = server.MatchDocument("-test"s, document_id_2);
    const auto find_document_5 = server.MatchDocument("-test test"s, document_id_2);
    const auto find_document_6 = server.MatchDocument("test -test_1"s, document_id_2);

    ASSERT_EQUAL(get<0>(find_document_1).size(), 2);
    ASSERT_EQUAL(get<0>(find_document_2).size(), 2);
    ASSERT(get<0>(find_document_3).empty());
    ASSERT(get<0>(find_document_4).empty());
    ASSERT(get<0>(find_document_5).empty());
    ASSERT_EQUAL(get<0>(find_document_6).size(), 1);
 
    ASSERT_EQUAL(get<1>(find_document_1), DocumentStatus::ACTUAL);
    ASSERT_EQUAL(get<1>(find_document_2), DocumentStatus::ACTUAL);
    ASSERT_EQUAL(get<1>(find_document_3), DocumentStatus::ACTUAL);
    ASSERT_EQUAL(get<1>(find_document_4), DocumentStatus::ACTUAL);
    ASSERT_EQUAL(get<1>(find_document_5), DocumentStatus::ACTUAL);
    ASSERT_EQUAL(get<1>(find_document_6), DocumentStatus::ACTUAL);
 
    ASSERT(get<0>(find_document_1)[0] == "test"s);
    ASSERT(get<0>(find_document_1)[1] == "test_1"s);
    ASSERT(get<0>(find_document_2)[0] == "test"s);
    ASSERT(get<0>(find_document_2)[1] == "test_3"s);
    ASSERT(get<0>(find_document_6)[0] == "test"s);
}

// ----5----
// Сортировка найденных документов по релевантности. 
// Возвращаемые при поиске документов результаты должны быть отсортированы 
// в порядке убывания релевантности.
void TestRelevanse() {
    {    
        SearchServer search_server;
        search_server.SetStopWords("и в на"s);

        search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
        search_server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
        search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
        search_server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::ACTUAL, {9});

        const auto find_document = search_server.FindTopDocuments("пушистый ухоженный кот"s);

        ASSERT(is_sorted(find_document.begin(), find_document.end(), [] (const auto lhs, const auto rhs) {
            return lhs.relevance > rhs.relevance; }));

        ASSERT_EQUAL(find_document[0].id, 1);
        ASSERT_EQUAL(find_document[1].id, 3);
        ASSERT_EQUAL(find_document[2].id, 0);
        ASSERT_EQUAL(find_document[3].id, 2);
    }

    {
        SearchServer search_server;
        search_server.AddDocument(0, "test"s, DocumentStatus::ACTUAL, {2});
        search_server.AddDocument(1, "test"s, DocumentStatus::ACTUAL, {1});
        search_server.AddDocument(2, "test"s, DocumentStatus::ACTUAL, {0});

        const auto find_document = search_server.FindTopDocuments("test"s);

        ASSERT(is_sorted(find_document.begin(), find_document.end(), [] (const auto lhs, const auto rhs) {
            return lhs.rating > rhs.rating; }));

    }
}

// ----6----
// Вычисление рейтинга документов. 
// Рейтинг добавленного документа равен среднему арифметическому оценок документа.

void TestRatingCalculation() {
    const int document_id_1 = 1;
    const int document_id_2 = 2;
    const int document_id_3 = 3;
    const int document_id_4 = 4;
    const string document = "test"s;
    const DocumentStatus status = DocumentStatus::ACTUAL;
    const vector<int> ratings_document_1 = {1, 2, 3}; // 2
    const vector<int> ratings_document_2 = {-20, -20, -20}; // -20
    const vector<int> ratings_document_3 = {0, 0, 0}; // 0
    const vector<int> ratings_document_4 = {2, 3, 5}; // 3

    SearchServer server;
    server.AddDocument(document_id_1, document, status, ratings_document_1);
    server.AddDocument(document_id_2, document, status, ratings_document_2);
    server.AddDocument(document_id_3, document, status, ratings_document_3);
    server.AddDocument(document_id_4, document, status, ratings_document_4);

    const auto find_document = server.FindTopDocuments("test"s, DocumentStatus::ACTUAL);
    
    ASSERT_EQUAL(find_document[0].rating, (2+3+5)/3);
    ASSERT_EQUAL(find_document[1].rating, (1+2+3)/3);
    ASSERT_EQUAL(find_document[2].rating, (0+0+0)/3);
    ASSERT_EQUAL(find_document[3].rating, (-20-20-20)/3);

    ASSERT_EQUAL(find_document[0].id, 4);
    ASSERT_EQUAL(find_document[1].id, 1);
    ASSERT_EQUAL(find_document[2].id, 3);
    ASSERT_EQUAL(find_document[3].id, 2);

}

// ----7----
// Фильтрация результатов поиска с использованием предиката, 
// задаваемого пользователем.
void TestFilterWithPredecant() {
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);

    search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});

    const auto find_document_1 = search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; });
    const auto find_document_2 = search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return status == DocumentStatus::ACTUAL; });
    const auto find_document_3 = search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return rating > 0; });
    const auto find_document_4 = search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return 1; });

    // Фильтрация результатов поиска с использованием предиката, 
    // задаваемого пользователем.
    ASSERT_EQUAL(find_document_1.size(), 2);
    ASSERT_EQUAL(find_document_1[0].id, 0);
    ASSERT_EQUAL(find_document_1[1].id, 2);
    // проверка четности всех id
    for (const auto& i : find_document_1) {
        ASSERT_EQUAL(i.id % 2, 0);
    }

    ASSERT_EQUAL(find_document_2.size(), 3);
    ASSERT_EQUAL(find_document_2[0].id, 1);
    ASSERT_EQUAL(find_document_2[1].id, 0);
    ASSERT_EQUAL(find_document_2[2].id, 2);
    // проверка отсутствия документа с DocumentStatus::BANNED
    for (const auto& i : find_document_2) {
        ASSERT(i.id != 3);
    }

    ASSERT_EQUAL(find_document_3.size(), 3);
    ASSERT_EQUAL(find_document_3[0].id, 1);
    ASSERT_EQUAL(find_document_3[1].id, 3);
    ASSERT_EQUAL(find_document_3[2].id, 0);
    // проверка, что рейтинг документов > 0
    for (const auto& i : find_document_3) {
        ASSERT(i.rating > 0);
    }

    ASSERT_EQUAL(find_document_4.size(), 4);
    ASSERT_EQUAL(find_document_4[0].id, 1);
    ASSERT_EQUAL(find_document_4[1].id, 3);
    ASSERT_EQUAL(find_document_4[2].id, 0);
    ASSERT_EQUAL(find_document_4[3].id, 2);
}

// ----8----
//Поиск документов, имеющих заданный статус.
void FineDocumendWithStatus() {
    SearchServer search_server;

    search_server.AddDocument(0, "тест"s,        DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "тест"s,       DocumentStatus::BANNED, {7, 2, 7});
    search_server.AddDocument(2, "тест"s, DocumentStatus::IRRELEVANT, {5, -12, 2, 1});
    search_server.AddDocument(3, "тест"s,         DocumentStatus::REMOVED, {9});

    const auto find_document_actual = search_server.FindTopDocuments("тест"s, DocumentStatus::ACTUAL);
    const auto find_document_banned = search_server.FindTopDocuments("тест"s, DocumentStatus::BANNED);
    const auto find_document_irrelevant = search_server.FindTopDocuments("тест"s, DocumentStatus::IRRELEVANT);
    const auto find_document_removed = search_server.FindTopDocuments("тест"s, DocumentStatus::REMOVED);

    ASSERT_EQUAL(find_document_actual[0].id, 0);
    ASSERT_EQUAL(find_document_actual.size(), 1);
    ASSERT_EQUAL(find_document_banned[0].id, 1);
    ASSERT_EQUAL(find_document_banned.size(), 1);
    ASSERT_EQUAL(find_document_irrelevant[0].id, 2);
    ASSERT_EQUAL(find_document_irrelevant.size(), 1);
    ASSERT_EQUAL(find_document_removed[0].id, 3);
    ASSERT_EQUAL(find_document_removed.size(), 1);

}

// ----9----
// Корректное вычисление релевантности найденных документов.
void TestSolveRelevanse() {
    {
        SearchServer search_server;
        search_server.SetStopWords("и в на"s);

        search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {0});
        search_server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {0});
        search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {0});

        const auto find_document = search_server.FindTopDocuments("пушистый ухоженный кот"s);
        
        const double MAXIMUM_MEASUREMENT_ERROR_FOR_TEST = 1e-6;

        ASSERT_EQUAL(find_document.size(), 3);
        ASSERT(std::abs(find_document[0].relevance - 0.650672) < MAXIMUM_MEASUREMENT_ERROR_FOR_TEST);
        ASSERT(std::abs(find_document[1].relevance - 0.274653) < MAXIMUM_MEASUREMENT_ERROR_FOR_TEST);
        ASSERT(std::abs(find_document[2].relevance - 0.101366) < MAXIMUM_MEASUREMENT_ERROR_FOR_TEST);
        ASSERT_EQUAL(find_document[0].id, 1);
        ASSERT_EQUAL(find_document[1].id, 2);
        ASSERT_EQUAL(find_document[2].id, 0);
    }

    {
        SearchServer search_server;
        search_server.SetStopWords("is are was a an in the with near at"s);

        search_server.AddDocument(0, "a colorful parrot with green wings and red tail is lost"s,        DocumentStatus::ACTUAL, {0});
        search_server.AddDocument(1, "a grey hound with black ears is found at the railway station"s,       DocumentStatus::ACTUAL, {0});
        search_server.AddDocument(2, "a white cat with long furry tail is found near the red square"s, DocumentStatus::ACTUAL, {0});

        const auto find_document = search_server.FindTopDocuments("white cat long tail"s);
        
        const double MAXIMUM_MEASUREMENT_ERROR_FOR_TEST = 1e-6;

        ASSERT_EQUAL(find_document.size(), 2);
        ASSERT(std::abs(find_document[0].relevance - 0.462663) < MAXIMUM_MEASUREMENT_ERROR_FOR_TEST);
        ASSERT(std::abs(find_document[1].relevance - 0.0506831) < MAXIMUM_MEASUREMENT_ERROR_FOR_TEST);
        ASSERT_EQUAL(find_document[0].id, 2);
        ASSERT_EQUAL(find_document[1].id, 0);
    }

    {
        SearchServer search_server;
        search_server.SetStopWords("и в на белый кот и модный ошейник"s);

        search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {0});
        search_server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {0});
        search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {0});

        const auto find_document = search_server.FindTopDocuments("пушистый ухоженный кот"s);
        
        const double MAXIMUM_MEASUREMENT_ERROR_FOR_TEST = 1e-6;

        ASSERT_EQUAL(find_document.size(), 2);
        ASSERT(std::abs(find_document[0].relevance - 0.732408) < MAXIMUM_MEASUREMENT_ERROR_FOR_TEST);
        ASSERT(std::abs(find_document[1].relevance - 0.274653) < MAXIMUM_MEASUREMENT_ERROR_FOR_TEST);
        ASSERT_EQUAL(find_document[0].id, 1);
        ASSERT_EQUAL(find_document[1].id, 2);
    }

}

// Функция TestSearchServer является точкой входа для запуска тестов.
void TestSearchServer() {
    TestExcludeStopWordsFromAddedDocumentContent(); // 0
    TestAddDocuments(); // 1
    TestStopWords(); // 2
    TestMinusWords(); // 3
    TestMatchDocument(); // 4
    TestRelevanse(); // 5
    TestRatingCalculation(); // 6
    TestFilterWithPredecant(); // 7
    FineDocumendWithStatus(); // 8
    TestSolveRelevanse(); // 9
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;

    SearchServer search_server;
    search_server.SetStopWords("и в на"s);

    search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});

    cout << "ACTUAL by default:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s)) {
        PrintDocument(document);
    }

    cout << "BANNED:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED)) {
        PrintDocument(document);
    }

    cout << "Even ids:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })) {
        PrintDocument(document);
    }
}