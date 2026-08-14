#include "Combat/Melee/MeleeHitResolver.h"

#include "ActiveGameplayEffectHandle.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Combat/Melee/CombatTeamAgent.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"

namespace
{
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

	if (ICombatTeamAgent::Execute_GetCombatTeamId(Request.SourceActor) == ICombatTeamAgent::Execute_GetCombatTeamId(TargetActor))
	{
		return false;
	}

	UAbilitySystemComponent* TargetAbilitySystemComponent = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor);
	const FGameplayTag& InvulnerableTag = GetInvulnerableTag();
	if (!TargetAbilitySystemComponent || !InvulnerableTag.IsValid() || TargetAbilitySystemComponent->HasMatchingGameplayTag(InvulnerableTag))
	{
		return false;
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

	return TargetAbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*DamageSpecHandle.Data.Get()).WasSuccessfullyApplied();
}
