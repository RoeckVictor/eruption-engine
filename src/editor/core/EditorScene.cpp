#include "EditorScene.h"
#include "engine/core/Engine.h"

namespace editor {

engine::Result<void, engine::ErrorInfo> EditorScene::on_enter(engine::Engine& /*engine*/) {
    return engine::Ok();
}

void EditorScene::on_exit(engine::Engine& /*engine*/) {
}

}
