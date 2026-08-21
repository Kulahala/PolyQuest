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

	/** Initial flight direction in world space. */
	FVector InitialFlightDirection = FVector::ForwardVector;

	/** Optional weak target actor selected at launch time. */
	TWeakObjectPtr<AActor> TargetActor = nullptr;

	/** Target aim point in world space computed at launch time. */
	FVector InitialTargetAimPoint = FVector::ZeroVector;

	/** Whether limited homing is enabled for this launch. */
	bool bEnableLimitedHoming = false;

	/** Delay in seconds before homing steering activates. */
	float HomingStartDelaySeconds = 0.0f;

	/** Maximum duration for homing tracking in seconds. */
	float HomingDurationSeconds = 0.0f;

	/** Maximum angular rotation speed in degrees per second. */
	float HomingTurnRateDegreesPerSecond = 0.0f;

	/** Maximum total accumulated deflection angle in degrees from initial flight direction. */
	float HomingMaxTotalTurnDegrees = 0.0f;

	/** Maximum horizontal distance in cm from launch location allowed for target validity. */
	float TargetAssistMaxDistance = 0.0f;

	/** Maximum height delta in cm from launch location allowed for target validity. */
	float TargetAssistMaxHeightDelta = 0.0f;
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

	virtual void Tick(float DeltaSeconds) override;

	USphereComponent* GetCollisionComponent() const { return CollisionComponent; }
	UProjectileMovementComponent* GetMovementComponent() const { return MovementComponent; }
	UStaticMeshComponent* GetMeshComponent() const { return ProjectileMeshComponent; }

#if WITH_DEV_AUTOMATION_TESTS
	TSubclassOf<UGameplayEffect> GetTestCachedDamageGameplayEffectClass() const { return CachedDamageGameplayEffectClass; }
	bool GetTestHomingActive() const { return bHomingActive; }
	float GetTestHomingElapsedTime() const { return HomingElapsedTime; }
	float GetTestHomingStartDelaySeconds() const { return CachedHomingStartDelaySeconds; }
	float GetTestTotalTurnAngleDegrees() const { return TotalTurnAngleDegrees; }
	AActor* GetTestTargetActor() const { return CachedTargetActor.Get(); }
	const FVector& GetTestInitialLaunchDirection() const { return InitialLaunchDirection; }
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
	void StopHomingAndFlyStraight();
	void UpdateLimitedHoming(float DeltaSeconds);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> ProjectileMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UProjectileMovementComponent> MovementComponent;

	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> CachedDamageGameplayEffectClass;

	FCombatProjectileLaunchRequest CachedLaunchRequest;
	FVector InitialLaunchLocation = FVector::ZeroVector;
	FVector InitialLaunchDirection = FVector::ForwardVector;
	TWeakObjectPtr<AActor> CachedTargetActor = nullptr;

	float HomingElapsedTime = 0.0f;
	float TotalTurnAngleDegrees = 0.0f;
	float CachedHomingStartDelaySeconds = 0.0f;
	float CachedHomingDurationSeconds = 0.0f;
	float CachedHomingTurnRateDegreesPerSecond = 0.0f;
	float CachedHomingMaxTotalTurnDegrees = 0.0f;
	float CachedTargetAssistMaxDistance = 0.0f;
	float CachedTargetAssistMaxHeightDelta = 0.0f;

	bool bHomingActive = false;
	bool bHitDelivered = false;
	bool bInitialized = false;
};
