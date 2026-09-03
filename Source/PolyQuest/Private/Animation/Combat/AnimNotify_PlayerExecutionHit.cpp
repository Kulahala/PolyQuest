#include "Animation/Combat/AnimNotify_PlayerExecutionHit.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemInterface.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameplayTagContainer.h"
#include "PolyQuest.h"

void UAnimNotify_PlayerExecutionHit::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference&)
{
	if (!MeshComp || !Animation)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Player Execution Hit notify received invalid mesh component or animation."));
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(Owner);
	if (!Owner || !AbilitySystemInterface || !AbilitySystemInterface->GetAbilitySystemComponent())
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Player Execution Hit notify could not find an ASC owner."));
		return;
	}

	const FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Hit")), false);
	if (!EventTag.IsValid())
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Player Execution Hit notify could not send invalid event 'Event.Action.Execution.Hit'."));
		return;
	}

	FGameplayEventData EventData;
	EventData.EventTag = EventTag;
	EventData.Instigator = Owner;
	EventData.Target = Owner;
	EventData.OptionalObject = Animation;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner, EventTag, EventData);
}

FString UAnimNotify_PlayerExecutionHit::GetNotifyName_Implementation() const
{
	return TEXT("Player Execution Hit");
}
