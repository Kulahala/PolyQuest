// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/EnemyHealthBarWidget.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Math/UnrealMathUtility.h"

void UEnemyHealthBarWidget::SetHealth(float Current, float Max)
{
	float DisplayPercent = 0.0f;

	if (FMath::IsFinite(Current) && FMath::IsFinite(Max) && Max > 0.0f)
	{
		const float DisplayCurrent = FMath::Clamp(Current, 0.0f, Max);
		DisplayPercent = FMath::Clamp(DisplayCurrent / Max, 0.0f, 1.0f);
	}

	if (HealthProgressBar)
	{
		HealthProgressBar->SetPercent(DisplayPercent);
	}
}

void UEnemyHealthBarWidget::SetLockOnHighlighted(const bool bHighlighted)
{
	if (TargetHighlightImage)
	{
		TargetHighlightImage->SetVisibility(bHighlighted ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

#if WITH_DEV_AUTOMATION_TESTS
void UEnemyHealthBarWidget::SetTestHealthProgressBar(UProgressBar* InBar)
{
	HealthProgressBar = InBar;
}

UProgressBar* UEnemyHealthBarWidget::GetTestHealthProgressBar() const
{
	return HealthProgressBar;
}

void UEnemyHealthBarWidget::SetTestTargetHighlightImage(UImage* InImage)
{
	TargetHighlightImage = InImage;
}

UImage* UEnemyHealthBarWidget::GetTestTargetHighlightImage() const
{
	return TargetHighlightImage;
}
#endif
