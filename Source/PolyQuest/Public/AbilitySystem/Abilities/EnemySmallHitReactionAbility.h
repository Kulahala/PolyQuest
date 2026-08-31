#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayTagContainer.h"
#include "EnemySmallHitReactionAbility.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAnimInstance;
class UAnimMontage;

/**
 * Server-authoritative, non-interrupting enemy small hit reaction.
 * Plays an additive/overlay montage without blocking movement, AI state, or active attacks.
 */
UCLASS()
class POLYQUEST_API UEnemySmallHitReactionAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UEnemySmallHitReactionAbility();

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
	const TArray<FAbilityTriggerData>& GetTestAbilityTriggers() const { return AbilityTriggers; }
	bool GetTestRetriggerInstancedAbility() const { return bRetriggerInstancedAbility; }
	UAnimMontage* GetTestActiveMontage() const { return ActiveMontage.Get(); }
	UAbilityTask_PlayMontageAndWait* GetTestMontageTask() const { return MontageTask.Get(); }
	UAnimInstance* GetTestBoundAnimInstance() const { return BoundAnimInstance.Get(); }
	bool GetTestEndAbilityRequested() const { return bEndAbilityRequested; }
	void SetTestActiveMontage(UAnimMontage* InMontage) { ActiveMontage = InMontage; }
	void SetTestBoundAnimInstance(UAnimInstance* InAnimInstance) { BoundAnimInstance = InAnimInstance; }
	void SetTestMontageTask(UAbilityTask_PlayMontageAndWait* InTask) { MontageTask = InTask; }
	void SetTestCurrentActorInfo(const FGameplayAbilityActorInfo* InActorInfo) { CurrentActorInfo = InActorInfo; }
	void SetTestCurrentSpecHandle(const FGameplayAbilitySpecHandle InHandle) { CurrentSpecHandle = InHandle; }
	void SetTestFrontSmallHitReactionMontage(UAnimMontage* InMontage) { FrontSmallHitReactionMontage = InMontage; }
	void SetTestBackSmallHitReactionMontage(UAnimMontage* InMontage) { BackSmallHitReactionMontage = InMontage; }
	void SetTestLeftSmallHitReactionMontage(UAnimMontage* InMontage) { LeftSmallHitReactionMontage = InMontage; }
	void SetTestRightSmallHitReactionMontage(UAnimMontage* InMontage) { RightSmallHitReactionMontage = InMontage; }
	void TestBindTaskCallbacks(UAbilityTask_PlayMontageAndWait* InTask);
	void TestUnbindTaskCallbacks(UAbilityTask_PlayMontageAndWait* InTask);
	void TestOnMontageCompleted() { OnMontageCompleted(); }
	void TestOnMontageInterrupted() { OnMontageInterrupted(); }
	void TestOnMontageCancelled() { OnMontageCancelled(); }
#endif

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Enemy|Reaction", meta = (AllowPrivateAccess = "true", ToolTip = "攻击者位于受击者本地前方时播放的叠加层受击动画 Montage 资产。此名称描述攻击者来源方向，非受击者位移方向。四个方向属性（Front、Back、Left、Right）必须完整同时配置。"))
	TObjectPtr<UAnimMontage> FrontSmallHitReactionMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Enemy|Reaction", meta = (AllowPrivateAccess = "true", ToolTip = "攻击者位于受击者本地后方时播放的叠加层受击动画 Montage 资产。此名称描述攻击者来源方向，非受击者位移方向。四个方向属性（Front、Back、Left、Right）必须完整同时配置。"))
	TObjectPtr<UAnimMontage> BackSmallHitReactionMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Enemy|Reaction", meta = (AllowPrivateAccess = "true", ToolTip = "攻击者位于受击者本地左方时播放的叠加层受击动画 Montage 资产。此名称描述攻击者来源方向，非受击者位移方向。四个方向属性（Front、Back、Left、Right）必须完整同时配置。"))
	TObjectPtr<UAnimMontage> LeftSmallHitReactionMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Enemy|Reaction", meta = (AllowPrivateAccess = "true", ToolTip = "攻击者位于受击者本地右方时播放的叠加层受击动画 Montage 资产。此名称描述攻击者来源方向，非受击者位移方向。四个方向属性（Front、Back、Left、Right）必须完整同时配置。"))
	TObjectPtr<UAnimMontage> RightSmallHitReactionMontage;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAnimInstance> BoundAnimInstance;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveMontage;

	FGameplayTag SmallHitReactionAbilityTag;
	FGameplayTag SmallHitReactionEventTag;
	FGameplayTag SmallHitReactingStateTag;
	FGameplayTag StunnedStateTag;
	FGameplayTag DeadStateTag;
	bool bEndAbilityRequested = false;

	UFUNCTION()
	void OnMontageCompleted();

	UFUNCTION()
	void OnMontageInterrupted();

	UFUNCTION()
	void OnMontageCancelled();

	bool ValidateActivationSetup(const FGameplayAbilityActorInfo* ActorInfo) const;
	void EndFromMontage(bool bWasCancelled);
};
