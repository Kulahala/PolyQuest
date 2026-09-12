#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "CombatImpactEffectContext.generated.h"

/**
 * Narrow GameplayEffectContext subclass carrying a snapshot of the world-space incoming direction.
 * Presence of this context type explicitly signifies an authored incoming direction (including zero-direction),
 * suppressing fallback to source actor location.
 */
USTRUCT()
struct POLYQUEST_API FCombatImpactEffectContext : public FGameplayEffectContext
{
	GENERATED_BODY()

public:
	FCombatImpactEffectContext();
	virtual ~FCombatImpactEffectContext() override = default;

	const FVector& GetWorldIncomingDirection() const { return WorldIncomingDirection; }
	void SetWorldIncomingDirection(const FVector& InDirection) { WorldIncomingDirection = InDirection; }

	virtual UScriptStruct* GetScriptStruct() const override;
	virtual FGameplayEffectContext* Duplicate() const override;
	virtual bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess) override;

protected:
	UPROPERTY()
	FVector WorldIncomingDirection = FVector::ZeroVector;
};

template<>
struct TStructOpsTypeTraits<FCombatImpactEffectContext> : public TStructOpsTypeTraitsBase2<FCombatImpactEffectContext>
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};
