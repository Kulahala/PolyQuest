#include "Combat/Execution/ExecutionLockContext.h"
#include "AbilitySystem/Abilities/EnemyVictimExecutionAbility.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "GameFramework/Actor.h"

FExecutionHitScopeGuard::FExecutionHitScopeGuard(
	UExecutionLockContext* InContext,
	const UObject* InSourceObject,
	const AActor* InSourceActor,
	const UAbilitySystemComponent* InSourceASC,
	const AActor* InTargetActor,
	const UAbilitySystemComponent* InTargetASC)
	: Context(InContext)
{
	if (InContext)
	{
		bScopeValid = InContext->BeginHitScope(InSourceObject, InSourceActor, InSourceASC, InTargetActor, InTargetASC);
	}
}

FExecutionHitScopeGuard::FExecutionHitScopeGuard(
	UExecutionLockContext* InContext,
	const AActor* InSourceActor,
	const AActor* InTargetActor)
	: Context(InContext)
{
	if (InContext)
	{
		bScopeValid = InContext->BeginHitScope(InSourceActor, InTargetActor);
	}
}

FExecutionHitScopeGuard::~FExecutionHitScopeGuard()
{
	if (bScopeValid)
	{
		if (UExecutionLockContext* Ctx = Context.Get())
		{
			if (Ctx->IsResolving())
			{
				if (bGESuccess)
				{
					Ctx->CompleteHitScope(bLethal, bPendingConfirmed);
				}
				else
				{
					Ctx->AbortHitScope();
				}
			}
		}
	}
}

UExecutionLockContext::UExecutionLockContext()
{
}

void UExecutionLockContext::InitializeSession(
	UGameplayAbility* InSourceAbility,
	AActor* InSourceActor,
	UAbilitySystemComponent* InSourceASC,
	AActor* InTargetActor,
	const FGameplayTag& InRequestTag,
	uint32 InActivationToken)
{
	SourceAbility = InSourceAbility;
	SourceActor = InSourceActor;
	SourceASC = InSourceASC;
	TargetActor = InTargetActor;
	RequestTag = InRequestTag;
	SourceActivationToken = InActivationToken;
	VictimAbility = nullptr;
	VictimASC = nullptr;
	HitState = EExecutionSessionHitState::Ready;
	bVictimAccepted = false;
	bActive = true;
	bReleaseSent = false;
	bReleaseWasCancelled = false;
}

bool UExecutionLockContext::AcceptVictim(
	UGameplayAbility* InVictimAbility,
	AActor* InVictimActor,
	UAbilitySystemComponent* InVictimASC,
	const FGameplayTag& InRequestTag,
	uint32 InActivationToken)
{
	if (!bActive || bVictimAccepted)
	{
		return false;
	}

	if (!InVictimAbility || !InVictimActor || !InVictimASC)
	{
		return false;
	}

	if (InRequestTag != RequestTag || InActivationToken != SourceActivationToken)
	{
		return false;
	}

	if (TargetActor.Get() != InVictimActor)
	{
		return false;
	}

	VictimAbility = InVictimAbility;
	VictimASC = InVictimASC;
	bVictimAccepted = true;
	return true;
}

bool UExecutionLockContext::IsCurrent(const UGameplayAbility* InAbility, uint32 InToken) const
{
	if (!bActive || !InAbility)
	{
		return false;
	}

	if (InAbility == SourceAbility.Get())
	{
		return SourceActivationToken == InToken;
	}

	if (InAbility == VictimAbility.Get())
	{
		return bVictimAccepted;
	}

	return false;
}

bool UExecutionLockContext::IsHitAuthorized(
	const UObject* InSourceObject,
	const AActor* InSourceActor,
	const UAbilitySystemComponent* InSourceASC,
	const AActor* InTargetActor,
	const UAbilitySystemComponent* InTargetASC) const
{
	if (!bActive || !bVictimAccepted)
	{
		return false;
	}

	if (HitState != EExecutionSessionHitState::Ready && HitState != EExecutionSessionHitState::Resolving)
	{
		return false;
	}

	if (!InSourceObject || !InSourceActor || !InSourceASC || !InTargetActor || !InTargetASC)
	{
		return false;
	}

	if (InSourceObject != SourceAbility.Get())
	{
		return false;
	}

	if (SourceActor.Get() != InSourceActor || SourceASC.Get() != InSourceASC)
	{
		return false;
	}

	if (TargetActor.Get() != InTargetActor || VictimASC.Get() != InTargetASC)
	{
		return false;
	}

	if (!SourceAbility.IsValid() || !SourceAbility->IsActive())
	{
		return false;
	}

	if (!VictimAbility.IsValid() || !VictimAbility->IsActive())
	{
		return false;
	}

	return true;
}

bool UExecutionLockContext::BeginHitScope(
	const UObject* InSourceObject,
	const AActor* InSourceActor,
	const UAbilitySystemComponent* InSourceASC,
	const AActor* InTargetActor,
	const UAbilitySystemComponent* InTargetASC)
{
	if (!IsHitAuthorized(InSourceObject, InSourceActor, InSourceASC, InTargetActor, InTargetASC))
	{
		return false;
	}

	if (HitState != EExecutionSessionHitState::Ready)
	{
		return false;
	}

	if (UEnemyVictimExecutionAbility* Victim = Cast<UEnemyVictimExecutionAbility>(VictimAbility.Get()))
	{
		if (!Victim->BeginAuthorizedHitScope())
		{
			HitState = EExecutionSessionHitState::Failed;
			return false;
		}
	}

	HitState = EExecutionSessionHitState::Resolving;
	return true;
}

bool UExecutionLockContext::BeginHitScope(const AActor* InSourceActor, const AActor* InTargetActor)
{
	return BeginHitScope(
		SourceAbility.Get(),
		InSourceActor,
		SourceASC.Get(),
		InTargetActor,
		VictimASC.Get());
}

void UExecutionLockContext::CompleteHitScope(bool bLethal, bool bPendingConfirmed)
{
	if (HitState != EExecutionSessionHitState::Resolving)
	{
		return;
	}

	if (UEnemyVictimExecutionAbility* Victim = Cast<UEnemyVictimExecutionAbility>(VictimAbility.Get()))
	{
		Victim->EndAuthorizedHitScope();
	}

	if (bLethal && bPendingConfirmed)
	{
		HitState = EExecutionSessionHitState::DeathPending;
	}
	else if (!bLethal)
	{
		HitState = EExecutionSessionHitState::NonLethal;
	}
	else
	{
		HitState = EExecutionSessionHitState::Failed;
	}
}

void UExecutionLockContext::AbortHitScope()
{
	if (HitState == EExecutionSessionHitState::Resolving)
	{
		if (UEnemyVictimExecutionAbility* Victim = Cast<UEnemyVictimExecutionAbility>(VictimAbility.Get()))
		{
			Victim->EndAuthorizedHitScope();
		}
		HitState = EExecutionSessionHitState::Failed;
	}
}

bool UExecutionLockContext::BeginFinalization(const UGameplayAbility* InVictimAbility)
{
	if (!bActive || InVictimAbility != VictimAbility.Get())
	{
		return false;
	}

	if (HitState != EExecutionSessionHitState::DeathPending)
	{
		return false;
	}

	HitState = EExecutionSessionHitState::Finalizing;
	return true;
}

void UExecutionLockContext::CompleteFinalization(const UGameplayAbility* InVictimAbility)
{
	if (!bActive || InVictimAbility != VictimAbility.Get() || HitState != EExecutionSessionHitState::Finalizing)
	{
		return;
	}

	HitState = EExecutionSessionHitState::Finalized;
}

void UExecutionLockContext::AbortFinalization(const UGameplayAbility* InVictimAbility)
{
	if (!bActive || InVictimAbility != VictimAbility.Get() || HitState != EExecutionSessionHitState::Finalizing)
	{
		return;
	}

	HitState = EExecutionSessionHitState::DeathPending;
}

void UExecutionLockContext::MarkReleaseSent(bool bWasCancelled)
{
	bReleaseSent = true;
	bReleaseWasCancelled = bWasCancelled;
}

void UExecutionLockContext::InvalidateSession()
{
	if (HitState == EExecutionSessionHitState::Resolving)
	{
		AbortHitScope();
	}

	bActive = false;
	bVictimAccepted = false;
}
