#pragma once

#include <string>
#include <memory>
#include <vector>

namespace editor {

// Commands encapsulate an action that can be executed, undone, and redone
class Command {
public:
    virtual ~Command() = default;

    virtual void execute() = 0;

    virtual void undo() = 0;

    virtual std::string name() const = 0;

    // Try to merge with a newer command of the same type
    // Returns true if merged (newer command will be deleted)
    // This is used to combine consecutive similar actions (e.g., typing, dragging)
    virtual bool try_merge(const Command* newer) {
        (void)newer;
        return false;
    }

    virtual bool is_mergeable() const { return false; }

    double timestamp() const { return m_timestamp; }

    void set_timestamp(double t) { m_timestamp = t; }

protected:
    double m_timestamp = 0.0;
};

// A composite command that groups multiple commands together
// Executing/undoing affects all contained commands
class CompositeCommand : public Command {
public:
    CompositeCommand(const std::string& name)
        : m_name(name)
    {}

    void add(std::unique_ptr<Command> cmd) {
        m_commands.push_back(std::move(cmd));
    }

    void execute() override {
        for (auto& cmd : m_commands) {
            cmd->execute();
        }
    }

    void undo() override {
        // Undo in reverse order
        for (auto it = m_commands.rbegin(); it != m_commands.rend(); ++it) {
            (*it)->undo();
        }
    }

    std::string name() const override { return m_name; }

    size_t command_count() const { return m_commands.size(); }

private:
    std::string m_name;
    std::vector<std::unique_ptr<Command>> m_commands;
};

}
