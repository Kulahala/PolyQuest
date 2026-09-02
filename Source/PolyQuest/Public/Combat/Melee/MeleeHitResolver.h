#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"

class AActor;
class UAbilitySystemComponent;
class UExecutionLockContext;
class UGameplayEffect;
class UObject;

/** Input to the narrow shared melee GameplayEffect delivery path. */
struct POLYQUEST_API FMeleeHitRequest
{
	AActor* SourceActor = nullptr;
	UAbilitySystemComponent* SourceAbilitySystemComponent = nullptr;
	TSubclassOf<UGameplayEffect> DamageGameplayEffectClass;
	float AbilityLevel = 1.0f;
	FGameplayTag SetByCallerMagnitudeTag;
	float SetByCallerMagnitude = 0.0f;
	TMap<FGameplayTag, float> SetByCallerMagnitudes;
	float GuardStaminaDamage = 0.0f;
	const UObject* SourceObject = nullptr;
	const UExecutionLockContext* ExecutionContext = nullptr;
	FHitResult HitResult;
};

/** Shared validation and GAS delivery for a melee contact candidate. */
class POLYQUEST_API FMeleeHitResolver
{
public:
	/** True only when the target accepted the resulting GameplayEffect spec. */
	static bool TryResolveHit(const FMeleeHitRequest& Request);
};
