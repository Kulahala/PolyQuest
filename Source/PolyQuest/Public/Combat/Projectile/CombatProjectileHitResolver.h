#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"

class AActor;
class UAbilitySystemComponent;
class UGameplayEffect;
class UObject;

/** Input request to the narrow shared projectile GameplayEffect delivery path. */
struct POLYQUEST_API FCombatProjectileHitRequest
{
	TWeakObjectPtr<AActor> SourceActor;
	TWeakObjectPtr<UAbilitySystemComponent> SourceAbilitySystemComponent;
	TSubclassOf<UGameplayEffect> DamageGameplayEffectClass;
	float AbilityLevel = 1.0f;
	FGameplayTag SetByCallerMagnitudeTag;
	float SetByCallerMagnitude = 0.0f;
	TMap<FGameplayTag, float> SetByCallerMagnitudes;
	float GuardStaminaDamage = 0.0f;
	const UObject* SourceObject = nullptr;
	FHitResult HitResult;
};

/** Shared validation and GAS delivery for projectile impact candidates. */
class POLYQUEST_API FCombatProjectileHitResolver
{
public:
	/** Returns true only when the target accepted the resulting GameplayEffect spec or consumed it via player defense. */
	static bool TryResolveHit(const FCombatProjectileHitRequest& Request);
};
