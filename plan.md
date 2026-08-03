# plan.md — AI Agent Collaboration File

## Rules

1. Read this file before continuing a PolyQuest stage.
2. Keep feedback, active plan, validation evidence, and unresolved blockers scoped to the current stage.
3. The implementation author cannot mark a stage as review-approved.
4. After a stage is complete and review-approved, clear transient detail below this header rather than turning this file into project history.
5. Durable architecture belongs in `ARCHITECTURE.md`; future direction belongs in `ROADMAP.md`.

---

## Feedback

<!-- No active feedback. -->

---

## Active Plan

### TODO-00A: Repository And Documentation Bootstrap v1 - In Progress

**Goal**

Establish a clean, independently versioned UE 5.8 PolyQuest baseline without migrating Test gameplay code, assets, or save data.

**Scope**

- Initialize the local repository against `https://github.com/Kulahala/PolyQuest.git` and retain its `main` history.
- Configure Git LFS for Unreal packages and ignore generated IDE/engine output.
- Create PolyQuest-specific `AGENTS.md`, `ARCHITECTURE.md`, `ROADMAP.md`, `plan.md`, and `README.md` from the durable documentation roles used in Test.
- Verify the official Unreal MCP ToolsetRegistry route and VibeUE-enhanced read route.

**Non-Goals**

- No C++ gameplay change, GAS module dependency change, GameplayAbility, AttributeSet, tag creation, asset import, template deletion, project plugin mutation, compilation, PIE, or commit.

**Execution Order**

1. Inspect the generated project, remote `main`, existing module, plugin state, and source/template boundaries.
2. Initialize Git, fetch and track the remote baseline, configure local LFS, then add focused ignore/attribute rules.
3. Create documentation that records verified current facts and the staged GAS-first roadmap without copying Test's FSM claims.
4. Validate repository status, remote tracking, LFS attributes, documentation links, and MCP read-only handshake.
5. Wait for explicit user approval before staging or committing the baseline.

**Validation**

- Confirm `origin/main` is the tracking branch and no remote history was overwritten.
- Confirm generated folders and `.slnx` are ignored while `Config/`, `Content/`, `Source/`, `.uproject`, docs, and `.gitattributes` remain eligible for tracking.
- Confirm `*.uasset` and `*.umap` resolve to Git LFS attributes.
- Confirm official ToolsetRegistry and VibeUE Python both complete a focused read-only project query.

**Document Impact And Commit Boundary**

The future initialization commit is limited to the generated UE 5.8 project baseline, `.gitignore`, `.gitattributes`, and the five project documents. It excludes user-specific IDE output, generated folders, imported Fab assets, and all gameplay implementation.
