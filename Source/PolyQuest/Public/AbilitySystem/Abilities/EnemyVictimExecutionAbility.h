#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "EnemyVictimExecutionAbility.generated.h"

class AEnemyCharacter;
class UAbilityTask_WaitGameplayEvent;
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

#if WITH_DEV_AUTOMATION_TESTS
	const FGameplayTagContainer& GetTestActivationOwnedTags() const { return ActivationOwnedTags; }
	const FGameplayTagContainer& GetTestActivationBlockedTags() const { return ActivationBlockedTags; }
	const FGameplayTagContainer& GetTestAbilityTags() const { return AbilityTags; }
	const FGameplayTagContainer& GetTestAbilitiesToCancel() const { return AbilitiesToCancel; }
	UExecutionLockContext* GetTestExecutionContext() const { return ActiveExecutionContext.Get(); }
	bool IsTestAIExecutionLocked() const { return bLockedAI; }
	bool HasTestHandoffFromStanceBreak() const { return bHandoffFromStanceBreak; }
	void TestEndAbility(bool bWasCancelled = false) { EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled); }
	void TestTriggerReleaseEvent(const FGameplayEventData& Payload) { OnReleaseReceived(Payload); }
	void SetTestInvalidateWaitReleaseTaskAfterReady(bool bInvalidate) { bTestInvalidateWaitReleaseTaskAfterReady = bInvalidate; }
private:
	bool bTestInvalidateWaitReleaseTaskAfterReady = false;
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

private:
	UFUNCTION()
	void OnReleaseReceived(FGameplayEventData Payload);

	bool ValidateExecutionRequest(const FGameplayEventData* TriggerEventData, AEnemyCharacter* EnemyCharacter) const;

	UPROPERTY(Transient)
	TObjectPtr<UExecutionLockContext> ActiveExecutionContext;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitReleaseTask;

	FGameplayTag VictimAbilityTag;
	FGameplayTag FrontRequestEventTag;
	FGameplayTag BackstabRequestEventTag;
	FGameplayTag ReleaseEventTag;
	FGameplayTag VictimLockedStateTag;
	FGameplayTag DeadStateTag;
	FGameplayTag StanceBreakAbilityTag;
	FGameplayTag StunnedStateTag;
	FGameplayTag InvulnerableStateTag;
	FGameplayTag BlockMovementTag;
	FGameplayTag BlockJumpTag;
	FGameplayTag TeardownOnUnpossessTag;

	FGameplayTagContainer AbilitiesToCancel;

	bool bMovementLockedByVictim = false;
	bool bHandoffFromStanceBreak = false;
	bool bLockedAI = false;
	bool bEndAbilityInProgress = false;
};
