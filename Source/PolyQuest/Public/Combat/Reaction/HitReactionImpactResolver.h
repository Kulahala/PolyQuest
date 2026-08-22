#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTypes.h"

/**
 * Pure, stateless native resolver for target-local planar impact direction.
 * Direction semantic: Target -> Attacker (in Target's local coordinate space).
 * It is context-free: no dependency on ASC, World, DataAsset, and no logging.
 */
class POLYQUEST_API FHitReactionImpactResolver
{
public:
	/**
	 * Resolves the planar attacker direction in the Target's local coordinate space.
	 * Evaluates finite non-zero HitResult.ImpactNormal first; falls back to finite non-zero (InstigatorLocation - TargetLocation).
	 * Projects to XY, normalizes, transforms to Target local space, and returns ZeroVector on failure.
	 */
	static FVector ResolveImpactDirection(const FGameplayEventData& EventData, const AActor* TargetActor);

	static FVector ResolveImpactDirectionFromContext(
		const FGameplayEffectContextHandle& ContextHandle,
		const AActor* FallbackInstigator,
		const AActor* TargetActor);
};
