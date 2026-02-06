#pragma once

// Include the new Logger system
#include "engine/core/Logger.h"

// Legacy macros for backward compatibility
// These now route through the Logger system
#define LOG_MSG(tag, fmt, ...) ::engine::Logger::instance().info(tag, fmt, ##__VA_ARGS__)
#define LOG_ERR(tag, fmt, ...) ::engine::Logger::instance().error(tag, fmt, ##__VA_ARGS__)

// ENGINE_LOG and ENGINE_ERR are defined in Logger.h
