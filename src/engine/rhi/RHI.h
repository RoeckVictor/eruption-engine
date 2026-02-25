#pragma once

/// @file RHI.h
/// @brief Main include file for the Render Hardware Interface (RHI)
///
/// The RHI provides an abstraction layer over graphics APIs (OpenGL, Vulkan, etc.)
/// to allow the engine to support multiple backends without changing higher-level code.
///
/// Usage:
/// @code
/// #include "engine/rhi/RHI.h"
///
/// // Create device (usually done once at startup)
/// auto device = engine::rhi::create_rhi_device(engine::rhi::Backend::OpenGL);
///
/// // Create resources
/// auto buffer = device->create_buffer({...});
/// auto texture = device->create_texture({...});
/// auto shader = device->create_graphics_shader({"shader.vert", "shader.frag"});
///
/// // Render
/// auto* ctx = device->context();
/// ctx->begin_frame();
/// ctx->clear(0.1f, 0.1f, 0.1f, 1.0f);
/// ctx->bind_pipeline(pipeline.get());
/// ctx->bind_vertex_buffer(buffer.get());
/// ctx->draw(vertex_count);
/// ctx->end_frame();
/// @endcode

#include "RHITypes.h"
#include "RHIBuffer.h"
#include "RHITexture.h"
#include "RHIShader.h"
#include "RHIPipeline.h"
#include "RHIFramebuffer.h"
#include "RHIContext.h"
#include "RHIDevice.h"
