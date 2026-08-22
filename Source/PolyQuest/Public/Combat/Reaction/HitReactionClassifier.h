#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

/**
 * Non-reflected hit reaction tier classification enum.
 */
enum class EHitReactionTier : uint8
{
	None,
	Small,
	Big,
	Launch,
	Invalid
};

/**
 * Pure, stateless native classifier for hit reaction asset tags.
 * Only exact matching against Data.Reaction.* tags is performed.
 * It is context-free: no dependency on Actor, ASC, World, UObject, or DataAsset, and no logging.
 */
class POLYQUEST_API FHitReactionClassifier
{
public:
	static EHitReactionTier ClassifyReactionTier(const FGameplayTagContainer& AssetTags);

	static const FGameplayTag& GetSmallReactionTag();
	static const FGameplayTag& GetBigReactionTag();
	static const FGameplayTag& GetLaunchReactionTag();
};
