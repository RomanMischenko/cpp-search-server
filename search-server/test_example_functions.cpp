#include "test_example_functions.h"
#include "search_server.h"
#include "remove_duplicates.h"
#include "process_queries.h"

#include <execution>

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

#define ASSERT_EQUAL(a, b) ASSERTEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) ASSERTEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

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
    // execution::seq
    {
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

        const auto [words_1, status_1] = server.MatchDocument(execution::seq, "test test_1 test_6"s, document_id_1);
        ASSERT_EQUAL(words_1.size(), 2U);
        ASSERT_EQUAL(words_1[0], "test"s);
        ASSERT_EQUAL(words_1[1], "test_1"s);

        const auto [words_2, status_2] = server.MatchDocument(execution::seq, "test_3 test"s, document_id_2);
        ASSERT_EQUAL(words_2.size(), 2U);
        ASSERT_EQUAL(words_2[0], "test"s);
        ASSERT_EQUAL(words_2[1], "test_3"s);

        const auto [words_3, status_3] = server.MatchDocument(execution::seq, "test -test_1"s, document_id_1);
        ASSERT(words_3.empty());

        const auto [words_4, status_4] = server.MatchDocument(execution::seq, "-test"s, document_id_2);
        ASSERT(words_4.empty());

        const auto [words_5, status_5] = server.MatchDocument(execution::seq, "-test test"s, document_id_2);
        ASSERT(words_5.empty());

        const auto [words_6, status_6] = server.MatchDocument(execution::seq, "test -test_1"s, document_id_2);
        ASSERT_EQUAL(words_6.size(), 1U);
        ASSERT_EQUAL(words_6[0], "test"s);    
    }

    // execution::par
    {
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

        const auto [words_1, status_1] = server.MatchDocument(execution::par, "test test_1 test_6 test4 test5 test6"s, document_id_1);
        ASSERT_EQUAL(words_1.size(), 2U);
        ASSERT_EQUAL(words_1[0], "test"s);
        ASSERT_EQUAL(words_1[1], "test_1"s);

        const auto [words_2, status_2] = server.MatchDocument(execution::par, "test_3 test"s, document_id_2);
        ASSERT_EQUAL(words_2.size(), 2U);
        ASSERT_EQUAL(words_2[0], "test"s);
        ASSERT_EQUAL(words_2[1], "test_3"s);

        const auto [words_3, status_3] = server.MatchDocument(execution::par, "test -test_1"s, document_id_1);
        ASSERT(words_3.empty());

        const auto [words_4, status_4] = server.MatchDocument(execution::par, "-test"s, document_id_2);
        ASSERT(words_4.empty());

        const auto [words_5, status_5] = server.MatchDocument(execution::par, "-test test"s, document_id_2);
        ASSERT(words_5.empty());

        const auto [words_6, status_6] = server.MatchDocument(execution::par, "test -test_1"s, document_id_2);
        ASSERT_EQUAL(words_6.size(), 1U);
        ASSERT_EQUAL(words_6[0], "test"s);
    }
    
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
        // Проверка если релевантность одинаковая, то сортируем по рейтингу
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
    // seq
    {
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
    // par
    {
        SearchServer search_server("и в на"s);

        (void) search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
        (void) search_server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
        (void) search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
        (void) search_server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});

        // проверка четности всех id
        const auto find_document_1 = search_server.FindTopDocuments(execution::par, "пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; });
        ASSERT_EQUAL(find_document_1.size(), 2U);
        ASSERT_EQUAL(find_document_1[0].id, 0);
        ASSERT_EQUAL(find_document_1[1].id, 2);
        for (const auto& doc : find_document_1) {
            ASSERT_EQUAL(doc.id % 2, 0);
        }

        // проверка отсутствия документа с DocumentStatus::BANNED
        const auto find_document_2 = search_server.FindTopDocuments(execution::par, "пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return status == DocumentStatus::ACTUAL; });
        ASSERT_EQUAL(find_document_2.size(), 3U);
        ASSERT_EQUAL(find_document_2[0].id, 1);
        ASSERT_EQUAL(find_document_2[1].id, 0);
        ASSERT_EQUAL(find_document_2[2].id, 2);
        for (const auto& doc : find_document_2) {
            ASSERT(doc.id != 3);
        }

        // проверка, что рейтинг документов > 0
        const auto find_document_3 = search_server.FindTopDocuments(execution::par, "пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return rating > 0; });
        ASSERT_EQUAL(find_document_3.size(), 3U);
        ASSERT_EQUAL(find_document_3[0].id, 1);
        ASSERT_EQUAL(find_document_3[1].id, 3);
        ASSERT_EQUAL(find_document_3[2].id, 0);
        for (const auto& doc : find_document_3) {
            ASSERT(doc.rating > 0);
        }


        // проверка на возврат всех добавленных документов
        const auto find_document_4 = search_server.FindTopDocuments(execution::par, "пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return 1; });
        ASSERT_EQUAL(find_document_4.size(), 4U);
        ASSERT_EQUAL(find_document_4[0].id, 1);
        ASSERT_EQUAL(find_document_4[1].id, 3);
        ASSERT_EQUAL(find_document_4[2].id, 0);
        ASSERT_EQUAL(find_document_4[3].id, 2);
    }
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

        try
        {
            SearchServer server(""s);
            server.AddDocument(0, "test"s, DocumentStatus::ACTUAL, {0});
            server.GetDocumentId(-1);
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

// ----15----
// Тест методов begin и end.
// Итераторы дают доступ к id всех документов, хранящихся в поисковом сервере
void TestIteratorBeginAndEnd() {
    SearchServer server(""s);
    server.AddDocument(0, "test"s, DocumentStatus::ACTUAL, {0});
    server.AddDocument(1, "test_2"s, DocumentStatus::ACTUAL, {0});
    server.AddDocument(2, "test_3"s, DocumentStatus::ACTUAL, {0});

    // проверка интератора begin
    auto iter_begin = server.begin();
    ASSERT_EQUAL(*iter_begin, 0);
    
    advance(iter_begin, 1);
    ASSERT_EQUAL(*iter_begin, 1);

    advance(iter_begin, 1);
    ASSERT_EQUAL(*iter_begin, 2);

    // проверка итератора end
    auto iter_end = server.end();
    advance(iter_end, -1);
    ASSERT_EQUAL(*iter_end, 2);

    // проверка возможности использовавания цикла range-based-for
    ostringstream out;
    for (const int id : server) {
        out << id;
    }
    ASSERT_EQUAL(out.str(), "012"s);
}

// ----16----
// Тест метода GetWordFrequencies.
// Метод должен возвращать ссылку на словать типа const map<string, double>&
// Если документа не существует, возвращать ссылку на пустой словать
void TestGetWordFrequencies() {
    SearchServer server(""s);
    server.AddDocument(0, "test test test_1"s, DocumentStatus::ACTUAL, {0});
    server.AddDocument(1, "test_2"s, DocumentStatus::ACTUAL, {0});
    server.AddDocument(2, "test_3 test"s, DocumentStatus::ACTUAL, {0});

    // проверка на возврат пустого словаря
    ASSERT_EQUAL(static_cast<int>(server.GetWordFrequencies(3).size()), 0);

    // проверка на возврат правильного словаря со значениями
    const map<string_view, double>& map_for_id_0 = server.GetWordFrequencies(0);
    ASSERT_EQUAL(static_cast<int>(map_for_id_0.size()), 2);
    ASSERT_EQUAL(map_for_id_0.at("test"s), 2.0/3.0);
    ASSERT_EQUAL(map_for_id_0.at("test_1"s), 1.0/3.0);

    const map<string_view, double>& map_for_id_1 = server.GetWordFrequencies(1);
    ASSERT_EQUAL(static_cast<int>(map_for_id_1.size()), 1);
    ASSERT_EQUAL(map_for_id_1.at("test_2"s), 1.0);

    const map<string_view, double>& map_for_id_2 = server.GetWordFrequencies(2);
    ASSERT_EQUAL(static_cast<int>(map_for_id_2.size()), 2);
    ASSERT_EQUAL(map_for_id_2.at("test"s), 1.0/2.0);
    ASSERT_EQUAL(map_for_id_2.at("test_3"s), 1.0/2.0);
}

// ----17----
// Тест метода RemoveDocument.
// Метод должен удалять документ по его id
void TestRemoveDocument() {
    // проверка удаления из word_to_document_freqs_ и из из documents_
    {
        SearchServer server(""s);
        server.AddDocument(0, "test test test_1"s, DocumentStatus::ACTUAL, {0});
        server.AddDocument(1, "test_2"s, DocumentStatus::ACTUAL, {0});
        server.AddDocument(2, "test_3 test"s, DocumentStatus::ACTUAL, {0});

        ASSERT_EQUAL(server.GetDocumentCount(), 3);
        server.RemoveDocument(0);
        ASSERT_EQUAL(server.GetDocumentCount(), 2);
  
        auto documents = server.FindTopDocuments("test"s);
        vector<int> documents_id;
        for (const auto& document : documents) {
            documents_id.push_back(document.id);
        }
        const auto iter_1 = find(documents_id.begin(), documents_id.end(), 0);
        const auto iter_2 = documents_id.end();
        ASSERT_EQUAL(*iter_1, *iter_2);
    }
    
    // проверка удаления из word_frequencies_
    {
        SearchServer server(""s);
        server.AddDocument(0, "test test test_1"s, DocumentStatus::ACTUAL, {0});
        server.AddDocument(1, "test_2"s, DocumentStatus::ACTUAL, {0});
        server.AddDocument(2, "test_3 test"s, DocumentStatus::ACTUAL, {0});

        auto M_1 = server.GetWordFrequencies(0);
        ASSERT_EQUAL(static_cast<int>(M_1.size()), 2);
        server.RemoveDocument(0);
        M_1 = server.GetWordFrequencies(0);
        ASSERT_EQUAL(M_1.empty(), true);
    }

    // проверка удаления из sequence_of_adding_id_
    {
        SearchServer server(""s);
        server.AddDocument(2, "test test test_1"s, DocumentStatus::ACTUAL, {0});
        server.AddDocument(0, "test_2"s, DocumentStatus::ACTUAL, {0});
        server.AddDocument(1, "test_3 test"s, DocumentStatus::ACTUAL, {0});

        ASSERT_EQUAL(server.GetDocumentId(1), 0);
        server.RemoveDocument(0);
        ASSERT_EQUAL(server.GetDocumentId(1), 1);
    }

    // проверка удаления однопоточным и многопоточным способом
    {
        SearchServer server(""s);
        server.AddDocument(0, "test test_1 test_2"s, DocumentStatus::ACTUAL, {0});
        server.AddDocument(1, "test test_3 test_4"s, DocumentStatus::ACTUAL, {0});
        server.AddDocument(2, "test test_5 test_6"s, DocumentStatus::ACTUAL, {0});

        auto documents = server.FindTopDocuments("test");
        ASSERT_EQUAL(static_cast<int>(documents.size()), 3);

        server.RemoveDocument(execution::seq, 2);
        documents.clear();
        documents = server.FindTopDocuments("test");
        ASSERT_EQUAL(static_cast<int>(documents.size()), 2);

        server.RemoveDocument(execution::par, 1);
        documents.clear();
        documents = server.FindTopDocuments("test");
        ASSERT_EQUAL(static_cast<int>(documents.size()), 1);
    }
}

// ----18----
// Тест функции RemoveDuplicates.
// При обнаружении дублирующихся документов функция должна удалить документ с большим id из поискового сервера
// Функция RemoveDuplicates должна для каждого удаляемого документа 
// вывести в cout сообщение в формате Found duplicate document id N, 
// где вместо N следует подставить id удаляемого документа
void TestRemoveDuplicates() {
    SearchServer search_server("and with"s);

    search_server.AddDocument(1, "funny pet and nasty rat"s, DocumentStatus::ACTUAL, {7, 2, 7});

    search_server.AddDocument(2, "funny pet with curly hair"s, DocumentStatus::ACTUAL, {1, 2});

    // дубликат документа 2, будет удалён
    search_server.AddDocument(3, "funny pet with curly hair"s, DocumentStatus::ACTUAL, {1, 2});

    // отличие только в стоп-словах, считаем дубликатом
    search_server.AddDocument(4, "funny pet and curly hair"s, DocumentStatus::ACTUAL, {1, 2});

    // множество слов такое же, считаем дубликатом документа 1
    search_server.AddDocument(5, "funny funny pet and nasty nasty rat"s, DocumentStatus::ACTUAL, {1, 2});

    // добавились новые слова, дубликатом не является
    search_server.AddDocument(6, "funny pet and not very nasty rat"s, DocumentStatus::ACTUAL, {1, 2});

    // множество слов такое же, как в id 6, несмотря на другой порядок, считаем дубликатом
    search_server.AddDocument(7, "very nasty rat and not very funny pet"s, DocumentStatus::ACTUAL, {1, 2});

    // есть не все слова, не является дубликатом
    search_server.AddDocument(8, "pet with rat and rat and rat"s, DocumentStatus::ACTUAL, {1, 2});

    // слова из разных документов, не является дубликатом
    search_server.AddDocument(9, "nasty rat with curly hair"s, DocumentStatus::ACTUAL, {1, 2});
    
    ASSERT_EQUAL(search_server.GetDocumentCount(), 9);
    
    RemoveDuplicates(search_server);

    ASSERT_EQUAL(search_server.GetDocumentCount(), 5);

}

// ----19----
// Тест функции ProcessQueries.
// Она принимает N запросов и возвращает вектор длины N, 
// i-й элемент которого — результат вызова FindTopDocuments для i-го запроса.
void TestProcessQueries() {
    SearchServer search_server("and with"s);

    int id = 0;
    for (
        const string& text : {
            "funny pet and nasty rat"s,
            "funny pet with curly hair"s,
            "funny pet and not very nasty rat"s,
            "pet with rat and rat and rat"s,
            "nasty rat with curly hair"s,
        }
    ) {
        search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
    }

    const vector<string> queries = {
        "nasty rat -not"s,
        "not very funny nasty pet"s,
        "curly hair"s
    };

    const auto documents = ProcessQueries(search_server, queries);

    ostringstream out;
    out << documents[0];
    
    ASSERT_EQUAL(out.str(), 
        "[{ document_id = 1, relevance = 0.183492, rating = 1 },"s
        " { document_id = 5, relevance = 0.183492, rating = 1 },"s
        " { document_id = 4, relevance = 0.167358, rating = 1 }]"s);
}

// ----19----
// Тест функции ProcessQueriesJoined.
// Функция должна вернуть объект documents. 
// Для него можно написать for (const Document& document : documents) 
// и получить сначала все документы из результата вызова 
// FindTopDocuments для первого запроса, затем для второго и так далее.
void TestProcessQueriesJoined() {
    SearchServer search_server("and with"s);

    int id = 0;
    for (
        const string& text : {
            "funny pet and nasty rat"s,
            "funny pet with curly hair"s,
            "funny pet and not very nasty rat"s,
            "pet with rat and rat and rat"s,
            "nasty rat with curly hair"s,
        }
    ) {
        search_server.AddDocument(++id, text, DocumentStatus::ACTUAL, {1, 2});
    }

    const vector<string> queries = {
        "nasty rat -not"s,
        "not very funny nasty pet"s,
        "curly hair"s
    };

    const auto documents = ProcessQueriesJoined(search_server, queries);

    ostringstream out;
    out << documents[0] << documents[1] << documents[2];
    
    ASSERT_EQUAL(out.str(), 
        "{ document_id = 1, relevance = 0.183492, rating = 1 }"s
        "{ document_id = 5, relevance = 0.183492, rating = 1 }"s
        "{ document_id = 4, relevance = 0.167358, rating = 1 }"s);
}

// Функция TestSearchServer является точкой входа для запуска тестов.
void TestSearchServer() {
    cerr << "TestExcludeStopWordsFromAddedDocumentContent begin...";
    TestExcludeStopWordsFromAddedDocumentContent(); // 0
    cerr << "ALL OK" << endl;

    cerr << "TestAddDocuments begin...";
    TestAddDocuments(); // 1
    cerr << "ALL OK" << endl;

    cerr << "TestMinusWords begin...";
    TestMinusWords(); // 3
    cerr << "ALL OK" << endl;

    cerr << "TestMatchDocument begin...";
    TestMatchDocument(); // 4
    cerr << "ALL OK" << endl;

    cerr << "TestRelevance begin...";
    TestRelevance(); // 5
    cerr << "ALL OK" << endl;

    cerr << "TestRatingCalculation begin...";
    TestRatingCalculation(); // 6
    cerr << "ALL OK" << endl;

    cerr << "TestFilterByPredicate begin...";
    TestFilterByPredicate(); // 7
    cerr << "ALL OK" << endl;

    cerr << "TestFineDocumendByStatus begin...";
    TestFineDocumendByStatus(); // 8
    cerr << "ALL OK" << endl;

    cerr << "TestCalculateRelevance begin...";
    TestCalculateRelevance(); // 9
    cerr << "ALL OK" << endl;

    cerr << "TestGetDocumentId begin...";
    TestGetDocumentId(); // 10
    cerr << "ALL OK" << endl;

    cerr << "TestSearchServerConstructor begin...";
    TestSearchServerConstructor(); // 11
    cerr << "ALL OK" << endl;

    cerr << "TestAddDocumentWithInvalidArgument begin...";
    TestAddDocumentWithInvalidArgument(); // 12
    cerr << "ALL OK" << endl;

    cerr << "TestFindTopDocumentsWithInvalidArgument begin...";
    TestFindTopDocumentsWithInvalidArgument(); // 13
    cerr << "ALL OK" << endl;

    cerr << "TestMatchDocumentWithInvalidArgument begin...";
    TestMatchDocumentWithInvalidArgument(); // 14
    cerr << "ALL OK" << endl;
    
    cerr << "TestIteratorBeginAndEnd begin...";
    TestIteratorBeginAndEnd(); // 15
    cerr << "ALL OK" << endl;

    cerr << "TestGetWordFrequencies begin...";
    TestGetWordFrequencies(); // 16
    cerr << "ALL OK" << endl;
    
    cerr << "TestRemoveDocument begin...";
    TestRemoveDocument(); // 17
    cerr << "ALL OK" << endl;

    cerr << "TestRemoveDuplicates begin..." << endl;
    TestRemoveDuplicates(); // 18
    cerr << "ALL OK" << endl;
    
    cerr << "TestProcessQueries begin...";
    TestProcessQueries(); // 19
    cerr << "ALL OK" << endl;

    cerr << "TestProcessQueriesJoined begin...";
    TestProcessQueriesJoined(); // 20
    cerr << "ALL OK" << endl;
}

// --------- Окончание модульных тестов поисковой системы ----------- 
