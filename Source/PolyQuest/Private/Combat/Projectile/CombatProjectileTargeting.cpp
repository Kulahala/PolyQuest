#include "Combat/Projectile/CombatProjectileTargeting.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Camera/PlayerCameraManager.h"
#include "Combat/Melee/CombatTeamAgent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"

namespace
{
	const FGameplayTag& GetTargetingDeadTag()
	{
		static const FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
		return DeadTag;
	}

	const FGameplayTag& GetTargetingInvulnerableTag()
	{
		static const FGameplayTag InvulnerableTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Invulnerable")), false);
		return InvulnerableTag;
	}

	bool IsTargetPointInScreen(const APlayerController* PC, const FVector& WorldPoint, float MarginRatio = 0.06f)
	{
		if (!PC || !PC->IsLocalController())
		{
			return false;
		}

		int32 ViewportSizeX = 0;
		int32 ViewportSizeY = 0;
		PC->GetViewportSize(ViewportSizeX, ViewportSizeY);
		if (ViewportSizeX <= 0 || ViewportSizeY <= 0)
		{
			return false;
		}

		// Camera forward check: ensure point is strictly in front of the camera plane.
		if (PC->PlayerCameraManager)
		{
			const FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
			const FVector CameraForward = PC->PlayerCameraManager->GetCameraRotation().Vector();
			const FVector ToPoint = WorldPoint - CameraLocation;
			if (FVector::DotProduct(CameraForward, ToPoint) <= 0.0f)
			{
				return false;
			}
		}

		FVector2D ScreenPos = FVector2D::ZeroVector;
		const bool bProjected = PC->ProjectWorldLocationToScreen(WorldPoint, ScreenPos, false);
		if (!bProjected || !FMath::IsFinite(ScreenPos.X) || !FMath::IsFinite(ScreenPos.Y))
		{
			return false;
		}

		const float ClampedMargin = FMath::Max(0.0f, MarginRatio);
		const float MarginX = static_cast<float>(ViewportSizeX) * ClampedMargin;
		const float MarginY = static_cast<float>(ViewportSizeY) * ClampedMargin;

		if (ScreenPos.X < -MarginX || ScreenPos.X > (static_cast<float>(ViewportSizeX) + MarginX)
			|| ScreenPos.Y < -MarginY || ScreenPos.Y > (static_cast<float>(ViewportSizeY) + MarginY))
		{
			return false;
		}

		return true;
	}
}

FGameplayTag FCombatProjectileTargeting::ResolveTeamTag(const AActor* Actor)
{
	if (!Actor)
	{
		return FGameplayTag();
	}

	if (const ICombatTeamAgent* TeamAgent = Cast<const ICombatTeamAgent>(Actor))
	{
		const FGameplayTag NativeTag = TeamAgent->GetCombatTeamTag_Implementation();
		if (NativeTag.IsValid())
		{
			return NativeTag;
		}
	}

	if (Actor->GetClass()->ImplementsInterface(UCombatTeamAgent::StaticClass()))
	{
		return ICombatTeamAgent::Execute_GetCombatTeamTag(const_cast<AActor*>(Actor));
	}

	return FGameplayTag();
}

FVector FCombatProjectileTargeting::GetTargetAimPoint(const AActor* TargetActor)
{
	if (!TargetActor)
	{
		return FVector::ZeroVector;
	}

	if (const ACharacter* Character = Cast<ACharacter>(TargetActor))
	{
		if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		{
			return Character->GetActorLocation() + Character->GetActorUpVector() * (0.5f * Capsule->GetScaledCapsuleHalfHeight());
		}
	}

	return TargetActor->GetActorLocation();
}

bool FCombatProjectileTargeting::IsValidTargetCandidate(const AActor* SourceActor, const UAbilitySystemComponent* SourceASC, const AActor* TargetActor)
{
	if (!SourceActor || !TargetActor || TargetActor == SourceActor || !SourceASC || TargetActor->IsActorBeingDestroyed())
	{
		return false;
	}

	if (!SourceActor->GetClass()->ImplementsInterface(UCombatTeamAgent::StaticClass())
		|| !TargetActor->GetClass()->ImplementsInterface(UCombatTeamAgent::StaticClass()))
	{
		return false;
	}

	const FGameplayTag SourceTeamTag = ResolveTeamTag(SourceActor);
	const FGameplayTag TargetTeamTag = ResolveTeamTag(TargetActor);
	if (!SourceTeamTag.IsValid() || !TargetTeamTag.IsValid() || SourceTeamTag.MatchesTagExact(TargetTeamTag))
	{
		return false;
	}

	const FGameplayTag& DeadTag = GetTargetingDeadTag();
	const FGameplayTag& InvulnerableTag = GetTargetingInvulnerableTag();

	if (!DeadTag.IsValid() || !InvulnerableTag.IsValid())
	{
		return false;
	}

	if (SourceASC->HasMatchingGameplayTag(DeadTag))
	{
		return false;
	}

	const UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor);
	if (!TargetASC)
	{
		return false;
	}

	if (TargetASC->HasMatchingGameplayTag(DeadTag) || TargetASC->HasMatchingGameplayTag(InvulnerableTag))
	{
		return false;
	}

	return true;
}

bool FCombatProjectileTargeting::TryFindBestTargetCandidate(
	const UWorld* World,
	const AActor* SourceActor,
	const UAbilitySystemComponent* SourceASC,
	const FVector& LaunchLocation,
	const FVector& AimDirection,
	const FCombatProjectileTargetFilter& Filter,
	FCombatProjectileTargetCandidate& OutCandidate)
{
	if (!World || !SourceActor || !SourceASC || LaunchLocation.ContainsNaN() || AimDirection.ContainsNaN())
	{
		return false;
	}

	const FVector HorizontalAimDir = FVector(AimDirection.X, AimDirection.Y, 0.0f).GetSafeNormal2D();
	if (HorizontalAimDir.IsNearlyZero())
	{
		return false;
	}

	const APawn* SourcePawn = Cast<APawn>(SourceActor);
	const APlayerController* PC = SourcePawn ? Cast<APlayerController>(SourcePawn->GetController()) : nullptr;

	TArray<FCombatProjectileTargetCandidate> ValidCandidates;

	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		ACharacter* Candidate = *It;
		if (!Candidate || Candidate == SourceActor)
		{
			continue;
		}

		if (!IsValidTargetCandidate(SourceActor, SourceASC, Candidate))
		{
			continue;
		}

		const FVector AimPoint = GetTargetAimPoint(Candidate);
		if (AimPoint.ContainsNaN())
		{
			continue;
		}

		const FVector ToAim = AimPoint - LaunchLocation;
		const float HeightDelta = FMath::Abs(ToAim.Z);
		if (HeightDelta > Filter.MaxHeightDelta)
		{
			continue;
		}

		const FVector ToAim2D = FVector(ToAim.X, ToAim.Y, 0.0f);
		const float HorizontalDistance = ToAim2D.Size();
		if (HorizontalDistance > Filter.MaxHorizontalDistance || HorizontalDistance < KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const FVector ToAimDir2D = ToAim2D.GetSafeNormal2D();
		const float Dot = FMath::Clamp(FVector::DotProduct(HorizontalAimDir, ToAimDir2D), -1.0f, 1.0f);
		const float AngleDegrees = FMath::RadiansToDegrees(FMath::Acos(Dot));
		if (AngleDegrees > Filter.MaxAngleDegrees)
		{
			continue;
		}

		const float PitchDegrees = FMath::RadiansToDegrees(FMath::Abs(FMath::Atan2(ToAim.Z, HorizontalDistance)));
		if (PitchDegrees > Filter.MaxPitchDegrees)
		{
			continue;
		}

		// In-screen viewport check: verify candidate's aim point projects within the active local viewport (with margin expansion)
#if WITH_DEV_AUTOMATION_TESTS
		if (Filter.TestScreenProjectionHook)
		{
			FVector2D TestScreenPos = FVector2D::ZeroVector;
			FVector2D TestViewportSize = FVector2D::ZeroVector;
			const bool bHookValid = Filter.TestScreenProjectionHook(AimPoint, TestScreenPos, TestViewportSize);
			const float ClampedMargin = FMath::Max(0.0f, Filter.ScreenMarginRatio);
			const float MarginX = TestViewportSize.X * ClampedMargin;
			const float MarginY = TestViewportSize.Y * ClampedMargin;

			if (!bHookValid || !FMath::IsFinite(TestScreenPos.X) || !FMath::IsFinite(TestScreenPos.Y)
				|| TestViewportSize.X <= 0.0f || TestViewportSize.Y <= 0.0f
				|| TestScreenPos.X < -MarginX || TestScreenPos.X > (TestViewportSize.X + MarginX)
				|| TestScreenPos.Y < -MarginY || TestScreenPos.Y > (TestViewportSize.Y + MarginY))
			{
				continue;
			}
		}
		else if (Filter.bBypassScreenFilterForTesting)
		{
			// Explicit bypass for non-screen geometry tests
		}
		else
#endif
		{
			if (!IsTargetPointInScreen(PC, AimPoint, Filter.ScreenMarginRatio))
			{
				continue;
			}
		}

		// Line trace visibility query ignoring source and candidate
		FCollisionQueryParams QueryParams(TEXT("BowTargetAssistVisibility"), false);
		QueryParams.AddIgnoredActor(SourceActor);
		QueryParams.AddIgnoredActor(Candidate);

		FHitResult HitResult;
		const bool bHit = World->LineTraceSingleByChannel(
			HitResult,
			LaunchLocation,
			AimPoint,
			ECC_Visibility,
			QueryParams);

		if (bHit && HitResult.bBlockingHit)
		{
			// Blocked by third-party obstruction (e.g. wall/static geometry)
			continue;
		}

		FCombatProjectileTargetCandidate ValidCandidate;
		ValidCandidate.TargetActor = Candidate;
		ValidCandidate.AimPoint = AimPoint;
		ValidCandidate.AngleDegrees = AngleDegrees;
		ValidCandidate.HorizontalDistance = HorizontalDistance;
		ValidCandidates.Add(ValidCandidate);
	}

	if (ValidCandidates.Num() == 0)
	{
		return false;
	}

	ValidCandidates.Sort([](const FCombatProjectileTargetCandidate& A, const FCombatProjectileTargetCandidate& B)
	{
		if (!FMath::IsNearlyEqual(A.AngleDegrees, B.AngleDegrees, 0.01f))
		{
			return A.AngleDegrees < B.AngleDegrees;
		}
		if (!FMath::IsNearlyEqual(A.HorizontalDistance, B.HorizontalDistance, 1.0f))
		{
			return A.HorizontalDistance < B.HorizontalDistance;
		}
		const FString NameA = A.TargetActor.IsValid() ? A.TargetActor->GetName() : FString();
		const FString NameB = B.TargetActor.IsValid() ? B.TargetActor->GetName() : FString();
		return NameA < NameB;
	});

	OutCandidate = ValidCandidates[0];
	return true;
}
