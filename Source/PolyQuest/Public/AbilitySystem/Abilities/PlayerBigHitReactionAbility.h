#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayTagContainer.h"
#include "PlayerBigHitReactionAbility.generated.h"

class ACharacter;
class APlayerCharacter;
class UAbilityTask_PlayMontageAndWait;
class UAnimInstance;
class UAnimMontage;

/**
 * Server-authoritative, full-body player big hit reaction.
 * Interrupts active player actions, blocks movement and jump input,
 * prevents ledge walk-off, and lets Montage Root Motion drive planar displacement.
 */
UCLASS()
class POLYQUEST_API UPlayerBigHitReactionAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UPlayerBigHitReactionAbility();

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
	const FGameplayTagContainer& GetTestBlockAbilitiesWithTag() const { return BlockAbilitiesWithTag; }
	const FGameplayTagContainer& GetTestAbilitiesToCancel() const { return AbilitiesToCancel; }
	const TArray<FAbilityTriggerData>& GetTestAbilityTriggers() const { return AbilityTriggers; }
	const FVector& GetImpactDirectionSnapshot() const { return ImpactDirectionSnapshot; }
#endif

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Player|Reaction", meta = (AllowPrivateAccess = "true", ToolTip = "攻击者位于受击者本地前方时播放的全身受击动画 Montage 资产。此名称描述攻击者来源方向，非受击者位移方向。四个方向属性（Front、Back、Left、Right）必须完整同时配置。"))
	TObjectPtr<UAnimMontage> FrontBigHitReactionMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Player|Reaction", meta = (AllowPrivateAccess = "true", ToolTip = "攻击者位于受击者本地后方时播放的全身受击动画 Montage 资产。此名称描述攻击者来源方向，非受击者位移方向。四个方向属性（Front、Back、Left、Right）必须完整同时配置。"))
	TObjectPtr<UAnimMontage> BackBigHitReactionMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Player|Reaction", meta = (AllowPrivateAccess = "true", ToolTip = "攻击者位于受击者本地左方时播放的全身受击动画 Montage 资产。此名称描述攻击者来源方向，非受击者位移方向。四个方向属性（Front、Back、Left、Right）必须完整同时配置。"))
	TObjectPtr<UAnimMontage> LeftBigHitReactionMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Player|Reaction", meta = (AllowPrivateAccess = "true", ToolTip = "攻击者位于受击者本地右方时播放的全身受击动画 Montage 资产。此名称描述攻击者来源方向，非受击者位移方向。四个方向属性（Front、Back、Left、Right）必须完整同时配置。"))
	TObjectPtr<UAnimMontage> RightBigHitReactionMontage;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAnimInstance> BoundAnimInstance;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveMontage;

	UPROPERTY(Transient)
	TWeakObjectPtr<APlayerCharacter> BoundPlayerCharacter;

	FGameplayTag BigHitReactionAbilityTag;
	FGameplayTag BigHitReactionEventTag;
	FGameplayTag HitReactingStateTag;
	FGameplayTag StunnedStateTag;
	FGameplayTag DeadStateTag;
	FGameplayTag HyperArmorStateTag;
	FGameplayTagContainer AbilitiesToCancel;

	FVector ImpactDirectionSnapshot = FVector::ZeroVector;
	bool bSavedCanWalkOffLedges = true;
	bool bLedgeSettingModified = false;
	bool bMovementModeDelegateBound = false;
	bool bEndAbilityRequested = false;

	UFUNCTION()
	void OnActiveMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	void OnMovementModeChanged(ACharacter* Character, EMovementMode PrevMovementMode, uint8 PreviousCustomMode);

	bool ValidateActivationSetup(const FGameplayAbilityActorInfo* ActorInfo) const;
	void EndFromMontage(bool bWasCancelled);
};
