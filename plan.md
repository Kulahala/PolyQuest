# plan.md — AI Agent Collaboration File

## Rules

1. Read this file before continuing a PolyQuest stage.
2. Keep feedback, active plan, validation evidence, and unresolved blockers scoped to the current stage.
3. The implementation author cannot mark a stage as review-approved.
4. After a stage is complete and review-approved, clear transient detail below this header rather than turning this file into project history.
5. Durable architecture belongs in `ARCHITECTURE.md`; future direction belongs in `ROADMAP.md`.

---

## No Active Stage

`TODO-01C1: Combat Input Intent Routing And Hold/Release v1` is closed.

- `APlayerCharacter` now routes `PrimaryAttack`, Aim, and four direct Ability Slots through the active `UCombatLoadoutDefinition`; only `Started` requests an Ability, while `Completed` and `Canceled` clear held input state and publish their distinct semantic events.
- The user confirmed the authored Scene01 PIE route. The production `DA_CombatLoadout_StraightSword` maps only `Input.PrimaryAttack -> Ability.Attack.Light`; the local Slot 1 fixture proved direct slot activation and was restored before closeout.
- Current validation deliberately remains keyboard/mouse-only by explicit user decision. Missing gamepad shoulder/trigger mappings are deferred rather than recorded as passed controller support.
- Legacy cleanup is complete: the user deleted zero-referencer `IA_Attack_Light`; `IMC_Default` has no legacy mapping; scoped GA/GE/Blueprint Tag-property inspection and content-name scanning found no `Input.Attack.Light` consumer; the obsolete config Tag was removed.
- The user restarted Unreal Editor and confirmed the Gameplay Tag Registry no longer lists `Input.Attack.Light`; persisted config and live readback now agree.
- Main normal and adversarial reviews found no remaining current-route source blocker. A fresh `gpt-5.6-luna / xhigh` Reviewer was requested but the provider returned HTTP 503, so Main's adversarial review is a fallback rather than independent evidence.
- `Event.Input.*` remains for active Ability event listening only. Generic `AbilityTriggers` must not use it without dedicated event tags or explicit payload-intent validation.
- Default commit scope remains C++, `Config/Tags/PolyQuestGameplayTags.ini`, and stage documentation. Input Actions, IMC, Loadout assets, BP_Player, GA/GE, Montage, AnimBP, maps, retargeting, and the temporary fixture remain local authored WIP.

Choose the next active stage from `ROADMAP.md` and record its scope, evidence gates, delegation decision, and commit boundary here.
