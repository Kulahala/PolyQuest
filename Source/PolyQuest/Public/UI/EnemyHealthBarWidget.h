// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EnemyHealthBarWidget.generated.h"

class UProgressBar;
class UImage;

/**
 * Native C++ base widget for overhead enemy health bar display.
 * It provides pure display setters without any numeric text, Poise, target, tick, or input logic.
 */
UCLASS(Blueprintable)
class POLYQUEST_API UEnemyHealthBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Updates displayed health progress bar. */
	void SetHealth(float Current, float Max);

	/** Shows or hides the existing Enemy bar's lock-on frame without changing Health state. */
	void SetLockOnHighlighted(bool bHighlighted);

	/** Triggers a micro-shake jolt on the enemy health bar on hit impact. */
	UFUNCTION(BlueprintCallable, Category = "Vital|Shake")
	void PlayHitShake();

#if WITH_DEV_AUTOMATION_TESTS
	void SetTestHealthProgressBar(UProgressBar* InBar);
	UProgressBar* GetTestHealthProgressBar() const;
	void SetTestTargetHighlightImage(UImage* InImage);
	UImage* GetTestTargetHighlightImage() const;
	void SetTestHealthBarOverlay(UWidget* InWidget);
	UWidget* GetTestHealthBarOverlay() const;
	float GetTestHitShakeTimer() const;
	float GetTestHitFlashTimer() const;
	FLinearColor GetTestHealthFillColor() const;
	FLinearColor GetTestHealthBaseColor() const;
	float GetTestHealthTranslationY() const;
	void SimulateTickForTesting(float InDeltaTime);
#endif

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	void UpdateShake(float InDeltaTime);
	void UpdateHitFlash(float InDeltaTime);
	float CalculateShakeOffset(float RemainingTimer) const;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> HealthBarOverlay;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthProgressBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> TargetHighlightImage;

	/** Base normal color for enemy health bar (dynamically captured from widget if valid, fallback #E74C3C). */
	UPROPERTY(EditDefaultsOnly, Category = "Vital|Color")
	FLinearColor HealthBaseColor = FLinearColor(0.85f, 0.2f, 0.2f, 1.0f);

	/** Duration in seconds of the white hit flash upon taking damage. */
	UPROPERTY(EditDefaultsOnly, Category = "Vital|Color", meta = (ClampMin = "0.01"))
	float HitFlashDuration = 0.15f;

	/** Max vertical displacement in pixels for hit impact micro-shake. */
	UPROPERTY(EditDefaultsOnly, Category = "Vital|Shake", meta = (ClampMin = "0.0"))
	float ShakeMaxDisplacement = 3.0f;

	/** Duration in seconds for micro-shake. */
	UPROPERTY(EditDefaultsOnly, Category = "Vital|Shake", meta = (ClampMin = "0.01"))
	float ShakeDuration = 0.12f;

	/** Frequency of oscillation for micro-shake in Hz. */
	UPROPERTY(EditDefaultsOnly, Category = "Vital|Shake", meta = (ClampMin = "1.0"))
	float ShakeFrequency = 28.0f;

private:
	float HitShakeTimer = 0.0f;
	float HitFlashTimer = 0.0f;
	float TargetHealthPercent = 1.0f;
	bool bIsHealthInitialized = false;
	bool bHasCapturedHealthBaseColor = false;
};
