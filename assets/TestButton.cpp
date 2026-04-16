#include "TestButton.h"
#include <string>

// Register the script with the engine
REGISTER_COMPONENT_SCRIPT(TestButton)

void TestButton::on_create() {
    log("TestButton: Script attached - ready to receive button events!");
}

void TestButton::on_button_click(entt::entity button) {
    (void)button;
    m_click_count++;
    std::string msg = "TestButton: Button CLICKED! (total clicks: " + std::to_string(m_click_count) + ")";
    log(msg.c_str());
}

void TestButton::on_button_press(entt::entity button) {
    (void)button;
    log("TestButton: Button PRESSED (mouse down)");
}

void TestButton::on_button_release(entt::entity button) {
    (void)button;
    log("TestButton: Button RELEASED (mouse up)");
}

void TestButton::on_pointer_enter(entt::entity ui_element) {
    (void)ui_element;
    log("TestButton: Pointer ENTERED button area");
}

void TestButton::on_pointer_exit(entt::entity ui_element) {
    (void)ui_element;
    log("TestButton: Pointer EXITED button area");
}
