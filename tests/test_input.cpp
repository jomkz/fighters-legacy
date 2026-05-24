// SPDX-License-Identifier: GPL-3.0-or-later
#include "input/AxisConfig.h"
#include "input/InputBindings.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

// ---------------------------------------------------------------------------
// AxisConfig::apply
// ---------------------------------------------------------------------------

TEST_CASE("AxisConfig dead zone clamps to zero", "[axis_config]") {
    AxisConfig cfg;
    cfg.deadzone = 0.1f;

    CHECK(cfg.apply(0.0f) == Catch::Approx(0.0f));
    CHECK(cfg.apply(0.05f) == Catch::Approx(0.0f));
    CHECK(cfg.apply(-0.09f) == Catch::Approx(0.0f));
    // Exactly at the boundary is still clamped
    CHECK(cfg.apply(0.1f) == Catch::Approx(0.0f));
}

TEST_CASE("AxisConfig linear rescaling is correct", "[axis_config]") {
    AxisConfig cfg;
    cfg.deadzone = 0.0f;
    cfg.curve = AxisCurve::Linear;
    cfg.invert = false;
    cfg.scale = 1.0f;

    CHECK(cfg.apply(1.0f) == Catch::Approx(1.0f));
    CHECK(cfg.apply(-1.0f) == Catch::Approx(-1.0f));
    CHECK(cfg.apply(0.5f) == Catch::Approx(0.5f));
}

TEST_CASE("AxisConfig rescales from [deadzone,1] to [0,1]", "[axis_config]") {
    AxisConfig cfg;
    cfg.deadzone = 0.5f;
    cfg.curve = AxisCurve::Linear;
    cfg.scale = 1.0f;

    // Just past deadzone → small positive value
    CHECK(cfg.apply(0.5f + 1e-4f) > 0.0f);
    // Full deflection → 1.0
    CHECK(cfg.apply(1.0f) == Catch::Approx(1.0f));
}

TEST_CASE("AxisConfig cubic curve produces expected shape", "[axis_config]") {
    AxisConfig cfg;
    cfg.deadzone = 0.0f;
    cfg.curve = AxisCurve::Cubic;
    cfg.scale = 1.0f;

    // At t=0.5, cubic gives 0.5^3 = 0.125
    CHECK(cfg.apply(0.5f) == Catch::Approx(0.125f).margin(1e-5f));
    // Cubic is always less than linear for t in (0,1)
    CHECK(cfg.apply(0.8f) < 0.8f);
    // Endpoints unchanged
    CHECK(cfg.apply(0.0f) == Catch::Approx(0.0f));
    CHECK(cfg.apply(1.0f) == Catch::Approx(1.0f));
}

TEST_CASE("AxisConfig invert flips sign", "[axis_config]") {
    AxisConfig cfg;
    cfg.deadzone = 0.0f;
    cfg.invert = true;
    cfg.scale = 1.0f;

    CHECK(cfg.apply(1.0f) == Catch::Approx(-1.0f));
    CHECK(cfg.apply(-1.0f) == Catch::Approx(1.0f));
    CHECK(cfg.apply(0.5f) == Catch::Approx(-0.5f));
}

TEST_CASE("AxisConfig scale multiplies output", "[axis_config]") {
    AxisConfig cfg;
    cfg.deadzone = 0.0f;
    cfg.scale = 2.0f;

    CHECK(cfg.apply(0.5f) == Catch::Approx(1.0f));
}

TEST_CASE("AxisConfig negative input mirrors positive", "[axis_config]") {
    AxisConfig cfg;
    cfg.deadzone = 0.1f;
    cfg.curve = AxisCurve::Cubic;
    cfg.scale = 1.0f;

    float pos = cfg.apply(0.7f);
    float neg = cfg.apply(-0.7f);
    CHECK(pos == Catch::Approx(-neg).margin(1e-5f));
}

// ---------------------------------------------------------------------------
// AxisConfigTable serialization
// ---------------------------------------------------------------------------

TEST_CASE("AxisConfigTable TOML roundtrip", "[axis_config]") {
    AxisConfigTable t;
    t.get(GamepadAxis::LeftY).deadzone = 0.15f;
    t.get(GamepadAxis::LeftY).curve = AxisCurve::Cubic;
    t.get(GamepadAxis::LeftY).invert = true;
    t.get(GamepadAxis::LeftY).scale = 0.9f;

    std::string toml = t.serialize();

    AxisConfigTable t2;
    REQUIRE(t2.deserialize(toml));
    CHECK(t2.get(GamepadAxis::LeftY).deadzone == Catch::Approx(0.15f));
    CHECK(t2.get(GamepadAxis::LeftY).curve == AxisCurve::Cubic);
    CHECK(t2.get(GamepadAxis::LeftY).invert == true);
    CHECK(t2.get(GamepadAxis::LeftY).scale == Catch::Approx(0.9f));
}

// ---------------------------------------------------------------------------
// InputBindings defaults
// ---------------------------------------------------------------------------

TEST_CASE("InputBindings default constructor applies defaults", "[bindings]") {
    InputBindings b;
    // Keyboard primary: PitchUp bound to S
    Binding pitch = b.get(InputAction::PitchUp);
    CHECK(pitch.source == BindingSource::Keyboard);
    CHECK(pitch.id == static_cast<uint32_t>(Key::S));

    // Gamepad alt: PitchAxis bound to LeftY
    Binding pitchAxis = b.get(InputAction::PitchAxis, true);
    CHECK(pitchAxis.source == BindingSource::GamepadAxis);
    CHECK(pitchAxis.id == static_cast<uint32_t>(GamepadAxis::LeftY));
}

// ---------------------------------------------------------------------------
// InputBindings set / get / clear
// ---------------------------------------------------------------------------

TEST_CASE("InputBindings set and get roundtrip", "[bindings]") {
    InputBindings b;
    Binding fire{BindingSource::Keyboard, static_cast<uint32_t>(Key::Space), false};
    b.set(InputAction::FireWeapon, fire);
    Binding got = b.get(InputAction::FireWeapon);
    CHECK(got.source == fire.source);
    CHECK(got.id == fire.id);
}

TEST_CASE("InputBindings clear sets source to None", "[bindings]") {
    InputBindings b;
    b.clear(InputAction::PitchUp);
    CHECK(b.get(InputAction::PitchUp).isNone());
}

// ---------------------------------------------------------------------------
// InputBindings conflict detection
// ---------------------------------------------------------------------------

TEST_CASE("InputBindings detects conflict on duplicate binding", "[bindings]") {
    InputBindings b;
    b.applyDefaults();

    // Assign the same key that PitchUp already has (S) to RollLeft
    Binding s{BindingSource::Keyboard, static_cast<uint32_t>(Key::S), false};
    auto conflict = b.conflictsWith(InputAction::RollLeft, s);
    REQUIRE(conflict.has_value());
    CHECK(*conflict == InputAction::PitchUp);
}

TEST_CASE("InputBindings no conflict after clearing the conflicting action", "[bindings]") {
    InputBindings b;
    b.applyDefaults();
    b.clear(InputAction::PitchUp);

    Binding s{BindingSource::Keyboard, static_cast<uint32_t>(Key::S), false};
    CHECK_FALSE(b.conflictsWith(InputAction::RollLeft, s).has_value());
}

TEST_CASE("InputBindings no conflict for None binding", "[bindings]") {
    InputBindings b;
    CHECK_FALSE(b.conflictsWith(InputAction::RollLeft, Binding{}).has_value());
}

TEST_CASE("InputBindings does not conflict with its own action", "[bindings]") {
    InputBindings b;
    // PitchUp is bound to S; asking conflictsWith(PitchUp, S) should return nullopt
    Binding s{BindingSource::Keyboard, static_cast<uint32_t>(Key::S), false};
    CHECK_FALSE(b.conflictsWith(InputAction::PitchUp, s).has_value());
}

// ---------------------------------------------------------------------------
// InputBindings TOML roundtrip
// ---------------------------------------------------------------------------

TEST_CASE("InputBindings TOML serialize then deserialize reproduces all bindings", "[bindings]") {
    InputBindings original;
    original.applyDefaults();

    std::string toml = original.serialize();

    InputBindings loaded;
    // Clear loaded first so we can confirm deserialize fills it in
    for (int i = 0; i < InputBindings::kActionCount; ++i) {
        loaded.clear(static_cast<InputAction>(i));
        loaded.clear(static_cast<InputAction>(i), true);
    }
    REQUIRE(loaded.deserialize(toml));

    for (int i = 0; i < InputBindings::kActionCount; ++i) {
        auto action = static_cast<InputAction>(i);
        Binding a = original.get(action);
        Binding b = loaded.get(action);
        INFO("Primary binding mismatch for action " << i);
        CHECK(a.source == b.source);
        CHECK(a.id == b.id);
        CHECK(a.axisNegative == b.axisNegative);

        Binding aAlt = original.get(action, true);
        Binding bAlt = loaded.get(action, true);
        INFO("Alt binding mismatch for action " << i);
        CHECK(aAlt.source == bAlt.source);
        CHECK(aAlt.id == bAlt.id);
        CHECK(aAlt.axisNegative == bAlt.axisNegative);
    }
}

TEST_CASE("InputBindings deserialize returns false on invalid TOML", "[bindings]") {
    InputBindings b;
    CHECK_FALSE(b.deserialize("this is not valid toml }{"));
}

TEST_CASE("InputBindings deserialize returns false on unrecognised key name", "[bindings]") {
    InputBindings b;
    CHECK_FALSE(b.deserialize("[primary]\nPitchUp = { source = \"Keyboard\", id = \"NotAKey\" }\n"));
}

TEST_CASE("InputBindings deserialize ignores unknown action names", "[bindings]") {
    InputBindings b;
    CHECK(b.deserialize("[primary]\nUnknownAction = { source = \"Keyboard\", id = \"A\" }\n"));
}

// ---------------------------------------------------------------------------
// InputBindings — serialization coverage for binding source types not in defaults
// ---------------------------------------------------------------------------

TEST_CASE("InputBindings roundtrips GamepadButton values not in defaults", "[bindings]") {
    InputBindings b;
    for (int i = 0; i < InputBindings::kActionCount; ++i) {
        b.clear(static_cast<InputAction>(i));
        b.clear(static_cast<InputAction>(i), true);
    }
    b.set(InputAction::RollLeft, {BindingSource::GamepadButton, static_cast<uint32_t>(GamepadButton::X), false});
    b.set(InputAction::RollRight, {BindingSource::GamepadButton, static_cast<uint32_t>(GamepadButton::Y), false});
    b.set(InputAction::YawLeft, {BindingSource::GamepadButton, static_cast<uint32_t>(GamepadButton::LeftStick), false});
    b.set(InputAction::YawRight,
          {BindingSource::GamepadButton, static_cast<uint32_t>(GamepadButton::RightStick), false});
    b.set(InputAction::ViewUp, {BindingSource::GamepadButton, static_cast<uint32_t>(GamepadButton::DpadUp), false});
    b.set(InputAction::ViewLeft, {BindingSource::GamepadButton, static_cast<uint32_t>(GamepadButton::DpadLeft), false});
    b.set(InputAction::ViewRight,
          {BindingSource::GamepadButton, static_cast<uint32_t>(GamepadButton::DpadRight), false});

    InputBindings b2;
    for (int i = 0; i < InputBindings::kActionCount; ++i) {
        b2.clear(static_cast<InputAction>(i));
        b2.clear(static_cast<InputAction>(i), true);
    }
    REQUIRE(b2.deserialize(b.serialize()));
    CHECK(b2.get(InputAction::RollLeft).id == static_cast<uint32_t>(GamepadButton::X));
    CHECK(b2.get(InputAction::RollRight).id == static_cast<uint32_t>(GamepadButton::Y));
    CHECK(b2.get(InputAction::YawLeft).id == static_cast<uint32_t>(GamepadButton::LeftStick));
    CHECK(b2.get(InputAction::YawRight).id == static_cast<uint32_t>(GamepadButton::RightStick));
    CHECK(b2.get(InputAction::ViewUp).id == static_cast<uint32_t>(GamepadButton::DpadUp));
    CHECK(b2.get(InputAction::ViewLeft).id == static_cast<uint32_t>(GamepadButton::DpadLeft));
    CHECK(b2.get(InputAction::ViewRight).id == static_cast<uint32_t>(GamepadButton::DpadRight));
}

TEST_CASE("InputBindings roundtrips GamepadAxis RightY and TriggerRight with axisNegative", "[bindings]") {
    InputBindings b;
    b.set(InputAction::PitchAxis, {BindingSource::GamepadAxis, static_cast<uint32_t>(GamepadAxis::RightY), false});
    b.set(InputAction::RollAxis, {BindingSource::GamepadAxis, static_cast<uint32_t>(GamepadAxis::TriggerRight), true});

    InputBindings b2;
    REQUIRE(b2.deserialize(b.serialize()));
    CHECK(b2.get(InputAction::PitchAxis).id == static_cast<uint32_t>(GamepadAxis::RightY));
    CHECK(b2.get(InputAction::RollAxis).id == static_cast<uint32_t>(GamepadAxis::TriggerRight));
    CHECK(b2.get(InputAction::RollAxis).axisNegative == true);
}

TEST_CASE("InputBindings roundtrips MouseButton Middle", "[bindings]") {
    InputBindings b;
    b.set(InputAction::FireMissile, {BindingSource::MouseButton, static_cast<uint32_t>(MouseButton::Middle), false});

    InputBindings b2;
    REQUIRE(b2.deserialize(b.serialize()));
    CHECK(b2.get(InputAction::FireMissile).source == BindingSource::MouseButton);
    CHECK(b2.get(InputAction::FireMissile).id == static_cast<uint32_t>(MouseButton::Middle));
}

TEST_CASE("InputBindings roundtrips keyboard keys not in default bindings", "[bindings]") {
    InputBindings b;
    for (int i = 0; i < InputBindings::kActionCount; ++i) {
        b.clear(static_cast<InputAction>(i));
        b.clear(static_cast<InputAction>(i), true);
    }
    b.set(InputAction::ViewUp, {BindingSource::Keyboard, static_cast<uint32_t>(Key::F1), false});
    b.set(InputAction::ViewDown, {BindingSource::Keyboard, static_cast<uint32_t>(Key::F12), false});
    b.set(InputAction::ViewLeft, {BindingSource::Keyboard, static_cast<uint32_t>(Key::LeftCtrl), false});
    b.set(InputAction::ViewRight, {BindingSource::Keyboard, static_cast<uint32_t>(Key::RightAlt), false});
    b.set(InputAction::LandingGear, {BindingSource::Keyboard, static_cast<uint32_t>(Key::Enter), false});
    b.set(InputAction::Flaps, {BindingSource::Keyboard, static_cast<uint32_t>(Key::Backspace), false});
    b.set(InputAction::Pause, {BindingSource::Keyboard, static_cast<uint32_t>(Key::Insert), false});
    b.set(InputAction::Menu, {BindingSource::Keyboard, static_cast<uint32_t>(Key::PageUp), false});

    InputBindings b2;
    for (int i = 0; i < InputBindings::kActionCount; ++i) {
        b2.clear(static_cast<InputAction>(i));
        b2.clear(static_cast<InputAction>(i), true);
    }
    REQUIRE(b2.deserialize(b.serialize()));
    CHECK(b2.get(InputAction::ViewUp).id == static_cast<uint32_t>(Key::F1));
    CHECK(b2.get(InputAction::ViewDown).id == static_cast<uint32_t>(Key::F12));
    CHECK(b2.get(InputAction::ViewLeft).id == static_cast<uint32_t>(Key::LeftCtrl));
    CHECK(b2.get(InputAction::ViewRight).id == static_cast<uint32_t>(Key::RightAlt));
    CHECK(b2.get(InputAction::LandingGear).id == static_cast<uint32_t>(Key::Enter));
    CHECK(b2.get(InputAction::Flaps).id == static_cast<uint32_t>(Key::Backspace));
    CHECK(b2.get(InputAction::Pause).id == static_cast<uint32_t>(Key::Insert));
    CHECK(b2.get(InputAction::Menu).id == static_cast<uint32_t>(Key::PageUp));
}

// ---------------------------------------------------------------------------
// AxisConfig — overrange clamping
// ---------------------------------------------------------------------------

TEST_CASE("AxisConfig clamps input beyond 1.0 to 1.0", "[axis_config]") {
    AxisConfig cfg;
    cfg.deadzone = 0.0f;
    cfg.scale = 1.0f;
    CHECK(cfg.apply(1.5f) == Catch::Approx(1.0f));
    CHECK(cfg.apply(-1.5f) == Catch::Approx(-1.0f));
}

// ---------------------------------------------------------------------------
// AxisConfigTable — absent section keeps defaults
// ---------------------------------------------------------------------------

TEST_CASE("AxisConfigTable deserialize with no axis_config section keeps defaults", "[axis_config]") {
    AxisConfigTable t;
    t.get(GamepadAxis::LeftY).deadzone = 0.2f;
    REQUIRE(t.deserialize("[other_section]\nfoo = 1\n"));
    CHECK(t.get(GamepadAxis::LeftY).deadzone == Catch::Approx(0.2f));
}
