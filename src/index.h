#pragma once
#include "tokenizer.h"
#include <string>
#include <vector>
#include <unordered_map>

struct Document {
    std::string path;
    std::string text;
    std::vector<Token> tokens;
};

struct SnippetPart {
    std::string text;
    bool mark;
};

struct Result {
    std::string path;
    int count;
    std::string snippet;
    std::vector<SnippetPart> parts;
};

class SearchIndex {
public:
    bool build(const std::string& folder);
    std::vector<Result> search(const std::string& query, bool phrase, int top, int& found);
    const Document* findDocument(const std::string& path);

    int filesCount();
    int wordsCount();
    int uniqueCount();
    double buildTime();
    std::string folderName();

private:
    std::vector<Result> searchWords(const std::vector<std::string>& q);
    std::vector<Result> searchPhrase(const std::vector<std::string>& q);
    void makeSnippet(Result& r, int docId, int pos, int len, const std::vector<std::string>& q, bool phrase);

    std::vector<Document> docs;
    std::unordered_map<std::string, std::unordered_map<int, std::vector<int>>> table;
    int totalWords = 0;
    double buildMs = 0;
    std::string folder;
};
