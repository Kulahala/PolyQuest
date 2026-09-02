#include "Animation/Combat/AnimNotify_PlayerBackstabExecutionHit.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemInterface.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameplayTagContainer.h"
#include "PolyQuest.h"

void UAnimNotify_PlayerBackstabExecutionHit::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference&)
{
	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(Owner);
	if (!Owner || !AbilitySystemInterface || !AbilitySystemInterface->GetAbilitySystemComponent())
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Player Backstab Execution Hit notify could not find an ASC owner."));
		return;
	}

	const FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Backstab.Hit")), false);
	if (!EventTag.IsValid())
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Player Backstab Execution Hit notify could not send invalid event 'Event.Action.Execution.Backstab.Hit'."));
		return;
	}

	FGameplayEventData EventData;
	EventData.EventTag = EventTag;
	EventData.Instigator = Owner;
	EventData.Target = Owner;
	EventData.OptionalObject = Animation;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner, EventTag, EventData);
}

FString UAnimNotify_PlayerBackstabExecutionHit::GetNotifyName_Implementation() const
{
	return TEXT("Player Backstab Execution Hit");
}
