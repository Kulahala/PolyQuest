#include "AI/StateTree/EnemyStateTreeTasks.h"

#include "AI/EnemyAIController.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeLinker.h"

FEnemyStateTreeTask_BeginAlert::FEnemyStateTreeTask_BeginAlert()
{
	bShouldCallTick = false;
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

bool FEnemyStateTreeTask_BeginAlert::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(EnemyAIControllerHandle);
	return true;
}

EStateTreeRunStatus FEnemyStateTreeTask_BeginAlert::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult&) const
{
	AEnemyAIController& EnemyAIController = Context.GetExternalData(EnemyAIControllerHandle);
	EnemyAIController.BeginAlert();
	return EStateTreeRunStatus::Succeeded;
}

FEnemyStateTreeTask_RequestMeleeAttack::FEnemyStateTreeTask_RequestMeleeAttack()
{
	bShouldCallTick = true;
}

bool FEnemyStateTreeTask_RequestMeleeAttack::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(EnemyAIControllerHandle);
	return true;
}

EStateTreeRunStatus FEnemyStateTreeTask_RequestMeleeAttack::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult&) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.bObservedAttacking = false;

	AEnemyAIController& EnemyAIController = Context.GetExternalData(EnemyAIControllerHandle);
	if (!EnemyAIController.HasValidCombatTarget() || !EnemyAIController.IsCombatTargetInMeleeRange())
	{
		return EStateTreeRunStatus::Failed;
	}

	if (EnemyAIController.IsEnemyMeleeAttackActive())
	{
		InstanceData.bObservedAttacking = true;
		return EStateTreeRunStatus::Running;
	}

	if (EnemyAIController.IsMeleeAttackOnCooldown())
	{
		return EStateTreeRunStatus::Running;
	}

	if (!EnemyAIController.TryRequestMeleeAttack() || !EnemyAIController.IsEnemyMeleeAttackActive())
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.bObservedAttacking = true;
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FEnemyStateTreeTask_RequestMeleeAttack::Tick(FStateTreeExecutionContext& Context, const float) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AEnemyAIController& EnemyAIController = Context.GetExternalData(EnemyAIControllerHandle);
	if (!EnemyAIController.HasValidCombatTarget() || !EnemyAIController.IsCombatTargetInMeleeRange())
	{
		return EStateTreeRunStatus::Failed;
	}

	if (EnemyAIController.IsEnemyMeleeAttackActive())
	{
		InstanceData.bObservedAttacking = true;
		return EStateTreeRunStatus::Running;
	}

	if (InstanceData.bObservedAttacking)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	if (EnemyAIController.IsMeleeAttackOnCooldown())
	{
		return EStateTreeRunStatus::Running;
	}

	if (!EnemyAIController.TryRequestMeleeAttack() || !EnemyAIController.IsEnemyMeleeAttackActive())
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.bObservedAttacking = true;
	return EStateTreeRunStatus::Running;
}
