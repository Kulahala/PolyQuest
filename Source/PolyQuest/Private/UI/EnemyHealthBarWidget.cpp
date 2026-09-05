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

	if (!bIsHealthInitialized)
	{
		TargetHealthPercent = DisplayPercent;
		bIsHealthInitialized = true;
	}
	else if (DisplayPercent < TargetHealthPercent)
	{
		// Strictly damage: update target health and trigger crisp hit impact shake
		TargetHealthPercent = DisplayPercent;
		PlayHitShake();
	}
	else
	{
		TargetHealthPercent = DisplayPercent;
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

void UEnemyHealthBarWidget::PlayHitShake()
{
	HitShakeTimer = ShakeDuration;
}

float UEnemyHealthBarWidget::CalculateShakeOffset(const float RemainingTimer) const
{
	if (RemainingTimer <= 0.0f || ShakeDuration <= 0.0f)
	{
		return 0.0f;
	}

	const float NormalizedRemaining = FMath::Clamp(RemainingTimer / ShakeDuration, 0.0f, 1.0f);
	const float Elapsed = ShakeDuration - RemainingTimer;
	// Damped amplitude (quadratic decay)
	const float Amplitude = ShakeMaxDisplacement * FMath::Square(NormalizedRemaining);
	// High-frequency sine oscillation (starts with downward displacement in Slate coords)
	return Amplitude * FMath::Sin(Elapsed * ShakeFrequency * 2.0f * UE_PI);
}

void UEnemyHealthBarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateShake(InDeltaTime);
}

void UEnemyHealthBarWidget::UpdateShake(const float InDeltaTime)
{
	if (HitShakeTimer > 0.0f)
	{
		HitShakeTimer = FMath::Max(0.0f, HitShakeTimer - InDeltaTime);
		const float OffsetY = CalculateShakeOffset(HitShakeTimer);
		UWidget* TargetWidget = HealthBarOverlay ? HealthBarOverlay.Get() : HealthProgressBar.Get();
		if (TargetWidget)
		{
			TargetWidget->SetRenderTranslation(FVector2D(0.0f, OffsetY));
		}
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

void UEnemyHealthBarWidget::SetTestHealthBarOverlay(UWidget* InWidget)
{
	HealthBarOverlay = InWidget;
}

UWidget* UEnemyHealthBarWidget::GetTestHealthBarOverlay() const
{
	return HealthBarOverlay;
}

float UEnemyHealthBarWidget::GetTestHitShakeTimer() const
{
	return HitShakeTimer;
}

float UEnemyHealthBarWidget::GetTestHealthTranslationY() const
{
	UWidget* TargetWidget = HealthBarOverlay ? HealthBarOverlay.Get() : HealthProgressBar.Get();
	return TargetWidget ? TargetWidget->GetRenderTransform().Translation.Y : 0.0f;
}

void UEnemyHealthBarWidget::SimulateTickForTesting(float InDeltaTime)
{
	UpdateShake(InDeltaTime);
}
#endif
