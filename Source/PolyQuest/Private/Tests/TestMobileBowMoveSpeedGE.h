#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "TestMobileBowMoveSpeedGE.generated.h"

/** Native-only controlled move-speed fixture for Mobile Bow lifecycle automation; it deliberately does not mirror mutable Content tuning. */
UCLASS()
class UTestMobileBowMoveSpeedGE : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UTestMobileBowMoveSpeedGE();
};
