#include "remove_duplicates.h"
#include <map>
#include <set>
#include <iostream>
 
using namespace std;
 
void RemoveDuplicates(SearchServer& search_server) {
    // множество с id документов под удаление
    set<int> id_documents_to_remove;
    // словарь со множеством слов и id их документа
    map<set<string_view>, int> words_to_document;
    for (const int document_id : search_server) {
        // нахождение уникальных слов для документа с document_id
        set<string_view> document_words;
        const map<string_view, double> word_freqs = search_server.GetWordFrequencies(document_id);
        for (auto [word, freqs] : word_freqs) {
            document_words.insert(word);
        }

        if (words_to_document.count(document_words)) {
            int prev_doc_id = words_to_document[document_words];
            if (prev_doc_id > document_id) {
                words_to_document[document_words] = document_id;
                id_documents_to_remove.insert(prev_doc_id);
            } else {
                id_documents_to_remove.insert(document_id);
            }
        } else {
            words_to_document[document_words] = document_id;
        }
    }

    // удаляем дубликаты методом RemoveDocument
    for ( int document_id : id_documents_to_remove ) {
        cout << "Found duplicate document id " << document_id << endl;
        search_server.RemoveDocument(document_id);
    }
}