// ROXBOX - logging
#pragma once
#include <string>
#include <cstdio>

namespace rb {

enum class LogLevel { Debug = 0, Info, Warn, Error };

void logSetLevel(LogLevel lv);
LogLevel logGetLevel();
// logs to stderr and (optionally) to a file
void logSetFile(const std::string& path);
void logWrite(LogLevel lv, const char* fmt, ...);

} // namespace rb

#define RB_LOG_D(...) rb::logWrite(rb::LogLevel::Debug, __VA_ARGS__)
#define RB_LOG_I(...) rb::logWrite(rb::LogLevel::Info, __VA_ARGS__)
#define RB_LOG_W(...) rb::logWrite(rb::LogLevel::Warn, __VA_ARGS__)
#define RB_LOG_E(...) rb::logWrite(rb::LogLevel::Error, __VA_ARGS__)
