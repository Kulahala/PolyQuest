#include "AbilitySystem/Abilities/MontageRateWindowLifecycle.h"

#include "Abilities/GameplayAbility.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/Combat/AnimNotifyState_ActionWindows.h"
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

	const UObject* SourceAnimation = Payload.OptionalObject.Get();
	if (!IsMontageOrSequenceMatch(SourceAnimation))
	{
		return;
	}

	const UAnimNotifyState_MontageRateWindow* RateNotify = Cast<UAnimNotifyState_MontageRateWindow>(Payload.OptionalObject2.Get());
	if (!RateNotify)
	{
		UE_LOG(LogPolyQuest, Verbose, TEXT("RateWindowLifecycle: Begin rejected due to missing or invalid RateNotify identity in OptionalObject2."));
		return;
	}

	if (!IsValidNotifyForSource(SourceAnimation, RateNotify))
	{
		UE_LOG(LogPolyQuest, Verbose, TEXT("RateWindowLifecycle: Begin rejected because RateNotify is not declared in source animation notifies."));
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

	// Duplicate Begin check (idempotent: ignore duplicate Begin for already active window)
	const int32 ExistingIndex = ActiveWindows.IndexOfByPredicate([SourceAnimation, RateNotify](const FRateWindowActiveEntry& Entry)
	{
		return Entry.Matches(SourceAnimation, RateNotify);
	});
	if (ExistingIndex != INDEX_NONE)
	{
		return;
	}

	FRateWindowActiveEntry NewEntry;
	NewEntry.WeakSourceAnimation = SourceAnimation;
	NewEntry.WeakNotifyState = RateNotify;
	NewEntry.TargetRate = Payload.EventMagnitude;
	ActiveWindows.Add(NewEntry);

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

	const UObject* SourceAnimation = Payload.OptionalObject.Get();
	if (!IsMontageOrSequenceMatch(SourceAnimation))
	{
		return;
	}

	const UAnimNotifyState_MontageRateWindow* RateNotify = Cast<UAnimNotifyState_MontageRateWindow>(Payload.OptionalObject2.Get());
	if (!RateNotify)
	{
		return;
	}

	if (!IsValidNotifyForSource(SourceAnimation, RateNotify))
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

	const int32 FoundIndex = ActiveWindows.IndexOfByPredicate([SourceAnimation, RateNotify](const FRateWindowActiveEntry& Entry)
	{
		return Entry.Matches(SourceAnimation, RateNotify);
	});
	if (FoundIndex == INDEX_NONE)
	{
		// Unknown or already ended window; ignore
		return;
	}

	ActiveWindows.RemoveAt(FoundIndex);

	const float TargetRate = ActiveWindows.IsEmpty() ? BaselinePlayRate : ActiveWindows.Last().TargetRate;
	AnimInstance->Montage_SetPlayRate(Montage, TargetRate);
}

bool FAbilityMontageRateWindowLifecycle::IsValidNotifyForSource(const UObject* SourceAnimation, const UAnimNotifyState_MontageRateWindow* RateNotify) const
{
	if (!SourceAnimation || !RateNotify)
	{
		return false;
	}

	const UAnimSequenceBase* AnimSeq = Cast<UAnimSequenceBase>(SourceAnimation);
	if (!AnimSeq)
	{
		return false;
	}

	for (const FAnimNotifyEvent& NotifyEvent : AnimSeq->Notifies)
	{
		if (NotifyEvent.NotifyStateClass == RateNotify)
		{
			return true;
		}
	}

	return false;
}

#if WITH_DEV_AUTOMATION_TESTS
bool FAbilityMontageRateWindowLifecycle::TestApplyBegin(
	const UObject* InSource,
	const UAnimNotifyState_MontageRateWindow* InNotify,
	float NewRate,
	float& OutAppliedRate)
{
	if (!bCaptured || !InSource || !InNotify || !FMath::IsFinite(NewRate) || NewRate <= 0.0f)
	{
		return false;
	}

	if (!IsMontageOrSequenceMatch(InSource))
	{
		return false;
	}

	if (!IsValidNotifyForSource(InSource, InNotify))
	{
		return false;
	}

	const int32 ExistingIndex = ActiveWindows.IndexOfByPredicate([InSource, InNotify](const FRateWindowActiveEntry& Entry)
	{
		return Entry.Matches(InSource, InNotify);
	});
	if (ExistingIndex != INDEX_NONE)
	{
		return false;
	}

	FRateWindowActiveEntry NewEntry;
	NewEntry.WeakSourceAnimation = InSource;
	NewEntry.WeakNotifyState = InNotify;
	NewEntry.TargetRate = NewRate;
	ActiveWindows.Add(NewEntry);

	OutAppliedRate = NewRate;
	return true;
}

bool FAbilityMontageRateWindowLifecycle::TestApplyEnd(
	const UObject* InSource,
	const UAnimNotifyState_MontageRateWindow* InNotify,
	float& OutRestoredRate)
{
	if (!bCaptured || !InSource || !InNotify)
	{
		return false;
	}

	const int32 FoundIndex = ActiveWindows.IndexOfByPredicate([InSource, InNotify](const FRateWindowActiveEntry& Entry)
	{
		return Entry.Matches(InSource, InNotify);
	});
	if (FoundIndex == INDEX_NONE)
	{
		return false;
	}

	ActiveWindows.RemoveAt(FoundIndex);
	OutRestoredRate = ActiveWindows.IsEmpty() ? BaselinePlayRate : ActiveWindows.Last().TargetRate;
	return true;
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

	ActiveWindows.Reset();
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
