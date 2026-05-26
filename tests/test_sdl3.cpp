// SPDX-License-Identifier: GPL-3.0-or-later
#include "SDL3Window.h"
#include <catch2/catch_test_macros.hpp>
#include <cstdlib>

static void useHeadlessDriver() {
#ifdef _WIN32
    _putenv_s("SDL_VIDEO_DRIVER", "dummy");
#else
    setenv("SDL_VIDEO_DRIVER", "dummy", 1);
#endif
}

TEST_CASE("SDL3Window init and shutdown (headless)", "[sdl3]") {
    useHeadlessDriver();
    SDL3Window window;
    if (!window.init("smoke", 64, 64))
        SKIP("SDL3 headless init failed");
    CHECK(window.getLastError() == nullptr);
    CHECK(window.width() == 64);
    CHECK(window.height() == 64);
    window.shutdown();
    window.shutdown(); // idempotent
}

TEST_CASE("SDL3Window shouldClose is false after init", "[sdl3]") {
    useHeadlessDriver();
    SDL3Window window;
    if (!window.init("smoke", 64, 64))
        SKIP("SDL3 headless init failed");
    CHECK(!window.shouldClose());
    window.shutdown();
}

TEST_CASE("SDL3Window pollEvents does not crash (headless)", "[sdl3]") {
    useHeadlessDriver();
    SDL3Window window;
    if (!window.init("smoke", 64, 64))
        SKIP("SDL3 headless init failed");
    window.pollEvents();
    CHECK(window.getLastError() == nullptr);
    window.shutdown();
}
