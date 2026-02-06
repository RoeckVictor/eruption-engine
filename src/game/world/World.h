#pragma once

#include "engine/simulation/PixelGrid.h"
#include "engine/simulation/MargolusSimulation.h"

namespace engine::graphics { class RenderContext; }

namespace game {

/// Owns the pixel grid and cellular automata simulation.
/// Scenes are responsible for choosing the grid dimensions and filling
/// initial terrain after calling init().
class World {
public:
    /// Initialize world with grid dimensions.
    /// @param max_material_slots Maximum number of material slots for simulation SSBO
    bool init(int width, int height, int max_material_slots = 256);
    void shutdown();
    void simulate(engine::graphics::RenderContext& ctx);

    int width() const { return m_grid.width(); }
    int height() const { return m_grid.height(); }

    engine::simulation::PixelGrid& grid() { return m_grid; }
    const engine::simulation::PixelGrid& grid() const { return m_grid; }

private:
    engine::simulation::PixelGrid m_grid;
    engine::simulation::MargolusSimulation m_simulation;
};

} // namespace game
