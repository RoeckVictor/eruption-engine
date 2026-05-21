#include "AnimationInit.h"
#include "PropertyResolver.h"
#include "engine/reflection/EngineComponentList.h"
#include "engine/core/Transform.h"
#include "engine/core/ScreenRect.h"
#include "engine/render/Camera2D.h"
#include "engine/render/Image.h"
#include "engine/render/Text.h"
#include "engine/render/PixelGridRenderer.h"
#include "engine/animation/Animator.h"
#include "engine/simulation/PixelGridComponent.h"
#include "engine/physics/Rigidbody.h"
#include "engine/physics/Colliders.h"
#include "engine/simulation/SimSurface.h"
#include "engine/ui/UIComponents.h"
#include "engine/audio/AudioSource.h"
#include "engine/audio/AudioListener.h"
#include "engine/core/Logger.h"

namespace engine::animation {

// Helper to extract short name from full type name
// "engine::Transform" -> "Transform"
static std::string extract_short_name(const std::string& full_name) {
    size_t last_colon = full_name.rfind("::");
    if (last_colon != std::string::npos) {
        return full_name.substr(last_colon + 2);
    }
    return full_name;
}

void init_animation_property_resolver() {
    engine::Logger::instance().info("Animation", "Initializing property resolver...");

    auto& resolver = PropertyResolver::instance();

    // Register all engine components using the central list
    // We use short names for property paths (e.g., "Transform.x" not "engine::Transform.x")
    // but also pass the full type name for TypeRegistry lookups
    #define REGISTER_COMPONENT(T) \
        resolver.register_component<T>(extract_short_name(#T), #T);
    ENGINE_COMPONENT_LIST(REGISTER_COMPONENT)
    #undef REGISTER_COMPONENT

    auto components = resolver.get_registered_components();
    engine::Logger::instance().info("Animation", "Property resolver initialized. Components: %zu", components.size());
}

} // namespace engine::animation
