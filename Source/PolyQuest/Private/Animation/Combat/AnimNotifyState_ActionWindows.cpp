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

	void SendGameplayEventWithMagnitude(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FName EventTagName, float EventMagnitude, const TCHAR* NotifyName)
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
		EventData.EventMagnitude = EventMagnitude;
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

void UAnimNotifyState_MontageRateWindow::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float, const FAnimNotifyEventReference&)
{
	SendGameplayEventWithMagnitude(MeshComp, Animation, TEXT("Event.Action.RateWindow.Begin"), FMath::Max(RateMultiplier, 0.01f), TEXT("Montage rate window"));
}

void UAnimNotifyState_MontageRateWindow::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference&)
{
	SendGameplayEvent(MeshComp, Animation, TEXT("Event.Action.RateWindow.End"), TEXT("Montage rate window"));
}

FString UAnimNotifyState_MontageRateWindow::GetNotifyName_Implementation() const
{
	return FString("Montage Rate Window");
}
