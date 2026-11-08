// SmartCare HTTP adapter — uses the completed main.cpp without modifying it.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include "httplib.h"

#define main phase3_cli_main
#include "main.cpp"
#undef main

#include <sstream>
#include <cctype>

#ifdef _WIN32
#include <windows.h>
#endif

using httplib::Request;
using httplib::Response;
using httplib::Server;

static HospitalSystem g_hospital;
static CityMap g_cityMap;

#ifdef _WIN32
static CRITICAL_SECTION g_hospitalLock;
class HospitalLockGuard {
public:
    HospitalLockGuard() { EnterCriticalSection(&g_hospitalLock); }
    ~HospitalLockGuard() { LeaveCriticalSection(&g_hospitalLock); }
};
#endif

static const int kPort = 8080;
static const char* kHost = "127.0.0.1";

class CoutCapture {
    std::streambuf* oldBuf;
    std::ostringstream buffer;
public:
    CoutCapture() {
        oldBuf = std::cout.rdbuf(buffer.rdbuf());
    }
    ~CoutCapture() {
        std::cout.rdbuf(oldBuf);
    }
    std::string str() const { return buffer.str(); }
};

static std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        switch (c) {
            case '\"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    out += c;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

static std::string trimCopy(std::string s) {
    while (!s.empty() && (s.back() == ' ' || s.back() == '\r' || s.back() == '\n' || s.back() == '\t')) {
        s.pop_back();
    }
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\r' || s[i] == '\n' || s[i] == '\t')) {
        i++;
    }
    return s.substr(i);
}

static bool extractJsonInt(const std::string& body, const std::string& key, int& value) {
    std::string pattern = "\"" + key + "\"";
    size_t pos = body.find(pattern);
    if (pos == std::string::npos) return false;
    pos = body.find(':', pos + pattern.size());
    if (pos == std::string::npos) return false;
    pos++;
    while (pos < body.size() && std::isspace(static_cast<unsigned char>(body[pos]))) pos++;
    if (pos >= body.size()) return false;
    try {
        size_t idx = 0;
        value = std::stoi(body.substr(pos), &idx);
        return idx > 0;
    } catch (...) {
        return false;
    }
}

static bool extractJsonString(const std::string& body, const std::string& key, std::string& value) {
    std::string pattern = "\"" + key + "\"";
    size_t pos = body.find(pattern);
    if (pos == std::string::npos) return false;
    pos = body.find(':', pos + pattern.size());
    if (pos == std::string::npos) return false;
    pos++;
    while (pos < body.size() && std::isspace(static_cast<unsigned char>(body[pos]))) pos++;
    if (pos >= body.size()) return false;

    if (body[pos] == '"') {
        pos++;
        std::string acc;
        while (pos < body.size()) {
            char c = body[pos++];
            if (c == '\\' && pos < body.size()) {
                acc.push_back(body[pos++]);
            } else if (c == '"') {
                break;
            } else {
                acc.push_back(c);
            }
        }
        value = acc;
        return true;
    }

    size_t end = pos;
    while (end < body.size() && body[end] != ',' && body[end] != '}' &&
           !std::isspace(static_cast<unsigned char>(body[end]))) {
        end++;
    }
    value = body.substr(pos, end - pos);
    return !value.empty();
}

static int exceptionStatus(const SmartCareException& e) {
    std::string msg = e.what();
    if (dynamic_cast<const WardFullException*>(&e) != 0 ||
        msg.find("[WARD SATURATED]") != std::string::npos) {
        return 409;
    }
    if (dynamic_cast<const PatientNotFoundException*>(&e) != 0 ||
        msg.find("[RECORD ERROR]") != std::string::npos) {
        return 404;
    }
    return 400;
}

static void sendJson(Response& res, int status, const std::string& json) {
    res.status = status;
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Cache-Control", "no-store");
    res.set_content(json, "application/json; charset=utf-8");
}

static void sendError(Response& res, int status, const std::string& message) {
    sendJson(res, status, "{\"ok\":false,\"error\":\"" + jsonEscape(message) + "\"}");
}

static bool parseAfterToken(const std::string& text, const std::string& token, std::string& value) {
    size_t pos = text.find(token);
    if (pos == std::string::npos) return false;
    pos += token.size();
    size_t end = text.find('\n', pos);
    if (end == std::string::npos) end = text.size();
    value = trimCopy(text.substr(pos, end - pos));
    return true;
}

static int parseIntFromString(const std::string& s, int fallback = 0) {
    try {
        return std::stoi(s);
    } catch (...) {
        return fallback;
    }
}

static std::vector<std::string> splitRoute(const std::string& routeLine) {
    std::vector<std::string> parts;
    const std::string delim = " -> ";
    size_t start = 0;
    while (start < routeLine.size()) {
        size_t found = routeLine.find(delim, start);
        if (found == std::string::npos) {
            std::string item = trimCopy(routeLine.substr(start));
            if (!item.empty()) parts.push_back(item);
            break;
        }
        std::string item = trimCopy(routeLine.substr(start, found - start));
        if (!item.empty()) parts.push_back(item);
        start = found + delim.size();
    }
    return parts;
}

static std::string routeToJson(const std::vector<std::string>& parts) {
    std::ostringstream os;
    os << "[";
    for (size_t i = 0; i < parts.size(); i++) {
        if (i) os << ",";
        os << "\"" << jsonEscape(parts[i]) << "\"";
    }
    os << "]";
    return os.str();
}

static std::string dumpLocationsJson() {
    std::ostringstream os;
    os << "{\"ok\":true,\"hospitalNode\":" << HOSPITAL_NODE
       << ",\"maxBeds\":" << MAX_BEDS
       << ",\"locations\":[";
    for (int i = 0; i < NUM_LOCATIONS; i++) {
        if (i) os << ",";
        os << "{\"id\":" << i
           << ",\"name\":\"" << jsonEscape(g_cityMap.getLocationName(i)) << "\""
           << ",\"isHospital\":" << (i == HOSPITAL_NODE ? "true" : "false")
           << "}";
    }
    os << "]}";
    return os.str();
}

static void occupancyFromBedsText(const std::string& text, int& occupied) {
    occupied = 0;
    size_t pos = text.find("HOSPITAL WARD OCCUPANCY (");
    if (pos == std::string::npos) return;
    pos += std::string("HOSPITAL WARD OCCUPANCY (").size();
    occupied = parseIntFromString(text.substr(pos), 0);
}

static std::string patientsJsonFromBedsText(const std::string& text) {
    std::ostringstream os;
    os << "[";
    bool first = true;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (line.find("Bed ") != 0) continue;
        if (line.find("| ID:") == std::string::npos) continue;

        int bedNo = parseIntFromString(line.substr(4));
        int id = 0;
        int fare = 0;
        std::string name;
        std::string ward;

        size_t idPos = line.find("ID:");
        if (idPos != std::string::npos) {
            id = parseIntFromString(line.substr(idPos + 3));
        }
        size_t namePos = line.find("Name:");
        size_t wardPos = line.find("Ward:");
        if (namePos != std::string::npos && wardPos != std::string::npos) {
            name = trimCopy(line.substr(namePos + 5, wardPos - (namePos + 5)));
            if (!name.empty() && name.back() == '|') {
                name = trimCopy(name.substr(0, name.size() - 1));
            }
        }
        size_t ambPos = line.find("Inbound Fare:");
        if (wardPos != std::string::npos) {
            size_t endWard = (ambPos == std::string::npos) ? line.size() : ambPos;
            ward = trimCopy(line.substr(wardPos + 5, endWard - (wardPos + 5)));
            if (!ward.empty() && ward.back() == '|') {
                ward = trimCopy(ward.substr(0, ward.size() - 1));
            }
        }
        if (ambPos != std::string::npos) {
            size_t rs = line.find("Rs.", ambPos);
            if (rs != std::string::npos) {
                fare = parseIntFromString(line.substr(rs + 3));
            }
        }

        bool icu = ward.find("ICU") != std::string::npos;
        if (!first) os << ",";
        first = false;
        os << "{\"bed\":" << bedNo
           << ",\"id\":" << id
           << ",\"name\":\"" << jsonEscape(name) << "\""
           << ",\"ward\":\"" << jsonEscape(ward) << "\""
           << ",\"bedType\":" << (icu ? 2 : 1)
           << ",\"ambulanceFare\":" << fare
           << "}";
    }
    os << "]";
    return os.str();
}

static void parseQueueText(const std::string& text, int& queueSize, bool& hasNext,
                           std::string& nextName, int& nextPriority) {
    queueSize = 0;
    hasNext = false;
    nextName.clear();
    nextPriority = 2;

    size_t pos = text.find("PENDING EMERGENCY CALLS (");
    if (pos != std::string::npos) {
        queueSize = parseIntFromString(text.substr(pos + std::string("PENDING EMERGENCY CALLS (").size()));
    }
    size_t npos = text.find("Next Dispatched Call -> ");
    if (npos != std::string::npos) {
        hasNext = true;
        std::string rest = text.substr(npos + std::string("Next Dispatched Call -> ").size());
        size_t bar = rest.find('|');
        if (bar != std::string::npos) {
            nextName = trimCopy(rest.substr(0, bar));
            nextPriority = (rest.find("Priority Rank: 1") != std::string::npos) ? 1 : 2;
        } else {
            nextName = trimCopy(rest);
        }
    }
}

static std::string pharmacyJsonFromText(const std::string& text) {
    std::ostringstream os;
    os << "[";
    bool first = true;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        size_t dot = line.find(". ");
        size_t stockPos = line.find("| Stock:");
        size_t pricePos = line.find("| Unit Price:");
        if (dot == std::string::npos || stockPos == std::string::npos || pricePos == std::string::npos) continue;
        if (dot > 4) continue;

        std::string name = trimCopy(line.substr(dot + 2, stockPos - (dot + 2)));
        int stock = parseIntFromString(line.substr(stockPos + 8));
        double price = 0.0;
        size_t rs = line.find("Rs.", pricePos);
        if (rs != std::string::npos) {
            try { price = std::stod(line.substr(rs + 3)); } catch (...) { price = 0.0; }
        }
        if (!first) os << ",";
        first = false;
        os << "{\"name\":\"" << jsonEscape(name) << "\",\"stock\":" << stock
           << ",\"price\":" << price << "}";
    }
    os << "]";
    return os.str();
}

static std::string exeDirectory() {
#ifdef _WIN32
    char path[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, path, MAX_PATH);
    if (n == 0) return ".";
    std::string full(path, path + n);
    size_t slash = full.find_last_of("\\/");
    if (slash == std::string::npos) return ".";
    return full.substr(0, slash);
#else
    return ".";
#endif
}

int main() {
    // Serialize access to shared hospital data and std::cout capture using
    // the Windows critical-section API (compatible with MinGW Win32 threads).
#ifdef _WIN32
    InitializeCriticalSection(&g_hospitalLock);
#endif
    Server svr;

    svr.Options("/.*", [](const Request&, Response& res) {
        res.status = 204;
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    });

    svr.Get("/api/locations", [](const Request&, Response& res) {
#ifdef _WIN32
        HospitalLockGuard lock;
#endif
        sendJson(res, 200, dumpLocationsJson());
    });

    svr.Get("/api/route", [](const Request& req, Response& res) {
#ifdef _WIN32
        HospitalLockGuard lock;
#endif
        if (!req.has_param("from")) {
            sendError(res, 400, "Missing query parameter 'from'.");
            return;
        }
        int fromNode = 0;
        try {
            fromNode = std::stoi(req.get_param_value("from"));
        } catch (...) {
            sendError(res, 400, "Query parameter 'from' must be an integer.");
            return;
        }

        try {
            CoutCapture capture;
            int dist = g_cityMap.findShortestDistance(fromNode, HOSPITAL_NODE, true);
            std::string out = capture.str();

            std::string routeLine;
            parseAfterToken(out, "Route Traversed:", routeLine);
            std::vector<std::string> parts = splitRoute(routeLine);
            int estTime = (dist * 60) / 40;

            std::ostringstream os;
            os << "{\"ok\":true"
               << ",\"from\":" << fromNode
               << ",\"to\":" << HOSPITAL_NODE
               << ",\"fromName\":\"" << jsonEscape(g_cityMap.getLocationName(fromNode)) << "\""
               << ",\"toName\":\"" << jsonEscape(g_cityMap.getLocationName(HOSPITAL_NODE)) << "\""
               << ",\"distanceKm\":" << dist
               << ",\"estimatedTimeMins\":" << estTime
               << ",\"route\":" << routeToJson(parts)
               << "}";
            sendJson(res, 200, os.str());
        } catch (const SmartCareException& e) {
            sendError(res, exceptionStatus(e), e.what());
        }
    });

    svr.Post("/api/emergency", [](const Request& req, Response& res) {
#ifdef _WIN32
        HospitalLockGuard lock;
#endif
        int pickupLoc = 0, id = 0, priority = 0;
        std::string name;
        if (!extractJsonInt(req.body, "pickupLoc", pickupLoc) ||
            !extractJsonInt(req.body, "id", id) ||
            !extractJsonInt(req.body, "priority", priority) ||
            !extractJsonString(req.body, "name", name)) {
            sendError(res, 400, "JSON body must include pickupLoc, id, name, and priority.");
            return;
        }
        name = trimCopy(name);
        if (name.empty()) {
            sendError(res, 400, "Patient name cannot be empty.");
            return;
        }
        if (name.find(' ') != std::string::npos) {
            sendError(res, 400, "Patient name must be a single word (C++ parser does not accept spaces).");
            return;
        }

        try {
            CoutCapture capture;
            g_hospital.queueEmergencyCall(pickupLoc, id, name, priority);
            std::string msg = trimCopy(capture.str());
            std::ostringstream os;
            os << "{\"ok\":true,\"message\":\"" << jsonEscape(msg) << "\""
               << ",\"pickupLoc\":" << pickupLoc
               << ",\"id\":" << id
               << ",\"name\":\"" << jsonEscape(name) << "\""
               << ",\"priority\":" << priority
               << "}";
            sendJson(res, 200, os.str());
        } catch (const SmartCareException& e) {
            sendError(res, exceptionStatus(e), e.what());
        }
    });

    svr.Post("/api/dispatch", [](const Request&, Response& res) {
#ifdef _WIN32
        HospitalLockGuard lock;
#endif
        try {
            CoutCapture capture;
            g_hospital.processNextDispatch();
            std::string out = capture.str();

            if (out.find("[QUEUE EMPTY]") != std::string::npos) {
                sendJson(res, 200, "{\"ok\":true,\"empty\":true,\"message\":\"No pending emergency vehicle calls.\"}");
                return;
            }

            std::string routeLine, name, condition, pickup, distText, timeText, fareText, bedText, idText;
            parseAfterToken(out, "Route Traversed:", routeLine);
            parseAfterToken(out, "Name :", name);
            parseAfterToken(out, "Patient ID :", idText);
            parseAfterToken(out, "Condition :", condition);
            parseAfterToken(out, "Pickup Point :", pickup);
            parseAfterToken(out, "Route Distance :", distText);
            parseAfterToken(out, "Est. Transit Time :", timeText);
            parseAfterToken(out, "Ambulance Fare :", fareText);
            parseAfterToken(out, "Bed Assigned :", bedText);

            int dist = parseIntFromString(distText);
            int mins = parseIntFromString(timeText);
            int fare = 0;
            size_t rs = fareText.find("Rs.");
            if (rs != std::string::npos) fare = parseIntFromString(fareText.substr(rs + 3));
            int bedNo = 0;
            size_t bedWord = bedText.find("Bed ");
            if (bedWord != std::string::npos) bedNo = parseIntFromString(bedText.substr(bedWord + 4));
            bool icu = bedText.find("ICU") != std::string::npos || condition.find("Critical") != std::string::npos;

            std::ostringstream os;
            os << "{\"ok\":true,\"empty\":false"
               << ",\"name\":\"" << jsonEscape(name) << "\""
               << ",\"patientId\":" << parseIntFromString(idText)
               << ",\"condition\":\"" << jsonEscape(condition) << "\""
               << ",\"pickupPoint\":\"" << jsonEscape(pickup) << "\""
               << ",\"distanceKm\":" << dist
               << ",\"estimatedTimeMins\":" << mins
               << ",\"ambulanceFare\":" << fare
               << ",\"bedAssigned\":" << bedNo
               << ",\"maxBeds\":" << MAX_BEDS
               << ",\"ward\":\"" << (icu ? "ICU" : "General Ward") << "\""
               << ",\"route\":" << routeToJson(splitRoute(routeLine))
               << "}";
            sendJson(res, 200, os.str());
        } catch (const SmartCareException& e) {
            sendError(res, exceptionStatus(e), e.what());
        }
    });

    svr.Get("/api/status", [](const Request&, Response& res) {
#ifdef _WIN32
        HospitalLockGuard lock;
#endif
        CoutCapture capture;
        g_hospital.displayBeds();
        g_hospital.displayQueueStatus();
        std::string out = capture.str();

        int occupied = 0;
        occupancyFromBedsText(out, occupied);
        int queueSize = 0;
        bool hasNext = false;
        std::string nextName;
        int nextPriority = 2;
        parseQueueText(out, queueSize, hasNext, nextName, nextPriority);

        std::ostringstream os;
        os << "{\"ok\":true"
           << ",\"occupancy\":" << occupied
           << ",\"maxBeds\":" << MAX_BEDS
           << ",\"available\":" << (MAX_BEDS - occupied)
           << ",\"patients\":" << patientsJsonFromBedsText(out)
           << ",\"queueSize\":" << queueSize;
        if (hasNext) {
            os << ",\"nextInQueue\":{\"name\":\"" << jsonEscape(nextName)
               << "\",\"priority\":" << nextPriority
               << ",\"priorityLabel\":\"" << (nextPriority == 1 ? "Critical" : "Normal") << "\"}";
        } else {
            os << ",\"nextInQueue\":null";
        }
        os << "}";
        sendJson(res, 200, os.str());
    });

    svr.Get("/api/pharmacy", [](const Request&, Response& res) {
#ifdef _WIN32
        HospitalLockGuard lock;
#endif
        CoutCapture capture;
        g_hospital.displayPharmacy();
        std::string out = capture.str();
        std::ostringstream os;
        os << "{\"ok\":true,\"medicines\":" << pharmacyJsonFromText(out) << "}";
        sendJson(res, 200, os.str());
    });

    svr.Post("/api/discharge", [](const Request& req, Response& res) {
#ifdef _WIN32
        HospitalLockGuard lock;
#endif
        int patientId = 0;
        int days = 1;
        if (!extractJsonInt(req.body, "patientId", patientId)) {
            sendError(res, 400, "JSON body must include patientId.");
            return;
        }
        if (!extractJsonInt(req.body, "daysStayed", days)) {
            days = 1;
        }
        if (days < 1) days = 1;

        try {
            CoutCapture capture;
            g_hospital.dischargePatient(patientId, days);
            std::string out = capture.str();

            std::string name, ward, ambText, roomText, totalText, receipt;
            parseAfterToken(out, "Patient Name     :", name);
            parseAfterToken(out, "Ward Category    :", ward);
            parseAfterToken(out, "Ambulance Transit:", ambText);
            parseAfterToken(out, "Room Stay Charge :", roomText);
            parseAfterToken(out, "TOTAL PAYABLE    :", totalText);

            int amb = 0, room = 0, total = 0;
            size_t rs1 = ambText.find("Rs.");
            size_t rs2 = roomText.find("Rs.");
            size_t rs3 = totalText.find("Rs.");
            if (rs1 != std::string::npos) amb = parseIntFromString(ambText.substr(rs1 + 3));
            if (rs2 != std::string::npos) room = parseIntFromString(roomText.substr(rs2 + 3));
            if (rs3 != std::string::npos) total = parseIntFromString(totalText.substr(rs3 + 3));

            size_t recPos = out.find("receipt_");
            if (recPos != std::string::npos) {
                size_t recEnd = out.find(".txt", recPos);
                if (recEnd != std::string::npos) {
                    receipt = out.substr(recPos, recEnd + 4 - recPos);
                }
            }

            std::ostringstream os;
            os << "{\"ok\":true"
               << ",\"patientId\":" << patientId
               << ",\"name\":\"" << jsonEscape(name) << "\""
               << ",\"ward\":\"" << jsonEscape(ward) << "\""
               << ",\"daysStayed\":" << days
               << ",\"ambulanceFare\":" << amb
               << ",\"roomRent\":" << room
               << ",\"totalBill\":" << total
               << ",\"receiptFile\":\"" << jsonEscape(receipt) << "\""
               << "}";
            sendJson(res, 200, os.str());
        } catch (const SmartCareException& e) {
            sendError(res, exceptionStatus(e), e.what());
        }
    });

    svr.Get("/api/beds", [](const Request&, Response& res) {
#ifdef _WIN32
        HospitalLockGuard lock;
#endif
        CoutCapture capture;
        g_hospital.displayBeds();
        std::string out = capture.str();
        int occupied = 0;
        occupancyFromBedsText(out, occupied);
        bool available = g_hospital.hasBedAvailable();
        std::ostringstream os;
        os << "{\"ok\":true"
           << ",\"occupied\":" << occupied
           << ",\"maxBeds\":" << MAX_BEDS
           << ",\"available\":" << (MAX_BEDS - occupied)
           << ",\"hasBedAvailable\":" << (available ? "true" : "false")
           << "}";
        sendJson(res, 200, os.str());
    });

    std::string base = exeDirectory();
    std::string dash1 = base + "\\dashboard";
    std::string dash2 = "dashboard";
    bool mounted = svr.set_mount_point("/", dash1.c_str()) || svr.set_mount_point("/", dash2.c_str());
    if (!mounted) {
        std::cerr << "[WARN] Could not mount dashboard folder. APIs will still run." << std::endl;
    }

    svr.Get("/", [](const Request&, Response& res) {
        res.set_redirect("/index.html");
    });

    std::cout << "SmartCare dashboard server listening on http://" << kHost << ":" << kPort << std::endl;
    std::cout << "Existing CLI remains available via phase3.exe (do not run both at once)." << std::endl;
    if (!svr.listen(kHost, kPort)) {
        std::cerr << "Failed to bind " << kHost << ":" << kPort << std::endl;
        return 1;
    }
    return 0;
}
