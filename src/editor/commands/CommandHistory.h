#pragma once

#include "Command.h"
#include <vector>
#include <memory>
#include <functional>

namespace editor {

class CommandHistory {
public:
    CommandHistory() = default;

    void execute(std::unique_ptr<Command> cmd);

    // Add an already-executed command to the history.
    // Use this when changes have already been applied (e.g., by gizmo manipulation).
    void add_executed(std::unique_ptr<Command> cmd);

    void undo();
    void redo();
    bool can_undo() const { return !m_undo_stack.empty(); }
    bool can_redo() const { return !m_redo_stack.empty(); }

    // Get the name of the command that would be undone.
    std::string undo_name() const;
    // Get the name of the command that would be redone.
    std::string redo_name() const;

    void clear();

    // Mark the current state as the "saved" state.
    void mark_saved();

    bool is_dirty() const;

    const std::vector<std::unique_ptr<Command>>& undo_stack() const { return m_undo_stack; }
    const std::vector<std::unique_ptr<Command>>& redo_stack() const { return m_redo_stack; }

    void set_on_change(std::function<void()> callback) {
        m_on_change = std::move(callback);
    }

    void set_max_history(size_t max) { m_max_history = max; }

    static double current_time();

private:
    void notify_change();
    void trim_history();

    std::vector<std::unique_ptr<Command>> m_undo_stack;
    std::vector<std::unique_ptr<Command>> m_redo_stack;

    std::function<void()> m_on_change;

    size_t m_max_history = 100;
    size_t m_saved_index = 0;
};

}
