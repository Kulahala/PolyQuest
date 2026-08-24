#include "Tests/TestMeleeTrailAbility.h"

#include "AbilitySystem/Tasks/AbilityTask_MeleeTraceWindow.h"
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
		0.0f);

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
	K2_EndAbility();
}
