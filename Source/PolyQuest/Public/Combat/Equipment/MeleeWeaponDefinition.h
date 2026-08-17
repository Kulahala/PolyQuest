#pragma once

#include "CoreMinimal.h"
#include "Combat/Equipment/WeaponDefinition.h"
#include "Combat/Input/CombatLoadoutDefinition.h"
#include "MeleeWeaponDefinition.generated.h"

/**
 * The compatible melee subclass of UWeaponDefinition: blade trace markers in
 * weapon-mesh or owner-socket local space, sweep shape, and the BaseGrantedActions
 * source the current LMB/Sprint base chain relies on. Display, slot, action, Defense
 * Profile, and Base Input Profile fields are owned by the base class; the promoted
 * field names keep the serialized values of the existing DataAssets.
 */
UCLASS(BlueprintType)
class POLYQUEST_API UMeleeWeaponDefinition : public UWeaponDefinition
{
	GENERATED_BODY()

public:
	virtual bool IsValidWeaponDefinition(FString& OutReason) const override;

	/**
	 * Trace markers resolve against the character SkeletalMesh socket named by
	 * AttachSocketName instead of a spawned weapon display: the Unarmed
	 * hand-contact source. Bidirectionally validated against WeaponMesh so the
	 * two contact sources are never combined or both omitted.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Trace Markers")
	bool bUseOwnerMeshSocketForTrace = false;

	/** Blade-root marker spawned relative to the display mesh, or to the owner-mesh socket when bUseOwnerMeshSocketForTrace is set. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Trace Markers")
	FVector BladeBaseMarkerRelativeLocation = FVector::ZeroVector;

	/** Blade-tip marker spawned relative to the display mesh, or to the owner-mesh socket when bUseOwnerMeshSocketForTrace is set; must differ from the base marker. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Trace Markers")
	FVector BladeTipMarkerRelativeLocation = FVector::ZeroVector;

	/** Sweep sphere radius for this weapon's melee trace. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Trace", meta = (ClampMin = "0.01"))
	float TraceRadius = 12.0f;

	/** Sphere-sweep samples along this weapon's blade. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Trace", meta = (ClampMin = "1", ClampMax = "8"))
	int32 BladeSubdivisions = 4;

};

inline bool UMeleeWeaponDefinition::IsValidWeaponDefinition(FString& OutReason) const
{
	OutReason.Empty();
	if (!Super::IsValidWeaponDefinition(OutReason))
	{
		return false;
	}

	if (bUseOwnerMeshSocketForTrace && WeaponMesh)
	{
		OutReason = TEXT("bUseOwnerMeshSocketForTrace cannot be combined with a WeaponMesh; the trace source would be ambiguous.");
		return false;
	}

	if (!bUseOwnerMeshSocketForTrace && !WeaponMesh)
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

	if (AssociatedLoadout && !AssociatedLoadout->IsRouteTableValid())
	{
		OutReason = TEXT("AssociatedLoadout has an invalid route table.");
		return false;
	}

	return true;
}
