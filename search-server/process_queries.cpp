#include "process_queries.h"

#include <algorithm>
#include <execution>

vector<vector<Document>> ProcessQueries(const SearchServer& search_server, const vector<string>& queries) {
    vector<vector<Document>> result(queries.size());

    transform(execution::par, queries.begin(), queries.end(), result.begin(), [&search_server](const string& str){
        return search_server.FindTopDocuments(str);
    });

    return result;
}

vector<Document> ProcessQueriesJoined(const SearchServer& search_server, const vector<string>& queries){
    vector<vector<Document>> result_transform(queries.size());
    result_transform = move(ProcessQueries(search_server, queries));


    vector<Document> result;

    for (const auto& documents : result_transform) {
        for (const auto& doc : documents) {
            result.push_back(move(doc));
        }
    }

    return result;
}