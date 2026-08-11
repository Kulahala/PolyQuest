#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "CombatLoadoutDefinition.generated.h"

/** One authored route from a stable physical-input intent to an optional GAS Ability tag. */
USTRUCT(BlueprintType)
struct POLYQUEST_API FCombatInputAbilityRoute
{
	GENERATED_BODY()

	/** The physical input intent, such as Input.PrimaryAttack or Input.AbilitySlot.1. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Input")
	FGameplayTag InputIntentTag;

	/** The Ability requested when this input starts. An invalid tag intentionally means no activation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Input")
	FGameplayTag AbilityTag;
};

/**
 * Immutable authored routes for one active combat loadout.
 * Runtime input state and active abilities remain owned by the player and ASC.
 */
UCLASS(BlueprintType)
class POLYQUEST_API UCombatLoadoutDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Returns false when an input intent is missing or duplicated in the authored table. */
	bool IsRouteTableValid() const
	{
		TSet<FGameplayTag> SeenInputIntentTags;
		for (const FCombatInputAbilityRoute& Route : InputAbilityRoutes)
		{
			if (!Route.InputIntentTag.IsValid() || SeenInputIntentTags.Contains(Route.InputIntentTag))
			{
				return false;
			}

			SeenInputIntentTags.Add(Route.InputIntentTag);
		}

		return true;
	}

	/** Looks up an authored route. A successful lookup may deliberately return an invalid Ability tag. */
	bool TryGetAbilityTagForInputIntent(const FGameplayTag& InputIntentTag, FGameplayTag& OutAbilityTag) const
	{
		OutAbilityTag = FGameplayTag();
		for (const FCombatInputAbilityRoute& Route : InputAbilityRoutes)
		{
			if (Route.InputIntentTag == InputIntentTag)
			{
				OutAbilityTag = Route.AbilityTag;
				return true;
			}
		}

		return false;
	}

protected:
	/** Optional direct Ability routes for this loadout; invalid Ability tags are intentional no-op routes. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Input", meta = (AllowPrivateAccess = "true"))
	TArray<FCombatInputAbilityRoute> InputAbilityRoutes;
};
