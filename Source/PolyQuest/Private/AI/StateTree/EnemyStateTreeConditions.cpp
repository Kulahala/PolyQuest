#include "AI/StateTree/EnemyStateTreeConditions.h"

#include "AI/EnemyAIController.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeLinker.h"

bool FEnemyStateTreeCondition_HasValidTarget::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(EnemyAIControllerHandle);
	return true;
}

bool FEnemyStateTreeCondition_HasValidTarget::TestCondition(FStateTreeExecutionContext& Context) const
{
	const AEnemyAIController& EnemyAIController = Context.GetExternalData(EnemyAIControllerHandle);
	return EnemyAIController.HasValidCombatTarget();
}

bool FEnemyStateTreeCondition_IsTargetInMeleeRange::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(EnemyAIControllerHandle);
	return true;
}

bool FEnemyStateTreeCondition_IsTargetInMeleeRange::TestCondition(FStateTreeExecutionContext& Context) const
{
	const AEnemyAIController& EnemyAIController = Context.GetExternalData(EnemyAIControllerHandle);
	return EnemyAIController.IsCombatTargetInMeleeRange();
}
