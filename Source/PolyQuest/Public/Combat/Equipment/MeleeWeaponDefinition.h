#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Combat/Equipment/WeaponDefinition.h"
#include "Combat/Input/CombatLoadoutDefinition.h"
#include "MeleeWeaponDefinition.generated.h"

class UGameplayAbility;

/**
 * The compatible melee subclass of UWeaponDefinition: blade trace markers in
 * weapon-mesh local space, sweep shape, and the BaseGrantedAbilities source
 * the current LMB/Sprint base chain relies on. Display, slot, action, Defense
 * Profile, and Base Input Profile fields are owned by the base class; the
 * promoted field names keep the serialized values of the existing DataAssets.
 */
UCLASS(BlueprintType)
class POLYQUEST_API UMeleeWeaponDefinition : public UWeaponDefinition
{
	GENERATED_BODY()

public:
	virtual bool IsValidWeaponDefinition(FString& OutReason) const override;

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

	/**
	 * BaseGrantedAbilities source: granted while equipped alongside prepared
	 * action grants, with duplicate classes rejected. The TODO-03A authoring
	 * relies on it for Light/Charged/Sprint Attack; removal requires the
	 * TODO-03A2 migration plus a zero-referencer scan and Editor readback.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Abilities")
	TArray<TSubclassOf<UGameplayAbility>> GrantedWeaponAbilities;
};

inline bool UMeleeWeaponDefinition::IsValidWeaponDefinition(FString& OutReason) const
{
	OutReason.Empty();
	if (!Super::IsValidWeaponDefinition(OutReason))
	{
		return false;
	}

	if (!WeaponMesh)
	{
		OutReason = TEXT("WeaponMesh is not assigned.");
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

	return true;
}
