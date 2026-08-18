#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GameplayTagContainer.h"
#include "Perception/AIPerceptionTypes.h"
#include "EnemyAIController.generated.h"

class APlayerCharacter;
class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UEnemyAIProfile;
class UStateTreeAIComponent;
struct FPathFollowingResult;

/**
 * Owns the first enemy's Sight target, focus, home location, and StateTree
 * lifecycle. StateTree tasks query this controller rather than storing a
 * competing AI state or target variable.
 */
UCLASS(Blueprintable)
class POLYQUEST_API AEnemyAIController : public AAIController
{
	GENERATED_BODY()

public:
	AEnemyAIController();

	/** Pure helper for target-relative reposition point calculation. */
	static bool CalculateTargetRelativeRepositionPoint(
		const FVector& TargetLocation,
		const FVector& EnemyLocation,
		float PreferredCombatDistance,
		float LateralRepositionDistance,
		float EngagementRange,
		bool bUseRightSide,
		FVector& OutRepositionPoint);

	UFUNCTION(BlueprintPure, Category = "AI|Target")
	APlayerCharacter* GetCurrentTarget() const;

	UFUNCTION(BlueprintPure, Category = "AI|Home")
	FVector GetHomeLocation() const { return HomeLocation; }

	UFUNCTION(BlueprintPure, Category = "AI|Combat")
	float GetMeleeRange() const { return MeleeRange; }

	/** True only after OnPossess has accepted and cached the possessed enemy's authored Attack Set. */
	bool HasValidAttackSet() const;

	/** True only after OnPossess has accepted and cached the possessed enemy's authored AI Profile. */
	UFUNCTION(BlueprintPure, Category = "AI|Profile")
	bool HasValidAIProfile() const;

	UFUNCTION(BlueprintPure, Category = "AI|Profile")
	const UEnemyAIProfile* GetAIProfile() const;

	/** True if the controlled enemy has exceeded LeashRadius from HomeLocation. */
	UFUNCTION(BlueprintPure, Category = "AI|Leash")
	bool IsExceedingLeash() const;

	/** Returns true and calculates the planar 2D distance to the current combat target if valid. */
	UFUNCTION(BlueprintPure, Category = "AI|Combat")
	bool TryGetCurrentTargetDistance2D(float& OutDistance2D) const;

	/** The active Ability starts this only after its Montage was confirmed to have started and then cleaned up. */
	void StartMeleeAttackCooldown(float CooldownAfterAttack);

	UFUNCTION(BlueprintPure, Category = "AI|Combat")
	bool IsMeleeAttackOnCooldown() const;

	/** Checks if repositioning during cooldown is allowed (has valid target, profile, cooldown active, attempts remaining, not dead/stunned/reacting/leash-broken). */
	UFUNCTION(BlueprintPure, Category = "AI|Combat|Reposition")
	bool CanRequestCooldownReposition() const;

	/** Attempts to calculate a reposition point and issue a MoveTo request. */
	UFUNCTION(BlueprintCallable, Category = "AI|Combat|Reposition")
	bool TryRequestCooldownReposition();

	/** Stops any active reposition movement and optionally resets attempt state. */
	UFUNCTION(BlueprintCallable, Category = "AI|Combat|Reposition")
	void StopCooldownReposition(bool bResetAttempts = false);

	UFUNCTION(BlueprintPure, Category = "AI|Combat|Reposition")
	bool IsRepositioning() const { return bIsRepositioning; }

	UFUNCTION(BlueprintPure, Category = "AI|Combat|Reposition")
	int32 GetRepositionAttemptsInCurrentCooldown() const { return RepositionAttemptsInCurrentCooldown; }

	UFUNCTION(BlueprintPure, Category = "AI|Combat|Reposition")
	bool GetLastRepositionUsedRightSide() const { return bLastRepositionUsedRightSide; }

	UFUNCTION(BlueprintPure, Category = "AI|Combat|Reposition")
	float GetNextAllowedRepositionTime() const { return NextAllowedRepositionTime; }

	UFUNCTION(BlueprintPure, Category = "AI|Combat|Reposition")
	int32 GetFailedAttemptsOnCurrentSide() const { return FailedAttemptsOnCurrentSide; }

	/** True if all combat/profile conditions are valid for repositioning, but delayed solely by NextAllowedRepositionTime. */
	UFUNCTION(BlueprintPure, Category = "AI|Combat|Reposition")
	bool IsRepositionTemporarilyIntervalGated() const;

	UFUNCTION(BlueprintPure, Category = "AI|Home")
	float GetHomeAcceptanceRadius() const { return HomeAcceptanceRadius; }

	/** True only while this controller owns a valid Player target and pawn. */
	UFUNCTION(BlueprintPure, Category = "AI|Target")
	bool HasValidCombatTarget() const;

	/** Uses the same range that the authored Chase MoveTo task should bind as its acceptance radius. */
	UFUNCTION(BlueprintPure, Category = "AI|Combat")
	bool IsCombatTargetInMeleeRange() const;

	/** Instant StateTree Alert action: stop stale movement while retaining the current target focus. */
	void BeginAlert();

	/** Requests the single configured enemy melee ability only when the controller target remains in range. */
	bool TryRequestMeleeAttack();

	/** Reads the authoritative GAS action tag; it never infers attack state from montage playback. */
	bool IsEnemyMeleeAttackActive() const;

	/** Reaction state is ASC-owned. StateTree waits rather than issuing another attack request. */
	bool IsEnemyHitReactionActive() const;

	/** Stance-break state is ASC-owned. StateTree waits rather than issuing another attack request. */
	bool IsEnemyStunned() const;

	/** Stops StateTree, movement, target, and focus once the currently possessed enemy owns State.Status.Dead. */
	void HandleControlledEnemyDeath();

	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

private:
	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	void ConfigureSight();
	bool IsControlledEnemyDead() const;
	void SetCurrentTarget(APlayerCharacter* NewTarget);
	void ClearCurrentTarget(bool bSendTargetLostEvent);
	void SendStateTreeEvent(const FGameplayTag& EventTag) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAIPerceptionComponent> EnemyPerceptionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStateTreeAIComponent> StateTreeComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Sight", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "Centimeters"))
	float SightRadius = 1500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Sight", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "Centimeters"))
	float LoseSightRadius = 1800.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Sight", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "180.0", Units = "Degrees"))
	float PeripheralVisionHalfAngleDegrees = 70.0f;

	/** AttackSet EngagementRange cached at runtime. StateTree binds through GetMeleeRange rather than treating this as CDO tuning. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "AI|Combat", meta = (AllowPrivateAccess = "true", Units = "Centimeters"))
	float MeleeRange = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Home", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "Centimeters"))
	float HomeAcceptanceRadius = 50.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "AI|Target", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<APlayerCharacter> CurrentTarget;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "AI|Home", meta = (AllowPrivateAccess = "true"))
	FVector HomeLocation = FVector::ZeroVector;

	FGameplayTag EnemyMeleeAbilityTag;
	FGameplayTag AttackingStateTag;
	FGameplayTag HitReactingStateTag;
	FGameplayTag StunnedStateTag;
	FGameplayTag TargetAcquiredEventTag;
	FGameplayTag TargetLostEventTag;
	float MeleeAttackCooldownEndTime = 0.0f;
	float CachedLeashRadius = 0.0f;
	bool bHasValidAttackSet = false;
	bool bHasValidAIProfile = false;
	bool bHasLoggedInvalidAttackSet = false;
	bool bHasLoggedInvalidAIProfile = false;
	bool bHasLoggedInvalidPoiseRecoverySetup = false;

	float NextAllowedRepositionTime = 0.0f;
	int32 RepositionAttemptsInCurrentCooldown = 0;
	int32 FailedAttemptsOnCurrentSide = 0;
	bool bLastRepositionUsedRightSide = false;
	bool bLastRepositionSucceeded = false;
	bool bIsRepositioning = false;
	FAIRequestID CurrentRepositionRequestID = FAIRequestID::InvalidRequest;
};
