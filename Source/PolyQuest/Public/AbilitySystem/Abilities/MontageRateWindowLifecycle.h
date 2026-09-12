#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayTagContainer.h"

class UAnimInstance;
class UAnimMontage;
class UAnimNotifyState_MontageRateWindow;
class UGameplayAbility;

/**
 * Active RateWindow record identifying the authored window instance and its target play rate.
 */
struct POLYQUEST_API FRateWindowActiveEntry
{
	TWeakObjectPtr<const UObject> WeakSourceAnimation;
	TWeakObjectPtr<const UAnimNotifyState_MontageRateWindow> WeakNotifyState;
	float TargetRate = 1.0f;

	bool Matches(const UObject* InSource, const UAnimNotifyState_MontageRateWindow* InNotify) const
	{
		return WeakSourceAnimation.Get() == InSource && WeakNotifyState.Get() == InNotify;
	}
};

/**
 * Ability-side lifecycle helper for playback-rate override windows (RateWindow).
 * Owns the captured baseline play rate, the active window identity collection (Last-Active-Wins),
 * and fail-closed validation across event callbacks and termination paths.
 */
struct POLYQUEST_API FAbilityMontageRateWindowLifecycle
{
public:
	void BindAndCapture(
		UGameplayAbility* InAbility,
		UAnimInstance* InAnimInstance,
		UAnimMontage* InMontage,
		const FGameplayTag& InBeginTag,
		const FGameplayTag& InEndTag);

	void HandleBegin(const FGameplayEventData& Payload);
	void HandleEnd(const FGameplayEventData& Payload);
	void RestoreAndClear();

	bool IsBound() const { return bCaptured; }

#if WITH_DEV_AUTOMATION_TESTS
	float GetBaselinePlayRate() const { return BaselinePlayRate; }
	int32 GetActiveWindowCount() const { return ActiveWindows.Num(); }
	int32 GetStackDepth() const { return ActiveWindows.Num(); }
	float GetCurrentTargetRate() const { return ActiveWindows.IsEmpty() ? BaselinePlayRate : ActiveWindows.Last().TargetRate; }
	const TArray<FRateWindowActiveEntry>& GetActiveWindows() const { return ActiveWindows; }
#endif

	bool IsMontageOrSequenceMatch(const UObject* OptionalObject) const;
	bool IsValidNotifyForSource(const UObject* SourceAnimation, const UAnimNotifyState_MontageRateWindow* RateNotify) const;

#if WITH_DEV_AUTOMATION_TESTS
	bool TestApplyBegin(const UObject* InSource, const UAnimNotifyState_MontageRateWindow* InNotify, float NewRate, float& OutAppliedRate);
	bool TestApplyEnd(const UObject* InSource, const UAnimNotifyState_MontageRateWindow* InNotify, float& OutRestoredRate);

	void SetTestBypassMontageActiveCheck(bool bBypass) { bTestBypassMontageActiveCheck = bBypass; }
	bool GetTestBypassMontageActiveCheck() const { return bTestBypassMontageActiveCheck; }

	void SetTestCapturedBaseline(float InRate)
	{
		BaselinePlayRate = FMath::IsFinite(InRate) && InRate > KINDA_SMALL_NUMBER ? InRate : 1.0f;
		bCaptured = true;
	}

	void SetTestActiveContext(
		UGameplayAbility* InAbility,
		UAnimInstance* InAnimInstance,
		UAnimMontage* InMontage,
		const FGameplayTag& InBeginTag,
		const FGameplayTag& InEndTag,
		float InBaselineRate)
	{
		WeakAbility = InAbility;
		WeakAnimInstance = InAnimInstance;
		WeakMontage = InMontage;
		RateWindowBeginEventTag = InBeginTag;
		RateWindowEndEventTag = InEndTag;
		BaselinePlayRate = FMath::IsFinite(InBaselineRate) && InBaselineRate > KINDA_SMALL_NUMBER ? InBaselineRate : 1.0f;
		bCaptured = true;
	}
#endif

private:
	TWeakObjectPtr<UGameplayAbility> WeakAbility;
	TWeakObjectPtr<UAnimInstance> WeakAnimInstance;
	TWeakObjectPtr<UAnimMontage> WeakMontage;

	FGameplayTag RateWindowBeginEventTag;
	FGameplayTag RateWindowEndEventTag;

	TArray<FRateWindowActiveEntry> ActiveWindows;
	float BaselinePlayRate = 1.0f;
	bool bCaptured = false;

#if WITH_DEV_AUTOMATION_TESTS
	bool bTestBypassMontageActiveCheck = false;
#endif
};
