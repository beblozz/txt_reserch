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
        } else if (c == '\r') {
            continue;
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

string resultJson(const Result& r) {
    string json = "{";
    json += "\"path\":\"" + escapeJson(r.path) + "\",";
    json += "\"count\":" + to_string(r.count) + ",";
    json += "\"snippet\":\"" + escapeJson(r.snippet) + "\",";
    json += "\"parts\":[";
    for (size_t i = 0; i < r.parts.size(); i++) {
        if (i > 0) {
            json += ",";
        }
        json += "{\"text\":\"" + escapeJson(r.parts[i].text) + "\",";
        json += string("\"mark\":") + (r.parts[i].mark ? "true" : "false") + "}";
    }
    json += "]}";
    return json;
}

void sendError(httplib::Response& res, int status, const string& message) {
    res.status = status;
    res.set_content("{\"error\":\"" + escapeJson(message) + "\"}", "application/json; charset=utf-8");
}

bool runServer(SearchIndex& index, int port, const string& webDir) {
    httplib::Server server;
    mutex mtx;

    if (!server.set_mount_point("/", webDir)) {
        cout << "Cannot find web folder: " << webDir << endl;
        return false;
    }

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
            if (top < 0) {
                top = 10;
            }
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
            json += resultJson(results[i]);
        }
        json += "]}";

        res.set_content(json, "application/json; charset=utf-8");
    });

    server.Get("/api/file", [&](const httplib::Request& req, httplib::Response& res) {
        lock_guard<mutex> lock(mtx);

        const Document* doc = index.findDocument(req.get_param_value("path"));
        if (doc == nullptr) {
            sendError(res, 404, "file not found");
            return;
        }
        string json = "{";
        json += "\"path\":\"" + escapeJson(doc->path) + "\",";
        json += "\"words\":" + to_string(doc->tokens.size()) + ",";
        json += "\"text\":\"" + escapeJson(doc->text) + "\"";
        json += "}";
        res.set_content(json, "application/json; charset=utf-8");
    });

    server.Post("/api/index", [&](const httplib::Request& req, httplib::Response& res) {
        lock_guard<mutex> lock(mtx);

        string path = req.get_param_value("path");
        SearchIndex fresh;
        if (path.empty() || !fresh.build(path)) {
            sendError(res, 400, "folder not found");
            return;
        }
        index = move(fresh);
        res.set_content(statsJson(index), "application/json; charset=utf-8");
    });

    if (!server.bind_to_port("127.0.0.1", port)) {
        cout << "Cannot start server on port " << port << " (maybe it is busy?)" << endl;
        return false;
    }
    cout << "Server started: http://localhost:" << port << endl;
    cout << "Press Ctrl+C to stop" << endl;
    return server.listen_after_bind();
}
