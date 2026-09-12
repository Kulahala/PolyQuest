#include "Combat/CombatImpactEffectContext.h"

FCombatImpactEffectContext::FCombatImpactEffectContext()
	: FGameplayEffectContext()
	, WorldIncomingDirection(FVector::ZeroVector)
{
}

UScriptStruct* FCombatImpactEffectContext::GetScriptStruct() const
{
	return FCombatImpactEffectContext::StaticStruct();
}

FGameplayEffectContext* FCombatImpactEffectContext::Duplicate() const
{
	FCombatImpactEffectContext* NewContext = new FCombatImpactEffectContext();
	*NewContext = *this;
	if (GetHitResult())
	{
		// Deep copy the HitResult
		NewContext->AddHitResult(*GetHitResult(), true);
	}
	return NewContext;
}

bool FCombatImpactEffectContext::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	if (!FGameplayEffectContext::NetSerialize(Ar, Map, bOutSuccess))
	{
		bOutSuccess = false;
		return false;
	}

	if (!bOutSuccess)
	{
		return false;
	}

	Ar << WorldIncomingDirection;

	if (Ar.IsError())
	{
		bOutSuccess = false;
		return false;
	}

	bOutSuccess = true;
	return true;
}
