#include "index.h"
#include "server.h"
#include <chrono>
#include <iostream>
#include <string>

using namespace std;

void printStats(SearchIndex& index) {
    cout << "Folder: " << index.folderName() << endl;
    cout << "Files: " << index.filesCount() << endl;
    cout << "Words: " << index.wordsCount() << endl;
    cout << "Unique words: " << index.uniqueCount() << endl;
    cout << "Index time: " << index.buildTime() << " ms" << endl;
}

void runConsole(SearchIndex& index) {
    cout << endl;
    cout << "Type words to search, or \"some phrase\" in quotes for exact phrase" << endl;
    cout << "Commands: :stats, :exit" << endl;

    string line;
    while (true) {
        cout << endl << "> ";
        if (!getline(cin, line)) {
            break;
        }
        if (line.empty()) {
            continue;
        }
        if (line == ":exit") {
            break;
        }
        if (line == ":stats") {
            printStats(index);
            continue;
        }

        bool phrase = false;
        if (line.size() >= 2 && line.front() == '"' && line.back() == '"') {
            phrase = true;
            line = line.substr(1, line.size() - 2);
        }

        auto start = chrono::steady_clock::now();
        int found = 0;
        vector<Result> results = index.search(line, phrase, 10, found);
        auto end = chrono::steady_clock::now();
        double ms = chrono::duration<double, milli>(end - start).count();

        cout << "Found files: " << found << " (" << ms << " ms)" << endl;
        for (size_t i = 0; i < results.size(); i++) {
            cout << i + 1 << ". " << results[i].path << " - " << results[i].count << endl;
            cout << "   " << results[i].snippet << endl;
        }
    }
}

int main(int argc, char* argv[]) {
    string folder = "texts";
    bool web = false;

    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "--web") {
            web = true;
        } else {
            folder = arg;
        }
    }

    SearchIndex index;
    if (!index.build(folder)) {
        cout << "Cannot open folder: " << folder << endl;
        return 1;
    }
    printStats(index);

    if (web) {
        runServer(index, 8080);
    } else {
        runConsole(index);
    }
    return 0;
}
