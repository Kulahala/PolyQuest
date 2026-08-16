#include "Combat/Melee/MeleeHitResolver.h"

#include "ActiveGameplayEffectHandle.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"

#include "Character/Player/PlayerCharacter.h"
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
		|| TargetAbilitySystemComponent->HasMatchingGameplayTag(DeadTag)
		|| TargetAbilitySystemComponent->HasMatchingGameplayTag(InvulnerableTag))
	{
		return false;
	}

	if (APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(TargetActor))
	{
		if (PlayerCharacter->TryResolveIncomingDefense(Request.SourceActor, Request.GuardStaminaDamage))
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

	return TargetAbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*DamageSpecHandle.Data.Get()).WasSuccessfullyApplied();
}
