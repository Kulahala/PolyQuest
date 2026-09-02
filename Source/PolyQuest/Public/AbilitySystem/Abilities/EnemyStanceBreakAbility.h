#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystem/Abilities/MontageRateWindowLifecycle.h"
#include "GameplayTagContainer.h"
#include "EnemyStanceBreakAbility.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAnimInstance;
class UAnimMontage;
class UEnemyStanceBreakAbility;

/**
 * Per-activation callback context. A stale task or montage delegate must not
 * be able to terminate a later activation of the same InstancedPerActor ability.
 */
UCLASS(Transient)
class POLYQUEST_API UEnemyStanceBreakExecutionContext : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(Transient)
	TWeakObjectPtr<UEnemyStanceBreakAbility> OwningAbility;

	uint32 Token = 0;

	UFUNCTION()
	void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	void OnRateWindowBegin(FGameplayEventData Payload);

	UFUNCTION()
	void OnRateWindowEnd(FGameplayEventData Payload);
};

/**
 * Server-authoritative enemy stance break. Poise delivery and event routing
 * stay outside the ability; this ability owns only the authored presentation,
 * playback-rate windows, interruption, and recovery teardown.
 */
UCLASS()
class POLYQUEST_API UEnemyStanceBreakAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UEnemyStanceBreakAbility();

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
	const FGameplayTagContainer& GetTestAbilityTags() const { return AbilityTags; }
	const FGameplayTagContainer& GetAbilitiesToCancel() const { return AbilitiesToCancel; }
	const FGameplayTag& GetRateWindowBeginEventTag() const { return RateWindowBeginEventTag; }
	const FGameplayTag& GetRateWindowEndEventTag() const { return RateWindowEndEventTag; }
	const FGameplayTag& GetTeardownOnUnpossessTag() const { return TeardownOnUnpossessTag; }
	const FAbilityMontageRateWindowLifecycle& GetRateWindowLifecycle() const { return RateWindowLifecycle; }
	FAbilityMontageRateWindowLifecycle& GetRateWindowLifecycle_Mutable() { return RateWindowLifecycle; }
	uint32 GetTestActivationToken() const { return CurrentActivationToken; }
	UEnemyStanceBreakExecutionContext* GetTestActiveContext() const { return ActiveContext.Get(); }
	UAbilityTask_WaitGameplayEvent* GetRateWindowBeginTask() const { return RateWindowBeginTask.Get(); }
	UAbilityTask_WaitGameplayEvent* GetRateWindowEndTask() const { return RateWindowEndTask.Get(); }
	UAbilityTask_PlayMontageAndWait* GetMontageTask() const { return MontageTask.Get(); }
	void SetTestStanceBreakMontage(UAnimMontage* InMontage) { StanceBreakMontage = InMontage; }
	UAnimMontage* GetTestStanceBreakMontage() const { return StanceBreakMontage.Get(); }
	bool IsMovementLockedByStanceBreak() const { return bMovementLockedByStanceBreak; }
	void SetTestBypassMontageActiveCheck(bool bBypass)
	{
		bTestBypassMontageActiveCheck = bBypass;
		RateWindowLifecycle.SetTestBypassMontageActiveCheck(bBypass);
	}
	bool GetTestBypassMontageActiveCheck() const { return bTestBypassMontageActiveCheck; }
	void SetTestBoundAnimInstance(UAnimInstance* InAnimInstance) { BoundAnimInstance = InAnimInstance; }
	UAnimInstance* GetTestBoundAnimInstance() const { return BoundAnimInstance.Get(); }
#endif

private:
#if WITH_DEV_AUTOMATION_TESTS
	bool bTestBypassMontageActiveCheck = false;
#endif
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Enemy|Stance Break", meta = (AllowPrivateAccess = "true", ToolTip = "敌人韧性归零发生架势崩解（Stance Break）时播放的虚弱硬直动画 Montage 资产。"))
	TObjectPtr<UAnimMontage> StanceBreakMontage;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> RateWindowBeginTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> RateWindowEndTask;

	UPROPERTY(Transient)
	TObjectPtr<UAnimInstance> BoundAnimInstance;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveMontage;

	UPROPERTY(Transient)
	TObjectPtr<UEnemyStanceBreakExecutionContext> ActiveContext;

	FAbilityMontageRateWindowLifecycle RateWindowLifecycle;

	UPROPERTY(Transient)
	FGameplayTag StanceBreakAbilityTag;

	UPROPERTY(Transient)
	FGameplayTag StanceBreakEventTag;

	UPROPERTY(Transient)
	FGameplayTag StunnedStateTag;

	UPROPERTY(Transient)
	FGameplayTag HitReactingStateTag;

	UPROPERTY(Transient)
	FGameplayTag EnemyMeleeAbilityTag;

	UPROPERTY(Transient)
	FGameplayTag EnemyHitReactionAbilityTag;

	UPROPERTY(Transient)
	FGameplayTag EnemySmallHitReactionAbilityTag;

	UPROPERTY(Transient)
	FGameplayTag EnemyLaunchReactionAbilityTag;

	UPROPERTY(Transient)
	FGameplayTag RateWindowBeginEventTag;

	UPROPERTY(Transient)
	FGameplayTag RateWindowEndEventTag;

	UPROPERTY(Transient)
	FGameplayTag TeardownOnUnpossessTag;

	UPROPERTY(Transient)
	FGameplayTagContainer AbilitiesToCancel;

	bool bMovementLockedByStanceBreak = false;
	bool bEndAbilityRequested = false;
	uint32 CurrentActivationToken = 0;

	bool ValidateActivationSetup(const FGameplayAbilityActorInfo* ActorInfo) const;
	void EndFromMontage(bool bWasCancelled);
	void InvalidateCallbackContext();
	void HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted, uint32 InToken);
	void HandleRateWindowBegin(FGameplayEventData Payload, uint32 InToken);
	void HandleRateWindowEnd(FGameplayEventData Payload, uint32 InToken);

	friend class UEnemyStanceBreakExecutionContext;
};
