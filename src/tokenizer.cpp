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

vector<string> splitWords(const string& text) {
    vector<string> words;
    string word;
    for (size_t i = 0; i < text.size(); i++) {
        unsigned char c = text[i];
        if (c < 128 && isalnum(c)) {
            word += text[i];
        } else if ((c == 0xD0 || c == 0xD1) && i + 1 < text.size()) {
            word += text[i];
            word += text[i + 1];
            i++;
        } else {
            if (!word.empty()) {
                words.push_back(word);
                word = "";
            }
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }
    return words;
}
