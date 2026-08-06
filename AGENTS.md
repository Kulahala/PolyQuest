# AGENTS.md

Guidance for coding agents working in PolyQuest.

## Operating Style

- Respond in Chinese by default. Give direct conclusions and identify the exact uncertainty when evidence is incomplete.
- Read the real repository, config, source, assets, and live-editor state before changing behavior or making architectural claims.
- Preserve user changes. Before editing, state the intended change briefly; ask before destructive, high-risk, irreversible, or scope-expanding work.
- Prefer the smallest change that satisfies the approved scope. Do not add speculative frameworks, generic systems, migration compatibility paths, or hidden refactors.
- The main agent owns architecture, accepted plans, validation interpretation, review conclusions, documentation, staging, and commits. A child can implement only a bounded, approved slice and cannot change policy, scope, ownership, or architecture.

## Project And Build

- PolyQuest is a Windows UE 5.8 C++ project. The runtime module is `PolyQuest`; use its module name and API macro rather than identifiers copied from the retired `Test` project.
- The current project is the generated UE 5.8 Third Person C++ template. `Variant_Combat`, `Variant_Platforming`, and `Variant_SideScrolling` are template/reference material, not PolyQuest gameplay architecture.
- PolyQuest is a GAS-first, single-player stylized action RPG with new assets. Do not copy the old Test FSM, Test assets, Test save schema, or Marketplace content into this project as a shortcut.
- `PolyQuest.Build.cs` links `GameplayAbilities`, `GameplayTags`, and `GameplayTasks`. `ABaseCharacter` owns the single-player ASC and `UCharacterAttributeSet`; `Config/Tags/PolyQuestGameplayTags.ini` owns the project's nine current tags.
- No `GameplayAbility`, `GameplayEffect`, combat action activation, or authored combat asset exists yet. Do not describe a proposed GAS design as implemented architecture.
- The user owns manual `PolyQuestEditor` compilation, PIE validation, packaging, and commit approval unless they explicitly delegate one of those actions. Do not invoke UBT, `Build.bat`, packaging, or Rider build tools without explicit permission.

```powershell
# Generate project files: right-click PolyQuest.uproject -> Generate Visual Studio project files
# Compile manually: build PolyQuestEditor (Development Editor) in Visual Studio 2022
# Launch editor: open PolyQuest.uproject
```

## Evidence, Scope, And Content Boundaries

- Source, config, `.uproject`, `.Build.cs`, and authored assets are primary truth. When documents disagree, use: source/assets/config > `ARCHITECTURE.md` > `plan.md` > `ROADMAP.md` > `README.md` > `AGENTS.md`.
- C++ belongs under `Source/PolyQuest/`. Keep engine includes before project includes; use forward declarations where practical; generated headers remain the final include.
- Never manually patch `.uasset` or `.umap` files. Use the live Unreal Editor through the documented MCP route.
- Imported Marketplace/Fab assets are read-only unless the user explicitly authorizes edits. Do not import, delete, reparent, or retarget assets as incidental setup work.
- `Content/Assets/` is the user-owned external resource reservoir for raw packages and source files. Its presence under `Content/` does not make a package an imported Unreal asset, PolyQuest product asset, or approved production baseline. Do not bulk import, move, duplicate, reparent, retarget, wire it into product Blueprints/AnimBPs/DataAssets, or stage raw resource files without a specific accepted stage and explicit user approval.
- Generated folders remain untracked: `Binaries/`, `DerivedDataCache/`, `Intermediate/`, `Saved/`, `.vs/`, `.idea/`, solution files, and `.slnx` workspaces. Track `Config/`, `Content/`, `Source/`, `.uproject`, project docs, and project-owned plugins.
- Git LFS is mandatory for `*.uasset` and `*.umap`. Before committing LFS-routed assets, verify at least one staged asset is an LFS pointer.
- Formal review and closeout ignore user-owned `Content/*.uasset` changes by default unless the user explicitly asks to inspect or include them.

## Stage Workflow And Delegation

- Use `ue-stage-workflow` as the outer lifecycle for non-trivial UE C++, GAS, Blueprint, source-plus-asset, persistence, AI, combat, or staged documentation work. Local repository rules and an already accepted `plan.md` override generic routing.
- For a new stage, inspect live project state, resolve material design decisions, write an implementation-ready `plan.md`, implement narrowly, let the user compile/validate, review, synchronize documents, and wait for explicit commit approval.
- Before the first source or text mutation of a non-trivial stage, record this delegation decision in `plan.md`:

```text
Plan explorers: 0 | 1 | 2
Implementation executors: 0 | 1 | 2
Complex Executor: none | one scoped lifecycle-sensitive implementation (replaces Executor slots)
Main parallel work: <specific non-overlapping task | none; choose 0 when a child would only make Main wait>
Reason: <concrete fit or reason not to delegate>
```

- Default to `0 / 0`. Use `1` only when a concrete benefit exceeds briefing and integration cost; `2` is a ceiling for truly independent questions or closed write sets with separate checks. Standard `Explorer` and `Executor` use `gpt-5.6-luna` with `medium` reasoning. `Complex Executor` and fresh `Reviewer` use `xhigh`.
- Main owns architecture and initial source reconstruction. A child must have an independently finishable artifact, explicit sources, non-overlap proof, checks, and a Main task that advances in parallel. Main must work that declared task rather than wait; one bounded wait is permitted only for a direct critical-path dependency.
- In PolyQuest, shared ASC, AttributeSet, Ability, Gameplay Tag, input, persistence, action-arbitration, and lifecycle contracts are never parallel writer territory. A Complex Executor handles one such lifecycle-sensitive slice alone. Editor assets, compilation, PIE, staging, and commits remain Main/user-owned.
- The full role contracts, benefit test, reviewer protocol, child retention, and combined-diff integration gate live in `ue-agent-orchestration`. Do not duplicate those mechanics here; use the skill as the authority.

## Implementation Ownership

- For an approved PolyQuest stage, the main agent leads and implements the scoped source, config, documentation, and permitted Editor work. Explain material ownership, lifecycle, and validation decisions as part of delivery, but do not require the user to construct code file-by-file unless they explicitly request a hands-on walkthrough.
- The user retains ownership of manual `PolyQuestEditor` compilation, PIE/visual acceptance, imported-asset decisions, and final commit approval unless they explicitly delegate one of those actions.
- A user request to inspect, review, explain, or teach remains read-only unless it also authorizes a mutation. A user may opt into a focused hands-on exercise for a specific file or system without changing the default implementation ownership for the rest of the stage.

## Planning, Roadmap, And Documentation

- Read `ARCHITECTURE.md`, `ROADMAP.md`, and the header of `plan.md` before structural or gameplay work.
- `README.md` is the public overview and evidence-conscious status summary.
- `ARCHITECTURE.md` records stable, implemented ownership, state/data flow, source-of-truth rules, and durable asset topology. Update it only after the corresponding implementation has passed its relevant validation and review.
- `ROADMAP.md` owns accepted future milestones, prerequisite order, adoption conditions, and known validation debt. A roadmap item is not permission to implement it.
- `plan.md` is the short-lived active-stage handoff. It contains the approved scope, source/assets/public contracts, execution order, validation, document impact, commit boundary, active feedback, and transient coordination detail.
- Put an accepted future requirement into `ROADMAP.md` by prerequisite and player-loop value. If a new fact materially changes an active plan, pause, explain the changed assumption, update the plan or roadmap deliberately, and obtain approval before proceeding.
- After approved validation and review, move a completed roadmap stage to Done Milestones, mark it `[x]`, retain only its compact durable result, and clear completed working detail from `plan.md` according to its header.
- Record a non-blocking risk only when it names the affected boundary, current evidence, player/technical impact, resolution condition, and owning stage or release gate. Blockers are fixed in the current stage; optional ideas remain Recommendations with adoption conditions.

## Tooling And Editor Boundaries

- Follow the detailed live-Editor, CodeGraph, code-review-graph, and auxiliary-tool protocols in `ue-stage-workflow`; this file records only PolyQuest-specific boundaries.
- Use official `unreal_mcp` first. VibeUE is a documented capability fallback, never a second writer. `UnrealClaude` is not configured for PolyQuest. Before an Editor mutation, prove one focused read works; use one writer, read back the result, and establish a recovery point for bulk or hard-to-reverse work.
- When `.codegraph/` exists, use CodeGraph before text search for C++ symbols and call paths. If it is absent or stale, state that fact and fall back to exact `rg` plus focused source reads; do not create or reindex it as incidental work.
- When `.code-review-graph/` is available, use it only as supplemental change/impact evidence. Review from an explicit baseline, use file-scoped impact with unrelated WIP, and do not treat graph scores or labels as findings by themselves.
- Use Rider for proven IDE reads, Context7 for version-sensitive external APIs, a real browser for browser behavior, and server-memory only for a concrete recurring failure pattern. None bypasses the user-owned build or Editor boundary.

## GAS And Gameplay Guardrails

- PolyQuest is a GAS learning project. For GAS-related work, lead with the runtime contract and explain ownership, lifecycle, responsibility boundaries, and verification; guide the user through relevant Editor or code decisions while the main agent implements the approved scoped C++/Blueprint/config changes. Use a hands-on, file-by-file exercise only when the user explicitly requests it.
- Migrate in vertical slices. Establish the Player Ability lifecycle before porting a combat system from Test.
- Do not retain `EActionState`-style player action arbitration as a parallel source of truth beside GAS. Map activation, cancellation, cost, status, recovery, and async work deliberately to GameplayAbilities, GameplayEffects, GameplayTags, AttributeSets, and AbilityTasks.
- Validate a technical GAS slice with template assets before broad stylized character, weapon, Skeleton, socket, Montage, root-motion, or world replacement. This separates GAS lifecycle failures from asset compatibility failures.
- Keep gameplay authority in native C++/GAS and use Blueprint for authored data and presentation unless a stage explicitly assigns a Blueprint gameplay contract.
- Timer callbacks, AnimNotifies, montage delegates, collision callbacks, perception callbacks, and async ability completions must check current state and object validity before mutating gameplay. They can fire after cancellation, death, interruption, teardown, or object destruction.
- Converge natural completion, interruption, Notify, and delegate paths that recover the same gameplay state through one explicit cleanup/EndAbility path. Missing mandatory configuration must warn or fail at the source; intentional fallbacks require a documented removal condition.
- Do not add multiplayer replication, client authority, rollback, or a generic ability framework beyond GAS merely because it may be useful later. PolyQuest is currently single-player unless the roadmap deliberately changes that product boundary.

## Review, Validation, Memory, And Git

- Formal reviews report bugs, regressions, lifecycle/authority risks, and missing validation first, ordered by severity with file/line evidence. Strict and stage-end reviews follow the main-review plus fresh-Reviewer route in `ue-stage-workflow` and `ue-agent-orchestration`.
- Before requesting a user compile for a non-trivial C++ change, perform lightweight static review, `git diff --check`, and a targeted memory query for the touched Unreal/GAS error family. A blocking review finding returns the stage to repair and relevant revalidation.
- Do not claim compilation, PIE, visual verification, packaging, or deployment success without actual evidence or explicit user confirmation. Separate tool/static evidence, user-confirmed validation, and remaining gaps.
- Do not use `git add -A` in a non-clean worktree. Stage only approved paths and wait for validation, review, document checks, and explicit approval before committing.
- Feature commits use `[Feature] Chinese title (English Title)`; non-trivial gameplay commits begin with `## Core Changes` and do not claim validation that did not occur.
