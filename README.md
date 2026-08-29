# Eruption Engine

A 2D game engine written in modern C++20, featuring a live editor, an entity-component-system core, and a pixel/falling-sand simulation.

## Features

- **ECS core** built on [EnTT](https://github.com/skypjack/entt)
- **Editor** with ImGui-based panels, gizmos, inspectors, and prefab support
- **Rendering** via OpenGL, with an optional Vulkan backend (requires the Vulkan SDK)
- **Physics** powered by [Box2D](https://box2d.org/)
- **Pixel simulation** — chunked falling-sand terrain with configurable materials
- **C++ scripting** with hot-reloadable game DLLs
- **Systems** for animation, audio (miniaudio), particles, UI, and save/load
- **Standalone runtime** (`eruption_game`) separate from the editor

## Building

Requires CMake 3.20+ and a C++20 compiler. Dependencies are fetched automatically.

```sh
cmake -B build
cmake --build build
```

### Targets

- `eruption_editor`: the editor application
- `eruption_game`: the standalone game runtime
- `eruption_engine`, `eruption_runtime`: static libraries

## Configuration

Runtime settings (physics, timing, graphics backend, assets) live in [`engine_config.json`](engine_config.json).

## Layout

- `src/engine/`: core engine (graphics, physics, simulation, audio, ...)
- `src/editor/`: editor application and tooling
- `src/game/`: standalone runtime entry point
- `src/runtime/`: code shared between editor and runtime
- `assets/`, `shaders/`, `templates/`: runtime content
