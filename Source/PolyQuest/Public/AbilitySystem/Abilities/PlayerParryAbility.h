#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/StaminaActionAbility.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayTagContainer.h"
#include "PlayerParryAbility.generated.h"

class AActor;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAnimInstance;
class UAnimMontage;
class UGameplayEffect;
class USoundBase;
struct FHitResult;

/**
 * Timed front-arc Parry. Startup pays only the Stamina Cost through
 * CommitStaminaCostOnly; the native cooldown is committed exactly once from
 * the single EndAbility path after the authored Montage completes naturally.
 */
UCLASS()
class POLYQUEST_API UPlayerParryAbility : public UStaminaActionAbility
{
	GENERATED_BODY()

public:
	UPlayerParryAbility();

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

	/** True only while the authored Parry window is open on the active Montage. */
	bool IsParryActive() const { return bParryWindowOpen && !bEndAbilityRequested; }

	/** Consumes one resolver-validated front-arc melee contact during the active window. */
	bool TryParryMeleeHit(AActor* AttackingActor, const FHitResult& HitResult);

#if WITH_DEV_AUTOMATION_TESTS
	const FGameplayTagContainer& GetTestActivationBlockedTags() const { return ActivationBlockedTags; }
	void SetTestCurrentActorInfo(const FGameplayAbilityActorInfo* InActorInfo) { CurrentActorInfo = InActorInfo; }
	void SetTestCurrentSpecHandle(const FGameplayAbilitySpecHandle InHandle) { CurrentSpecHandle = InHandle; }
	void TestSetParryWindowOpen(bool bOpen) { bParryWindowOpen = bOpen; }
	void SetTestParryCounterPoiseGameplayEffectClass(TSubclassOf<UGameplayEffect> InClass) { ParryCounterPoiseGameplayEffectClass = InClass; }
	void SetTestParryPoiseDamage(float InPoiseDamage) { ParryPoiseDamage = InPoiseDamage; }
	void SetTestBypassAudioPlayback(const bool bBypass) { bTestBypassAudioPlayback = bBypass; }
	int32 GetTestParrySuccessFeedbackCount() const { return TestParrySuccessFeedbackCount; }
	int32 GetTestParrySuccessSoundDispatchCount() const { return TestParrySuccessSoundDispatchCount; }
	FVector GetTestLastParrySuccessSoundLocation() const { return TestLastParrySuccessSoundLocation; }
	const FGameplayTagContainer& GetTestDefenseCancelableAbilityTags() const { return DefenseCancelableAbilityTags; }
#endif

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Player|Parry", meta = (AllowPrivateAccess = "true", ToolTip = "弹反/招架动作动画 Montage 资产。"))
	TObjectPtr<UAnimMontage> ParryMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Player|Parry", meta = (AllowPrivateAccess = "true", ToolTip = "弹反成功对攻击方施加削韧惩罚的 GameplayEffect 类。"))
	TSubclassOf<UGameplayEffect> ParryCounterPoiseGameplayEffectClass;

	/** Poise removed from the attacker by one successful Parry; applied as negative Data.Poise.Parry. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Player|Parry", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", ToolTip = "弹反成功直接扣除攻击方的削韧数值。"))
	float ParryPoiseDamage = 100.0f;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> ParryWindowBeginTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> ParryWindowEndTask;

	UPROPERTY(Transient)
	TObjectPtr<UAnimInstance> BoundAnimInstance;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveMontage;

	FGameplayTag ParryAbilityTag;
	FGameplayTag ParryingStateTag;
	FGameplayTag ParryActiveStateTag;
	FGameplayTag ParryWindowBeginEventTag;
	FGameplayTag ParryWindowEndEventTag;
	FGameplayTag ParryPoiseDataTag;
	FGameplayTag AttackingStateTag;
	FGameplayTag DefenseCancelableStateTag;
	FGameplayTag DeadStateTag;
	FGameplayTag StunnedStateTag;
	FGameplayTagContainer DefenseCancelableAbilityTags;
	bool bParryWindowOpen = false;
	bool bParryActiveTagApplied = false;
	bool bMovementLockedByParry = false;
	bool bMontageCompletedNaturally = false;
	bool bEndAbilityRequested = false;

	UFUNCTION()
	void OnActiveMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	void OnParryWindowBegin(FGameplayEventData Payload);

	UFUNCTION()
	void OnParryWindowEnd(FGameplayEventData Payload);

	bool ValidateActivationSetup(const FGameplayAbilityActorInfo* ActorInfo) const;
	bool IsParryWindowEventFromActiveMontage(const FGameplayEventData& Payload) const;
	bool IsAttackerInParryArc(const AActor* AttackingActor) const;
	void EndFromMontage(bool bWasCancelled);
	void TriggerParrySuccessFeedback(const FHitResult& HitResult);

#if WITH_DEV_AUTOMATION_TESTS
	int32 TestParrySuccessFeedbackCount = 0;
	int32 TestParrySuccessSoundDispatchCount = 0;
	FVector TestLastParrySuccessSoundLocation = FVector::ZeroVector;
	bool bTestBypassAudioPlayback = false;
#endif
};
