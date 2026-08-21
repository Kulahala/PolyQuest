#include "Combat/Projectile/CombatProjectile.h"

#include "AbilitySystemComponent.h"
#include "Combat/Equipment/ProjectileDefinition.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "PolyQuest.h"

ACombatProjectile::ACombatProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
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
	if (CollisionComponent)
	{
		CollisionComponent->OnComponentBeginOverlap.RemoveAll(this);
		CollisionComponent->OnComponentHit.RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
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

	MovementComponent->InitialSpeed = Def->InitialSpeed;
	MovementComponent->MaxSpeed = Def->MaxSpeed;
	MovementComponent->Velocity = GetActorForwardVector() * Def->InitialSpeed;

	SetLifeSpan(Def->LifespanSeconds);

	if (AActor* Source = LaunchRequest.SourceActor.Get())
	{
		CollisionComponent->IgnoreActorWhenMoving(Source, true);
	}

	return true;
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
		Destroy();
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

	bHitDelivered = true;
	Destroy();
}
