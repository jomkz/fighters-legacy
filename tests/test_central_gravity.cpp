// SPDX-License-Identifier: GPL-3.0-or-later
#include "flight/CentralGravityField.h"
#include "flight/IGravityField.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

using Catch::Approx;

TEST_CASE("CentralGravityField: gravity at origin matches surface g", "[gravity]") {
    fl::CentralGravityField g(6'371'000.f, 9.80665f);
    double pos[3] = {0.0, 0.0, 0.0};
    auto a = g.accelWorld(pos);
    CHECK(a[0] == Approx(0.f).margin(1e-4f));
    CHECK(a[1] == Approx(-9.80665f).epsilon(1e-4));
    CHECK(a[2] == Approx(0.f).margin(1e-4f));
}

TEST_CASE("CentralGravityField: gravity weaker at altitude", "[gravity]") {
    const float R = 6'371'000.f;
    fl::CentralGravityField g(R, 9.80665f);
    double pos[3] = {0.0, 1000.0, 0.0}; // 1 km above surface
    auto a = g.accelWorld(pos);
    const float mag = std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
    CHECK(mag < 9.80665f);
    // 1/r² ratio: g * R² / (R+h)²
    const float expected = 9.80665f * R * R / ((R + 1000.f) * (R + 1000.f));
    CHECK(mag == Approx(expected).epsilon(1e-4));
}

TEST_CASE("CentralGravityField: gravity tilts toward planet centre at lateral position", "[gravity]") {
    fl::CentralGravityField g(6'371'000.f, 9.80665f);
    double pos[3] = {1e5, 0.0, 0.0}; // 100 km from Z-axis
    auto a = g.accelWorld(pos);
    CHECK(a[0] < 0.f); // gravity has negative X component (toward origin)
    CHECK(a[1] < 0.f); // still has downward (negative Y) component
    CHECK(a[2] == Approx(0.f).margin(1e-4f));
}

TEST_CASE("CentralGravityField: geodeticAltitude returns distance from surface", "[gravity]") {
    const float R = 6'371'000.f;
    fl::CentralGravityField g(R, 9.80665f);

    double atSurface[3] = {0.0, 0.0, 0.0};
    CHECK(g.geodeticAltitude(atSurface) == Approx(0.0).margin(1.0));

    double at1km[3] = {0.0, 1000.0, 0.0};
    CHECK(g.geodeticAltitude(at1km) == Approx(1000.0).epsilon(1e-4));

    // Lateral position: world-Y does not equal geodetic altitude
    double lateral[3] = {1e5, 0.0, 0.0};
    const double r = std::sqrt(1e5 * 1e5 + static_cast<double>(R) * R);
    CHECK(g.geodeticAltitude(lateral) == Approx(r - R).epsilon(1e-4));
    CHECK(g.geodeticAltitude(lateral) != Approx(lateral[1]).epsilon(1e-2));
}

TEST_CASE("CentralGravityField: earthInstance singleton matches explicit construction", "[gravity]") {
    const auto& e = fl::CentralGravityField::earthInstance();
    fl::CentralGravityField explicit_{6'371'000.f, 9.80665f};
    double pos[3] = {0.0, 0.0, 0.0};
    auto a1 = e.accelWorld(pos);
    auto a2 = explicit_.accelWorld(pos);
    CHECK(a1[1] == Approx(a2[1]).epsilon(1e-6));
}

TEST_CASE("CentralGravityField: guard at planet centre returns zero", "[gravity]") {
    const float R = 6'371'000.f;
    fl::CentralGravityField g(R, 9.80665f);
    // Planet centre is at {0, -R, 0}
    double centre[3] = {0.0, -static_cast<double>(R), 0.0};
    auto a = g.accelWorld(centre);
    CHECK(a[0] == 0.f);
    CHECK(a[1] == 0.f);
    CHECK(a[2] == 0.f);
}

TEST_CASE("IGravityField default: geodeticAltitude returns world-Y", "[gravity]") {
    struct MinimalField : fl::IGravityField {
        std::array<float, 3> accelWorld(const double[3]) const override {
            return {};
        }
    };
    MinimalField f;
    double p[3] = {1e5, 500.0, 2e4};
    CHECK(f.geodeticAltitude(p) == Approx(500.0).margin(1e-4));
}
