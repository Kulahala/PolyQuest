#include "Combat/Projectile/CombatProjectileHitResolver.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "ActiveGameplayEffectHandle.h"
#include "Character/Player/PlayerCharacter.h"
#include "Combat/Projectile/CombatProjectileTargeting.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"

namespace
{
	const FGameplayTag& GetProjectileDeadTag()
	{
		static const FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
		return DeadTag;
	}

	const FGameplayTag& GetProjectileInvulnerableTag()
	{
		static const FGameplayTag InvulnerableTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Invulnerable")), false);
		return InvulnerableTag;
	}
}

bool FCombatProjectileHitResolver::TryResolveHit(const FCombatProjectileHitRequest& Request)
{
	AActor* SourceActor = Request.SourceActor.Get();
	UAbilitySystemComponent* SourceASC = Request.SourceAbilitySystemComponent.Get();
	AActor* TargetActor = Request.HitResult.GetActor();

	if (!SourceActor || !TargetActor || TargetActor == SourceActor || !SourceASC || !Request.DamageGameplayEffectClass)
	{
		return false;
	}

	if (!SourceActor->GetClass()->ImplementsInterface(UCombatTeamAgent::StaticClass())
		|| !TargetActor->GetClass()->ImplementsInterface(UCombatTeamAgent::StaticClass()))
	{
		return false;
	}

	const FGameplayTag SourceTeamTag = FCombatProjectileTargeting::ResolveTeamTag(SourceActor);
	const FGameplayTag TargetTeamTag = FCombatProjectileTargeting::ResolveTeamTag(TargetActor);
	if (!SourceTeamTag.IsValid() || !TargetTeamTag.IsValid() || SourceTeamTag.MatchesTagExact(TargetTeamTag))
	{
		return false;
	}

	UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor);
	const FGameplayTag& DeadTag = GetProjectileDeadTag();
	const FGameplayTag& InvulnerableTag = GetProjectileInvulnerableTag();

	if (!TargetASC || !DeadTag.IsValid() || !InvulnerableTag.IsValid()
		|| SourceASC->HasMatchingGameplayTag(DeadTag)
		|| TargetASC->HasMatchingGameplayTag(DeadTag)
		|| TargetASC->HasMatchingGameplayTag(InvulnerableTag))
	{
		return false;
	}

	if (APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(TargetActor))
	{
		if (PlayerCharacter->TryResolveIncomingDefense(SourceActor, Request.GuardStaminaDamage))
		{
			// Player defense (Guard / Parry) successfully consumed the contact.
			return true;
		}
	}

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddSourceObject(Request.SourceObject ? Request.SourceObject : SourceActor);
	EffectContext.AddHitResult(Request.HitResult, true);

	const FGameplayEffectSpecHandle DamageSpecHandle = SourceASC->MakeOutgoingSpec(
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

	const FActiveGameplayEffectHandle AppliedHandle = TargetASC->ApplyGameplayEffectSpecToSelf(*DamageSpecHandle.Data.Get());
	if (AppliedHandle.WasSuccessfullyApplied())
	{
		return true;
	}

	// Instant GameplayEffects execute immediately without returning a persistent active handle.
	if (DamageSpecHandle.Data->Def && DamageSpecHandle.Data->Def->DurationPolicy == EGameplayEffectDurationType::Instant)
	{
		return true;
	}

	return false;
}
