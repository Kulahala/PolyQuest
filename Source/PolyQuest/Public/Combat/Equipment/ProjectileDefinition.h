#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ProjectileDefinition.generated.h"

class UGameplayEffect;
class UStaticMesh;

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
};
