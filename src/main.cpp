#include "index.h"
#include "server.h"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

using namespace std;
namespace fs = std::filesystem;

bool colors = false;

void setupConsole() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(out, &mode)) {
        colors = SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
#else
    colors = isatty(fileno(stdout));
#endif
}

// В консоли Windows русские буквы через std::getline приходят сломанными,
// поэтому там читаем строку в UTF-16 и сами переводим её в UTF-8.
bool readLine(string& line) {
#ifdef _WIN32
    HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(in, &mode)) {
        wstring wide;
        wchar_t buffer[1024];
        DWORD read = 0;
        while (true) {
            if (!ReadConsoleW(in, buffer, 1024, &read, nullptr) || read == 0) {
                return false;
            }
            wide.append(buffer, read);
            if (wide.back() == L'\n') {
                break;
            }
        }
        while (!wide.empty() && (wide.back() == L'\n' || wide.back() == L'\r')) {
            wide.pop_back();
        }
        int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), nullptr, 0, nullptr, nullptr);
        line.assign(size, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), (int)wide.size(), &line[0], size, nullptr, nullptr);
        return true;
    }
#endif
    if (!getline(cin, line)) {
        return false;
    }
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    return true;
}

void printStats(SearchIndex& index) {
    cout << "Folder: " << index.folderName() << endl;
    cout << "Files: " << index.filesCount() << endl;
    cout << "Words: " << index.wordsCount() << endl;
    cout << "Unique words: " << index.uniqueCount() << endl;
    cout << "Index time: " << index.buildTime() << " ms" << endl;
}

void printSnippet(const Result& r) {
    for (const SnippetPart& part : r.parts) {
        if (!part.mark) {
            cout << part.text;
        } else if (colors) {
            cout << "\x1b[30;43m" << part.text << "\x1b[0m";
        } else {
            cout << "[" << part.text << "]";
        }
    }
    cout << endl;
}

void runConsole(SearchIndex& index) {
    cout << endl;
    cout << "Type words to search, or \"some phrase\" in quotes for exact phrase" << endl;
    cout << "Commands: :stats, :top N, :index <folder>, :help, :exit" << endl;

    int top = 10;
    string line;
    while (true) {
        cout << endl << "> " << flush;
        if (!readLine(line)) {
            break;
        }
        if (line.empty()) {
            continue;
        }
        if (line == ":exit" || line == ":quit") {
            break;
        }
        if (line == ":help") {
            cout << "word1 word2      files that contain all the words" << endl;
            cout << "\"some phrase\"    files that contain the exact phrase" << endl;
            cout << ":stats           index statistics" << endl;
            cout << ":top N           show N best files (0 = all), now " << top << endl;
            cout << ":index <folder>  index another folder" << endl;
            cout << ":exit            quit" << endl;
            continue;
        }
        if (line == ":stats") {
            printStats(index);
            continue;
        }
        if (line.rfind(":top", 0) == 0) {
            int n = atoi(line.substr(4).c_str());
            if (n < 0) {
                n = 10;
            }
            top = n;
            cout << "Top: " << (top == 0 ? string("all") : to_string(top)) << endl;
            continue;
        }
        if (line.rfind(":index ", 0) == 0) {
            string folder = line.substr(7);
            SearchIndex fresh;
            if (!fresh.build(folder)) {
                cout << "Cannot open folder: " << folder << endl;
                continue;
            }
            index = move(fresh);
            printStats(index);
            continue;
        }
        if (line[0] == ':') {
            cout << "Unknown command, type :help" << endl;
            continue;
        }

        bool phrase = false;
        if (line.size() >= 2 && line.front() == '"' && line.back() == '"') {
            phrase = true;
            line = line.substr(1, line.size() - 2);
        }

        auto start = chrono::steady_clock::now();
        int found = 0;
        vector<Result> results = index.search(line, phrase, top, found);
        auto end = chrono::steady_clock::now();
        double ms = chrono::duration<double, milli>(end - start).count();

        cout << "Found files: " << found << " (" << ms << " ms)" << endl;
        for (size_t i = 0; i < results.size(); i++) {
            cout << i + 1 << ". " << results[i].path << " - " << results[i].count << endl;
            cout << "   ";
            printSnippet(results[i]);
        }
    }
}

// Ищем папку web: рядом с текущей папкой, рядом с программой или на уровень выше
// (если программа лежит в build/).
string findWebDir(const char* argv0) {
    vector<fs::path> candidates = {"web"};
    error_code ec;
    fs::path exeDir = fs::absolute(argv0, ec).parent_path();
    if (!ec) {
        candidates.push_back(exeDir / "web");
        candidates.push_back(exeDir.parent_path() / "web");
        candidates.push_back(exeDir.parent_path().parent_path() / "web");
    }
    for (const fs::path& p : candidates) {
        if (fs::exists(p / "index.html", ec)) {
            return p.string();
        }
    }
    return "web";
}

void printHelp() {
    cout << "Usage: txt_search [folder] [--web] [--port N]" << endl;
    cout << endl;
    cout << "  folder     folder with .txt files (default: texts)" << endl;
    cout << "  --web      start web interface instead of console" << endl;
    cout << "  --port N   port for web interface (default: 8080)" << endl;
    cout << "  --help     show this help" << endl;
}

int main(int argc, char* argv[]) {
    setupConsole();

    string folder = "texts";
    bool web = false;
    int port = 8080;

    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "--web") {
            web = true;
        } else if (arg == "--port" && i + 1 < argc) {
            port = atoi(argv[++i]);
            if (port <= 0 || port > 65535) {
                cout << "Wrong port: " << argv[i] << endl;
                return 1;
            }
        } else if (arg == "--help" || arg == "-h") {
            printHelp();
            return 0;
        } else if (arg.rfind("--", 0) == 0) {
            cout << "Unknown option: " << arg << endl;
            printHelp();
            return 1;
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
        if (!runServer(index, port, findWebDir(argv[0]))) {
            return 1;
        }
    } else {
        runConsole(index);
    }
    return 0;
}
