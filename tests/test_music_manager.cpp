// SPDX-License-Identifier: GPL-3.0-or-later
#include "ILogger.h"
#include "audio/MusicManager.h"
#include "audio/PlaylistLoader.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cstddef>

// ---------------------------------------------------------------------------
// NullAudio — no-op IAudio for unit tests (no OpenAL device required).
// All 4 streaming methods must be implemented or the test will not compile.
// ---------------------------------------------------------------------------
struct NullAudio : IAudio {
    bool init() override {
        return true;
    }
    void shutdown() override {}
    const char* getLastError() const override {
        return nullptr;
    }

    AudioBufferId uploadBuffer(const void*, std::size_t, int, int) override {
        return 1;
    }
    void freeBuffer(AudioBufferId) override {}

    AudioBufferId allocStreamBuffer() override {
        return ++m_nextBuf;
    }
    void queueBuffer(AudioSourceId, AudioBufferId, const void*, std::size_t, int, int) override {}
    int processedBufferCount(AudioSourceId) override {
        return 0;
    }
    void unqueueProcessed(AudioSourceId, AudioBufferId*, int) override {}
    void detachBuffers(AudioSourceId) override {}

    AudioSourceId createSource() override {
        return ++m_nextSrc;
    }
    void destroySource(AudioSourceId) override {}
    void play(AudioSourceId, AudioBufferId) override {}
    void stop(AudioSourceId) override {}
    void pause(AudioSourceId) override {}
    void resume(AudioSourceId) override {}
    bool isPlaying(AudioSourceId) const override {
        return false;
    }
    void setLooping(AudioSourceId, bool) override {}
    void setPitch(AudioSourceId, float) override {}
    void setGain(AudioSourceId, float) override {}
    void setPosition(AudioSourceId, float, float, float) override {}
    void setVelocity(AudioSourceId, float, float, float) override {}
    void setReferenceDistance(AudioSourceId, float) override {}
    void setMaxDistance(AudioSourceId, float) override {}
    void setRolloffFactor(AudioSourceId, float) override {}
    void setSourceRelative(AudioSourceId, bool) override {}
    void setListenerTransform(const float[3], const float[3], const float[3]) override {}
    void setListenerVelocity(const float[3]) override {}

    AudioBufferId m_nextBuf{0};
    AudioSourceId m_nextSrc{0};
};

// Minimal ILogger that discards all messages.
struct NullLogger : ILogger {
    void log(LogLevel, const char*, int, const char*) override {}
    void setMinLevel(LogLevel) override {}
    void flush() override {}
};

// ---------------------------------------------------------------------------
// PlaylistLoader tests (pure parse, no filesystem)
// ---------------------------------------------------------------------------

static constexpr const char* kValidPlaylist = R"(
[crossfade]
duration_s = 2.5

[[states]]
id = "Menu"
tracks = ["music/menu"]
loop = true

[[states]]
id = "FlightPatrol"
tracks = ["music/patrol_01", "music/patrol_02"]
loop = true
shuffle = true
)";

TEST_CASE("parsePlaylist crossfade duration", "[audio][playlist]") {
    NullLogger log;
    PlaylistData pd = parsePlaylist(kValidPlaylist, log);
    REQUIRE_THAT(pd.crossfadeDuration, Catch::Matchers::WithinAbs(2.5f, 0.001f));
}

TEST_CASE("parsePlaylist state count and track names", "[audio][playlist]") {
    NullLogger log;
    PlaylistData pd = parsePlaylist(kValidPlaylist, log);
    REQUIRE(pd.states.size() == 2);
    REQUIRE(pd.states[0].id == "Menu");
    REQUIRE(pd.states[0].tracks.size() == 1);
    REQUIRE(pd.states[0].tracks[0] == "music/menu");
    REQUIRE(pd.states[1].id == "FlightPatrol");
    REQUIRE(pd.states[1].tracks.size() == 2);
    REQUIRE(pd.states[1].shuffle);
}

TEST_CASE("parsePlaylist findState", "[audio][playlist]") {
    NullLogger log;
    PlaylistData pd = parsePlaylist(kValidPlaylist, log);
    REQUIRE(pd.findState("Menu") != nullptr);
    REQUIRE(pd.findState("Unknown") == nullptr);
}

TEST_CASE("parsePlaylist empty text returns empty PlaylistData", "[audio][playlist]") {
    NullLogger log;
    PlaylistData pd = parsePlaylist("", log);
    REQUIRE(pd.states.empty());
}

// ---------------------------------------------------------------------------
// MusicManager state transition tests (no actual audio; NullAudio no-ops all calls)
// ---------------------------------------------------------------------------

TEST_CASE("MusicManager init succeeds with NullAudio", "[audio][music]") {
    NullAudio audio;
    NullLogger log;
    MusicManager mm;
    bool ok = mm.init(&audio, nullptr, &log);
    REQUIRE(ok);
    mm.shutdown();
}

TEST_CASE("MusicManager setState does not crash with empty playlist", "[audio][music]") {
    NullAudio audio;
    NullLogger log;
    MusicManager mm;
    mm.init(&audio, nullptr, &log);
    // No playlist loaded — setState should silently no-op.
    mm.setState(GameState::Menu);
    mm.setState(GameState::FlightCombat);
    mm.update(1.0f / 60.0f, 0.8f, 0.7f);
    mm.shutdown();
}

TEST_CASE("MusicManager update does not crash after shutdown", "[audio][music]") {
    NullAudio audio;
    NullLogger log;
    MusicManager mm;
    mm.init(&audio, nullptr, &log);
    mm.shutdown();
    // update after shutdown must be a no-op (m_audio == nullptr guard).
    mm.update(0.016f, 1.0f, 1.0f);
}
