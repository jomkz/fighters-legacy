// SPDX-License-Identifier: GPL-3.0-or-later
#include "ILogger.h"
#include "debug/DebugCommandRegistry.h"
#include "debug/DebugCommands.h"
#include "debug/DebugConsole.h"
#include "entity/EntityDef.h"
#include "entity/EntityTypeRegistry.h"
#include "render/RenderSnapshot.h"
#include "render/SimRenderBridge.h"

#include <catch2/catch_test_macros.hpp>

#include <glm/glm.hpp>
#include <string>

// ---------------------------------------------------------------------------
// Minimal ILogger stub — no output, no deps
// ---------------------------------------------------------------------------

struct NullLogger : public ILogger {
    void log(LogLevel, const char*, int, const char*) override {}
    void setMinLevel(LogLevel) override {}
    void flush() override {}
};

// ============================================================================
// DebugCommandRegistry
// ============================================================================

TEST_CASE("DebugCommandRegistry dispatch", "[dbg][registry]") {
    DebugCommandRegistry reg;
    reg.registerCommand("greet", "greet command", [](std::span<std::string_view>) { return std::string("hello"); });
    REQUIRE(reg.dispatch("greet") == "hello");
}

TEST_CASE("DebugCommandRegistry unknown command", "[dbg][registry]") {
    DebugCommandRegistry reg;
    std::string result = reg.dispatch("nope");
    REQUIRE(result.find("nope") != std::string::npos);
    REQUIRE(result.find("unknown command") != std::string::npos);
}

TEST_CASE("DebugCommandRegistry help lists commands", "[dbg][registry]") {
    DebugCommandRegistry reg;
    reg.registerCommand("alpha", "first cmd", [](std::span<std::string_view>) { return std::string{}; });
    reg.registerCommand("beta", "second cmd", [](std::span<std::string_view>) { return std::string{}; });
    std::string h = reg.helpText();
    REQUIRE(h.find("alpha") != std::string::npos);
    REQUIRE(h.find("beta") != std::string::npos);
}

TEST_CASE("DebugCommandRegistry empty input", "[dbg][registry]") {
    DebugCommandRegistry reg;
    REQUIRE(reg.dispatch("") == "");
    REQUIRE(reg.dispatch("   ") == "");
}

TEST_CASE("DebugCommandRegistry multi-space tokenization", "[dbg][registry]") {
    DebugCommandRegistry reg;
    std::vector<std::string_view> captured;
    reg.registerCommand("cmd", "test", [&captured](std::span<std::string_view> args) {
        captured.assign(args.begin(), args.end());
        return std::string{};
    });
    (void)reg.dispatch("cmd  arg1   arg2");
    REQUIRE(captured.size() == 2);
    REQUIRE(captured[0] == "arg1");
    REQUIRE(captured[1] == "arg2");
}

TEST_CASE("DebugCommandRegistry handler receives correct args", "[dbg][registry]") {
    DebugCommandRegistry reg;
    std::string got0, got1;
    reg.registerCommand("add", "add two nums", [&got0, &got1](std::span<std::string_view> args) {
        if (args.size() >= 2) {
            got0 = std::string(args[0]);
            got1 = std::string(args[1]);
        }
        return std::string{};
    });
    (void)reg.dispatch("add 3 4");
    REQUIRE(got0 == "3");
    REQUIRE(got1 == "4");
}

// ============================================================================
// DebugConsole
// ============================================================================

TEST_CASE("DebugConsole output ring wrapping", "[dbg][console]") {
    NullLogger logger;
    DebugCommandRegistry reg;
    DebugConsole con(logger, reg);

    // Push more than kMaxOutputLines (64) entries
    for (int i = 0; i < 70; ++i)
        con.execute("unknown_cmd_" + std::to_string(i));

    // Output ring stores at most 64 unique entries per line (echo + error = 2 per execute)
    // We just verify no crash and the console is usable afterward.
    con.openHeadless();
    glm::dvec3 pos{};
    con.buildHud(&pos);
    REQUIRE(con.elements().size() > 0);
}

TEST_CASE("DebugConsole onTextInput accumulates characters", "[dbg][console]") {
    NullLogger logger;
    DebugCommandRegistry reg;
    DebugConsole con(logger, reg);

    con.openHeadless(); // must be open before text input is accepted
    con.onTextInput("hel");
    con.onTextInput("p");

    // Confirm the accumulated input is visible in the prompt element after buildHud
    con.openHeadless();
    con.buildHud();
    // Find the prompt element (last Text element)
    bool found = false;
    for (const auto& el : con.elements()) {
        if (el.type == HudElement::Type::Text && std::string(el.text).find("help") != std::string::npos) {
            found = true;
            break;
        }
    }
    REQUIRE(found);
}

TEST_CASE("DebugConsole execute dispatches and records output", "[dbg][console]") {
    NullLogger logger;
    DebugCommandRegistry reg;
    reg.registerCommand("ping", "test", [](std::span<std::string_view>) { return std::string("pong"); });
    DebugConsole con(logger, reg);

    con.execute("ping");

    // Output should contain both the echo and the result
    con.openHeadless();
    con.buildHud();
    bool foundEcho = false, foundPong = false;
    for (const auto& el : con.elements()) {
        if (el.type != HudElement::Type::Text)
            continue;
        std::string t(el.text);
        if (t.find("> ping") != std::string::npos)
            foundEcho = true;
        if (t.find("pong") != std::string::npos)
            foundPong = true;
    }
    REQUIRE(foundEcho);
    REQUIRE(foundPong);
}

TEST_CASE("DebugConsole history records submitted commands", "[dbg][console]") {
    NullLogger logger;
    DebugCommandRegistry reg;
    DebugConsole con(logger, reg);

    con.execute("cmd1");
    con.execute("cmd2");

    // Verify history has 2 entries by navigating: open, simulate ArrowUp via internal check
    // We can't call tick() without a mock IInput, so we just verify execute() doesn't crash
    // and the output ring has both echoes.
    con.openHeadless();
    con.buildHud();
    bool foundCmd1 = false, foundCmd2 = false;
    for (const auto& el : con.elements()) {
        if (el.type != HudElement::Type::Text)
            continue;
        std::string t(el.text);
        if (t.find("cmd1") != std::string::npos)
            foundCmd1 = true;
        if (t.find("cmd2") != std::string::npos)
            foundCmd2 = true;
    }
    REQUIRE(foundCmd1);
    REQUIRE(foundCmd2);
}

TEST_CASE("DebugConsole buildHud open produces elements", "[dbg][console]") {
    NullLogger logger;
    DebugCommandRegistry reg;
    DebugConsole con(logger, reg);

    con.openHeadless();
    con.buildHud();

    auto elems = con.elements();
    REQUIRE(elems.size() > 0);

    bool hasRect = false;
    bool hasLine = false;
    bool hasText = false;
    for (const auto& el : elems) {
        if (el.type == HudElement::Type::Rect)
            hasRect = true;
        if (el.type == HudElement::Type::Line)
            hasLine = true;
        if (el.type == HudElement::Type::Text)
            hasText = true;
    }
    REQUIRE(hasRect);
    REQUIRE(hasLine);
    REQUIRE(hasText);
}

TEST_CASE("DebugConsole buildHud when closed no pos", "[dbg][console]") {
    NullLogger logger;
    DebugCommandRegistry reg;
    DebugConsole con(logger, reg);

    con.buildHud(); // closed, no pos
    REQUIRE(con.elements().empty());
}

TEST_CASE("DebugConsole pos widget visible when closed", "[dbg][console]") {
    NullLogger logger;
    DebugCommandRegistry reg;
    DebugConsole con(logger, reg);

    con.showPosRef() = true;
    glm::dvec3 pos{100.0, 200.0, 300.0};
    con.buildHud(&pos);

    REQUIRE(!con.elements().empty());
    bool foundPos = false;
    for (const auto& el : con.elements()) {
        if (el.type == HudElement::Type::Text && std::string(el.text).find("100") != std::string::npos) {
            foundPos = true;
            break;
        }
    }
    REQUIRE(foundPos);
}

TEST_CASE("DebugConsole pos widget hidden when null", "[dbg][console]") {
    NullLogger logger;
    DebugCommandRegistry reg;
    DebugConsole con(logger, reg);

    con.showPosRef() = true;
    con.buildHud(nullptr); // showPos=true but nullptr → no element

    REQUIRE(con.elements().empty());
}

// ============================================================================
// DebugCommands — builtin commands
// ============================================================================

TEST_CASE("DebugCommands types command lists registered types", "[dbg][commands]") {
    fl::EntityTypeRegistry reg;
    fl::EntityDef defA;
    defA.id = "test:alpha";
    defA.name = "Alpha";
    fl::EntityDef defB;
    defB.id = "test:beta";
    defB.name = "Beta";
    reg.registerType(std::move(defA));
    reg.registerType(std::move(defB));

    DebugCommandRegistry cmds;
    DebugCommandContext ctx{};
    ctx.typeRegistry = &reg;
    registerBuiltinCommands(cmds, ctx);

    std::string out = cmds.dispatch("types");
    REQUIRE(out.find("test:alpha") != std::string::npos);
    REQUIRE(out.find("test:beta") != std::string::npos);
}

TEST_CASE("DebugCommands entities command lists snapshot entries", "[dbg][commands]") {
    fl::EntityTypeRegistry tyReg;
    fl::EntityDef def;
    def.id = "test:ship";
    def.name = "Ship";
    tyReg.registerType(std::move(def));

    fl::SimRenderBridge bridge;
    fl::RenderSnapshot snap;
    snap.tickIndex = 1;
    fl::EntityRenderEntry e{};
    e.entityIdx = 3;
    e.entityGen = 1;
    e.typeIndex = 0;
    e.position = {10.0, 20.0, 30.0};
    snap.entries.push_back(e);
    bridge.publish(std::move(snap));
    bridge.tryAdvance();

    DebugCommandRegistry cmds;
    DebugCommandContext ctx{};
    ctx.typeRegistry = &tyReg;
    ctx.renderBridge = &bridge;
    registerBuiltinCommands(cmds, ctx);

    std::string out = cmds.dispatch("entities");
    REQUIRE(out.find("test:ship") != std::string::npos);
    REQUIRE(out.find("3/1") != std::string::npos);
}

TEST_CASE("DebugCommands help command lists all builtins", "[dbg][commands]") {
    DebugCommandRegistry cmds;
    DebugCommandContext ctx{};
    registerBuiltinCommands(cmds, ctx);

    std::string out = cmds.dispatch("help");
    REQUIRE(out.find("types") != std::string::npos);
    REQUIRE(out.find("entities") != std::string::npos);
    REQUIRE(out.find("spawn") != std::string::npos);
    REQUIRE(out.find("kill") != std::string::npos);
    REQUIRE(out.find("tp") != std::string::npos);
    REQUIRE(out.find("toggle_pos") != std::string::npos);
    REQUIRE(out.find("set_weather") != std::string::npos);
    REQUIRE(out.find("set_difficulty") != std::string::npos);
    REQUIRE(out.find("reload_content") != std::string::npos);
}
