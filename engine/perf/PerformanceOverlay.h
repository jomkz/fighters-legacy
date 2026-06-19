// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "RenderTypes.h"
#include "config/DebugSettings.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

// Collects per-frame statistics and builds text lines for the debug overlay.
// The overlay is toggled via F3: Off → Compact → Full → Off.
//
// Threading: all methods are main-thread-only.
class PerformanceOverlay {
  public:
    void cycleMode();
    [[nodiscard]] OverlayMode mode() const noexcept {
        return m_mode;
    }
    void setMode(OverlayMode m) noexcept {
        m_mode = m;
    }

    // Call once per rendered frame. Updates rolling history and rebuilds line strings.
    // simTickMs: estimated sim tick duration (1000/tickRateHz in standalone mode).
    void update(const FrameStats& stats, uint32_t entityCount, float simTickMs);

    // Append camera + entity debug lines after the perf lines. Call after update(), before lines().
    // No-op when the overlay is Off. modeStr is the camera mode label (e.g. "FREE"); cam supplies
    // the eye (worldOrigin) and forward (from the view matrix); entityPos may be null; the terrain
    // heights are the ground elevation under the camera and under the entity (for AGL readouts).
    void setSceneInfo(const char* modeStr, const CameraView& cam, const glm::dvec3* entityPos, double terrainAtCam,
                      double terrainAtEntity);

    // Returns the built lines for IRenderer::setOverlayLines().
    // Returns empty span when mode == Off. Valid until the next call to update().
    [[nodiscard]] std::span<const std::string_view> lines() const noexcept;

  private:
    void buildLines(const FrameStats& stats, uint32_t entityCount, float simTickMs);

    static constexpr int kHistoryLen = 128;
    float m_history[kHistoryLen]{};
    int m_histHead{0};
    OverlayMode m_mode{OverlayMode::Off};

    // Pre-allocated string storage (avoids per-frame allocation).
    // Up to 5 perf lines + 2 scene-info lines (camera + entity) appended by setSceneInfo().
    static constexpr int kMaxLines = 8;
    std::string m_line[kMaxLines];
    std::string_view m_lineViews[kMaxLines];
    int m_lineCount{0};
};
