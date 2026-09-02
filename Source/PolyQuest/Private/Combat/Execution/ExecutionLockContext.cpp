#include "Combat/Execution/ExecutionLockContext.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "GameFramework/Actor.h"

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
	bVictimAccepted = false;
	bActive = true;
	bReleaseSent = false;
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

void UExecutionLockContext::MarkReleaseSent()
{
	bReleaseSent = true;
}

void UExecutionLockContext::InvalidateSession()
{
	bActive = false;
	bVictimAccepted = false;
}
