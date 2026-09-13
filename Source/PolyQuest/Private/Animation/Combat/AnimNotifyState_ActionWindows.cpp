#include "Animation/Combat/AnimNotifyState_ActionWindows.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Animation/ActiveMontageInstanceScope.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifyLibrary.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameplayTagContainer.h"
#include "PolyQuest.h"

namespace
{
	bool HasQueuedCancelWindowReachedEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference, int32 InstanceID)
	{
		const FAnimNotifyEvent* Event = EventReference.GetNotify();
		UAnimInstance* AnimInstance = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
		FAnimMontageInstance* Instance = AnimInstance && InstanceID != INDEX_NONE
			? AnimInstance->GetMontageInstanceForID(InstanceID) : nullptr;
		const float SampledTrackTime = EventReference.GetCurrentAnimationTime();
		if (!Event || !Instance || !Instance->Montage || !FMath::IsFinite(SampledTrackTime))
		{
			return false;
		}

		// HandleEvents records montage track time, including for notifications gathered from its segments.
		// Never read the current playback position: a queued callback may run after pause, jump or replay.
		float SourceTime = SampledTrackTime;
		bool bPlayingBackwards = Instance->GetPlayRate() * Instance->Montage->RateScale < 0.0f;
		if (Animation != Instance->Montage)
		{
			const FAnimSegment* SourceSegment = nullptr;
			for (const FSlotAnimationTrack& Track : Instance->Montage->SlotAnimTracks)
			{
				for (const FAnimSegment& Segment : Track.AnimTrack.AnimSegments)
				{
					if (Segment.GetAnimReference() == Animation && SampledTrackTime >= Segment.StartPos
						&& (!SourceSegment || Segment.StartPos > SourceSegment->StartPos))
					{
						SourceSegment = &Segment;
					}
				}
			}
			if (!SourceSegment || SourceSegment->AnimEndTime <= SourceSegment->AnimStartTime)
			{
				return false;
			}
			// Match engine extraction at the segment edge even when the sampling step overshoots it.
			SourceTime = SourceSegment->ConvertTrackPosToAnimPos(FMath::Clamp(SampledTrackTime,
				SourceSegment->StartPos, SourceSegment->StartPos + SourceSegment->GetLength()));
			bPlayingBackwards ^= SourceSegment->GetValidPlayRate() < 0.0f;
		}
		return FMath::IsFinite(SourceTime) && (bPlayingBackwards
			? SourceTime <= Event->GetTriggerTime() : SourceTime >= Event->GetEndTriggerTime());
	}

	void SendGameplayEvent(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FName EventTagName, const TCHAR* NotifyName, const UObject* OptionalObject2 = nullptr)
	{
		AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
		IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(Owner);
		if (!Owner || !AbilitySystemInterface || !AbilitySystemInterface->GetAbilitySystemComponent())
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("%s could not find an ASC owner."), NotifyName);
			return;
		}

		const FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(EventTagName, false);
		if (!EventTag.IsValid())
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("%s could not send invalid event '%s'."), NotifyName, *EventTagName.ToString());
			return;
		}

		FGameplayEventData EventData;
		EventData.EventTag = EventTag;
		EventData.Instigator = Owner;
		EventData.Target = Owner;
		EventData.OptionalObject = Animation;
		EventData.OptionalObject2 = OptionalObject2;
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner, EventTag, EventData);
	}

	void SendGameplayEventWithMagnitude(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FName EventTagName, float EventMagnitude, const TCHAR* NotifyName, const UObject* OptionalObject2 = nullptr)
	{
		AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
		IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(Owner);
		if (!Owner || !AbilitySystemInterface || !AbilitySystemInterface->GetAbilitySystemComponent())
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("%s could not find an ASC owner."), NotifyName);
			return;
		}

		const FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(EventTagName, false);
		if (!EventTag.IsValid())
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("%s could not send invalid event '%s'."), NotifyName, *EventTagName.ToString());
			return;
		}

		FGameplayEventData EventData;
		EventData.EventTag = EventTag;
		EventData.Instigator = Owner;
		EventData.Target = Owner;
		EventData.OptionalObject = Animation;
		EventData.OptionalObject2 = OptionalObject2;
		EventData.EventMagnitude = EventMagnitude;
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner, EventTag, EventData);
	}

	void SendRateWindowEvent(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FName EventTagName,
		float EventMagnitude,
		const TCHAR* NotifyName,
		const UObject* OptionalObject2,
		int32 MontageInstanceID)
	{
		AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
		IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(Owner);
		if (!Owner || !AbilitySystemInterface || !AbilitySystemInterface->GetAbilitySystemComponent())
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("%s could not find an ASC owner."), NotifyName);
			return;
		}

		const FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(EventTagName, false);
		if (!EventTag.IsValid())
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("%s could not send invalid event '%s'."), NotifyName, *EventTagName.ToString());
			return;
		}

		FGameplayEventData EventData;
		EventData.EventTag = EventTag;
		EventData.Instigator = Owner;
		EventData.Target = Owner;
		EventData.OptionalObject = Animation;
		EventData.OptionalObject2 = OptionalObject2;
		EventData.EventMagnitude = EventMagnitude;

		UAnimInstance* AnimInst = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
		FGameplayAbilityTargetData_MontageRateWindowSource* SourceData = new FGameplayAbilityTargetData_MontageRateWindowSource();
		SourceData->AnimInstance = AnimInst;
		SourceData->MontageInstanceID = MontageInstanceID;
		EventData.TargetData.Add(SourceData);

		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner, EventTag, EventData);
	}

	void SendCancelWindowEvent(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FName EventTagName,
		const TCHAR* NotifyName,
		const UObject* OptionalObject2,
		int32 MontageInstanceID,
		bool bReachedEnd = false,
		bool bNativeReachedEnd = false)
	{
		AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
		IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(Owner);
		if (!Owner || !AbilitySystemInterface || !AbilitySystemInterface->GetAbilitySystemComponent())
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("%s could not find an ASC owner."), NotifyName);
			return;
		}

		const FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(EventTagName, false);
		if (!EventTag.IsValid())
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("%s could not send invalid event '%s'."), NotifyName, *EventTagName.ToString());
			return;
		}

		FGameplayEventData EventData;
		EventData.EventTag = EventTag;
		EventData.Instigator = Owner;
		EventData.Target = Owner;
		EventData.OptionalObject = Animation;
		EventData.OptionalObject2 = OptionalObject2;

		UAnimInstance* AnimInst = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
		FGameplayAbilityTargetData_MontageRateWindowSource* SourceData = new FGameplayAbilityTargetData_MontageRateWindowSource();
		SourceData->AnimInstance = AnimInst;
		SourceData->MontageInstanceID = MontageInstanceID;
		SourceData->bReachedEnd = bReachedEnd;
		SourceData->bNativeReachedEnd = bNativeReachedEnd;
		EventData.TargetData.Add(SourceData);

		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner, EventTag, EventData);
	}
}

void UAnimNotifyState_ActionDodgeCancelWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float, const FAnimNotifyEventReference& EventReference)
{
	int32 InstanceID = INDEX_NONE;
	if (const UE::Anim::FAnimNotifyMontageInstanceContext* Context = EventReference.GetContextData<UE::Anim::FAnimNotifyMontageInstanceContext>())
	{
		InstanceID = Context->MontageInstanceID;
	}
	SendCancelWindowEvent(MeshComp, Animation, TEXT("Event.Action.CancelWindow.Dodge.Begin"), TEXT("Dodge cancel window"), this, InstanceID, false);
}

void UAnimNotifyState_ActionDodgeCancelWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	int32 InstanceID = INDEX_NONE;
	if (const UE::Anim::FAnimNotifyMontageInstanceContext* Context = EventReference.GetContextData<UE::Anim::FAnimNotifyMontageInstanceContext>())
	{
		InstanceID = Context->MontageInstanceID;
	}
	const bool bNativeReachedEnd = UAnimNotifyLibrary::NotifyStateReachedEnd(EventReference);
	// UE 5.8 shares TickRecord context data: another notify (including HoldReady) can set the raw end flag.
	const bool bReachedEnd = bNativeReachedEnd && HasQueuedCancelWindowReachedEnd(MeshComp, Animation, EventReference, InstanceID);
	SendCancelWindowEvent(MeshComp, Animation, TEXT("Event.Action.CancelWindow.Dodge.End"), TEXT("Dodge cancel window"), this, InstanceID, bReachedEnd, bNativeReachedEnd);
}

void UAnimNotifyState_ActionDodgeCancelWindow::BranchingPointNotifyBegin(FBranchingPointNotifyPayload& BranchingPointPayload)
{
	SendCancelWindowEvent(BranchingPointPayload.SkelMeshComponent, BranchingPointPayload.SequenceAsset, TEXT("Event.Action.CancelWindow.Dodge.Begin"), TEXT("Dodge cancel window (BranchingPoint)"), this, BranchingPointPayload.MontageInstanceID, false);
}

void UAnimNotifyState_ActionDodgeCancelWindow::BranchingPointNotifyEnd(FBranchingPointNotifyPayload& BranchingPointPayload)
{
	SendCancelWindowEvent(BranchingPointPayload.SkelMeshComponent, BranchingPointPayload.SequenceAsset, TEXT("Event.Action.CancelWindow.Dodge.End"), TEXT("Dodge cancel window (BranchingPoint)"), this, BranchingPointPayload.MontageInstanceID, BranchingPointPayload.bReachedEnd, BranchingPointPayload.bReachedEnd);
}

FString UAnimNotifyState_ActionDodgeCancelWindow::GetNotifyName_Implementation() const
{
	return FString("Dodge Cancel Window");
}

void UAnimNotifyState_DodgeInvulnerability::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float, const FAnimNotifyEventReference&)
{
	SendGameplayEvent(MeshComp, Animation, TEXT("Event.Dodge.Invulnerability.Begin"), TEXT("Dodge invulnerability"));
}

void UAnimNotifyState_DodgeInvulnerability::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference&)
{
	SendGameplayEvent(MeshComp, Animation, TEXT("Event.Dodge.Invulnerability.End"), TEXT("Dodge invulnerability"));
}

FString UAnimNotifyState_DodgeInvulnerability::GetNotifyName_Implementation() const
{
	return FString("Dodge Invulnerability");
}

void UAnimNotify_PlayerChargedAttackHoldReady::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference&)
{
	SendGameplayEvent(MeshComp, Animation, TEXT("Event.Attack.Charged.HoldReady"), TEXT("Charged attack hold-ready notify"));
}

FString UAnimNotify_PlayerChargedAttackHoldReady::GetNotifyName_Implementation() const
{
	return FString("Player Charged Attack Hold Ready");
}

void UAnimNotifyState_AttackTraceWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float, const FAnimNotifyEventReference&)
{
	SendGameplayEvent(MeshComp, Animation, TEXT("Event.Attack.TraceWindow.Begin"), TEXT("Attack trace window"), this);
}

void UAnimNotifyState_AttackTraceWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference&)
{
	SendGameplayEvent(MeshComp, Animation, TEXT("Event.Attack.TraceWindow.End"), TEXT("Attack trace window"), this);
}

FString UAnimNotifyState_AttackTraceWindow::GetNotifyName_Implementation() const
{
	return FString("Attack Trace Window");
}

void UAnimNotifyState_PlayerComboInputWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float, const FAnimNotifyEventReference&)
{
	SendGameplayEvent(MeshComp, Animation, TEXT("Event.Attack.Light.Combo.InputWindow.Begin"), TEXT("Combo input window"));
}

void UAnimNotifyState_PlayerComboInputWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference&)
{
	SendGameplayEvent(MeshComp, Animation, TEXT("Event.Attack.Light.Combo.InputWindow.End"), TEXT("Combo input window"));
}

FString UAnimNotifyState_PlayerComboInputWindow::GetNotifyName_Implementation() const
{
	return FString("Player Combo Input Window");
}

void UAnimNotifyState_PlayerComboBranchWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float, const FAnimNotifyEventReference&)
{
	SendGameplayEvent(MeshComp, Animation, TEXT("Event.Attack.Light.Combo.BranchWindow.Begin"), TEXT("Combo branch window"));
}

void UAnimNotifyState_PlayerComboBranchWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference&)
{
	SendGameplayEvent(MeshComp, Animation, TEXT("Event.Attack.Light.Combo.BranchWindow.End"), TEXT("Combo branch window"));
}

FString UAnimNotifyState_PlayerComboBranchWindow::GetNotifyName_Implementation() const
{
	return FString("Player Combo Branch Window");
}

void UAnimNotifyState_PlayerParryWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float, const FAnimNotifyEventReference&)
{
	SendGameplayEvent(MeshComp, Animation, TEXT("Event.Defense.Parry.Window.Begin"), TEXT("Parry window"));
}

void UAnimNotifyState_PlayerParryWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference&)
{
	SendGameplayEvent(MeshComp, Animation, TEXT("Event.Defense.Parry.Window.End"), TEXT("Parry window"));
}

FString UAnimNotifyState_PlayerParryWindow::GetNotifyName_Implementation() const
{
	return FString("Player Parry Window");
}

void UAnimNotifyState_EnemyHyperArmor::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float, const FAnimNotifyEventReference&)
{
	SendGameplayEvent(MeshComp, Animation, TEXT("Event.Attack.HyperArmor.Begin"), TEXT("Enemy Hyper Armor window"));
}

void UAnimNotifyState_EnemyHyperArmor::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference&)
{
	SendGameplayEvent(MeshComp, Animation, TEXT("Event.Attack.HyperArmor.End"), TEXT("Enemy Hyper Armor window"));
}

FString UAnimNotifyState_EnemyHyperArmor::GetNotifyName_Implementation() const
{
	return FString("Enemy Hyper Armor");
}

void UAnimNotifyState_MontageRateWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float, const FAnimNotifyEventReference& EventReference)
{
	int32 InstanceID = INDEX_NONE;
	if (const UE::Anim::FAnimNotifyMontageInstanceContext* Context = EventReference.GetContextData<UE::Anim::FAnimNotifyMontageInstanceContext>())
	{
		InstanceID = Context->MontageInstanceID;
	}
	SendRateWindowEvent(MeshComp, Animation, TEXT("Event.Action.RateWindow.Begin"), FMath::Max(RateMultiplier, 0.01f), TEXT("Montage rate window"), this, InstanceID);
}

void UAnimNotifyState_MontageRateWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	int32 InstanceID = INDEX_NONE;
	if (const UE::Anim::FAnimNotifyMontageInstanceContext* Context = EventReference.GetContextData<UE::Anim::FAnimNotifyMontageInstanceContext>())
	{
		InstanceID = Context->MontageInstanceID;
	}
	SendRateWindowEvent(MeshComp, Animation, TEXT("Event.Action.RateWindow.End"), 0.0f, TEXT("Montage rate window"), this, InstanceID);
}

void UAnimNotifyState_MontageRateWindow::BranchingPointNotifyBegin(FBranchingPointNotifyPayload& BranchingPointPayload)
{
	SendRateWindowEvent(BranchingPointPayload.SkelMeshComponent, BranchingPointPayload.SequenceAsset, TEXT("Event.Action.RateWindow.Begin"), FMath::Max(RateMultiplier, 0.01f), TEXT("Montage rate window (BranchingPoint)"), this, BranchingPointPayload.MontageInstanceID);
}

void UAnimNotifyState_MontageRateWindow::BranchingPointNotifyEnd(FBranchingPointNotifyPayload& BranchingPointPayload)
{
	SendRateWindowEvent(BranchingPointPayload.SkelMeshComponent, BranchingPointPayload.SequenceAsset, TEXT("Event.Action.RateWindow.End"), 0.0f, TEXT("Montage rate window (BranchingPoint)"), this, BranchingPointPayload.MontageInstanceID);
}

FString UAnimNotifyState_MontageRateWindow::GetNotifyName_Implementation() const
{
	return FString("Montage Rate Window");
}
