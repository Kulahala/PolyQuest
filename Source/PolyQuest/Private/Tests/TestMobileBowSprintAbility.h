#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "TestMobileBowSprintAbility.generated.h"

/**
 * Minimal native-only Sprint ability carrying the real Ability.Movement.Sprint
 * and State.Movement.Sprinting contracts for testing CancelSprintAbility().
 */
UCLASS()
class UTestMobileBowSprintAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UTestMobileBowSprintAbility();
};
