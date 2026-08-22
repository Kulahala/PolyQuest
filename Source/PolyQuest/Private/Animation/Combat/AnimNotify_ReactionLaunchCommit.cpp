#include "Animation/Combat/AnimNotify_ReactionLaunchCommit.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameplayTagContainer.h"
#include "PolyQuest.h"

void UAnimNotify_ReactionLaunchCommit::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference&)
{
	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(Owner);
	if (!Owner || !AbilitySystemInterface || !AbilitySystemInterface->GetAbilitySystemComponent())
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Reaction Launch Commit notify could not find an ASC owner."));
		return;
	}

	const FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Launch.Commit")), false);
	if (!EventTag.IsValid())
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Reaction Launch Commit notify could not find valid tag 'Event.Reaction.Launch.Commit'."));
		return;
	}

	FGameplayEventData EventData;
	EventData.EventTag = EventTag;
	EventData.Instigator = Owner;
	EventData.Target = Owner;
	EventData.OptionalObject = Animation;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner, EventTag, EventData);
}

FString UAnimNotify_ReactionLaunchCommit::GetNotifyName_Implementation() const
{
	return FString(TEXT("Reaction Launch Commit"));
}
