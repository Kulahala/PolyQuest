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
- PolyQuest originated from the UE 5.8 Third Person C++ template. The generated ThirdPerson and `Variant_Combat` / `Variant_Platforming` / `Variant_SideScrolling` closures were retired in TODO-00C; do not reintroduce them as PolyQuest gameplay architecture.
- PolyQuest is a single-developer, single-player stylized action RPG being rebuilt from Test's validated combat ideas while replacing the runtime architecture with GAS and new assets. Reuse proven behavior contracts selectively, but do not copy the old Test FSM, Test save schema, or Marketplace content as a shortcut; GAS remains PolyQuest's runtime source of truth.
- `PolyQuest.Build.cs` links `GameplayAbilities`, `GameplayTags`, and `GameplayTasks`. `ABaseCharacter` owns the single-player ASC and `UCharacterAttributeSet`; `Config/Tags/PolyQuestGameplayTags.ini` owns the project's Gameplay Tag taxonomy.
- TODO-01A provides a user-compiled and PIE-validated native GAS light-attack lifecycle. Its GA/GE, Montage, AnimBP, player Blueprint, and input assets remain deliberately local mutable authoring WIP, so no commit should describe the local fixture as clean-checkout reproducible.
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
- Treat a large feature or migration as a sequence of bounded vertical slices. Split it when it contains multiple player-facing outcomes, independent lifecycle or authority contracts, or separate compile/PIE gates. Each slice must state one primary runtime question, prerequisites, owned files/assets, success criteria, validation gate, and commit boundary. Prefer dependency-ordered slices over file-by-file fragments, and do not start the next slice until the current slice has sufficient evidence. `ROADMAP.md` records the durable sequence; `plan.md` records only the active slice.
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
- In PolyQuest, shared ASC, AttributeSet, Ability, Gameplay Tag, input, persistence, action-arbitration, and lifecycle contracts are never parallel writer territory. For an approved GAS stage, Main is the primary implementation owner and may mutate the planned GAS C++/Blueprint/config/asset slice; child agents remain read-only unless the current request explicitly delegates one exact, non-overlapping GAS slice. Editor assets, compilation, PIE, staging, and commits remain Main/user-owned unless explicitly delegated.
- The full role contracts, benefit test, reviewer protocol, child retention, and combined-diff integration gate live in `ue-agent-orchestration`. Do not duplicate those mechanics here; use the skill as the authority.

## Implementation Ownership

- For an approved PolyQuest stage, Main leads and implements the scoped non-GAS and GAS source/config/documentation work, plus permitted Editor authoring. During implementation, give appropriately sized teaching where it improves the current decision, Editor configuration, validation, or debugging: explain the runtime problem, ownership/lifecycle boundary, important call path, material tradeoff, and remaining verification when relevant. Do not turn every coherent change set into a mandatory multi-message tutorial or block stage progress on a teaching confirmation. Detailed source-level/GAS-system decomposition is a separate, user-requested learning pass that defaults to after project completion; do it earlier only when the user explicitly asks. User-led hands-on exercises remain available when the user explicitly asks to write a specific slice, but they are no longer the default.
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
- Use Rider for proven IDE reads, Context7 for version-sensitive external APIs, a real browser for browser behavior, and the `memory` MCP only for a concrete recurring failure pattern. Its configured server ID is `memory`; `@modelcontextprotocol/server-memory` and `mcp-server-memory` are the package and binary names, not valid MCP server IDs. `codex mcp list` showing `enabled` proves only that Codex parsed the configuration; `Auth: Unsupported` is expected for this local stdio server and is not an API-key failure. A current task can still lack `mcp__memory__*` tools because tool injection is separate from configuration discovery. Treat the server as usable only when those tools are injected or the configured stdio command succeeds at `initialize` -> `notifications/initialized` -> `tools/list`; report a successful direct JSON-RPC stdio fallback as a fallback, never as an injected tool call. None bypasses the user-owned build or Editor boundary.

## GAS And Gameplay Guardrails

- PolyQuest is a GAS learning project. The default mode is Main-led implementation with concise, connected explanations during the work. Explain enough for the user to understand the current feature's problem, ownership, lifecycle, key call path, data contract, deliberate non-goals, and focused validation when those facts affect a decision or the next Editor step. Do not scatter explanations file-by-file or turn the implementation route into a compulsory lesson sequence. Provide a systematic high-level-to-detail GAS/source decomposition after project completion when the user asks for it; before then, do so only on an explicit early learning request. User-led hands-on exercises remain supported for explicitly requested practice slices. After implementation, use MCP and source tools for read-only confirmation; do not present static inspection as compile or PIE evidence.
- Keep teaching proportional to the current need. Answer concrete questions and explain material decisions as they arise, but continue the approved stage without waiting for a teaching acknowledgement. Reserve detailed step-by-step or source-by-source exploration for an explicit post-project learning request, unless the user explicitly requests it earlier.
- For small, related implementation types—such as AnimNotify/AnimNotifyState classes, narrow ability helpers, effect adapters, or state/condition helpers—group source files by a coherent feature when several types are expected and the grouping improves findability. Keep every runtime behavior as its own reflected class or type, reuse classes across placements, and do not create a monolithic enum-plus-switch dispatcher. Use the narrowest appropriate base type, such as `UAnimNotifyState` for duration windows. Establish a feature group early when the roadmap already indicates several related types; otherwise avoid a file-only move with no current discoverability benefit.
- Migrate in vertical slices. Establish the Player Ability lifecycle before porting a combat system from Test.
- Do not retain `EActionState`-style player action arbitration as a parallel source of truth beside GAS. Map activation, cancellation, cost, status, recovery, and async work deliberately to GameplayAbilities, GameplayEffects, GameplayTags, AttributeSets, and AbilityTasks.
- Validate a technical GAS slice with template assets before broad stylized character, weapon, Skeleton, socket, Montage, root-motion, or world replacement. This separates GAS lifecycle failures from asset compatibility failures.
- Keep gameplay authority in native C++/GAS and use Blueprint for authored data and presentation unless a stage explicitly assigns a Blueprint gameplay contract.
- Timer callbacks, AnimNotifies, montage delegates, collision callbacks, perception callbacks, and async ability completions must check current state and object validity before mutating gameplay. They can fire after cancellation, death, interruption, teardown, or object destruction.
- Converge natural completion, interruption, Notify, and delegate paths that recover the same gameplay state through one explicit cleanup/EndAbility path. Missing mandatory configuration must warn or fail at the source; intentional fallbacks require a documented removal condition.
- Do not add multiplayer replication, client authority, rollback, or a generic ability framework beyond GAS merely because it may be useful later. PolyQuest is currently single-player unless the roadmap deliberately changes that product boundary.

## Review, Validation, Memory, And Git

- Formal reviews report bugs, regressions, lifecycle/authority risks, and missing validation first, ordered by severity with file/line evidence. Strict and stage-end reviews follow the main-review plus fresh-Reviewer route in `ue-stage-workflow` and `ue-agent-orchestration`.
- Before requesting a user compile for a non-trivial C++ change, perform lightweight static review, `git diff --check`, and a targeted `memory` query for the touched Unreal/GAS error family when the current task exposes its tools. When a compile or runtime failure is confirmed as reusable, write a concise entity through `memory` after the fix and read it back; store the trigger, wrong approach, correct fix, scope, and verification, not raw transient logs. `@modelcontextprotocol/server-memory` is a knowledge graph rather than an append-only note API: call `search_nodes` first; use `create_entities` with `name`, `entityType`, and atomic `observations` when no entity matches; use `add_observations` for an existing entity; then use `open_nodes` for readback. Direct stdio calls use MCP `tools/call`; never manually edit the JSONL. For later source changes, query the same error family again and check the diff/static evidence for recurrence before requesting compilation. Before the first write, configure `MEMORY_FILE_PATH` to an explicit durable JSONL path; the package default is `memory.jsonl` in its server directory and is not an approved project memory location. If `memory` is not injected into the current task, state that fact; when the documented handshake succeeds and persistence is required, the direct fallback may write and must record its create/update result plus readback. If the handshake fails, provide the proposed memory text instead of claiming a write.
- Do not claim compilation, PIE, visual verification, packaging, or deployment success without actual evidence or explicit user confirmation. Separate tool/static evidence, user-confirmed validation, and remaining gaps.
- Do not use `git add -A` in a non-clean worktree. Stage only approved paths and wait for validation, review, document checks, and explicit approval before committing.
- Feature 标题使用 `[Feature] 中文标题 (English Title)`；不要只写英文标题。非微小的 Feature/Chore 提交正文默认使用中文，只有类名、资产路径、模块名、Gameplay Ability System（GAS）等精确技术术语保留英文。
- 非微小提交从 `## 核心改动` 开始，并按事实按需增加 `## 验证`、`## 复核`、`## 文档更新` 或 `## 范围说明`。每一条说明应以中文描述做了什么、为何重要或明确未包含什么；不要把英文句子、`Core Changes`、`Validation`、`Review`、`Scope` 等章节名当作默认模板。
- 只记录真实验证：用户确认的编译或 PIE、已执行的静态检查、未能启动的独立 Reviewer 都要如实区分。没有实质信息的章节不创建，也不为了格式补写验证结论。
