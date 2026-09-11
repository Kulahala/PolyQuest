#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "EnemyVictimExecutionAbility.generated.h"

class AEnemyCharacter;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAnimInstance;
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

	/** Returns true only if this ability is active and actively in non-lethal recovery from the specified source actor. */
	bool IsNonLethalRecoveryFrom(const AActor* SourceActor) const;

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
	UAnimMontage* GetTestActiveVictimMontage() const { return ActiveVictimMontage.Get(); }
	UAnimMontage* GetTestPendingVictimMontage() const { return PendingVictimMontage.Get(); }
	UAbilityTask_PlayMontageAndWait* GetTestVictimMontageTask() const { return VictimMontageTask.Get(); }
	bool IsTestVictimPresentationStarted() const { return bVictimPresentationStarted; }
	void TestTriggerVictimStartEvent(const FGameplayEventData& Payload) { OnVictimStartReceived(Payload); }
	void SetTestInvalidateWaitVictimStartTaskAfterReady(bool bInvalidate);
	void TestTriggerVictimMontageCompleted() { OnVictimMontageCompleted(); }
	void TestTriggerVictimMontageBlendOut() { OnVictimMontageBlendOut(); }
	void TestTriggerVictimMontageInterrupted() { OnVictimMontageInterrupted(); }
	bool IsTestNonLethalRecoveryActive() const { return bNonLethalRecoveryActive; }
	bool GetTestHasSavedCanWalkOffLedges() const { return bHasSavedCanWalkOffLedges; }
	bool GetTestSavedCanWalkOffLedges() const { return bSavedCanWalkOffLedges; }
	int32 GetTestActiveVictimMontageInstanceID() const { return ActiveVictimMontageInstanceID; }
	void SetTestActiveVictimMontageInstanceID(int32 InID) { ActiveVictimMontageInstanceID = InID; }
	void SetTestActiveVictimMontage(UAnimMontage* InMontage) { ActiveVictimMontage = InMontage; }
	void TestStopVictimMontagePresentation(bool bIsNaturalCompletion) { StopVictimMontagePresentation(bIsNaturalCompletion); }
	void TestTriggerMovementModeChanged(EMovementMode PrevMode, uint8 PrevCustomMode);
	void SetTestBypassMontageActiveCheck(bool bBypass) { bTestBypassMontageActiveCheck = bBypass; }
	bool GetTestBypassMontageActiveCheck() const { return bTestBypassMontageActiveCheck; }
	void SetTestBoundAnimInstance(UAnimInstance* InAnimInstance) { BoundAnimInstance = InAnimInstance; }
	UAnimInstance* GetTestBoundAnimInstance() const { return BoundAnimInstance.Get(); }
	void SetTestCancelDuringStartupMovementMode(bool bCancel) { bTestCancelDuringStartupMovementMode = bCancel; }
	void SetTestCancelDuringStartupReadyForActivation(bool bCancel) { bTestCancelDuringStartupReadyForActivation = bCancel; }
	void SetTestNonLethalRecovery(bool bActive, const AActor* InSourceActor)
	{
		bNonLethalRecoveryActive = bActive;
		NonLethalRecoverySourceActor = InSourceActor;
	}
	void SetTestDeathPending(bool bInDeathPending) { bDeathPending = bInDeathPending; }
	void SetTestAbilityActive(bool bInActive)
	{
		bIsActive = bInActive;
	}
	void SetTestActorInfo(FGameplayAbilitySpecHandle InHandle, const FGameplayAbilityActorInfo* InActorInfo)
	{
		SetCurrentActorInfo(InHandle, InActorInfo);
	}
	int32 GetTestStartupVictimMontageInstanceID() const { return StartupVictimMontageInstanceID; }
	UAnimMontage* GetTestPendingStartupVictimMontage() const { return PendingStartupVictimMontage.Get(); }
	void SetTestOnMontageStartedHook(TFunction<void(UAnimMontage*)> InHook) { TestOnMontageStartedHook = InHook; }
private:
	TFunction<void(UAnimMontage*)> TestOnMontageStartedHook;
	bool bTestInvalidateWaitReleaseTaskAfterReady = false;
	bool bTestInvalidateWaitVictimStartTaskAfterReady = false;
	bool bTestBypassMontageActiveCheck = false;
	bool bTestCancelDuringStartupMovementMode = false;
	bool bTestCancelDuringStartupReadyForActivation = false;
	TWeakObjectPtr<UAnimInstance> BoundAnimInstance;
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Execution", meta = (ToolTip = "正面处决抽刀后受害者播放的唯一作者化恢复动画 Montage（支持原地或 Root Motion 动画）。"))
	TObjectPtr<UAnimMontage> FrontExecutionVictimMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Execution", meta = (ToolTip = "背刺处决抽刀后受害者播放的唯一作者化恢复动画 Montage（支持原地或 Root Motion 动画）。"))
	TObjectPtr<UAnimMontage> BackstabExecutionVictimMontage;

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

	UFUNCTION()
	void HandleOnMontageStarted(UAnimMontage* Montage);

	UFUNCTION()
	void OnMovementModeChanged(
		ACharacter* Character,
		EMovementMode PrevMovementMode,
		uint8 PreviousCustomMode);

	void StopVictimMontagePresentation(bool bIsNaturalCompletion);

	bool ValidateExecutionRequest(
		const FGameplayEventData* TriggerEventData,
		AEnemyCharacter* EnemyCharacter,
		bool& bOutHandoffFromStanceBreak) const;

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

	UPROPERTY(Transient)
	TWeakObjectPtr<const AActor> NonLethalRecoverySourceActor;

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
	bool bNonLethalRecoveryActive = false;
	bool bSavedCanWalkOffLedges = false;
	bool bHasSavedCanWalkOffLedges = false;
	int32 ActiveVictimMontageInstanceID = INDEX_NONE;
	int32 StartupVictimMontageInstanceID = INDEX_NONE;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> PendingStartupVictimMontage;

	bool bStartupCancellationPending = false;
};
