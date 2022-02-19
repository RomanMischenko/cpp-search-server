#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>

using namespace std;

struct Document {
    Document();

    Document(int new_id, double new_relevance, int new_rating);

    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

ostream& operator<<(ostream& out, const Document doc);

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

void PrintDocument(const Document& document);

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

ostream& operator<<(ostream& out, const DocumentStatus status);
