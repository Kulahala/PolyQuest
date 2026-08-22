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

	/** Required main-hand locomotion mode for this off-hand's composition override to activate. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Locomotion")
	EWeaponLocomotionMode RequiredMainHandLocomotionMode = EWeaponLocomotionMode::Default;

	/** Locomotion mode applied when paired with a matching main hand. Default means no composition override. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Locomotion")
	EWeaponLocomotionMode CompositionLocomotionMode = EWeaponLocomotionMode::Default;
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

	if (LocomotionMode != EWeaponLocomotionMode::Default)
	{
		OutReason = TEXT("OffHandWeaponDefinition base LocomotionMode must be Default (base locomotion mode belongs to MainHand weapons only; use CompositionLocomotionMode for off-hand overrides).");
		return false;
	}

	if (!WeaponMesh)
	{
		OutReason = TEXT("WeaponMesh is not assigned on OffHandWeaponDefinition.");
		return false;
	}

	const bool bNoOverride = (RequiredMainHandLocomotionMode == EWeaponLocomotionMode::Default && CompositionLocomotionMode == EWeaponLocomotionMode::Default);
	const bool bValidSwordShield = (RequiredMainHandLocomotionMode == EWeaponLocomotionMode::LightSword && CompositionLocomotionMode == EWeaponLocomotionMode::SwordShield);
	if (!bNoOverride && !bValidSwordShield)
	{
		OutReason = TEXT("OffHandWeaponDefinition locomotion override must be either both Default (no override) or RequiredMainHandLocomotionMode=LightSword with CompositionLocomotionMode=SwordShield.");
		return false;
	}

	return true;
}
