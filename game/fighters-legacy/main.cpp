// SPDX-License-Identifier: GPL-3.0-or-later
#include "FileLogger.h"
#include "IWindowEventHandler.h"
#include "Platform.h"
#include "Version.h"
#include "config/UserConfig.h"
#include "content/AssetManager.h"
#include "content/ModLoader.h"
#include "crash/CrashInfo.h"
#include "crash/CrashReporter.h"
#include "entity/EntityDef.h"
#include "entity/EntityManager.h"
#include "entity/EntityTypeRegistry.h"
#include "firstrun/FirstRun.h"
#include "loop/GameLoop.h"
#include "openal/OALAudio.h"
#include "render/CameraController.h"
#include "render/ParticleSystem.h"
#include "render/SceneRenderer.h"
#include "render/SimRenderBridge.h"
#include "sandbox/SandboxInspector.h"
#include "sdl3/SDL3Cursor.h"
#include "sdl3/SDL3Display.h"
#include "sdl3/SDL3Filesystem.h"
#include "sdl3/SDL3Input.h"
#include "sdl3/SDL3Joystick.h"
#include "sdl3/SDL3Window.h"
#include "vulkan/VkRendererFactory.h"

#include <SDL3/SDL.h>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <optional>

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    // Step 0: Early flag handling — before any platform init so they work
    // even when SDL/Vulkan are absent.
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--version") == 0) {
            std::printf("fighters-legacy %s (%s)\n", FL_VERSION_STRING, FL_GIT_HASH);
            return 0;
        }
        if (std::strcmp(argv[i], "--help") == 0) {
            std::printf("Usage: fighters-legacy [--log-level debug|info|warn|error] [--version] [--help]\n");
            return 0;
        }
    }

    // Step 1: Resolve UserData directory.
    SDL_Init(0);
    char* prefRaw = SDL_GetPrefPath("jomkz", "fighters-legacy");
    fs::path userDataDir = prefRaw ? fs::path(prefRaw) : fs::path(".");
    if (prefRaw)
        SDL_free(prefRaw);

    Platform p;

    // Step 2: FileLogger — open at default level.
    auto fileLogger = std::make_unique<FileLogger>();
    if (!fileLogger->open((userDataDir / "logs").string(), 10)) {
        std::fprintf(stderr, "fighters-legacy: cannot open log file in %s, falling back to stderr\n",
                     (userDataDir / "logs").string().c_str());
    }
    FileLogger* rawLogger = fileLogger.get();
    p.logger = std::move(fileLogger);

    // Step 3: SDL3Filesystem — assets root is the directory containing the binary.
    const char* baseRaw = SDL_GetBasePath();
    fs::path assetsRoot = baseRaw ? fs::path(baseRaw) : fs::path(".");
    p.filesystem = std::make_unique<SDL3Filesystem>(assetsRoot, userDataDir);

    // Step 4: UserConfig — load persisted settings (missing file is non-fatal).
    UserConfig userConfig(*p.filesystem, *rawLogger);
    userConfig.load();

    // Step 5: Apply --log-level CLI override (session-only; does not persist).
    for (int i = 1; i < argc - 1; ++i) {
        if (std::strcmp(argv[i], "--log-level") == 0)
            rawLogger->setMinLevel(parseLogLevel(argv[i + 1]));
    }

    // Step 6: Audio backend.
    auto oalAudio = std::make_unique<OALAudio>();
    if (!oalAudio->init()) {
        rawLogger->log(LogLevel::Error, __FILE__, __LINE__, oalAudio->getLastError());
        return 1;
    }
    p.audio = std::move(oalAudio);

    // Step 7: Input and joystick backends — keep raw pointers before move for sink
    // wiring below.
    auto sdl3Input = std::make_unique<SDL3Input>();
    SDL3Input* rawInput = sdl3Input.get();
    p.input = std::move(sdl3Input);

    auto sdl3Joystick = std::make_unique<SDL3Joystick>();
    SDL3Joystick* rawJoystick = sdl3Joystick.get();
    p.joystick = std::move(sdl3Joystick);

    // Step 8: Window (SDL_ShowMessageBox safe before video init per SDL3 docs).
    auto window = std::make_unique<SDL3Window>();
    p.window = std::move(window);

    // Step 9: Post-crash dialog.
    CrashReporter::checkPreviousCrash(userDataDir.string(), p.window.get(), rawLogger,
                                      "https://github.com/jomkz/fighters-legacy/issues/new");

    // Step 10: CrashReporter — sentinel + signal handlers.
    CrashInfo crashInfo;
    crashInfo.engineVersion = FL_VERSION_STRING;
    crashInfo.populateOS();
    CrashReporter crashReporter;
    crashReporter.init(
        {userDataDir.string(), "https://github.com/jomkz/fighters-legacy/issues/new", rawLogger, p.window.get()},
        crashInfo);

    // Step 11: Platform init — wire input and joystick sinks before window init so
    // events are routed during the very first pollEvents() call.
    auto* sdlWindow = static_cast<SDL3Window*>(p.window.get());
    sdlWindow->setInputSink(rawInput);
    sdlWindow->setJoystickSink(rawJoystick);

    if (!p.window->init("Fighters Legacy", 1280, 720)) {
        rawLogger->log(LogLevel::Error, __FILE__, __LINE__, "window init failed");
        crashReporter.shutdown();
        return 1;
    }

    // Step 11.5: Display and cursor — usable now that SDL_INIT_VIDEO is up.
    p.display = std::make_unique<SDL3Display>();
    p.cursor = std::make_unique<SDL3Cursor>();

    // Step 12: Renderer.
    auto renderer = createVulkanRenderer();
    p.renderer = std::move(renderer);
    if (!p.renderer->init(p.window.get())) {
        rawLogger->log(LogLevel::Error, __FILE__, __LINE__, "renderer init failed");
        crashReporter.shutdown();
        return 1;
    }

    // Step 13: Resize handler — forwards window resize events to the renderer.
    struct ResizeHandler : IWindowEventHandler {
        IRenderer* r = nullptr;
        void onResize(int w, int h) override {
            r->onResize(w, h);
        }
        void onClose() override {}
    } resizeHandler;
    resizeHandler.r = p.renderer.get();
    p.window->setEventHandler(&resizeHandler);

    // Step 14: GPU info → crash reporter.
    crashReporter.setGpuInfo(p.renderer->gpuInfo());

    // Step 14.1: Translate GraphicsSettings → RendererSettings and apply to renderer.
    // drawDistanceKm is applied to sceneRenderer after it is constructed (step 17b.2).
    RendererSettings rendererSettings{};
    {
        const GraphicsSettings g = userConfig.graphics();
        switch (g.vsync) {
        case VsyncMode::Off:
            rendererSettings.vsync = RendererVsyncMode::Off;
            break;
        case VsyncMode::Adaptive:
            rendererSettings.vsync = RendererVsyncMode::Adaptive;
            break;
        default:
            rendererSettings.vsync = RendererVsyncMode::On;
            break;
        }
        rendererSettings.antiAliasing = g.antiAliasing;
        rendererSettings.bloom = (g.qualityPreset >= QualityLevel::Medium);
        switch (g.drawDistance) {
        case DrawDistance::Low:
            rendererSettings.drawDistanceKm = 15.0f;
            break;
        case DrawDistance::Medium:
            rendererSettings.drawDistanceKm = 30.0f;
            break;
        case DrawDistance::Ultra:
            rendererSettings.drawDistanceKm = 100.0f;
            break;
        default: // High
            rendererSettings.drawDistanceKm = 50.0f;
            break;
        }
        p.renderer->applySettings(rendererSettings);
    }

    // Step 15: Mod loading.
    ModLoader modLoader(*p.filesystem, *rawLogger);
    auto packs = modLoader.load();
    const bool hasPacks = !packs.empty();

    // Build crash reporter mod list before packs are moved into AssetManager.
    CrashInfo::ModEntry modEntries[CrashInfo::kMaxMods];
    int modCount = 0;
    for (const auto& pack : packs) {
        if (modCount >= CrashInfo::kMaxMods)
            break;
        auto& e = modEntries[modCount++];
        std::snprintf(e.id, sizeof(e.id), "%s", pack->id());
        std::snprintf(e.version, sizeof(e.version), "%s", pack->version());
    }
    crashReporter.setMods(modEntries, modCount);

    AssetManager assets(std::move(packs), *rawLogger);
    assets.initialize(p.window.get());

    // Step 16: First-run routing.
    FirstRun firstRun(userConfig, *rawLogger);
    auto outcome = firstRun.check(hasPacks);

    // Step 17a: Entity system — type registry and manager replace the Phase 2.1 NullSim stub.
    fl::EntityTypeRegistry entityRegistry;
    fl::EntityManager entityManager(*rawLogger, entityRegistry);

    // Step 17b: Render bridge — wired before the sim thread starts so EntityManager::onTick
    // can publish snapshots from the very first tick.
    fl::SimRenderBridge renderBridge;
    entityManager.setRenderBridge(&renderBridge);

    // Step 17b.1: Particle system — preset registry + per-frame emitter accumulation.
    fl::ParticleSystem particleSystem;
    // Built-in presets covering the roadmap.md:68 acceptance criteria.
    particleSystem.registerPreset("explosion",
                                  {200.0f, 1.5f, 15.0f, {1.0f, 0.6f, 0.1f}, {0.4f, 0.2f, 0.1f}, 0.3f, 3.0f, true});
    particleSystem.registerPreset("fire",
                                  {120.0f, 2.0f, 8.0f, {1.0f, 0.4f, 0.05f}, {0.6f, 0.1f, 0.0f}, 0.2f, 1.5f, true});
    particleSystem.registerPreset("smoke",
                                  {60.0f, 4.0f, 3.0f, {0.4f, 0.4f, 0.4f}, {0.15f, 0.15f, 0.15f}, 0.5f, 3.0f, false});

    // Step 17b.2: Scene renderer — converts entity snapshots to FrameScene each frame.
    // MeshNameResolver breaks the circular dep between engine-render and engine-entity:
    // the lambda captures entityRegistry (main-thread-only, read-only after start()).
    fl::CameraController cameraController;
    fl::SceneRenderer sceneRenderer{renderBridge,
                                    [&entityRegistry](uint32_t idx, std::string& mesh, std::string& dmg) -> bool {
                                        const fl::EntityDef* def = entityRegistry.byIndex(idx);
                                        if (!def)
                                            return false;
                                        mesh = def->mesh;
                                        dmg = def->classicDamageMesh;
                                        return true;
                                    },
                                    assets, *p.renderer};

    // Apply draw distance from settings (must come after SceneRenderer construction).
    sceneRenderer.setDrawDistance(rendererSettings.drawDistanceKm);

    // Wire the particle system so damaged entities emit effects each frame.
    // EffectResolver uses the snapshot typeIndex + damageLevel without touching sim-thread state.
    sceneRenderer.setParticleSystem(&particleSystem,
                                    [&entityRegistry](uint32_t idx, uint8_t damageLevel) -> std::string {
                                        const fl::EntityDef* def = entityRegistry.byIndex(idx);
                                        if (!def || !def->damage)
                                            return {};
                                        const fl::DamagePenalty* pen = nullptr;
                                        switch (static_cast<fl::DamageLevel>(damageLevel)) {
                                        case fl::DamageLevel::Light:
                                            pen = &def->damage->light;
                                            break;
                                        case fl::DamageLevel::Heavy:
                                            pen = &def->damage->heavy;
                                            break;
                                        case fl::DamageLevel::Critical:
                                            pen = &def->damage->critical;
                                            break;
                                        default:
                                            break;
                                        }
                                        return pen ? pen->visualEffect : std::string{};
                                    });

    // Step 17c: Sandbox inspector (when no content packs are present).
    std::optional<SandboxInspector> inspector;
    if (outcome == FirstRunOutcome::LaunchSandboxInspector)
        inspector.emplace(*p.audio, *p.input, *rawLogger, 440.0f, &entityManager);

    // Step 17d: Game loop — sim thread starts here.
    GameLoop gameLoop(entityManager, *rawLogger);
    gameLoop.start();

    // Step 18: Shell loop — main thread owns all HAL.
    bool running = true;
    while (running && !p.window->shouldClose()) {
        p.window->pollEvents();
        p.renderer->beginFrame();
        if (inspector && !inspector->update())
            running = false;
        float alpha = gameLoop.shellTick();
        float aspect =
            static_cast<float>(p.window->width()) / static_cast<float>(p.window->height() > 0 ? p.window->height() : 1);
        sceneRenderer.renderFrame(alpha, cameraController.view(aspect), EnvironmentState{});
        p.renderer->endFrame();
        p.input->flush();
        p.joystick->flush();
    }

    // Step 19: Clean shutdown.
    gameLoop.stop(); // join sim thread before any HAL teardown
    inspector.reset();
    p.cursor.reset(); // destroy cursor while SDL video is still alive (before SDL_Quit)
    p.audio->shutdown();
    p.renderer->shutdown();
    p.window->shutdown();
    crashReporter.shutdown();
    return 0;
}
