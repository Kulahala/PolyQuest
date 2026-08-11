#include "Animation/Combat/AnimNotifyState_ActionWindows.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameplayTagContainer.h"
#include "PolyQuest.h"

namespace
{
	void SendGameplayEvent(USkeletalMeshComponent* MeshComp, const FName EventTagName, const TCHAR* NotifyName)
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
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner, EventTag, EventData);
	}
}

void UAnimNotifyState_ActionDodgeCancelWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase*, float, const FAnimNotifyEventReference&)
{
	SendGameplayEvent(MeshComp, TEXT("Event.Action.CancelWindow.Dodge.Begin"), TEXT("Dodge cancel window"));
}

void UAnimNotifyState_ActionDodgeCancelWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase*, const FAnimNotifyEventReference&)
{
	SendGameplayEvent(MeshComp, TEXT("Event.Action.CancelWindow.Dodge.End"), TEXT("Dodge cancel window"));
}

FString UAnimNotifyState_ActionDodgeCancelWindow::GetNotifyName_Implementation() const
{
	return FString("Dodge Cancel Window");
}

void UAnimNotifyState_DodgeInvulnerability::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase*, float, const FAnimNotifyEventReference&)
{
	SendGameplayEvent(MeshComp, TEXT("Event.Dodge.Invulnerability.Begin"), TEXT("Dodge invulnerability"));
}

void UAnimNotifyState_DodgeInvulnerability::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase*, const FAnimNotifyEventReference&)
{
	SendGameplayEvent(MeshComp, TEXT("Event.Dodge.Invulnerability.End"), TEXT("Dodge invulnerability"));
}

FString UAnimNotifyState_DodgeInvulnerability::GetNotifyName_Implementation() const
{
	return FString("Dodge Invulnerability");
}
