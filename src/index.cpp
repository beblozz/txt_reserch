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
        doc.text = buffer.str();
        if (doc.text.size() >= 3 && doc.text.compare(0, 3, "\xEF\xBB\xBF") == 0) {
            doc.text.erase(0, 3);
        }
        doc.tokens = tokenize(doc.text);

        int docId = docs.size();
        for (int i = 0; i < (int)doc.tokens.size(); i++) {
            string word = toLower(doc.tokens[i].word);
            table[word][docId].push_back(i);
        }

        totalWords += doc.tokens.size();
        docs.push_back(move(doc));
    }

    auto end = chrono::steady_clock::now();
    buildMs = chrono::duration<double, milli>(end - start).count();
    return true;
}

void addPart(vector<SnippetPart>& parts, const string& text, bool mark) {
    if (text.empty()) {
        return;
    }
    if (!parts.empty() && parts.back().mark == mark) {
        parts.back().text += text;
    } else {
        parts.push_back({text, mark});
    }
}

string cleanGap(const string& gap) {
    string res;
    for (char c : gap) {
        if (c == '\n' || c == '\r' || c == '\t' || c == ' ') {
            if (res.empty() || res.back() != ' ') {
                res += ' ';
            }
        } else {
            res += c;
        }
    }
    return res;
}

void SearchIndex::makeSnippet(Result& r, int docId, int pos, int len, const vector<string>& q, bool phrase) {
    const Document& doc = docs[docId];
    const vector<Token>& tokens = doc.tokens;
    int from = max(0, pos - 6);
    int to = min((int)tokens.size(), pos + len + 8);

    vector<SnippetPart> parts;
    if (from > 0) {
        addPart(parts, "... ", false);
    }
    for (int i = from; i < to; i++) {
        bool mark;
        if (phrase) {
            mark = i >= pos && i < pos + len;
        } else {
            mark = find(q.begin(), q.end(), toLower(tokens[i].word)) != q.end();
        }
        addPart(parts, tokens[i].word, mark);

        if (i + 1 < to) {
            size_t gapStart = tokens[i].start + tokens[i].len;
            string gap = cleanGap(doc.text.substr(gapStart, tokens[i + 1].start - gapStart));
            addPart(parts, gap, phrase && mark && i + 1 < pos + len);
        }
    }
    if (to < (int)tokens.size()) {
        addPart(parts, " ...", false);
    }

    r.snippet = "";
    for (const SnippetPart& part : parts) {
        r.snippet += part.text;
    }
    r.parts = parts;
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
            int firstPos = item.second[0];
            for (size_t i = 1; i < q.size(); i++) {
                firstPos = min(firstPos, table[q[i]][docId][0]);
            }

            Result r;
            r.path = docs[docId].path;
            r.count = count;
            makeSnippet(r, docId, firstPos, 1, q, false);
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
            makeSnippet(r, docId, firstPos, q.size(), q, true);
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

const Document* SearchIndex::findDocument(const string& path) {
    for (const Document& doc : docs) {
        if (doc.path == path) {
            return &doc;
        }
    }
    return nullptr;
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
