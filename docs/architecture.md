# Architecture Overview

fighters-legacy is a clean-room reimplementation of a combat flight simulator. The engine is fully **content-agnostic** — it has no knowledge of any specific game, franchise, or asset format. All game content enters the engine through a single boundary: the `IContentPack` interface.

For a full description of the project vision and roadmap, see the [README](../README.md).

---

## Layered model

```
┌───────────────────────────────────────┐
│           Content Pack(s)             │  ← IContentPack implementors (external repos)
├───────────────────────────────────────┤
│           Content System              │  ← engine/content/ — AssetManager, ModLoader
├───────────────────────────────────────┤
│           Engine Core                 │  ← engine/ — game loop, entity system, flight model,
│                                       │    AI runtime, mission loader
├───────────────────────────────────────┤
│           Platform HAL                │  ← platform/ — Vulkan, SDL3, OpenAL Soft, ENet
└───────────────────────────────────────┘
              Host OS / Hardware
```

### Platform HAL (`platform/`)

Thin abstraction over OS and hardware APIs. Each backend is isolated:

| Module | Backend | Path |
|---|---|---|
| Renderer | Vulkan 1.3 | `platform/vulkan/` |
| Windowing / Input | SDL3 | `platform/sdl3/` |
| Audio | OpenAL Soft | `platform/openal/` |
| Networking | ENet | `platform/net/` |

The HAL exposes platform-independent interfaces to the engine core. Nothing above the HAL layer links directly against Vulkan, SDL3, OpenAL, or ENet headers.

### Engine Core (`engine/`)

Game logic and simulation, independent of any specific content:

- **Game loop** — fixed-timestep update, variable-rate render
- **Entity system** — component-based scene graph
- **Flight model** — aerodynamics simulation
- **AI runtime** — Lua-scripted AI behaviours
- **Mission loader** — scenario and campaign structure

### Content System (`engine/content/`)

Bridges the engine core to external content:

- **`IContentPack`** — the single interface all content packs must implement. Exposes asset loading, mission data, and configuration. The engine calls only this interface; it never knows what implements it.
- **`AssetManager`** — caches and serves assets requested by the engine core via active content packs.
- **`ModLoader`** — discovers and loads `IContentPack` implementors at runtime (shared libraries).

### Content Packs (external)

Any repository that implements `IContentPack` and compiles to a shared library can be loaded as a content pack. The engine is indifferent to the source or format of the underlying assets.

FA-specific content support is implemented in a separate repository ([fa-content](https://github.com/jomkz/fa-content)) that the engine has no compile-time or runtime dependency on. Users supply their own licensed FA installation; the bridge plugin translates FA formats into the engine's asset types at runtime.

---

## Key design constraints

- **No FA-specific code in this repo.** fighters-legacy must build and run without fa-content or any FA installation present.
- **Cross-platform from day one.** All code compiles on MSVC (Windows), GCC/Clang (Linux), and AppleClang (macOS). Platform-specific paths are confined to `platform/`.
- **`IContentPack` is the only content boundary.** Adding a new content source means implementing this interface, not modifying the engine.
- **C++20, no extensions.** `CMAKE_CXX_EXTENSIONS OFF` is enforced across all presets.
