#include "../src/index.h"
#include "../src/tokenizer.h"
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace std;
namespace fs = std::filesystem;

int failed = 0;
int passed = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (cond) {                                                            \
            passed++;                                                          \
        } else {                                                               \
            failed++;                                                          \
            cout << "FAILED: " << #cond << " (line " << __LINE__ << ")" << endl; \
        }                                                                      \
    } while (0)

void writeFile(const fs::path& path, const string& text) {
    fs::create_directories(path.parent_path());
    ofstream file(path, ios::binary);
    file << text;
}

void testTokenizer() {
    vector<string> words = splitWords("Hello, world! Привет, МИР 2024");
    CHECK(words.size() == 5);
    CHECK(words[0] == "Hello");
    CHECK(words[3] == "МИР");
    CHECK(words[4] == "2024");

    CHECK(toLower("ПРИВЕТ Мир ЁЖ ABC") == "привет мир ёж abc");
    CHECK(splitWords("").empty());
    CHECK(splitWords("... --- !!!").empty());

    vector<Token> tokens = tokenize("ab  вг");
    CHECK(tokens.size() == 2);
    CHECK(tokens[1].start == 4);
    CHECK(tokens[1].len == 4);
}

void testSearch(const fs::path& dir) {
    writeFile(dir / "a.txt", "Ракета-носитель вывела спутник. Ракета летит, ракета гудит.");
    writeFile(dir / "b.txt", "\xEF\xBB\xBF" "Спутник на орбите. Ракета носитель не нужна.");
    writeFile(dir / "sub/c.txt", "Совсем другой текст про кошек.");
    writeFile(dir / "skip.md", "ракета ракета ракета ракета");

    SearchIndex index;
    CHECK(index.build(dir.string()));
    CHECK(index.filesCount() == 3);
    SearchIndex empty;
    CHECK(!empty.build((dir / "missing").string()));

    int found = 0;

    vector<Result> res = index.search("РАКЕТА", false, 10, found);
    CHECK(found == 2);
    CHECK(res.size() == 2);
    CHECK(res[0].path == "a.txt");
    CHECK(res[0].count == 3);

    res = index.search("ракета спутник", false, 10, found);
    CHECK(found == 2);

    res = index.search("ракета кошек", false, 10, found);
    CHECK(found == 0);

    res = index.search("неизвестное", false, 10, found);
    CHECK(found == 0);
    CHECK(res.empty());

    res = index.search("ракета носитель", true, 10, found);
    CHECK(found == 2);

    res = index.search("носитель ракета", true, 10, found);
    CHECK(found == 0);

    res = index.search("ракета", false, 1, found);
    CHECK(found == 2);
    CHECK(res.size() == 1);

    res = index.search("кошек", false, 10, found);
    CHECK(res.size() == 1);
    CHECK(res[0].path == "sub/c.txt");
    CHECK(res[0].snippet == "Совсем другой текст про кошек");

    res = index.search("спутник на орбите", true, 10, found);
    CHECK(res.size() == 1);
    bool marked = false;
    for (const SnippetPart& part : res[0].parts) {
        if (part.mark) {
            CHECK(part.text == "Спутник на орбите");
            marked = true;
        }
    }
    CHECK(marked);

    res = index.search("   ", false, 10, found);
    CHECK(found == 0);

    CHECK(index.findDocument("b.txt") != nullptr);
    CHECK(index.findDocument("b.txt")->text.rfind("Спутник", 0) == 0);
    CHECK(index.findDocument("../secret.txt") == nullptr);
}

int main() {
    fs::path dir = fs::temp_directory_path() / "txt_search_tests";
    fs::remove_all(dir);

    testTokenizer();
    testSearch(dir);

    fs::remove_all(dir);

    cout << "Passed: " << passed << ", failed: " << failed << endl;
    return failed == 0 ? 0 : 1;
}
