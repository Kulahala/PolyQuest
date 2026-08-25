#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayTagContainer.h"
#include "EnemyStanceBreakAbility.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAnimInstance;
class UAnimMontage;

/**
 * Server-authoritative enemy stance break. Poise delivery and event routing
 * stay outside the ability; this ability owns only the authored presentation,
 * interruption, and recovery teardown.
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
	const FGameplayTagContainer& GetAbilitiesToCancel() const { return AbilitiesToCancel; }
#endif

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Enemy|Stance Break", meta = (AllowPrivateAccess = "true", ToolTip = "敌人韧性归零发生架势崩解（Stance Break）时播放的虚弱硬直动画 Montage 资产。"))
	TObjectPtr<UAnimMontage> StanceBreakMontage;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAnimInstance> BoundAnimInstance;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveMontage;

	FGameplayTag StanceBreakAbilityTag;
	FGameplayTag StanceBreakEventTag;
	FGameplayTag StunnedStateTag;
	FGameplayTag HitReactingStateTag;
	FGameplayTag EnemyMeleeAbilityTag;
	FGameplayTag EnemyHitReactionAbilityTag;
	FGameplayTag EnemySmallHitReactionAbilityTag;
	FGameplayTag EnemyLaunchReactionAbilityTag;
	FGameplayTagContainer AbilitiesToCancel;
	bool bMovementLockedByStanceBreak = false;
	bool bEndAbilityRequested = false;

	UFUNCTION()
	void OnActiveMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	bool ValidateActivationSetup(const FGameplayAbilityActorInfo* ActorInfo) const;
	void EndFromMontage(bool bWasCancelled);
};
