# plan.md — AI Agent Collaboration File

## Rules

1. Read this file before continuing a PolyQuest stage.
2. Keep feedback, active plan, validation evidence, and unresolved blockers scoped to the current stage.
3. The implementation author cannot mark a stage as review-approved.
4. After a stage is complete and review-approved, clear transient detail below this header rather than turning this file into project history.
5. Durable architecture belongs in `ARCHITECTURE.md`; future direction belongs in `ROADMAP.md`.

---

## Active Stage: TODO-00C - Template Cleanup v1

### Goal

Replace the temporary Third Person validation route with a product-owned prototype map and desktop input route. Retire only template source and assets whose Editor dependency closure is proven empty, while preserving the validated TODO-01A player ability slice and unrelated imported-asset WIP.

### Decisions

- Create `/Game/Maps/Scene01` through Unreal Editor `Save Current As`. If its dependency closure retains unwanted Third Person content, create a clean prototype map and re-place only the product validation fixture.
- Keep the reflected names `APolyQuestGameMode` and `APolyQuestPlayerController`; relocate their headers to `Public/Framework` and implementations to `Private/Framework`, preserving Blueprint parent compatibility.
- Convert the active PlayerController to desktop-only input. `IMC_Default` and `IMC_MouseLook` must both be configured in `DefaultMappingContexts` before the mobile-only property and code path are removed.
- Preserve `ABaseCharacter`, `APlayerCharacter`, `UCharacterAttributeSet`, Gameplay Tags, TODO-01A source, and product Blueprint WIP.

### Current Evidence And Closeout Status

- Baseline commit: `9f1d9e1` (`[Feature] Player Light Attack Lifecycle`). The worktree contains broad unrelated imported assets and template deletions; never use bulk staging.
- `Content/Maps/Scene01.umap` exists. `GameDefaultMap` and `EditorStartupMap` now point to `/Game/Maps/Scene01`.
- The retained GameMode and PlayerController sources now live under `Public/Framework` and `Private/Framework`. The new files are intentionally untracked until the approved stage commit; the old root paths are deleted in the working tree.
- Post-move CodeGraph still returned the old root paths, so its index is stale for this change. Direct source reads confirm the moved definitions and are the temporary static fallback.
- The user confirmed `PolyQuestEditor` opens and the Scene01 desktop input, movement, look, jump, and TODO-01A validation path still pass in PIE.
- Retained product C++ has no direct reference to `APolyQuestCharacter` or any `Variant_*` symbol. The three Variant Content directories and their 76 native source files, plus the Variant Build.cs include paths, have been removed after the product-source/config/product-asset static audit.
- The user deleted `/Game/ThirdPerson` through the Editor. Reference Viewer evidence shows `Scene01` has no ThirdPerson dependency; incoming links from the retiring `Lvl_ThirdPerson` to retained product Blueprints are not reverse dependencies. `BP_PlayerController` reads only product input and GameMode dependencies.
- With that Editor evidence, Main removed `APolyQuestCharacter` and the five legacy `TP_ThirdPerson` redirects.
- The remaining `ThirdPersonCPP` references were inactive template editor defaults: the obsolete `SimpleMapName` was removed and the tracked Content Browser default now starts at `/Game`. Existing editor preview-profile WIP in `DefaultEditor.ini` remains outside this stage.
- The user confirmed a second `PolyQuestEditor` compilation and Editor open after Variant source removal, with no Variant-related error.
- The user confirmed the final `PolyQuestEditor` compilation/open and Scene01 PIE pass after native template-class and redirect removal, with no missing parent, redirect, map-load, input, or TODO-01A regression.
- Scoped static checks passed: `git diff --check`; exact source/config/project-metadata scans show no remaining retired-template references; all 5 ThirdPerson and 61 Variant content packages plus all 76 Variant native files are represented by deletions; `Scene01.umap` is routed through Git LFS.
- Main normal review and a labeled adversarial fallback found no blocking issue. The required fresh `gpt-5.6-luna / xhigh` Reviewer did not run: its request failed before analysis with a service-side HTTP 503, so no independent review result is claimed.
- The current session has no live Unreal Editor or server-memory MCP tool. Asset creation, reference checks, Blueprint configuration, redirector fix-up, and map save must be completed in the Editor by the user unless a live writer becomes available later.
- Do not stage imported packs, mutable ability/animation/player/input WIP, or unrelated External Actors as part of this template-cleanup commit.

### Implementation Checklist

- [x] User created and opened `/Game/Maps/Scene01`, preserving the active product validation fixture.
- [x] User moved `IMC_MouseLook` into `BP_PlayerController.DefaultMappingContexts` alongside `IMC_Default` and saved the Blueprint.
- [x] Main relocated the retained GameMode and PlayerController source files, added `POLYQUEST_API`, and removed the desktop-inapplicable touch path.
- [x] Main updated the default map paths to `/Game/Maps/Scene01`.
- [x] Main removed the Variant native source closure and Variant Build.cs include paths after scoped static evidence.
- [x] User supplied the required Reference Viewer evidence and deleted the ThirdPerson asset closure through the Editor; Main removed the matching native template class and legacy redirects.
- [x] User compiled `PolyQuestEditor` and validated Scene01 startup, desktop input, movement, look, jump, LMB cost, and target damage.
- [x] User recompiled and opened `PolyQuestEditor` after the native template class and redirect deletion, then rechecked Scene01 PIE for missing parent, redirect, map-load, input, or TODO-01A regressions.
- [x] Main completed scoped static checks, normal review, and an adversarial fallback after the independent Reviewer service failure; architecture, roadmap, and plan are synchronized. Await explicit commit approval.

### Commit Boundary

Include only the approved prototype-map closure, `BP_PlayerController`, verified ThirdPerson/Variant retirements, Framework source moves, the specific template-default config hunks and Build.cs changes, and TODO-00C documentation. Exclude imported packs, mutable ability/animation/player/input WIP, unrelated External Actors, editor preview-profile/config WIP, and every asset without Reference Viewer evidence.
