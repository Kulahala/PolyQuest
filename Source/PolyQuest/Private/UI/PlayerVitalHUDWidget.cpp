// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PlayerVitalHUDWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Internationalization/Text.h"
#include "Math/UnrealMathUtility.h"

void UPlayerVitalHUDWidget::SetHealth(float Current, float Max)
{
	float DisplayCurrent = 0.0f;
	float DisplayMax = 0.0f;
	float DisplayPercent = 0.0f;

	if (FMath::IsFinite(Current) && FMath::IsFinite(Max) && Max > 0.0f)
	{
		DisplayCurrent = FMath::Clamp(Current, 0.0f, Max);
		DisplayMax = Max;
		DisplayPercent = FMath::Clamp(DisplayCurrent / DisplayMax, 0.0f, 1.0f);
	}

	if (!bIsHealthInitialized)
	{
		CurrentBufferPercent = DisplayPercent;
		TargetHealthPercent = DisplayPercent;
		bIsHealthInitialized = true;
		BufferDelayTimer = 0.0f;
		if (HealthBufferProgressBar)
		{
			HealthBufferProgressBar->SetPercent(CurrentBufferPercent);
		}
	}
	else if (DisplayPercent > TargetHealthPercent)
	{
		// Strictly healing: buffer snaps up immediately with current health
		CurrentBufferPercent = DisplayPercent;
		TargetHealthPercent = DisplayPercent;
		BufferDelayTimer = 0.0f;
		if (HealthBufferProgressBar)
		{
			HealthBufferProgressBar->SetPercent(CurrentBufferPercent);
		}
	}
	else if (DisplayPercent < TargetHealthPercent)
	{
		// Strictly damage: start delay timer before catch-up begins
		TargetHealthPercent = DisplayPercent;
		BufferDelayTimer = BufferCatchUpDelay;
	}
	// If DisplayPercent == TargetHealthPercent, health did not change (e.g. stamina regen trigger).
	// Preserve ongoing buffer delay and interpolation without any disturbance.

	if (HealthProgressBar)
	{
		HealthProgressBar->SetPercent(DisplayPercent);
	}

	if (HealthCurrentText)
	{
		const int32 RoundedCurrent = FMath::RoundToInt(DisplayCurrent);
		HealthCurrentText->SetText(FText::AsNumber(RoundedCurrent));
	}

	if (HealthMaxText)
	{
		const int32 RoundedMax = FMath::RoundToInt(DisplayMax);
		HealthMaxText->SetText(FText::AsNumber(RoundedMax));
	}
}

void UPlayerVitalHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateBufferHealth(InDeltaTime);
}

void UPlayerVitalHUDWidget::UpdateBufferHealth(float InDeltaTime)
{
	if (!HealthBufferProgressBar)
	{
		return;
	}

	if (BufferDelayTimer > 0.0f)
	{
		BufferDelayTimer -= InDeltaTime;
		return;
	}

	if (!FMath::IsNearlyEqual(CurrentBufferPercent, TargetHealthPercent, 0.001f))
	{
		CurrentBufferPercent = FMath::FInterpTo(CurrentBufferPercent, TargetHealthPercent, InDeltaTime, BufferCatchUpSpeed);
		HealthBufferProgressBar->SetPercent(CurrentBufferPercent);
	}
	else if (CurrentBufferPercent != TargetHealthPercent)
	{
		CurrentBufferPercent = TargetHealthPercent;
		HealthBufferProgressBar->SetPercent(CurrentBufferPercent);
	}
}

void UPlayerVitalHUDWidget::SetStamina(float Current, float Max)
{
	float DisplayCurrent = 0.0f;
	float DisplayMax = 0.0f;
	float DisplayPercent = 0.0f;

	if (FMath::IsFinite(Current) && FMath::IsFinite(Max) && Max > 0.0f)
	{
		DisplayCurrent = FMath::Clamp(Current, 0.0f, Max);
		DisplayMax = Max;
		DisplayPercent = FMath::Clamp(DisplayCurrent / DisplayMax, 0.0f, 1.0f);
	}

	if (StaminaProgressBar)
	{
		StaminaProgressBar->SetPercent(DisplayPercent);
	}

	if (StaminaCurrentText)
	{
		const int32 RoundedCurrent = FMath::RoundToInt(DisplayCurrent);
		StaminaCurrentText->SetText(FText::AsNumber(RoundedCurrent));
	}

	if (StaminaMaxText)
	{
		const int32 RoundedMax = FMath::RoundToInt(DisplayMax);
		StaminaMaxText->SetText(FText::AsNumber(RoundedMax));
	}
}

void UPlayerVitalHUDWidget::SetExhausted(const bool bIsExhausted)
{
	if (StaminaExhaustedOverlay)
	{
		StaminaExhaustedOverlay->SetVisibility(bIsExhausted ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

#if WITH_DEV_AUTOMATION_TESTS
void UPlayerVitalHUDWidget::SetTestHealthWidgets(UProgressBar* InBar, UTextBlock* InCurr, UTextBlock* InMax)
{
	HealthProgressBar = InBar;
	HealthCurrentText = InCurr;
	HealthMaxText = InMax;
}

void UPlayerVitalHUDWidget::SetTestStaminaWidgets(UProgressBar* InBar, UTextBlock* InCurr, UTextBlock* InMax)
{
	StaminaProgressBar = InBar;
	StaminaCurrentText = InCurr;
	StaminaMaxText = InMax;
}

UProgressBar* UPlayerVitalHUDWidget::GetTestHealthProgressBar() const
{
	return HealthProgressBar;
}

UTextBlock* UPlayerVitalHUDWidget::GetTestHealthCurrentText() const
{
	return HealthCurrentText;
}

UTextBlock* UPlayerVitalHUDWidget::GetTestHealthMaxText() const
{
	return HealthMaxText;
}

UProgressBar* UPlayerVitalHUDWidget::GetTestStaminaProgressBar() const
{
	return StaminaProgressBar;
}

UTextBlock* UPlayerVitalHUDWidget::GetTestStaminaCurrentText() const
{
	return StaminaCurrentText;
}

UTextBlock* UPlayerVitalHUDWidget::GetTestStaminaMaxText() const
{
	return StaminaMaxText;
}

void UPlayerVitalHUDWidget::SetTestHealthBufferProgressBar(UProgressBar* InBar)
{
	HealthBufferProgressBar = InBar;
}

UProgressBar* UPlayerVitalHUDWidget::GetTestHealthBufferProgressBar() const
{
	return HealthBufferProgressBar;
}

float UPlayerVitalHUDWidget::GetTestBufferDelayTimer() const
{
	return BufferDelayTimer;
}

float UPlayerVitalHUDWidget::GetTestCurrentBufferPercent() const
{
	return CurrentBufferPercent;
}

void UPlayerVitalHUDWidget::SetTestStaminaExhaustedOverlay(UWidget* InWidget)
{
	StaminaExhaustedOverlay = InWidget;
}

UWidget* UPlayerVitalHUDWidget::GetTestStaminaExhaustedOverlay() const
{
	return StaminaExhaustedOverlay;
}

bool UPlayerVitalHUDWidget::IsTestExhaustedOverlayVisible() const
{
	return StaminaExhaustedOverlay && StaminaExhaustedOverlay->GetVisibility() == ESlateVisibility::HitTestInvisible;
}

void UPlayerVitalHUDWidget::SimulateTickForTesting(float InDeltaTime)
{
	UpdateBufferHealth(InDeltaTime);
}
#endif
