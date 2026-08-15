#include "Character/Enemy/EnemyCharacter.h"

#include "AI/EnemyAIController.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystem/CharacterAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "PolyQuest.h"

AEnemyCharacter::AEnemyCharacter()
{
	CombatTeamTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Team.Enemy")), false);
	DeadStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
	HitReactionEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Enemy.Hit")), false);
	InterruptReactionDataTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Data.Reaction.Interrupt")), false);
	AIControllerClass = AEnemyAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	bUseControllerRotationYaw = true;

	GetCharacterMovement()->bOrientRotationToMovement = false;
}

void AEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();
	BindDeathEvents();
}

void AEnemyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindDeathEvents();
	Super::EndPlay(EndPlayReason);
}

bool AEnemyCharacter::IsDead() const
{
	const UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent();
	return CharacterASC && DeadStateTag.IsValid()
		&& CharacterASC->HasMatchingGameplayTag(DeadStateTag);
}

void AEnemyCharacter::BindDeathEvents()
{
	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent();
	if (!CharacterASC || DeathBoundAbilitySystemComponent.Get() == CharacterASC)
	{
		return;
	}

	UnbindDeathEvents();
	DeathBoundAbilitySystemComponent = CharacterASC;
	HealthAttributeChangedHandle = CharacterASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetHealthAttribute())
		.AddUObject(this, &AEnemyCharacter::OnHealthAttributeChanged);

	if (DeadStateTag.IsValid())
	{
		DeadStateTagChangedHandle = CharacterASC->RegisterGameplayTagEvent(DeadStateTag)
			.AddUObject(this, &AEnemyCharacter::OnDeadStateTagChanged);
	}

	if (IsDead())
	{
		HandleDeath();
	}
	else if (CharacterASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute()) <= 0.0f)
	{
		SetDeadState();
	}
}

void AEnemyCharacter::UnbindDeathEvents()
{
	UAbilitySystemComponent* BoundASC = DeathBoundAbilitySystemComponent.Get();
	if (BoundASC)
	{
		if (HealthAttributeChangedHandle.IsValid())
		{
			BoundASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetHealthAttribute())
				.Remove(HealthAttributeChangedHandle);
		}
		if (DeadStateTagChangedHandle.IsValid() && DeadStateTag.IsValid())
		{
			BoundASC->UnregisterGameplayTagEvent(DeadStateTagChangedHandle, DeadStateTag);
		}
	}

	HealthAttributeChangedHandle.Reset();
	DeadStateTagChangedHandle.Reset();
	DeathBoundAbilitySystemComponent.Reset();
}

void AEnemyCharacter::OnHealthAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	if (ChangeData.NewValue <= 0.0f)
	{
		if (!IsDead())
		{
			SetDeadState();
		}
		return;
	}

	if (!HasAuthority() || ChangeData.NewValue >= ChangeData.OldValue || IsDead() || !ChangeData.GEModData
		|| !HitReactionEventTag.IsValid() || !InterruptReactionDataTag.IsValid())
	{
		return;
	}

	FGameplayTagContainer AssetTags;
	ChangeData.GEModData->EffectSpec.GetAllAssetTags(AssetTags);
	if (!AssetTags.HasTagExact(InterruptReactionDataTag))
	{
		return;
	}

	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent();
	if (!CharacterASC)
	{
		return;
	}

	FGameplayEventData ReactionEventData;
	ReactionEventData.EventTag = HitReactionEventTag;
	ReactionEventData.Instigator = ChangeData.GEModData->EffectSpec.GetContext().GetInstigator();
	ReactionEventData.Target = this;
	ReactionEventData.EventMagnitude = ChangeData.OldValue - ChangeData.NewValue;
	CharacterASC->HandleGameplayEvent(HitReactionEventTag, &ReactionEventData);
}

void AEnemyCharacter::OnDeadStateTagChanged(const FGameplayTag, int32 NewCount)
{
	if (NewCount > 0)
	{
		HandleDeath();
	}
}

void AEnemyCharacter::SetDeadState()
{
	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent();
	if (!HasAuthority() || !CharacterASC || !DeadStateTag.IsValid())
	{
		return;
	}

	CharacterASC->SetLooseGameplayTagCount(DeadStateTag, 1);
}

void AEnemyCharacter::HandleDeath()
{
	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent();
	if (!HasAuthority() || bDeathTeardownStarted || !CharacterASC || !IsDead())
	{
		return;
	}

	bDeathTeardownStarted = true;
	// A Dead Tag granted by any legal source becomes terminal in C2; revival is out of scope.
	CharacterASC->SetLooseGameplayTagCount(DeadStateTag, 1);
	CharacterASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 0.0f);

	if (AEnemyAIController* EnemyAIController = Cast<AEnemyAIController>(GetController()))
	{
		EnemyAIController->HandleControlledEnemyDeath();
	}

	CharacterASC->CancelAllAbilities();

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
		MovementComponent->DisableMovement();
	}

	StartDeathRagdoll();
}

void AEnemyCharacter::StartDeathRagdoll()
{
	if (!bUseRagdollOnDeath || bDeathRagdollStarted)
	{
		return;
	}

	USkeletalMeshComponent* SkeletalMesh = GetMesh();
	if (!SkeletalMesh || !SkeletalMesh->GetPhysicsAsset())
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Enemy '%s' cannot enter death ragdoll: SkeletalMesh and Physics Asset are required."), *GetNameSafe(this));
		return;
	}

	DisableFixedWeaponDisplayCollision();

	UCapsuleComponent* CharacterCapsule = GetCapsuleComponent();
	if (CharacterCapsule)
	{
		CharacterCapsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		CharacterCapsule->SetGenerateOverlapEvents(false);
	}

	SkeletalMesh->SetCollisionProfileName(FName(TEXT("Ragdoll")));
	SkeletalMesh->SetSimulatePhysics(true);
	SkeletalMesh->WakeAllRigidBodies();
	bDeathRagdollStarted = true;
}
