#include "Character/BaseCharacter.h"

#include "AbilitySystem/CharacterAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayAbilitySpec.h"
#include "GameFramework/Controller.h"

ABaseCharacter::ABaseCharacter()
{
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	Attributes = CreateDefaultSubobject<UCharacterAttributeSet>(TEXT("Attributes"));
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
