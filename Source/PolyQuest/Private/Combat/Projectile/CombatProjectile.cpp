#include "Combat/Projectile/CombatProjectile.h"

#include "AbilitySystemComponent.h"
#include "Combat/Equipment/ProjectileDefinition.h"
#include "Combat/Projectile/CombatProjectileTargeting.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "PolyQuest.h"

ACombatProjectile::ACombatProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	SetReplicates(false);

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	SetRootComponent(CollisionComponent);
	CollisionComponent->InitSphereRadius(12.0f);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComponent->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CollisionComponent->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	CollisionComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	CollisionComponent->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	CollisionComponent->SetGenerateOverlapEvents(true);

	ProjectileMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProjectileMeshComponent"));
	ProjectileMeshComponent->SetupAttachment(CollisionComponent);
	ProjectileMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ProjectileMeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	ProjectileMeshComponent->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	ProjectileMeshComponent->SetGenerateOverlapEvents(false);

	FlightTrailComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("FlightTrailComponent"));
	FlightTrailComponent->SetupAttachment(ProjectileMeshComponent);
	FlightTrailComponent->bAutoActivate = false;
	FlightTrailComponent->bAutoManageAttachment = false;
	FlightTrailComponent->SetAutoDestroy(false);

	MovementComponent = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("MovementComponent"));
	MovementComponent->SetUpdatedComponent(CollisionComponent);
	MovementComponent->InitialSpeed = 3000.0f;
	MovementComponent->MaxSpeed = 3000.0f;
	MovementComponent->ProjectileGravityScale = 0.0f;
	MovementComponent->bShouldBounce = false;
	MovementComponent->bIsHomingProjectile = false;
	MovementComponent->bRotationFollowsVelocity = true;
	MovementComponent->bInitialVelocityInLocalSpace = true;
}

void ACombatProjectile::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (CollisionComponent)
	{
		CollisionComponent->OnComponentBeginOverlap.AddUniqueDynamic(this, &ACombatProjectile::OnProjectileOverlap);
		CollisionComponent->OnComponentHit.AddUniqueDynamic(this, &ACombatProjectile::OnProjectileHit);
	}

	if (FlightTrailComponent)
	{
		FlightTrailComponent->OnSystemFinished.AddUniqueDynamic(this, &ACombatProjectile::OnFlightTrailSystemFinished);
	}
}

void ACombatProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (CollisionComponent)
	{
		CollisionComponent->OnComponentBeginOverlap.AddUniqueDynamic(this, &ACombatProjectile::OnProjectileOverlap);
		CollisionComponent->OnComponentHit.AddUniqueDynamic(this, &ACombatProjectile::OnProjectileHit);
	}
}

void ACombatProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FlightTrailFinishTimeoutTimerHandle);
	}

	if (FlightTrailComponent)
	{
		FlightTrailComponent->OnSystemFinished.RemoveAll(this);
	}

	StopFlightTrail();

	if (MovementComponent)
	{
		MovementComponent->RemoveTickPrerequisiteActor(this);
	}
	StopHomingAndFlyStraight();

	if (CollisionComponent)
	{
		CollisionComponent->OnComponentBeginOverlap.RemoveAll(this);
		CollisionComponent->OnComponentHit.RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

void ACombatProjectile::LifeSpanExpired()
{
	BeginTerminalFlightTrailFadeOut();
}

void ACombatProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bHomingActive)
	{
		UpdateLimitedHoming(DeltaSeconds);
	}
}

bool ACombatProjectile::InitializeProjectile(const FCombatProjectileLaunchRequest& LaunchRequest)
{
	if (!LaunchRequest.Definition)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("ACombatProjectile::InitializeProjectile rejected a null Definition."));
		Destroy();
		return false;
	}

	FString ValidationReason;
	if (!LaunchRequest.Definition->IsValidProjectileDefinition(ValidationReason))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("ACombatProjectile::InitializeProjectile rejected invalid Definition: %s"), *ValidationReason);
		Destroy();
		return false;
	}

	CachedLaunchRequest = LaunchRequest;
	CachedDamageGameplayEffectClass = LaunchRequest.Definition->DamageGameplayEffectClass;
	bInitialized = true;

	const UProjectileDefinition* Def = LaunchRequest.Definition;

	CollisionComponent->SetSphereRadius(Def->CollisionRadius);

	if (Def->ProjectileMesh)
	{
		ProjectileMeshComponent->SetStaticMesh(Def->ProjectileMesh);
		ProjectileMeshComponent->SetRelativeRotation(Def->DisplayRotationOffset);
		ProjectileMeshComponent->SetRelativeScale3D(Def->DisplayScale);
	}

	InitialLaunchLocation = GetActorLocation();
	InitialLaunchDirection = LaunchRequest.InitialFlightDirection.IsNearlyZero()
		? GetActorForwardVector()
		: LaunchRequest.InitialFlightDirection.GetSafeNormal();

	MovementComponent->InitialSpeed = Def->InitialSpeed;
	MovementComponent->MaxSpeed = Def->MaxSpeed;
	MovementComponent->Velocity = InitialLaunchDirection * Def->InitialSpeed;
	SetActorRotation(InitialLaunchDirection.Rotation());

	SetLifeSpan(Def->LifespanSeconds);

	if (AActor* Source = LaunchRequest.SourceActor.Get())
	{
		CollisionComponent->IgnoreActorWhenMoving(Source, true);
	}

	if (LaunchRequest.bEnableLimitedHoming && LaunchRequest.TargetActor.IsValid())
	{
		CachedTargetActor = LaunchRequest.TargetActor;
		bHomingActive = true;
		HomingElapsedTime = 0.0f;
		TotalTurnAngleDegrees = 0.0f;
		CachedHomingStartDelaySeconds = LaunchRequest.HomingStartDelaySeconds;
		CachedHomingDurationSeconds = LaunchRequest.HomingDurationSeconds;
		CachedHomingTurnRateDegreesPerSecond = LaunchRequest.HomingTurnRateDegreesPerSecond;
		CachedHomingMaxTotalTurnDegrees = LaunchRequest.HomingMaxTotalTurnDegrees;
		CachedTargetAssistMaxDistance = LaunchRequest.TargetAssistMaxDistance;
		CachedTargetAssistMaxHeightDelta = LaunchRequest.TargetAssistMaxHeightDelta;

		MovementComponent->AddTickPrerequisiteActor(this);
		PrimaryActorTick.SetTickFunctionEnable(true);
	}
	else
	{
		bHomingActive = false;
		PrimaryActorTick.SetTickFunctionEnable(false);
	}

	ConfigureAndStartFlightTrail(*Def);

	return true;
}

void ACombatProjectile::StopHomingAndFlyStraight()
{
	bHomingActive = false;
	CachedTargetActor = nullptr;
	PrimaryActorTick.SetTickFunctionEnable(false);
}

void ACombatProjectile::UpdateLimitedHoming(float DeltaSeconds)
{
	if (!bHomingActive || !bInitialized || !MovementComponent)
	{
		return;
	}

	HomingElapsedTime += DeltaSeconds;
	if (HomingElapsedTime < CachedHomingStartDelaySeconds)
	{
		// Still in initial straight-flight delay: do not alter velocity or accumulate turn angle
		return;
	}

	const float ActiveHomingTime = HomingElapsedTime - CachedHomingStartDelaySeconds;
	if (ActiveHomingTime >= CachedHomingDurationSeconds)
	{
		StopHomingAndFlyStraight();
		return;
	}

	if (TotalTurnAngleDegrees >= CachedHomingMaxTotalTurnDegrees)
	{
		StopHomingAndFlyStraight();
		return;
	}

	AActor* Target = CachedTargetActor.Get();
	if (!Target || Target->IsActorBeingDestroyed())
	{
		StopHomingAndFlyStraight();
		return;
	}

	if (!FCombatProjectileTargeting::IsValidTargetCandidate(
			CachedLaunchRequest.SourceActor.Get(),
			CachedLaunchRequest.SourceAbilitySystemComponent.Get(),
			Target))
	{
		StopHomingAndFlyStraight();
		return;
	}

	const FVector CurrentTargetAimPoint = FCombatProjectileTargeting::GetTargetAimPoint(Target);
	if (CurrentTargetAimPoint.ContainsNaN())
	{
		StopHomingAndFlyStraight();
		return;
	}

	const FVector FromLaunch = CurrentTargetAimPoint - InitialLaunchLocation;
	if (FMath::Abs(FromLaunch.Z) > CachedTargetAssistMaxHeightDelta || FromLaunch.Size2D() > CachedTargetAssistMaxDistance)
	{
		StopHomingAndFlyStraight();
		return;
	}

	FVector CurrentVelocity = MovementComponent->Velocity;
	const float CurrentSpeed = CurrentVelocity.Size();
	if (CurrentSpeed < KINDA_SMALL_NUMBER || CurrentVelocity.ContainsNaN())
	{
		StopHomingAndFlyStraight();
		return;
	}

	const FVector CurrentDir = CurrentVelocity / CurrentSpeed;
	const FVector ToTarget = CurrentTargetAimPoint - GetActorLocation();
	if (ToTarget.SizeSquared() < KINDA_SMALL_NUMBER || ToTarget.ContainsNaN())
	{
		StopHomingAndFlyStraight();
		return;
	}

	const FVector DesiredDir = ToTarget.GetSafeNormal();
	const float DotWithCurrent = FVector::DotProduct(CurrentDir, DesiredDir);
	if (DotWithCurrent <= 0.0f && CachedHomingMaxTotalTurnDegrees <= 90.0f)
	{
		// Target is in rear hemisphere and max turn is <= 90 deg: abandon tracking and fly straight
		StopHomingAndFlyStraight();
		return;
	}

	const float AngleToTargetDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(DotWithCurrent, -1.0f, 1.0f)));
	if (AngleToTargetDeg <= 0.01f)
	{
		return;
	}

	const float EffectiveDeltaSeconds = FMath::Min(DeltaSeconds, ActiveHomingTime);
	const float MaxTurnStep = CachedHomingTurnRateDegreesPerSecond * EffectiveDeltaSeconds;
	const float RemainingBudget = CachedHomingMaxTotalTurnDegrees - TotalTurnAngleDegrees;
	const float TurnAngleDeg = FMath::Min3(MaxTurnStep, RemainingBudget, AngleToTargetDeg);
	if (TurnAngleDeg <= 0.0f)
	{
		StopHomingAndFlyStraight();
		return;
	}

	const FRotator CurrentRot = CurrentDir.Rotation();
	const FRotator DesiredRot = DesiredDir.Rotation();

	const float DeltaYaw = FMath::FindDeltaAngleDegrees(CurrentRot.Yaw, DesiredRot.Yaw);
	const float DeltaPitch = FMath::FindDeltaAngleDegrees(CurrentRot.Pitch, DesiredRot.Pitch);

	const float AbsYaw = FMath::Abs(DeltaYaw);
	const float AbsPitch = FMath::Abs(DeltaPitch);
	const float TotalAbs = AbsYaw + AbsPitch;

	float StepYaw = 0.0f;
	float StepPitch = 0.0f;

	if (TotalAbs > KINDA_SMALL_NUMBER)
	{
		float YawWeight = AbsYaw / TotalAbs;
		float PitchWeight = AbsPitch / TotalAbs;

		// When reversing or turning sharply (> 60 deg yaw difference), prioritize horizontal yaw turning
		// to prevent vertical downward diving into the ground during U-turns.
		if (AbsYaw > 60.0f)
		{
			const float Factor = FMath::Clamp((AbsYaw - 60.0f) / 60.0f, 0.0f, 1.0f);
			PitchWeight *= (1.0f - Factor);
			YawWeight = 1.0f - PitchWeight;
		}

		StepYaw = FMath::Sign(DeltaYaw) * FMath::Min(AbsYaw, TurnAngleDeg * YawWeight);
		StepPitch = FMath::Sign(DeltaPitch) * FMath::Min(AbsPitch, TurnAngleDeg * PitchWeight);
	}

	const FRotator NewRot(
		FMath::Clamp(CurrentRot.Pitch + StepPitch, -80.0f, 80.0f),
		CurrentRot.Yaw + StepYaw,
		0.0f
	);
	const FVector NewDir = NewRot.Vector().GetSafeNormal();

	const float ActualTurnDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(CurrentDir, NewDir), -1.0f, 1.0f)));
	TotalTurnAngleDegrees += FMath::Max(TurnAngleDeg, ActualTurnDeg);
	MovementComponent->Velocity = NewDir * CurrentSpeed;
	MovementComponent->UpdateComponentVelocity();

	if (TotalTurnAngleDegrees >= CachedHomingMaxTotalTurnDegrees)
	{
		StopHomingAndFlyStraight();
	}
}

void ACombatProjectile::OnProjectileOverlap(
	UPrimitiveComponent*,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (bHitDelivered || !OtherActor || OtherActor == this || OtherActor == CachedLaunchRequest.SourceActor.Get())
	{
		return;
	}

	FHitResult HitResult = (bFromSweep && SweepResult.GetActor())
		? SweepResult
		: FHitResult(OtherActor, OtherComp, OtherActor->GetActorLocation(), -GetActorForwardVector());
	HandlePawnImpact(OtherActor, HitResult);
}

void ACombatProjectile::OnProjectileHit(
	UPrimitiveComponent*,
	AActor* OtherActor,
	UPrimitiveComponent*,
	FVector,
	const FHitResult& Hit)
{
	if (bHitDelivered || (OtherActor && (OtherActor == this || OtherActor == CachedLaunchRequest.SourceActor.Get())))
	{
		return;
	}

	if (Cast<APawn>(OtherActor))
	{
		HandlePawnImpact(OtherActor, Hit);
	}
	else
	{
		HandleBlockingImpact(Hit);
	}
}

void ACombatProjectile::HandlePawnImpact(AActor* HitActor, const FHitResult& HitResult)
{
	if (bHitDelivered || IsActorBeingDestroyed() || !HitActor || HitActor == this || HitActor == CachedLaunchRequest.SourceActor.Get())
	{
		return;
	}

	if (!bInitialized)
	{
		return;
	}

	FCombatProjectileHitRequest HitRequest;
	HitRequest.SourceActor = CachedLaunchRequest.SourceActor;
	HitRequest.SourceAbilitySystemComponent = CachedLaunchRequest.SourceAbilitySystemComponent;
	HitRequest.DamageGameplayEffectClass = CachedDamageGameplayEffectClass;
	HitRequest.AbilityLevel = CachedLaunchRequest.AbilityLevel;
	HitRequest.SetByCallerMagnitudeTag = CachedLaunchRequest.SetByCallerMagnitudeTag;
	HitRequest.SetByCallerMagnitude = CachedLaunchRequest.SetByCallerMagnitude;
	HitRequest.SetByCallerMagnitudes = CachedLaunchRequest.SetByCallerMagnitudes;
	HitRequest.GuardStaminaDamage = CachedLaunchRequest.GuardStaminaDamage;
	HitRequest.SourceObject = this;
	HitRequest.HitResult = HitResult;

	const bool bResolved = FCombatProjectileHitResolver::TryResolveHit(HitRequest);
	if (bResolved)
	{
		bHitDelivered = true;
#if !(UE_BUILD_SHIPPING)
		if (UWorld* World = GetWorld())
		{
			DrawDebugSphere(World, HitResult.ImpactPoint, 20.0f, 12, FColor::Red, false, 3.0f, 0, 2.5f);
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(1003, 3.0f, FColor::Red,
					FString::Printf(TEXT("[Projectile Hit] Damage Delivered to '%s'"), *GetNameSafe(HitActor)));
			}
		}
#endif
		BeginTerminalFlightTrailFadeOut();
	}
	else
	{
		// Non-hostile, dead, invulnerable, or unresolvable pawn: ignore and continue flight without delivering hit.
		if (CollisionComponent)
		{
			CollisionComponent->IgnoreActorWhenMoving(HitActor, true);
		}
	}
}

void ACombatProjectile::HandleBlockingImpact(const FHitResult& HitResult)
{
	if (bHitDelivered || IsActorBeingDestroyed())
	{
		return;
	}

#if !(UE_BUILD_SHIPPING)
	if (UWorld* World = GetWorld())
	{
		DrawDebugSphere(World, HitResult.ImpactPoint, 15.0f, 12, FColor::Orange, false, 3.0f, 0, 2.0f);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(1002, 3.0f, FColor::Orange,
				FString::Printf(TEXT("[Projectile Hit] Blocked by '%s' (Comp: '%s') at (%.0f, %.0f, %.0f)"),
					*GetNameSafe(HitResult.GetActor()), *GetNameSafe(HitResult.GetComponent()),
					HitResult.ImpactPoint.X, HitResult.ImpactPoint.Y, HitResult.ImpactPoint.Z));
		}
	}
#endif

	bHitDelivered = true;
	BeginTerminalFlightTrailFadeOut();
}

void ACombatProjectile::ResolveFlightTrailSocket(
	const UProjectileDefinition& Definition,
	FName& OutSocketName,
	bool& bOutUsedRootFallback)
{
	if (Definition.FlightTrailSocketName.IsNone())
	{
		OutSocketName = NAME_None;
		bOutUsedRootFallback = false;
		return;
	}

	if (ProjectileMeshComponent && ProjectileMeshComponent->GetStaticMesh() && ProjectileMeshComponent->DoesSocketExist(Definition.FlightTrailSocketName))
	{
		OutSocketName = Definition.FlightTrailSocketName;
		bOutUsedRootFallback = false;
	}
	else
	{
		OutSocketName = NAME_None;
		bOutUsedRootFallback = true;
		UE_LOG(
			LogPolyQuest,
			Warning,
			TEXT("ACombatProjectile '%s' requested FlightTrailSocket '%s', but it was not found on mesh '%s'. Falling back to component root."),
			*GetName(),
			*Definition.FlightTrailSocketName.ToString(),
			ProjectileMeshComponent && ProjectileMeshComponent->GetStaticMesh() ? *ProjectileMeshComponent->GetStaticMesh()->GetName() : TEXT("None"));
	}
}

void ACombatProjectile::ConfigureAndStartFlightTrail(const UProjectileDefinition& Definition)
{
	StopFlightTrail();

	if (!Definition.FlightTrailSystem)
	{
		return;
	}

	CachedFlightTrailFinishTimeoutSeconds = Definition.FlightTrailFinishTimeoutSeconds;
	if (!FMath::IsFinite(CachedFlightTrailFinishTimeoutSeconds) || CachedFlightTrailFinishTimeoutSeconds <= 0.0f)
	{
		UE_LOG(
			LogPolyQuest,
			Warning,
			TEXT("ACombatProjectile '%s' has non-positive or non-finite FlightTrailFinishTimeoutSeconds (%.3f); falling back to 0.35s."),
			*GetName(),
			CachedFlightTrailFinishTimeoutSeconds);
		CachedFlightTrailFinishTimeoutSeconds = 0.35f;
	}

	if (!FlightTrailComponent || !ProjectileMeshComponent)
	{
		return;
	}

	FName ResolvedSocketName = NAME_None;
	bool bUsedRootFallback = false;
	ResolveFlightTrailSocket(Definition, ResolvedSocketName, bUsedRootFallback);

	FlightTrailComponent->AttachToComponent(
		ProjectileMeshComponent,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		ResolvedSocketName);

#if WITH_DEV_AUTOMATION_TESTS
	if (bTestFlightTrailTrackingEnabled)
	{
		TestFlightTrailSystem = Definition.FlightTrailSystem;
		TestFlightTrailAttachSocketName = ResolvedSocketName;
		bTestFlightTrailUsedRootFallback = bUsedRootFallback;
		bTestFlightTrailActive = true;
		return;
	}
#endif

	FlightTrailComponent->SetAsset(Definition.FlightTrailSystem);
	FlightTrailComponent->Activate();
}

void ACombatProjectile::StopFlightTrail()
{
	if (FlightTrailComponent)
	{
		FlightTrailComponent->Deactivate();
		FlightTrailComponent->SetAsset(nullptr);
	}

#if WITH_DEV_AUTOMATION_TESTS
	if (bTestFlightTrailTrackingEnabled)
	{
		bTestFlightTrailActive = false;
		bTestTerminalFlightTrailFadeOutActive = false;
	}
#endif
}

void ACombatProjectile::BeginTerminalFlightTrailFadeOut()
{
	if (bTerminalFlightTrailFadeOutActive || IsActorBeingDestroyed())
	{
		return;
	}

	bTerminalFlightTrailFadeOutActive = true;
	SetLifeSpan(0.0f);

	if (CollisionComponent)
	{
		CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		CollisionComponent->OnComponentBeginOverlap.RemoveAll(this);
		CollisionComponent->OnComponentHit.RemoveAll(this);
	}

	if (MovementComponent)
	{
		MovementComponent->StopMovementImmediately();
		MovementComponent->SetComponentTickEnabled(false);
		MovementComponent->RemoveTickPrerequisiteActor(this);
	}

	StopHomingAndFlyStraight();
	PrimaryActorTick.SetTickFunctionEnable(false);

#if WITH_DEV_AUTOMATION_TESTS
	const bool bHasActiveTrail = bTestFlightTrailTrackingEnabled
		? (TestFlightTrailSystem.IsValid() && bTestFlightTrailActive)
		: (FlightTrailComponent && FlightTrailComponent->GetAsset() != nullptr && FlightTrailComponent->IsActive());
	if (bTestFlightTrailTrackingEnabled && bHasActiveTrail)
	{
		bTestTerminalFlightTrailFadeOutActive = true;
	}
#else
	const bool bHasActiveTrail = FlightTrailComponent && FlightTrailComponent->GetAsset() != nullptr && FlightTrailComponent->IsActive();
#endif

	if (!bHasActiveTrail)
	{
		FinishTerminalFlightTrailFadeOut();
		return;
	}

	if (FlightTrailComponent)
	{
		FlightTrailComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	}

	if (ProjectileMeshComponent)
	{
		ProjectileMeshComponent->SetHiddenInGame(true);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			FlightTrailFinishTimeoutTimerHandle,
			this,
			&ACombatProjectile::OnFlightTrailFinishTimeout,
			CachedFlightTrailFinishTimeoutSeconds,
			false);
	}

	if (FlightTrailComponent)
	{
		FlightTrailComponent->Deactivate();
	}
}

void ACombatProjectile::FinishTerminalFlightTrailFadeOut()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FlightTrailFinishTimeoutTimerHandle);
	}

	if (FlightTrailComponent)
	{
		FlightTrailComponent->OnSystemFinished.RemoveAll(this);
	}

	Destroy();
}

void ACombatProjectile::OnFlightTrailFinishTimeout()
{
	FinishTerminalFlightTrailFadeOut();
}

void ACombatProjectile::OnFlightTrailSystemFinished(UNiagaraComponent* FinishedComponent)
{
	if (FinishedComponent != FlightTrailComponent || !bTerminalFlightTrailFadeOutActive)
	{
		return;
	}

	FinishTerminalFlightTrailFadeOut();
}
