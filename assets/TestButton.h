#pragma once

#include "runtime/ComponentScript.h"

/// Simple test script for UI buttons.
/// Attach this to a button entity to log click events.
class TestButton : public runtime::ComponentScript {
public:
    const char* type_name() const override { return "TestButton"; }

    void on_create() override;

    // UI callbacks
    void on_button_click(entt::entity button) override;
    void on_button_press(entt::entity button) override;
    void on_button_release(entt::entity button) override;
    void on_pointer_enter(entt::entity ui_element) override;
    void on_pointer_exit(entt::entity ui_element) override;

private:
    int m_click_count = 0;
};
