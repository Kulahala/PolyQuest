#include "Combat/Melee/MeleeWeaponTrailComponent.h"

#include "NiagaraSystem.h"

namespace
{
	const FName BladeBaseParameterName(TEXT("User.BladeBase"));
	const FName BladeTipParameterName(TEXT("User.BladeTip"));
}

UMeleeWeaponTrailComponent::UMeleeWeaponTrailComponent()
{
	bAutoActivate = false;
	bAutoManageAttachment = false;
	SetAutoDestroy(false);
}

bool UMeleeWeaponTrailComponent::GetAutoDestroy() const
{
	static const FBoolProperty* BoolProp = CastField<FBoolProperty>(UNiagaraComponent::StaticClass()->FindPropertyByName(TEXT("bAutoDestroy")));
	return BoolProp ? BoolProp->GetPropertyValue_InContainer(this) : false;
}

void UMeleeWeaponTrailComponent::StartTrail(const UObject* Requester, const FVector& BladeBase, const FVector& BladeTip)
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bTestTrackingEnabled)
	{
		++TestStartCallCount;
		TestLastBladeBase = BladeBase;
		TestLastBladeTip = BladeTip;
		ActiveRequester = Requester;
		bTestIsActive = true;
		return;
	}
#endif

	if (!GetAsset())
	{
		return;
	}

	ActiveRequester = Requester;
	SetBladeEndpoints(BladeBase, BladeTip);
	Activate(true);
}

void UMeleeWeaponTrailComponent::UpdateTrail(const UObject* Requester, const FVector& BladeBase, const FVector& BladeTip)
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bTestTrackingEnabled)
	{
		if (ActiveRequester.Get() == Requester)
		{
			++TestUpdateCallCount;
			TestLastBladeBase = BladeBase;
			TestLastBladeTip = BladeTip;
		}
		return;
	}
#endif

	if (!GetAsset() || ActiveRequester.Get() != Requester)
	{
		return;
	}

	SetBladeEndpoints(BladeBase, BladeTip);
}

void UMeleeWeaponTrailComponent::EndTrail(const UObject* Requester)
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bTestTrackingEnabled)
	{
		if (ActiveRequester.Get() == Requester)
		{
			++TestEndCallCount;
			ActiveRequester.Reset();
			bTestIsActive = false;
		}
		return;
	}
#endif

	if (ActiveRequester.Get() != Requester)
	{
		return;
	}

	ActiveRequester.Reset();

	if (GetAsset())
	{
		Deactivate();
	}
}

void UMeleeWeaponTrailComponent::SetBladeEndpoints(const FVector& BladeBase, const FVector& BladeTip)
{
	SetVariablePosition(BladeBaseParameterName, BladeBase);
	SetVariablePosition(BladeTipParameterName, BladeTip);
}
