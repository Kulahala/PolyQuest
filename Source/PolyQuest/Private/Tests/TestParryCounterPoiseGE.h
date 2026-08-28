#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "TestParryCounterPoiseGE.generated.h"

/**
 * Isolated native GameplayEffect test class used exclusively by automation tests.
 * Applies counter-Poise damage using Data.Poise.Parry SetByCaller magnitude without mutating Content assets.
 */
UCLASS()
class UTestParryCounterPoiseGE : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UTestParryCounterPoiseGE();
};
