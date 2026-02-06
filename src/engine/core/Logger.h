#pragma once

#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <cstdarg>

namespace engine {

/// Log severity levels.
enum class LogLevel {
    Info,
    Warning,
    Error
};

/// A log entry containing all information about a log message.
struct LogEntry {
    LogLevel level;
    std::string tag;
    std::string message;
    // Could add timestamp, source file/line, etc.
};

/// Function type for log sinks.
/// Sinks receive log entries and can process them (print, store, send to UI, etc.)
using LogSink = std::function<void(const LogEntry&)>;

/// Global logger with support for multiple sinks.
/// Thread-safe for adding sinks and logging.
class Logger {
public:
    /// Get the global logger instance.
    static Logger& instance();

    /// Add a log sink. Returns an ID that can be used to remove it.
    size_t add_sink(LogSink sink);

    /// Remove a log sink by ID.
    void remove_sink(size_t id);

    /// Clear all sinks.
    void clear_sinks();

    /// Log a message.
    void log(LogLevel level, const char* tag, const char* fmt, ...);

    /// Log a message (va_list version).
    void log_v(LogLevel level, const char* tag, const char* fmt, va_list args);

    /// Convenience methods.
    void info(const char* tag, const char* fmt, ...);
    void warning(const char* tag, const char* fmt, ...);
    void error(const char* tag, const char* fmt, ...);

private:
    Logger();

    struct SinkEntry {
        size_t id;
        LogSink sink;
    };

    std::vector<SinkEntry> m_sinks;
    std::mutex m_mutex;
    size_t m_next_id = 1;
};

/// Default console sink that prints to stdout/stderr.
void default_console_sink(const LogEntry& entry);

} // namespace engine

// Convenience macros that use the global logger
#define ENGINE_LOG_INFO(fmt, ...) ::engine::Logger::instance().info("Engine", fmt, ##__VA_ARGS__)
#define ENGINE_LOG_WARN(fmt, ...) ::engine::Logger::instance().warning("Engine", fmt, ##__VA_ARGS__)
#define ENGINE_LOG_ERROR(fmt, ...) ::engine::Logger::instance().error("Engine", fmt, ##__VA_ARGS__)

// Keep backward compatibility with existing macros
#define ENGINE_LOG(fmt, ...) ENGINE_LOG_INFO(fmt, ##__VA_ARGS__)
#define ENGINE_ERR(fmt, ...) ENGINE_LOG_ERROR(fmt, ##__VA_ARGS__)
