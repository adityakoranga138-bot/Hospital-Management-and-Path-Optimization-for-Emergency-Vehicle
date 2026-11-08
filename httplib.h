// MinGW.org 6.3 compatible subset of the cpp-httplib Server API.
// Official cpp-httplib cannot compile here because this toolchain has no std::thread.
#ifndef SMARTCARE_MINIMAL_HTTPLIB_H
#define SMARTCARE_MINIMAL_HTTPLIB_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <functional>
#include <cstring>
#include <cstdlib>

namespace httplib {

struct Request {
    std::string method;
    std::string path;
    std::string body;
    std::map<std::string, std::string> params;
    std::map<std::string, std::string> headers;

    bool has_param(const std::string& key) const {
        return params.find(key) != params.end();
    }

    std::string get_param_value(const std::string& key) const {
        std::map<std::string, std::string>::const_iterator it = params.find(key);
        if (it == params.end()) return "";
        return it->second;
    }
};

struct Response {
    int status;
    std::string body;
    std::map<std::string, std::string> headers;

    Response() : status(200) {}

    void set_content(const std::string& content, const std::string& type) {
        body = content;
        headers["Content-Type"] = type;
    }

    void set_header(const std::string& key, const std::string& value) {
        headers[key] = value;
    }

    void set_redirect(const std::string& location) {
        status = 302;
        headers["Location"] = location;
        body.clear();
    }
};

typedef std::function<void(const Request&, Response&)> Handler;

class Server {
public:
    void Get(const std::string& pattern, Handler handler) {
        gets_.push_back(Route(pattern, handler));
    }

    void Post(const std::string& pattern, Handler handler) {
        posts_.push_back(Route(pattern, handler));
    }

    void Options(const std::string& /*pattern*/, Handler handler) {
        options_ = handler;
    }

    bool set_mount_point(const char* /*mount*/, const char* folder) {
        DWORD attr = GetFileAttributesA(folder);
        if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
            return false;
        }
        mount_dir_ = folder;
        if (!mount_dir_.empty() && mount_dir_.back() == '\\') {
            mount_dir_.pop_back();
        }
        return true;
    }

    bool listen(const char* host, int port) {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;

        SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) {
            WSACleanup();
            return false;
        }

        int opt = 1;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

        sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons((u_short)port);
        addr.sin_addr.s_addr = inet_addr(host);
        if (addr.sin_addr.s_addr == INADDR_NONE) {
            addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        }

        if (bind(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            closesocket(sock);
            WSACleanup();
            return false;
        }
        if (::listen(sock, 16) == SOCKET_ERROR) {
            closesocket(sock);
            WSACleanup();
            return false;
        }

        running_ = true;
        while (running_) {
            sockaddr_in client;
            int clientLen = sizeof(client);
            SOCKET cs = accept(sock, (sockaddr*)&client, &clientLen);
            if (cs == INVALID_SOCKET) continue;
            handleClient(cs);
            closesocket(cs);
        }

        closesocket(sock);
        WSACleanup();
        return true;
    }

private:
    struct Route {
        std::string pattern;
        Handler handler;
        Route(const std::string& p, Handler h) : pattern(p), handler(h) {}
    };

    std::vector<Route> gets_;
    std::vector<Route> posts_;
    Handler options_;
    std::string mount_dir_;
    bool running_;

public:
    Server() : running_(false) {}

    static std::string urlDecode(const std::string& in) {
        std::string out;
        for (size_t i = 0; i < in.size(); i++) {
            if (in[i] == '%' && i + 2 < in.size()) {
                std::string hex = in.substr(i + 1, 2);
                char* endp = 0;
                long v = strtol(hex.c_str(), &endp, 16);
                out.push_back((char)v);
                i += 2;
            } else if (in[i] == '+') {
                out.push_back(' ');
            } else {
                out.push_back(in[i]);
            }
        }
        return out;
    }

    static std::string mimeType(const std::string& path) {
        std::string p = path;
        for (size_t i = 0; i < p.size(); i++) p[i] = (char)tolower((unsigned char)p[i]);
        if (p.size() >= 5 && p.substr(p.size() - 5) == ".html") return "text/html; charset=utf-8";
        if (p.size() >= 4 && p.substr(p.size() - 4) == ".css") return "text/css; charset=utf-8";
        if (p.size() >= 3 && p.substr(p.size() - 3) == ".js") return "application/javascript; charset=utf-8";
        if (p.size() >= 5 && p.substr(p.size() - 5) == ".json") return "application/json; charset=utf-8";
        if (p.size() >= 4 && p.substr(p.size() - 4) == ".svg") return "image/svg+xml";
        if (p.size() >= 4 && p.substr(p.size() - 4) == ".png") return "image/png";
        if (p.size() >= 4 && p.substr(p.size() - 4) == ".ico") return "image/x-icon";
        return "application/octet-stream";
    }

    static bool readFile(const std::string& path, std::string& out) {
        std::ifstream in(path.c_str(), std::ios::binary);
        if (!in) return false;
        std::ostringstream ss;
        ss << in.rdbuf();
        out = ss.str();
        return true;
    }

    static Handler matchRoute(const std::vector<Route>& routes, const std::string& path) {
        for (size_t i = 0; i < routes.size(); i++) {
            if (routes[i].pattern == path) return routes[i].handler;
        }
        return Handler();
    }

    bool serveStatic(const std::string& urlPath, Response& res) {
        if (mount_dir_.empty()) return false;
        std::string rel = urlPath;
        if (rel.empty() || rel[0] != '/') return false;
        if (rel.find("..") != std::string::npos) return false;
        if (rel == "/") rel = "/index.html";

        std::string full = mount_dir_;
        for (size_t i = 0; i < rel.size(); i++) {
            char c = rel[i];
            full.push_back(c == '/' ? '\\' : c);
        }

        DWORD attr = GetFileAttributesA(full.c_str());
        if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY)) {
            return false;
        }

        std::string data;
        if (!readFile(full, data)) return false;
        res.status = 200;
        res.set_content(data, mimeType(rel));
        return true;
    }

    void parseRequest(const std::string& raw, Request& req) {
        size_t lineEnd = raw.find("\r\n");
        if (lineEnd == std::string::npos) return;
        std::string requestLine = raw.substr(0, lineEnd);
        std::istringstream rs(requestLine);
        std::string url;
        rs >> req.method >> url;

        std::string path = url;
        size_t q = url.find('?');
        if (q != std::string::npos) {
            path = url.substr(0, q);
            std::string query = url.substr(q + 1);
            size_t start = 0;
            while (start < query.size()) {
                size_t amp = query.find('&', start);
                if (amp == std::string::npos) amp = query.size();
                std::string pair = query.substr(start, amp - start);
                size_t eq = pair.find('=');
                if (eq != std::string::npos) {
                    req.params[urlDecode(pair.substr(0, eq))] = urlDecode(pair.substr(eq + 1));
                }
                start = amp + 1;
            }
        }
        req.path = path;

        size_t headersEnd = raw.find("\r\n\r\n");
        size_t pos = lineEnd + 2;
        while (headersEnd != std::string::npos && pos < headersEnd) {
            size_t nl = raw.find("\r\n", pos);
            if (nl == std::string::npos || nl > headersEnd) break;
            std::string line = raw.substr(pos, nl - pos);
            size_t colon = line.find(':');
            if (colon != std::string::npos) {
                std::string k = line.substr(0, colon);
                std::string v = line.substr(colon + 1);
                while (!v.empty() && (v[0] == ' ' || v[0] == '\t')) v.erase(v.begin());
                req.headers[k] = v;
            }
            pos = nl + 2;
        }

        if (headersEnd != std::string::npos) {
            req.body = raw.substr(headersEnd + 4);
        }
    }

    std::string recvAll(SOCKET cs) {
        std::string data;
        char buf[4096];
        int n = recv(cs, buf, sizeof(buf), 0);
        if (n <= 0) return data;
        data.append(buf, buf + n);

        size_t headersEnd = data.find("\r\n\r\n");
        int contentLength = 0;
        if (headersEnd != std::string::npos) {
            std::string headerBlock = data.substr(0, headersEnd);
            std::string key = "Content-Length:";
            size_t cl = headerBlock.find(key);
            if (cl == std::string::npos) cl = headerBlock.find("content-length:");
            if (cl != std::string::npos) {
                contentLength = atoi(headerBlock.c_str() + cl + key.size());
            }
            size_t have = data.size() - (headersEnd + 4);
            while ((int)have < contentLength) {
                n = recv(cs, buf, sizeof(buf), 0);
                if (n <= 0) break;
                data.append(buf, buf + n);
                have += n;
            }
        }
        return data;
    }

    void sendAll(SOCKET cs, const std::string& data) {
        size_t sent = 0;
        while (sent < data.size()) {
            int n = send(cs, data.data() + sent, (int)(data.size() - sent), 0);
            if (n <= 0) break;
            sent += n;
        }
    }

    static std::string reason(int status) {
        if (status == 200) return "OK";
        if (status == 204) return "No Content";
        if (status == 302) return "Found";
        if (status == 400) return "Bad Request";
        if (status == 404) return "Not Found";
        if (status == 409) return "Conflict";
        if (status == 500) return "Internal Server Error";
        return "OK";
    }

    void handleClient(SOCKET cs) {
        std::string raw = recvAll(cs);
        if (raw.empty()) return;

        Request req;
        parseRequest(raw, req);

        Response res;
        bool handled = false;

        if (req.method == "OPTIONS" && options_) {
            options_(req, res);
            handled = true;
        } else if (req.method == "GET") {
            Handler h = matchRoute(gets_, req.path);
            if (h) {
                h(req, res);
                handled = true;
            } else if (serveStatic(req.path, res)) {
                handled = true;
            }
        } else if (req.method == "POST") {
            Handler h = matchRoute(posts_, req.path);
            if (h) {
                h(req, res);
                handled = true;
            }
        }

        if (!handled) {
            res.status = 404;
            res.set_content("{\"ok\":false,\"error\":\"Not found\"}", "application/json; charset=utf-8");
        }

        if (res.headers.find("Access-Control-Allow-Origin") == res.headers.end()) {
            res.headers["Access-Control-Allow-Origin"] = "*";
        }
        res.headers["Connection"] = "close";

        std::ostringstream out;
        out << "HTTP/1.1 " << res.status << " " << reason(res.status) << "\r\n";
        for (std::map<std::string, std::string>::iterator it = res.headers.begin();
             it != res.headers.end(); ++it) {
            out << it->first << ": " << it->second << "\r\n";
        }
        out << "Content-Length: " << res.body.size() << "\r\n\r\n";
        out << res.body;
        sendAll(cs, out.str());
    }
};

} // namespace httplib

#endif
