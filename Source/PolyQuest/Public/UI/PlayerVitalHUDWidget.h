// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerVitalHUDWidget.generated.h"

class UProgressBar;
class UTextBlock;
class UWidget;
class UImage;

/**
 * Native C++ base widget for the player vital HUD (Health and Stamina).
 * It provides pure display setters without any GAS, gameplay, tick, or input logic.
 */
UCLASS(Blueprintable)
class POLYQUEST_API UPlayerVitalHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Updates displayed health progress bar and numeric text. */
	void SetHealth(float Current, float Max);

	/** Updates displayed stamina progress bar and numeric text. */
	void SetStamina(float Current, float Max);

	/** Updates displayed stamina exhaustion overlay visibility. */
	void SetExhausted(bool bIsExhausted);

	/** Triggers a micro-shake jolt on the health bar (e.g. on hit impact). */
	UFUNCTION(BlueprintCallable, Category = "Vital|Shake")
	void PlayHealthShake();

	/** Triggers a micro-shake rejection nudge on the stamina bar (e.g. when stamina is depleted or action rejected). */
	UFUNCTION(BlueprintCallable, Category = "Vital|Shake")
	void PlayStaminaRejectionShake();

#if WITH_DEV_AUTOMATION_TESTS
	void SetTestHealthWidgets(UProgressBar* InBar, UTextBlock* InCurr, UTextBlock* InMax);
	void SetTestHealthBufferProgressBar(UProgressBar* InBar);
	void SetTestHealthBarOverlay(UWidget* InWidget);
	void SetTestStaminaWidgets(UProgressBar* InBar, UTextBlock* InCurr, UTextBlock* InMax);
	void SetTestStaminaExhaustedOverlay(UWidget* InWidget);
	void SetTestStaminaBarOverlay(UWidget* InWidget);
	void SetTestLowHealthVignetteImage(UImage* InImage);
	UProgressBar* GetTestHealthProgressBar() const;
	UProgressBar* GetTestHealthBufferProgressBar() const;
	UWidget* GetTestHealthBarOverlay() const;
	UTextBlock* GetTestHealthCurrentText() const;
	UTextBlock* GetTestHealthMaxText() const;
	UProgressBar* GetTestStaminaProgressBar() const;
	UTextBlock* GetTestStaminaCurrentText() const;
	UTextBlock* GetTestStaminaMaxText() const;
	UWidget* GetTestStaminaExhaustedOverlay() const;
	UWidget* GetTestStaminaBarOverlay() const;
	UImage* GetTestLowHealthVignetteImage() const;
	bool IsTestExhaustedOverlayVisible() const;
	bool IsTestLowHealthVignetteVisible() const;
	float GetTestBufferDelayTimer() const;
	float GetTestCurrentBufferPercent() const;
	float GetTestCurrentVignetteAlpha() const;
	float GetTestLowHealthPulseWeight() const;
	float GetTestDamageFlashTimer() const;
	float GetTestBufferDamageFlashTimer() const;
	FLinearColor GetTestBufferFillColor() const;
	FLinearColor GetTestHealthBufferBaseColor() const;
	float GetTestStaminaChargeFlashTimer() const;
	FLinearColor GetTestStaminaFillColor() const;
	FLinearColor GetTestStaminaBaseColor() const;
	float GetTestLastStaminaPercent() const;
	float GetTestHealthShakeTimer() const;
	float GetTestStaminaShakeTimer() const;
	float GetTestHealthTranslationY() const;
	float GetTestStaminaTranslationY() const;
	float GetTestTargetHealthPercent() const;
	float GetTestCurrentHealthPercent() const;
	float GetTestTargetStaminaPercent() const;
	float GetTestCurrentStaminaPercent() const;
	void SimulateTickForTesting(float InDeltaTime);
#endif

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	void UpdateHealth(const float InDeltaTime);
	void UpdateBufferHealth(float InDeltaTime);
	void UpdateBufferDamageFlash(float InDeltaTime);
	void UpdateStamina(const float InDeltaTime);
	void UpdateStaminaChargeFlash(float InDeltaTime);
	void UpdateVignette(float InDeltaTime);
	void UpdateShake(float InDeltaTime);
	float CalculateShakeOffset(float RemainingTimer) const;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> HealthBarOverlay;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthProgressBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> HealthBufferProgressBar;

	/** Delay in seconds before buffer health starts catching up after damage. */
	UPROPERTY(EditDefaultsOnly, Category = "Vital|Buffer", meta = (ClampMin = "0.0"))
	float BufferCatchUpDelay = 0.5f;

	/** Speed of buffer catch up interpolation. */
	UPROPERTY(EditDefaultsOnly, Category = "Vital|Buffer", meta = (ClampMin = "0.1"))
	float BufferCatchUpSpeed = 4.0f;

	/** Base normal color for health buffer bar (dynamically captured from widget if valid, fallback #DDAA00). */
	UPROPERTY(EditDefaultsOnly, Category = "Vital|Buffer")
	FLinearColor HealthBufferBaseColor = FLinearColor(0.73f, 0.41f, 0.0f, 1.0f);

	/** Duration in seconds of the white impact crest flash on buffer bar upon taking damage. */
	UPROPERTY(EditDefaultsOnly, Category = "Vital|Buffer", meta = (ClampMin = "0.01"))
	float BufferDamageFlashDuration = 0.15f;

	/** Interp speed for health recovery / healing fill-up. */
	UPROPERTY(EditDefaultsOnly, Category = "Vital|Buffer", meta = (ClampMin = "0.1"))
	float HealthRegenInterpSpeed = 8.0f;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> StaminaBarOverlay;

	/** Interp speed for stamina natural recovery / regeneration. */
	UPROPERTY(EditDefaultsOnly, Category = "Vital|Stamina", meta = (ClampMin = "0.1"))
	float StaminaRegenInterpSpeed = 6.0f;

	/** Interp speed for stamina consumption / drain (fast catch-up to soften steps). */
	UPROPERTY(EditDefaultsOnly, Category = "Vital|Stamina", meta = (ClampMin = "0.1"))
	float StaminaDrainInterpSpeed = 12.0f;

	/** Base normal color for stamina bar (dynamically captured from widget if valid, fallback #2ECC71). */
	UPROPERTY(EditDefaultsOnly, Category = "Vital|Stamina")
	FLinearColor StaminaBaseColor = FLinearColor(0.18f, 0.80f, 0.44f, 1.0f);

	/** Bright fluorescent white-green color for stamina full charge flash. */
	UPROPERTY(EditDefaultsOnly, Category = "Vital|Stamina")
	FLinearColor StaminaFullChargeFlashColor = FLinearColor(0.85f, 1.0f, 0.88f, 1.0f);

	/** Duration in seconds of the stamina full charge flash fade-out. */
	UPROPERTY(EditDefaultsOnly, Category = "Vital|Stamina", meta = (ClampMin = "0.01"))
	float StaminaFullChargeFlashDuration = 0.20f;

	/** Max vertical displacement in pixels for hit and rejection micro-shake. */
	UPROPERTY(EditDefaultsOnly, Category = "Vital|Shake", meta = (ClampMin = "0.0"))
	float ShakeMaxDisplacement = 4.0f;

	/** Duration in seconds for micro-shake. */
	UPROPERTY(EditDefaultsOnly, Category = "Vital|Shake", meta = (ClampMin = "0.01"))
	float ShakeDuration = 0.16f;

	/** Frequency of oscillation for micro-shake in Hz. */
	UPROPERTY(EditDefaultsOnly, Category = "Vital|Shake", meta = (ClampMin = "1.0"))
	float ShakeFrequency = 25.0f;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> LowHealthVignetteImage;

	/** Health percentage threshold to trigger low health vignette pulse (0.0 - 1.0). */
	UPROPERTY(EditDefaultsOnly, Category = "Vital|Vignette", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LowHealthThreshold = 0.25f;

	/** Oscillation period in seconds for low health pulse. */
	UPROPERTY(EditDefaultsOnly, Category = "Vital|Vignette", meta = (ClampMin = "0.1"))
	float LowHealthPulsePeriod = 1.1f;

	/** Minimum opacity of vignette during pulse oscillation. */
	UPROPERTY(EditDefaultsOnly, Category = "Vital|Vignette", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LowHealthPulseMinAlpha = 0.08f;

	/** Maximum opacity of vignette during pulse oscillation. */
	UPROPERTY(EditDefaultsOnly, Category = "Vital|Vignette", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LowHealthPulseMaxAlpha = 0.40f;

	/** Duration in seconds to fade out low health pulse when health recovers above threshold. */
	UPROPERTY(EditDefaultsOnly, Category = "Vital|Vignette", meta = (ClampMin = "0.05"))
	float LowHealthFadeOutDuration = 0.5f;

	/** Duration in seconds of damage hit flash vignette. */
	UPROPERTY(EditDefaultsOnly, Category = "Vital|Vignette", meta = (ClampMin = "0.01"))
	float DamageFlashDuration = 0.14f;

	/** Peak opacity of damage hit flash vignette. */
	UPROPERTY(EditDefaultsOnly, Category = "Vital|Vignette", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DamageFlashMaxAlpha = 0.40f;

	/** Global clamp ceiling for combined vignette alpha to ensure center vision stays clear. */
	UPROPERTY(EditDefaultsOnly, Category = "Vital|Vignette", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxAllowedVignetteAlpha = 0.45f;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> HealthCurrentText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> HealthMaxText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> StaminaProgressBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> StaminaExhaustedOverlay;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> StaminaCurrentText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> StaminaMaxText;

private:
	float TargetHealthPercent = 0.0f;
	float CurrentHealthPercent = 0.0f;
	float CurrentBufferPercent = 0.0f;
	float TargetStaminaPercent = 1.0f;
	float CurrentStaminaPercent = 1.0f;
	float BufferDelayTimer = 0.0f;
	float BufferDamageFlashTimer = 0.0f;
	float StaminaChargeFlashTimer = 0.0f;
	float LastStaminaPercent = 1.0f;
	float LowHealthPulseWeight = 0.0f;
	float LowHealthPulseTimer = 0.0f;
	float DamageFlashTimer = 0.0f;
	float CurrentVignetteAlpha = 0.0f;
	float HealthShakeTimer = 0.0f;
	float StaminaShakeTimer = 0.0f;
	bool bIsHealthInitialized = false;
	bool bIsStaminaInitialized = false;
	bool bHasCapturedBufferBaseColor = false;
	bool bHasCapturedStaminaBaseColor = false;
	bool bWasExhausted = false;
};
