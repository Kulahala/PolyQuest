// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/Equipment/MeleeWeaponDefinition.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"

bool UMeleeWeaponDefinition::UsesDisplayMeshTraceSockets() const
{
	return !bUseOwnerMeshSocketForTrace && !BladeBaseSocketName.IsNone() && !BladeTipSocketName.IsNone();
}

bool UMeleeWeaponDefinition::IsValidWeaponDefinition(FString& OutReason) const
{
	OutReason.Empty();
	if (!Super::IsValidWeaponDefinition(OutReason))
	{
		return false;
	}

	if (bUseOwnerMeshSocketForTrace)
	{
		if (WeaponMesh)
		{
			OutReason = TEXT("bUseOwnerMeshSocketForTrace cannot be combined with a WeaponMesh; the trace source would be ambiguous.");
			return false;
		}

		if (!BladeBaseSocketName.IsNone() || !BladeTipSocketName.IsNone())
		{
			OutReason = TEXT("bUseOwnerMeshSocketForTrace cannot be combined with Static Mesh socket names.");
			return false;
		}

		if (BladeBaseMarkerRelativeLocation.Equals(BladeTipMarkerRelativeLocation, KINDA_SMALL_NUMBER))
		{
			OutReason = TEXT("BladeBaseMarkerRelativeLocation and BladeTipMarkerRelativeLocation must be distinct positions.");
			return false;
		}
	}
	else
	{
		if (!WeaponMesh)
		{
			OutReason = TEXT("WeaponMesh is not assigned.");
			return false;
		}

		const bool bHasBaseSocket = !BladeBaseSocketName.IsNone();
		const bool bHasTipSocket = !BladeTipSocketName.IsNone();

		if (bHasBaseSocket != bHasTipSocket)
		{
			OutReason = TEXT("BladeBaseSocketName and BladeTipSocketName must either both be set or both be None.");
			return false;
		}

		if (bHasBaseSocket && bHasTipSocket)
		{
			if (BladeBaseSocketName == BladeTipSocketName)
			{
				OutReason = TEXT("BladeBaseSocketName and BladeTipSocketName must be distinct socket names.");
				return false;
			}

			const UStaticMeshSocket* BaseSocket = WeaponMesh->FindSocket(BladeBaseSocketName);
			if (!BaseSocket)
			{
				OutReason = FString::Printf(TEXT("WeaponMesh '%s' does not contain socket '%s' for blade base."), *GetNameSafe(WeaponMesh), *BladeBaseSocketName.ToString());
				return false;
			}

			const UStaticMeshSocket* TipSocket = WeaponMesh->FindSocket(BladeTipSocketName);
			if (!TipSocket)
			{
				OutReason = FString::Printf(TEXT("WeaponMesh '%s' does not contain socket '%s' for blade tip."), *GetNameSafe(WeaponMesh), *BladeTipSocketName.ToString());
				return false;
			}

			if (BaseSocket->RelativeLocation.Equals(TipSocket->RelativeLocation, KINDA_SMALL_NUMBER))
			{
				OutReason = FString::Printf(TEXT("BladeBaseSocket '%s' and BladeTipSocket '%s' on WeaponMesh '%s' must not have identical RelativeLocations."), *BladeBaseSocketName.ToString(), *BladeTipSocketName.ToString(), *GetNameSafe(WeaponMesh));
				return false;
			}
		}
		else
		{
			if (BladeBaseMarkerRelativeLocation.Equals(BladeTipMarkerRelativeLocation, KINDA_SMALL_NUMBER))
			{
				OutReason = TEXT("BladeBaseMarkerRelativeLocation and BladeTipMarkerRelativeLocation must be distinct positions.");
				return false;
			}
		}
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
