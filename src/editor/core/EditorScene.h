#pragma once

#include "engine/scene/Scene.h"

namespace editor {

class EditorScene : public engine::scene::Scene {
public:
    EditorScene() = default;

    const char* name() const override { return "EditorScene"; }

    engine::Result<void, engine::ErrorInfo> on_enter(engine::Engine& engine) override;
    void on_exit(engine::Engine& engine) override;
};

}
