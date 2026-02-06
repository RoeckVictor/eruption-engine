#include "EditorScene.h"
#include "engine/core/Engine.h"

namespace editor {

engine::Result<void, engine::ErrorInfo> EditorScene::on_enter(engine::Engine& /*engine*/) {
    // Editor scene initialization
    // No systems needed - the editor UI is handled by EditorApplication
    return engine::Ok();
}

void EditorScene::on_exit(engine::Engine& /*engine*/) {
    // Editor scene cleanup
}

} // namespace editor
