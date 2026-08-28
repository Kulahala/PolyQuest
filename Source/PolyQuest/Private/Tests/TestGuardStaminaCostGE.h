#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "TestGuardStaminaCostGE.generated.h"

/**
 * Isolated native GameplayEffect test class used exclusively by automation tests.
 * Applies Guard Stamina cost using Data.Stamina.GuardDamage SetByCaller magnitude without mutating Content assets.
 */
UCLASS()
class UTestGuardStaminaCostGE : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UTestGuardStaminaCostGE();
};
