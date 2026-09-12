#include "Combat/Projectile/CombatProjectileHitResolver.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "ActiveGameplayEffectHandle.h"
#include "Character/Player/PlayerCharacter.h"
#include "Combat/CombatImpactEffectContext.h"
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

	FVector SanitizeIncomingDirection(const FVector& Direction)
	{
		if (!FMath::IsFinite(Direction.X) || !FMath::IsFinite(Direction.Y) || !FMath::IsFinite(Direction.Z))
		{
			return FVector::ZeroVector;
		}
		const FVector Planar(Direction.X, Direction.Y, 0.0f);
		if (Planar.IsNearlyZero())
		{
			return FVector::ZeroVector;
		}
		return Planar.GetSafeNormal();
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

	const FVector ValidatedIncomingDirection = SanitizeIncomingDirection(Request.WorldIncomingDirection);

	if (APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(TargetActor))
	{
		if (PlayerCharacter->TryResolveIncomingDefense(SourceActor, Request.GuardStaminaDamage, Request.HitResult, false, &ValidatedIncomingDirection))
		{
			// Player defense (Guard) successfully consumed the contact.
			return true;
		}
	}

	FCombatImpactEffectContext* TypedContext = new FCombatImpactEffectContext();
	const FGameplayEffectContextHandle BaseContext = SourceASC->MakeEffectContext();
	if (BaseContext.IsValid())
	{
		*static_cast<FGameplayEffectContext*>(TypedContext) = *BaseContext.Get();
	}
	TypedContext->SetWorldIncomingDirection(ValidatedIncomingDirection);

	FGameplayEffectContextHandle EffectContext(TypedContext);
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
