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

	// 1. Priority 1: Finite, non-zero planar relative line (InstigatorLocation - TargetLocation) -> points Target -> Attacker
	const AActor* InstigatorActor = (ContextHandle.IsValid() && ContextHandle.GetInstigator())
		? ContextHandle.GetInstigator()
		: FallbackInstigator;

	if (InstigatorActor && InstigatorActor != TargetActor)
	{
		const FVector InstigatorLoc = InstigatorActor->GetActorLocation();
		const FVector TargetLoc = TargetActor->GetActorLocation();
		if (FMath::IsFinite(InstigatorLoc.X) && FMath::IsFinite(InstigatorLoc.Y) &&
			FMath::IsFinite(TargetLoc.X) && FMath::IsFinite(TargetLoc.Y))
		{
			FVector Offset = InstigatorLoc - TargetLoc;
			Offset.Z = 0.0f;
			if (!Offset.IsNearlyZero() && Offset.Normalize())
			{
				WorldDirection = Offset;
				bFoundValidDirection = true;
			}
		}
	}

	// 2. Priority 2 (Fallback): HitResult ImpactNormal (points Target -> Attacker)
	if (!bFoundValidDirection && ContextHandle.IsValid())
	{
		if (const FHitResult* HitResult = ContextHandle.GetHitResult())
		{
			FVector Normal = HitResult->ImpactNormal;
			if (!Normal.IsNearlyZero() && FMath::IsFinite(Normal.X) && FMath::IsFinite(Normal.Y))
			{
				Normal.Z = 0.0f;
				if (!Normal.IsNearlyZero() && Normal.Normalize())
				{
					WorldDirection = Normal;
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

bool FHitReactionImpactResolver::TryBuildLaunchFacingAndVelocity(
	const FVector& LocalAttackerDirection,
	float ImpactReferenceYaw,
	float HorizontalSpeed,
	float VerticalSpeed,
	float& OutFacingYaw,
	FVector& OutLaunchVelocity)
{
	OutFacingYaw = 0.0f;
	OutLaunchVelocity = FVector::ZeroVector;

	if (!FMath::IsFinite(LocalAttackerDirection.X) || !FMath::IsFinite(LocalAttackerDirection.Y))
	{
		return false;
	}

	FVector LocalPlanarDir(LocalAttackerDirection.X, LocalAttackerDirection.Y, 0.0f);
	if (LocalPlanarDir.IsNearlyZero() || !LocalPlanarDir.Normalize())
	{
		return false;
	}

	if (!FMath::IsFinite(LocalPlanarDir.X) || !FMath::IsFinite(LocalPlanarDir.Y))
	{
		return false;
	}

	if (!FMath::IsFinite(ImpactReferenceYaw))
	{
		return false;
	}

	if (!FMath::IsFinite(HorizontalSpeed) || HorizontalSpeed <= 0.0f)
	{
		return false;
	}

	if (!FMath::IsFinite(VerticalSpeed) || VerticalSpeed <= 0.0f)
	{
		return false;
	}

	const FRotator TargetRotation(0.0f, ImpactReferenceYaw, 0.0f);
	const FVector WorldAttackerDir = TargetRotation.RotateVector(LocalPlanarDir);
	if (!FMath::IsFinite(WorldAttackerDir.X) || !FMath::IsFinite(WorldAttackerDir.Y))
	{
		return false;
	}

	const float ResolvedFacingYaw = WorldAttackerDir.Rotation().Yaw;
	if (!FMath::IsFinite(ResolvedFacingYaw))
	{
		return false;
	}

	const FVector WorldLaunchDir = -WorldAttackerDir;
	if (!FMath::IsFinite(WorldLaunchDir.X) || !FMath::IsFinite(WorldLaunchDir.Y))
	{
		return false;
	}

	const FVector ResolvedLaunchVelocity(
		WorldLaunchDir.X * HorizontalSpeed,
		WorldLaunchDir.Y * HorizontalSpeed,
		VerticalSpeed);

	if (!FMath::IsFinite(ResolvedLaunchVelocity.X) || !FMath::IsFinite(ResolvedLaunchVelocity.Y) || !FMath::IsFinite(ResolvedLaunchVelocity.Z))
	{
		OutFacingYaw = 0.0f;
		OutLaunchVelocity = FVector::ZeroVector;
		return false;
	}

	OutFacingYaw = ResolvedFacingYaw;
	OutLaunchVelocity = ResolvedLaunchVelocity;
	return true;
}

bool FHitReactionImpactResolver::TryBuildLaunchVelocity(
	const FVector& LocalAttackerDirection,
	float TargetYaw,
	float HorizontalSpeed,
	float VerticalSpeed,
	FVector& OutLaunchVelocity)
{
	float IgnoredFacingYaw = 0.0f;
	return TryBuildLaunchFacingAndVelocity(LocalAttackerDirection, TargetYaw, HorizontalSpeed, VerticalSpeed, IgnoredFacingYaw, OutLaunchVelocity);
}
