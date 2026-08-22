#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "TestPoiseRecoveryGE.generated.h"

/**
 * Isolated native GameplayEffect test class used exclusively by automation tests.
 * Recovers Poise using Data.Poise.Recovery SetByCaller magnitude without mutating Content assets.
 */
UCLASS()
class UTestPoiseRecoveryGE : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UTestPoiseRecoveryGE();
};

/**
 * Isolated native damage GE for testing modifier ordering where Poise modifier is at index 0.
 */
UCLASS()
class UTestLaunchDamageGE_PoiseFirst : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UTestLaunchDamageGE_PoiseFirst();
};

/**
 * Isolated native damage GE for testing modifier ordering where Health modifier is at index 0.
 */
UCLASS()
class UTestLaunchDamageGE_HealthFirst : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UTestLaunchDamageGE_HealthFirst();
};

/**
 * Isolated native GE dealing only Poise damage (-100) to test Poise-only break lifecycle.
 */
UCLASS()
class UTestPoiseDamageOnlyGE : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UTestPoiseDamageOnlyGE();
};
