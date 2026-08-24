#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "TestExhaustionMoveSpeedGE.generated.h"

/** Native-only move-speed fixture matching the authored Guard and Exhaustion multiplier. */
UCLASS()
class UTestExhaustionMoveSpeedGE : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UTestExhaustionMoveSpeedGE();
};
