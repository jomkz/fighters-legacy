# Roadmap

Development is tracked through [GitHub milestones](https://github.com/jomkz/fighters-legacy/milestones).
Each phase has a milestone with individual issues for every workstream task.

## Schedule

| Phase | Weeks | Key Dependency |
|---|---|---|
| 1 — Engine Foundation + FA Bridge + RE Gap Closure | 1–14 | None (start immediately) |
| 2 — Modern-Particles Engine | 10–36 | HAL + FA bridge stable; SH opcodes closed by week 18 |
| 3 — Classic/Parity Mode | 28–42 | All C.2–C.5 RE closed; Phase 2 running |
| 4 — In-Game Mission Editor | 26–38 | Phase 2 Vulkan HAL stable |
| 5 — Linux/macOS Release | ongoing → ~44 | All CI green; MoltenVK verified |
| 6 — Native Open Formats + Free Base Pack | 36–50 | ft-gui extended; modding docs complete |

Total estimated duration: **~52–60 weeks** of focused work.

### Phase 1 Workstreams

| Workstream | Weeks | Dependency |
|---|---|---|
| 1A — Engine Core Setup | 1–10 | None (start immediately) |
| 1B — FA Content Bridge | 2–8 | Phase 1A `IContentPack` interface |
| 1C — RE Gap Closure | 1–14 | Ghidra + FA.EXE; audit OpenFA first |

---

## Critical Path

1. **SH opcode RE (C.2) → SH renderer (3.1) → Parity mode (3.2/3.3)**
   Longest sequential chain. Audit OpenFA first to shorten.

2. **PT_TYPE byte offsets (fighters-codex) → FA bridge flight model translation (2.3)**
   PT_TYPE offsets are needed by the FA bridge to correctly populate `FlightModel` from
   FA's BRF data. Must close before Phase 2 flight model work starts with FA content.

3. **`IContentPack` interface (A.3) → FA bridge (1B) → all Phase 2 asset loading**
   Interface must be stable before the bridge and engine subsystems depend on it.
   Lock the interface in week 2; do not break it after that.

4. **Weapon TOML format → loadout screen (2.5) → dynamic campaign unit pools (2.5)**
   Weapon IDs must be stable before the loadout screen, AI weapons selection, and
   dynamic campaign mission templates can reference them.

5. **Ground/naval unit TOML format → dynamic campaign (2.5) → sandbox unit spawning (2.14)**
   Unit IDs and AI script contracts must be defined before the dynamic campaign generator
   can instantiate unit pools.

6. **Radar & EW (2.13) → AI weapons selection (2.4) → multiplayer balance (2.7)**
   AI must know missile guidance types to select correct weapons and respond to jamming.
   EW parameters affect game balance in multiplayer; lock before server-side enforcement.

---

## Verification / Acceptance Criteria

### Phase 1 — Engine Foundation

- All three CI jobs build clean (Windows/Linux/macOS).
- Vulkan validation layers: zero errors on triangle hello-world.
- MoltenVK smoke test passes on Apple Silicon.
- FA content bridge loads a single `.SH` mesh and displays it in a debug window.

### Phase 2 — Modern-Particles Engine

- FA mission `U01.M` loads and runs to completion via FA bridge.
- Flight model stall speed + fuel burn match design spec for each aircraft type.
- Radar lock, missile fire, and chaff defeat sequence works at all three difficulty tiers.
- AIM-120 fire-and-forget and AIM-7 continuous-lock guidance both function correctly.
- RWR triggers on SA-10 track radar; launch warning sounds; HARM homes on active emitter.
- Progressive damage: light / heavy / critical thresholds produce correct visual + penalty.
- Carrier landing: wire catch at correct speed; bolter triggers go-around correctly.
- In-flight refueling: all three tiers (auto / simplified / manual) functional.
- Wingman commands: all six commands acknowledged and executed by AI.
- Dynamic campaign: frontline advances after objective completion; story mission injects at trigger.
- Sandbox mode: configurable start, no win condition, session saves and resumes.
- Game Master: entity spawn/despawn and weather change take effect in running session.
- Replay: mission records and plays back from cockpit and free-camera views.
- GPU particles render for explosion / smoke / fire.
- Multiplayer: two clients on fl-server complete a cooperative strike mission.
- CI green on all three platforms.

### Phase 3 — Classic/Parity Mode

- Screenshot comparison: classic mode vs. original FA at matching camera angles.
  No structural geometry differences.
- All 1,275 SH files render; damage/LOD states visually correct.
- 65 x86-only effect files produce matching particle geometry.

### Phase 4 — ft-gui Cross-Platform

- All editor panels work on Ubuntu and macOS.
- Round-trip: create a TOML aircraft + glTF mesh; load it in the engine.

### Phase 6 — Open Formats

- A mission using only native YAML + glTF + TOML + OGG content runs without
  the FA content bridge installed.
- `validate-mod` passes on the free base pack when available.
