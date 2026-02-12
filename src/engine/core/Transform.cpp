#include "Transform.h"

namespace engine {

// Note: update_world_transforms is declared in the header but implemented
// in editor code (editor/core/EditorComponents.cpp) since it requires access
// to editor::Hierarchy component. The function works with engine::Transform
// but needs the hierarchy structure that's editor-specific.

} // namespace engine
