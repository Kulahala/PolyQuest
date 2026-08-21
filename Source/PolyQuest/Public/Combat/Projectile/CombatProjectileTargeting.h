#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class AActor;
class ACharacter;
class UAbilitySystemComponent;
class UWorld;
class UProjectileDefinition;

/**
 * Filter parameters for Bow target assistance queries.
 */
struct POLYQUEST_API FCombatProjectileTargetFilter
{
	float MaxHorizontalDistance = 1500.0f;
	float MaxAngleDegrees = 50.0f;
	float MaxHeightDelta = 250.0f;
	float MaxPitchDegrees = 45.0f;

	/** Viewport edge expansion ratio (e.g. 0.06 = 6% overscan) allowing slightly off-screen/partially visible candidates to be targeted. */
	float ScreenMarginRatio = 0.06f;

#if WITH_DEV_AUTOMATION_TESTS
	/** Optional test seam to mock screen projection during headless automation tests. */
	TFunction<bool(const FVector& WorldPoint, FVector2D& OutScreenPos, FVector2D& OutViewportSize)> TestScreenProjectionHook;
	bool bBypassScreenFilterForTesting = false;
#endif
};

/**
 * Evaluated target assist candidate.
 */
struct POLYQUEST_API FCombatProjectileTargetCandidate
{
	TWeakObjectPtr<AActor> TargetActor = nullptr;
	FVector AimPoint = FVector::ZeroVector;
	float AngleDegrees = 0.0f;
	float HorizontalDistance = 0.0f;
};

/**
 * Narrow, read-only helper for projectile targeting qualification, aim point resolution, and candidate sorting.
 * Reuses team, ASC, dead, and invulnerability checks from combat projectile rules.
 */
class POLYQUEST_API FCombatProjectileTargeting
{
public:
	/** Computes the stable upper-torso aim point for an actor (ActorLocation + UpVector * 0.5 * ScaledCapsuleHalfHeight). */
	static FVector GetTargetAimPoint(const AActor* TargetActor);

	/** Returns true if TargetActor qualifies as a hostile, living, non-invulnerable target with a valid ASC relative to SourceActor. */
	static bool IsValidTargetCandidate(const AActor* SourceActor, const UAbilitySystemComponent* SourceASC, const AActor* TargetActor);

	/**
	 * Finds the single best target assist candidate matching all geometric, team, ASC, and visibility constraints.
	 * Returns true if a candidate was found, setting OutCandidate.
	 */
	static bool TryFindBestTargetCandidate(
		const UWorld* World,
		const AActor* SourceActor,
		const UAbilitySystemComponent* SourceASC,
		const FVector& LaunchLocation,
		const FVector& AimDirection,
		const FCombatProjectileTargetFilter& Filter,
		FCombatProjectileTargetCandidate& OutCandidate);

	/** Resolves team tag from an actor implementing ICombatTeamAgent. */
	static FGameplayTag ResolveTeamTag(const AActor* Actor);
};
