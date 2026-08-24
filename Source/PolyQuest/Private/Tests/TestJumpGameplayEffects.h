#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/JumpAbility.h"
#include "GameplayEffect.h"
#include "TestJumpGameplayEffects.generated.h"

/** Instant zero-cost Jump effect used only to exercise the native Jump activation path. */
UCLASS()
class UTestJumpCostGE : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UTestJumpCostGE();
};

/** Persistent observable stand-in for the authored Jump regeneration delay effect. */
UCLASS()
class UTestJumpRegenDelayGE : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UTestJumpRegenDelayGE();
};

/** Test-only Jump class with explicit zero-cost and observable delay authored inputs. */
UCLASS()
class UTestJumpExhaustionAbility : public UJumpAbility
{
	GENERATED_BODY()

public:
	UTestJumpExhaustionAbility();
};
