#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayTagContainer.h"
#include "EnemyLaunchReactionAbility.generated.h"

class ACharacter;
class AEnemyCharacter;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitDelay;
class UAbilityTask_WaitGameplayEvent;
class UAnimInstance;
class UAnimMontage;

/**
 * Server-authoritative, multi-phase enemy launch hit reaction.
 * Lifecycle: Takeoff -> AwaitingAirborne -> Airborne -> LandingRecovery.
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
	bool GetTestLandingRecoveryCompletedNaturally() const { return bLandingRecoveryCompletedNaturally; }
	void SetTestLandingRecoveryCompletedNaturally(bool bValue) { bLandingRecoveryCompletedNaturally = bValue; }
#endif

private:
	enum class ELaunchPhase : uint8
	{
		None,
		Takeoff,
		AwaitingAirborne,
		Airborne,
		LandingRecovery
	};

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Enemy|Reaction", meta = (AllowPrivateAccess = "true", ToolTip = "敌人击飞受击起飞与滞空姿态动画 Montage 资产。"))
	TObjectPtr<UAnimMontage> TakeoffMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Enemy|Reaction", meta = (AllowPrivateAccess = "true", ToolTip = "敌人击飞受击落地恢复动画 Montage 资产。"))
	TObjectPtr<UAnimMontage> LandingRecoveryMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Enemy|Reaction", meta = (AllowPrivateAccess = "true", ToolTip = "击飞受击时沿受击水平方向施加的初速度（厘米/秒）。"))
	float LaunchHorizontalSpeed = 450.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Enemy|Reaction", meta = (AllowPrivateAccess = "true", ToolTip = "击飞受击时施加的向上垂直初速度（厘米/秒）。"))
	float LaunchVerticalSpeed = 550.0f;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> CommitEventTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

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
	FGameplayTagContainer AbilitiesToCancel;

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

	UFUNCTION()
	void OnFallValidationFinished();

	UFUNCTION()
	void OnActiveMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	void OnMovementModeChanged(ACharacter* Character, EMovementMode PrevMovementMode, uint8 PreviousCustomMode);

	bool ValidateActivationSetup(const FGameplayAbilityActorInfo* ActorInfo) const;
	bool IsEventFromTakeoffMontage(const FGameplayEventData& Payload) const;
	void EndFromMontage(bool bWasCancelled);
};
