#include "Combat/Melee/MeleeTraceSourceComponent.h"

#include "Components/SceneComponent.h"
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

bool UMeleeTraceSourceComponent::TryGetBladeEndpoints(FVector& OutBladeBase, FVector& OutBladeTip)
{
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
