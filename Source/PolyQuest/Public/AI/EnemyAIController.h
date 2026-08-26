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
class UEnemyAttackProfile;
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

	/** True if controller currently holds a valid pending attack profile selected from AttackSet. */
	UFUNCTION(BlueprintPure, Category = "AI|Combat")
	bool HasPendingAttackProfile() const;

	/** Returns the immutable pending attack profile snapshot, if any. */
	UFUNCTION(BlueprintPure, Category = "AI|Combat")
	const UEnemyAttackProfile* GetPendingAttackProfile() const;

	/** Returns the AttackRange of the pending attack profile, or MeleeRange if none. */
	UFUNCTION(BlueprintPure, Category = "AI|Combat")
	float GetPendingAttackRange() const;

	/** Returns true if target distance is within the pending attack profile's AttackRange. */
	UFUNCTION(BlueprintPure, Category = "AI|Combat")
	bool IsPendingAttackInRange() const;

	/**
	 * Prepares / selects a weighted attack profile for the current target if none is currently pending.
	 * If a profile is already pending, retains it unchanged.
	 * Returns true if a valid profile is ready for execution or approach.
	 */
	UFUNCTION(BlueprintCallable, Category = "AI|Combat")
	bool PreparePendingAttackProfile();

	/** Clears the pending attack profile and stops any active approach movement. */
	UFUNCTION(BlueprintCallable, Category = "AI|Combat")
	void ClearPendingAttackProfile();

	/** Checks if melee approach can be requested towards the current combat target. */
	UFUNCTION(BlueprintPure, Category = "AI|Combat|Approach")
	bool CanRequestApproach() const;

	/** Issues a dynamic MoveTo request tracking TargetActor with acceptance radius equal to PendingAttackProfile.AttackRange. */
	UFUNCTION(BlueprintCallable, Category = "AI|Combat|Approach")
	bool TryRequestApproach();

	/** Stops active approach movement and optionally clears pending profile. */
	UFUNCTION(BlueprintCallable, Category = "AI|Combat|Approach")
	void StopApproach(bool bClearPendingProfile = false);

	UFUNCTION(BlueprintPure, Category = "AI|Combat|Approach")
	bool IsApproaching() const { return bIsApproaching; }

	/** True if approach has exceeded ApproachTimeout. */
	UFUNCTION(BlueprintPure, Category = "AI|Combat|Approach")
	bool HasApproachTimedOut() const;

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

#if WITH_DEV_AUTOMATION_TESTS
	/** Automation helper: sets combat target through production SetCurrentTarget route. */
	void SetTestTargetForAutomation(APlayerCharacter* InTarget) { SetCurrentTarget(InTarget); }

	/** Automation helper: clears combat target through production ClearCurrentTarget route. */
	void ClearTestTargetForAutomation(bool bSendTargetLostEvent = false) { ClearCurrentTarget(bSendTargetLostEvent); }

	/** Automation helper: processes perception for a player through the production perception route. */
	void TriggerTestProcessTargetPerception(APlayerCharacter* InPlayer, bool bSuccessfullySensed) { ProcessTargetPerception(InPlayer, bSuccessfullySensed); }

	/** Automation helper: triggers retained target revalidation check. */
	void TriggerTestRevalidateRetainedTarget() { RevalidateRetainedCombatTarget(); }

	/** Automation inspector: checks whether current target is retained without visual sight. */
	bool IsTargetRetainedWithoutSightForTest() const { return bIsTargetRetainedWithoutSight; }

	/** Automation helper: invokes production UpdateControlRotation with an explicit delta time. */
	void TriggerTestUpdateControlRotation(float DeltaTime, bool bUpdatePawn = true) { UpdateControlRotation(DeltaTime, bUpdatePawn); }

	/** Automation helper: triggers production Reposition pace override transition. */
	bool TriggerTestBeginCooldownRepositionPace() { return BeginRepositionPaceOverride(); }

	/** Automation inspector: checks whether Reposition pace override is active. */
	bool IsRepositionPaceOverriddenForTest() const { return bHasOverriddenRepositionSpeed; }

	/** Automation inspector: returns the captured original MaxWalkSpeed. */
	float GetCapturedRepositionMaxWalkSpeedForTest() const { return OriginalRepositionMaxWalkSpeed; }

	/** Automation inspector: checks whether facing recovery state is active. */
	bool IsFacingRecoveryActiveForTest() const { return bIsFacingRecoveryActive; }
#endif

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void UpdateControlRotation(float DeltaTime, bool bUpdatePawn = true) override;

private:
	static constexpr float TacticalRepositionSpeed = 300.0f;
	static constexpr float PostRootMotionRecoveryTurnRate = 800.0f;
	static constexpr float FacingRecoveryThresholdDegrees = 0.01f;

	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	void ConfigureSight();
	bool IsControlledEnemyDead() const;
	bool CanRetainCurrentTargetWithoutSight() const;
	void ProcessTargetPerception(APlayerCharacter* PlayerCharacter, bool bSuccessfullySensed);
	void RevalidateRetainedCombatTarget();
	void SetCurrentTarget(APlayerCharacter* NewTarget);
	void ClearCurrentTarget(bool bSendTargetLostEvent);
	void SendStateTreeEvent(const FGameplayTag& EventTag) const;

	bool HasControlledEnemyRootMotion() const;
	bool IsEnemyLaunchReactionActive() const;
	void ApplyTargetFocus(APlayerCharacter* TargetToFocus);
	void ClearTargetFocus();
	void ResetRootMotionFacingHandoff();

	bool BeginRepositionPaceOverride();
	void RestoreRepositionPaceOverride();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAIPerceptionComponent> EnemyPerceptionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStateTreeAIComponent> StateTreeComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Sight", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "Centimeters", ToolTip = "AI 视觉感知的最大初始发现半径（厘米）。"))
	float SightRadius = 1500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Sight", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "Centimeters", ToolTip = "已发现目标的视觉感知保持半径（厘米），必须大于等于 SightRadius；失去视线后仍在此半径且未越过 Leash 时，战斗目标会暂时保留。"))
	float LoseSightRadius = 1800.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Sight", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "180.0", Units = "Degrees", ToolTip = "AI 视觉感知圆锥的水平半角（度）。"))
	float PeripheralVisionHalfAngleDegrees = 70.0f;

	/** AttackSet EngagementRange cached at runtime. StateTree binds through GetMeleeRange rather than treating this as CDO tuning. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "AI|Combat", meta = (AllowPrivateAccess = "true", Units = "Centimeters"))
	float MeleeRange = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Home", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "Centimeters", ToolTip = "脱战返回出生点移动请求的到达容差半径（厘米）。"))
	float HomeAcceptanceRadius = 50.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "AI|Target", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<APlayerCharacter> CurrentTarget;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "AI|Home", meta = (AllowPrivateAccess = "true"))
	FVector HomeLocation = FVector::ZeroVector;

	FGameplayTag EnemyMeleeAbilityTag;
	FGameplayTag EnemyLaunchReactionAbilityTag;
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

	UPROPERTY(Transient)
	TWeakObjectPtr<const UEnemyAttackProfile> PendingAttackProfile = nullptr;

	bool bIsApproaching = false;
	FAIRequestID CurrentApproachRequestID = FAIRequestID::InvalidRequest;
	float ApproachStartTime = 0.0f;

	bool bWasRootMotionActive = false;
	bool bIsFacingRecoveryActive = false;
	bool bHasOverriddenRepositionSpeed = false;
	float OriginalRepositionMaxWalkSpeed = 0.0f;
	bool bIsTargetRetainedWithoutSight = false;
};
