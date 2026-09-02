#include "AbilitySystem/Abilities/MontageRateWindowLifecycle.h"

#include "Abilities/GameplayAbility.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "PolyQuest.h"

void FAbilityMontageRateWindowLifecycle::BindAndCapture(
	UGameplayAbility* InAbility,
	UAnimInstance* InAnimInstance,
	UAnimMontage* InMontage,
	const FGameplayTag& InBeginTag,
	const FGameplayTag& InEndTag)
{
	RestoreAndClear();

	WeakAbility = InAbility;
	WeakAnimInstance = InAnimInstance;
	WeakMontage = InMontage;
	RateWindowBeginEventTag = InBeginTag;
	RateWindowEndEventTag = InEndTag;

	if (!InAbility || !InAnimInstance || !InMontage || !InBeginTag.IsValid() || !InEndTag.IsValid())
	{
		return;
	}

#if WITH_DEV_AUTOMATION_TESTS
	const bool bMontageIsActive = bTestBypassMontageActiveCheck || InAnimInstance->Montage_IsActive(InMontage);
#else
	const bool bMontageIsActive = InAnimInstance->Montage_IsActive(InMontage);
#endif

	if (!bMontageIsActive)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("RateWindowLifecycle: Montage '%s' is not active during BindAndCapture."), *GetNameSafe(InMontage));
		return;
	}

	const float CapturedRate = InAnimInstance->Montage_GetPlayRate(InMontage);
	if (FMath::IsFinite(CapturedRate) && CapturedRate > KINDA_SMALL_NUMBER)
	{
		BaselinePlayRate = CapturedRate;
	}
	else
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("RateWindowLifecycle: invalid captured baseline rate %.3f for montage '%s'; falling back to 1.0."), CapturedRate, *GetNameSafe(InMontage));
		BaselinePlayRate = 1.0f;
	}

	bCaptured = true;
}

void FAbilityMontageRateWindowLifecycle::HandleBegin(const FGameplayEventData& Payload)
{
	const UGameplayAbility* Ability = WeakAbility.Get();
	UAnimInstance* AnimInstance = WeakAnimInstance.Get();
	UAnimMontage* Montage = WeakMontage.Get();

	if (!bCaptured || !Ability || !Ability->IsActive() || !AnimInstance || !Montage)
	{
		return;
	}

	const AActor* AvatarActor = Ability->GetAvatarActorFromActorInfo();
	if (!AvatarActor || Payload.Instigator != AvatarActor || Payload.Target != AvatarActor)
	{
		return;
	}

	if (!RateWindowBeginEventTag.IsValid() || !Payload.EventTag.IsValid() || !Payload.EventTag.MatchesTagExact(RateWindowBeginEventTag))
	{
		return;
	}

	if (!IsMontageOrSequenceMatch(Payload.OptionalObject.Get()))
	{
		return;
	}

	if (!FMath::IsFinite(Payload.EventMagnitude) || Payload.EventMagnitude <= 0.0f)
	{
		UE_LOG(LogPolyQuest, Verbose, TEXT("RateWindowLifecycle: non-positive or non-finite rate multiplier %.3f rejected."), Payload.EventMagnitude);
		return;
	}

#if WITH_DEV_AUTOMATION_TESTS
	const bool bMontageIsActive = bTestBypassMontageActiveCheck || AnimInstance->Montage_IsActive(Montage);
#else
	const bool bMontageIsActive = AnimInstance->Montage_IsActive(Montage);
#endif

	if (!bMontageIsActive)
	{
		return;
	}

	float CurrentRate = AnimInstance->Montage_GetPlayRate(Montage);
#if WITH_DEV_AUTOMATION_TESTS
	if (bTestBypassMontageActiveCheck && (!FMath::IsFinite(CurrentRate) || CurrentRate <= KINDA_SMALL_NUMBER))
	{
		CurrentRate = RateStack.IsEmpty() ? BaselinePlayRate : RateStack.Last();
	}
#endif

	if (!FMath::IsFinite(CurrentRate) || CurrentRate <= KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("RateWindowLifecycle: current play rate %.3f is invalid; rejecting Begin event."), CurrentRate);
		return;
	}

	if (!PushRate(CurrentRate, Payload.EventMagnitude))
	{
		return;
	}

	AnimInstance->Montage_SetPlayRate(Montage, Payload.EventMagnitude);
}

void FAbilityMontageRateWindowLifecycle::HandleEnd(const FGameplayEventData& Payload)
{
	const UGameplayAbility* Ability = WeakAbility.Get();
	UAnimInstance* AnimInstance = WeakAnimInstance.Get();
	UAnimMontage* Montage = WeakMontage.Get();

	if (!bCaptured || !Ability || !Ability->IsActive() || !AnimInstance || !Montage)
	{
		return;
	}

	const AActor* AvatarActor = Ability->GetAvatarActorFromActorInfo();
	if (!AvatarActor || Payload.Instigator != AvatarActor || Payload.Target != AvatarActor)
	{
		return;
	}

	if (!RateWindowEndEventTag.IsValid() || !Payload.EventTag.IsValid() || !Payload.EventTag.MatchesTagExact(RateWindowEndEventTag))
	{
		return;
	}

	if (!IsMontageOrSequenceMatch(Payload.OptionalObject.Get()))
	{
		return;
	}

#if WITH_DEV_AUTOMATION_TESTS
	const bool bMontageIsActive = bTestBypassMontageActiveCheck || AnimInstance->Montage_IsActive(Montage);
#else
	const bool bMontageIsActive = AnimInstance->Montage_IsActive(Montage);
#endif

	if (!bMontageIsActive)
	{
		return;
	}

	float RestoreRate = 0.0f;
	if (PopRate(RestoreRate))
	{
		AnimInstance->Montage_SetPlayRate(Montage, RestoreRate);
	}
}

bool FAbilityMontageRateWindowLifecycle::PushRate(float CurrentRate, float NewRate)
{
	if (!bCaptured
		|| !FMath::IsFinite(CurrentRate) || CurrentRate <= KINDA_SMALL_NUMBER
		|| !FMath::IsFinite(NewRate) || NewRate <= 0.0f)
	{
		return false;
	}

	RateStack.Add(CurrentRate);
	return true;
}

bool FAbilityMontageRateWindowLifecycle::PopRate(float& OutRestoredRate)
{
	if (!bCaptured || RateStack.IsEmpty())
	{
		return false;
	}

	const float CandidateRate = RateStack.Pop();
	if (!FMath::IsFinite(CandidateRate) || CandidateRate <= KINDA_SMALL_NUMBER)
	{
		RateStack.Reset();
		return false;
	}

	OutRestoredRate = CandidateRate;
	return true;
}

#if WITH_DEV_AUTOMATION_TESTS
bool FAbilityMontageRateWindowLifecycle::TestApplyBegin(float CurrentRate, float NewRate, float& OutAppliedRate)
{
	if (!PushRate(CurrentRate, NewRate))
	{
		return false;
	}

	OutAppliedRate = NewRate;
	return true;
}

bool FAbilityMontageRateWindowLifecycle::TestApplyEnd(float& OutRestoredRate)
{
	return PopRate(OutRestoredRate);
}
#endif

void FAbilityMontageRateWindowLifecycle::RestoreAndClear()
{
	if (bCaptured)
	{
		UAnimInstance* AnimInstance = WeakAnimInstance.Get();
		UAnimMontage* Montage = WeakMontage.Get();
#if WITH_DEV_AUTOMATION_TESTS
		const bool bMontageIsActive = bTestBypassMontageActiveCheck || (AnimInstance && Montage && AnimInstance->Montage_IsActive(Montage));
#else
		const bool bMontageIsActive = AnimInstance && Montage && AnimInstance->Montage_IsActive(Montage);
#endif
		if (AnimInstance && Montage && bMontageIsActive)
		{
			AnimInstance->Montage_SetPlayRate(Montage, BaselinePlayRate);
		}
	}

	RateStack.Reset();
	BaselinePlayRate = 1.0f;
	bCaptured = false;
	WeakAbility.Reset();
	WeakAnimInstance.Reset();
	WeakMontage.Reset();
	RateWindowBeginEventTag = FGameplayTag();
	RateWindowEndEventTag = FGameplayTag();
}

bool FAbilityMontageRateWindowLifecycle::IsMontageOrSequenceMatch(const UObject* OptionalObject) const
{
	const UAnimMontage* Montage = WeakMontage.Get();
	if (!Montage || !OptionalObject)
	{
		return false;
	}

	if (OptionalObject == Montage)
	{
		return true;
	}

	if (const UAnimSequenceBase* Sequence = Cast<UAnimSequenceBase>(OptionalObject))
	{
		for (const FSlotAnimationTrack& Track : Montage->SlotAnimTracks)
		{
			for (const FAnimSegment& Segment : Track.AnimTrack.AnimSegments)
			{
				if (Segment.GetAnimReference() == Sequence)
				{
					return true;
				}
			}
		}
	}

	return false;
}
