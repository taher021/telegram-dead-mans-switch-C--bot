#define _CRT_SECURE_NO_WARNINGS

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <curl/curl.h>
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;


static const char* VERSION = "1.0.0";
static const long long MAX_FILE_BYTES = 50LL * 1024 * 1024;  // Telegram bot limit: 50 MB
static const size_t MAX_MESSAGE_CHARS = 4000;                // Telegram limit is 4096
static const int POLL_TIMEOUT_SEC = 20;                      // long-polling time

static string g_configPath = "config.json";
static string g_statePath = "state.json";

static volatile sig_atomic_t g_running = 1;
static void onSignal(int) { g_running = 0; }



struct Config {
    string botToken;
    long long ownerId = 0;
    string channelId;
    string password;
    vector<string> files;
    string message;
    string proxy;
    long long defaultSeconds = 24 * 3600;
    long long warnBeforeSeconds = 4 * 3600;
};

struct State {
    long long lastCheckin = 0;
    long long intervalSec = 24 * 3600;
    long long warnBeforeSec = 4 * 3600;
    long long lastUpdateId = 0;
    long long lastWarn = 0;
    long long sentParts = 0;
    bool paused = false;
    bool sent = false;
};


static long long nowSec() {
    return chrono::duration_cast<chrono::seconds>(
               chrono::system_clock::now().time_since_epoch())
        .count();
}

static string fmtTime(long long ts) {
    time_t t = (time_t)ts;
    tm tmv{};
#if defined(_MSC_VER)
    localtime_s(&tmv, &t);
#elif defined(_WIN32)
    tm* tp = localtime(&t);  // MinGW (Code::Blocks): no localtime_r, and we only call this from one thread
    if (tp) tmv = *tp;
#else
    localtime_r(&t, &tmv);
#endif
    char b[64];
    strftime(b, sizeof b, "%Y-%m-%d %H:%M:%S", &tmv);
    return b;
}

static void logLine(const string& s) {
    cout << "[" << fmtTime(nowSec()) << "] " << s << endl;
}


static bool safeEq(const string& a, const string& b) {
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (size_t i = 0; i < a.size(); ++i) diff |= (unsigned char)(a[i] ^ b[i]);
    return diff == 0;
}

static string trim(const string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

static string plural(long long n, const string& word) {
    return to_string(n) + " " + word + (n == 1 ? "" : "s");
}

// 93784 -> "1 day, 2 hours and 3 minutes"
static string fmtDur(long long s) {
    if (s < 0) s = 0;
    long long d = s / 86400, h = (s % 86400) / 3600, m = (s % 3600) / 60;
    vector<string> p;
    if (d) p.push_back(plural(d, "day"));
    if (h) p.push_back(plural(h, "hour"));
    if (m || p.empty()) p.push_back(plural(m, "minute"));
    string out;
    for (size_t i = 0; i < p.size(); ++i) {
        if (i) out += (i + 1 == p.size()) ? " and " : ", ";
        out += p[i];
    }
    return out;
}


static long long parseDuration(string s) {
    if (s.empty()) return -1;
    char unit = 'h';
    char last = (char)tolower((unsigned char)s.back());
    if (last == 'm' || last == 'h' || last == 'd') {
        unit = last;
        s.pop_back();
    }
    try {
        size_t pos = 0;
        double v = stod(s, &pos);
        if (pos != s.size() || v <= 0) return -1;
        double mult = (unit == 'm') ? 60.0 : (unit == 'h') ? 3600.0 : 86400.0;
        double sec = v * mult;
        if (sec < 60.0 || sec > 365.0 * 86400.0) return -1;
        return (long long)sec;
    } catch (...) {
        return -1;
    }
}

static string envOr(const char* name, const string& fallback) {
    const char* v = getenv(name);
    return (v && *v) ? string(v) : fallback;
}

static bool isDigits(const string& s) {
    return !s.empty() && all_of(s.begin(), s.end(), [](unsigned char ch) { return isdigit(ch) != 0; });
}

static bool validChannelId(const string& id) {
    if (id.size() >= 6 && id[0] == '@') {
        return all_of(id.begin() + 1, id.end(), [](unsigned char ch) { return isalnum(ch) || ch == '_'; });
    }
    string digits = (!id.empty() && id[0] == '-') ? id.substr(1) : id;
    return digits.size() >= 5 && isDigits(digits);
}


static bool loadConfig(Config& c) {
    json j = json::object();
    ifstream in(g_configPath);
    if (in) {
        j = json::parse(in, nullptr, false);
        if (j.is_discarded() || !j.is_object()) {
            cerr << "CONFIG ERROR: " << g_configPath
                 << " is not valid JSON. Check for missing quotes or commas.\n";
            return false;
        }
    } else {
        cerr << "Note: " << g_configPath << " not found, using environment variables only.\n";
    }

    try {
        c.botToken = j.value("bot_token", string());
        c.ownerId = j.value("owner_id", 0LL);
        if (j.contains("channel_id")) {
            const json& ch = j["channel_id"];
            if (ch.is_string()) c.channelId = ch.get<string>();
            else if (ch.is_number_integer()) c.channelId = to_string(ch.get<long long>());
        }
        c.password = j.value("password", string());
        c.message = j.value("message", string());
        c.proxy = j.value("proxy", string());
        c.defaultSeconds = (long long)(j.value("default_hours", 24.0) * 3600.0);
        c.warnBeforeSeconds = (long long)(j.value("warn_before_hours", 4.0) * 3600.0);

        if (j.contains("files") && j["files"].is_array()) {
            for (const auto& f : j["files"]) {
                if (f.is_string()) c.files.push_back(f.get<string>());
            }
        } else if (j.contains("file_path") && j["file_path"].is_string()) {
            c.files.push_back(j["file_path"].get<string>());  // old single-file name
        }
    } catch (const exception& e) {
        cerr << "CONFIG ERROR: a value in " << g_configPath << " has the wrong type (" << e.what()
             << "). Numbers must not be in quotes (owner_id), text must be in quotes.\n";
        return false;
    }

    // Environment variables win over the config file.
    c.botToken = envOr("DEADSWITCH_BOT_TOKEN", c.botToken);
    c.channelId = envOr("DEADSWITCH_CHANNEL_ID", c.channelId);
    c.password = envOr("DEADSWITCH_PASSWORD", c.password);
    c.proxy = envOr("DEADSWITCH_PROXY", c.proxy);
    string ownerEnv = envOr("DEADSWITCH_OWNER_ID", "");
    if (!ownerEnv.empty()) c.ownerId = atoll(ownerEnv.c_str());

    return true;
}


static bool validateConfig(const Config& c) {
    vector<string> errs;

    if (c.botToken.find(':') == string::npos || c.botToken.find(' ') != string::npos)
        errs.push_back("bot_token is missing or wrong. It must look like 123456789:AAH... (copy it from @BotFather).");
    if (c.ownerId <= 0)
        errs.push_back("owner_id must be your numeric Telegram user ID (a positive number, without quotes).");
    if (!validChannelId(c.channelId))
        errs.push_back("channel_id must look like @mychannel (public channel) or -1001234567890 (private channel).");

    if (c.password.size() < 8)
        errs.push_back("password must be at least 8 characters long (longer is better).");
    if (c.password.find_first_of(" \t\r\n") != string::npos)
        errs.push_back("password must not contain spaces.");
    if (!c.password.empty() && c.password[0] == '/')
        errs.push_back("password must not start with '/'.");
    if (c.password.rfind("CHANGE-THIS", 0) == 0)
        errs.push_back("you still use the example password. Choose your own secret password.");

    if (c.files.empty() && c.message.empty())
        errs.push_back("nothing to publish: set \"files\" and/or \"message\" in the config.");
    if (c.message.size() > MAX_MESSAGE_CHARS)
        errs.push_back("message is too long (max about 4000 characters). Put long text into a file instead.");

    for (const auto& f : c.files) {
        error_code ec;
        if (!filesystem::is_regular_file(f, ec)) {
            errs.push_back("file not found: " + f);
            continue;
        }
        auto sz = filesystem::file_size(f, ec);
        if (ec || sz == 0) errs.push_back("file is empty or unreadable: " + f);
        else if ((long long)sz > MAX_FILE_BYTES) errs.push_back("file is bigger than 50 MB (Telegram's bot limit): " + f);
    }

    if (!c.proxy.empty() && c.proxy.find("://") == string::npos)
        errs.push_back("proxy must look like socks5h://127.0.0.1:10808 or http://127.0.0.1:10809 (or be empty).");

    if (c.defaultSeconds < 60)
        errs.push_back("default_hours must be at least 1 minute (0.017).");

    for (const auto& e : errs) cerr << "CONFIG ERROR: " << e << "\n";
    return errs.empty();
}


static void saveState(const State& s) {
    json j = {{"last_checkin", s.lastCheckin},
              {"interval_sec", s.intervalSec},
              {"warn_before_sec", s.warnBeforeSec},
              {"last_update_id", s.lastUpdateId},
              {"last_warn", s.lastWarn},
              {"sent_parts", s.sentParts},
              {"paused", s.paused},
              {"sent", s.sent}};
    string tmp = g_statePath + ".tmp";
    {
        ofstream out(tmp, ios::trunc);
        out << j.dump(2);
    }
#ifdef _WIN32
    // MinGW's rename() fails if the target exists, so ask Windows to replace it.
    if (!MoveFileExA(tmp.c_str(), g_statePath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        logLine("WARNING: could not save state (Windows error " + to_string((long long)GetLastError()) + ")");
#else
    error_code ec;
    filesystem::rename(tmp, g_statePath, ec);
    if (ec) logLine("WARNING: could not save state: " + ec.message());
#endif
}

static State loadState(const Config& c) {
    State s;
    s.lastCheckin = nowSec();
    s.intervalSec = c.defaultSeconds;
    s.warnBeforeSec = c.warnBeforeSeconds;

    ifstream in(g_statePath);
    if (in) {
        json j = json::parse(in, nullptr, false);
        if (!j.is_discarded() && j.is_object()) {
            try {
                s.lastCheckin = j.value("last_checkin", s.lastCheckin);
                s.intervalSec = j.value("interval_sec", s.intervalSec);
                s.warnBeforeSec = j.value("warn_before_sec", s.warnBeforeSec);
                s.lastUpdateId = j.value("last_update_id", 0LL);
                s.lastWarn = j.value("last_warn", 0LL);
                s.sentParts = j.value("sent_parts", 0LL);
                s.paused = j.value("paused", false);
                s.sent = j.value("sent", false);
            } catch (...) {
                logLine("WARNING: state file has wrong values, using defaults.");
            }
        } else {
            logLine("WARNING: state file is damaged, starting a fresh timer.");
        }
    }
    return s;
}


static size_t writeCb(void* contents, size_t size, size_t nmemb, string* out) {
    out->append((char*)contents, size * nmemb);
    return size * nmemb;
}

static json apiCall(const Config& cfg, const string& method, const map<string, string>& fields,
                    const string& filePath = "", const string& fileField = "document",
                    long timeoutSec = 30) {
    json result = {{"ok", false}, {"description", "network error"}, {"error_code", 0}};

    CURL* curl = curl_easy_init();
    if (!curl) return result;

    string url = "https://api.telegram.org/bot" + cfg.botToken + "/" + method;
    string body;


    curl_mime* mime = nullptr;
    if (!fields.empty() || !filePath.empty()) {
        mime = curl_mime_init(curl);
        for (const auto& kv : fields) {
            curl_mimepart* part = curl_mime_addpart(mime);
            curl_mime_name(part, kv.first.c_str());
            curl_mime_data(part, kv.second.c_str(), kv.second.size());
        }
        if (!filePath.empty()) {
            curl_mimepart* part = curl_mime_addpart(mime);
            curl_mime_name(part, fileField.c_str());
            curl_mime_filedata(part, filePath.c_str());
        }
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    if (mime) curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeoutSec);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    if (!cfg.proxy.empty()) curl_easy_setopt(curl, CURLOPT_PROXY, cfg.proxy.c_str());

    struct curl_slist* headers = curl_slist_append(nullptr, "Expect:");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);


    CURLcode rc = CURLE_OK;
    for (int attempt = 1; attempt <= 3; ++attempt) {
        body.clear();
        rc = curl_easy_perform(curl);
        bool retryable = (rc == CURLE_COULDNT_RESOLVE_PROXY || rc == CURLE_COULDNT_RESOLVE_HOST ||
                          rc == CURLE_COULDNT_CONNECT || rc == CURLE_SSL_CONNECT_ERROR ||
                          rc == CURLE_SEND_ERROR || rc == CURLE_RECV_ERROR || rc == CURLE_PARTIAL_FILE);
        if (rc == CURLE_OK || !retryable || attempt == 3 || !g_running) break;
        logLine(string("Network hiccup (") + curl_easy_strerror(rc) + "), trying again...");
        this_thread::sleep_for(chrono::seconds(2 * attempt));
    }
    if (rc == CURLE_OK) {
        json r = json::parse(body, nullptr, false);
        if (!r.is_discarded() && r.is_object()) {
            result = r;
        } else {
            long http = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http);
            result["description"] = "unexpected reply (HTTP " + to_string(http) + "): " +
                                    (body.empty() ? string("<empty>") : body.substr(0, 150));
        }
    } else {
        result["description"] = string("network problem: ") + curl_easy_strerror(rc);
    }

    if (mime) curl_mime_free(mime);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return result;
}

static string errorText(const json& r) {
    return r.value("description", string("unknown error"));
}

static void tell(const Config& c, const string& text) {
    json r = apiCall(c, "sendMessage", {{"chat_id", to_string(c.ownerId)}, {"text", text}});
    if (!r.value("ok", false)) logLine("Could not send a message to you: " + errorText(r));
}

static void deleteMsg(const Config& c, long long messageId) {
    apiCall(c, "deleteMessage",
            {{"chat_id", to_string(c.ownerId)}, {"message_id", to_string(messageId)}});
}


static bool selfCheck(const Config& c, string& report, bool& fatal) {
    ostringstream o;
    bool ok = true;
    fatal = false;

    json me = apiCall(c, "getMe", {});
    if (!me.value("ok", false)) {
        o << "[PROBLEM] Cannot use the bot token: " << errorText(me) << "\n";
        if (me.value("error_code", 0) == 401) {
            o << "          Telegram says the token is wrong. Copy it again from @BotFather.\n";
            fatal = true;
        } else {
            o << "          This computer cannot reach Telegram. Is the internet working?\n"
              << "          If Telegram is blocked on your network, turn on a VPN that covers the whole\n"
              << "          computer, or set \"proxy\" in config.json (see README, Troubleshooting).\n";
        }
        report = o.str();
        return false;
    }
    long long botId = me["result"].value("id", 0LL);
    o << "[OK]      Bot token works (@" << me["result"].value("username", string("?")) << ").\n";

    json ch = apiCall(c, "getChat", {{"chat_id", c.channelId}});
    if (!ch.value("ok", false)) {
        o << "[PROBLEM] Cannot find the channel " << c.channelId << ": " << errorText(ch) << "\n"
          << "          Check channel_id, and add the bot to the channel as an administrator.\n";
        ok = false;
    } else {
        o << "[OK]      Channel found: " << ch["result"].value("title", string("?")) << "\n";
        json mem = apiCall(c, "getChatMember", {{"chat_id", c.channelId}, {"user_id", to_string(botId)}});
        if (!mem.value("ok", false)) {
            o << "[PROBLEM] Cannot read the bot's rights in the channel: " << errorText(mem) << "\n";
            ok = false;
        } else {
            string status = mem["result"].value("status", string());
            bool canPost = mem["result"].value("can_post_messages", false);
            if (status == "creator" || (status == "administrator" && canPost)) {
                o << "[OK]      The bot is an administrator and may post messages.\n";
            } else if (status == "administrator") {
                o << "[PROBLEM] The bot is an administrator but \"Post Messages\" is switched off.\n";
                ok = false;
            } else {
                o << "[PROBLEM] The bot is not an administrator of the channel (status: " << status << ").\n";
                ok = false;
            }
        }
    }

    json me2 = apiCall(c, "getChat", {{"chat_id", to_string(c.ownerId)}});
    if (!me2.value("ok", false)) {
        o << "[PROBLEM] The bot cannot write to you (owner_id " << c.ownerId << "): " << errorText(me2) << "\n"
          << "          Open your bot in Telegram and press START, then check owner_id.\n";
        ok = false;
    } else {
        o << "[OK]      The bot can send messages to you.\n";
    }

    report = o.str();
    return ok;
}

static bool fireSwitch(const Config& c, State& st) {
    const long long total = (c.message.empty() ? 0 : 1) + (long long)c.files.size();

    while (st.sentParts < total) {
        json r;
        if (!c.message.empty() && st.sentParts == 0) {
            r = apiCall(c, "sendMessage", {{"chat_id", c.channelId}, {"text", c.message}});
        } else {
            size_t fileIndex = (size_t)(st.sentParts - (c.message.empty() ? 0 : 1));
            r = apiCall(c, "sendDocument", {{"chat_id", c.channelId}}, c.files[fileIndex], "document", 300);
        }
        if (!r.value("ok", false)) {
            logLine("Publishing failed (will retry in 60 seconds): " + errorText(r));
            return false;
        }
        st.sentParts++;
        saveState(st);
    }

    st.sent = true;
    saveState(st);
    logLine("PUBLISHED: everything was posted to the channel.");
    tell(c, "\xF0\x9F\x9A\xA8 The timer ran out. Your content was published to the channel.");
    return true;
}

// Sends the same content to the owner's private chat, as a rehearsal.
static void sendTest(const Config& c) {
    if (!c.message.empty()) tell(c, "[TEST] This text would be posted first:\n\n" + c.message);
    for (const auto& f : c.files) {
        json r = apiCall(c, "sendDocument", {{"chat_id", to_string(c.ownerId)}}, f, "document", 300);
        if (!r.value("ok", false)) {
            tell(c, "[TEST] Could not send " + f + ": " + errorText(r));
            return;
        }
    }
    tell(c, "[TEST] Done. This is exactly what the channel would receive.");
}


static string statusText(const State& st) {
    long long now = nowSec();
    long long deadline = st.lastCheckin + st.intervalSec;
    ostringstream o;
    if (st.sent) {
        o << "\xF0\x9F\x9A\xA8 The switch already fired and the content was published.\n"
          << "Send /resume <password> to arm it again.";
    } else if (st.paused) {
        o << "\xE2\x8F\xB8 Paused. The timer is not running.";
    } else {
        o << "\xE2\x8F\xB3 Time left: " << fmtDur(deadline - now) << "\n"
          << "Deadline: " << fmtTime(deadline) << " (computer time)";
    }
    o << "\n\xE2\x9A\x99\xEF\xB8\x8F Timer length: " << fmtDur(st.intervalSec)
      << "\n\xF0\x9F\x94\x94 Warning: "
      << (st.warnBeforeSec > 0 ? fmtDur(st.warnBeforeSec) + " before the deadline" : "off");
    return o.str();
}

static const char* HELP_TEXT =
    "Commands (they only work in your private chat with me):\n\n"
    "<your password>  -> check in, the timer restarts\n"
    "/status  -> show the time left\n"
    "/check  -> test the channel and my rights\n"
    "/settime <time> <password>  -> change the timer length and restart it\n"
    "      examples: 90m, 12h, 3d, 1.5d\n"
    "/setwarn <time|off> <password>  -> when to warn you before the deadline\n"
    "/pause <password>  -> stop the timer\n"
    "/resume <password>  -> start again (also re-arms after publishing)\n"
    "/test <password>  -> send the content to YOU only, as a rehearsal\n"
    "/sendnow <password>  -> publish to the channel right now\n\n"
    "Messages that contain the password are deleted automatically.";

static void handleMessage(const Config& c, State& st, const json& msg) {
    if (!msg.contains("chat") || !msg.contains("text")) return;
    const json& chat = msg["chat"];

    if (chat.value("type", "") != "private" || chat.value("id", 0LL) != c.ownerId) return;

    string text = trim(msg.value("text", string()));
    if (text.empty()) return;
    long long msgId = msg.value("message_id", 0LL);
    long long now = nowSec();

    vector<string> tok;
    {
        istringstream is(text);
        string t;
        while (is >> t) tok.push_back(t);
    }

    if (safeEq(text, c.password)) {
        if (st.sent) {
            tell(c, "The switch already fired. Send /resume <password> to arm it again.");
        } else if (st.paused) {
            tell(c, "The timer is paused. Send /resume <password> to start it.");
        } else {

            st.lastCheckin = now;
            st.lastWarn = 0;
            saveState(st);
            logLine("Check-in received.");
            tell(c, "\xE2\x9C\x85 Password correct. The timer was reset.\n\n" + statusText(st));
        }
        deleteMsg(c, msgId);
        return;
    }

    if (tok.empty() || tok[0][0] != '/') return;

    string cmd = tok[0];
    size_t at = cmd.find('@');
    if (at != string::npos) cmd = cmd.substr(0, at);

    if (cmd == "/start" || cmd == "/help") { tell(c, HELP_TEXT); return; }
    if (cmd == "/status") { tell(c, statusText(st)); return; }
    if (cmd == "/check") {
        string report;
        bool fatal = false;
        selfCheck(c, report, fatal);
        tell(c, report);
        return;
    }


    const bool known = (cmd == "/settime" || cmd == "/setwarn" || cmd == "/pause" ||
                        cmd == "/resume" || cmd == "/test" || cmd == "/sendnow");
    if (!known) { tell(c, "Unknown command. Send /help to see all commands."); return; }

    deleteMsg(c, msgId);
    if (tok.size() < 2 || !safeEq(tok.back(), c.password)) {
        tell(c, "\xE2\x9D\x8C Wrong or missing password. Put the password as the last word.");
        return;
    }

    if (cmd == "/settime") {
        if (tok.size() != 3) { tell(c, "Usage: /settime 48h <password>"); return; }
        long long sec = parseDuration(tok[1]);
        if (sec < 0) { tell(c, "Invalid time. Use between 1m and 365d, for example 90m, 12h or 3d."); return; }
        st.intervalSec = sec;
        st.lastCheckin = now;
        st.lastWarn = 0;
        st.paused = false;
        saveState(st);
        logLine("Timer length changed to " + fmtDur(sec));
        tell(c, "\xE2\x9C\x85 Timer length changed and the timer was reset.\n\n" + statusText(st));
    } else if (cmd == "/setwarn") {
        if (tok.size() != 3) { tell(c, "Usage: /setwarn 4h <password>  or  /setwarn off <password>"); return; }
        if (tok[1] == "off") {
            st.warnBeforeSec = 0;
        } else {
            long long sec = parseDuration(tok[1]);
            if (sec < 0) { tell(c, "Invalid time. Example: 4h"); return; }
            st.warnBeforeSec = sec;
        }
        st.lastWarn = 0;
        saveState(st);
        tell(c, "\xE2\x9C\x85 Saved.\n\n" + statusText(st));
    } else if (cmd == "/pause") {
        st.paused = true;
        saveState(st);
        logLine("Paused by owner.");
        tell(c, "\xE2\x8F\xB8 Paused. Nothing will be published until you send /resume.");
    } else if (cmd == "/resume") {
        st.paused = false;
        st.sent = false;
        st.sentParts = 0;
        st.lastCheckin = now;
        st.lastWarn = 0;
        saveState(st);
        logLine("Resumed by owner.");
        tell(c, "\xE2\x96\xB6\xEF\xB8\x8F Armed. The timer was reset.\n\n" + statusText(st));
    } else if (cmd == "/test") {
        sendTest(c);
    } else if (cmd == "/sendnow") {
        if (st.sent) { tell(c, "Already published."); return; }
        logLine("Owner asked to publish now.");
        if (!fireSwitch(c, st)) tell(c, "\xE2\x9D\x8C Publishing failed. Send /check to see what is wrong.");
    }
}

static bool pollUpdates(const Config& c, State& st, int timeoutSec) {
    static bool failing = false;

    json r = apiCall(c, "getUpdates",
                     {{"offset", to_string(st.lastUpdateId + 1)},
                      {"timeout", to_string(timeoutSec)},
                      {"allowed_updates", "[\"message\"]"}},
                     "", "", timeoutSec + 15);

    if (!r.value("ok", false)) {
        if (!failing) {  // log once, not every few seconds
            logLine("Cannot read messages from Telegram: " + errorText(r));
            if (r.value("error_code", 0) == 409)
                logLine("Hint: another copy of this program (or another program) uses the same bot token. Only one may run.");
            failing = true;
        }
        return false;
    }
    if (failing) {
        logLine("Connection to Telegram is back.");
        failing = false;
    }

    for (const auto& u : r["result"]) {
        st.lastUpdateId = u.value("update_id", st.lastUpdateId);
        if (u.contains("message")) handleMessage(c, st, u["message"]);
        saveState(st);
    }
    return true;
}


static void printUsage() {
    cout << "Telegram Dead Man's Switch " << VERSION << "\n\n"
         << "Usage: deadswitch [options]\n\n"
         << "  --config <file>   config file (default: config.json)\n"
         << "  --state <file>    state file  (default: state.json)\n"
         << "  --check           test the settings and Telegram access, then exit\n"
         << "  --version         show the version\n"
         << "  --help            show this help\n\n"
         << "Environment variables (override the config file):\n"
         << "  DEADSWITCH_BOT_TOKEN, DEADSWITCH_OWNER_ID,\n"
         << "  DEADSWITCH_CHANNEL_ID, DEADSWITCH_PASSWORD, DEADSWITCH_PROXY\n";
}

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    bool checkOnly = false;
    for (int i = 1; i < argc; ++i) {
        string a = argv[i];
        if (a == "--help" || a == "-h") { printUsage(); return 0; }
        else if (a == "--version") { cout << VERSION << "\n"; return 0; }
        else if (a == "--check") { checkOnly = true; }
        else if (a == "--config" && i + 1 < argc) { g_configPath = argv[++i]; }
        else if (a == "--state" && i + 1 < argc) { g_statePath = argv[++i]; }
        else { cerr << "Unknown option: " << a << "\n\n"; printUsage(); return 2; }
    }

    signal(SIGINT, onSignal);
    signal(SIGTERM, onSignal);
    curl_global_init(CURL_GLOBAL_DEFAULT);

    Config cfg;
    if (!loadConfig(cfg) || !validateConfig(cfg)) return 1;

    string report;
    bool fatal = false;
    bool healthy = selfCheck(cfg, report, fatal);
    cout << report;

    if (checkOnly) {
        cout << (healthy ? "\nEverything looks good.\n" : "\nFix the problems above and run --check again.\n");
        return healthy ? 0 : 1;
    }
    if (fatal) return 1;


    apiCall(cfg, "deleteWebhook", {});

    State st = loadState(cfg);
    saveState(st);

    logLine("Dead man's switch is running.");
    logLine(string("Timer length: ") + fmtDur(st.intervalSec) +
            (st.paused ? " (paused)" : "") + (st.sent ? " (already fired)" : ""));
    if (!healthy) logLine("WARNING: the start-up check found problems (see above). Fix them or publishing may fail.");

    tell(cfg, string("\xF0\x9F\xA4\x96 Dead man's switch started.\n\n") + statusText(st) +
                  (healthy ? "" : "\n\n\xE2\x9A\xA0\xEF\xB8\x8F Problems found:\n" + report) +
                  "\n\nSend /help to see the commands.");

    while (g_running) {
        long long now = nowSec();

        if (!st.paused && !st.sent) {
            long long remaining = st.lastCheckin + st.intervalSec - now;

            if (remaining <= 0) {

                pollUpdates(cfg, st, 0);
                remaining = st.lastCheckin + st.intervalSec - nowSec();
                if (!st.paused && !st.sent && remaining <= 0) {
                    logLine("Time is up. Publishing...");
                    if (!fireSwitch(cfg, st)) {
                        for (int i = 0; i < 60 && g_running; ++i) this_thread::sleep_for(chrono::seconds(1));
                    }
                    continue;
                }
            } else if (st.warnBeforeSec > 0 && remaining <= st.warnBeforeSec) {
                long long every = max(300LL, min(3600LL, st.warnBeforeSec / 3));
                if (now - st.lastWarn >= every) {
                    tell(cfg, "\xE2\x9A\xA0\xEF\xB8\x8F Warning! " + fmtDur(remaining) +
                                  " left before your content is published.\nSend your password now to reset the timer.");
                    st.lastWarn = now;
                    saveState(st);
                }
            }
        }

        if (!pollUpdates(cfg, st, POLL_TIMEOUT_SEC)) {
            for (int i = 0; i < 5 && g_running; ++i) this_thread::sleep_for(chrono::seconds(1));
        }
    }

    logLine("Stopped.");
    curl_global_cleanup();
    return 0;
}
