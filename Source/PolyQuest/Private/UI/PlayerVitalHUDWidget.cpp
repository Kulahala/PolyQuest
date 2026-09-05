// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PlayerVitalHUDWidget.h"
#include "Components/Image.h"
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
		// Strictly damage: start delay timer before catch-up begins, trigger hit flash and health shake
		TargetHealthPercent = DisplayPercent;
		BufferDelayTimer = BufferCatchUpDelay;
		DamageFlashTimer = DamageFlashDuration;
		PlayHealthShake();
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
	UpdateVignette(InDeltaTime);
	UpdateShake(InDeltaTime);
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

void UPlayerVitalHUDWidget::UpdateVignette(float InDeltaTime)
{
	// 1. Damage Hit Flash calculation (asymmetric Attack-Decay ease-in/ease-out)
	float FlashAlpha = 0.0f;
	if (DamageFlashTimer > 0.0f)
	{
		DamageFlashTimer = FMath::Max(0.0f, DamageFlashTimer - InDeltaTime);
		if (DamageFlashDuration > 0.0f)
		{
			// Normalized progress: 0.0 (impact moment) -> 1.0 (fully dissipated)
			const float Progress = 1.0f - (DamageFlashTimer / DamageFlashDuration);

			// Fast attack ramp-in (~20% of duration) eliminates 1-frame strobe popping,
			// followed by quadratic ease-out decay (~80% of duration) for a soft dissipating finish.
			constexpr float AttackRatio = 0.20f;
			float CurveWeight = 0.0f;

			if (Progress <= AttackRatio)
			{
				const float AttackAlpha = Progress / AttackRatio;
				// Smooth ease-out ramp-in (sine quarter wave: 0 -> 1 with zero derivative at peak)
				CurveWeight = FMath::Sin(AttackAlpha * (0.5f * UE_PI));
			}
			else
			{
				const float DecayAlpha = (Progress - AttackRatio) / (1.0f - AttackRatio);
				// Quadratic ease-out fade (smoothly dissolves to 0 without abrupt cut-off)
				CurveWeight = FMath::Square(1.0f - DecayAlpha);
			}

			FlashAlpha = DamageFlashMaxAlpha * CurveWeight;
		}
	}

	// 2. Low Health Pulse calculation (sine wave breathing & smooth exit fade-out)
	float PulseAlpha = 0.0f;
	const bool bIsLowHealth = bIsHealthInitialized && (TargetHealthPercent <= LowHealthThreshold);

	if (bIsLowHealth)
	{
		// Smoothly fade in pulse weight to 1.0 (~0.2s)
		LowHealthPulseWeight = FMath::FInterpConstantTo(LowHealthPulseWeight, 1.0f, InDeltaTime, 5.0f);
		LowHealthPulseTimer += InDeltaTime;
		if (LowHealthPulsePeriod > 0.0f && LowHealthPulseTimer >= LowHealthPulsePeriod)
		{
			LowHealthPulseTimer = FMath::Fmod(LowHealthPulseTimer, LowHealthPulsePeriod);
		}
	}
	else
	{
		// Smoothly fade out pulse weight to 0.0 over LowHealthFadeOutDuration (~0.5s)
		if (LowHealthPulseWeight > 0.0f)
		{
			const float FadeOutSpeed = (LowHealthFadeOutDuration > 0.0f) ? (1.0f / LowHealthFadeOutDuration) : 10.0f;
			LowHealthPulseWeight = FMath::FInterpConstantTo(LowHealthPulseWeight, 0.0f, InDeltaTime, FadeOutSpeed);
			LowHealthPulseTimer += InDeltaTime;
			if (LowHealthPulsePeriod > 0.0f && LowHealthPulseTimer >= LowHealthPulsePeriod)
			{
				LowHealthPulseTimer = FMath::Fmod(LowHealthPulseTimer, LowHealthPulsePeriod);
			}
		}
		else
		{
			LowHealthPulseTimer = 0.0f;
		}
	}

	if (LowHealthPulseWeight > 0.0f && LowHealthPulsePeriod > 0.0f)
	{
		// Sine wave starting from trough (-0.5 PI) to swell gracefully upward
		const float NormalizedPhase = (LowHealthPulseTimer / LowHealthPulsePeriod) * 2.0f * UE_PI - (0.5f * UE_PI);
		const float SineAlpha = 0.5f + 0.5f * FMath::Sin(NormalizedPhase);
		const float RawPulseAlpha = FMath::Lerp(LowHealthPulseMinAlpha, LowHealthPulseMaxAlpha, SineAlpha);
		PulseAlpha = RawPulseAlpha * LowHealthPulseWeight;
	}

	// 3. Additive blend with global clamp ceiling
	CurrentVignetteAlpha = FMath::Clamp(FlashAlpha + PulseAlpha, 0.0f, MaxAllowedVignetteAlpha);

	// 4. Drive optional UMG Image representation
	if (LowHealthVignetteImage)
	{
		if (CurrentVignetteAlpha > 0.001f)
		{
			if (LowHealthVignetteImage->GetVisibility() != ESlateVisibility::HitTestInvisible)
			{
				LowHealthVignetteImage->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
			LowHealthVignetteImage->SetRenderOpacity(CurrentVignetteAlpha);
		}
		else
		{
			if (LowHealthVignetteImage->GetVisibility() != ESlateVisibility::Collapsed)
			{
				LowHealthVignetteImage->SetVisibility(ESlateVisibility::Collapsed);
			}
			LowHealthVignetteImage->SetRenderOpacity(0.0f);
		}
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

	// Trigger immediate micro-shake rejection nudge when newly entering exhausted state
	if (bIsExhausted && !bWasExhausted)
	{
		PlayStaminaRejectionShake();
	}
	bWasExhausted = bIsExhausted;
}

void UPlayerVitalHUDWidget::PlayHealthShake()
{
	HealthShakeTimer = ShakeDuration;
}

void UPlayerVitalHUDWidget::PlayStaminaRejectionShake()
{
	StaminaShakeTimer = ShakeDuration;
}

float UPlayerVitalHUDWidget::CalculateShakeOffset(const float RemainingTimer) const
{
	if (RemainingTimer <= 0.0f || ShakeDuration <= 0.0f)
	{
		return 0.0f;
	}

	const float NormalizedRemaining = FMath::Clamp(RemainingTimer / ShakeDuration, 0.0f, 1.0f);
	const float Elapsed = ShakeDuration - RemainingTimer;
	// Damped amplitude (quadratic decay)
	const float Amplitude = ShakeMaxDisplacement * FMath::Square(NormalizedRemaining);
	// High-frequency sine oscillation
	return Amplitude * FMath::Sin(Elapsed * ShakeFrequency * 2.0f * UE_PI);
}

void UPlayerVitalHUDWidget::UpdateShake(const float InDeltaTime)
{
	// 1. Health Bar Micro-Shake (vertical up-down impact)
	if (HealthShakeTimer > 0.0f)
	{
		HealthShakeTimer = FMath::Max(0.0f, HealthShakeTimer - InDeltaTime);
		const float OffsetY = CalculateShakeOffset(HealthShakeTimer);
		UWidget* TargetWidget = HealthBarOverlay ? HealthBarOverlay.Get() : HealthProgressBar.Get();
		if (TargetWidget)
		{
			TargetWidget->SetRenderTranslation(FVector2D(0.0f, OffsetY));
		}
	}

	// 2. Stamina Bar Micro-Shake (vertical up-down rejection)
	if (StaminaShakeTimer > 0.0f)
	{
		StaminaShakeTimer = FMath::Max(0.0f, StaminaShakeTimer - InDeltaTime);
		const float OffsetY = CalculateShakeOffset(StaminaShakeTimer);
		UWidget* TargetWidget = StaminaBarOverlay ? StaminaBarOverlay.Get() : StaminaProgressBar.Get();
		if (TargetWidget)
		{
			TargetWidget->SetRenderTranslation(FVector2D(0.0f, OffsetY));
		}
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

void UPlayerVitalHUDWidget::SetTestLowHealthVignetteImage(UImage* InImage)
{
	LowHealthVignetteImage = InImage;
}

UImage* UPlayerVitalHUDWidget::GetTestLowHealthVignetteImage() const
{
	return LowHealthVignetteImage;
}

bool UPlayerVitalHUDWidget::IsTestLowHealthVignetteVisible() const
{
	return LowHealthVignetteImage && LowHealthVignetteImage->GetVisibility() == ESlateVisibility::HitTestInvisible;
}

float UPlayerVitalHUDWidget::GetTestCurrentVignetteAlpha() const
{
	return CurrentVignetteAlpha;
}

float UPlayerVitalHUDWidget::GetTestLowHealthPulseWeight() const
{
	return LowHealthPulseWeight;
}

float UPlayerVitalHUDWidget::GetTestDamageFlashTimer() const
{
	return DamageFlashTimer;
}

void UPlayerVitalHUDWidget::SetTestHealthBarOverlay(UWidget* InWidget)
{
	HealthBarOverlay = InWidget;
}

UWidget* UPlayerVitalHUDWidget::GetTestHealthBarOverlay() const
{
	return HealthBarOverlay;
}

void UPlayerVitalHUDWidget::SetTestStaminaBarOverlay(UWidget* InWidget)
{
	StaminaBarOverlay = InWidget;
}

UWidget* UPlayerVitalHUDWidget::GetTestStaminaBarOverlay() const
{
	return StaminaBarOverlay;
}

float UPlayerVitalHUDWidget::GetTestHealthShakeTimer() const
{
	return HealthShakeTimer;
}

float UPlayerVitalHUDWidget::GetTestStaminaShakeTimer() const
{
	return StaminaShakeTimer;
}

float UPlayerVitalHUDWidget::GetTestHealthTranslationY() const
{
	UWidget* TargetWidget = HealthBarOverlay ? HealthBarOverlay.Get() : HealthProgressBar.Get();
	return TargetWidget ? TargetWidget->GetRenderTransform().Translation.Y : 0.0f;
}

float UPlayerVitalHUDWidget::GetTestStaminaTranslationY() const
{
	UWidget* TargetWidget = StaminaBarOverlay ? StaminaBarOverlay.Get() : StaminaProgressBar.Get();
	return TargetWidget ? TargetWidget->GetRenderTransform().Translation.Y : 0.0f;
}

void UPlayerVitalHUDWidget::SimulateTickForTesting(float InDeltaTime)
{
	UpdateBufferHealth(InDeltaTime);
	UpdateVignette(InDeltaTime);
	UpdateShake(InDeltaTime);
}
#endif
