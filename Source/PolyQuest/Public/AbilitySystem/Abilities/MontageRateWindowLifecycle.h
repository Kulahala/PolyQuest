#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayTagContainer.h"

class UAnimInstance;
class UAnimMontage;
class UGameplayAbility;

/**
 * Ability-side lifecycle helper for playback-rate override windows (RateWindow).
 * Owns the captured baseline play rate, the LIFO restoration rate stack,
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
	int32 GetStackDepth() const { return RateStack.Num(); }
	const TArray<float>& GetRateStack() const { return RateStack; }
#endif

	bool IsMontageOrSequenceMatch(const UObject* OptionalObject) const;

#if WITH_DEV_AUTOMATION_TESTS
	bool TestApplyBegin(float CurrentRate, float NewRate, float& OutAppliedRate);
	bool TestApplyEnd(float& OutRestoredRate);

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

	TArray<float> RateStack;
	float BaselinePlayRate = 1.0f;
	bool bCaptured = false;

#if WITH_DEV_AUTOMATION_TESTS
	bool bTestBypassMontageActiveCheck = false;
#endif

	bool PushRate(float CurrentRate, float NewRate);
	bool PopRate(float& OutRestoredRate);
};
