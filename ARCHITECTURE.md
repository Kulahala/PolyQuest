# PolyQuest Architecture

## Current Verified State

PolyQuest is a UE 5.8 Windows C++ project created from the Third Person template. The only runtime module is `PolyQuest`.

The generated module currently declares these public dependencies:

```text
Core, CoreUObject, Engine, InputCore, EnhancedInput, AIModule,
StateTreeModule, GameplayStateTreeModule, UMG, Slate
```

The live UE editor resolves `GameplayAbilities` as enabled. The game module does not yet reference the `GameplayAbilities`, `GameplayTags`, or `GameplayTasks` C++ modules, and the project has no product ASC, AttributeSet, GameplayAbility, GameplayEffect, or gameplay-tag asset/configuration.

## Generated Template Boundary

`Source/PolyQuest/` currently contains the base Third Person classes plus generated `Variant_Combat`, `Variant_Platforming`, and `Variant_SideScrolling` sample code. Those classes are template content, not PolyQuest-owned combat, input, AI, save, or ability contracts.

They remain available as temporary technical references until a separately approved template-retirement stage. New product gameplay must be introduced through a documented PolyQuest stage rather than by extending a generated variant opportunistically.

## Not Yet Established

The following are intentional future design decisions, not implemented facts:

- Player ASC ownership and Avatar/Owner lifecycle.
- Enemy ASC topology.
- AttributeSet fields and persistence ownership.
- Gameplay tag taxonomy.
- Ability activation, cancellation, montage, damage, and GameplayCue contracts.
- Stylized character, weapon, Skeleton, socket, animation, and Motion Warping topology.

These decisions stay in the active `plan.md` until their corresponding stage has passed validation and review.
