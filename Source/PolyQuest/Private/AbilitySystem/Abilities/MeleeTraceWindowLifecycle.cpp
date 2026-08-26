#include "AbilitySystem/Abilities/MeleeTraceWindowLifecycle.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/Tasks/AbilityTask_MeleeTraceWindow.h"
#include "Animation/Combat/AnimNotifyState_ActionWindows.h"
#include "Character/BaseCharacter.h"

void FMeleeTraceWindowLifecycle::OpenOrKeepScalar(
	UGameplayAbility* OwningAbility,
	TObjectPtr<UAbilityTask_MeleeTraceWindow>& InOutTask,
	TSubclassOf<UGameplayEffect> DamageGameplayEffectClass,
	float AbilityLevel,
	FGameplayTag SetByCallerMagnitudeTag,
	float SetByCallerMagnitude,
	float GuardStaminaDamage,
	const TArray<FName>& TraceSourceNames)
{
	if (!OwningAbility)
	{
		return;
	}

	if (InOutTask && !InOutTask->IsTraceWindowOpen())
	{
		InOutTask = nullptr;
	}

	if (InOutTask)
	{
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = OwningAbility->GetCurrentActorInfo();
	ABaseCharacter* Character = (ActorInfo && ActorInfo->AvatarActor.IsValid())
		? Cast<ABaseCharacter>(ActorInfo->AvatarActor.Get())
		: nullptr;
	InOutTask = Character
		? UAbilityTask_MeleeTraceWindow::OpenMeleeTraceWindow(
			OwningAbility,
			Character->GetMeleeTraceSource(),
			DamageGameplayEffectClass,
			AbilityLevel,
			SetByCallerMagnitudeTag,
			SetByCallerMagnitude,
			GuardStaminaDamage,
			TraceSourceNames)
		: nullptr;

	if (InOutTask)
	{
		InOutTask->ReadyForActivation();
		if (!InOutTask->IsTraceWindowOpen())
		{
			InOutTask = nullptr;
		}
	}
}

void FMeleeTraceWindowLifecycle::OpenOrKeepMagnitudes(
	UGameplayAbility* OwningAbility,
	TObjectPtr<UAbilityTask_MeleeTraceWindow>& InOutTask,
	TSubclassOf<UGameplayEffect> DamageGameplayEffectClass,
	float AbilityLevel,
	const TMap<FGameplayTag, float>& SetByCallerMagnitudes,
	const TArray<FName>& TraceSourceNames)
{
	if (!OwningAbility)
	{
		return;
	}

	if (InOutTask && !InOutTask->IsTraceWindowOpen())
	{
		InOutTask = nullptr;
	}

	if (InOutTask)
	{
		return;
	}

	const FGameplayAbilityActorInfo* ActorInfo = OwningAbility->GetCurrentActorInfo();
	ABaseCharacter* Character = (ActorInfo && ActorInfo->AvatarActor.IsValid())
		? Cast<ABaseCharacter>(ActorInfo->AvatarActor.Get())
		: nullptr;
	InOutTask = Character
		? UAbilityTask_MeleeTraceWindow::OpenMeleeTraceWindow(
			OwningAbility,
			Character->GetMeleeTraceSource(),
			DamageGameplayEffectClass,
			AbilityLevel,
			SetByCallerMagnitudes,
			TraceSourceNames)
		: nullptr;

	if (InOutTask)
	{
		InOutTask->ReadyForActivation();
		if (!InOutTask->IsTraceWindowOpen())
		{
			InOutTask = nullptr;
		}
	}
}

void FMeleeTraceWindowLifecycle::CloseAndClear(
	TObjectPtr<UAbilityTask_MeleeTraceWindow>& InOutTask,
	TWeakObjectPtr<const UAnimNotifyState_AttackTraceWindow>& InOutActiveNotifyState)
{
	InOutActiveNotifyState.Reset();
	if (InOutTask)
	{
		InOutTask->EndTask();
		InOutTask = nullptr;
	}
}
