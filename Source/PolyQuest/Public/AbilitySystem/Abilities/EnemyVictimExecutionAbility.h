#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "EnemyVictimExecutionAbility.generated.h"

class AEnemyCharacter;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAnimMontage;
class UExecutionLockContext;

/**
 * Server-authoritative victim execution ability for enemies.
 * Triggered synchronously via GameplayEvent requests from Player execution abilities.
 * Holds victim lock, invulnerability, and freezes movement and AI logic until released.
 */
UCLASS()
class POLYQUEST_API UEnemyVictimExecutionAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UEnemyVictimExecutionAbility();

	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	/** Begins authorized hit scope tracking. */
	bool BeginAuthorizedHitScope();

	/** Ends authorized hit scope tracking. */
	void EndAuthorizedHitScope();

	/** Returns true if currently within an authorized hit scope. */
	bool IsInAuthorizedHitScope() const { return bInAuthorizedHitScope; }

	/** Called synchronously by AEnemyCharacter when lethal health drop occurs during hit scope. */
	void NotifyLethalDamageReceived();

	/** True if victim has received lethal execution damage and is awaiting Release to finalize death. */
	bool IsDeathPending() const { return bDeathPending; }

#if WITH_DEV_AUTOMATION_TESTS
	const FGameplayTagContainer& GetTestActivationOwnedTags() const { return ActivationOwnedTags; }
	const FGameplayTagContainer& GetTestActivationBlockedTags() const { return ActivationBlockedTags; }
	const FGameplayTagContainer& GetTestAbilityTags() const { return AbilityTags; }
	const FGameplayTagContainer& GetTestAbilitiesToCancel() const { return AbilitiesToCancel; }
	const TArray<FAbilityTriggerData>& GetTestAbilityTriggers() const { return AbilityTriggers; }
	UExecutionLockContext* GetTestExecutionContext() const { return ActiveExecutionContext.Get(); }
	bool IsTestAIExecutionLocked() const { return bLockedAI; }
	bool HasTestHandoffFromStanceBreak() const { return bHandoffFromStanceBreak; }
	bool IsTestDeathPending() const { return bDeathPending; }
	bool IsTestInAuthorizedHitScope() const { return bInAuthorizedHitScope; }
	void TestNotifyLethalDamage() { NotifyLethalDamageReceived(); }
	void TestEndAbility(bool bWasCancelled = false) { EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled); }
	void TestTriggerReleaseEvent(const FGameplayEventData& Payload) { OnReleaseReceived(Payload); }
	void SetTestInvalidateWaitReleaseTaskAfterReady(bool bInvalidate) { bTestInvalidateWaitReleaseTaskAfterReady = bInvalidate; }
	void SetTestVictimMontages(UAnimMontage* InFront, UAnimMontage* InBackstab)
	{
		FrontExecutionVictimMontage = InFront;
		BackstabExecutionVictimMontage = InBackstab;
	}
	void SetTestLaunchNonLethalOnRelease(bool bLaunch) { bLaunchNonLethalOnRelease = bLaunch; }
	bool GetTestLaunchNonLethalOnRelease() const { return bLaunchNonLethalOnRelease; }
	UAnimMontage* GetTestActiveVictimMontage() const { return ActiveVictimMontage.Get(); }
	UAnimMontage* GetTestPendingVictimMontage() const { return PendingVictimMontage.Get(); }
	UAbilityTask_PlayMontageAndWait* GetTestVictimMontageTask() const { return VictimMontageTask.Get(); }
	bool IsTestVictimPresentationStarted() const { return bVictimPresentationStarted; }
	void TestTriggerVictimStartEvent(const FGameplayEventData& Payload) { OnVictimStartReceived(Payload); }
	void SetTestInvalidateWaitVictimStartTaskAfterReady(bool bInvalidate) { bTestInvalidateWaitVictimStartTaskAfterReady = bInvalidate; }
	void TestTriggerVictimMontageCompleted() { OnVictimMontageCompleted(); }
	void TestTriggerVictimMontageBlendOut() { OnVictimMontageBlendOut(); }
private:
	bool bTestInvalidateWaitReleaseTaskAfterReady = false;
	bool bTestInvalidateWaitVictimStartTaskAfterReady = false;
public:
#endif

protected:
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Execution", meta = (ToolTip = "正面处决时受害者播放的可选配合动画 Montage。"))
	TObjectPtr<UAnimMontage> FrontExecutionVictimMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Execution", meta = (ToolTip = "背刺处决时受害者播放的可选配合动画 Montage。"))
	TObjectPtr<UAnimMontage> BackstabExecutionVictimMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Execution", meta = (ToolTip = "非致死处决释放时是否派发击飞受击反应。默认开启。"))
	bool bLaunchNonLethalOnRelease = true;

private:
	UFUNCTION()
	void OnReleaseReceived(FGameplayEventData Payload);

	UFUNCTION()
	void OnVictimStartReceived(FGameplayEventData Payload);

	UFUNCTION()
	void OnVictimMontageCompleted();

	UFUNCTION()
	void OnVictimMontageBlendOut();

	UFUNCTION()
	void OnVictimMontageInterrupted();

	UFUNCTION()
	void OnVictimMontageCancelled();

	void StopVictimMontagePresentation(bool bIsNaturalCompletion);

	bool ValidateExecutionRequest(const FGameplayEventData* TriggerEventData, AEnemyCharacter* EnemyCharacter) const;

	UPROPERTY(Transient)
	TObjectPtr<UExecutionLockContext> ActiveExecutionContext;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitReleaseTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitVictimStartTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> VictimMontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> PendingVictimMontage;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveVictimMontage;

	FGameplayTag VictimAbilityTag;
	FGameplayTag FrontRequestEventTag;
	FGameplayTag BackstabRequestEventTag;
	FGameplayTag ReleaseEventTag;
	FGameplayTag VictimStartEventTag;
	FGameplayTag VictimLockedStateTag;
	FGameplayTag DeadStateTag;
	FGameplayTag StanceBreakAbilityTag;
	FGameplayTag StunnedStateTag;
	FGameplayTag InvulnerableStateTag;
	FGameplayTag DeathPendingStateTag;
	FGameplayTag BlockMovementTag;
	FGameplayTag BlockJumpTag;
	FGameplayTag TeardownOnUnpossessTag;

	FGameplayTagContainer AbilitiesToCancel;

	bool bMovementLockedByVictim = false;
	bool bHandoffFromStanceBreak = false;
	bool bLockedAI = false;
	bool bInAuthorizedHitScope = false;
	bool bDeathPending = false;
	bool bAddedDeathPendingTag = false;
	bool bEndAbilityInProgress = false;
	bool bVictimPresentationStarted = false;
};
