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
- The live editor resolves the `GameplayAbilities` plugin, but `PolyQuest.Build.cs` does not yet reference `GameplayAbilities`, `GameplayTags`, or `GameplayTasks`. That module integration belongs to the approved GAS foundation stage.
- No product ASC, AttributeSet, gameplay tag taxonomy, player action contract, or authored combat asset exists yet. Do not describe a proposed GAS design as implemented architecture.
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
- Generated folders remain untracked: `Binaries/`, `DerivedDataCache/`, `Intermediate/`, `Saved/`, `.vs/`, `.idea/`, solution files, and `.slnx` workspaces. Track `Config/`, `Content/`, `Source/`, `.uproject`, project docs, and project-owned plugins.
- Git LFS is mandatory for `*.uasset` and `*.umap`. Before committing LFS-routed assets, verify at least one staged asset is an LFS pointer.
- Formal review and closeout ignore user-owned `Content/*.uasset` changes by default unless the user explicitly asks to inspect or include them.

## Stage Workflow And Delegation

- Use `ue-stage-workflow` as the outer lifecycle for non-trivial UE C++, GAS, Blueprint, source-plus-asset, persistence, AI, combat, or staged documentation work. Local repository rules and an already accepted `plan.md` override generic routing.
- For a new non-trivial stage, first inspect the current project state, then choose the specialist route, resolve material design decisions, write an implementation-ready `plan.md`, implement narrowly, let the user compile/validate, review, synchronize documents, and wait for explicit commit approval.
- Before the first source or text mutation of an approved non-trivial stage, record the delegation decision in `plan.md` when the stage changes three or more C++ files, a public/config/reflection contract, a lifecycle-sensitive path, or when the staged workflow is explicitly invoked:

```text
Implementation: Main | Executor | Complex Executor
Explorer sidecar: none | <exceptional factual question>
Reason: <concrete fit or reason not to delegate>
```

- A small local fix may stay on the main-thread fast path only when its behavior, validation path, and ownership are already clear; it must not change reflection, module dependencies, assets, persistence, timers, delegates, montage callbacks, teardown, action cancellation, input priority, or damage resolution.
- At most one child is active for the root task. Do not fan out implementation, review, or Explorer work. Do not delegate live Editor mutation, `.uasset`/`.umap` work, compilation, PIE, user-visible visual acceptance, staging, commits, or external publication.
- The main agent performs initial source exploration and architecture. An Explorer is exceptional and factual only. Reuse an Executor only for the same accepted workstream after the main agent has inspected its prior output; never reuse a Reviewer.

## Planning, Roadmap, And Documentation

- Read `ARCHITECTURE.md`, `ROADMAP.md`, and the header of `plan.md` before structural or gameplay work.
- `README.md` is the public overview and evidence-conscious status summary.
- `ARCHITECTURE.md` records stable, implemented ownership, state/data flow, source-of-truth rules, and durable asset topology. Update it only after the corresponding implementation has passed its relevant validation and review.
- `ROADMAP.md` owns accepted future milestones, prerequisite order, adoption conditions, and known validation debt. A roadmap item is not permission to implement it.
- `plan.md` is the short-lived active-stage handoff. It contains the approved scope, source/assets/public contracts, execution order, validation, document impact, commit boundary, active feedback, and transient coordination detail.
- Put an accepted future requirement into `ROADMAP.md` by prerequisite and player-loop value. If a new fact materially changes an active plan, pause, explain the changed assumption, update the plan or roadmap deliberately, and obtain approval before proceeding.
- After approved validation and review, move a completed roadmap stage to Done Milestones, mark it `[x]`, retain only its compact durable result, and clear completed working detail from `plan.md` according to its header.
- Record a non-blocking risk only when it names the affected boundary, current evidence, player/technical impact, resolution condition, and owning stage or release gate. Blockers are fixed in the current stage; optional ideas remain Recommendations with adoption conditions.

## Live Editor MCP

### Official ToolsetRegistry And VibeUE

- The primary live-editor route is the official `unreal_mcp` server. Begin an editor task with `list_toolsets`, then `describe_toolset` for the relevant capability, then `call_tool` using the discovered schema. Do not guess Toolset names, action names, or arguments.
- VibeUE is exposed through the same editor connection as enhanced `unreal.*Service` APIs and Python discovery/execution tools. It is not a second Editor writer. Use it only where the official ToolsetRegistry lacks the required documented capability.
- Before `execute_python_code`, use targeted `discover_python_function`, `discover_python_class`, or `discover_python_module`. Keep Python narrow, print full affected paths/actions, and never use speculative graph reconstruction or broad mutation scripts.
- `UnrealClaude` is not configured for PolyQuest. Do not assume its endpoints, routers, REST ports, or asset APIs exist here.

### Preconditions And Mutation Protocol

- Before an asset, Blueprint, UMG, animation, level, plugin, or project-setting mutation, verify that `UnrealEditor.exe` is running the PolyQuest project and that the configured MCP connection completes one focused read-only call. A tool list, plugin checkbox, process, or status response alone is not mutation proof.
- Use one writer per operation: query, mutate, read back, and save/verify. Do not send the same mutation through native Toolsets and VibeUE.
- Confirm a recovery point before bulk or hard-to-reverse changes. VibeUE Python has no automatic rollback.
- After visible editor work, prefer a viewport/widget screenshot and inspect it. If capture is unavailable, report that limitation and rely on property/graph readback plus user compilation/PIE validation; do not claim visual verification without an actual capture.
- When an editor mutation fails, a secondary route may be tried only when it exposes a distinct documented capability for that exact operation. If both documented paths fail, or the remaining approach requires guessed API calls or repeated workarounds, stop and hand the user concrete manual Editor steps. After user-side work, perform one focused read-only verification.

## CodeGraph

<!-- CODEGRAPH_START -->
- When `.codegraph/` exists, use CodeGraph before `rg`, file reads, or guessed symbol paths for C++ symbols, callers/callees, call paths, and blast-radius checks.
- Prefer MCP `codegraph_explore` or `codegraph_node` when available. The shell fallback is `codegraph explore "<symbol or question>"` and `codegraph node <symbol-or-file>`.
- If CodeGraph is unavailable, stale, or the repository has no `.codegraph/`, state that fact and fall back to exact `rg` plus focused source reads. Do not create or reindex CodeGraph merely because a task needs source exploration; indexing remains a user tooling decision.
<!-- CODEGRAPH_END -->

## Code Review Graph

<!-- code-review-graph MCP tools -->
- When `.code-review-graph/` and its MCP or executable are available, use code-review-graph as a supplemental change-review and impact-analysis tool. It never replaces CodeGraph, the actual diff, or source verification.
- For exploration, use CodeGraph first. Use code-review-graph semantic search only when CodeGraph is unavailable, then fall back to `rg`/direct source reads if graph evidence cannot answer the question.
- For ordinary review, use `detect_changes_tool` and `get_review_context_tool` as context aids; use `get_impact_radius_tool`, `get_affected_flows_tool`, or `query_graph_tool` for concrete caller/callee, impact, and test relationships.
- For strict or stage-end review, use an explicit baseline: `HEAD` for an active uncommitted stage, or the recorded merge-base for a committed branch. Never use the default `HEAD~1` baseline.
- When the review worktree contains only the stage diff, obtain bounded `status`, `detect-changes --brief --base <baseline>`, and `impact --files <reviewed-files> --base <baseline>` evidence. With unrelated WIP, skip aggregate change conclusions and use the filtered Git diff plus file-scoped impact instead.
- Pass a compact graph packet to a Reviewer: baseline, changed symbols, affected external files, and at most ten relevant relationships. The Reviewer independently runs `status` and one file-scoped impact query, then verifies material relations against CodeGraph or source. Raw graph JSON, risk scores, token estimates, and `test gap` labels are not findings by themselves.
- Refresh with `update --skip-flows` only when relevant source edits are not indexed. Do not enable `watch`, daemon installation, embedding providers, cloud embeddings, `install`, or `uninstall` without an approved tooling task.

## Other Tool Routing

- Use Rider MCP, when connected, for live IDE state, Problems View, semantic lookup, refactor previews, configured debug sessions, and runtime inspection. First prove a targeted read works; if a UE C++ symbol does not resolve, fall back to CodeGraph. Rider is not an alternate asset writer and must not bypass the user-owned build boundary.
- Use Context7, when available, for third-party, engine, framework, or API documentation that may vary by version. Verify the project version and local headers before applying a recommendation.
- Use a real browser/Playwright route, when available, for browser UI, web behavior, or network-page verification. Static page inspection is not browser behavior proof.
- Use server-memory, when available, only for targeted recurring compiler/runtime/tool failure patterns. Do not scan it broadly during ordinary exploration or strict review.

## GAS And Gameplay Guardrails

- Migrate in vertical slices. Establish the Player Ability lifecycle before porting a combat system from Test.
- Do not retain `EActionState`-style player action arbitration as a parallel source of truth beside GAS. Map activation, cancellation, cost, status, recovery, and async work deliberately to GameplayAbilities, GameplayEffects, GameplayTags, AttributeSets, and AbilityTasks.
- Validate a technical GAS slice with template assets before broad stylized character, weapon, Skeleton, socket, Montage, root-motion, or world replacement. This separates GAS lifecycle failures from asset compatibility failures.
- Keep gameplay authority in native C++/GAS and use Blueprint for authored data and presentation unless a stage explicitly assigns a Blueprint gameplay contract.
- Timer callbacks, AnimNotifies, montage delegates, collision callbacks, perception callbacks, and async ability completions must check current state and object validity before mutating gameplay. They can fire after cancellation, death, interruption, teardown, or object destruction.
- Converge natural completion, interruption, Notify, and delegate paths that recover the same gameplay state through one explicit cleanup/EndAbility path. Missing mandatory configuration must warn or fail at the source; intentional fallbacks require a documented removal condition.
- Do not add multiplayer replication, client authority, rollback, or a generic ability framework beyond GAS merely because it may be useful later. PolyQuest is currently single-player unless the roadmap deliberately changes that product boundary.

## Review, Validation, Memory, And Git

- Formal review finds bugs, regressions, lifecycle risks, authority shortcuts, and missing validation first. Order findings by severity and cite file/line evidence. When a finding needs a user decision, explain concrete player impact, normal trigger, and whether it affects saves, level progress, or only logs.
- A strict or stage-end review has two passes: the main agent performs normal review, then a fresh Reviewer performs an adversarial review through `ue-agent-orchestration` when the runtime is available. The main agent verifies findings and owns acceptance.
- Every independent Reviewer, including a delta-only repair Reviewer, uses `gpt-5.6-luna` with `xhigh` reasoning and `fork_context: false`. The brief contains outcome, benefit, exact sources/diff, scope, checks, stop condition, return format, unresolved risk, evidence already passed, and explicit non-repetition boundaries.
- A Reviewer starts with one focused CodeGraph query. When code-review-graph is available, it independently runs its required `status` and one file-scoped impact query. It normally reads at most three source files and two targeted expansions unless concrete evidence exposes a non-local dependency.
- The normal Reviewer evidence budget is `600 s`. On timeout, send one return-now request, wait at most `30 s`, then close it and perform a labeled main-thread adversarial fallback. Do not keep repeatedly prompting or reusing the same Reviewer.
- A blocking review finding returns the stage to repair. After a focused repair, use a new delta-only Reviewer for the repair, original finding, and adjacent paths rather than rerunning whole-stage discovery. Reopen the validation relevant to the repair.
- Before requesting a user compile for a non-trivial C++ change, perform a lightweight static review, `git diff --check`, and a targeted memory query derived from the actual touched Unreal/GAS types or likely error family. During a compile/runtime repair loop, query memory before selecting a fix and record reusable failures through the configured memory route when available.
- Do not claim compilation, PIE, visual verification, packaging, or deployment success without actual evidence or explicit user confirmation. State separately what tools verified, what the user validated manually, and what remains unverified.
- Do not use `git add -A` in a non-clean worktree. Use focused staging. Do not commit immediately after implementation; wait for known validation, completed review, document checks, and explicit approval.
- Feature commits use `[Feature] Chinese title (English Title)` and non-trivial gameplay commits include a factual body beginning with `## Core Changes`, followed by focused domain sections and `## Documentation` when applicable. Do not claim unperformed validation in a commit message.
