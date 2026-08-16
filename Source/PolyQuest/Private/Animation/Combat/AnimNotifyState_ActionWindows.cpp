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
	void SendGameplayEvent(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FName EventTagName, const TCHAR* NotifyName)
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

void UAnimNotifyState_ActionDodgeCancelWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float, const FAnimNotifyEventReference&)
{
	SendGameplayEvent(MeshComp, Animation, TEXT("Event.Action.CancelWindow.Dodge.Begin"), TEXT("Dodge cancel window"));
}

void UAnimNotifyState_ActionDodgeCancelWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference&)
{
	SendGameplayEvent(MeshComp, Animation, TEXT("Event.Action.CancelWindow.Dodge.End"), TEXT("Dodge cancel window"));
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

void UAnimNotify_ChargedAttackHoldReady::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference&)
{
	SendGameplayEvent(MeshComp, Animation, TEXT("Event.Attack.Charged.HoldReady"), TEXT("Charged attack hold-ready notify"));
}

FString UAnimNotify_ChargedAttackHoldReady::GetNotifyName_Implementation() const
{
	return FString("Charged Attack Hold Ready");
}

void UAnimNotifyState_AttackTraceWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float, const FAnimNotifyEventReference&)
{
	SendGameplayEvent(MeshComp, Animation, TEXT("Event.Attack.TraceWindow.Begin"), TEXT("Attack trace window"));
}

void UAnimNotifyState_AttackTraceWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference&)
{
	SendGameplayEvent(MeshComp, Animation, TEXT("Event.Attack.TraceWindow.End"), TEXT("Attack trace window"));
}

FString UAnimNotifyState_AttackTraceWindow::GetNotifyName_Implementation() const
{
	return FString("Attack Trace Window");
}

void UAnimNotifyState_ComboInputWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float, const FAnimNotifyEventReference&)
{
	SendGameplayEvent(MeshComp, Animation, TEXT("Event.Attack.Light.Combo.InputWindow.Begin"), TEXT("Combo input window"));
}

void UAnimNotifyState_ComboInputWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference&)
{
	SendGameplayEvent(MeshComp, Animation, TEXT("Event.Attack.Light.Combo.InputWindow.End"), TEXT("Combo input window"));
}

FString UAnimNotifyState_ComboInputWindow::GetNotifyName_Implementation() const
{
	return FString("Combo Input Window");
}

void UAnimNotifyState_ComboBranchWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float, const FAnimNotifyEventReference&)
{
	SendGameplayEvent(MeshComp, Animation, TEXT("Event.Attack.Light.Combo.BranchWindow.Begin"), TEXT("Combo branch window"));
}

void UAnimNotifyState_ComboBranchWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference&)
{
	SendGameplayEvent(MeshComp, Animation, TEXT("Event.Attack.Light.Combo.BranchWindow.End"), TEXT("Combo branch window"));
}

FString UAnimNotifyState_ComboBranchWindow::GetNotifyName_Implementation() const
{
	return FString("Combo Branch Window");
}

void UAnimNotifyState_ParryWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float, const FAnimNotifyEventReference&)
{
	SendGameplayEvent(MeshComp, Animation, TEXT("Event.Defense.Parry.Window.Begin"), TEXT("Parry window"));
}

void UAnimNotifyState_ParryWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference&)
{
	SendGameplayEvent(MeshComp, Animation, TEXT("Event.Defense.Parry.Window.End"), TEXT("Parry window"));
}

FString UAnimNotifyState_ParryWindow::GetNotifyName_Implementation() const
{
	return FString("Parry Window");
}
