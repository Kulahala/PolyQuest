#include "Public/Animation/Combat/AnimNotify_Attack.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Components/SkeletalMeshComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayTagContainer.h"
#include "PolyQuest.h"

void UAnimNotify_LightAttackHit::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase*, const FAnimNotifyEventReference&)
{
	AActor* Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(Owner);
	if (!Owner || !AbilitySystemInterface || !AbilitySystemInterface->GetAbilitySystemComponent())
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Light attack hit notify could not find an ASC owner."));
		return;
	}

	const FGameplayTag HitEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Attack.Light.Hit")), false);
	if (!HitEventTag.IsValid())
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Light attack hit notify could not send an invalid Event.Attack.Light.Hit tag."));
		return;
	}

	FGameplayEventData EventData;
	EventData.EventTag = HitEventTag;
	EventData.Instigator = Owner;
	EventData.Target = Owner;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Owner, HitEventTag, EventData);
}

FString UAnimNotify_LightAttackHit::GetNotifyName_Implementation() const
{
	return FString("Light Attack Hit");
}
