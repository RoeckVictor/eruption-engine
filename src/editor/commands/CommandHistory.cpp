#include "CommandHistory.h"
#include <chrono>

namespace editor {

void CommandHistory::execute(std::unique_ptr<Command> cmd) {
    if (!cmd) return;

    cmd->set_timestamp(current_time());

    // Try to merge with the last command if both are mergeable
    if (!m_undo_stack.empty() && cmd->is_mergeable()) {
        Command* last = m_undo_stack.back().get();
        if (last->is_mergeable() && last->try_merge(cmd.get())) {
            // Merged successfully, don't add the new command
            notify_change();
            return;
        }
    }

    cmd->execute();

    m_undo_stack.push_back(std::move(cmd));

    m_redo_stack.clear();

    trim_history();

    notify_change();
}

void CommandHistory::add_executed(std::unique_ptr<Command> cmd) {
    if (!cmd) return;

    cmd->set_timestamp(current_time());

    // Try to merge with the last command if both are mergeable
    if (!m_undo_stack.empty() && cmd->is_mergeable()) {
        Command* last = m_undo_stack.back().get();
        if (last->is_mergeable() && last->try_merge(cmd.get())) {
            // Merged successfully, don't add the new command
            notify_change();
            return;
        }
    }

    // Don't execute - already applied
    // Just add to undo stack
    m_undo_stack.push_back(std::move(cmd));

    m_redo_stack.clear();

    trim_history();

    notify_change();
}

void CommandHistory::undo() {
    if (m_undo_stack.empty()) return;

    auto cmd = std::move(m_undo_stack.back());
    m_undo_stack.pop_back();

    cmd->undo();

    m_redo_stack.push_back(std::move(cmd));

    notify_change();
}

void CommandHistory::redo() {
    if (m_redo_stack.empty()) return;

    auto cmd = std::move(m_redo_stack.back());
    m_redo_stack.pop_back();

    cmd->execute();

    m_undo_stack.push_back(std::move(cmd));

    notify_change();
}

std::string CommandHistory::undo_name() const {
    if (m_undo_stack.empty()) {
        return "";
    }
    return m_undo_stack.back()->name();
}

std::string CommandHistory::redo_name() const {
    if (m_redo_stack.empty()) {
        return "";
    }
    return m_redo_stack.back()->name();
}

void CommandHistory::clear() {
    m_undo_stack.clear();
    m_redo_stack.clear();
    m_saved_index = 0;
    notify_change();
}

void CommandHistory::mark_saved() {
    m_saved_index = m_undo_stack.size();
}

bool CommandHistory::is_dirty() const {
    return m_undo_stack.size() != m_saved_index;
}

double CommandHistory::current_time() {
    using namespace std::chrono;
    auto now = steady_clock::now();
    return duration<double>(now.time_since_epoch()).count();
}

void CommandHistory::notify_change() {
    if (m_on_change) {
        m_on_change();
    }
}

void CommandHistory::trim_history() {
    while (m_undo_stack.size() > m_max_history) {
        m_undo_stack.erase(m_undo_stack.begin());
        // Adjust saved index if we removed commands before it
        if (m_saved_index > 0) {
            m_saved_index--;
        }
    }
}

}
