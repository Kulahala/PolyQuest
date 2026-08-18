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
	if (EnemyAIController.IsEnemyStunned() || EnemyAIController.IsEnemyHitReactionActive())
	{
		return EStateTreeRunStatus::Running;
	}

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
	if (EnemyAIController.IsEnemyStunned() || EnemyAIController.IsEnemyHitReactionActive())
	{
		return EStateTreeRunStatus::Running;
	}

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

FEnemyStateTreeTask_RepositionDuringCooldown::FEnemyStateTreeTask_RepositionDuringCooldown()
{
	bShouldCallTick = true;
}

bool FEnemyStateTreeTask_RepositionDuringCooldown::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(EnemyAIControllerHandle);
	return true;
}

EStateTreeRunStatus FEnemyStateTreeTask_RepositionDuringCooldown::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult&) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.bIssuedMoveRequest = false;

	AEnemyAIController& EnemyAIController = Context.GetExternalData(EnemyAIControllerHandle);
	if (EnemyAIController.IsEnemyStunned()
		|| EnemyAIController.IsEnemyHitReactionActive()
		|| EnemyAIController.IsEnemyMeleeAttackActive()
		|| !EnemyAIController.HasValidCombatTarget()
		|| !EnemyAIController.HasValidAttackSet()
		|| !EnemyAIController.HasValidAIProfile()
		|| EnemyAIController.IsExceedingLeash())
	{
		return EStateTreeRunStatus::Failed;
	}

	if (!EnemyAIController.IsMeleeAttackOnCooldown())
	{
		// Cooldown is already over, no need to reposition
		return EStateTreeRunStatus::Succeeded;
	}

	if (!EnemyAIController.CanRequestCooldownReposition())
	{
		// Only if temporarily delayed by the minimum request interval (NextAllowedRepositionTime),
		// return Succeeded to cycle through Wait without breaking the cooldown loop.
		if (EnemyAIController.IsRepositionTemporarilyIntervalGated())
		{
			return EStateTreeRunStatus::Succeeded;
		}

		// Otherwise it is an unrecoverable/concurrent failure -> Fail closed
		return EStateTreeRunStatus::Failed;
	}

	if (!EnemyAIController.TryRequestCooldownReposition())
	{
		// Move request calculation or immediate failure:
		// Returning Succeeded allows transitioning to Wait for the next retry interval instead of prematurely breaking Combat.
		return EStateTreeRunStatus::Succeeded;
	}

	InstanceData.bIssuedMoveRequest = true;
	return EnemyAIController.IsRepositioning() ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Succeeded;
}

EStateTreeRunStatus FEnemyStateTreeTask_RepositionDuringCooldown::Tick(FStateTreeExecutionContext& Context, const float) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AEnemyAIController& EnemyAIController = Context.GetExternalData(EnemyAIControllerHandle);

	if (EnemyAIController.IsEnemyStunned()
		|| EnemyAIController.IsEnemyHitReactionActive()
		|| EnemyAIController.IsEnemyMeleeAttackActive()
		|| !EnemyAIController.HasValidCombatTarget()
		|| !EnemyAIController.HasValidAttackSet()
		|| !EnemyAIController.HasValidAIProfile()
		|| EnemyAIController.IsExceedingLeash())
	{
		EnemyAIController.StopCooldownReposition(false);
		return EStateTreeRunStatus::Failed;
	}

	if (!EnemyAIController.IsMeleeAttackOnCooldown())
	{
		// Cooldown expired during reposition
		EnemyAIController.StopCooldownReposition(false);
		return EStateTreeRunStatus::Succeeded;
	}

	if (InstanceData.bIssuedMoveRequest)
	{
		if (EnemyAIController.IsRepositioning())
		{
			return EStateTreeRunStatus::Running;
		}

		// Move completed (either success or path following end)
		return EStateTreeRunStatus::Succeeded;
	}

	return EStateTreeRunStatus::Failed;
}

void FEnemyStateTreeTask_RepositionDuringCooldown::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult&) const
{
	AEnemyAIController& EnemyAIController = Context.GetExternalData(EnemyAIControllerHandle);
	if (EnemyAIController.IsRepositioning())
	{
		EnemyAIController.StopCooldownReposition(false);
	}
}

FEnemyStateTreeTask_PrepareMeleeAttack::FEnemyStateTreeTask_PrepareMeleeAttack()
{
	bShouldCallTick = false;
	bShouldCopyBoundPropertiesOnTick = false;
	bShouldCopyBoundPropertiesOnExitState = false;
}

bool FEnemyStateTreeTask_PrepareMeleeAttack::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(EnemyAIControllerHandle);
	return true;
}

EStateTreeRunStatus FEnemyStateTreeTask_PrepareMeleeAttack::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult&) const
{
	AEnemyAIController& EnemyAIController = Context.GetExternalData(EnemyAIControllerHandle);
	if (EnemyAIController.IsEnemyStunned() || EnemyAIController.IsEnemyHitReactionActive() || EnemyAIController.IsEnemyMeleeAttackActive() || EnemyAIController.IsMeleeAttackOnCooldown())
	{
		EnemyAIController.ClearPendingAttackProfile();
		return EStateTreeRunStatus::Failed;
	}

	if (!EnemyAIController.HasValidCombatTarget() || !EnemyAIController.IsCombatTargetInMeleeRange() || EnemyAIController.IsExceedingLeash())
	{
		EnemyAIController.ClearPendingAttackProfile();
		return EStateTreeRunStatus::Failed;
	}

	return EnemyAIController.PreparePendingAttackProfile() ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
}

FEnemyStateTreeTask_ApproachSelectedMeleeAttack::FEnemyStateTreeTask_ApproachSelectedMeleeAttack()
{
	bShouldCallTick = true;
}

bool FEnemyStateTreeTask_ApproachSelectedMeleeAttack::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(EnemyAIControllerHandle);
	return true;
}

EStateTreeRunStatus FEnemyStateTreeTask_ApproachSelectedMeleeAttack::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult&) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.bIssuedMoveRequest = false;

	AEnemyAIController& EnemyAIController = Context.GetExternalData(EnemyAIControllerHandle);
	if (EnemyAIController.IsEnemyStunned()
		|| EnemyAIController.IsEnemyHitReactionActive()
		|| EnemyAIController.IsEnemyMeleeAttackActive()
		|| !EnemyAIController.HasValidCombatTarget()
		|| !EnemyAIController.HasValidAttackSet()
		|| !EnemyAIController.HasValidAIProfile()
		|| EnemyAIController.IsExceedingLeash()
		|| !EnemyAIController.IsCombatTargetInMeleeRange()
		|| !EnemyAIController.HasPendingAttackProfile())
	{
		EnemyAIController.ClearPendingAttackProfile();
		return EStateTreeRunStatus::Failed;
	}

	if (EnemyAIController.IsPendingAttackInRange())
	{
		// Target is already within the selected attack's range
		return EStateTreeRunStatus::Succeeded;
	}

	if (!EnemyAIController.CanRequestApproach())
	{
		EnemyAIController.ClearPendingAttackProfile();
		return EStateTreeRunStatus::Failed;
	}

	if (!EnemyAIController.TryRequestApproach())
	{
		// Pathfinding/navigation failed
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.bIssuedMoveRequest = true;
	return EnemyAIController.IsApproaching() ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Succeeded;
}

EStateTreeRunStatus FEnemyStateTreeTask_ApproachSelectedMeleeAttack::Tick(FStateTreeExecutionContext& Context, const float) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AEnemyAIController& EnemyAIController = Context.GetExternalData(EnemyAIControllerHandle);

	if (EnemyAIController.IsEnemyStunned()
		|| EnemyAIController.IsEnemyHitReactionActive()
		|| EnemyAIController.IsEnemyMeleeAttackActive()
		|| !EnemyAIController.HasValidCombatTarget()
		|| !EnemyAIController.HasValidAttackSet()
		|| !EnemyAIController.HasValidAIProfile()
		|| EnemyAIController.IsExceedingLeash()
		|| !EnemyAIController.IsCombatTargetInMeleeRange()
		|| !EnemyAIController.HasPendingAttackProfile())
	{
		EnemyAIController.ClearPendingAttackProfile();
		return EStateTreeRunStatus::Failed;
	}

	if (EnemyAIController.HasApproachTimedOut())
	{
		EnemyAIController.ClearPendingAttackProfile();
		return EStateTreeRunStatus::Failed;
	}

	if (EnemyAIController.IsPendingAttackInRange())
	{
		EnemyAIController.StopApproach(false);
		return EStateTreeRunStatus::Succeeded;
	}

	if (InstanceData.bIssuedMoveRequest)
	{
		if (EnemyAIController.IsApproaching())
		{
			return EStateTreeRunStatus::Running;
		}

		// Move completed: check if within attack range
		if (EnemyAIController.IsPendingAttackInRange())
		{
			return EStateTreeRunStatus::Succeeded;
		}

		// Otherwise navigation completed without reaching attack range -> fail and retry
		EnemyAIController.ClearPendingAttackProfile();
		return EStateTreeRunStatus::Failed;
	}

	EnemyAIController.ClearPendingAttackProfile();
	return EStateTreeRunStatus::Failed;
}

void FEnemyStateTreeTask_ApproachSelectedMeleeAttack::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult&) const
{
	AEnemyAIController& EnemyAIController = Context.GetExternalData(EnemyAIControllerHandle);
	if (EnemyAIController.IsApproaching())
	{
		EnemyAIController.StopApproach(false);
	}
}
