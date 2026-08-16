#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Combat/Input/CombatLoadoutDefinition.h"
#include "MeleeWeaponDefinition.generated.h"

class UStaticMesh;

/**
 * One authored melee weapon: display attachment, blade trace markers in
 * weapon-mesh local space, sweep shape, granted abilities, and the optional
 * loadout activated while equipped. Authored data only; all runtime state is
 * owned by UWeaponEquipmentComponent.
 */
UCLASS(BlueprintType)
class POLYQUEST_API UMeleeWeaponDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Validates every authored field; returns false with a focused reason the equipment transaction refuses to mutate past. */
	bool IsValidDefinition(FString& OutReason) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Display")
	TObjectPtr<UStaticMesh> WeaponMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Display")
	FName AttachSocketName = TEXT("Weapon_R");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Display")
	FVector DisplayLocationOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Display")
	FRotator DisplayRotationOffset = FRotator::ZeroRotator;

	/** Blade-root marker spawned relative to the display mesh. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Trace Markers")
	FVector BladeBaseMarkerRelativeLocation = FVector::ZeroVector;

	/** Blade-tip marker spawned relative to the display mesh; must differ from the base marker. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Trace Markers")
	FVector BladeTipMarkerRelativeLocation = FVector::ZeroVector;

	/** Sweep sphere radius for this weapon's melee trace. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Trace", meta = (ClampMin = "0.01"))
	float TraceRadius = 12.0f;

	/** Sphere-sweep samples along this weapon's blade. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Trace", meta = (ClampMin = "1", ClampMax = "8"))
	int32 BladeSubdivisions = 4;

	/** Ability classes granted while this weapon is equipped; duplicates against StartupAbilities are rejected by the equipment preflight. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Abilities")
	TArray<TSubclassOf<UGameplayAbility>> GrantedWeaponAbilities;

	/** Optional loadout activated on equip; null keeps the currently active loadout. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Loadout")
	TObjectPtr<UCombatLoadoutDefinition> AssociatedLoadout;
};

inline bool UMeleeWeaponDefinition::IsValidDefinition(FString& OutReason) const
{
	if (!WeaponMesh)
	{
		OutReason = TEXT("WeaponMesh is not assigned.");
		return false;
	}

	if (AttachSocketName.IsNone())
	{
		OutReason = TEXT("AttachSocketName is not set.");
		return false;
	}

	if (BladeBaseMarkerRelativeLocation.Equals(BladeTipMarkerRelativeLocation, KINDA_SMALL_NUMBER))
	{
		OutReason = TEXT("BladeBaseMarkerRelativeLocation and BladeTipMarkerRelativeLocation must be distinct positions.");
		return false;
	}

	if (TraceRadius <= 0.0f)
	{
		OutReason = TEXT("TraceRadius must be positive.");
		return false;
	}

	if (BladeSubdivisions < 1 || BladeSubdivisions > 8)
	{
		OutReason = TEXT("BladeSubdivisions must be between 1 and 8.");
		return false;
	}

	TSet<TSubclassOf<UGameplayAbility>> SeenAbilityClasses;
	for (const TSubclassOf<UGameplayAbility>& AbilityClass : GrantedWeaponAbilities)
	{
		if (!AbilityClass)
		{
			OutReason = TEXT("GrantedWeaponAbilities contains a null entry.");
			return false;
		}

		if (SeenAbilityClasses.Contains(AbilityClass))
		{
			OutReason = TEXT("GrantedWeaponAbilities contains a duplicate ability class.");
			return false;
		}

		SeenAbilityClasses.Add(AbilityClass);
	}

	if (AssociatedLoadout && !AssociatedLoadout->IsRouteTableValid())
	{
		OutReason = TEXT("AssociatedLoadout has an invalid route table.");
		return false;
	}

	OutReason.Empty();
	return true;
}
