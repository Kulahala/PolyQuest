#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/StaminaActionAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Combat/Melee/MeleeMotionWarping.h"
#include "GameplayTagContainer.h"
#include "AbilitySystem/Abilities/MontageRateWindowLifecycle.h"
#include "LightAttackAbility.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_MeleeTraceWindow;
class UAbilityTask_WaitGameplayEvent;
class UAnimInstance;
class UAnimMontage;
class UComboChainDataAsset;
class UGameplayEffect;
class ULightAttackAbility;

/**
 * Transient context for per-entry RateWindow event isolation.
 */
UCLASS(Transient)
class POLYQUEST_API ULightAttackRateWindowContext : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(Transient)
	TWeakObjectPtr<ULightAttackAbility> OwningAbility;

	uint32 Token = 0;

	UFUNCTION()
	void OnRateWindowBegin(FGameplayEventData Payload);

	UFUNCTION()
	void OnRateWindowEnd(FGameplayEventData Payload);
};

UCLASS()
class POLYQUEST_API ULightAttackAbility : public UStaminaActionAbility
{
	GENERATED_BODY()

public:
	ULightAttackAbility();

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

	/**
	 * Pure geometric evaluator for melee motion-warp contact assist.
	 * Calculates the one-shot target Transform if all geometry and ground conditions are met.
	 * Returns true if a valid warp transform was generated, false otherwise.
	 */
	static bool EvaluateMeleeMotionWarpTransform(
		const FVector& PlayerLocation,
		const FVector& PlayerForwardVector,
		bool bPlayerOnGround,
		const FVector& TargetLocation,
		bool bTargetOnGround,
		const struct FComboChainEntry& EntryConfig,
		FTransform& OutWarpTransform);

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combo", meta = (ToolTip = "连续轻攻击连招段落配置数据资产（ComboChainDataAsset）。"))
	TObjectPtr<UComboChainDataAsset> ComboDefinition;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack", meta = (ToolTip = "轻攻击命中时施加的伤害 GameplayEffect 类。"))
	TSubclassOf<UGameplayEffect> DamageGameplayEffectClass;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> TraceWindowBeginTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> TraceWindowEndTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_MeleeTraceWindow> TraceWindowTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> DodgeCancelWindowBeginTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> DodgeCancelWindowEndTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> PrimaryAttackPressedTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> ComboInputWindowBeginTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> ComboInputWindowEndTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> ComboBranchWindowBeginTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> ComboBranchWindowEndTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> RateWindowBeginTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> RateWindowEndTask;

	UPROPERTY(Transient)
	TObjectPtr<ULightAttackRateWindowContext> ActiveRateWindowContext;

	UPROPERTY(Transient)
	TObjectPtr<UAnimInstance> BoundAnimInstance;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveEntryMontage;

	FAbilityMontageRateWindowLifecycle RateWindowLifecycle;
	uint32 CurrentActivationToken = 0;
	int32 ActiveMontageInstanceID = INDEX_NONE;

	FGameplayTag DodgeCancelWindowBeginEventTag;
	FGameplayTag DodgeCancelWindowEndEventTag;
	FGameplayTag DodgeCancelableStateTag;
	FGameplayTag DefenseCancelableStateTag;
	FGameplayTag TraceWindowBeginEventTag;
	FGameplayTag TraceWindowEndEventTag;
	FGameplayTag PrimaryAttackPressedEventTag;
	FGameplayTag PrimaryAttackInputTag;
	FGameplayTag ComboInputWindowBeginEventTag;
	FGameplayTag ComboInputWindowEndEventTag;
	FGameplayTag ComboBranchWindowBeginEventTag;
	FGameplayTag ComboBranchWindowEndEventTag;
	FGameplayTag RateWindowBeginEventTag;
	FGameplayTag RateWindowEndEventTag;

	int32 ActiveEntryIndex = INDEX_NONE;
	bool bDodgeCancelable = false;
	bool bComboInputWindowOpen = false;
	bool bComboBranchWindowOpen = false;
	bool bContinuationBuffered = false;
	bool bComboTransitionInProgress = false;
	bool bEndAbilityRequested = false;

	UFUNCTION()
	void OnActiveMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	void OnTraceWindowBegin(FGameplayEventData Payload);

	UFUNCTION()
	void OnTraceWindowEnd(FGameplayEventData Payload);

	UFUNCTION()
	void OnDodgeCancelWindowBegin(FGameplayEventData Payload);

	UFUNCTION()
	void OnDodgeCancelWindowEnd(FGameplayEventData Payload);

	UFUNCTION()
	void OnPrimaryAttackPressed(FGameplayEventData Payload);

	UFUNCTION()
	void OnComboInputWindowBegin(FGameplayEventData Payload);

	UFUNCTION()
	void OnComboInputWindowEnd(FGameplayEventData Payload);

	UFUNCTION()
	void OnComboBranchWindowBegin(FGameplayEventData Payload);

	UFUNCTION()
	void OnComboBranchWindowEnd(FGameplayEventData Payload);

	void OnRateWindowBegin(const FGameplayEventData& Payload);
	void OnRateWindowEnd(const FGameplayEventData& Payload);
	void ClearRateWindow(bool bRestoreRate);

	void EndFromMontage(bool bWasCancelled);
	bool ValidateComboDefinition() const;
	bool StartComboEntry(int32 EntryIndex);
	bool IsGameplayEventFromActiveMontage(const FGameplayEventData& Payload) const;
	bool IsPrimaryAttackInputEvent(const FGameplayEventData& Payload) const;
	void TryConsumeBufferedComboContinuation();
	void OpenTraceWindow(const TArray<FName>& InTraceSourceNames);
	void CloseTraceWindow();
	void SetDodgeCancelable(bool bShouldBeCancelable);
	void TryApplyMeleeMotionWarpTarget(class APlayerCharacter* PlayerCharacter, const struct FComboChainEntry& EntryConfig);
	void ResetMeleeMotionWarpState();

	FMeleeMotionWarpSnapshot MeleeMotionWarpSnapshot;

	TWeakObjectPtr<const class UAnimNotifyState_AttackTraceWindow> ActiveTraceNotifyState;

	friend class ULightAttackRateWindowContext;

#if WITH_DEV_AUTOMATION_TESTS
public:
	const FGameplayTag& GetTestRateWindowBeginEventTag() const { return RateWindowBeginEventTag; }
	const FGameplayTag& GetTestRateWindowEndEventTag() const { return RateWindowEndEventTag; }
	const FAbilityMontageRateWindowLifecycle& GetTestRateWindowLifecycle() const { return RateWindowLifecycle; }
	FAbilityMontageRateWindowLifecycle& GetTestRateWindowLifecycle_Mutable() { return RateWindowLifecycle; }
	int32 GetTestActiveMontageInstanceID() const { return ActiveMontageInstanceID; }
	void SetTestActiveMontageInstanceID(int32 InID) { ActiveMontageInstanceID = InID; }
	uint32 GetTestCurrentActivationToken() const { return CurrentActivationToken; }
	ULightAttackRateWindowContext* GetTestActiveRateWindowContext() const { return ActiveRateWindowContext.Get(); }
	void TestClearRateWindow() { ClearRateWindow(true); }

	void SetTestComboDefinition(UComboChainDataAsset* InComboDefinition) { ComboDefinition = InComboDefinition; }
	void SetTestBoundAnimInstance(UAnimInstance* InAnimInstance) { BoundAnimInstance = InAnimInstance; }
	void SetTestActorInfo(const FGameplayAbilityActorInfo* InActorInfo) { CurrentActorInfo = InActorInfo; }
	void SetTestAbilityActive(bool bInActive) { bIsActive = bInActive; }
	void SetTestBypassMontageActiveCheck(bool bBypass)
	{
		bTestBypassMontageActiveCheck = bBypass;
		RateWindowLifecycle.SetTestBypassMontageActiveCheck(bBypass);
	}
	void SetTestCostGameplayEffectClass(TSubclassOf<UGameplayEffect> InClass) { CostGameplayEffectClass = InClass; }
	void SetTestDamageGameplayEffectClass(TSubclassOf<UGameplayEffect> InClass) { DamageGameplayEffectClass = InClass; }
	void SetTestStaminaRegenDelayGameplayEffectClass(TSubclassOf<UGameplayEffect> InClass) { StaminaRegenDelayGameplayEffectClass = InClass; }
	bool GetTestBypassMontageActiveCheck() const { return bTestBypassMontageActiveCheck; }
	bool TestStartComboEntry(int32 EntryIndex) { return StartComboEntry(EntryIndex); }
	void TestResetMeleeMotionWarpState() { ResetMeleeMotionWarpState(); }
	bool HasTestMeleeMotionWarpSnapshot() const { return MeleeMotionWarpSnapshot.CapturedTarget.IsValid(); }
	bool HasTestMeleeMotionWarpCaptureAttempted() const { return MeleeMotionWarpSnapshot.bAttemptedCapture; }
	FVector GetTestMeleeMotionWarpCapturedLocation() const { return MeleeMotionWarpSnapshot.CapturedTargetLocation; }
	bool GetTestMeleeMotionWarpCapturedOnGround() const { return MeleeMotionWarpSnapshot.bCapturedTargetOnGround; }
	int32 GetTestActiveEntryIndex() const { return ActiveEntryIndex; }
	void SetTestActiveEntryIndex(int32 InIndex) { ActiveEntryIndex = InIndex; }
	void SetTestEndAbilityRequested(bool bRequested) { bEndAbilityRequested = bRequested; }

private:
	bool bTestBypassMontageActiveCheck = false;
#endif
};
