// SPDX-License-Identifier: GPL-3.0-or-later
#include "IWindowEventHandler.h"
#include "Platform.h"
#include "SDL3Display.h"
#include "SDL3Window.h"
#include "VkRendererFactory.h"
#include "render/BuiltinGeometry.h"

#include <atomic>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>

static std::atomic<bool> g_quit{false};
static void onSignal(int) {
    g_quit = true;
}

class App : public IWindowEventHandler {
  public:
    explicit App(Platform& p) : m_platform(p) {}

    void onResize(int w, int h) override {
        m_platform.renderer->onResize(w, h);
    }
    void onClose() override {
        m_running = false;
    }

    int run() {
        if (!m_platform.window->init("Fighters Legacy — Hello Triangle", 1280, 720)) {
            std::fprintf(stderr, "window init failed: %s\n", m_platform.window->getLastError());
            return 1;
        }
        m_platform.window->setEventHandler(this);

        if (!m_platform.renderer->init(m_platform.window.get())) {
            std::fprintf(stderr, "renderer init failed: %s\n", m_platform.renderer->getLastError());
            return 1;
        }

        // Upload the builtin tetrahedron — a single orange entity at the origin.
        m_mesh = m_platform.renderer->createMesh({"builtin:entity", fl::builtinTetrahedronGlb()});
        if (!m_mesh.valid())
            std::fprintf(stderr, "hello_triangle: builtin mesh upload failed\n");

        MaterialDesc md{};
        md.baseColorFactor = {1.0f, 0.45f, 0.1f, 1.0f}; // orange
        md.roughnessFactor = 0.7f;
        m_mat = m_platform.renderer->createMaterial(md);

        m_env.sunDirection = glm::normalize(glm::vec3{0.6f, -1.0f, 0.4f});

        while (m_running && !m_platform.window->shouldClose() && !g_quit) {
            m_platform.window->pollEvents();
            m_platform.renderer->beginFrame();
            submitScene();
            m_platform.renderer->endFrame();
        }

        m_platform.renderer->shutdown();
        m_platform.window->shutdown();
        return 0;
    }

  private:
    void submitScene() {
        // Infinite reverse-Z perspective: proj[3][2] = near (read by shadow cascade logic).
        // Vulkan Y-flip: proj[1][1] = -f.
        const float fovY = 1.0472f; // 60 degrees
        const int wi = m_platform.window->width();
        const int hi = m_platform.window->height();
        const float aspect = static_cast<float>(wi) / static_cast<float>(hi > 0 ? hi : 1);
        const float near = 0.1f;
        const float f = 1.0f / std::tan(fovY * 0.5f);
        glm::mat4 proj{0.0f};
        proj[0][0] = f / aspect;
        proj[1][1] = -f;
        proj[2][3] = -1.0f;
        proj[3][2] = near;

        const glm::vec3 eye{0.0f, 5.0f, -15.0f};
        CameraView cam{glm::lookAt(eye, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}), proj, eye};

        if (m_mesh.valid()) {
            RenderItem item{m_mesh, m_mat, glm::mat4(1.0f), 0, 0, {}};
            FrameScene scene{cam, std::span<const RenderItem>{&item, 1}, m_env, {}};
            m_platform.renderer->setScene(scene);
        } else {
            FrameScene scene{cam, {}, m_env, {}};
            m_platform.renderer->setScene(scene);
        }
    }

    Platform& m_platform;
    MeshHandle m_mesh{};
    MaterialHandle m_mat{};
    EnvironmentState m_env{};
    bool m_running{true};
};

int main() {
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);
    Platform p;
    p.window = std::make_unique<SDL3Window>();
    p.display = std::make_unique<SDL3Display>();
    p.renderer = createVulkanRenderer();
    return App(p).run();
}
