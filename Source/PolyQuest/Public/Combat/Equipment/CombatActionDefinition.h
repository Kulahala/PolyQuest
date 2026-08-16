#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "CombatActionDefinition.generated.h"

class UGameplayAbility;

/**
 * One authored combat-skill reference for equipment-prepared slots. It is a
 * reference list only: the referenced Gameplay Ability Blueprint remains the
 * owner of Montage, GameplayEffect, presentation, and tuning configuration.
 * It never owns input, targets, hit consumption, cooldown state, or damage.
 */
UCLASS(BlueprintType)
class POLYQUEST_API UCombatActionDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	/** The ability implementation activated when this action occupies a prepared slot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action")
	TSubclassOf<UGameplayAbility> AbilityClass;

	/** Optional semantic identity for diagnostics and future UI; never a runtime routing tag. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat Action")
	FGameplayTag ActionIntentTag;
};
