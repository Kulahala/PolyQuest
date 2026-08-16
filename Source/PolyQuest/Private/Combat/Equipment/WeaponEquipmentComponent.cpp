#include "Combat/Equipment/WeaponEquipmentComponent.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Character/Player/PlayerCharacter.h"
#include "Combat/Equipment/MeleeWeaponDefinition.h"
#include "Combat/Input/CombatLoadoutDefinition.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameplayAbilitySpec.h"
#include "PolyQuest.h"

UWeaponEquipmentComponent::UWeaponEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);

	AttackingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Attacking")), false);
	GuardingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Guarding")), false);
	ParryingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Parrying")), false);
	DodgingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Dodging")), false);
	PrimaryAttackAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Primary")), false);
}

bool UWeaponEquipmentComponent::EquipWeapon(UMeleeWeaponDefinition* Definition)
{
	if (!Definition)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Weapon equipment on '%s' rejected a null weapon definition."), *GetNameSafe(GetOwner()));
		return false;
	}

	if (CurrentWeapon == Definition)
	{
		return true;
	}

	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetOwner());
	UAbilitySystemComponent* CharacterASC = PlayerCharacter ? PlayerCharacter->GetAbilitySystemComponent() : nullptr;
	if (!CanSwapNow(CharacterASC))
	{
		if (!bSwapRefusalWarningIssued)
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("Weapon equipment on '%s' refused a swap while a combat action is active."), *GetNameSafe(GetOwner()));
			bSwapRefusalWarningIssued = true;
		}
		return false;
	}

	FString PreflightReason;
	if (!RunPreflight(PlayerCharacter, CharacterASC, Definition, PreflightReason))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Weapon equipment on '%s' failed its preflight: %s"), *GetNameSafe(GetOwner()), *PreflightReason);
		return false;
	}

	// Snapshot before any mutation; the restore path rebuilds exactly this state.
	UMeleeWeaponDefinition* OldWeapon = CurrentWeapon;
	UCombatLoadoutDefinition* OldLoadout = PlayerCharacter ? PlayerCharacter->GetActiveCombatLoadout() : nullptr;

	TeardownEquippedWeapon();

	if (!ApplyEquippedWeapon(PlayerCharacter, CharacterASC, Definition))
	{
		// The failed apply has cleaned its own partial state; the loadout was
		// never changed because its activation is the final apply step.
		if (OldWeapon)
		{
			if (ApplyEquippedWeapon(PlayerCharacter, CharacterASC, OldWeapon)
				&& (!OldLoadout || PlayerCharacter->SetActiveCombatLoadout(OldLoadout)))
			{
				UE_LOG(LogPolyQuest, Warning, TEXT("Weapon equipment on '%s' failed mid-apply and restored the previous weapon."), *GetNameSafe(GetOwner()));
				return false;
			}
		}

		UE_LOG(LogPolyQuest, Error, TEXT("Weapon equipment on '%s' failed mid-apply and could not restore a previous weapon; the player has no valid melee weapon. This is a fatal configuration error."), *GetNameSafe(GetOwner()));
		return false;
	}

	bSwapRefusalWarningIssued = false;
	return true;
}

bool UWeaponEquipmentComponent::TryGetBladeMarkers(USceneComponent*& OutBladeBase, USceneComponent*& OutBladeTip) const
{
	OutBladeBase = EquippedBladeBaseMarker.Get();
	OutBladeTip = EquippedBladeTipMarker.Get();
	return CurrentWeapon && OutBladeBase && OutBladeTip;
}

void UWeaponEquipmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	TeardownEquippedWeapon();
	Super::EndPlay(EndPlayReason);
}

bool UWeaponEquipmentComponent::CanSwapNow(const UAbilitySystemComponent* CharacterASC)
{
	if (!CharacterASC
		|| (AttackingStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(AttackingStateTag))
		|| (GuardingStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(GuardingStateTag))
		|| (ParryingStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(ParryingStateTag))
		|| (DodgingStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(DodgingStateTag)))
	{
		return false;
	}

	// The Primary hold/release arbitration window owns only input-block tags, not a
	// combat action tag, so an active Primary spec is checked directly to keep a
	// swap from re-routing an in-flight attack decision.
	if (PrimaryAttackAbilityTag.IsValid())
	{
		FGameplayTagContainer PrimaryAbilityTags;
		PrimaryAbilityTags.AddTag(PrimaryAttackAbilityTag);
		TArray<FGameplayAbilitySpec*> PrimaryAbilitySpecs;
		CharacterASC->GetActivatableGameplayAbilitySpecsByAllMatchingTags(PrimaryAbilityTags, PrimaryAbilitySpecs, false);
		for (const FGameplayAbilitySpec* PrimaryAbilitySpec : PrimaryAbilitySpecs)
		{
			if (PrimaryAbilitySpec && PrimaryAbilitySpec->IsActive())
			{
				return false;
			}
		}
	}

	return true;
}

bool UWeaponEquipmentComponent::RunPreflight(APlayerCharacter* PlayerCharacter, UAbilitySystemComponent* CharacterASC, const UMeleeWeaponDefinition* Definition, FString& OutReason) const
{
	OutReason.Empty();
	if (!Definition->IsValidDefinition(OutReason))
	{
		return false;
	}

	if (!PlayerCharacter)
	{
		OutReason = TEXT("the owning actor is not an APlayerCharacter.");
		return false;
	}

	const USkeletalMeshComponent* OwnerMesh = PlayerCharacter->GetMesh();
	if (!OwnerMesh)
	{
		OutReason = TEXT("the owner has no skeletal mesh.");
		return false;
	}

	if (!OwnerMesh->DoesSocketExist(Definition->AttachSocketName))
	{
		OutReason = FString::Printf(TEXT("the owner mesh does not contain socket '%s'."), *Definition->AttachSocketName.ToString());
		return false;
	}

	if (!CharacterASC || !PlayerCharacter->HasAuthority())
	{
		OutReason = TEXT("the owner has no authoritative Ability System Component.");
		return false;
	}

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : Definition->GrantedWeaponAbilities)
	{
		if (PlayerCharacter->IsStartupAbilityClass(AbilityClass))
		{
			OutReason = FString::Printf(TEXT("ability '%s' is also granted by the character's StartupAbilities."), *GetNameSafe(AbilityClass));
			return false;
		}

		// Enumerate every matching spec: FindAbilitySpecFromClass stops at the first
		// same-class spec, so a foreign spec could hide behind one this component
		// granted and is about to revoke in the swap.
		const UGameplayAbility* AbilityCDO = AbilityClass.GetDefaultObject();
		bool bForeignSpecExists = false;
		for (const FGameplayAbilitySpec& ExistingSpec : CharacterASC->GetActivatableAbilities())
		{
			if (ExistingSpec.Ability == AbilityCDO && !GrantedAbilitySpecHandles.Contains(ExistingSpec.Handle))
			{
				bForeignSpecExists = true;
				break;
			}
		}

		if (bForeignSpecExists)
		{
			OutReason = FString::Printf(TEXT("ability '%s' already exists on the ASC outside this weapon's granted handles."), *GetNameSafe(AbilityClass));
			return false;
		}
	}

	return true;
}

void UWeaponEquipmentComponent::TeardownEquippedWeapon()
{
	if (UAbilitySystemComponent* CharacterASC = GetOwner() ? GetOwner()->FindComponentByClass<UAbilitySystemComponent>() : nullptr)
	{
		for (const FGameplayAbilitySpecHandle& GrantedHandle : GrantedAbilitySpecHandles)
		{
			CharacterASC->ClearAbility(GrantedHandle);
		}
	}
	GrantedAbilitySpecHandles.Reset();

	if (EquippedBladeBaseMarker)
	{
		EquippedBladeBaseMarker->DestroyComponent();
		EquippedBladeBaseMarker = nullptr;
	}

	if (EquippedBladeTipMarker)
	{
		EquippedBladeTipMarker->DestroyComponent();
		EquippedBladeTipMarker = nullptr;
	}

	if (EquippedDisplayComponent)
	{
		EquippedDisplayComponent->DestroyComponent();
		EquippedDisplayComponent = nullptr;
	}

	CurrentWeapon = nullptr;
}

bool UWeaponEquipmentComponent::ApplyEquippedWeapon(APlayerCharacter* PlayerCharacter, UAbilitySystemComponent* CharacterASC, UMeleeWeaponDefinition* Definition)
{
	USkeletalMeshComponent* OwnerMesh = PlayerCharacter->GetMesh();

	UStaticMeshComponent* DisplayComponent = NewObject<UStaticMeshComponent>(PlayerCharacter, NAME_None, RF_Transient);
	DisplayComponent->SetStaticMesh(Definition->WeaponMesh);
	DisplayComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DisplayComponent->SetGenerateOverlapEvents(false);
	DisplayComponent->RegisterComponent();
	DisplayComponent->AttachToComponent(OwnerMesh, FAttachmentTransformRules::KeepRelativeTransform, Definition->AttachSocketName);
	DisplayComponent->SetRelativeLocation(Definition->DisplayLocationOffset);
	DisplayComponent->SetRelativeRotation(Definition->DisplayRotationOffset);

	USceneComponent* BladeBaseMarker = NewObject<USceneComponent>(PlayerCharacter, NAME_None, RF_Transient);
	BladeBaseMarker->RegisterComponent();
	BladeBaseMarker->AttachToComponent(DisplayComponent, FAttachmentTransformRules::KeepRelativeTransform);
	BladeBaseMarker->SetRelativeLocation(Definition->BladeBaseMarkerRelativeLocation);

	USceneComponent* BladeTipMarker = NewObject<USceneComponent>(PlayerCharacter, NAME_None, RF_Transient);
	BladeTipMarker->RegisterComponent();
	BladeTipMarker->AttachToComponent(DisplayComponent, FAttachmentTransformRules::KeepRelativeTransform);
	BladeTipMarker->SetRelativeLocation(Definition->BladeTipMarkerRelativeLocation);

	TArray<FGameplayAbilitySpecHandle> NewGrantedHandles;
	for (const TSubclassOf<UGameplayAbility>& AbilityClass : Definition->GrantedWeaponAbilities)
	{
		const FGameplayAbilitySpecHandle GrantedHandle = CharacterASC->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, PlayerCharacter));
		if (!GrantedHandle.IsValid())
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("Weapon equipment on '%s' could not grant ability '%s'."), *GetNameSafe(GetOwner()), *GetNameSafe(AbilityClass));

			for (const FGameplayAbilitySpecHandle& RollbackHandle : NewGrantedHandles)
			{
				CharacterASC->ClearAbility(RollbackHandle);
			}
			BladeBaseMarker->DestroyComponent();
			BladeTipMarker->DestroyComponent();
			DisplayComponent->DestroyComponent();
			return false;
		}

		NewGrantedHandles.Add(GrantedHandle);
	}

	CurrentWeapon = Definition;
	EquippedDisplayComponent = DisplayComponent;
	EquippedBladeBaseMarker = BladeBaseMarker;
	EquippedBladeTipMarker = BladeTipMarker;
	GrantedAbilitySpecHandles = MoveTemp(NewGrantedHandles);

	if (Definition->AssociatedLoadout && !PlayerCharacter->SetActiveCombatLoadout(Definition->AssociatedLoadout))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Weapon equipment on '%s' could not activate the weapon's associated loadout."), *GetNameSafe(GetOwner()));
		TeardownEquippedWeapon();
		return false;
	}

	return true;
}
