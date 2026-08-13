#include "Public/Animation/Combat/AnimNotify_Attack.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Components/SkeletalMeshComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayTagContainer.h"
#include "PolyQuest.h"

namespace
{
	void SendAttackGameplayEvent(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FName EventTagName, const TCHAR* NotifyName)
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

void UAnimNotify_LightAttackHit::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference&)
{
	SendAttackGameplayEvent(MeshComp, Animation, TEXT("Event.Attack.Light.Hit"), TEXT("Light attack hit notify"));
}

FString UAnimNotify_LightAttackHit::GetNotifyName_Implementation() const
{
	return FString("Light Attack Hit");
}

void UAnimNotify_ChargedAttackHoldReady::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference&)
{
	SendAttackGameplayEvent(MeshComp, Animation, TEXT("Event.Attack.Charged.HoldReady"), TEXT("Charged attack hold-ready notify"));
}

FString UAnimNotify_ChargedAttackHoldReady::GetNotifyName_Implementation() const
{
	return FString("Charged Attack Hold Ready");
}

void UAnimNotify_ChargedAttackHit::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference&)
{
	SendAttackGameplayEvent(MeshComp, Animation, TEXT("Event.Attack.Charged.Hit"), TEXT("Charged attack hit notify"));
}

FString UAnimNotify_ChargedAttackHit::GetNotifyName_Implementation() const
{
	return FString("Charged Attack Hit");
}

void UAnimNotify_SprintAttackHit::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference&)
{
	SendAttackGameplayEvent(MeshComp, Animation, TEXT("Event.Attack.Sprint.Hit"), TEXT("Sprint attack hit notify"));
}

FString UAnimNotify_SprintAttackHit::GetNotifyName_Implementation() const
{
	return FString("Sprint Attack Hit");
}
