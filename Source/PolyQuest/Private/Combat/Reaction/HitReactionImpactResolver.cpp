#include "Combat/Reaction/HitReactionImpactResolver.h"

#include "GameFramework/Actor.h"

FVector FHitReactionImpactResolver::ResolveImpactDirection(const FGameplayEventData& EventData, const AActor* TargetActor)
{
	const AActor* EffectiveTarget = TargetActor ? TargetActor : Cast<AActor>(EventData.Target.Get());
	const AActor* FallbackInstigator = Cast<AActor>(EventData.Instigator.Get());
	return ResolveImpactDirectionFromContext(EventData.ContextHandle, FallbackInstigator, EffectiveTarget);
}

FVector FHitReactionImpactResolver::ResolveImpactDirectionFromContext(
	const FGameplayEffectContextHandle& ContextHandle,
	const AActor* FallbackInstigator,
	const AActor* TargetActor)
{
	if (!TargetActor)
	{
		return FVector::ZeroVector;
	}

	FVector WorldDirection = FVector::ZeroVector;
	bool bFoundValidDirection = false;

	// 1. Prioritize Effect Context HitResult ImpactNormal (points target -> attacker)
	if (ContextHandle.IsValid())
	{
		if (const FHitResult* HitResult = ContextHandle.GetHitResult())
		{
			FVector Normal = HitResult->ImpactNormal;
			if (!Normal.IsNearlyZero() && FMath::IsFinite(Normal.X) && FMath::IsFinite(Normal.Y))
			{
				Normal.Z = 0.0f;
				if (Normal.Normalize())
				{
					WorldDirection = Normal;
					bFoundValidDirection = true;
				}
			}
		}
	}

	// 2. Fallback: InstigatorLocation - TargetLocation (points target -> attacker)
	if (!bFoundValidDirection)
	{
		const AActor* InstigatorActor = (ContextHandle.IsValid() && ContextHandle.GetInstigator())
			? ContextHandle.GetInstigator()
			: FallbackInstigator;

		if (InstigatorActor && InstigatorActor != TargetActor)
		{
			FVector Offset = InstigatorActor->GetActorLocation() - TargetActor->GetActorLocation();
			if (FMath::IsFinite(Offset.X) && FMath::IsFinite(Offset.Y))
			{
				Offset.Z = 0.0f;
				if (Offset.Normalize())
				{
					WorldDirection = Offset;
					bFoundValidDirection = true;
				}
			}
		}
	}

	if (!bFoundValidDirection || !FMath::IsFinite(WorldDirection.X) || !FMath::IsFinite(WorldDirection.Y))
	{
		return FVector::ZeroVector;
	}

	// 3. Transform to Target Actor local space
	const FRotator TargetRotation = TargetActor->GetActorRotation();
	if (!FMath::IsFinite(TargetRotation.Yaw))
	{
		return FVector::ZeroVector;
	}

	const FRotator PlanarRotation(0.0f, TargetRotation.Yaw, 0.0f);
	FVector LocalDirection = PlanarRotation.UnrotateVector(WorldDirection);
	LocalDirection.Z = 0.0f;
	if (!LocalDirection.Normalize() || !FMath::IsFinite(LocalDirection.X) || !FMath::IsFinite(LocalDirection.Y))
	{
		return FVector::ZeroVector;
	}

	return LocalDirection;
}
