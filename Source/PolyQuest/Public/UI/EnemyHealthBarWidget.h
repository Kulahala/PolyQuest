// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EnemyHealthBarWidget.generated.h"

class UProgressBar;

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

#if WITH_DEV_AUTOMATION_TESTS
	void SetTestHealthProgressBar(UProgressBar* InBar);
	UProgressBar* GetTestHealthProgressBar() const;
#endif

protected:
	UPROPERTY(meta = (BindWidget))
TObjectPtr<UProgressBar> HealthProgressBar;
};
