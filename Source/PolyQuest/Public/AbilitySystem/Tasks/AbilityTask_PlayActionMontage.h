#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilitySystem/Abilities/MontageRateWindowLifecycle.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "GameplayTagContainer.h"
#include "AbilityTask_PlayActionMontage.generated.h"

class UAnimNotifyState;

UENUM(BlueprintType)
enum class EActionMontageCancelPolicy : uint8
{
	None,
	DodgeOnly,
	DodgeAndDefense
};

struct FCancelWindowIdentity
{
	TWeakObjectPtr<const UObject> SourceAnimation;
	TWeakObjectPtr<const UAnimNotifyState> NotifyState;

	bool operator==(const FCancelWindowIdentity& Other) const
	{
		return SourceAnimation == Other.SourceAnimation && NotifyState == Other.NotifyState;
	}

	friend uint32 GetTypeHash(const FCancelWindowIdentity& Key)
	{
		return HashCombine(GetTypeHash(Key.SourceAnimation), GetTypeHash(Key.NotifyState));
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPlayActionMontageDelegate);

/**
 * Standard Action Montage playback task with default RateWindow and CancelWindow management.
 * Plays a montage via ASC, owns RateWindow and CancelWindow lifecycles, and fail-closed validates
 * event source identity, reentrancy boundaries, and instance-authorized teardown.
 */
UCLASS()
class POLYQUEST_API UAbilityTask_PlayActionMontage : public UAbilityTask
{
	GENERATED_BODY()

public:
	UAbilityTask_PlayActionMontage(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(BlueprintAssignable)
	FPlayActionMontageDelegate OnCompleted;

	UPROPERTY(BlueprintAssignable)
	FPlayActionMontageDelegate OnBlendedIn;

	UPROPERTY(BlueprintAssignable)
	FPlayActionMontageDelegate OnBlendOut;

	UPROPERTY(BlueprintAssignable)
	FPlayActionMontageDelegate OnInterrupted;

	UPROPERTY(BlueprintAssignable)
	FPlayActionMontageDelegate OnCancelled;

	UPROPERTY(BlueprintAssignable)
	FPlayActionMontageDelegate OnFailed;

	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (DisplayName = "PlayActionMontage",
		HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_PlayActionMontage* PlayActionMontage(
		UGameplayAbility* OwningAbility,
		FName TaskInstanceName,
		UAnimMontage* MontageToPlay,
		float Rate = 1.0f,
		FName StartSection = NAME_None,
		float AnimRootMotionTranslationScale = 1.0f,
		float StartTimeSeconds = 0.0f,
		bool bAllowInterruptAfterBlendOut = false,
		EActionMontageCancelPolicy CancelPolicy = EActionMontageCancelPolicy::None);

	virtual void Activate() override;
	virtual void ExternalCancel() override;
	virtual FString GetDebugString() const override;

	/** Latch active cancel windows across a montage pause (e.g. Charged HoldReady). Prevents premature tag revocation while paused. */
	void LatchCancelWindowsAcrossPause();

	/** Unlatch cancel windows after montage pause is resumed (e.g. Charged Release). Natural-ended windows are cleaned up, active windows wait for real End. */
	void UnlatchCancelWindowsAfterPause();

	EActionMontageCancelPolicy GetCancelPolicy() const { return CancelPolicy; }
	bool IsCancelWindowLatched() const { return bCancelWindowLatched; }

protected:
	virtual void OnDestroy(bool AbilityEnded) override;

private:
	// --- Internal Callbacks ---
	UFUNCTION()
	void OnMontageBlendedIn(UAnimMontage* Montage);

	UFUNCTION()
	void OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	void OnGameplayAbilityCancelled();

	void OnRateWindowBeginReceived(const FGameplayEventData* Payload);
	void OnRateWindowEndReceived(const FGameplayEventData* Payload);

	// --- Private lifecycle & helpers ---
	void BindRateWindowEvents(UAbilitySystemComponent* ASC);
	void UnbindRateWindowEvents();
	bool ValidateRateWindowEventSource(const FGameplayEventData& Payload) const;
	bool CleanupTask(bool bStopMontage);
	bool StopPlayingMontage();
	void ResetRootMotionScale();
	bool CanResetRootMotionScale() const;

	// --- Playback configuration ---
	UPROPERTY()
	TObjectPtr<UAnimMontage> MontageToPlay;

	float Rate = 1.0f;
	FName StartSection;
	float AnimRootMotionTranslationScale = 1.0f;
	float StartTimeSeconds = 0.0f;
	bool bAllowInterruptAfterBlendOut = false;
	bool bAllowInterruptAfterBlendOutState = false;

	// --- Active instance & lifecycle state ---
	int32 BoundMontageInstanceID = INDEX_NONE;
	TWeakObjectPtr<UAnimInstance> BoundAnimInstance;
	FAbilityMontageRateWindowLifecycle RateWindowLifecycle;

	FGameplayTag RateWindowBeginEventTag;
	FGameplayTag RateWindowEndEventTag;
	FDelegateHandle RateWindowBeginHandle;
	FDelegateHandle RateWindowEndHandle;

	FOnMontageBlendedInEnded BlendedInDelegate;
	FOnMontageBlendingOutStarted BlendingOutDelegate;
	FOnMontageEnded MontageEndedDelegate;
	FDelegateHandle InterruptedHandle;

	// --- Cancel Window management ---
	void OnCancelWindowBeginReceived(const FGameplayEventData* Payload);
	void OnCancelWindowEndReceived(const FGameplayEventData* Payload);
	void BindCancelWindowEvents(UAbilitySystemComponent* ASC);
	void UnbindCancelWindowEvents();
	bool ValidateCancelWindowEventSource(const FGameplayEventData& Payload, FCancelWindowIdentity& OutIdentity, bool& OutReachedEnd) const;
	bool IsMontageOrSequenceMatch(const UObject* SourceAnimation) const;
	bool IsValidCancelNotifyForSource(const UObject* SourceAnimation, const UAnimNotifyState* Notify) const;
	void UpdateCancelPolicyTags();
	void RemoveContributedCancelTags();

	EActionMontageCancelPolicy CancelPolicy = EActionMontageCancelPolicy::None;
	TSet<FCancelWindowIdentity> ActiveCancelWindows;
	TSet<FCancelWindowIdentity> PendingNaturalEndWindows;
	bool bCancelWindowLatched = false;
	bool bContributedDodgeTag = false;
	bool bContributedDefenseTag = false;

	FGameplayTag CancelWindowBeginEventTag;
	FGameplayTag CancelWindowEndEventTag;
	FGameplayTag DodgeCancelStateTag;
	FGameplayTag DefenseCancelStateTag;
	FDelegateHandle CancelWindowBeginHandle;
	FDelegateHandle CancelWindowEndHandle;

	bool bTerminated = false;
	bool bAppliedRootMotionScale = false;

#if WITH_DEV_AUTOMATION_TESTS
	bool bTestBypassMontageActiveCheck = false;

public:
	int32 GetBoundMontageInstanceID() const { return BoundMontageInstanceID; }
	void SetTestBoundMontageInstanceID(int32 InID) { BoundMontageInstanceID = InID; }
	void SetTestBypassMontageActiveCheck(bool bBypass)
	{
		bTestBypassMontageActiveCheck = bBypass;
		RateWindowLifecycle.SetTestBypassMontageActiveCheck(bBypass);
	}
	bool GetTestBypassMontageActiveCheck() const { return bTestBypassMontageActiveCheck; }
	UAnimMontage* GetMontageToPlay() const { return MontageToPlay; }
	UAnimInstance* GetBoundAnimInstance() const { return BoundAnimInstance.Get(); }
	void SetTestBoundAnimInstance(UAnimInstance* InAnimInstance) { BoundAnimInstance = InAnimInstance; }
	const FAbilityMontageRateWindowLifecycle& GetRateWindowLifecycle() const { return RateWindowLifecycle; }
	FAbilityMontageRateWindowLifecycle& GetRateWindowLifecycle_Mutable() { return RateWindowLifecycle; }
	bool IsTerminated() const { return bTerminated; }
	void TestCleanupTask(bool bStopMontage) { CleanupTask(bStopMontage); }
	bool TestStopPlayingMontage() { return StopPlayingMontage(); }
	void TestBindRateWindowEvents(UAbilitySystemComponent* ASC) { BindRateWindowEvents(ASC); }
	void SetTestTaskActive(bool bActive)
	{
		TaskState = bActive ? EGameplayTaskState::Active : EGameplayTaskState::Finished;
	}
	void TestInvokeOnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
	{
		OnMontageEnded(Montage, bInterrupted);
	}

	int32 GetActiveCancelWindowsNum() const { return ActiveCancelWindows.Num(); }
	int32 GetActiveCancelWindowCount() const { return ActiveCancelWindows.Num(); }
	int32 GetPendingNaturalEndWindowsNum() const { return PendingNaturalEndWindows.Num(); }
	bool IsCancelWindowLatchedAcrossPause() const { return bCancelWindowLatched; }
	bool HasContributedDodgeTag() const { return bContributedDodgeTag; }
	bool HasContributedDefenseTag() const { return bContributedDefenseTag; }
	void TestInvokeCancelBegin(const FGameplayEventData& Payload) { OnCancelWindowBeginReceived(&Payload); }
	void TestInvokeCancelEnd(const FGameplayEventData& Payload) { OnCancelWindowEndReceived(&Payload); }
	void TestBindCancelWindowEvents(UAbilitySystemComponent* ASC) { BindCancelWindowEvents(ASC); }
	bool TestValidateCancelWindowEventSource(const FGameplayEventData& Payload, FCancelWindowIdentity& OutIdentity, bool& OutReachedEnd) const
	{
		return ValidateCancelWindowEventSource(Payload, OutIdentity, OutReachedEnd);
	}
	void TestUpdateCancelPolicyTags() { UpdateCancelPolicyTags(); }
	void SetTestCancelPolicy(EActionMontageCancelPolicy InPolicy) { CancelPolicy = InPolicy; }
#endif
};
