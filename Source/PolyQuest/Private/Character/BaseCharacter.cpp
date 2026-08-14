#include "Character/BaseCharacter.h"

#include "AbilitySystem/CharacterAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Combat/Melee/MeleeTraceSourceComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffectTypes.h"
#include "GameFramework/Controller.h"
#include "GameFramework/CharacterMovementComponent.h"

ABaseCharacter::ABaseCharacter()
{
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	Attributes = CreateDefaultSubobject<UCharacterAttributeSet>(TEXT("Attributes"));
	MeleeTraceSource = CreateDefaultSubobject<UMeleeTraceSourceComponent>(TEXT("MeleeTraceSource"));
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Block);
	AbilitySystemComponent->AddAttributeSetSubobject(Attributes.Get());
}

void ABaseCharacter::BeginPlay()
{
	Super::BeginPlay();
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

int32 ABaseCharacter::GetCombatTeamId_Implementation() const
{
	return CombatTeamId;
}

UMeleeTraceSourceComponent* ABaseCharacter::GetMeleeTraceSource() const
{
	return MeleeTraceSource.Get();
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
