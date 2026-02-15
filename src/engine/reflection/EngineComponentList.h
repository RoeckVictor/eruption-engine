#pragma once

/// Central list of all engine component types.
///
/// To add a new engine component, add a single X(Type) entry below.
/// It will automatically be registered for:
///   - TypeRegistry (reflection / inspector UI)      — ReflectionInit.cpp
///   - ComponentRegistry (prefab deserialization)     — EngineComponentRegistry.cpp
///   - ComponentTypeRegistry (dynamic ECS access)     — EditorComponents.cpp
///
/// You still need to define REFLECT_TYPE_BEGIN/END for the new type
/// in ReflectionInit.cpp to declare its properties.

#define ENGINE_COMPONENT_LIST(X) \
    X(engine::Transform) \
    X(engine::render::Camera2D) \
    X(engine::animation::Animator) \
    X(engine::simulation::PixelGridComponent) \
    X(engine::render::PixelGridRenderer) \
    X(engine::physics::Rigidbody) \
    X(engine::physics::BoxCollider) \
    X(engine::physics::CapsuleCollider) \
    X(engine::physics::CircleCollider) \
    X(engine::physics::DynamicCollider) \
    X(engine::gameplay::PlayerController) \
    X(engine::gameplay::CameraFollower) \
    X(engine::simulation::SimSurface)
