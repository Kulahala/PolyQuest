// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/Feedback/CombatFeedbackDataAsset.h"

UCombatFeedbackDataAsset::UCombatFeedbackDataAsset()
{
	HitFeedbackOverlayDurationSeconds = 0.10f;
}

UPlayerCombatFeedbackDataAsset::UPlayerCombatFeedbackDataAsset()
{
	Defense.ParrySuccessHitStopDurationSeconds = 0.05f;
	Defense.ParrySuccessHitStopTimeDilation = 0.03f;
}

const FPlayerCombatFeedbackTierSettings* UPlayerCombatFeedbackDataAsset::GetTierSettings(const EHitReactionTier Tier) const
{
	switch (Tier)
	{
	case EHitReactionTier::Small:
		return &SmallTier;
	case EHitReactionTier::Big:
		return &BigTier;
	case EHitReactionTier::Launch:
		return &LaunchTier;
	case EHitReactionTier::None:
	case EHitReactionTier::Invalid:
	default:
		return nullptr;
	}
}

UEnemyCombatFeedbackDataAsset::UEnemyCombatFeedbackDataAsset()
{
	SmallTier.ImpactHitStopDurationSeconds = 0.03f;
	SmallTier.ImpactHitStopTimeDilation = 0.1f;

	BigTier.ImpactHitStopDurationSeconds = 0.05f;
	BigTier.ImpactHitStopTimeDilation = 0.03f;

	LaunchTier.ImpactHitStopDurationSeconds = 0.05f;
	LaunchTier.ImpactHitStopTimeDilation = 0.05f;

	Execution.ImpactHitStopDurationSeconds = 0.05f;
	Execution.ImpactHitStopTimeDilation = 0.03f;
}

const FEnemyCombatFeedbackTierSettings* UEnemyCombatFeedbackDataAsset::GetTierSettings(const EHitReactionTier Tier) const
{
	switch (Tier)
	{
	case EHitReactionTier::Small:
		return &SmallTier;
	case EHitReactionTier::Big:
		return &BigTier;
	case EHitReactionTier::Launch:
		return &LaunchTier;
	case EHitReactionTier::None:
	case EHitReactionTier::Invalid:
	default:
		return nullptr;
	}
}
