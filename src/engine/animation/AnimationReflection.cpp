#include "engine/animation/Animator.h"
#include "engine/reflection/ReflectionMacros.h"

// Register Animator component for reflection
// Note: This provides minimal property reflection. The clips map, current_clip string,
// and other complex state will need custom inspector UI (Phase 7).
REFLECT_TYPE_BEGIN(engine::animation::Animator)
    // Reflect enabled and simple bool properties
    REFLECT_PROPERTY(enabled, "Enabled")
    REFLECT_PROPERTY(playing, "Playing")
REFLECT_TYPE_END()

// Register the type at static initialization
// Using anonymous namespace to avoid variable name conflicts with ::
namespace {
    static engine::reflection::TypeRegistrar<engine::animation::Animator> s_animator_registrar;
}
