#include "Character/BaseCharacter.h"

#include "AbilitySystem/CharacterAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Combat/Feedback/CombatFeedbackDataAsset.h"
#include "Combat/Melee/MeleeTraceSourceComponent.h"
#include "Combat/Melee/MeleeWeaponTrailComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffectTypes.h"
#include "GameFramework/Controller.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInterface.h"
#include "PolyQuest.h"
#include "TimerManager.h"

ABaseCharacter::ABaseCharacter()
{
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	Attributes = CreateDefaultSubobject<UCharacterAttributeSet>(TEXT("Attributes"));
	MeleeTraceSource = CreateDefaultSubobject<UMeleeTraceSourceComponent>(TEXT("MeleeTraceSource"));
	MeleeWeaponTrail = CreateDefaultSubobject<UMeleeWeaponTrailComponent>(TEXT("MeleeWeaponTrail"));
	MeleeWeaponTrail->SetupAttachment(GetCapsuleComponent());
	MeleeWeaponTrail->SetAutoActivate(false);
	MeleeWeaponTrail->bAutoManageAttachment = false;
	MeleeWeaponTrail->SetAutoDestroy(false);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	AbilitySystemComponent->AddAttributeSetSubobject(Attributes.Get());
}

void ABaseCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (UCapsuleComponent* CapsuleComp = GetCapsuleComponent())
	{
		CapsuleComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}
	DisableFixedWeaponDisplayCollision();
	InitializeAbilityActorInfo();
}

void ABaseCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	InitializeAbilityActorInfo();

	if (HasAuthority())
	{
		GrantStartupAbilities();
	}
}

void ABaseCharacter::InitializeAbilityActorInfo()
{
	if (!ensure(AbilitySystemComponent))
	{
		return;
	}

	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	BindMoveSpeedAttribute();
}

void ABaseCharacter::GrantStartupAbilities()
{
	if (!ensure(AbilitySystemComponent))
	{
		return;
	}

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : StartupAbilities)
	{
		if (!AbilityClass || AbilitySystemComponent->FindAbilitySpecFromClass(AbilityClass))
		{
			continue;
		}

		AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, this));
	}
}

void ABaseCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearHitFeedbackOverlay();
	UnbindMoveSpeedAttribute();

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->CancelAllAbilities();
	}

	Super::EndPlay(EndPlayReason);
}

UAbilitySystemComponent* ABaseCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent.Get();
}

FGameplayTag ABaseCharacter::GetCombatTeamTag_Implementation() const
{
	return CombatTeamTag;
}

UMeleeTraceSourceComponent* ABaseCharacter::GetMeleeTraceSource() const
{
	return MeleeTraceSource.Get();
}

#if WITH_DEV_AUTOMATION_TESTS
void ABaseCharacter::ConfigureTestHitFeedbackOverlay(UMaterialInterface* InOverlayMaterial, const float InDurationSeconds)
{
	if (!CombatFeedbackData)
	{
		CombatFeedbackData = NewObject<UCombatFeedbackDataAsset>(this, NAME_None, RF_Transient);
	}
	CombatFeedbackData->HitFeedbackOverlayMaterial = InOverlayMaterial;
	CombatFeedbackData->HitFeedbackOverlayDurationSeconds = InDurationSeconds;
	bHasLoggedInvalidHitFeedbackOverlayConfiguration = false;
}
#endif

void ABaseCharacter::TriggerHitFeedbackOverlay()
{
	USkeletalMeshComponent* MeshComponent = GetMesh();
	if (!MeshComponent)
	{
		return;
	}

	if (!CombatFeedbackData)
	{
		if (!bHasLoggedInvalidHitFeedbackOverlayConfiguration)
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("'%s' cannot apply hit feedback Overlay without CombatFeedbackData configured."), *GetNameSafe(this));
			bHasLoggedInvalidHitFeedbackOverlayConfiguration = true;
		}
		return;
	}

	UMaterialInterface* OverlayMaterial = CombatFeedbackData->HitFeedbackOverlayMaterial.Get();
	const float DurationSeconds = CombatFeedbackData->HitFeedbackOverlayDurationSeconds;

	if (!OverlayMaterial || !FMath::IsFinite(DurationSeconds) || DurationSeconds <= 0.0f)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UMaterialInterface* CurrentOverlayMaterial = MeshComponent->GetOverlayMaterial();
	if (!bHitFeedbackOverlayActive)
	{
		PreviousHitFeedbackOverlayMaterial = CurrentOverlayMaterial;
		bHitFeedbackOverlayActive = true;
	}
	else if (CurrentOverlayMaterial != ActiveHitFeedbackOverlayMaterial.Get())
	{
		// An external presentation owner changed the Overlay between hits. Preserve it for restoration.
		PreviousHitFeedbackOverlayMaterial = CurrentOverlayMaterial;
	}

	ActiveHitFeedbackOverlayMaterial = OverlayMaterial;
	MeshComponent->SetOverlayMaterial(ActiveHitFeedbackOverlayMaterial.Get());
	World->GetTimerManager().SetTimer(
		HitFeedbackOverlayTimerHandle,
		this,
		&ABaseCharacter::ClearHitFeedbackOverlay,
		DurationSeconds,
		false);
}

void ABaseCharacter::ClearHitFeedbackOverlay()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HitFeedbackOverlayTimerHandle);
	}
	HitFeedbackOverlayTimerHandle.Invalidate();

	if (bHitFeedbackOverlayActive)
	{
		if (USkeletalMeshComponent* MeshComponent = GetMesh())
		{
			if (MeshComponent->GetOverlayMaterial() == ActiveHitFeedbackOverlayMaterial.Get())
			{
				MeshComponent->SetOverlayMaterial(PreviousHitFeedbackOverlayMaterial.Get());
			}
		}
	}

	PreviousHitFeedbackOverlayMaterial = nullptr;
	ActiveHitFeedbackOverlayMaterial = nullptr;
	bHitFeedbackOverlayActive = false;
}

void ABaseCharacter::DisableFixedWeaponDisplayCollision()
{
	static const FName WeaponDisplayComponentName(TEXT("WeaponMesh"));
	TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(this);
	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!PrimitiveComponent || PrimitiveComponent->GetFName() != WeaponDisplayComponentName)
		{
			continue;
		}

		PrimitiveComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PrimitiveComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
		PrimitiveComponent->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		PrimitiveComponent->SetGenerateOverlapEvents(false);
		return;
	}
}

void ABaseCharacter::BindMoveSpeedAttribute()
{
	UAbilitySystemComponent* CurrentAbilitySystemComponent = AbilitySystemComponent.Get();
	if (!CurrentAbilitySystemComponent)
	{
		return;
	}

	if (MoveSpeedBoundAbilitySystemComponent.Get() != CurrentAbilitySystemComponent)
	{
		UnbindMoveSpeedAttribute();
		MoveSpeedBoundAbilitySystemComponent = CurrentAbilitySystemComponent;
		MoveSpeedAttributeChangedHandle = CurrentAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetMoveSpeedAttribute())
			.AddUObject(this, &ABaseCharacter::OnMoveSpeedAttributeChanged);
	}

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->MaxWalkSpeed = FMath::Max(
			CurrentAbilitySystemComponent->GetNumericAttribute(UCharacterAttributeSet::GetMoveSpeedAttribute()),
			0.0f);
	}
}

void ABaseCharacter::UnbindMoveSpeedAttribute()
{
	if (UAbilitySystemComponent* BoundAbilitySystemComponent = MoveSpeedBoundAbilitySystemComponent.Get())
	{
		if (MoveSpeedAttributeChangedHandle.IsValid())
		{
			BoundAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetMoveSpeedAttribute())
				.Remove(MoveSpeedAttributeChangedHandle);
		}
	}

	MoveSpeedAttributeChangedHandle.Reset();
	MoveSpeedBoundAbilitySystemComponent.Reset();
}

void ABaseCharacter::OnMoveSpeedAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->MaxWalkSpeed = FMath::Max(ChangeData.NewValue, 0.0f);
	}
}
