#pragma once

#include "Command.h"
#include <vector>
#include <memory>
#include <functional>

namespace editor {

/// Manages the undo/redo history of commands.
class CommandHistory {
public:
    CommandHistory() = default;

    /// Execute a command and add it to the history.
    void execute(std::unique_ptr<Command> cmd);

    /// Add an already-executed command to the history.
    /// Use this when changes have already been applied (e.g., by gizmo manipulation).
    void add_executed(std::unique_ptr<Command> cmd);

    /// Undo the last command.
    void undo();

    /// Redo the last undone command.
    void redo();

    /// Check if there are commands to undo.
    bool can_undo() const { return !m_undo_stack.empty(); }

    /// Check if there are commands to redo.
    bool can_redo() const { return !m_redo_stack.empty(); }

    /// Get the name of the command that would be undone.
    std::string undo_name() const;

    /// Get the name of the command that would be redone.
    std::string redo_name() const;

    /// Clear all history.
    void clear();

    /// Mark the current state as the "saved" state.
    void mark_saved();

    /// Check if the history has changed since the last save.
    bool is_dirty() const;

    /// Get the undo stack for display.
    const std::vector<std::unique_ptr<Command>>& undo_stack() const { return m_undo_stack; }

    /// Get the redo stack for display.
    const std::vector<std::unique_ptr<Command>>& redo_stack() const { return m_redo_stack; }

    /// Set callback for when history changes.
    void set_on_change(std::function<void()> callback) {
        m_on_change = std::move(callback);
    }

    /// Set the maximum number of commands to keep.
    void set_max_history(size_t max) { m_max_history = max; }

    /// Get current time for command timestamps.
    static double current_time();

private:
    void notify_change();
    void trim_history();

    std::vector<std::unique_ptr<Command>> m_undo_stack;
    std::vector<std::unique_ptr<Command>> m_redo_stack;

    std::function<void()> m_on_change;

    size_t m_max_history = 100;
    size_t m_saved_index = 0;  // Index in undo stack when last saved
};

} // namespace editor
