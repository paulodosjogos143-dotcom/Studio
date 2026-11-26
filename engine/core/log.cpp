#include "log.h"
#include <cstdarg>
#include <ctime>
#include <mutex>
#include <cstring>

namespace rb {
namespace {
LogLevel g_level = LogLevel::Info;
std::mutex g_mutex;
FILE* g_file = nullptr;
std::string g_filePath;

const char* levelName(LogLevel lv) {
    switch (lv) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO ";
        case LogLevel::Warn:  return "WARN ";
        case LogLevel::Error: return "ERROR";
    }
    return "?????";
}

void writeToFile(const std::string& line) {
    if (g_file) {
        std::fputs(line.c_str(), g_file);
        std::fputc('\n', g_file);
        std::fflush(g_file);
    }
}
} // namespace

void logSetLevel(LogLevel lv) { g_level = lv; }
LogLevel logGetLevel() { return g_level; }

void logSetFile(const std::string& path) {
    std::lock_guard<std::mutex> lk(g_mutex);
    if (g_file) { std::fclose(g_file); g_file = nullptr; }
    if (!path.empty()) {
        g_file = std::fopen(path.c_str(), "w");
        g_filePath = path;
    }
}

void logWrite(LogLevel lv, const char* fmt, ...) {
    if (lv < g_level) return;
    char body[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(body, sizeof(body), fmt, ap);
    va_end(ap);

    std::time_t t = std::time(nullptr);
    char tstr[32];
    std::strftime(tstr, sizeof(tstr), "%H:%M:%S", std::localtime(&t));

    char line[1100];
    snprintf(line, sizeof(line), "[%s][%s] %s", tstr, levelName(lv), body);

    std::lock_guard<std::mutex> lk(g_mutex);
    std::fprintf(stderr, "%s\n", line);
    writeToFile(line);
}

} // namespace rb
