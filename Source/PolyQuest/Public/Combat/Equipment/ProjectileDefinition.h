#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ProjectileDefinition.generated.h"

class UGameplayEffect;
class UStaticMesh;
class UNiagaraSystem;

/**
 * Immutable authored data definition for travelling combat projectiles.
 * Contains display, initial velocity, collision, and damage effect data.
 * Does not store runtime handles, targets, or active flight state.
 */
UCLASS(BlueprintType)
class POLYQUEST_API UProjectileDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Validates authored parameters; returns true if all parameters are valid and finite. */
	virtual bool IsValidProjectileDefinition(FString& OutReason) const;

	/** Initial flight speed in cm/s. Must be positive and finite. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Movement", meta = (ClampMin = "1.0", UIMin = "100.0"))
	float InitialSpeed = 3000.0f;

	/** Maximum flight speed in cm/s. Must be >= InitialSpeed and finite. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Movement", meta = (ClampMin = "1.0", UIMin = "100.0"))
	float MaxSpeed = 3000.0f;

	/** Maximum lifespan in seconds before automatic cleanup. Must be positive and finite. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Movement", meta = (ClampMin = "0.01", UIMin = "0.5"))
	float LifespanSeconds = 5.0f;

	/** Radius of the collision sphere in cm. Must be positive and finite. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Collision", meta = (ClampMin = "0.1", UIMin = "1.0"))
	float CollisionRadius = 12.0f;

	/** The GameplayEffect applied to target ASC upon a valid hostile hit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Damage")
	TSubclassOf<UGameplayEffect> DamageGameplayEffectClass;

	/** Optional static mesh visual presentation for the projectile. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Display")
	TObjectPtr<UStaticMesh> ProjectileMesh;

	/** Display rotation offset relative to the projectile forward direction. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Display")
	FRotator DisplayRotationOffset = FRotator::ZeroRotator;

	/** Display scale multiplier. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Display")
	FVector DisplayScale = FVector::OneVector;

	/** When true, Bow Release will attempt to pick a valid hostile target within angle and distance cones. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|TargetAssist")
	bool bEnableTargetAssist = false;

	/** Maximum horizontal distance in cm for target assist selection. Must be positive and finite. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|TargetAssist", meta = (EditCondition = "bEnableTargetAssist", ClampMin = "100.0", UIMin = "500.0"))
	float TargetAssistMaxDistance = 1500.0f;

	/** Maximum horizontal half-angle in degrees from pointer direction for target assist selection. Must be > 0 and <= 180. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|TargetAssist", meta = (EditCondition = "bEnableTargetAssist", ClampMin = "1.0", ClampMax = "180.0", UIMin = "10.0", UIMax = "180.0"))
	float TargetAssistMaxAngleDegrees = 50.0f;

	/** Maximum absolute height difference in cm between launch socket and target aim point. Must be positive and finite. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|TargetAssist", meta = (EditCondition = "bEnableTargetAssist", ClampMin = "1.0", UIMin = "50.0"))
	float TargetAssistMaxHeightDelta = 250.0f;

	/** Maximum absolute pitch angle in degrees from horizontal for target assist selection. Must be > 0 and < 90. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|TargetAssist", meta = (EditCondition = "bEnableTargetAssist", ClampMin = "1.0", ClampMax = "89.0", UIMin = "5.0", UIMax = "60.0"))
	float TargetAssistMaxPitchDegrees = 45.0f;

	/** When true, projectile will perform limited homing towards the target selected at launch. Requires bEnableTargetAssist=true. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Homing")
	bool bEnableLimitedHoming = false;

	/** Delay in seconds before homing steering activates, during which the projectile flies straight in its initial launch direction. Must be non-negative, finite, and < LifespanSeconds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Homing", meta = (EditCondition = "bEnableLimitedHoming", ClampMin = "0.0", UIMin = "0.0"))
	float HomingStartDelaySeconds = 0.06f;

	/** Maximum homing flight duration in seconds before abandoning tracking and flying straight. Must be positive, finite, and <= LifespanSeconds. Defaults to 5.0s (matching LifespanSeconds). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Homing", meta = (EditCondition = "bEnableLimitedHoming", ClampMin = "0.01", UIMin = "0.1"))
	float HomingDurationSeconds = 5.0f;

	/** Maximum turning rate in degrees per second. Must be positive and finite. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Homing", meta = (EditCondition = "bEnableLimitedHoming", ClampMin = "1.0", UIMin = "30.0"))
	float HomingTurnRateDegreesPerSecond = 120.0f;

	/** Maximum total accumulated deflection angle in degrees relative to the initial launch direction. Must be positive and finite. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Homing", meta = (EditCondition = "bEnableLimitedHoming", ClampMin = "1.0", UIMin = "15.0", UIMax = "360.0"))
	float HomingMaxTotalTurnDegrees = 60.0f;

	/** Optional Niagara flight trail particle system. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|VFX")
	TObjectPtr<UNiagaraSystem> FlightTrailSystem = nullptr;

	/** Optional socket name on the projectile mesh for trail attachment. If None or missing, attaches to mesh root. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|VFX")
	FName FlightTrailSocketName = NAME_None;

	/** Maximum duration in seconds to wait for a detached flight trail to naturally finish before destroying the projectile actor. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|VFX", meta = (ClampMin = "0.01", UIMin = "0.1"))
	float FlightTrailFinishTimeoutSeconds = 0.35f;
};
