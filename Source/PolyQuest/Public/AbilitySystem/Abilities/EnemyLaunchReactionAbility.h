#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystem/Abilities/LaunchFacingSmoothingState.h"
#include "GameplayTagContainer.h"
#include "EnemyLaunchReactionAbility.generated.h"

class ACharacter;
class AEnemyCharacter;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_TurnToFacing;
class UAbilityTask_WaitDelay;
class UAbilityTask_WaitGameplayEvent;
class UAnimInstance;
class UAnimMontage;

/**
 * Server-authoritative, multi-phase enemy launch hit reaction.
 * Lifecycle: Takeoff -> TurningToLaunch -> AwaitingAirborne -> Airborne -> LandingRecovery.
 * Takeoff Montage provides full-body takeoff and airborne pose presentation.
 * On Event.Reaction.Launch.Commit, the Takeoff Montage is paused to hold the airborne flight silhouette,
 * while CharacterMovement exclusively drives all capsule displacement and physics falling.
 * Upon landing on ground, the paused Takeoff Montage is stopped and LandingRecovery Montage plays.
 */
UCLASS()
class POLYQUEST_API UEnemyLaunchReactionAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UEnemyLaunchReactionAbility();

	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

#if WITH_DEV_AUTOMATION_TESTS
	const FGameplayTagContainer& GetTestActivationOwnedTags() const { return ActivationOwnedTags; }
	const FGameplayTagContainer& GetTestActivationBlockedTags() const { return ActivationBlockedTags; }
	const FGameplayTagContainer& GetTestAbilitiesToCancel() const { return AbilitiesToCancel; }
	const TArray<FAbilityTriggerData>& GetTestAbilityTriggers() const { return AbilityTriggers; }
	const FVector& GetImpactDirectionSnapshot() const { return ImpactDirectionSnapshot; }
	float GetLaunchHorizontalSpeed() const { return LaunchHorizontalSpeed; }
	float GetLaunchVerticalSpeed() const { return LaunchVerticalSpeed; }
	float GetFacingTurnRateDegreesPerSecond() const { return FacingTurnRateDegreesPerSecond; }
	bool GetTestLandingRecoveryCompletedNaturally() const { return bLandingRecoveryCompletedNaturally; }
	void SetTestLandingRecoveryCompletedNaturally(bool bValue) { bLandingRecoveryCompletedNaturally = bValue; }

	bool GetUseGroundedRootMotionKnockdown() const { return bUseGroundedRootMotionKnockdown; }
	void SetTestUseGroundedRootMotionKnockdown(bool bValue) { bUseGroundedRootMotionKnockdown = bValue; }
	UAnimMontage* GetTestRootMotionKnockdownMontage() const { return RootMotionKnockdownMontage.Get(); }
	void SetTestRootMotionKnockdownMontage(UAnimMontage* Montage) { RootMotionKnockdownMontage = Montage; }
	void SetTestTakeoffMontage(UAnimMontage* Montage) { TakeoffMontage = Montage; }
	void SetTestLandingRecoveryMontage(UAnimMontage* Montage) { LandingRecoveryMontage = Montage; }
	bool IsTestPhaseRootMotionKnockdown() const;
	bool IsTestPhaseTakeoff() const;
	bool IsTestPhaseNone() const;
	uint8 GetTestCurrentPhaseRaw() const;
	bool GetTestLedgeSettingModified() const { return bLedgeSettingModified; }
	void SetTestLedgeSettingModified(bool bModified) { bLedgeSettingModified = bModified; }
	bool GetTestSavedCanWalkOffLedges() const { return bSavedCanWalkOffLedges; }
	void SetTestSavedCanWalkOffLedges(bool bValue) { bSavedCanWalkOffLedges = bValue; }
	bool GetTestCommitEventTaskActive() const { return CommitEventTask != nullptr; }
	bool GetTestFacingTurnTaskActive() const { return FacingTurnTask != nullptr; }
	bool GetTestFallValidationTaskActive() const { return FallValidationTask != nullptr; }
	void SetTestBypassMontageActiveCheck(bool bBypass) { bTestBypassMontageActiveCheck = bBypass; }
	bool GetTestBypassMontageActiveCheck() const { return bTestBypassMontageActiveCheck; }
	UAnimInstance* GetTestBoundAnimInstance() const { return BoundAnimInstance.Get(); }
	void SetTestBoundAnimInstance(UAnimInstance* AnimInst) { BoundAnimInstance = AnimInst; }
	bool CallTestIsRootMotionKnockdownCandidate(const AEnemyCharacter* InEnemy, const class UCharacterMovementComponent* InMovement) const
	{
		return IsRootMotionKnockdownCandidate(InEnemy, InMovement);
	}
	bool CallTestIsLegacyLaunchCandidate(const class UCharacterMovementComponent* InMovement) const
	{
		return IsLegacyLaunchCandidate(InMovement);
	}
	bool CallTestValidateActivationSetup(const FGameplayAbilityActorInfo* ActorInfo) const { return ValidateActivationSetup(ActorInfo); }
	void TriggerTestMovementModeChanged(ACharacter* Character, EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
	{
		OnMovementModeChanged(Character, PrevMovementMode, PreviousCustomMode);
	}
	void TriggerTestLaunchCommitEvent(const FGameplayEventData& Payload)
	{
		OnLaunchCommitEventReceived(Payload);
	}
	void TriggerTestActiveMontageEnded(UAnimMontage* Montage, bool bInterrupted)
	{
		OnActiveMontageEnded(Montage, bInterrupted);
	}
	static bool CallTestTryResolveRootMotionFacingYaw(const FVector& LocalAttackerDirection, float ImpactReferenceYaw, float& OutFacingYaw)
	{
		return TryResolveRootMotionFacingYaw(LocalAttackerDirection, ImpactReferenceYaw, OutFacingYaw);
	}
#endif

private:
	enum class ELaunchPhase : uint8
	{
		None,
		Takeoff,
		TurningToLaunch,
		AwaitingAirborne,
		Airborne,
		LandingRecovery,
		RootMotionKnockdown
	};

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Enemy|Reaction", meta = (AllowPrivateAccess = "true", ToolTip = "敌人击飞受击地面 Root Motion 击倒动画 Montage 资产。"))
	TObjectPtr<UAnimMontage> RootMotionKnockdownMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Enemy|Reaction", meta = (AllowPrivateAccess = "true", ToolTip = "是否在地面 MOVE_Walking 下优先使用作者化 Root Motion 击倒而非旧物理 Launch。"))
	bool bUseGroundedRootMotionKnockdown = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Enemy|Reaction", meta = (AllowPrivateAccess = "true", ToolTip = "敌人击飞受击起飞与滞空姿态动画 Montage 资产。"))
	TObjectPtr<UAnimMontage> TakeoffMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Enemy|Reaction", meta = (AllowPrivateAccess = "true", ToolTip = "敌人击飞受击落地恢复动画 Montage 资产。"))
	TObjectPtr<UAnimMontage> LandingRecoveryMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Enemy|Reaction", meta = (AllowPrivateAccess = "true", ToolTip = "击飞受击时沿受击水平方向施加的初速度（厘米/秒）。"))
	float LaunchHorizontalSpeed = 450.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Enemy|Reaction", meta = (AllowPrivateAccess = "true", ToolTip = "击飞受击时施加的向上垂直初速度（厘米/秒）。"))
	float LaunchVerticalSpeed = 550.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Enemy|Reaction", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ToolTip = "敌人击飞受击起飞阶段朝向攻击者的平滑转向速率（度/秒）。"))
	float FacingTurnRateDegreesPerSecond = 1440.0f;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> CommitEventTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_TurnToFacing> FacingTurnTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitDelay> FallValidationTask;

	UPROPERTY(Transient)
	TObjectPtr<UAnimInstance> BoundAnimInstance;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveMontage;

	UPROPERTY(Transient)
	TWeakObjectPtr<AEnemyCharacter> BoundEnemyCharacter;

	FGameplayTag EnemyLaunchReactionAbilityTag;
	FGameplayTag EnemyLaunchReactionEventTag;
	FGameplayTag LaunchCommitEventTag;
	FGameplayTag HitReactingStateTag;
	FGameplayTag StunnedStateTag;
	FGameplayTag DeadStateTag;
	FGameplayTag HyperArmorStateTag;
	FGameplayTag EnemyMeleeAbilityTag;
	FGameplayTag EnemySmallHitReactionAbilityTag;
	FGameplayTag TeardownOnUnpossessTag;
	FGameplayTag FacingBlockedStateTag;
	FGameplayTagContainer AbilitiesToCancel;

	FLaunchFacingSmoothingState SmoothingState;
	FVector ImpactDirectionSnapshot = FVector::ZeroVector;
	float ImpactReferenceYawSnapshot = 0.0f;
	ELaunchPhase CurrentPhase = ELaunchPhase::None;
	bool bCommitHandled = false;
	bool bSavedCanWalkOffLedges = true;
	bool bLedgeSettingModified = false;
	bool bMovementModeDelegateBound = false;
	bool bEndAbilityRequested = false;
	bool bLandingRecoveryCompletedNaturally = false;

	UFUNCTION()
	void OnLaunchCommitEventReceived(FGameplayEventData Payload);

	void OnFacingTurnCompleted();
	void OnFacingTurnFailed();
	void CommitFrozenLaunch();

	UFUNCTION()
	void OnFallValidationFinished();

	UFUNCTION()
	void OnActiveMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	void OnMovementModeChanged(ACharacter* Character, EMovementMode PrevMovementMode, uint8 PreviousCustomMode);

	bool ValidateActivationSetup(const FGameplayAbilityActorInfo* ActorInfo) const;
	bool IsRootMotionKnockdownCandidate(const AEnemyCharacter* EnemyCharacter, const class UCharacterMovementComponent* MovementComponent) const;
	bool IsLegacyLaunchCandidate(const class UCharacterMovementComponent* MovementComponent) const;
	static bool TryResolveRootMotionFacingYaw(const FVector& LocalAttackerDirection, float ImpactReferenceYaw, float& OutFacingYaw);
	bool IsEventFromTakeoffMontage(const FGameplayEventData& Payload) const;
	void EndFromMontage(bool bWasCancelled);

#if WITH_DEV_AUTOMATION_TESTS
	bool bTestBypassMontageActiveCheck = false;
#endif
};
