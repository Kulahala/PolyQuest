#include "AbilitySystem/Tasks/AbilityTask_TurnToFacing.h"

#include "GameFramework/Character.h"

UAbilityTask_TurnToFacing::UAbilityTask_TurnToFacing()
{
	bTickingTask = true;
}

UAbilityTask_TurnToFacing* UAbilityTask_TurnToFacing::TurnToFacing(
	UGameplayAbility* OwningAbility,
	ACharacter* InTargetCharacter,
	float InStartYaw,
	float InTargetYaw,
	float InTurnRateDegreesPerSecond)
{
	UAbilityTask_TurnToFacing* Task = NewAbilityTask<UAbilityTask_TurnToFacing>(OwningAbility);
	Task->TargetCharacter = InTargetCharacter;
	Task->StartYaw = InStartYaw;
	Task->TargetYaw = InTargetYaw;
	Task->TurnRateDegreesPerSecond = InTurnRateDegreesPerSecond;
	return Task;
}

void UAbilityTask_TurnToFacing::Activate()
{
	Super::Activate();

	ACharacter* Character = TargetCharacter.Get();
	if (!Character || Character->IsActorBeingDestroyed()
		|| !FMath::IsFinite(StartYaw) || !FMath::IsFinite(TargetYaw)
		|| !FMath::IsFinite(TurnRateDegreesPerSecond) || TurnRateDegreesPerSecond <= 0.0f)
	{
		BroadcastFailureAndEnd();
		return;
	}

	if (Character->HasAnyRootMotion())
	{
		BroadcastFailureAndEnd();
		return;
	}

	const float CurrentYaw = Character->GetActorRotation().Yaw;
	if (!FMath::IsFinite(CurrentYaw))
	{
		BroadcastFailureAndEnd();
		return;
	}

	const float DeltaYaw = FMath::Abs(FMath::FindDeltaAngleDegrees(CurrentYaw, TargetYaw));
	if (DeltaYaw <= 0.01f)
	{
		const FRotator CurrentRot = Character->GetActorRotation();
		Character->SetActorRotation(FRotator(CurrentRot.Pitch, TargetYaw, CurrentRot.Roll));
		BroadcastCompletionAndEnd();
		return;
	}
}

void UAbilityTask_TurnToFacing::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);

	if (bCompleted || bFailed)
	{
		return;
	}

	ACharacter* Character = TargetCharacter.Get();
	if (!Character || Character->IsActorBeingDestroyed()
		|| !FMath::IsFinite(DeltaTime) || DeltaTime <= 0.0f)
	{
		BroadcastFailureAndEnd();
		return;
	}

	if (Character->HasAnyRootMotion())
	{
		BroadcastFailureAndEnd();
		return;
	}

	const float CurrentYaw = Character->GetActorRotation().Yaw;
	if (!FMath::IsFinite(CurrentYaw))
	{
		BroadcastFailureAndEnd();
		return;
	}

	const float StepDegrees = TurnRateDegreesPerSecond * DeltaTime;
	const float RemainingAngle = FMath::Abs(FMath::FindDeltaAngleDegrees(CurrentYaw, TargetYaw));
	if (RemainingAngle <= StepDegrees || RemainingAngle <= 0.01f)
	{
		const FRotator CurrentRot = Character->GetActorRotation();
		Character->SetActorRotation(FRotator(CurrentRot.Pitch, TargetYaw, CurrentRot.Roll));
		BroadcastCompletionAndEnd();
		return;
	}

	const float NewYaw = FMath::FixedTurn(CurrentYaw, TargetYaw, StepDegrees);
	const FRotator CurrentRot = Character->GetActorRotation();
	Character->SetActorRotation(FRotator(CurrentRot.Pitch, NewYaw, CurrentRot.Roll));
}

void UAbilityTask_TurnToFacing::OnDestroy(bool AbilityIsEnding)
{
	TargetCharacter.Reset();
	Super::OnDestroy(AbilityIsEnding);
}

void UAbilityTask_TurnToFacing::BroadcastCompletionAndEnd()
{
	if (!bCompleted && !bFailed)
	{
		bCompleted = true;
		OnTurnCompleted.Broadcast();
		EndTask();
	}
}

void UAbilityTask_TurnToFacing::BroadcastFailureAndEnd()
{
	if (!bCompleted && !bFailed)
	{
		bFailed = true;
		OnTurnFailed.Broadcast();
		EndTask();
	}
}
