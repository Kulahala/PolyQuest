#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayTagContainer.h"
#include "EnemyStanceBreakAbility.generated.h"

class UAbilityTask_PlayActionMontage;
class UAnimInstance;
class UAnimMontage;
class UEnemyStanceBreakAbility;

/** Per-activation business completion callback; window ownership stays in the Task. */
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
	const FGameplayTag& GetTeardownOnUnpossessTag() const { return TeardownOnUnpossessTag; }
	UAbilityTask_PlayActionMontage* GetMontageTask() const { return MontageTask.Get(); }
	uint32 GetTestActivationToken() const { return CurrentActivationToken; }
	UEnemyStanceBreakExecutionContext* GetTestActiveContext() const { return ActiveContext.Get(); }
	int32 GetTestActiveMontageInstanceID() const { return ActiveMontageInstanceID; }
	void SetTestStanceBreakMontage(UAnimMontage* InMontage) { StanceBreakMontage = InMontage; }
	UAnimMontage* GetTestStanceBreakMontage() const { return StanceBreakMontage.Get(); }
	bool IsMovementLockedByStanceBreak() const { return bMovementLockedByStanceBreak; }
	void SetTestBypassMontageActiveCheck(bool bBypass)
	{
		bTestBypassMontageActiveCheck = bBypass;
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
	TObjectPtr<UAbilityTask_PlayActionMontage> MontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UEnemyStanceBreakExecutionContext> ActiveContext;

	UPROPERTY(Transient)
	TObjectPtr<UAnimInstance> BoundAnimInstance;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveMontage;

	UPROPERTY(Transient)
	FGameplayTag StanceBreakAbilityTag;

	UPROPERTY(Transient)
	FGameplayTag StanceBreakEventTag;

	UPROPERTY(Transient)
	FGameplayTag StunnedStateTag;

	UPROPERTY(Transient)
	FGameplayTag VictimLockedStateTag;

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
	FGameplayTag TeardownOnUnpossessTag;

	UPROPERTY(Transient)
	FGameplayTag FacingBlockedStateTag;

	UPROPERTY(Transient)
	FGameplayTagContainer AbilitiesToCancel;

	bool bMovementLockedByStanceBreak = false;
	bool bEndAbilityRequested = false;
	int32 ActiveMontageInstanceID = INDEX_NONE;
	uint32 CurrentActivationToken = 0;

	bool ValidateActivationSetup(const FGameplayAbilityActorInfo* ActorInfo) const;
	void EndFromMontage(bool bWasCancelled);
	void InvalidateCallbackContext();
	void HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted, uint32 InToken);
	friend class UEnemyStanceBreakExecutionContext;
};
