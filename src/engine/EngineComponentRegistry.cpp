#include "EngineComponentRegistry.h"
#include "engine/prefab/ComponentRegistry.h"
#include "engine/reflection/ReflectionSerializer.h"
#include "engine/reflection/EngineComponentList.h"
#include "engine/core/Transform.h"
#include "engine/render/Camera2D.h"
#include "engine/render/PixelGridRenderer.h"
#include "engine/animation/Animator.h"
#include "engine/simulation/PixelGridComponent.h"
#include "engine/simulation/SimSurface.h"
#include "engine/physics/Rigidbody.h"
#include "engine/physics/Colliders.h"
#include "engine/gameplay/PlayerController.h"
#include "engine/gameplay/CameraFollower.h"

namespace engine {

/// Helper: register a component whose JSON deserialization is fully
/// covered by the reflection property list (the common case).
template<typename T>
static void register_reflected(prefab::ComponentRegistry& registry, const char* name) {
    registry.register_component<T>(name, [](const nlohmann::json& j) {
        return reflection::deserialize_as<T>(j);
    });
}

void register_engine_components(engine::prefab::ComponentRegistry& registry) {
    // Register all engine components from the central list (EngineComponentList.h)
    #define REGISTER_PREFAB(T) register_reflected<T>(registry, #T);
    ENGINE_COMPONENT_LIST(REGISTER_PREFAB)
    #undef REGISTER_PREFAB
}

} // namespace engine
