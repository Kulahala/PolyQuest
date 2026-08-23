#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "TestStaminaRegenGE.generated.h"

/**
 * Persistent no-op GameplayEffect used only to prove Player startup fixture
 * wiring without simulating the authored Stamina recovery numbers.
 */
UCLASS()
class UTestStaminaRegenGE : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UTestStaminaRegenGE();
};
