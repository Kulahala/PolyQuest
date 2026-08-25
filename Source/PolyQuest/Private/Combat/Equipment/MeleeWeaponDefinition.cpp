// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/Equipment/MeleeWeaponDefinition.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"

bool UMeleeWeaponDefinition::UsesDisplayMeshTraceSockets() const
{
	return !bUseOwnerMeshSocketForTrace && !BladeBaseSocketName.IsNone() && !BladeTipSocketName.IsNone();
}

bool UMeleeWeaponDefinition::TryResolveTraceSourceName(FName RequestedSourceName, FName& OutResolvedSourceName) const
{
	OutResolvedSourceName = NAME_None;

	if (OwnerMeshTraceSources.Num() == 0)
	{
		if (RequestedSourceName.IsNone() || RequestedSourceName == DefaultOwnerMeshTraceSourceName)
		{
			OutResolvedSourceName = NAME_None;
			return true;
		}
		return false;
	}

	if (RequestedSourceName.IsNone())
	{
		if (!DefaultOwnerMeshTraceSourceName.IsNone())
		{
			OutResolvedSourceName = DefaultOwnerMeshTraceSourceName;
			return true;
		}
		return false;
	}

	for (const FOwnerMeshMeleeTraceSource& Source : OwnerMeshTraceSources)
	{
		if (Source.TraceSourceName == RequestedSourceName)
		{
			OutResolvedSourceName = Source.TraceSourceName;
			return true;
		}
	}

	return false;
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

		if (OwnerMeshTraceSources.Num() > 0)
		{
			if (DefaultOwnerMeshTraceSourceName.IsNone())
			{
				OutReason = TEXT("DefaultOwnerMeshTraceSourceName must be set when OwnerMeshTraceSources is non-empty.");
				return false;
			}

			bool bFoundDefault = false;
			TSet<FName> SeenSourceNames;
			for (const FOwnerMeshMeleeTraceSource& Source : OwnerMeshTraceSources)
			{
				if (Source.TraceSourceName.IsNone())
				{
					OutReason = TEXT("OwnerMeshTraceSources entry has None TraceSourceName.");
					return false;
				}

				if (SeenSourceNames.Contains(Source.TraceSourceName))
				{
					OutReason = FString::Printf(TEXT("OwnerMeshTraceSources contains duplicate TraceSourceName '%s'."), *Source.TraceSourceName.ToString());
					return false;
				}
				SeenSourceNames.Add(Source.TraceSourceName);

				if (Source.OwnerMeshSocketName.IsNone())
				{
					OutReason = FString::Printf(TEXT("OwnerMeshTraceSources entry '%s' has None OwnerMeshSocketName."), *Source.TraceSourceName.ToString());
					return false;
				}

				if (!FMath::IsFinite(Source.BladeBaseMarkerRelativeLocation.X) || !FMath::IsFinite(Source.BladeBaseMarkerRelativeLocation.Y) || !FMath::IsFinite(Source.BladeBaseMarkerRelativeLocation.Z) ||
					!FMath::IsFinite(Source.BladeTipMarkerRelativeLocation.X) || !FMath::IsFinite(Source.BladeTipMarkerRelativeLocation.Y) || !FMath::IsFinite(Source.BladeTipMarkerRelativeLocation.Z))
				{
					OutReason = FString::Printf(TEXT("OwnerMeshTraceSources entry '%s' marker locations must be finite."), *Source.TraceSourceName.ToString());
					return false;
				}

				if (Source.BladeBaseMarkerRelativeLocation.Equals(Source.BladeTipMarkerRelativeLocation, KINDA_SMALL_NUMBER))
				{
					OutReason = FString::Printf(TEXT("OwnerMeshTraceSources entry '%s' BladeBaseMarkerRelativeLocation and BladeTipMarkerRelativeLocation must be distinct positions."), *Source.TraceSourceName.ToString());
					return false;
				}

				if (Source.TraceSourceName == DefaultOwnerMeshTraceSourceName)
				{
					bFoundDefault = true;
				}
			}

			if (!bFoundDefault)
			{
				OutReason = FString::Printf(TEXT("DefaultOwnerMeshTraceSourceName '%s' does not match any entry in OwnerMeshTraceSources."), *DefaultOwnerMeshTraceSourceName.ToString());
				return false;
			}
		}
		else
		{
			if (!DefaultOwnerMeshTraceSourceName.IsNone())
			{
				OutReason = TEXT("DefaultOwnerMeshTraceSourceName must be None when OwnerMeshTraceSources is empty.");
				return false;
			}

			if (!FMath::IsFinite(BladeBaseMarkerRelativeLocation.X) || !FMath::IsFinite(BladeBaseMarkerRelativeLocation.Y) || !FMath::IsFinite(BladeBaseMarkerRelativeLocation.Z) ||
				!FMath::IsFinite(BladeTipMarkerRelativeLocation.X) || !FMath::IsFinite(BladeTipMarkerRelativeLocation.Y) || !FMath::IsFinite(BladeTipMarkerRelativeLocation.Z))
			{
				OutReason = TEXT("BladeBaseMarkerRelativeLocation and BladeTipMarkerRelativeLocation must be finite.");
				return false;
			}

			if (BladeBaseMarkerRelativeLocation.Equals(BladeTipMarkerRelativeLocation, KINDA_SMALL_NUMBER))
			{
				OutReason = TEXT("BladeBaseMarkerRelativeLocation and BladeTipMarkerRelativeLocation must be distinct positions.");
				return false;
			}
		}
	}
	else
	{
		if (OwnerMeshTraceSources.Num() > 0)
		{
			OutReason = TEXT("OwnerMeshTraceSources is only valid when bUseOwnerMeshSocketForTrace is true.");
			return false;
		}

		if (!DefaultOwnerMeshTraceSourceName.IsNone())
		{
			OutReason = TEXT("DefaultOwnerMeshTraceSourceName is only valid when bUseOwnerMeshSocketForTrace is true.");
			return false;
		}

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

			if (!FMath::IsFinite(BaseSocket->RelativeLocation.X) || !FMath::IsFinite(BaseSocket->RelativeLocation.Y) || !FMath::IsFinite(BaseSocket->RelativeLocation.Z) ||
				!FMath::IsFinite(TipSocket->RelativeLocation.X) || !FMath::IsFinite(TipSocket->RelativeLocation.Y) || !FMath::IsFinite(TipSocket->RelativeLocation.Z))
			{
				OutReason = TEXT("Blade socket relative locations must be finite.");
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
			if (!FMath::IsFinite(BladeBaseMarkerRelativeLocation.X) || !FMath::IsFinite(BladeBaseMarkerRelativeLocation.Y) || !FMath::IsFinite(BladeBaseMarkerRelativeLocation.Z) ||
				!FMath::IsFinite(BladeTipMarkerRelativeLocation.X) || !FMath::IsFinite(BladeTipMarkerRelativeLocation.Y) || !FMath::IsFinite(BladeTipMarkerRelativeLocation.Z))
			{
				OutReason = TEXT("BladeBaseMarkerRelativeLocation and BladeTipMarkerRelativeLocation must be finite.");
				return false;
			}

			if (BladeBaseMarkerRelativeLocation.Equals(BladeTipMarkerRelativeLocation, KINDA_SMALL_NUMBER))
			{
				OutReason = TEXT("BladeBaseMarkerRelativeLocation and BladeTipMarkerRelativeLocation must be distinct positions.");
				return false;
			}
		}
	}

	if (!FMath::IsFinite(TraceRadius) || TraceRadius <= 0.0f)
	{
		OutReason = TEXT("TraceRadius must be positive and finite.");
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

bool UMeleeWeaponDefinition::IsValidStaticMeshTraceGeometry(FString& OutReason) const
{
	OutReason.Empty();

	if (bUseOwnerMeshSocketForTrace)
	{
		OutReason = TEXT("bUseOwnerMeshSocketForTrace cannot be used as StaticMesh trace geometry.");
		return false;
	}

	if (OwnerMeshTraceSources.Num() > 0 || !DefaultOwnerMeshTraceSourceName.IsNone())
	{
		OutReason = TEXT("OwnerMeshTraceSources cannot be used as StaticMesh trace geometry.");
		return false;
	}

	if (!WeaponMesh)
	{
		OutReason = TEXT("WeaponMesh is not assigned.");
		return false;
	}

	if (BladeBaseSocketName.IsNone() || BladeTipSocketName.IsNone())
	{
		OutReason = TEXT("BladeBaseSocketName and BladeTipSocketName must both be configured.");
		return false;
	}

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

	if (!FMath::IsFinite(BaseSocket->RelativeLocation.X) || !FMath::IsFinite(BaseSocket->RelativeLocation.Y) || !FMath::IsFinite(BaseSocket->RelativeLocation.Z) ||
		!FMath::IsFinite(TipSocket->RelativeLocation.X) || !FMath::IsFinite(TipSocket->RelativeLocation.Y) || !FMath::IsFinite(TipSocket->RelativeLocation.Z))
	{
		OutReason = TEXT("Blade socket relative locations must be finite.");
		return false;
	}

	if (BaseSocket->RelativeLocation.Equals(TipSocket->RelativeLocation, KINDA_SMALL_NUMBER))
	{
		OutReason = FString::Printf(TEXT("BladeBaseSocket '%s' and BladeTipSocket '%s' on WeaponMesh '%s' must not have identical RelativeLocations."), *BladeBaseSocketName.ToString(), *BladeTipSocketName.ToString(), *GetNameSafe(WeaponMesh));
		return false;
	}

	if (!FMath::IsFinite(TraceRadius) || TraceRadius <= 0.0f)
	{
		OutReason = TEXT("TraceRadius must be positive and finite.");
		return false;
	}

	if (BladeSubdivisions < 1 || BladeSubdivisions > 8)
	{
		OutReason = TEXT("BladeSubdivisions must be between 1 and 8.");
		return false;
	}

	return true;
}
