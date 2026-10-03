#include "server.h"
#include "../third_party/httplib.h"
#include <chrono>
#include <iostream>
#include <mutex>

using namespace std;

string escapeJson(const string& s) {
    string res;
    for (char c : s) {
        if (c == '"') {
            res += "\\\"";
        } else if (c == '\\') {
            res += "\\\\";
        } else if (c == '\n') {
            res += "\\n";
        } else if ((unsigned char)c < 32) {
            res += " ";
        } else {
            res += c;
        }
    }
    return res;
}

string statsJson(SearchIndex& index) {
    string json = "{";
    json += "\"folder\":\"" + escapeJson(index.folderName()) + "\",";
    json += "\"files\":" + to_string(index.filesCount()) + ",";
    json += "\"words\":" + to_string(index.wordsCount()) + ",";
    json += "\"unique\":" + to_string(index.uniqueCount()) + ",";
    json += "\"ms\":" + to_string(index.buildTime());
    json += "}";
    return json;
}

void runServer(SearchIndex& index, int port) {
    httplib::Server server;
    mutex mtx;

    server.set_mount_point("/", "./web");

    server.Get("/api/stats", [&](const httplib::Request&, httplib::Response& res) {
        lock_guard<mutex> lock(mtx);
        res.set_content(statsJson(index), "application/json; charset=utf-8");
    });

    server.Get("/api/search", [&](const httplib::Request& req, httplib::Response& res) {
        lock_guard<mutex> lock(mtx);

        string query = req.get_param_value("q");
        bool phrase = req.get_param_value("phrase") == "1";
        int top = 10;
        if (req.has_param("top")) {
            top = atoi(req.get_param_value("top").c_str());
        }

        auto start = chrono::steady_clock::now();
        int found = 0;
        vector<Result> results = index.search(query, phrase, top, found);
        auto end = chrono::steady_clock::now();
        double ms = chrono::duration<double, milli>(end - start).count();

        string json = "{";
        json += "\"found\":" + to_string(found) + ",";
        json += "\"ms\":" + to_string(ms) + ",";
        json += "\"results\":[";
        for (size_t i = 0; i < results.size(); i++) {
            if (i > 0) {
                json += ",";
            }
            json += "{";
            json += "\"path\":\"" + escapeJson(results[i].path) + "\",";
            json += "\"count\":" + to_string(results[i].count) + ",";
            json += "\"snippet\":\"" + escapeJson(results[i].snippet) + "\"";
            json += "}";
        }
        json += "]}";

        res.set_content(json, "application/json; charset=utf-8");
    });

    server.Post("/api/index", [&](const httplib::Request& req, httplib::Response& res) {
        lock_guard<mutex> lock(mtx);

        string path = req.get_param_value("path");
        SearchIndex fresh;
        if (path.empty() || !fresh.build(path)) {
            res.status = 400;
            res.set_content("{\"error\":\"folder not found\"}", "application/json; charset=utf-8");
            return;
        }
        index = fresh;
        res.set_content(statsJson(index), "application/json; charset=utf-8");
    });

    cout << "Server started: http://localhost:" << port << endl;
    cout << "Press Ctrl+C to stop" << endl;
    server.listen("localhost", port);
}
