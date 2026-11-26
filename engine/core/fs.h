// ROXBOX - filesystem helpers (no external deps)
#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace rb::fs {

bool exists(const std::string& path);
bool readBytes(const std::string& path, std::vector<uint8_t>& out);
bool writeBytes(const std::string& path, const uint8_t* data, size_t size);
bool writeBytes(const std::string& path, const std::vector<uint8_t>& data);
bool ensureDir(const std::string& path);
std::string join(const std::string& a, const std::string& b);
// directory of the running executable (falls back to cwd)
std::string executableDir();
std::string cwd();

} // namespace rb::fs
