#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
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

/** The authored locomotion presentation mode for a weapon family. */
UENUM(BlueprintType)
enum class EWeaponLocomotionMode : uint8
{
	Default = 0,
	LightSword = 1,
	HeavySword = 2,

	/** Retains Bow's serialized value 4 while filling the index gap for BlendListByEnum. */
	Deprecated_Reserved = 3 UMETA(Hidden),

	Bow = 4
};

/**
 * The authored base of every player equipment item: hand-slot occupancy,
 * display attachment, combat-action candidate ability classes, the default
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

	/** Base locomotion mode authored on this weapon definition. Default for unarmed and generic weapons. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Locomotion")
	EWeaponLocomotionMode LocomotionMode = EWeaponLocomotionMode::Default;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Display")
	TObjectPtr<UStaticMesh> WeaponMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Display")
	FName AttachSocketName = TEXT("Weapon_R");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Display")
	FVector DisplayLocationOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Display")
	FRotator DisplayRotationOffset = FRotator::ZeroRotator;

	/** Candidate grouping only: these ability classes join the same runtime candidate union as ExclusiveCombatActions. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Combat Actions")
	TArray<TSubclassOf<UGameplayAbility>> ReusableCombatActions;

	/** Compat-retained candidate grouping merged into the same runtime candidate union as ReusableCombatActions; no exclusivity rule is implemented in v1 (real exclusive selection belongs to TODO-03D1). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Combat Actions")
	TArray<TSubclassOf<UGameplayAbility>> ExclusiveCombatActions;

	/** The always-granted-while-equipped ability chain (Light/Charged/Sprint Attack per weapon family); validated disjoint from the candidate lists. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Combat Actions")
	TArray<TSubclassOf<UGameplayAbility>> BaseGrantedActions;

	/** The initial 1-4 layout; at most four entries, null entries are legal no-op slots, and duplicate non-null classes are rejected. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Prepared Slots")
	TArray<TSubclassOf<UGameplayAbility>> DefaultPreparedActions;

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

	if (LocomotionMode != EWeaponLocomotionMode::Default
		&& LocomotionMode != EWeaponLocomotionMode::LightSword
		&& LocomotionMode != EWeaponLocomotionMode::HeavySword
		&& LocomotionMode != EWeaponLocomotionMode::Bow)
	{
		OutReason = TEXT("LocomotionMode contains an invalid enum value.");
		return false;
	}

	// Reusable and Exclusive are authoring groupings only; the runtime candidate
	// pool is this one union with no exclusivity rule.
	TSet<TSubclassOf<UGameplayAbility>> CandidateClasses;
	auto AppendCandidateClasses = [&CandidateClasses](const TArray<TSubclassOf<UGameplayAbility>>& Classes, const TCHAR* ListName, FString& Reason) -> bool
	{
		for (const TSubclassOf<UGameplayAbility>& AbilityClass : Classes)
		{
			if (!AbilityClass)
			{
				Reason = FString::Printf(TEXT("%s contains a null entry."), ListName);
				return false;
			}

			if (CandidateClasses.Contains(AbilityClass))
			{
				Reason = FString::Printf(TEXT("%s contains an ability class that already appears in the candidate lists."), ListName);
				return false;
			}

			CandidateClasses.Add(AbilityClass);
		}

		return true;
	};

	if (!AppendCandidateClasses(ReusableCombatActions, TEXT("ReusableCombatActions"), OutReason)
		|| !AppendCandidateClasses(ExclusiveCombatActions, TEXT("ExclusiveCombatActions"), OutReason))
	{
		return false;
	}

	TSet<TSubclassOf<UGameplayAbility>> BaseClasses;
	for (const TSubclassOf<UGameplayAbility>& AbilityClass : BaseGrantedActions)
	{
		if (!AbilityClass)
		{
			OutReason = TEXT("BaseGrantedActions contains a null entry.");
			return false;
		}

		if (BaseClasses.Contains(AbilityClass) || CandidateClasses.Contains(AbilityClass))
		{
			OutReason = TEXT("BaseGrantedActions contains a duplicate ability class or one that is also a combat-action candidate.");
			return false;
		}

		BaseClasses.Add(AbilityClass);
	}

	if (DefaultPreparedActions.Num() > 4)
	{
		OutReason = TEXT("DefaultPreparedActions must hold at most four entries.");
		return false;
	}

	TSet<TSubclassOf<UGameplayAbility>> SeenPreparedClasses;
	for (const TSubclassOf<UGameplayAbility>& PreparedClass : DefaultPreparedActions)
	{
		if (!PreparedClass)
		{
			// A null default entry is a legal no-op prepared slot.
			continue;
		}

		if (!CandidateClasses.Contains(PreparedClass))
		{
			OutReason = TEXT("DefaultPreparedActions contains an ability class outside the weapon's candidate lists.");
			return false;
		}

		if (SeenPreparedClasses.Contains(PreparedClass))
		{
			OutReason = TEXT("DefaultPreparedActions contains a duplicate ability class.");
			return false;
		}

		SeenPreparedClasses.Add(PreparedClass);
	}

	if (DefenseProfile && !DefenseProfile->IsProfileValid())
	{
		OutReason = TEXT("DefenseProfile has an incomplete Guard/Parry tag mapping.");
		return false;
	}

	return true;
}
