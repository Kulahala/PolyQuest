#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayTagContainer.h"
#include "PlayerSmallHitReactionAbility.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAnimInstance;
class UAnimMontage;

/**
 * Server-authoritative, non-interrupting player small hit reaction.
 * Plays an additive/overlay montage without blocking movement, input, or active attacks.
 */
UCLASS()
class POLYQUEST_API UPlayerSmallHitReactionAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UPlayerSmallHitReactionAbility();

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
#endif

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Player|Reaction", meta = (AllowPrivateAccess = "true", ToolTip = "攻击者位于受击者本地前方时播放的叠加层受击动画 Montage 资产。此名称描述攻击者来源方向，非受击者位移方向。四个方向属性（Front、Back、Left、Right）必须完整同时配置。"))
	TObjectPtr<UAnimMontage> FrontSmallHitReactionMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Player|Reaction", meta = (AllowPrivateAccess = "true", ToolTip = "攻击者位于受击者本地后方时播放的叠加层受击动画 Montage 资产。此名称描述攻击者来源方向，非受击者位移方向。四个方向属性（Front、Back、Left、Right）必须完整同时配置。"))
	TObjectPtr<UAnimMontage> BackSmallHitReactionMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Player|Reaction", meta = (AllowPrivateAccess = "true", ToolTip = "攻击者位于受击者本地左方时播放的叠加层受击动画 Montage 资产。此名称描述攻击者来源方向，非受击者位移方向。四个方向属性（Front、Back、Left、Right）必须完整同时配置。"))
	TObjectPtr<UAnimMontage> LeftSmallHitReactionMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Player|Reaction", meta = (AllowPrivateAccess = "true", ToolTip = "攻击者位于受击者本地右方时播放的叠加层受击动画 Montage 资产。此名称描述攻击者来源方向，非受击者位移方向。四个方向属性（Front、Back、Left、Right）必须完整同时配置。"))
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
	void OnActiveMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	bool ValidateActivationSetup(const FGameplayAbilityActorInfo* ActorInfo) const;
	void EndFromMontage(bool bWasCancelled);
};
