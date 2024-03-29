#pragma once

#include <sstream>
#include <string>
#include <iostream>

using namespace std;

const bool TEST_FLAG = true;

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



void ASSERTImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
                const string& hint);



// -------- Начало модульных тестов поисковой системы ----------

// ----0----
// Тест из примера.
// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов.
void TestExcludeStopWordsFromAddedDocumentContent();

// ----1----
// Добавление документов. 
// Добавленный документ должен находиться по поисковому запросу, который содержит слова из документа.
void TestAddDocuments();

// ----2----
// Поддержка стоп-слов. 
// Стоп-слова исключаются из текста документов.
// Ранее реализовано в тестах из примера.

// ----3----
// Поддержка минус-слов. 
// Документы, содержащие минус-слова поискового запроса, не должны включаться
// в результаты поиска.
void TestMinusWords();

// ----4----
// Матчинг документов. 
// При матчинге документа по поисковому запросу должны быть возвращены
// все слова из поискового запроса, присутствующие в документе. 
// Если есть соответствие хотя бы по одному минус-слову, 
// должен возвращаться пустой список слов.
void TestMatchDocument();

// ----5----
// Сортировка найденных документов по релевантности. 
// Возвращаемые при поиске документов результаты должны быть отсортированы 
// в порядке убывания релевантности.
void TestRelevance();

// ----6----
// Вычисление рейтинга документов. 
// Рейтинг добавленного документа равен среднему арифметическому оценок документа.
void TestRatingCalculation();

// ----7----
// Фильтрация результатов поиска с использованием предиката, 
// задаваемого пользователем.
void TestFilterByPredicate();

// ----8----
//Поиск документов, имеющих заданный статус.
void TestFineDocumendByStatus();

// ----9----
// Корректное вычисление релевантности найденных документов.
void TestCalculateRelevance();

// ----10----
// Должен получать идентификатор документа по его порядковому номеру;
// Должен выбрасывать исключение out_of_range, если индекс переданного 
// документа выходит за пределы допустимого диапазона (0; количество документов).
void TestGetDocumentId();

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
void TestSearchServerConstructor();

// ----12----
// Тест метода AddDocument. Он должен выбрасывать исключение invalid_argument в следующих ситуациях:
// - Попытка добавить документ с отрицательным id;
// - Попытка добавить документ c id ранее добавленного документа;
// - Наличие недопустимых символов (с кодами от 0 до 31) в тексте добавляемого документа.
void TestAddDocumentWithInvalidArgument();

// ----13----
// Тест метода FindTopDocuments. Он должен выбрасывать исключение invalid_argument в следующих ситуациях:
// - В словах поискового запроса есть недопустимые символы с кодами от 0 до 31;
// - Наличие более чем одного минуса перед словами;
// - Отсутствие текста после символа «минус».
void TestFindTopDocumentsWithInvalidArgument();

// ----14----
// Тест метода MatchDocument. Он должен выбрасывать исключение invalid_argument в следующих ситуациях:
// - В словах поискового запроса есть недопустимые символы с кодами от 0 до 31;
// - Наличие более чем одного минуса перед словами;
// - Отсутствие текста после символа «минус».
void TestMatchDocumentWithInvalidArgument();

// ----15----
// Тест методов begin и end.
// Итераторы дают доступ к id всех документов, хранящихся в поисковом сервере
void TestIteratorBeginAndEnd();

// ----16----
// Тест метода GetWordFrequencies.
// Метод должен возвращать ссылку на словать типа const map<string, double>&
// Если документа не существует, возвращать ссылку на пустой словать
void TestGetWordFrequencies();

// ----17----
// Тест метода RemoveDocument.
// Метод должен удалять документ по его id
void TestRemoveDocument();

// ----18----
// Тест функции RemoveDuplicates.
// При обнаружении дублирующихся документов функция должна удалить документ с большим id из поискового сервера
// Функция RemoveDuplicates должна для каждого удаляемого документа 
// вывести в cout сообщение в формате Found duplicate document id N, 
// где вместо N следует подставить id удаляемого документа
void TestRemoveDuplicates();

// ----19----
// Тест функции ProcessQueries.
// Она принимает N запросов и возвращает вектор длины N, 
// i-й элемент которого — результат вызова FindTopDocuments для i-го запроса.
void TestProcessQueries();

// ----19----
// Тест функции ProcessQueriesJoined.
// Функция должна вернуть объект documents. 
// Для него можно написать for (const Document& document : documents) 
// и получить сначала все документы из результата вызова 
// FindTopDocuments для первого запроса, затем для второго и так далее.
void TestProcessQueriesJoined();



// Функция TestSearchServer является точкой входа для запуска тестов.
void TestSearchServer();

// --------- Окончание модульных тестов поисковой системы -----------   
