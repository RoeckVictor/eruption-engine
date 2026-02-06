#pragma once

#include <string>

namespace engine {

/// Common error types for the engine.
/// Systems can extend this with their own error types if needed.
enum class EngineError {
    // General errors
    Success = 0,
    Unknown,
    InitFailed,
    NotInitialized,
    AlreadyInitialized,
    InvalidArgument,
    NullPointer,
    OutOfMemory,

    // File system errors
    FileNotFound,
    FileReadError,
    FileWriteError,
    InvalidPath,

    // Asset errors
    AssetNotFound,
    AssetLoadFailed,
    AssetTypeMismatch,
    InvalidAssetFormat,

    // Graphics errors
    ShaderCompileFailed,
    ShaderLinkFailed,
    TextureLoadFailed,
    BufferCreateFailed,

    // Physics errors
    PhysicsWorldInvalid,
    BodyCreateFailed,
    ShapeCreateFailed,

    // Parsing errors
    JsonParseFailed,
    InvalidFormat,
};

/// Convert an error code to a human-readable string.
inline const char* error_to_string(EngineError error) {
    switch (error) {
        case EngineError::Success: return "Success";
        case EngineError::Unknown: return "Unknown error";
        case EngineError::InitFailed: return "Initialization failed";
        case EngineError::NotInitialized: return "Not initialized";
        case EngineError::AlreadyInitialized: return "Already initialized";
        case EngineError::InvalidArgument: return "Invalid argument";
        case EngineError::NullPointer: return "Null pointer";
        case EngineError::OutOfMemory: return "Out of memory";

        case EngineError::FileNotFound: return "File not found";
        case EngineError::FileReadError: return "File read error";
        case EngineError::FileWriteError: return "File write error";
        case EngineError::InvalidPath: return "Invalid path";

        case EngineError::AssetNotFound: return "Asset not found";
        case EngineError::AssetLoadFailed: return "Asset load failed";
        case EngineError::AssetTypeMismatch: return "Asset type mismatch";
        case EngineError::InvalidAssetFormat: return "Invalid asset format";

        case EngineError::ShaderCompileFailed: return "Shader compile failed";
        case EngineError::ShaderLinkFailed: return "Shader link failed";
        case EngineError::TextureLoadFailed: return "Texture load failed";
        case EngineError::BufferCreateFailed: return "Buffer create failed";

        case EngineError::PhysicsWorldInvalid: return "Physics world invalid";
        case EngineError::BodyCreateFailed: return "Body create failed";
        case EngineError::ShapeCreateFailed: return "Shape create failed";

        case EngineError::JsonParseFailed: return "JSON parse failed";
        case EngineError::InvalidFormat: return "Invalid format";

        default: return "Unknown error code";
    }
}

/// Detailed error information with context.
struct ErrorInfo {
    EngineError code;
    std::string message;
    std::string context;

    ErrorInfo() : code(EngineError::Success) {}

    explicit ErrorInfo(EngineError error_code)
        : code(error_code)
        , message(error_to_string(error_code))
    {}

    ErrorInfo(EngineError error_code, std::string msg)
        : code(error_code)
        , message(std::move(msg))
    {}

    ErrorInfo(EngineError error_code, std::string msg, std::string ctx)
        : code(error_code)
        , message(std::move(msg))
        , context(std::move(ctx))
    {}

    /// Get full error description including context.
    std::string full_message() const {
        if (context.empty()) {
            return message;
        }
        return message + " (context: " + context + ")";
    }
};

} // namespace engine
