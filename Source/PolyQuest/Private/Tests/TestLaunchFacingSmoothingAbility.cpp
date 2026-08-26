#include "Tests/TestLaunchFacingSmoothingAbility.h"

#include "AbilitySystem/Tasks/AbilityTask_TurnToFacing.h"
#include "GameFramework/Character.h"

UTestLaunchFacingSmoothingAbility::UTestLaunchFacingSmoothingAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
}

void UTestLaunchFacingSmoothingAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (bStartTaskOnActivate)
	{
		StartTurnTask(TestStartYaw, TestTargetYaw, TestTurnRate);
	}
}

void UTestLaunchFacingSmoothingAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (TurnTask)
	{
		TurnTask->EndTask();
		TurnTask = nullptr;
	}

	SmoothingState.Reset();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UTestLaunchFacingSmoothingAbility::StartTurnTask(float InStartYaw, float InTargetYaw, float InTurnRate)
{
	if (TurnTask)
	{
		TurnTask->EndTask();
		TurnTask = nullptr;
	}

	bTaskCompleted = false;
	bTaskFailed = false;

	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Character)
	{
		return;
	}

	TurnTask = UAbilityTask_TurnToFacing::TurnToFacing(this, Character, InStartYaw, InTargetYaw, InTurnRate);
	if (TurnTask)
	{
		TurnTask->OnTurnCompleted.AddUObject(this, &UTestLaunchFacingSmoothingAbility::HandleTurnCompleted);
		TurnTask->OnTurnFailed.AddUObject(this, &UTestLaunchFacingSmoothingAbility::HandleTurnFailed);
		TurnTask->ReadyForActivation();
	}
}

void UTestLaunchFacingSmoothingAbility::EndTurnTask()
{
	if (TurnTask)
	{
		TurnTask->EndTask();
		TurnTask = nullptr;
	}
}

void UTestLaunchFacingSmoothingAbility::ResetCounters()
{
	CompletedCount = 0;
	FailedCount = 0;
	bTaskCompleted = false;
	bTaskFailed = false;
}

void UTestLaunchFacingSmoothingAbility::HandleTurnCompleted()
{
	bTaskCompleted = true;
	++CompletedCount;
	SmoothingState.MarkTurnCompleted();
}

void UTestLaunchFacingSmoothingAbility::HandleTurnFailed()
{
	bTaskFailed = true;
	++FailedCount;
}
