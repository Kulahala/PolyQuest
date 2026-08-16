#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Combat/Equipment/CombatActionDefinition.h"
#include "Combat/Equipment/DefenseProfileDefinition.h"
#include "WeaponDefinition.generated.h"

class UCombatLoadoutDefinition;
class UStaticMesh;

/** Which hand slot one authored weapon definition occupies. */
UENUM(BlueprintType)
enum class EWeaponHandSlot : uint8
{
	/** A one-handed item carried in the main hand; leaves the off-hand free. */
	MainHandOneHanded,
	/** A two-handed main-hand item that atomically reserves the off-hand (Bow, Staff, Greatsword). */
	MainHandTwoHanded,
	/** An off-hand item (Shield); it may not coexist with a TwoHanded main hand. */
	OffHand
};

/**
 * The authored base of every player equipment item: hand-slot occupancy,
 * display attachment, compatible combat-action candidates, the default
 * prepared layout, the optional Defense Profile, and the Base Input Profile.
 * Subclasses add combat geometry; this base owns no runtime state.
 */
UCLASS(Abstract)
class POLYQUEST_API UWeaponDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Validates slot/display/composition rules; subclasses extend with their combat geometry. */
	virtual bool IsValidWeaponDefinition(FString& OutReason) const;

	/** Which hand slot this item occupies when equipped. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Slot")
	EWeaponHandSlot HandSlot = EWeaponHandSlot::MainHandOneHanded;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Display")
	TObjectPtr<UStaticMesh> WeaponMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Display")
	FName AttachSocketName = TEXT("Weapon_R");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Display")
	FVector DisplayLocationOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Display")
	FRotator DisplayRotationOffset = FRotator::ZeroRotator;

	/** Combat actions equippable repeatedly alongside other actions. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Combat Actions")
	TArray<TObjectPtr<UCombatActionDefinition>> ReusableCombatActions;

	/** Combat actions that exclude every other action of this weapon. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Combat Actions")
	TArray<TObjectPtr<UCombatActionDefinition>> ExclusiveCombatActions;

	/** The initial 1-4 layout; empty entries are legal no-op slots and the count is validated to at most four. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Prepared Slots")
	TArray<TObjectPtr<UCombatActionDefinition>> DefaultPreparedActions;

	/** Optional Defense Profile; effective as fallback from the main hand and as override from the off hand. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Defense")
	TObjectPtr<UDefenseProfileDefinition> DefenseProfile;

	/** The Base Input Profile consumed by the main hand (TODO-03A field name retained for asset compatibility). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Loadout")
	TObjectPtr<UCombatLoadoutDefinition> AssociatedLoadout;
};

inline bool UWeaponDefinition::IsValidWeaponDefinition(FString& OutReason) const
{
	OutReason.Empty();

	if (AttachSocketName.IsNone())
	{
		OutReason = TEXT("AttachSocketName is not set.");
		return false;
	}

	TArray<UCombatActionDefinition*> CandidateActions;
	for (const TObjectPtr<UCombatActionDefinition>& Action : ReusableCombatActions)
	{
		CandidateActions.Add(Action.Get());
	}
	for (const TObjectPtr<UCombatActionDefinition>& Action : ExclusiveCombatActions)
	{
		CandidateActions.Add(Action.Get());
	}

	TSet<UCombatActionDefinition*> SeenActions;
	for (UCombatActionDefinition* Candidate : CandidateActions)
	{
		if (!Candidate)
		{
			OutReason = TEXT("the combat-action lists contain a null entry.");
			return false;
		}

		if (SeenActions.Contains(Candidate))
		{
			OutReason = TEXT("the combat-action lists contain a duplicate action.");
			return false;
		}

		SeenActions.Add(Candidate);
	}

	if (DefaultPreparedActions.Num() > 4)
	{
		OutReason = TEXT("DefaultPreparedActions must hold at most four entries.");
		return false;
	}

	for (const TObjectPtr<UCombatActionDefinition>& PreparedAction : DefaultPreparedActions)
	{
		if (PreparedAction && !SeenActions.Contains(PreparedAction.Get()))
		{
			OutReason = TEXT("DefaultPreparedActions contains an action outside the weapon's candidate lists.");
			return false;
		}
	}

	if (DefenseProfile && !DefenseProfile->IsProfileValid())
	{
		OutReason = TEXT("DefenseProfile has an incomplete Guard/Parry tag mapping.");
		return false;
	}

	return true;
}
