#include "tokenizer.h"
#include <cctype>

using namespace std;

string toLower(const string& s) {
    string res = s;
    for (size_t i = 0; i < res.size(); i++) {
        unsigned char c = res[i];
        if (c < 128) {
            res[i] = tolower(c);
        } else if (c == 0xD0 && i + 1 < res.size()) {
            unsigned char next = res[i + 1];
            if (next >= 0x90 && next <= 0x9F) {
                res[i + 1] = next + 0x20;
            } else if (next >= 0xA0 && next <= 0xAF) {
                res[i] = 0xD1;
                res[i + 1] = next - 0x20;
            } else if (next == 0x81) {
                res[i] = 0xD1;
                res[i + 1] = 0x91;
            }
            i++;
        }
    }
    return res;
}

vector<Token> tokenize(const string& text) {
    vector<Token> tokens;
    string word;
    size_t start = 0;
    for (size_t i = 0; i < text.size(); i++) {
        unsigned char c = text[i];
        if (c < 128 && isalnum(c)) {
            if (word.empty()) {
                start = i;
            }
            word += text[i];
        } else if ((c == 0xD0 || c == 0xD1) && i + 1 < text.size()) {
            if (word.empty()) {
                start = i;
            }
            word += text[i];
            word += text[i + 1];
            i++;
        } else {
            if (!word.empty()) {
                tokens.push_back({word, start, word.size()});
                word = "";
            }
        }
    }
    if (!word.empty()) {
        tokens.push_back({word, start, word.size()});
    }
    return tokens;
}

vector<string> splitWords(const string& text) {
    vector<string> words;
    for (const Token& t : tokenize(text)) {
        words.push_back(t.word);
    }
    return words;
}
