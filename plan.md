# plan.md - AI Agent Collaboration File

## Rules

1. Read this file before continuing a PolyQuest stage.
2. Keep feedback, active plan, validation evidence, and unresolved blockers scoped to the current stage.
3. The implementation author cannot mark a stage as review-approved.
4. Retain the completed plan and closeout record until the next accepted stage deliberately replaces it.
5. Durable architecture belongs in `ARCHITECTURE.md`; accepted future direction and validation debt belong in `ROADMAP.md`.

---

## Previous Stage: TODO-02C3A - Enemy Death Presentation v1

Baseline: `6df6dde`.

- The user compiled `ABP_Enemy_Goblin` successfully and confirmed Scene01 PIE: idle, chase, and attack deaths enter the terminal presentation; the enemy does not revive or resume action.
- `To Land` is a state alias for `Fall Loop` and `Jump`; `To Falling` is a state alias for `Land` and `Locomotion`. Connecting both aliases to `Dead` covers their selected source states. The `Dead` state has no exit transition.
- The AnimBP reads `AEnemyCharacter::IsDead()` through the presentation-only `bIsDead` cache. Native C2 remains the owner of Health, the Dead Tag, AI, Ability, Montage, Trace, and movement teardown.
- C3A had no native source/config commit candidate; all AnimBP, animation, Blueprint, map, and imported Content remains mutable WIP.

---

## Most Recent Completed Stage: TODO-02C3D - Enemy Ragdoll Death Presentation v1

Baseline: `6df6dde` with existing user-authored `Content/**` and `ROADMAP.md` WIP preserved.

### Objective

Add an immediate, native enemy ragdoll presentation after the existing C2 terminal teardown. `State.Status.Dead` remains the only gameplay survival source. The current C3A AnimBP Dead state remains a controlled fallback when ragdoll is disabled or the Mesh has no Physics Asset; ragdoll does not create a second gameplay state or replace GAS death cleanup.

This v1 deliberately has no hit impulse, directional selection, auto-destroy, loot, reward, respawn, replication contract, or player ragdoll.

### Route And Ownership

```text
Outer: ue-stage-workflow
Primary: ue5-cpp-gameplay
Support: ue5-debug-validation, unreal-mcp
Route reason: the change crosses enemy death teardown, SkeletalMesh physics, Capsule collision, and the existing AnimBP presentation fallback; native lifecycle ownership must stay in Main.
```

```text
Plan explorers: 0
Implementation executors: 0
Complex Executor: none
Main parallel work: none
Reason: AEnemyCharacter death ordering and collision/physics teardown are one lifecycle-sensitive C++ slice. Editor authoring, compilation, PIE, and visual proof remain user-owned.
```

- Main owns C++, the death/physics contract, documentation, static review, and commit boundary.
- The user owns Editor readback, optional Blueprint default inspection, `PolyQuestEditor` compilation, and Scene01 PIE/visual validation.

### Editor Evidence And Gate

Read-only Editor evidence for the current Goblin fixture:

- Mesh: `/Game/PolygonDungeons/Meshes/Characters/SK_Character_Goblin_Warrior_Male`.
- Skeleton: `/Game/PolygonDungeons/Meshes/Characters/SKEL_Character_Dungeon`.
- Physics Asset: `/Game/PolygonDungeons/Meshes/Characters/PHYS_Character_Goblin_Warrior_Male`.
- Physics Asset readback reports 21 bodies and 20 constraints; the representative `Pelvis` body uses the default physics mode.
- `CharacterMesh0` currently uses the `CharacterMesh` profile with query-only collision; the inherited Capsule currently uses a custom Pawn query-and-physics profile. Ragdoll must switch the Mesh to the standard engine `Ragdoll` profile and disable the Capsule collision/overlaps after the physics asset has been validated.
- `WeaponMesh` is the shared fixed-v1 static-mesh display fixture. Read-only Editor MCP confirms the Goblin fixture attaches it below `CharacterMesh0` but currently gives it `BlockAllDynamic` and `QueryAndPhysics`; that is invalid for this fixture because melee uses marker-driven `MeleeTrace`, not physical weapon collision. The same policy prevents both self-contact during ragdoll and SpringArm Camera obstruction during player swings.

The user must only confirm in Editor that the Mesh still references this compatible Physics Asset and that the `Ragdoll` profile is available. No `.uasset` or Physics Asset edit is required for this v1.

### Native Runtime Contract

- Add `bUseRagdollOnDeath` as an `EditDefaultsOnly, BlueprintReadOnly` presentation option on `AEnemyCharacter`, defaulting to `true`; it is configuration, not a second survival state.
- Add a private idempotence guard and `StartDeathRagdoll()` helper. `HandleDeath()` calls it only after the existing Dead Tag, Health clamp, Controller stop, Ability cancellation, and CharacterMovement disable have completed.
- `ABaseCharacter` treats the exact fixed-v1 `WeaponMesh` component as visual-only: on `BeginPlay()` it disables its collision and overlap generation. It does not change the `WeaponMesh` transform, attachment, Blade marker sampling, or `MeleeTrace` delivery path. `StartDeathRagdoll()` applies the same idempotent policy again before enabling Mesh physics, then disables the Capsule collision/overlaps, assigns the Mesh collision profile `Ragdoll`, enables SkeletalMesh physics simulation, and wakes the bodies.
- The existing C2 `EndAbility()` cleanup and `FMeleeHitResolver` Dead checks remain unchanged. No new Gameplay Tag, GameplayEffect, Ability, StateTree branch, AnimNotify, or collision channel is introduced.
- If ragdoll is disabled or the Mesh has no Physics Asset, the existing C3A one-way `Dead` AnimBP state remains the presentation path. The fallback is intentional and is removed only when every enemy presentation is proven to have a valid Physics Asset.

### Non-Goals

- No directional or timed impulse, hit-reaction integration, Poise/stance break, player ragdoll, network replication, corpse pooling, auto-destroy, drops, rewards, respawn, or save/persistence behavior.
- No manual Physics Asset authoring, Blueprint graph mutation, hard-coded Content asset path, or broad collision-profile/config cleanup.
- Do not make AnimBP responsible for enabling physics, changing collision, Health, Tags, AI, or Ability state.

### Validation Matrix

Main static checks:

- Re-read the final `AEnemyCharacter` death path and direct callers with CodeGraph and source inspection.
- Verify the Engine 5.8 SkeletalMesh physics API and the standard `Ragdoll` profile contract against the installed headers/Editor readback.
- Run `git diff --check` and inspect the exact source/doc diff. `code-review-graph` is supplemental and may be stale relative to the working HEAD.

Editor readback (user):

- `BP_Enemy_Goblin` still derives from `AEnemyCharacter`, uses `SKEL_Character_Dungeon`, and retains the compatible Physics Asset.
- `bUseRagdollOnDeath` is enabled on the enemy CDO unless the fallback animation is intentionally being tested.
- The Physics Asset opens without missing bodies/constraints and the standard `Ragdoll` collision profile is present.

Compile (user):

- Compile `PolyQuestEditor` and report the actual result. Main does not invoke UBT, Visual Studio, or Live Coding.

Scene01 PIE (user):

- Death from idle, chase, and active attack enters ragdoll after C2 teardown; the active Montage, `State.Action.Attacking`, Trace Window, StateTree, focus, and movement remain cleared.
- The mesh falls and settles against the level; the original Capsule no longer blocks the player or keeps the corpse upright.
- During ordinary player weapon swings, the SpringArm must not shorten because it hits the owning `WeaponMesh`. On enemy death, the equipped weapon remains visually attached but cannot collide with the simulated corpse or level.
- A dead enemy cannot reacquire Sight, attack, receive or deliver meaningful melee damage, revive, or restart animation-driven movement. Health remains `0`.
- Existing player attack, enemy attack, same-team rejection, invulnerability rejection, and C3A fallback behavior regress cleanly.
- Optional fallback check: disable `bUseRagdollOnDeath`, confirm the C3A terminal animation still works, then restore the default.

### Documentation And Commit Boundary

- C3A closeout is recorded in `ARCHITECTURE.md` and `ROADMAP.md` before this stage's implementation details are finalized.
- At C3D closeout, `ARCHITECTURE.md` may record only the stable rule that native enemy death can hand presentation to a validated ragdoll while the ASC Dead Tag and C2 teardown remain authoritative; AnimBP remains a read-only fallback.
- At C3D closeout, `ROADMAP.md` moves this stage to Done Milestones only after user compile, PIE, and strict review evidence.
- Native candidate paths: `Source/PolyQuest/Public/Character/BaseCharacter.h`, `Source/PolyQuest/Private/Character/BaseCharacter.cpp`, `Source/PolyQuest/Public/Character/Enemy/EnemyCharacter.h`, `Source/PolyQuest/Private/Character/Enemy/EnemyCharacter.cpp`, the Unity Build-safe `Source/PolyQuest/Private/Combat/Melee/MeleeHitResolver.cpp` repair, and exact C3D documentation hunks. No Content or Physics Asset files are candidates.
- Do not use `git add -A`, stage Content WIP, or commit during this turn without explicit approval.

### Current Status

- C3A user compile/PIE: confirmed by user.
- Goblin Physics Asset and collision baseline: confirmed by read-only Editor MCP.
- C3D native implementation: complete. `ABaseCharacter` disables collision and overlap generation only for the exact fixed-v1 `WeaponMesh` display component during `BeginPlay()`. `AEnemyCharacter::StartDeathRagdoll()` repeats that idempotent terminal policy before disabling the Capsule collision/overlaps, applying `Ragdoll`, starting default-body physics, and waking the bodies after C2 teardown.
- C3D static preflight and normal review: complete. Main re-read the final Base/Enemy death, Ability teardown, Controller death gate, resolver, and fixed-weapon collision paths with CodeGraph; refreshed code-review-graph for the exact five C++ paths; checked the Unity Build helper namespace surface; and ran `git diff --check`. The graph reports no automated coverage for `BeginPlay()`, `DisableFixedWeaponDisplayCollision()`, `HandleDeath()`, `StartDeathRagdoll()`, or `TryResolveHit()`; this is coverage context, not a runtime finding.
- C3D compile: user first reported Unity Build `C2084` because `CharacterAttributeSet.cpp` and `MeleeHitResolver.cpp` both defined anonymous-namespace `GetDeadTag()`. The Resolver helper was renamed to `GetMeleeDeadTag()`; the reported `C2440` was cascading. The user subsequently confirmed recompilation passed.
- C3D PIE: user confirmed the focused C3D test route passed after the repair. Main did not invoke a build, Editor, or PIE session.
- C3D strict review: Main normal review plus a separately performed Main adversarial fallback found no unresolved P0-P2 native/GAS blocker. `gpt-5.6-luna / xhigh` remained unavailable, so no independent review is claimed.
- Debt handoff: C3D's fixed-name `WeaponMesh` collision guard is accepted only as a temporary fixture. `TODO-03A` owns a weapon component or equipment-data collision policy that preserves marker-driven melee delivery while preventing visual-weapon camera and corpse-physics interaction.
