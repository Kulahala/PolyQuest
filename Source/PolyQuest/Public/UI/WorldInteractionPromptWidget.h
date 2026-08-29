// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WorldInteractionPromptWidget.generated.h"

class UTextBlock;

/**
 * Passive world interaction prompt widget that displays candidate pickup prompt text.
 * Owns no input, candidate arbitration, GAS, equipment mutations, timer, or tick.
 */
UCLASS()
class POLYQUEST_API UWorldInteractionPromptWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Updates the text displayed in the prompt text block; null-safe. */
	void SetPromptText(const FText& InText);

#if WITH_DEV_AUTOMATION_TESTS
	void SetTestPromptTextBlock(UTextBlock* InTextBlock);
	UTextBlock* GetTestPromptTextBlock() const;
#endif

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PromptText;
};
