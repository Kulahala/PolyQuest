#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "DefenseProfileDefinition.generated.h"

/**
 * The authored defensive input-to-ability mapping of one hand slot. Carried by
 * a MainHand definition it is the fallback; carried by an OffHand definition it
 * overrides the fallback. TODO-03A1 permits only the default Guard/Parry tags;
 * Shield presentation fields arrive with TODO-03A4, not earlier.
 */
UCLASS(BlueprintType)
class POLYQUEST_API UDefenseProfileDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Ability tag the Guard input resolves to while this profile is effective. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Defense")
	FGameplayTag GuardAbilityTag;

	/** Ability tag the Parry input resolves to while this profile is effective. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Defense")
	FGameplayTag ParryAbilityTag;

	bool IsProfileValid() const
	{
		return GuardAbilityTag.IsValid() && ParryAbilityTag.IsValid() && (GuardAbilityTag != ParryAbilityTag);
	}
};
