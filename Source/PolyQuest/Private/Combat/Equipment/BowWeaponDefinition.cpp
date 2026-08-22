#include "Combat/Equipment/BowWeaponDefinition.h"

#include "AbilitySystem/Abilities/BowDrawFireAbility.h"
#include "AbilitySystem/Abilities/PrimaryAttackAbility.h"
#include "Combat/Equipment/ProjectileDefinition.h"
#include "Combat/Input/CombatLoadoutDefinition.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "GameplayTagContainer.h"

UBowWeaponDefinition::UBowWeaponDefinition()
{
	HandSlot = EWeaponHandSlot::MainHandTwoHanded;
	AttachSocketName = TEXT("Bow_L");
	LaunchSocketName = TEXT("Socket_Arrow");
	LocomotionMode = EWeaponLocomotionMode::Bow;
}

bool UBowWeaponDefinition::IsValidWeaponDefinition(FString& OutReason) const
{
	if (!Super::IsValidWeaponDefinition(OutReason))
	{
		return false;
	}

	if (LocomotionMode != EWeaponLocomotionMode::Bow)
	{
		OutReason = TEXT("Bow weapon definition must have LocomotionMode set to Bow.");
		return false;
	}

	if (HandSlot != EWeaponHandSlot::MainHandTwoHanded)
	{
		OutReason = TEXT("Bow weapon definition must use MainHandTwoHanded slot.");
		return false;
	}

	if (!WeaponMesh)
	{
		OutReason = TEXT("Bow weapon definition requires a valid WeaponMesh.");
		return false;
	}

	if (LaunchSocketName.IsNone())
	{
		OutReason = TEXT("LaunchSocketName is not set.");
		return false;
	}

	if (!WeaponMesh->FindSocket(LaunchSocketName))
	{
		OutReason = FString::Printf(TEXT("WeaponMesh '%s' does not contain launch socket '%s'."),
			*GetNameSafe(WeaponMesh), *LaunchSocketName.ToString());
		return false;
	}

	if (!DefaultProjectileDefinition)
	{
		OutReason = TEXT("DefaultProjectileDefinition is null.");
		return false;
	}

	FString ProjectileReason;
	if (!DefaultProjectileDefinition->IsValidProjectileDefinition(ProjectileReason))
	{
		OutReason = FString::Printf(TEXT("DefaultProjectileDefinition is invalid: %s"), *ProjectileReason);
		return false;
	}

	if (!AssociatedLoadout)
	{
		OutReason = TEXT("AssociatedLoadout is required for bow weapon definition.");
		return false;
	}

	const FGameplayTag PrimaryAttackInputTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false);
	const FGameplayTag PrimaryAttackAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Primary")), false);

	if (!PrimaryAttackInputTag.IsValid() || !PrimaryAttackAbilityTag.IsValid())
	{
		OutReason = TEXT("Required Input.PrimaryAttack or Ability.Attack.Primary gameplay tags are not registered.");
		return false;
	}

	FGameplayTag ResolvedAbilityTag;
	if (!AssociatedLoadout->TryGetAbilityTagForInputIntent(PrimaryAttackInputTag, ResolvedAbilityTag)
		|| ResolvedAbilityTag != PrimaryAttackAbilityTag)
	{
		OutReason = FString::Printf(TEXT("AssociatedLoadout must map '%s' to '%s'."),
			*PrimaryAttackInputTag.ToString(), *PrimaryAttackAbilityTag.ToString());
		return false;
	}

	// Bow requires exactly one BaseGrantedAction whose CDO carries Ability.Attack.Primary,
	// and it must be UBowDrawFireAbility or its subclass (rejecting UPrimaryAttackAbility or unrelated Primary abilities).
	int32 PrimaryAbilityGrantCount = 0;
	for (const TSubclassOf<UGameplayAbility>& ActionClass : BaseGrantedActions)
	{
		if (!ActionClass)
		{
			continue;
		}

		if (ActionClass->IsChildOf(UPrimaryAttackAbility::StaticClass()))
		{
			OutReason = TEXT("Bow weapon definition must not grant UPrimaryAttackAbility (melee arbitration ability).");
			return false;
		}

		const UGameplayAbility* AbilityCDO = ActionClass.GetDefaultObject();
		if (AbilityCDO && AbilityCDO->AbilityTags.HasTag(PrimaryAttackAbilityTag))
		{
			if (!ActionClass->IsChildOf(UBowDrawFireAbility::StaticClass()))
			{
				OutReason = FString::Printf(TEXT("Bow weapon definition primary attack action '%s' must derive from UBowDrawFireAbility."),
					*ActionClass->GetName());
				return false;
			}
			PrimaryAbilityGrantCount++;
		}
	}

	if (PrimaryAbilityGrantCount != 1)
	{
		OutReason = FString::Printf(TEXT("Bow weapon definition must have exactly one BaseGrantedAction matching '%s' (found %d)."),
			*PrimaryAttackAbilityTag.ToString(), PrimaryAbilityGrantCount);
		return false;
	}

	return true;
}
