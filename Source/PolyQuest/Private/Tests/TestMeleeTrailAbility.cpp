#include "Tests/TestMeleeTrailAbility.h"

#include "AbilitySystem/Tasks/AbilityTask_MeleeTraceWindow.h"
#include "Animation/Combat/AnimNotifyState_ActionWindows.h"
#include "Character/BaseCharacter.h"
#include "Combat/Melee/MeleeTraceSourceComponent.h"
#include "Tests/TestProjectileDamageGE.h"

UTestMeleeTrailAbility::UTestMeleeTrailAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
}

void UTestMeleeTrailAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ABaseCharacter* Character = Cast<ABaseCharacter>(GetAvatarActorFromActorInfo());
	UMeleeTraceSourceComponent* TraceSource = Character ? Character->GetMeleeTraceSource() : nullptr;
	TSubclassOf<UGameplayEffect> DamageEffect = TestDamageEffectClass ? TestDamageEffectClass : TSubclassOf<UGameplayEffect>(UTestProjectileDamageGE::StaticClass());

	if (!Character || !TraceSource)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	TraceWindowTask = UAbilityTask_MeleeTraceWindow::OpenMeleeTraceWindow(
		this,
		TraceSource,
		DamageEffect,
		1.0f,
		FGameplayTag(),
		0.0f,
		0.0f,
		TestTraceSourceNames);

	if (TraceWindowTask)
	{
		TraceWindowTask->ReadyForActivation();
	}
}

void UTestMeleeTrailAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (TraceWindowTask)
	{
		TraceWindowTask->EndTask();
		TraceWindowTask = nullptr;
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UTestMeleeTrailAbility::EndTestAbility()
{
	ActiveTraceNotifyState.Reset();
	K2_EndAbility();
}

void UTestMeleeTrailAbility::HandleTestTraceWindowBegin(const UAnimNotifyState_AttackTraceWindow* NotifyState)
{
	if (!NotifyState)
	{
		return;
	}

	if (TraceWindowTask)
	{
		if (ActiveTraceNotifyState.IsValid() && ActiveTraceNotifyState.Get() == NotifyState)
		{
			return;
		}

		TraceWindowTask->EndTask();
		TraceWindowTask = nullptr;
		ActiveTraceNotifyState.Reset();
	}

	ActiveTraceNotifyState = NotifyState;

	ABaseCharacter* Character = Cast<ABaseCharacter>(GetAvatarActorFromActorInfo());
	UMeleeTraceSourceComponent* TraceSource = Character ? Character->GetMeleeTraceSource() : nullptr;
	TSubclassOf<UGameplayEffect> DamageEffect = TestDamageEffectClass ? TestDamageEffectClass : TSubclassOf<UGameplayEffect>(UTestProjectileDamageGE::StaticClass());

	if (Character && TraceSource)
	{
		TraceWindowTask = UAbilityTask_MeleeTraceWindow::OpenMeleeTraceWindow(
			this,
			TraceSource,
			DamageEffect,
			1.0f,
			FGameplayTag(),
			0.0f,
			0.0f,
			NotifyState->GetTraceSourceNames());

		if (TraceWindowTask)
		{
			TraceWindowTask->ReadyForActivation();
		}
	}
}

void UTestMeleeTrailAbility::HandleTestTraceWindowEnd(const UAnimNotifyState_AttackTraceWindow* NotifyState)
{
	if (!NotifyState || NotifyState != ActiveTraceNotifyState.Get())
	{
		return;
	}

	ActiveTraceNotifyState.Reset();
	if (TraceWindowTask)
	{
		TraceWindowTask->EndTask();
		TraceWindowTask = nullptr;
	}
}
