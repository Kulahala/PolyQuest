#include "Animation/Combat/AnimNotify_PlayerBowAction.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameplayTagContainer.h"
#include "PolyQuest.h"

namespace
{
	void SendBowGameplayEvent(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FName EventTagName, const TCHAR* NotifyName)
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
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner, EventTag, EventData);
	}
}

void UAnimNotify_PlayerBowDrawReady::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference&)
{
	SendBowGameplayEvent(MeshComp, Animation, TEXT("Event.Attack.Bow.DrawReady"), TEXT("Player Bow Draw Ready"));
}

FString UAnimNotify_PlayerBowDrawReady::GetNotifyName_Implementation() const
{
	return FString("Player Bow Draw Ready");
}

void UAnimNotify_PlayerBowRelease::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference&)
{
	SendBowGameplayEvent(MeshComp, Animation, TEXT("Event.Attack.Bow.Release"), TEXT("Player Bow Release"));
}

FString UAnimNotify_PlayerBowRelease::GetNotifyName_Implementation() const
{
	return FString("Player Bow Release");
}
