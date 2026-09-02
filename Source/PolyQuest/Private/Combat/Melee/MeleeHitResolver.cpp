#include "Combat/Melee/MeleeHitResolver.h"

#include "ActiveGameplayEffectHandle.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"

#include "AbilitySystem/CharacterAttributeSet.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Character/Player/PlayerCharacter.h"
#include "Combat/Execution/ExecutionLockContext.h"
#include "Combat/Melee/CombatTeamAgent.h"

namespace
{
	const FGameplayTag& GetMeleeDeadTag()
	{
		static const FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
		return DeadTag;
	}

	const FGameplayTag& GetInvulnerableTag()
	{
		static const FGameplayTag InvulnerableTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Invulnerable")), false);
		return InvulnerableTag;
	}
}

bool FMeleeHitResolver::TryResolveHit(const FMeleeHitRequest& Request)
{
	AActor* TargetActor = Request.HitResult.GetActor();
	if (!Request.SourceActor || !TargetActor || TargetActor == Request.SourceActor || !Request.SourceAbilitySystemComponent || !Request.DamageGameplayEffectClass)
	{
		return false;
	}

	if (!Request.SourceActor->GetClass()->ImplementsInterface(UCombatTeamAgent::StaticClass())
		|| !TargetActor->GetClass()->ImplementsInterface(UCombatTeamAgent::StaticClass()))
	{
		return false;
	}

	const FGameplayTag SourceTeamTag = ICombatTeamAgent::Execute_GetCombatTeamTag(Request.SourceActor);
	const FGameplayTag TargetTeamTag = ICombatTeamAgent::Execute_GetCombatTeamTag(TargetActor);
	if (!SourceTeamTag.IsValid() || !TargetTeamTag.IsValid() || SourceTeamTag.MatchesTagExact(TargetTeamTag))
	{
		return false;
	}

	UAbilitySystemComponent* TargetAbilitySystemComponent = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor);
	const FGameplayTag& DeadTag = GetMeleeDeadTag();
	const FGameplayTag& InvulnerableTag = GetInvulnerableTag();
	if (!TargetAbilitySystemComponent || !DeadTag.IsValid() || !InvulnerableTag.IsValid()
		|| Request.SourceAbilitySystemComponent->HasMatchingGameplayTag(DeadTag)
		|| TargetAbilitySystemComponent->HasMatchingGameplayTag(DeadTag))
	{
		return false;
	}

	if (Request.ExecutionContext)
	{
		const bool bIsAuthorizedExecutionHit = Request.ExecutionContext->IsHitAuthorized(
			Request.SourceObject,
			Request.SourceActor,
			Request.SourceAbilitySystemComponent,
			TargetActor,
			TargetAbilitySystemComponent);

		if (!bIsAuthorizedExecutionHit)
		{
			return false;
		}
	}

	if (TargetAbilitySystemComponent->HasMatchingGameplayTag(InvulnerableTag))
	{
		if (!Request.ExecutionContext)
		{
			return false;
		}
	}

	if (APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(TargetActor))
	{
		if (PlayerCharacter->TryResolveIncomingDefense(Request.SourceActor, Request.GuardStaminaDamage, Request.HitResult, true))
		{
			// A successful Parry or Guard is terminal for this trace contact, just like
			// a successfully applied damage spec, so the trace task records the target.
			return true;
		}
	}

	FGameplayEffectContextHandle EffectContext = Request.SourceAbilitySystemComponent->MakeEffectContext();
	EffectContext.AddSourceObject(Request.SourceObject ? Request.SourceObject : Request.SourceActor);
	EffectContext.AddHitResult(Request.HitResult, true);

	const FGameplayEffectSpecHandle DamageSpecHandle = Request.SourceAbilitySystemComponent->MakeOutgoingSpec(
		Request.DamageGameplayEffectClass,
		Request.AbilityLevel,
		EffectContext);
	if (!DamageSpecHandle.IsValid() || !DamageSpecHandle.Data.IsValid())
	{
		return false;
	}

	if (Request.SetByCallerMagnitudeTag.IsValid())
	{
		DamageSpecHandle.Data->SetSetByCallerMagnitude(Request.SetByCallerMagnitudeTag, Request.SetByCallerMagnitude);
	}

	for (const TPair<FGameplayTag, float>& SetByCallerMagnitude : Request.SetByCallerMagnitudes)
	{
		if (SetByCallerMagnitude.Key.IsValid())
		{
			DamageSpecHandle.Data->SetSetByCallerMagnitude(SetByCallerMagnitude.Key, SetByCallerMagnitude.Value);
		}
	}

	UExecutionLockContext* ExecContext = const_cast<UExecutionLockContext*>(Request.ExecutionContext);
	TOptional<FExecutionHitScopeGuard> HitScopeGuard;
	if (ExecContext)
	{
		HitScopeGuard.Emplace(
			ExecContext,
			Request.SourceObject,
			Request.SourceActor,
			Request.SourceAbilitySystemComponent,
			TargetActor,
			TargetAbilitySystemComponent);
		if (!HitScopeGuard->IsScopeValid())
		{
			return false;
		}
	}

	const FActiveGameplayEffectHandle AppliedHandle = TargetAbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*DamageSpecHandle.Data.Get());
	const bool bAppliedSuccessfully = AppliedHandle.WasSuccessfullyApplied();

	if (HitScopeGuard.IsSet())
	{
		HitScopeGuard->SetGESuccess(bAppliedSuccessfully);
		if (bAppliedSuccessfully)
		{
			const float RemainingHealth = TargetAbilitySystemComponent->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute());
			const bool bLethal = (RemainingHealth <= 0.0f);
			AEnemyCharacter* TargetEnemy = Cast<AEnemyCharacter>(TargetActor);
			const bool bPendingConfirmed = (TargetEnemy && TargetEnemy->IsDeathPending()) || (ExecContext && ExecContext->IsDeathPending());
			HitScopeGuard->SetLethal(bLethal);
			HitScopeGuard->SetPendingConfirmed(bPendingConfirmed);

			if (bLethal && !bPendingConfirmed)
			{
				if (TargetEnemy && !TargetEnemy->IsDead())
				{
					TargetEnemy->SetDeadState();
				}
				HitScopeGuard->SetGESuccess(false);
				return false;
			}
		}
	}

	return bAppliedSuccessfully;
}
