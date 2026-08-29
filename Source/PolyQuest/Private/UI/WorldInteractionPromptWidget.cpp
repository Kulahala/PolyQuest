// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/WorldInteractionPromptWidget.h"
#include "Components/TextBlock.h"

void UWorldInteractionPromptWidget::SetPromptText(const FText& InText)
{
	if (PromptText)
	{
		PromptText->SetText(InText);
	}
}

#if WITH_DEV_AUTOMATION_TESTS
void UWorldInteractionPromptWidget::SetTestPromptTextBlock(UTextBlock* InTextBlock)
{
	PromptText = InTextBlock;
}

UTextBlock* UWorldInteractionPromptWidget::GetTestPromptTextBlock() const
{
	return PromptText.Get();
}
#endif
