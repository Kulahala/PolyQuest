#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilitySystem/Abilities/MontageRateWindowLifecycle.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "GameplayTagContainer.h"
#include "AbilityTask_PlayActionMontage.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPlayActionMontageDelegate);

/**
 * Standard Action Montage playback task with default RateWindow management.
 * Plays a montage via ASC, owns the active RateWindow lifecycle, and fail-closed validates
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
		bool bAllowInterruptAfterBlendOut = false);

	virtual void Activate() override;
	virtual void ExternalCancel() override;
	virtual FString GetDebugString() const override;

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
#endif
};
