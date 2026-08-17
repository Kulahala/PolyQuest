#pragma once

#include "CoreMinimal.h"
#include "Combat/Equipment/WeaponDefinition.h"
#include "OffHandWeaponDefinition.generated.h"

/**
 * A concrete off-hand weapon definition (such as a Shield) occupying EWeaponHandSlot::OffHand.
 * Adds display attachment and slot validation; contains no melee trace markers,
 * damage configuration, or runtime ability state.
 */
UCLASS(BlueprintType)
class POLYQUEST_API UOffHandWeaponDefinition : public UWeaponDefinition
{
	GENERATED_BODY()

public:
	UOffHandWeaponDefinition()
	{
		HandSlot = EWeaponHandSlot::OffHand;
		AttachSocketName = TEXT("Weapon_L");
	}

	virtual bool IsValidWeaponDefinition(FString& OutReason) const override;
};

inline bool UOffHandWeaponDefinition::IsValidWeaponDefinition(FString& OutReason) const
{
	OutReason.Empty();
	if (!Super::IsValidWeaponDefinition(OutReason))
	{
		return false;
	}

	if (HandSlot != EWeaponHandSlot::OffHand)
	{
		OutReason = TEXT("OffHandWeaponDefinition must have HandSlot set to OffHand.");
		return false;
	}

	if (!WeaponMesh)
	{
		OutReason = TEXT("WeaponMesh is not assigned on OffHandWeaponDefinition.");
		return false;
	}

	return true;
}
