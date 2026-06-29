// SPDX-License-Identifier: GPL-3.0-or-later
#include "net/SnapshotScheduler.h"

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <vector>

namespace {
// Make a candidate at a given distance with a fixed per-record byte cost.
fl::SnapshotCandidate cand(uint32_t idx, double dist, uint32_t estBytes = 24u) {
    fl::SnapshotCandidate c;
    c.idx = idx;
    c.distSq = dist * dist;
    c.estBytes = estBytes;
    return c;
}

bool contains(const std::vector<uint32_t>& v, uint32_t idx) {
    return std::find(v.begin(), v.end(), idx) != v.end();
}
} // namespace

TEST_CASE("SnapshotScheduler: budget==0 admits every candidate", "[snapshot_scheduler]") {
    std::vector<fl::SnapshotCandidate> cands;
    for (uint32_t i = 1; i <= 50; ++i)
        cands.push_back(cand(i, i * 100.0));
    fl::SchedulerWeights w;
    auto out = fl::selectSnapshotRecords(cands, /*budget=*/0u, w, /*drawDistM=*/200000.0);
    CHECK(out.size() == 50u);
}

TEST_CASE("SnapshotScheduler: byte budget caps the admitted set", "[snapshot_scheduler]") {
    std::vector<fl::SnapshotCandidate> cands;
    for (uint32_t i = 1; i <= 100; ++i)
        cands.push_back(cand(i, i * 100.0, /*estBytes=*/24u));
    fl::SchedulerWeights w;
    // 240-byte budget => at most 10 records of 24 bytes each.
    auto out = fl::selectSnapshotRecords(cands, /*budget=*/240u, w, 200000.0);
    CHECK(out.size() == 10u);
    // Total admitted bytes never exceed the budget (24 B per record here).
    CHECK(out.size() * 24u <= 240u);
}

TEST_CASE("SnapshotScheduler: own entity always admitted even past the budget", "[snapshot_scheduler]") {
    std::vector<fl::SnapshotCandidate> cands;
    for (uint32_t i = 1; i <= 20; ++i)
        cands.push_back(cand(i, /*dist=*/10.0, 24u)); // all very close so they compete hard
    // The own entity is the farthest away — would never rank in under a tiny budget without the override.
    fl::SnapshotCandidate own = cand(999u, /*dist=*/199000.0, 31u);
    own.isOwn = true;
    cands.push_back(own);

    fl::SchedulerWeights w;
    auto out = fl::selectSnapshotRecords(cands, /*budget=*/24u, w, 200000.0); // only ~1 record fits
    REQUIRE(contains(out, 999u));
    CHECK(out.front() == 999u); // admitted first
}

TEST_CASE("SnapshotScheduler: closer entities outrank farther ones", "[snapshot_scheduler]") {
    std::vector<fl::SnapshotCandidate> cands;
    cands.push_back(cand(1u, 190000.0)); // far
    cands.push_back(cand(2u, 500.0));    // near
    cands.push_back(cand(3u, 90000.0));  // mid
    fl::SchedulerWeights w;
    // Budget fits exactly one record (24 B); the nearest must win.
    auto out = fl::selectSnapshotRecords(cands, 24u, w, 200000.0);
    REQUIRE(out.size() == 1u);
    CHECK(out.front() == 2u);
}

TEST_CASE("SnapshotScheduler: higher closing speed boosts relevance", "[snapshot_scheduler]") {
    std::vector<fl::SnapshotCandidate> cands;
    auto slow = cand(1u, 50000.0);
    auto fast = cand(2u, 50000.0); // same distance
    fast.closingSpeed = 400.f;     // bearing down on the peer
    cands.push_back(slow);
    cands.push_back(fast);
    fl::SchedulerWeights w;
    auto out = fl::selectSnapshotRecords(cands, 24u, w, 200000.0);
    REQUIRE(out.size() == 1u);
    CHECK(out.front() == 2u);
}

TEST_CASE("SnapshotScheduler: player-owned entities outrank AI at equal distance", "[snapshot_scheduler]") {
    std::vector<fl::SnapshotCandidate> cands;
    auto ai = cand(1u, 50000.0);
    auto player = cand(2u, 50000.0);
    player.playerOwned = true;
    cands.push_back(ai);
    cands.push_back(player);
    fl::SchedulerWeights w;
    auto out = fl::selectSnapshotRecords(cands, 24u, w, 200000.0);
    REQUIRE(out.size() == 1u);
    CHECK(out.front() == 2u);
}

TEST_CASE("SnapshotScheduler: recency lifts a starved entity into the budget", "[snapshot_scheduler]") {
    // Entity A is always nearer; entity B is farther but starved. With a 1-record budget, A wins until
    // B's recency term dominates.
    fl::SchedulerWeights w;
    const double drawDist = 200000.0;

    auto run = [&](uint64_t bTicksSinceSent) {
        std::vector<fl::SnapshotCandidate> cands;
        auto a = cand(1u, 1000.0);
        a.ticksSinceSent = 0;
        auto b = cand(2u, 150000.0);
        b.ticksSinceSent = bTicksSinceSent;
        cands.push_back(a);
        cands.push_back(b);
        return fl::selectSnapshotRecords(cands, 24u, w, drawDist);
    };

    CHECK(run(0).front() == 1u); // fresh: nearer A wins
    // After enough deferral, B's recency term overtakes A's distance advantage.
    bool bEventuallyWins = false;
    for (uint64_t t = 1; t <= w.recencyCapTicks && !bEventuallyWins; ++t)
        bEventuallyWins = (run(t).front() == 2u);
    CHECK(bEventuallyWins);
}

TEST_CASE("SnapshotScheduler: deterministic idx tie-break", "[snapshot_scheduler]") {
    // Identical relevance (same distance, no other signal) => admitted in ascending idx order.
    std::vector<fl::SnapshotCandidate> cands;
    cands.push_back(cand(30u, 1000.0));
    cands.push_back(cand(10u, 1000.0));
    cands.push_back(cand(20u, 1000.0));
    fl::SchedulerWeights w;
    auto out = fl::selectSnapshotRecords(cands, /*budget=*/48u, w, 200000.0); // fits 2 of 3
    REQUIRE(out.size() == 2u);
    CHECK(out[0] == 10u);
    CHECK(out[1] == 20u);
}
