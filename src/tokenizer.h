#pragma once
#include <string>
#include <vector>

struct Token {
    std::string word;
    size_t start;
    size_t len;
};

std::string toLower(const std::string& s);
std::vector<Token> tokenize(const std::string& text);
std::vector<std::string> splitWords(const std::string& text);
