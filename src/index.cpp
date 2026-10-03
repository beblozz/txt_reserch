#include "index.h"
#include "tokenizer.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace std;
namespace fs = std::filesystem;

bool SearchIndex::build(const string& folderPath) {
    if (!fs::exists(folderPath) || !fs::is_directory(folderPath)) {
        return false;
    }

    auto start = chrono::steady_clock::now();

    docs.clear();
    table.clear();
    totalWords = 0;
    folder = folderPath;

    for (const auto& entry : fs::recursive_directory_iterator(folderPath)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (entry.path().extension() != ".txt") {
            continue;
        }

        ifstream file(entry.path(), ios::binary);
        if (!file.is_open()) {
            continue;
        }
        stringstream buffer;
        buffer << file.rdbuf();

        Document doc;
        doc.path = fs::relative(entry.path(), folderPath).generic_string();
        doc.words = splitWords(buffer.str());

        int docId = docs.size();
        for (int i = 0; i < (int)doc.words.size(); i++) {
            string word = toLower(doc.words[i]);
            table[word][docId].push_back(i);
        }

        totalWords += doc.words.size();
        docs.push_back(doc);
    }

    auto end = chrono::steady_clock::now();
    buildMs = chrono::duration<double, milli>(end - start).count();
    return true;
}

string SearchIndex::makeSnippet(int docId, int pos, int len) {
    const vector<string>& words = docs[docId].words;
    int from = max(0, pos - 6);
    int to = min((int)words.size(), pos + len + 8);

    string res;
    if (from > 0) {
        res += "... ";
    }
    for (int i = from; i < to; i++) {
        res += words[i];
        if (i + 1 < to) {
            res += " ";
        }
    }
    if (to < (int)words.size()) {
        res += " ...";
    }
    return res;
}

vector<Result> SearchIndex::searchWords(const vector<string>& q) {
    vector<Result> results;

    for (const string& word : q) {
        if (table.find(word) == table.end()) {
            return results;
        }
    }

    for (const auto& item : table[q[0]]) {
        int docId = item.first;
        int count = item.second.size();
        bool ok = true;

        for (size_t i = 1; i < q.size(); i++) {
            auto it = table[q[i]].find(docId);
            if (it == table[q[i]].end()) {
                ok = false;
                break;
            }
            count += it->second.size();
        }

        if (ok) {
            Result r;
            r.path = docs[docId].path;
            r.count = count;
            r.snippet = makeSnippet(docId, item.second[0], 1);
            results.push_back(r);
        }
    }
    return results;
}

vector<Result> SearchIndex::searchPhrase(const vector<string>& q) {
    vector<Result> results;

    for (const string& word : q) {
        if (table.find(word) == table.end()) {
            return results;
        }
    }

    for (const auto& item : table[q[0]]) {
        int docId = item.first;
        int count = 0;
        int firstPos = -1;

        for (int pos : item.second) {
            bool ok = true;
            for (size_t i = 1; i < q.size(); i++) {
                auto it = table[q[i]].find(docId);
                if (it == table[q[i]].end()) {
                    ok = false;
                    break;
                }
                if (!binary_search(it->second.begin(), it->second.end(), pos + (int)i)) {
                    ok = false;
                    break;
                }
            }
            if (ok) {
                count++;
                if (firstPos == -1) {
                    firstPos = pos;
                }
            }
        }

        if (count > 0) {
            Result r;
            r.path = docs[docId].path;
            r.count = count;
            r.snippet = makeSnippet(docId, firstPos, q.size());
            results.push_back(r);
        }
    }
    return results;
}

bool compareResults(const Result& a, const Result& b) {
    if (a.count != b.count) {
        return a.count > b.count;
    }
    return a.path < b.path;
}

vector<Result> SearchIndex::search(const string& query, bool phrase, int top, int& found) {
    vector<string> q = splitWords(toLower(query));
    found = 0;
    if (q.empty()) {
        return {};
    }

    vector<Result> results;
    if (phrase) {
        results = searchPhrase(q);
    } else {
        sort(q.begin(), q.end());
        q.erase(unique(q.begin(), q.end()), q.end());
        results = searchWords(q);
    }

    found = results.size();
    if (top > 0 && (int)results.size() > top) {
        partial_sort(results.begin(), results.begin() + top, results.end(), compareResults);
        results.resize(top);
    } else {
        sort(results.begin(), results.end(), compareResults);
    }
    return results;
}

int SearchIndex::filesCount() {
    return docs.size();
}

int SearchIndex::wordsCount() {
    return totalWords;
}

int SearchIndex::uniqueCount() {
    return table.size();
}

double SearchIndex::buildTime() {
    return buildMs;
}

string SearchIndex::folderName() {
    return folder;
}
