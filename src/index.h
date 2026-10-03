#pragma once
#include <string>
#include <vector>
#include <unordered_map>

struct Document {
    std::string path;
    std::vector<std::string> words;
};

struct Result {
    std::string path;
    int count;
    std::string snippet;
};

class SearchIndex {
public:
    bool build(const std::string& folder);
    std::vector<Result> search(const std::string& query, bool phrase, int top, int& found);

    int filesCount();
    int wordsCount();
    int uniqueCount();
    double buildTime();
    std::string folderName();

private:
    std::vector<Result> searchWords(const std::vector<std::string>& q);
    std::vector<Result> searchPhrase(const std::vector<std::string>& q);
    std::string makeSnippet(int docId, int pos, int len);

    std::vector<Document> docs;
    std::unordered_map<std::string, std::unordered_map<int, std::vector<int>>> table;
    int totalWords = 0;
    double buildMs = 0;
    std::string folder;
};
