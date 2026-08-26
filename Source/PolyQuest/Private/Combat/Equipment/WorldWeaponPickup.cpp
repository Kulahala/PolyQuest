#include "Combat/Equipment/WorldWeaponPickup.h"

#include "AbilitySystemComponent.h"
#include "Character/Player/PlayerCharacter.h"
#include "Combat/Equipment/WeaponDefinition.h"
#include "Combat/Equipment/WorldPickupGrounding.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "PolyQuest.h"

AWorldWeaponPickup::AWorldWeaponPickup()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(RootComponent);
	InteractionSphere->SetSphereRadius(InteractionRadius);
	InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractionSphere->SetGenerateOverlapEvents(true);

	PickupMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMeshComponent"));
	PickupMeshComponent->SetupAttachment(RootComponent);
	PickupMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PickupMeshComponent->SetGenerateOverlapEvents(false);
}

void AWorldWeaponPickup::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (InteractionSphere)
	{
		InteractionSphere->SetSphereRadius(InteractionRadius);
	}

	UpdateVisualMesh();
}

void AWorldWeaponPickup::BeginPlay()
{
	Super::BeginPlay();
	UpdateVisualMesh();
}

void AWorldWeaponPickup::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bInteractionInProgress = false;
	FormerOwner.Reset();
	Super::EndPlay(EndPlayReason);
}

bool AWorldWeaponPickup::CanInteract(const APlayerCharacter* Requester) const
{
	if (!Requester || IsActorBeingDestroyed() || bInteractionInProgress || !WeaponDefinition)
	{
		return false;
	}

	if (Requester->IsActorBeingDestroyed())
	{
		return false;
	}

	const UAbilitySystemComponent* RequesterASC = Requester->GetAbilitySystemComponent();
	const FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
	if (RequesterASC && DeadTag.IsValid() && RequesterASC->HasMatchingGameplayTag(DeadTag))
	{
		return false;
	}

	const UWorld* World = GetWorld();
	if (FormerOwner.IsValid() && FormerOwner.Get() == Requester && World)
	{
		if (World->GetTimeSeconds() < RejectUntilTime)
		{
			return false;
		}
	}

	return true;
}

void AWorldWeaponPickup::SetWeaponDefinition(UWeaponDefinition* InDefinition)
{
	WeaponDefinition = InDefinition;
	UpdateVisualMesh();
}

void AWorldWeaponPickup::InitializeDroppedPickup(UWeaponDefinition* InDefinition, APlayerCharacter* InFormerOwner, float InRejectDurationSeconds)
{
	WeaponDefinition = InDefinition;
	FormerOwner = InFormerOwner;

	const UWorld* World = GetWorld();
	RejectUntilTime = (World ? World->GetTimeSeconds() : 0.0f) + FMath::Max(InRejectDurationSeconds, 0.0f);
	bInteractionInProgress = false;

	UpdateVisualMesh();
}

void AWorldWeaponPickup::BeginInteraction()
{
	bInteractionInProgress = true;
}

void AWorldWeaponPickup::EndInteraction()
{
	bInteractionInProgress = false;
}

void AWorldWeaponPickup::SetInteractionEnabled(bool bEnabled)
{
	if (InteractionSphere)
	{
		InteractionSphere->SetCollisionEnabled(bEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}
}

void AWorldWeaponPickup::UpdateVisualMesh()
{
	if (!PickupMeshComponent)
	{
		return;
	}

	if (WeaponDefinition)
	{
		PickupMeshComponent->SetStaticMesh(WeaponDefinition->WeaponMesh.Get());
		PickupMeshComponent->SetRelativeTransform(WeaponDefinition->WorldPickupDisplayTransform);
	}
	else
	{
		PickupMeshComponent->SetStaticMesh(nullptr);
		PickupMeshComponent->SetRelativeTransform(FTransform::Identity);
	}
}

namespace
{
	bool LineTraceGround(UWorld* World, const FVector& SourceLocation, FHitResult& OutHitResult, const AActor* IgnoreActor)
	{
		if (!World)
		{
			return false;
		}

		constexpr float TraceDownDistance = 300.0f;
		const FVector TraceStart = SourceLocation;
		const FVector TraceEnd = SourceLocation - FVector(0.0f, 0.0f, TraceDownDistance);

		FCollisionQueryParams QueryParams(TEXT("PickupGroundProjection"), false, IgnoreActor);

		const bool bHit = World->LineTraceSingleByChannel(
			OutHitResult,
			TraceStart,
			TraceEnd,
			ECC_Visibility,
			QueryParams);

		return bHit && OutHitResult.bBlockingHit;
	}
}

bool AWorldWeaponPickup::ProjectLocationToGround(UWorld* World, const FVector& SourceLocation, FVector& OutGroundLocation, const AActor* IgnoreActor)
{
	constexpr float NormalOffsetDistance = 2.0f;
	FHitResult HitResult;
	if (LineTraceGround(World, SourceLocation, HitResult, IgnoreActor))
	{
		OutGroundLocation = HitResult.ImpactPoint + (HitResult.ImpactNormal * NormalOffsetDistance);
		return true;
	}

	return false;
}

bool AWorldWeaponPickup::StageDisplacedDrops(APlayerCharacter* PlayerCharacter, const TArray<UWeaponDefinition*>& DisplacedDefinitions, TArray<AWorldWeaponPickup*>& OutProvisionalDrops)
{
	OutProvisionalDrops.Reset();

	if (DisplacedDefinitions.IsEmpty())
	{
		return true;
	}

	UWorld* World = GetWorld();
	if (!World || !PlayerCharacter)
	{
		return false;
	}

	const FVector PlayerLocation = PlayerCharacter->GetActorLocation();
	const FVector ForwardVector = PlayerCharacter->GetActorForwardVector();
	const FVector RightVector = PlayerCharacter->GetActorRightVector();

	for (int32 Index = 0; Index < DisplacedDefinitions.Num(); ++Index)
	{
		UWeaponDefinition* DisplacedDef = DisplacedDefinitions[Index];
		if (!DisplacedDef)
		{
			continue;
		}

		// Offset multiple drops slightly left/right so they do not spawn perfectly co-located
		FVector HorizontalOffset = FVector::ZeroVector;
		if (DisplacedDefinitions.Num() > 1)
		{
			const float SideMultiplier = (Index % 2 == 0) ? 1.0f : -1.0f;
			const float Step = 40.0f * (1 + Index / 2);
			HorizontalOffset = (RightVector * SideMultiplier * Step) + (ForwardVector * 20.0f);
		}
		else
		{
			HorizontalOffset = ForwardVector * 30.0f;
		}

		const FVector ProjectionSource = PlayerLocation + HorizontalOffset + FVector(0.0f, 0.0f, 50.0f);
		FHitResult HitResult;
		if (!LineTraceGround(World, ProjectionSource, HitResult, PlayerCharacter))
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("AWorldWeaponPickup: Ground projection trace failed for displaced definition '%s' near '%s'."), *GetNameSafe(DisplacedDef), *GetNameSafe(PlayerCharacter));
			for (AWorldWeaponPickup* SpawnedDrop : OutProvisionalDrops)
			{
				if (SpawnedDrop)
				{
					SpawnedDrop->Destroy();
				}
			}
			OutProvisionalDrops.Reset();
			return false;
		}

		const FTransform InitialSpawnTransform(FRotator::ZeroRotator, HitResult.ImpactPoint);
		AWorldWeaponPickup* NewDrop = World->SpawnActorDeferred<AWorldWeaponPickup>(
			GetClass(),
			InitialSpawnTransform,
			nullptr,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

		if (!NewDrop)
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("AWorldWeaponPickup: Failed to deferred-spawn drop actor for '%s'."), *GetNameSafe(DisplacedDef));
			for (AWorldWeaponPickup* SpawnedDrop : OutProvisionalDrops)
			{
				if (SpawnedDrop)
				{
					SpawnedDrop->Destroy();
				}
			}
			OutProvisionalDrops.Reset();
			return false;
		}

		NewDrop->InitializeDroppedPickup(DisplacedDef, PlayerCharacter, FormerOwnerRejectDuration);

		FVector FinalRootLocation = FVector::ZeroVector;
		constexpr float FixedClearance = 2.0f;
		bool bGroundingSuccess = false;

		if (const UStaticMesh* Mesh = DisplacedDef->WeaponMesh.Get())
		{
			const FBox LocalBox = Mesh->GetBoundingBox();
			bGroundingSuccess = FWorldPickupGrounding::TryComputeGroundedRootLocation(
				LocalBox,
				DisplacedDef->WorldPickupDisplayTransform,
				HitResult.ImpactPoint,
				HitResult.ImpactNormal,
				FixedClearance,
				FinalRootLocation);
		}
		else
		{
			FinalRootLocation = HitResult.ImpactPoint + (HitResult.ImpactNormal * FixedClearance);
			bGroundingSuccess = true;
		}

		if (!bGroundingSuccess)
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("AWorldWeaponPickup: Grounding calculation failed for displaced definition '%s' near '%s'."), *GetNameSafe(DisplacedDef), *GetNameSafe(PlayerCharacter));
			NewDrop->Destroy();
			for (AWorldWeaponPickup* SpawnedDrop : OutProvisionalDrops)
			{
				if (SpawnedDrop)
				{
					SpawnedDrop->Destroy();
				}
			}
			OutProvisionalDrops.Reset();
			return false;
		}

		NewDrop->SetInteractionEnabled(false);
		const FTransform FinalSpawnTransform(FRotator::ZeroRotator, FinalRootLocation);
		NewDrop->FinishSpawning(FinalSpawnTransform);

		OutProvisionalDrops.Add(NewDrop);
	}

	return true;
}
