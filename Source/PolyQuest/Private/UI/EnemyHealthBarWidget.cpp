// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/EnemyHealthBarWidget.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Math/UnrealMathUtility.h"

void UEnemyHealthBarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (HealthProgressBar && !bHasCapturedHealthBaseColor)
	{
		HealthBaseColor = HealthProgressBar->GetFillColorAndOpacity();
		bHasCapturedHealthBaseColor = true;
	}
}

void UEnemyHealthBarWidget::SetHealth(float Current, float Max)
{
	float DisplayPercent = 0.0f;

	if (FMath::IsFinite(Current) && FMath::IsFinite(Max) && Max > 0.0f)
	{
		const float DisplayCurrent = FMath::Clamp(Current, 0.0f, Max);
		DisplayPercent = FMath::Clamp(DisplayCurrent / Max, 0.0f, 1.0f);
	}

	if (HealthProgressBar && !bHasCapturedHealthBaseColor)
	{
		HealthBaseColor = HealthProgressBar->GetFillColorAndOpacity();
		bHasCapturedHealthBaseColor = true;
	}

	if (!bIsHealthInitialized)
	{
		TargetHealthPercent = DisplayPercent;
		bIsHealthInitialized = true;
		HitFlashTimer = 0.0f;
	}
	else if (DisplayPercent < TargetHealthPercent)
	{
		// Strictly damage: update target health, trigger hit impact shake and crisp white hit flash
		TargetHealthPercent = DisplayPercent;
		HitFlashTimer = HitFlashDuration;
		if (HealthProgressBar)
		{
			HealthProgressBar->SetFillColorAndOpacity(FLinearColor(2.0f, 2.0f, 2.0f, HealthBaseColor.A));
		}
		PlayHitShake();
	}
	else
	{
		TargetHealthPercent = DisplayPercent;
		// Healing or no change: clear flash and restore base color
		HitFlashTimer = 0.0f;
		if (HealthProgressBar)
		{
			HealthProgressBar->SetFillColorAndOpacity(HealthBaseColor);
		}
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
	UpdateHitFlash(InDeltaTime);
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

void UEnemyHealthBarWidget::UpdateHitFlash(const float InDeltaTime)
{
	if (!HealthProgressBar)
	{
		return;
	}

	if (HitFlashTimer > 0.0f)
	{
		HitFlashTimer = FMath::Max(0.0f, HitFlashTimer - InDeltaTime);
		if (HitFlashDuration > 0.0f)
		{
			// Hold peak white for initial 0.03s (~2 frames at 60fps) before decaying
			constexpr float PeakHoldDuration = 0.03f;
			const float DecayDuration = HitFlashDuration - PeakHoldDuration;
			const float Elapsed = HitFlashDuration - HitFlashTimer;

			float Weight = 1.0f;
			if (Elapsed > PeakHoldDuration && DecayDuration > 0.0f)
			{
				const float DecayRemaining = HitFlashTimer;
				const float DecayRatio = FMath::Clamp(DecayRemaining / DecayDuration, 0.0f, 1.0f);
				Weight = FMath::Square(DecayRatio);
			}

			const FLinearColor PeakColor = FLinearColor(2.0f, 2.0f, 2.0f, HealthBaseColor.A);
			const FLinearColor CurrentColor = FMath::Lerp(HealthBaseColor, PeakColor, Weight);
			HealthProgressBar->SetFillColorAndOpacity(CurrentColor);
		}
		else
		{
			HealthProgressBar->SetFillColorAndOpacity(HealthBaseColor);
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

float UEnemyHealthBarWidget::GetTestHitFlashTimer() const
{
	return HitFlashTimer;
}

FLinearColor UEnemyHealthBarWidget::GetTestHealthFillColor() const
{
	return HealthProgressBar ? HealthProgressBar->GetFillColorAndOpacity() : FLinearColor::Transparent;
}

FLinearColor UEnemyHealthBarWidget::GetTestHealthBaseColor() const
{
	return HealthBaseColor;
}

float UEnemyHealthBarWidget::GetTestHealthTranslationY() const
{
	UWidget* TargetWidget = HealthBarOverlay ? HealthBarOverlay.Get() : HealthProgressBar.Get();
	return TargetWidget ? TargetWidget->GetRenderTransform().Translation.Y : 0.0f;
}

void UEnemyHealthBarWidget::SimulateTickForTesting(float InDeltaTime)
{
	UpdateShake(InDeltaTime);
	UpdateHitFlash(InDeltaTime);
}
#endif
