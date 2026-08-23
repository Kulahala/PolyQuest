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

#if WITH_DEV_AUTOMATION_TESTS
	void SetTestHealthProgressBar(UProgressBar* InBar);
	UProgressBar* GetTestHealthProgressBar() const;
	void SetTestTargetHighlightImage(UImage* InImage);
	UImage* GetTestTargetHighlightImage() const;
#endif

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthProgressBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> TargetHighlightImage;
};
