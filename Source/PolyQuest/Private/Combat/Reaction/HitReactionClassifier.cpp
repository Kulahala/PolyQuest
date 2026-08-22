#include "Combat/Reaction/HitReactionClassifier.h"

const FGameplayTag& FHitReactionClassifier::GetSmallReactionTag()
{
	static const FGameplayTag SmallTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Data.Reaction.Small")), false);
	return SmallTag;
}

const FGameplayTag& FHitReactionClassifier::GetBigReactionTag()
{
	static const FGameplayTag BigTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Data.Reaction.Big")), false);
	return BigTag;
}

const FGameplayTag& FHitReactionClassifier::GetLaunchReactionTag()
{
	static const FGameplayTag LaunchTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Data.Reaction.Launch")), false);
	return LaunchTag;
}

EHitReactionTier FHitReactionClassifier::ClassifyReactionTier(const FGameplayTagContainer& AssetTags)
{
	const FGameplayTag& SmallTag = GetSmallReactionTag();
	const FGameplayTag& BigTag = GetBigReactionTag();
	const FGameplayTag& LaunchTag = GetLaunchReactionTag();

	const bool bHasSmall = SmallTag.IsValid() && AssetTags.HasTagExact(SmallTag);
	const bool bHasBig = BigTag.IsValid() && AssetTags.HasTagExact(BigTag);
	const bool bHasLaunch = LaunchTag.IsValid() && AssetTags.HasTagExact(LaunchTag);

	const int32 MatchCount = (bHasSmall ? 1 : 0) + (bHasBig ? 1 : 0) + (bHasLaunch ? 1 : 0);

	if (MatchCount == 0)
	{
		return EHitReactionTier::None;
	}

	if (MatchCount > 1)
	{
		return EHitReactionTier::Invalid;
	}

	if (bHasSmall)
	{
		return EHitReactionTier::Small;
	}

	if (bHasBig)
	{
		return EHitReactionTier::Big;
	}

	if (bHasLaunch)
	{
		return EHitReactionTier::Launch;
	}

	return EHitReactionTier::None;
}
