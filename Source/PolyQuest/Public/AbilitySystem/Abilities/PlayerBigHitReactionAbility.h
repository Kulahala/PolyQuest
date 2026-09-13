#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystem/Abilities/MontageRateWindowLifecycle.h"
#include "GameplayTagContainer.h"
#include "PlayerBigHitReactionAbility.generated.h"

class ACharacter;
class APlayerCharacter;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAnimInstance;
class UAnimMontage;
class UPlayerBigHitReactionAbility;

/** Transient receiver scoped to one RateWindow playback binding. */
UCLASS(Transient)
class POLYQUEST_API UPlayerBigHitReactionRateWindowContext : public UObject
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<UPlayerBigHitReactionAbility> OwningAbility;
	uint32 Token = 0;

	UFUNCTION()
	void OnBegin(FGameplayEventData Payload);

	UFUNCTION()
	void OnEnd(FGameplayEventData Payload);
};

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

	void SetTestMontages(UAnimMontage* InFront, UAnimMontage* InBack, UAnimMontage* InLeft, UAnimMontage* InRight)
	{
		FrontBigHitReactionMontage = InFront;
		BackBigHitReactionMontage = InBack;
		LeftBigHitReactionMontage = InLeft;
		RightBigHitReactionMontage = InRight;
	}

	UAnimMontage* GetTestFrontMontage() const { return FrontBigHitReactionMontage.Get(); }
	UAnimMontage* GetTestBackMontage() const { return BackBigHitReactionMontage.Get(); }
	UAnimMontage* GetTestLeftMontage() const { return LeftBigHitReactionMontage.Get(); }
	UAnimMontage* GetTestRightMontage() const { return RightBigHitReactionMontage.Get(); }
	UAnimMontage* GetTestActiveMontage() const { return ActiveMontage.Get(); }

	const FAbilityMontageRateWindowLifecycle& GetTestRateWindowLifecycle() const { return RateWindowLifecycle; }
	FAbilityMontageRateWindowLifecycle& GetTestRateWindowLifecycle_Mutable() { return RateWindowLifecycle; }
	UPlayerBigHitReactionRateWindowContext* GetTestRateWindowContext() const { return RateWindowContext.Get(); }
	int32 GetTestRateWindowMontageInstanceID() const { return RateWindowMontageInstanceID; }
	bool HasTestRateWindowTasks() const { return RateWindowBeginTask != nullptr || RateWindowEndTask != nullptr; }
	bool TestBindRateWindow(UAnimInstance* AnimInstance, UAnimMontage* Montage) { return BindRateWindow(AnimInstance, Montage); }
	void TestClearRateWindow() { ClearRateWindow(); }

	bool GetTestDodgeCancelable() const { return bDodgeCancelable; }
	bool HasTestCancelTasks() const { return CancelBeginTask != nullptr || CancelEndTask != nullptr; }
	void TestOnCancelWindowBegin(const FGameplayEventData& Payload) { OnCancelWindowBegin(Payload); }
	void TestOnCancelWindowEnd(const FGameplayEventData& Payload) { OnCancelWindowEnd(Payload); }
	void TestSetDodgeCancelable(bool bCancelable) { SetDodgeCancelable(bCancelable); }
	bool TestIsEventFromMontage(const FGameplayEventData& Payload, const UAnimMontage* ExpectedMontage) const
	{
		return IsEventFromMontage(Payload, ExpectedMontage);
	}
	bool CallTestValidateActivationSetup(const FGameplayAbilityActorInfo* ActorInfo) const
	{
		return ValidateActivationSetup(ActorInfo);
	}
#endif

private:
	friend struct FMontageRateWindowBinding;
	friend class UPlayerBigHitReactionRateWindowContext;

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
	TObjectPtr<UAbilityTask_WaitGameplayEvent> CancelBeginTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> CancelEndTask;

	UPROPERTY(Transient)
	TObjectPtr<UPlayerBigHitReactionRateWindowContext> RateWindowContext;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> RateWindowBeginTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> RateWindowEndTask;

	UPROPERTY(Transient)
	TObjectPtr<UAnimInstance> BoundAnimInstance;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveMontage;

	UPROPERTY(Transient)
	TWeakObjectPtr<APlayerCharacter> BoundPlayerCharacter;

	FAbilityMontageRateWindowLifecycle RateWindowLifecycle;
	TWeakObjectPtr<UAnimInstance> RateWindowAnimInstance;
	TWeakObjectPtr<UAnimMontage> RateWindowMontage;
	uint32 RateWindowBindingToken = 0;
	int32 RateWindowMontageInstanceID = INDEX_NONE;

	FGameplayTag BigHitReactionAbilityTag;
	FGameplayTag BigHitReactionEventTag;
	FGameplayTag HitReactingStateTag;
	FGameplayTag StunnedStateTag;
	FGameplayTag DeadStateTag;
	FGameplayTag HyperArmorStateTag;
	FGameplayTag CancelWindowBeginEventTag;
	FGameplayTag CancelWindowEndEventTag;
	FGameplayTag DodgeCancelableStateTag;
	FGameplayTag CancelableByDodgeAbilityTag;
	FGameplayTag RateWindowBeginEventTag;
	FGameplayTag RateWindowEndEventTag;
	FGameplayTagContainer AbilitiesToCancel;

	FVector ImpactDirectionSnapshot = FVector::ZeroVector;
	bool bSavedCanWalkOffLedges = true;
	bool bLedgeSettingModified = false;
	bool bMovementModeDelegateBound = false;
	bool bEndAbilityRequested = false;
	bool bDodgeCancelable = false;

	UFUNCTION()
	void OnActiveMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	void OnMovementModeChanged(ACharacter* Character, EMovementMode PrevMovementMode, uint8 PreviousCustomMode);

	UFUNCTION()
	void OnCancelWindowBegin(FGameplayEventData Payload);

	UFUNCTION()
	void OnCancelWindowEnd(FGameplayEventData Payload);

	void OnRateWindowBegin(const FGameplayEventData& Payload);
	void OnRateWindowEnd(const FGameplayEventData& Payload);

	bool BindRateWindow(UAnimInstance* AnimInstance, UAnimMontage* Montage);
	bool HasOwnedRateWindowMontageInstance() const;
	void ClearRateWindow();
	void SetDodgeCancelable(bool bShouldCancel);
	bool IsEventFromMontage(const FGameplayEventData& Payload, const UAnimMontage* ExpectedMontage) const;

	bool ValidateActivationSetup(const FGameplayAbilityActorInfo* ActorInfo) const;
	void EndFromMontage(bool bWasCancelled);
};
