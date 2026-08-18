#include "Combat/Melee/MeleeTraceSourceComponent.h"

#include "Combat/Equipment/MeleeWeaponDefinition.h"
#include "Combat/Equipment/WeaponEquipmentComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "PolyQuest.h"

namespace
{
	bool IsAttachedBelow(const USceneComponent* Candidate, const USceneComponent* ExpectedAncestor)
	{
		for (const USceneComponent* Current = Candidate ? Candidate->GetAttachParent() : nullptr; Current; Current = Current->GetAttachParent())
		{
			if (Current == ExpectedAncestor)
			{
				return true;
			}
		}

		return false;
	}
}

UMeleeTraceSourceComponent::UMeleeTraceSourceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UMeleeTraceSourceComponent::BeginPlay()
{
	Super::BeginPlay();

	CachedEquipmentComponent = GetOwner() ? GetOwner()->FindComponentByClass<UWeaponEquipmentComponent>() : nullptr;
}

float UMeleeTraceSourceComponent::GetTraceRadius() const
{
	if (const UWeaponEquipmentComponent* EquipmentComponent = CachedEquipmentComponent.Get())
	{
		if (const UMeleeWeaponDefinition* EquippedWeapon = EquipmentComponent->GetEquippedMainHandMelee())
		{
			return EquippedWeapon->TraceRadius;
		}
	}

	if (StaticMeleeWeaponDefinition)
	{
		return StaticMeleeWeaponDefinition->TraceRadius;
	}

	return TraceRadius;
}

int32 UMeleeTraceSourceComponent::GetBladeSubdivisions() const
{
	if (const UWeaponEquipmentComponent* EquipmentComponent = CachedEquipmentComponent.Get())
	{
		if (const UMeleeWeaponDefinition* EquippedWeapon = EquipmentComponent->GetEquippedMainHandMelee())
		{
			return EquippedWeapon->BladeSubdivisions;
		}
	}

	if (StaticMeleeWeaponDefinition)
	{
		return StaticMeleeWeaponDefinition->BladeSubdivisions;
	}

	return BladeSubdivisions;
}

bool UMeleeTraceSourceComponent::TryGetBladeEndpoints(FVector& OutBladeBase, FVector& OutBladeTip)
{
	if (const UWeaponEquipmentComponent* EquipmentComponent = CachedEquipmentComponent.Get())
	{
		USceneComponent* EquippedBladeBase = nullptr;
		USceneComponent* EquippedBladeTip = nullptr;
		if (EquipmentComponent->TryGetBladeMarkers(EquippedBladeBase, EquippedBladeTip))
		{
			OutBladeBase = EquippedBladeBase->GetComponentLocation();
			OutBladeTip = EquippedBladeTip->GetComponentLocation();
			if (OutBladeBase.Equals(OutBladeTip, KINDA_SMALL_NUMBER))
			{
				WarnInvalidConfiguration(TEXT("the equipped weapon's blade markers resolve to the same world position."));
				return false;
			}

			bConfigurationWarningIssued = false;
			return true;
		}
	}

	if (StaticMeleeWeaponDefinition)
	{
		FString GeometryReason;
		if (!StaticMeleeWeaponDefinition->IsValidStaticMeshTraceGeometry(GeometryReason))
		{
			WarnInvalidConfiguration(FString::Printf(TEXT("StaticMeleeWeaponDefinition '%s' is invalid: %s"), *GetNameSafe(StaticMeleeWeaponDefinition), *GeometryReason));
			return false;
		}

		AActor* Owner = GetOwner();
		if (!Owner)
		{
			WarnInvalidConfiguration(TEXT("the component has no owning actor."));
			return false;
		}

		UStaticMeshComponent* WeaponDisplayStaticMesh = nullptr;
		TInlineComponentArray<UStaticMeshComponent*> StaticMeshComponents(Owner);
		for (UStaticMeshComponent* MeshComp : StaticMeshComponents)
		{
			if (MeshComp && MeshComp->GetFName() == WeaponDisplayComponentName)
			{
				WeaponDisplayStaticMesh = MeshComp;
				break;
			}
		}

		if (!WeaponDisplayStaticMesh)
		{
			WarnInvalidConfiguration(FString::Printf(
				TEXT("owning actor '%s' has no UStaticMeshComponent named '%s'."),
				*GetNameSafe(Owner),
				*WeaponDisplayComponentName.ToString()));
			return false;
		}

		if (WeaponDisplayStaticMesh->GetStaticMesh() != StaticMeleeWeaponDefinition->WeaponMesh)
		{
			WarnInvalidConfiguration(FString::Printf(
				TEXT("WeaponMesh component '%s' mesh '%s' does not match StaticMeleeWeaponDefinition mesh '%s'."),
				*WeaponDisplayComponentName.ToString(),
				*GetNameSafe(WeaponDisplayStaticMesh->GetStaticMesh()),
				*GetNameSafe(StaticMeleeWeaponDefinition->WeaponMesh)));
			return false;
		}

		OutBladeBase = WeaponDisplayStaticMesh->GetSocketLocation(StaticMeleeWeaponDefinition->BladeBaseSocketName);
		OutBladeTip = WeaponDisplayStaticMesh->GetSocketLocation(StaticMeleeWeaponDefinition->BladeTipSocketName);

		if (!FMath::IsFinite(OutBladeBase.X) || !FMath::IsFinite(OutBladeBase.Y) || !FMath::IsFinite(OutBladeBase.Z) ||
			!FMath::IsFinite(OutBladeTip.X) || !FMath::IsFinite(OutBladeTip.Y) || !FMath::IsFinite(OutBladeTip.Z))
		{
			WarnInvalidConfiguration(TEXT("StaticMeleeWeaponDefinition blade socket world locations must be finite."));
			return false;
		}

		if (OutBladeBase.Equals(OutBladeTip, KINDA_SMALL_NUMBER))
		{
			WarnInvalidConfiguration(FString::Printf(
				TEXT("StaticMeleeWeaponDefinition blade sockets '%s' and '%s' resolve to the same world position."),
				*StaticMeleeWeaponDefinition->BladeBaseSocketName.ToString(),
				*StaticMeleeWeaponDefinition->BladeTipSocketName.ToString()));
			return false;
		}

		bConfigurationWarningIssued = false;
		return true;
	}

	USceneComponent* WeaponDisplay = nullptr;
	USceneComponent* BladeBase = nullptr;
	USceneComponent* BladeTip = nullptr;
	if (!ResolveConfiguredComponents(WeaponDisplay, BladeBase, BladeTip))
	{
		return false;
	}

	if (!IsAttachedBelow(BladeBase, WeaponDisplay) || !IsAttachedBelow(BladeTip, WeaponDisplay))
	{
		WarnInvalidConfiguration(TEXT("BladeTraceBase and BladeTraceTip must both be attached below WeaponMesh."));
		return false;
	}

	OutBladeBase = BladeBase->GetComponentLocation();
	OutBladeTip = BladeTip->GetComponentLocation();
	if (OutBladeBase.Equals(OutBladeTip, KINDA_SMALL_NUMBER))
	{
		WarnInvalidConfiguration(TEXT("BladeTraceBase and BladeTraceTip resolve to the same world position."));
		return false;
	}

	bConfigurationWarningIssued = false;
	return true;
}

bool UMeleeTraceSourceComponent::ResolveConfiguredComponents(USceneComponent*& OutWeaponDisplay, USceneComponent*& OutBladeBase, USceneComponent*& OutBladeTip)
{
	OutWeaponDisplay = nullptr;
	OutBladeBase = nullptr;
	OutBladeTip = nullptr;

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		WarnInvalidConfiguration(TEXT("the component has no owning actor."));
		return false;
	}

	if (WeaponDisplayComponentName.IsNone() || BladeTraceBaseComponentName.IsNone() || BladeTraceTipComponentName.IsNone())
	{
		WarnInvalidConfiguration(TEXT("WeaponMesh, BladeTraceBase, and BladeTraceTip component names must all be configured."));
		return false;
	}

	TInlineComponentArray<USceneComponent*> SceneComponents(Owner);
	for (USceneComponent* SceneComponent : SceneComponents)
	{
		if (!SceneComponent)
		{
			continue;
		}

		const FName ComponentName = SceneComponent->GetFName();
		if (ComponentName == WeaponDisplayComponentName)
		{
			OutWeaponDisplay = SceneComponent;
		}
		else if (ComponentName == BladeTraceBaseComponentName)
		{
			OutBladeBase = SceneComponent;
		}
		else if (ComponentName == BladeTraceTipComponentName)
		{
			OutBladeTip = SceneComponent;
		}
	}

	if (!OutWeaponDisplay || !OutBladeBase || !OutBladeTip)
	{
		WarnInvalidConfiguration(FString::Printf(
			TEXT("the owning actor must contain SceneComponents named '%s', '%s', and '%s'."),
			*WeaponDisplayComponentName.ToString(),
			*BladeTraceBaseComponentName.ToString(),
			*BladeTraceTipComponentName.ToString()));
		return false;
	}

	return true;
}

void UMeleeTraceSourceComponent::WarnInvalidConfiguration(const FString& Reason)
{
	if (bConfigurationWarningIssued)
	{
		return;
	}

	bConfigurationWarningIssued = true;
	UE_LOG(LogPolyQuest, Warning, TEXT("Melee trace source on '%s' cannot deliver hits: %s"), *GetNameSafe(GetOwner()), *Reason);
}
