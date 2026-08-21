#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Combat/Projectile/CombatProjectileHitResolver.h"
#include "CombatProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class UProjectileDefinition;

/** Launch initialization payload for a combat projectile. */
struct POLYQUEST_API FCombatProjectileLaunchRequest
{
	TObjectPtr<const UProjectileDefinition> Definition = nullptr;
	TWeakObjectPtr<AActor> SourceActor = nullptr;
	TWeakObjectPtr<UAbilitySystemComponent> SourceAbilitySystemComponent = nullptr;
	float AbilityLevel = 1.0f;
	FGameplayTag SetByCallerMagnitudeTag;
	float SetByCallerMagnitude = 0.0f;
	TMap<FGameplayTag, float> SetByCallerMagnitudes;
	float GuardStaminaDamage = 0.0f;
};

/**
 * Concrete travelling combat projectile Actor.
 * Driven by UProjectileMovementComponent with zero gravity, no bounce, and no homing in v1.
 * Owns collision, flight lifecycle, single-hit delivery, and fail-closed cleanup.
 */
UCLASS()
class POLYQUEST_API ACombatProjectile : public AActor
{
	GENERATED_BODY()

public:
	ACombatProjectile();

	/** Initializes projectile movement, lifespan, collision, and damage parameters from the launch request. */
	bool InitializeProjectile(const FCombatProjectileLaunchRequest& LaunchRequest);

	USphereComponent* GetCollisionComponent() const { return CollisionComponent; }
	UProjectileMovementComponent* GetMovementComponent() const { return MovementComponent; }
	UStaticMeshComponent* GetMeshComponent() const { return ProjectileMeshComponent; }

#if WITH_DEV_AUTOMATION_TESTS
	TSubclassOf<UGameplayEffect> GetTestCachedDamageGameplayEffectClass() const { return CachedDamageGameplayEffectClass; }
#endif

protected:
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnProjectileOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnProjectileHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		FVector NormalImpulse,
		const FHitResult& Hit);

private:
	void HandleBlockingImpact(const FHitResult& HitResult);
	void HandlePawnImpact(AActor* HitActor, const FHitResult& HitResult);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> ProjectileMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UProjectileMovementComponent> MovementComponent;

	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> CachedDamageGameplayEffectClass;

	FCombatProjectileLaunchRequest CachedLaunchRequest;
	bool bHitDelivered = false;
	bool bInitialized = false;
};
