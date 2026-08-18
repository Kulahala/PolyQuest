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

bool FEnemyStateTreeCondition_IsAttackOnCooldown::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(EnemyAIControllerHandle);
	return true;
}

bool FEnemyStateTreeCondition_IsAttackOnCooldown::TestCondition(FStateTreeExecutionContext& Context) const
{
	const AEnemyAIController& EnemyAIController = Context.GetExternalData(EnemyAIControllerHandle);
	return EnemyAIController.IsMeleeAttackOnCooldown();
}

bool FEnemyStateTreeCondition_CanRequestReposition::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(EnemyAIControllerHandle);
	return true;
}

bool FEnemyStateTreeCondition_CanRequestReposition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const AEnemyAIController& EnemyAIController = Context.GetExternalData(EnemyAIControllerHandle);
	return EnemyAIController.CanRequestCooldownReposition();
}

bool FEnemyStateTreeCondition_IsAttackReady::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(EnemyAIControllerHandle);
	return true;
}

bool FEnemyStateTreeCondition_IsAttackReady::TestCondition(FStateTreeExecutionContext& Context) const
{
	const AEnemyAIController& EnemyAIController = Context.GetExternalData(EnemyAIControllerHandle);
	return !EnemyAIController.IsMeleeAttackOnCooldown();
}

bool FEnemyStateTreeCondition_IsTargetOutsideMeleeRange::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(EnemyAIControllerHandle);
	return true;
}

bool FEnemyStateTreeCondition_IsTargetOutsideMeleeRange::TestCondition(FStateTreeExecutionContext& Context) const
{
	const AEnemyAIController& EnemyAIController = Context.GetExternalData(EnemyAIControllerHandle);
	return !EnemyAIController.IsCombatTargetInMeleeRange();
}

bool FEnemyStateTreeCondition_HasPendingMeleeAttack::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(EnemyAIControllerHandle);
	return true;
}

bool FEnemyStateTreeCondition_HasPendingMeleeAttack::TestCondition(FStateTreeExecutionContext& Context) const
{
	const AEnemyAIController& EnemyAIController = Context.GetExternalData(EnemyAIControllerHandle);
	return EnemyAIController.HasPendingAttackProfile();
}

bool FEnemyStateTreeCondition_IsPendingAttackInRange::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(EnemyAIControllerHandle);
	return true;
}

bool FEnemyStateTreeCondition_IsPendingAttackInRange::TestCondition(FStateTreeExecutionContext& Context) const
{
	const AEnemyAIController& EnemyAIController = Context.GetExternalData(EnemyAIControllerHandle);
	return EnemyAIController.IsPendingAttackInRange();
}

bool FEnemyStateTreeCondition_IsPendingAttackOutOfRange::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(EnemyAIControllerHandle);
	return true;
}

bool FEnemyStateTreeCondition_IsPendingAttackOutOfRange::TestCondition(FStateTreeExecutionContext& Context) const
{
	const AEnemyAIController& EnemyAIController = Context.GetExternalData(EnemyAIControllerHandle);
	return EnemyAIController.HasPendingAttackProfile() && !EnemyAIController.IsPendingAttackInRange();
}
