// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

enum class LogLevel : uint8_t {
    Debug,
    Info,
    Warn,
    Error
};

// Platform-specific logging backend (OutputDebugString on Windows, os_log on
// macOS, stderr/journald on Linux). Engine code should call this through a thin
// macro wrapper defined in engine/ that injects __FILE__ and __LINE__ and
// formats the message into a stack buffer (no heap allocation at call sites).
class ILogger {
public:
    virtual ~ILogger() = default;

    // file and message are const char* to avoid string allocation on any call path.
    virtual void log(LogLevel level, const char* file, int line,
                     const char* message) = 0;

    // Suppresses messages below minLevel (e.g. set Info in release builds).
    virtual void setMinLevel(LogLevel minLevel) = 0;
};
