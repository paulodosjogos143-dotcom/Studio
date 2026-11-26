#include "fs.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined(_WIN32)
#  include <direct.h>
#  include <windows.h>
#  define RB_ACCESS _access
#  define RB_MKDIR  _mkdir
#else
#  include <unistd.h>
#  include <sys/stat.h>
#  define RB_ACCESS access
#  define RB_MKDIR  mkdir
#endif

namespace rb::fs {

bool exists(const std::string& path) {
    return RB_ACCESS(path.c_str(), 0) == 0;
}

bool readBytes(const std::string& path, std::vector<uint8_t>& out) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (sz < 0) { std::fclose(f); return false; }
    out.resize((size_t)sz);
    size_t got = std::fread(out.data(), 1, (size_t)sz, f);
    std::fclose(f);
    return got == (size_t)sz;
}

bool writeBytes(const std::string& path, const uint8_t* data, size_t size) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    size_t put = std::fwrite(data, 1, size, f);
    std::fclose(f);
    return put == size;
}

bool writeBytes(const std::string& path, const std::vector<uint8_t>& data) {
    return writeBytes(path, data.data(), data.size());
}

bool ensureDir(const std::string& path) {
    if (path.empty()) return true;
    if (exists(path)) return true;
    std::string partial;
    size_t i = 0;
    while (i < path.size()) {
        size_t sep = path.find_first_of("/\\", i);
        if (sep == std::string::npos) sep = path.size();
        partial = path.substr(0, sep);
        if (!partial.empty() && !exists(partial)) {
#if defined(_WIN32)
            if (RB_MKDIR(partial.c_str()) != 0) return false;
#else
            if (RB_MKDIR(partial.c_str(), 0755) != 0) return false;
#endif
        }
        i = sep + 1;
    }
    return exists(path);
}

std::string join(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    char last = a.back();
    if (last == '/' || last == '\\') return a + b;
    return a + "/" + b;
}

std::string executableDir() {
    char buf[4096];
#if defined(_WIN32)
    size_t n = GetModuleFileNameA(nullptr, buf, sizeof(buf) - 1);
    if (n == 0 || n >= sizeof(buf)) return cwd();
    buf[n] = 0;
    std::string p(buf);
    size_t sep = p.find_last_of("\\/");
    return sep == std::string::npos ? "." : p.substr(0, sep);
#else
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = 0;
        std::string p(buf);
        size_t sep = p.find_last_of('/');
        return sep == std::string::npos ? "." : p.substr(0, sep);
    }
    return cwd();
#endif
}

std::string cwd() {
    char buf[4096];
#if defined(_WIN32)
    if (_getcwd(buf, sizeof(buf) - 1)) return std::string(buf);
#else
    if (getcwd(buf, sizeof(buf) - 1)) return std::string(buf);
#endif
    return ".";
}

} // namespace rb::fs
