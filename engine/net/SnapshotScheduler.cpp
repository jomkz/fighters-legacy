// SPDX-License-Identifier: GPL-3.0-or-later
#include "net/SnapshotScheduler.h"

#include <algorithm>
#include <cmath>

namespace fl {

namespace {
float relevanceScore(const SnapshotCandidate& c, const SchedulerWeights& w, double drawDistM) {
    // Distance term: 1 at the peer, falling to 0 at the draw distance (clamped).
    float distTerm = 1.f;
    if (drawDistM > 0.0) {
        const double dist = std::sqrt(c.distSq);
        distTerm = static_cast<float>(std::clamp(1.0 - dist / drawDistM, 0.0, 1.0));
    }
    const uint64_t recencyTicks = std::min(c.ticksSinceSent, w.recencyCapTicks);
    const float threatTerm = (w.closingSpeedRef > 0.f) ? std::max(0.f, c.closingSpeed) / w.closingSpeedRef : 0.f;
    return w.wDistance * distTerm + w.wRecency * static_cast<float>(recencyTicks) + w.wThreat * threatTerm +
           w.wPlayer * (c.playerOwned ? 1.f : 0.f);
}
} // namespace

std::vector<uint32_t> selectSnapshotRecords(std::vector<SnapshotCandidate>& cands, uint32_t recordByteBudget,
                                            const SchedulerWeights& weights, double drawDistM) {
    // Sort by descending relevance; own entity always first; ascending idx breaks ties so the
    // selection is deterministic (serial-equivalent across the parallel sim tick).
    std::sort(cands.begin(), cands.end(), [&](const SnapshotCandidate& a, const SnapshotCandidate& b) {
        if (a.isOwn != b.isOwn)
            return a.isOwn; // own entity outranks everything
        const float sa = relevanceScore(a, weights, drawDistM);
        const float sb = relevanceScore(b, weights, drawDistM);
        if (sa != sb)
            return sa > sb;
        return a.idx < b.idx;
    });

    std::vector<uint32_t> admitted;
    admitted.reserve(cands.size());
    uint32_t used = 0;
    for (const SnapshotCandidate& c : cands) {
        // Own entity is admitted unconditionally (prediction needs it every tick); for all others,
        // stop once the budget is exhausted. budget == 0 means unlimited.
        if (!c.isOwn && recordByteBudget != 0u && used + c.estBytes > recordByteBudget)
            continue;
        admitted.push_back(c.idx);
        used += c.estBytes;
    }
    return admitted;
}

} // namespace fl
