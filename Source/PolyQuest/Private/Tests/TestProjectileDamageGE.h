#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "TestProjectileDamageGE.generated.h"

/**
 * Isolated transient GameplayEffect subclass used exclusively by projectile automation tests.
 * Deducts 25.0 Health points instantly without mutating global UGameplayEffect CDO.
 */
UCLASS()
class UTestProjectileDamageGE : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UTestProjectileDamageGE();
};
