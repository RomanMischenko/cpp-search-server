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
#include <stdexcept>

using namespace std;

const bool TEST_FLAG = true; // true or false
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

    Document() = default;

    Document(int new_id, double new_relevance, int new_rating) 
    : id(new_id), relevance(new_relevance), rating(new_rating)
    {
    }

    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:

    explicit SearchServer(const string& stop_words) 
    : SearchServer(SplitIntoWords(stop_words))
    {
    }

    template<typename StringCollection>
    explicit SearchServer(const StringCollection& stop_words) {
        InsertCorrectStopWords(stop_words);
    }

    void SetStopWords(const string& text) {
        InsertCorrectStopWords(SplitIntoWords(text));
    }    
    
    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
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
                status,
                index_
            });
        ++index_;
    }

    vector<Document> FindTopDocuments(const string& raw_query) const { 
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL); 
    }

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const { 
        auto key_l = [status](int document_id, DocumentStatus compare_status, int rating) { 
            return status == compare_status;  };
        return FindTopDocuments(raw_query, key_l); 
    }

    template <typename KeyMapper>
    vector<Document> FindTopDocuments(const string& raw_query, KeyMapper key_mapper) const {            
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

    int GetDocumentId(int index) const {
        if (documents_.empty()) {
            throw out_of_range("out_of_range"s);
        }
        if (index > static_cast<int>(documents_.size())) {
            throw out_of_range("out_of_range"s);
        }
        for (const auto& [id, value] : documents_) {
            if (value.index == index) {
                return id;
            }
        }
        throw out_of_range("out_of_range"s);
    }
    
private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
        int index;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;
    int index_ = 0;

    template<typename StringCollection>
    void InsertCorrectStopWords(const StringCollection& stop_words) {
        for (const auto& word : stop_words) {
            if (!IsValidWord(word)) {
                    throw invalid_argument("invalid_argument"s);
                }
            if (!word.empty()) {
                stop_words_.insert(word);
            }
        }
    }
    
    static bool IsValidWord(const string& word) {
        return none_of(word.begin(), word.end(), [](char c) {
            return c >= '\0' && c < ' ';
        });
    }
    
    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }
    
    vector<string> SplitIntoWordsNoStop(const string& text) const {
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
    
    Query ParseQuery(const string& raw_query) const {
        Query query;
        // Проверка на отсутствие в поисковом запросе текста после символа «минус»
        for (size_t i = 0; i < raw_query.size() - 1; ++i) {
            if (raw_query[i] == '-' && raw_query[i+1] == ' ') {
                throw invalid_argument("invalid_argument"s);
            } else if ((raw_query[i+1] == '-') && (i + 1 == raw_query.size() - 1)) {
                throw invalid_argument("invalid_argument"s);
            }
        }
        for (const string& word : SplitIntoWords(raw_query)) {
            // Проверка на отсутствие спецсимволов
            if (!IsValidWord(word)) {
                throw invalid_argument("invalid_argument"s);
            }
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                // Проверка на указание в поисковом запросе более чем одного минуса перед словами
                if (query_word.data[0] == '-') {
                    throw invalid_argument("invalid_argument"s);
                }
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
        SearchServer server("  и  в на   "s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1U);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    // Затем убеждаемся, что поиск этого же слова, входящего в список стоп-слов,
    // возвращает пустой результат
    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("in"s).empty());
    }
}

// ----1----
// Добавление документов. 
// Добавленный документ должен находиться по поисковому запросу, который содержит слова из документа.
void TestAddDocuments() {
    SearchServer server("  и  в на   "s);

    ASSERT(server.FindTopDocuments("test"s).empty());

    server.AddDocument(0, "te-st test test_1"s, DocumentStatus::ACTUAL, {1, 2, 3});
    ASSERT_EQUAL(server.GetDocumentCount(), 1);
    ASSERT_EQUAL(server.FindTopDocuments("test"s).size(), 1U);
    ASSERT_EQUAL(server.FindTopDocuments("test"s)[0].id, 0);

    server.AddDocument(1, "te-st"s, DocumentStatus::ACTUAL, {1, 2, 3});
    ASSERT_EQUAL(server.GetDocumentCount(), 2);
    ASSERT_EQUAL(server.FindTopDocuments("te-st"s).size(), 2U);
    ASSERT_EQUAL(server.FindTopDocuments("te-st"s)[0].id, 0);
    ASSERT_EQUAL(server.FindTopDocuments("te-st"s)[1].id, 1);
}

// ----2----
// Поддержка стоп-слов. 
// Стоп-слова исключаются из текста документов.
// Ранее реализовано в тестах из примера.

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

        SearchServer server("  и  в на   "s);
        server.AddDocument(document_id_1, document_1, status, ratings_document_1);
        server.AddDocument(document_id_2, document_2, status, ratings_document_2);

        ASSERT_EQUAL(server.FindTopDocuments("test"s).size(), 2U);
        ASSERT_EQUAL(server.FindTopDocuments("test"s)[0].id, 2);
        ASSERT_EQUAL(server.FindTopDocuments("test"s)[1].id, 1);

        ASSERT_EQUAL(server.FindTopDocuments("test_1"s).size(), 1U);
        ASSERT_EQUAL(server.FindTopDocuments("test_1"s)[0].id, 1);

        ASSERT(server.FindTopDocuments("-test test"s).empty());

        ASSERT_EQUAL(server.FindTopDocuments("-test_1 test"s).size(), 1U);
        ASSERT_EQUAL(server.FindTopDocuments("-test_1 test"s)[0].id, 2);
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

    SearchServer server("in the"s);
    server.AddDocument(document_id_1, document_1, status, ratings_document_1);
    server.AddDocument(document_id_2, document_2, status, ratings_document_2);

    const auto [words_1, status_1] = server.MatchDocument("test test_1 test_6"s, document_id_1);
    ASSERT_EQUAL(words_1.size(), 2U);
    ASSERT_EQUAL(words_1[0], "test"s);
    ASSERT_EQUAL(words_1[1], "test_1"s);

    const auto [words_2, status_2] = server.MatchDocument("test_3 test"s, document_id_2);
    ASSERT_EQUAL(words_2.size(), 2U);
    ASSERT_EQUAL(words_2[0], "test"s);
    ASSERT_EQUAL(words_2[1], "test_3"s);

    const auto [words_3, status_3] = server.MatchDocument("test -test_1"s, document_id_1);
    ASSERT(words_3.empty());

    const auto [words_4, status_4] = server.MatchDocument("-test"s, document_id_2);
    ASSERT(words_4.empty());

    const auto [words_5, status_5] = server.MatchDocument("-test test"s, document_id_2);
    ASSERT(words_5.empty());

    const auto [words_6, status_6] = server.MatchDocument("test -test_1"s, document_id_2);
    ASSERT_EQUAL(words_6.size(), 1U);
    ASSERT_EQUAL(words_6[0], "test"s);
}

// ----5----
// Сортировка найденных документов по релевантности. 
// Возвращаемые при поиске документов результаты должны быть отсортированы 
// в порядке убывания релевантности.
void TestRelevance() {
    vector<Document> find_docs;
    {    
        SearchServer search_server("и в на"s);

        (void) search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, {8, -3});
        (void) search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
        (void) search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
        (void) search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::ACTUAL, {9});

        const auto find_document = search_server.FindTopDocuments("пушистый ухоженный кот"s);

        ASSERT(is_sorted(find_document.begin(), find_document.end(), [] (const auto lhs, const auto rhs) {
            return lhs.relevance > rhs.relevance; }));

        ASSERT_EQUAL(find_document[0].id, 1);
        ASSERT_EQUAL(find_document[1].id, 3);
        ASSERT_EQUAL(find_document[2].id, 0);
        ASSERT_EQUAL(find_document[3].id, 2);
    }

    {
        // А то, что по условию задачи если релевантность одинаковая, то сортируем по рейтингу
        // можно не проверять?
        // Или если проверяем, то делать для этого отдельный тест?
        // Может всё-таки оставим?)
        SearchServer search_server("и в на"s);
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

    SearchServer server("и в на"s);
    server.AddDocument(document_id_1, document, status, ratings_document_1);
    server.AddDocument(document_id_2, document, status, ratings_document_2);
    server.AddDocument(document_id_3, document, status, ratings_document_3);
    server.AddDocument(document_id_4, document, status, ratings_document_4);

    const auto find_document = server.FindTopDocuments("test"s, DocumentStatus::ACTUAL);
    
    ASSERT_EQUAL(find_document[0].rating, (2 + 3 + 5) / 3);
    ASSERT_EQUAL(find_document[1].rating, (1 + 2 + 3) / 3);
    ASSERT_EQUAL(find_document[2].rating, (0 + 0 + 0) / 3);
    ASSERT_EQUAL(find_document[3].rating, ((-20) + (-20) + (-20))/ 3);

    ASSERT_EQUAL(find_document[0].id, 4);
    ASSERT_EQUAL(find_document[1].id, 1);
    ASSERT_EQUAL(find_document[2].id, 3);
    ASSERT_EQUAL(find_document[3].id, 2);
}

// ----7----
// Фильтрация результатов поиска с использованием предиката, 
// задаваемого пользователем.
void TestFilterByPredicate() {
    SearchServer search_server("и в на"s);

    (void) search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    (void) search_server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    (void) search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    (void) search_server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});

    // проверка четности всех id
    const auto find_document_1 = search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; });
    ASSERT_EQUAL(find_document_1.size(), 2U);
    ASSERT_EQUAL(find_document_1[0].id, 0);
    ASSERT_EQUAL(find_document_1[1].id, 2);
    for (const auto& doc : find_document_1) {
        ASSERT_EQUAL(doc.id % 2, 0);
    }

    // проверка отсутствия документа с DocumentStatus::BANNED
    const auto find_document_2 = search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return status == DocumentStatus::ACTUAL; });
    ASSERT_EQUAL(find_document_2.size(), 3U);
    ASSERT_EQUAL(find_document_2[0].id, 1);
    ASSERT_EQUAL(find_document_2[1].id, 0);
    ASSERT_EQUAL(find_document_2[2].id, 2);
    for (const auto& doc : find_document_2) {
        ASSERT(doc.id != 3);
    }

    // проверка, что рейтинг документов > 0
    const auto find_document_3 = search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return rating > 0; });
    ASSERT_EQUAL(find_document_3.size(), 3U);
    ASSERT_EQUAL(find_document_3[0].id, 1);
    ASSERT_EQUAL(find_document_3[1].id, 3);
    ASSERT_EQUAL(find_document_3[2].id, 0);
    for (const auto& doc : find_document_3) {
        ASSERT(doc.rating > 0);
    }


    // проверка на возврат всех добавленных документов
    const auto find_document_4 = search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return 1; });
    ASSERT_EQUAL(find_document_4.size(), 4U);
    ASSERT_EQUAL(find_document_4[0].id, 1);
    ASSERT_EQUAL(find_document_4[1].id, 3);
    ASSERT_EQUAL(find_document_4[2].id, 0);
    ASSERT_EQUAL(find_document_4[3].id, 2);
}

// ----8----
//Поиск документов, имеющих заданный статус.
void TestFineDocumendByStatus() {
    SearchServer search_server("и в на"s);

    search_server.AddDocument(0, "тест"s, DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "тест"s, DocumentStatus::BANNED, {7, 2, 7});
    search_server.AddDocument(2, "тест"s, DocumentStatus::IRRELEVANT, {5, -12, 2, 1});
    search_server.AddDocument(3, "тест"s, DocumentStatus::REMOVED, {9});

    // проверка поиска документа со статусом DocumentStatus::ACTUAL
    const auto find_document_actual = search_server.FindTopDocuments("тест"s, DocumentStatus::ACTUAL);
    ASSERT_EQUAL(find_document_actual[0].id, 0);
    ASSERT_EQUAL(find_document_actual.size(), 1U);

    // проверка поиска документа со статусом DocumentStatus::BANNED
    const auto find_document_banned = search_server.FindTopDocuments("тест"s, DocumentStatus::BANNED);
    ASSERT_EQUAL(find_document_banned[0].id, 1);
    ASSERT_EQUAL(find_document_banned.size(), 1U);

    // проверка поиска документа со статусом DocumentStatus::IRRELEVANT
    const auto find_document_irrelevant = search_server.FindTopDocuments("тест"s, DocumentStatus::IRRELEVANT);
    ASSERT_EQUAL(find_document_irrelevant[0].id, 2);
    ASSERT_EQUAL(find_document_irrelevant.size(), 1U);

    // проверка поиска документа со статусом DocumentStatus::REMOVED
    const auto find_document_removed = search_server.FindTopDocuments("тест"s, DocumentStatus::REMOVED);
    ASSERT_EQUAL(find_document_removed[0].id, 3);
    ASSERT_EQUAL(find_document_removed.size(), 1U);
}

// ----9----
// Корректное вычисление релевантности найденных документов.
void TestCalculateRelevance() {
    {
        SearchServer search_server("и в на"s);

        search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {0});
        search_server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {0});
        search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {0});

        const auto find_document = search_server.FindTopDocuments("пушистый ухоженный кот"s);
        
        const double MAXIMUM_MEASUREMENT_ERROR_FOR_TEST = 1e-6;
        const double RELEVANCE_DOC0 = 0.0 * log(3.0) + 0.0 * log(3.0) + (1.0 / 4.0) * log(3.0 / 2.0); // ~0.1014
        const double RELEVANCE_DOC1 = 0.5 * log(3.0) + 0.0 * log(3.0) + (1.0 / 4.0) * log(3.0 / 2.0); // ~0.6507
        const double RELEVANCE_DOC2 = 0.0 * log(3.0) + (1.0 / 4.0) * log(3.0) + 0.0 * log(3.0 / 2.0); // ~0.2746

        ASSERT_EQUAL(find_document.size(), 3U);
        ASSERT(std::abs(find_document[0].relevance - RELEVANCE_DOC1) < MAXIMUM_MEASUREMENT_ERROR_FOR_TEST);
        ASSERT(std::abs(find_document[1].relevance - RELEVANCE_DOC2) < MAXIMUM_MEASUREMENT_ERROR_FOR_TEST);
        ASSERT(std::abs(find_document[2].relevance - RELEVANCE_DOC0) < MAXIMUM_MEASUREMENT_ERROR_FOR_TEST);
        ASSERT_EQUAL(find_document[0].id, 1);
        ASSERT_EQUAL(find_document[1].id, 2);
        ASSERT_EQUAL(find_document[2].id, 0);
    }
}

// ----10----
// Должен получать идентификатор документа по его порядковому номеру;
// Должен выбрасывать исключение out_of_range, если индекс переданного 
// документа выходит за пределы допустимого диапазона (0; количество документов).
void TestGetDocumentId () {
    // Должен получать идентификатор документа по его порядковому номеру
    {
        SearchServer server(""s);
        server.AddDocument(2, "test"s, DocumentStatus::ACTUAL, {0});
        server.AddDocument(1, "test"s, DocumentStatus::ACTUAL, {0});
        server.AddDocument(0, "test"s, DocumentStatus::ACTUAL, {0});

        ASSERT_EQUAL(server.GetDocumentId(0), 2);
        ASSERT_EQUAL(server.GetDocumentId(1), 1);
        ASSERT_EQUAL(server.GetDocumentId(2), 0);
    }
    // Должен выбрасывать исключение out_of_range
    {
        try
        {
            SearchServer server(""s);
            server.AddDocument(0, "test"s, DocumentStatus::ACTUAL, {0});
            server.GetDocumentId(1);
            ASSERT_EQUAL_HINT(1, 2, "No exception");
        }
        catch(const out_of_range& e)
        {
            ostringstream output;
            output << e.what();
            ASSERT_EQUAL(output.str(), "out_of_range"s);
        }
    }   
}
// ----11----
// Тест конструктора класса SerchServer
// Параметризованный конструктор, должен принимать стоп-слова в следующих форматах:
// - В виде строки, где стоп-слова разделены пробелами.
// В начале, в конце строки и между стоп-словами может быть произвольное количество пробелов.
// - В виде произвольной коллекции строк, такой как vector или set. 
// Пустые строки и слова-дубликаты внутри коллекции должны игнорироваться.
// Конструкторы класса SearchServer должны выбрасывать исключение invalid_argument, 
// если любое из переданных стоп-слов содержит недопустимые символы, 
// то есть символы с кодами от 0 до 31 
void TestSearchServerConstructor() {
    vector<Document> find_document;
    // Инициализируем поисковую систему, передавая стоп-слова в контейнере vector
    {
        const vector<string> stop_words_vector = {"и"s, "в"s, "на"s, ""s, "в"s};
        
        SearchServer search_server1(stop_words_vector);
        

        search_server1.AddDocument(0, "test_1 и в на"s, DocumentStatus::ACTUAL, {0});
        search_server1.AddDocument(1, "test_1 и в на test_2"s, DocumentStatus::ACTUAL, {0});

        ASSERT(search_server1.FindTopDocuments("и").empty());

        ASSERT_EQUAL(search_server1.FindTopDocuments("test_1"s).size(), 2U);
        ASSERT_EQUAL(search_server1.FindTopDocuments("test_1"s)[0].id, 0);
        ASSERT_EQUAL(search_server1.FindTopDocuments("test_1"s)[1].id, 1);

        ASSERT_EQUAL(search_server1.FindTopDocuments("test_2"s).size(), 1U);
        ASSERT_EQUAL(search_server1.FindTopDocuments("test_2"s)[0].id, 1);
    }

    // Инициализируем поисковую систему передавая стоп-слова в контейнере set
    { 
        const set<string> stop_words_set = {"и"s, "в"s, "на"s};
        SearchServer search_server2(stop_words_set);

        search_server2.AddDocument(0, "test_1 и в на"s, DocumentStatus::ACTUAL, {0});
        search_server2.AddDocument(1, "test_1 и в на test_2"s, DocumentStatus::ACTUAL, {0});

        ASSERT(search_server2.FindTopDocuments("и").empty());

        ASSERT_EQUAL(search_server2.FindTopDocuments("test_1"s).size(), 2U);
        ASSERT_EQUAL(search_server2.FindTopDocuments("test_1"s)[0].id, 0);
        ASSERT_EQUAL(search_server2.FindTopDocuments("test_1"s)[1].id, 1);

        ASSERT_EQUAL(search_server2.FindTopDocuments("test_2"s).size(), 1U);
        ASSERT_EQUAL(search_server2.FindTopDocuments("test_2"s)[0].id, 1);
    }

    // Инициализируем поисковую систему строкой со стоп-словами, разделёнными пробелами
    {
        
        SearchServer search_server3("  и  в на   "s);

        search_server3.AddDocument(0, "test_1 и в на"s, DocumentStatus::ACTUAL, {0});
        search_server3.AddDocument(1, "test_1 и в на test_2"s, DocumentStatus::ACTUAL, {0});

        ASSERT(search_server3.FindTopDocuments("и").empty());

        ASSERT_EQUAL(search_server3.FindTopDocuments("test_1"s).size(), 2U);
        ASSERT_EQUAL(search_server3.FindTopDocuments("test_1"s)[0].id, 0);
        ASSERT_EQUAL(search_server3.FindTopDocuments("test_1"s)[1].id, 1);

        ASSERT_EQUAL(search_server3.FindTopDocuments("test_2"s).size(), 1U);
        ASSERT_EQUAL(search_server3.FindTopDocuments("test_2"s)[0].id, 1);
    }

    // Инициализируем поисковую систему со стоп словом содержащим недопустимые символы
    {
        // При добавлении стоп слов строкой
        try
        {
            SearchServer server_1("te\x12st"s);
            ASSERT_EQUAL_HINT(1, 2, "No exception");
        }
        catch(const invalid_argument& e)
        {
            ostringstream output;
            output << e.what();
            ASSERT_EQUAL(output.str(), "invalid_argument"s);
        }
        // При добавлении стоп слов вектором
        try
        {
            const vector<string> stop_words_vector = {"и"s, "te\x12st"s, "на"s, ""s, "в"s};
            SearchServer server_2(stop_words_vector);
            ASSERT_EQUAL_HINT(1, 2, "No exception");
        }
        catch(const invalid_argument& e)
        {
            ostringstream output;
            output << e.what();
            ASSERT_EQUAL(output.str(), "invalid_argument"s);
        }
        // При добавлении стоп слов в контейнере set
        try
        {
            const set<string> stop_words_set = {"и"s, "te\x12st"s, "на"s};
            SearchServer server_3(stop_words_set);
            ASSERT_EQUAL_HINT(1, 2, "No exception");
        }
        catch(const invalid_argument& e)
        {
            ostringstream output;
            output << e.what();
            ASSERT_EQUAL(output.str(), "invalid_argument"s);
        }
    }
}

// ----12----
// Тест метода AddDocument. Он должен выбрасывать исключение invalid_argument в следующих ситуациях:
// - Попытка добавить документ с отрицательным id;
// - Попытка добавить документ c id ранее добавленного документа;
// - Наличие недопустимых символов (с кодами от 0 до 31) в тексте добавляемого документа.
void TestAddDocumentWithInvalidArgument() {
    // Попытка добавить документ с отрицательным id
    try
    {
        SearchServer server_1(""s);
        server_1.AddDocument(-1, "test"s, DocumentStatus::ACTUAL, {0});
        ASSERT_EQUAL_HINT(1, 2, "No exception");
    }
    catch(const invalid_argument& e)
    {
        ostringstream output;
        output << e.what();
        ASSERT_EQUAL(output.str(), "invalid_argument"s);
    }
    // Попытка добавить документ c id ранее добавленного документа
    try
    {
        SearchServer server_2(""s);
        server_2.AddDocument(0, "test"s, DocumentStatus::ACTUAL, {0});
        ASSERT_EQUAL(server_2.GetDocumentCount(), 1);
        
        server_2.AddDocument(0, "test"s, DocumentStatus::ACTUAL, {0});
        ASSERT_EQUAL_HINT(1, 2, "No exception");
    }
    catch(const invalid_argument& e)
    {
        ostringstream output;
        output << e.what();
        ASSERT_EQUAL(output.str(), "invalid_argument"s);
    }
    // Наличие недопустимых символов (с кодами от 0 до 31) в тексте добавляемого документа
    try
    {
        SearchServer server_3(""s);
        server_3.AddDocument(0, "test te\x12st"s, DocumentStatus::ACTUAL, {0});
        ASSERT_EQUAL_HINT(1, 2, "No exception");
    }
    catch(const invalid_argument& e)
    {
        ostringstream output;
        output << e.what();
        ASSERT_EQUAL(output.str(), "invalid_argument"s);
    }
}

// ----13----
// Тест метода FindTopDocuments. Он должен выбрасывать исключение invalid_argument в следующих ситуациях:
// - В словах поискового запроса есть недопустимые символы с кодами от 0 до 31;
// - Наличие более чем одного минуса перед словами;
// - Отсутствие текста после символа «минус».
void TestFindTopDocumentsWithInvalidArgument() {
    // В словах поискового запроса есть недопустимые символы с кодами от 0 до 31
    try
    {
        SearchServer server(""s);
        server.AddDocument(0, "test"s, DocumentStatus::ACTUAL,{0});

        server.FindTopDocuments("te\x12st");
        ASSERT_EQUAL_HINT(1, 2, "No exception");
    }
    catch(const invalid_argument& e)
    {
        ostringstream output;
        output << e.what();
        ASSERT_EQUAL(output.str(), "invalid_argument"s);
    }
    // Наличие более чем одного минуса перед словами
    try
    {
        SearchServer server(""s);
        server.AddDocument(0, "test"s, DocumentStatus::ACTUAL,{0});

        server.FindTopDocuments("test --test");
        ASSERT_EQUAL_HINT(1, 2, "No exception");
    }
    catch(const invalid_argument& e)
    {
        ostringstream output;
        output << e.what();
        ASSERT_EQUAL(output.str(), "invalid_argument"s);
    }
    // Отсутствие текста после символа «минус»
    try
    {
        SearchServer server(""s);
        server.AddDocument(0, "test"s, DocumentStatus::ACTUAL,{0});

        server.FindTopDocuments("test - test");
        ASSERT_EQUAL_HINT(1, 2, "No exception");
    }
    catch(const invalid_argument& e)
    {
        ostringstream output;
        output << e.what();
        ASSERT_EQUAL(output.str(), "invalid_argument"s);
    }
    
}

// ----14----
// Тест метода MatchDocument. Он должен выбрасывать исключение invalid_argument в следующих ситуациях:
// - В словах поискового запроса есть недопустимые символы с кодами от 0 до 31;
// - Наличие более чем одного минуса перед словами;
// - Отсутствие текста после символа «минус».
void TestMatchDocumentWithInvalidArgument() {
    // В словах поискового запроса есть недопустимые символы с кодами от 0 до 31
    try
    {
        SearchServer server(""s);
        server.AddDocument(0, "test"s, DocumentStatus::ACTUAL,{0});

        server.MatchDocument("te\x12st", 0);
        ASSERT_EQUAL_HINT(1, 2, "No exception");
    }
    catch(const invalid_argument& e)
    {
        ostringstream output;
        output << e.what();
        ASSERT_EQUAL(output.str(), "invalid_argument"s);
    }
    // Наличие более чем одного минуса перед словами
    try
    {
        SearchServer server(""s);
        server.AddDocument(0, "test"s, DocumentStatus::ACTUAL,{0});

        server.MatchDocument("test --test", 0);
        ASSERT_EQUAL_HINT(1, 2, "No exception");
    }
    catch(const invalid_argument& e)
    {
        ostringstream output;
        output << e.what();
        ASSERT_EQUAL(output.str(), "invalid_argument"s);
    }
    // Отсутствие текста после символа «минус»
    try
    {
        SearchServer server(""s);
        server.AddDocument(0, "test"s, DocumentStatus::ACTUAL,{0});

        server.MatchDocument("test - test", 0);
        ASSERT_EQUAL_HINT(1, 2, "No exception");
    }
    catch(const invalid_argument& e)
    {
        ostringstream output;
        output << e.what();
        ASSERT_EQUAL(output.str(), "invalid_argument"s);
    }
}

// Функция TestSearchServer является точкой входа для запуска тестов.
void TestSearchServer() {
    TestExcludeStopWordsFromAddedDocumentContent(); // 0
    TestAddDocuments(); // 1
    TestMinusWords(); // 3
    TestMatchDocument(); // 4
    TestRelevance(); // 5
    TestRatingCalculation(); // 6
    TestFilterByPredicate(); // 7
    TestFineDocumendByStatus(); // 8
    TestCalculateRelevance(); // 9
    TestGetDocumentId(); // 10
    TestSearchServerConstructor(); // 11
    TestAddDocumentWithInvalidArgument(); // 12
    TestFindTopDocumentsWithInvalidArgument(); // 13
    TestMatchDocumentWithInvalidArgument(); // 14
}

// --------- Окончание модульных тестов поисковой системы -----------  

void PrintMatchDocumentResult(int document_id, const vector<string>& words, DocumentStatus status) {
    cout << "{ "s
         << "document_id = "s << document_id << ", "s
         << "status = "s << static_cast<int>(status) << ", "s
         << "words ="s;
    for (const string& word : words) {
        cout << ' ' << word;
    }
    cout << "}"s << endl;
}

void AddDocument(SearchServer& search_server, int document_id, const string& document, DocumentStatus status,
                 const vector<int>& ratings) {
    try {
        search_server.AddDocument(document_id, document, status, ratings);
    } catch (const exception& e) {
        cout << "Ошибка добавления документа "s << document_id << ": "s << e.what() << endl;
    }
}

void FindTopDocuments(const SearchServer& search_server, const string& raw_query) {
    cout << "Результаты поиска по запросу: "s << raw_query << endl;
    try {
        for (const Document& document : search_server.FindTopDocuments(raw_query)) {
            PrintDocument(document);
        }
    } catch (const exception& e) {
        cout << "Ошибка поиска: "s << e.what() << endl;
    }
}

void MatchDocuments(const SearchServer& search_server, const string& query) {
    try {
        cout << "Матчинг документов по запросу: "s << query << endl;
        const int document_count = search_server.GetDocumentCount();
        for (int index = 0; index < document_count; ++index) {
            const int document_id = search_server.GetDocumentId(index);
            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    } catch (const exception& e) {
        cout << "Ошибка матчинга документов на запрос "s << query << ": "s << e.what() << endl;
    }
}

int main() {
    if (TEST_FLAG) {
        TestSearchServer();
        cout << "ALL TEST IS OK" << endl;
    }
    SearchServer search_server("и в на"s);

    AddDocument(search_server, 1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, {7, 2, 7});
    AddDocument(search_server, 1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, {1, 2});
    AddDocument(search_server, -1, "пушистый пёс и модный ошейник"s, DocumentStatus::ACTUAL, {1, 2});
    AddDocument(search_server, 3, "большой пёс скво\x12рец евгений"s, DocumentStatus::ACTUAL, {1, 3, 2});
    AddDocument(search_server, 4, "большой пёс скворец евгений"s, DocumentStatus::ACTUAL, {1, 1, 1});

    FindTopDocuments(search_server, "пушистый -пёс"s);
    FindTopDocuments(search_server, "пушистый --кот"s);
    FindTopDocuments(search_server, "пушистый -"s);

    MatchDocuments(search_server, "пушистый пёс"s);
    MatchDocuments(search_server, "модный -кот"s);
    MatchDocuments(search_server, "модный --пёс"s);
    MatchDocuments(search_server, "пушистый - хвост"s);
}