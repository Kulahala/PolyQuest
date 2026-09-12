#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystem/Abilities/MontageRateWindowLifecycle.h"
#include "GameplayTagContainer.h"
#include "EnemyMeleeAbility.generated.h"

class UAbilityTask_MeleeTraceWindow;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAnimInstance;
class UAnimMontage;
class UEnemyAttackProfile;
class UGameplayEffect;
class UEnemyMeleeAbility;

/**
 * Transient context for per-activation RateWindow event isolation.
 */
UCLASS(Transient)
class POLYQUEST_API UEnemyMeleeRateWindowContext : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(Transient)
	TWeakObjectPtr<UEnemyMeleeAbility> OwningAbility;

	uint32 Token = 0;

	UFUNCTION()
	void OnRateWindowBegin(FGameplayEventData Payload);

	UFUNCTION()
	void OnRateWindowEnd(FGameplayEventData Payload);
};

/**
 * One server-authoritative enemy melee action. It owns only montage timing,
 * trace-window delivery, and teardown; targeting remains on the controller.
 */
UCLASS()
class POLYQUEST_API UEnemyMeleeAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UEnemyMeleeAbility();

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
	const FGameplayTag& GetTestRateWindowBeginEventTag() const { return RateWindowBeginEventTag; }
	const FGameplayTag& GetTestRateWindowEndEventTag() const { return RateWindowEndEventTag; }
	const FAbilityMontageRateWindowLifecycle& GetTestRateWindowLifecycle() const { return RateWindowLifecycle; }
	FAbilityMontageRateWindowLifecycle& GetTestRateWindowLifecycle_Mutable() { return RateWindowLifecycle; }
	int32 GetTestActiveMontageInstanceID() const { return ActiveMontageInstanceID; }
	void SetTestActiveMontageInstanceID(int32 InID) { ActiveMontageInstanceID = InID; }
	uint32 GetTestCurrentActivationToken() const { return CurrentActivationToken; }
	UEnemyMeleeRateWindowContext* GetTestActiveRateWindowContext() const { return ActiveRateWindowContext.Get(); }
	void SetTestBypassMontageActiveCheck(bool bBypass)
	{
		bTestBypassMontageActiveCheck = bBypass;
		RateWindowLifecycle.SetTestBypassMontageActiveCheck(bBypass);
	}
	bool GetTestBypassMontageActiveCheck() const { return bTestBypassMontageActiveCheck; }
	void SetTestAbilityActive(bool bInActive) { bIsActive = bInActive; }
	void SetTestActorInfo(FGameplayAbilitySpecHandle InHandle, const FGameplayAbilityActorInfo* InActorInfo)
	{
		SetCurrentActorInfo(InHandle, InActorInfo);
	}
#endif

private:
	UPROPERTY(Transient)
	TObjectPtr<const UEnemyAttackProfile> ActiveAttackProfile;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> TraceWindowBeginTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> TraceWindowEndTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> HyperArmorBeginTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> HyperArmorEndTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> RateWindowBeginTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> RateWindowEndTask;

	UPROPERTY(Transient)
	TObjectPtr<UEnemyMeleeRateWindowContext> ActiveRateWindowContext;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_MeleeTraceWindow> TraceWindowTask;

	UPROPERTY(Transient)
	TObjectPtr<UAnimInstance> BoundAnimInstance;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveMontage;

	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> ActiveDamageGameplayEffectClass;

	FAbilityMontageRateWindowLifecycle RateWindowLifecycle;

	FGameplayTag EnemyMeleeAbilityTag;
	FGameplayTag AttackingStateTag;
	FGameplayTag HitReactingStateTag;
	FGameplayTag TraceWindowBeginEventTag;
	FGameplayTag TraceWindowEndEventTag;
	FGameplayTag HyperArmorStateTag;
	FGameplayTag HyperArmorBeginEventTag;
	FGameplayTag HyperArmorEndEventTag;
	FGameplayTag RateWindowBeginEventTag;
	FGameplayTag RateWindowEndEventTag;
	FGameplayTag TeardownOnUnpossessTag;
	FGameplayTag FacingBlockedStateTag;
	float ActiveCooldownAfterAttack = 0.0f;
	float ActiveGuardStaminaDamage = 0.0f;
	uint32 CurrentActivationToken = 0;
	int32 ActiveMontageInstanceID = INDEX_NONE;
	bool bAttackStarted = false;
	bool bHyperArmorActive = false;
	bool bEndAbilityRequested = false;

#if WITH_DEV_AUTOMATION_TESTS
	bool bTestBypassMontageActiveCheck = false;
#endif

	UFUNCTION()
	void OnActiveMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	void OnTraceWindowBegin(FGameplayEventData Payload);

	UFUNCTION()
	void OnTraceWindowEnd(FGameplayEventData Payload);

	UFUNCTION()
	void OnHyperArmorBegin(FGameplayEventData Payload);

	UFUNCTION()
	void OnHyperArmorEnd(FGameplayEventData Payload);

	void OnRateWindowBegin(const FGameplayEventData& Payload);
	void OnRateWindowEnd(const FGameplayEventData& Payload);
	void ClearRateWindow(bool bRestoreRate);

	bool ValidateActivationSetup(const FGameplayAbilityActorInfo* ActorInfo) const;
	bool IsGameplayEventFromActiveMontage(const FGameplayEventData& Payload) const;
	void EndFromMontage(bool bWasCancelled);
	void OpenTraceWindow(const TArray<FName>& InTraceSourceNames);
	void CloseTraceWindow();

	TWeakObjectPtr<const class UAnimNotifyState_AttackTraceWindow> ActiveTraceNotifyState;

	friend class UEnemyMeleeRateWindowContext;
};
