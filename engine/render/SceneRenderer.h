// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "RenderTypes.h"

#include <functional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

class AssetManager;
class IRenderer;
namespace fl {
class ParticleSystem;
class SimRenderBridge;
} // namespace fl

namespace fl {

// Converts the per-tick entity snapshot from SimRenderBridge into a FrameScene and submits
// it to IRenderer::setScene each frame.
//
// Threading: all methods must be called from the main (render) thread.
//
// Dependency injection:
//   MeshNameResolver breaks the circular CMake dep between engine-render and engine-entity.
//   Caller (main.cpp) provides a lambda that captures &EntityTypeRegistry and resolves
//   typeIndex → (meshName, classicDamageMeshName). Returns false if the typeIndex is unknown.
class SceneRenderer {
  public:
    // Given a typeIndex, fills meshName and classicDamageMeshName (empty if no damage variant).
    // Returns true if the type is known; false to skip the entity entirely.
    using MeshNameResolver =
        std::function<bool(uint32_t typeIndex, std::string& meshName, std::string& damageMeshName)>;

    // Given a typeIndex and damageLevel (uint8_t cast of DamageLevel), returns the visual
    // effect preset name, or empty string if none.  Used to emit particle effects from
    // damaged entities without introducing a CMake dep on engine-entity.
    using EffectResolver = std::function<std::string(uint32_t typeIndex, uint8_t damageLevel)>;

    SceneRenderer(SimRenderBridge& bridge, MeshNameResolver resolver, AssetManager& assets, IRenderer& renderer);
    ~SceneRenderer();

    // Optional: wire a ParticleSystem to emit per-entity damage effects each frame.
    // effectResolver is called for each entity with damageLevel > 0; the returned preset
    // name is forwarded to ParticleSystem::emit(). Pass nullptr/empty to disable.
    void setParticleSystem(ParticleSystem* ps, EffectResolver effectResolver) noexcept;

    // Advance to the latest sim snapshot and submit a FrameScene to the renderer.
    // Must be called between IRenderer::beginFrame() and endFrame().
    // alpha — render-interpolation factor from GameLoop::shellTick(), in [0, 1].
    // extraEmitters — additional emitters beyond entity damage effects (may be empty).
    void renderFrame(float alpha, const CameraView& camera, const EnvironmentState& env,
                     std::span<const ParticleEmitterState> extraEmitters = {});

  private:
    MeshHandle getOrUploadMesh(const std::string& name);
    MaterialHandle getOrUploadMaterial(const std::string& meshName);

    SimRenderBridge& m_bridge;
    MeshNameResolver m_resolver;
    AssetManager& m_assets;
    IRenderer& m_renderer;

    ParticleSystem* m_particleSystem{nullptr};
    EffectResolver m_effectResolver;

    // Per-typeIndex resolved names, cached so the resolver is called at most once per type.
    std::unordered_map<uint32_t, std::pair<std::string, std::string>> m_typeNameCache;

    std::unordered_map<std::string, MeshHandle> m_meshCache;
    std::unordered_map<std::string, MaterialHandle> m_materialCache;
    std::vector<RenderItem> m_items; // reused each frame; avoids per-frame allocation

    // Nominal tick period used for velocity-based position extrapolation.
    static constexpr float kTickDt = 1.0f / 60.0f;
};

} // namespace fl
