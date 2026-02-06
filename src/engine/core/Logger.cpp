#include "Logger.h"
#include <cstdio>
#include <cstring>

namespace engine {

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

Logger::Logger() {
    // Add default console sink
    add_sink(default_console_sink);
}

size_t Logger::add_sink(LogSink sink) {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t id = m_next_id++;
    m_sinks.push_back({id, std::move(sink)});
    return id;
}

void Logger::remove_sink(size_t id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sinks.erase(
        std::remove_if(m_sinks.begin(), m_sinks.end(),
            [id](const SinkEntry& e) { return e.id == id; }),
        m_sinks.end()
    );
}

void Logger::clear_sinks() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sinks.clear();
}

void Logger::log(LogLevel level, const char* tag, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_v(level, tag, fmt, args);
    va_end(args);
}

void Logger::log_v(LogLevel level, const char* tag, const char* fmt, va_list args) {
    // Format the message
    char buffer[2048];
    vsnprintf(buffer, sizeof(buffer), fmt, args);

    LogEntry entry;
    entry.level = level;
    entry.tag = tag ? tag : "";
    entry.message = buffer;

    // Dispatch to all sinks
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& sink_entry : m_sinks) {
        sink_entry.sink(entry);
    }
}

void Logger::info(const char* tag, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_v(LogLevel::Info, tag, fmt, args);
    va_end(args);
}

void Logger::warning(const char* tag, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_v(LogLevel::Warning, tag, fmt, args);
    va_end(args);
}

void Logger::error(const char* tag, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_v(LogLevel::Error, tag, fmt, args);
    va_end(args);
}

void default_console_sink(const LogEntry& entry) {
    const char* level_str = "";
    FILE* output = stdout;

    switch (entry.level) {
        case LogLevel::Info:
            level_str = "";
            break;
        case LogLevel::Warning:
            level_str = "WARNING: ";
            output = stderr;
            break;
        case LogLevel::Error:
            level_str = "ERROR: ";
            output = stderr;
            break;
    }

    fprintf(output, "[%s] %s%s\n", entry.tag.c_str(), level_str, entry.message.c_str());
}

} // namespace engine
