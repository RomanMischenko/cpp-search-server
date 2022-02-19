#include "document.h"
#include <iostream>
#include <string>

using namespace std;

Document::Document() = default;

Document::Document(int new_id, double new_relevance, int new_rating) 
    : id(new_id), relevance(new_relevance), rating(new_rating)
    {
    }

ostream& operator<<(ostream& out, const Document doc) {
    out << "{ document_id = "s;
    out << doc.id;
    out << ", relevance = "s;
    out << doc.relevance;
    out << ", rating = "s;
    out << doc.rating;
    out << " }"s;
    return out;
}

void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating
         << " }"s << endl;
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
