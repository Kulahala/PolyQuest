// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PlayerSkillSlotWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Materials/MaterialInstanceDynamic.h"

const FName UPlayerSkillSlotWidget::CooldownPercentParameterName = FName(TEXT("CooldownPercent"));

UPlayerSkillSlotWidget::UPlayerSkillSlotWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UPlayerSkillSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	EnsureMIDCreated();

	if (SlotIndex != INDEX_NONE && !bHasInitializedNumber)
	{
		InitializeSlot(SlotIndex);
	}

	ApplyVisualState();
}

void UPlayerSkillSlotWidget::InitializeSlot(const int32 InSlotIndex)
{
	if (SlotIndex == InSlotIndex && bHasInitializedNumber)
	{
		return;
	}

	SlotIndex = InSlotIndex;
	if (SlotNumberText)
	{
		SlotNumberText->SetText(FText::AsNumber(SlotIndex + 1));
		bHasInitializedNumber = true;
	}
}

void UPlayerSkillSlotWidget::UpdateSlotState(const EPlayerSkillSlotDisplayState NewState, const float InCooldownPercent)
{
	const float SafePercent = FMath::IsFinite(InCooldownPercent) ? FMath::Clamp(InCooldownPercent, 0.0f, 1.0f) : 0.0f;

	const bool bStateChanged = (CurrentState != NewState);
	const bool bPercentChanged = !FMath::IsNearlyEqual(CurrentCooldownPercent, SafePercent, 0.0005f);

	CurrentState = NewState;
	CurrentCooldownPercent = (NewState == EPlayerSkillSlotDisplayState::Cooldown) ? SafePercent : 0.0f;

	if (bStateChanged || bPercentChanged)
	{
		ApplyVisualState();
	}
}

void UPlayerSkillSlotWidget::EnsureMIDCreated()
{
	if (!CooldownSweepMID && CooldownSweepImage)
	{
		CooldownSweepMID = CooldownSweepImage->GetDynamicMaterial();
	}
}

void UPlayerSkillSlotWidget::ApplyVisualState()
{
	if (SlotNumberText && (!bHasInitializedNumber && SlotIndex != INDEX_NONE))
	{
		SlotNumberText->SetText(FText::AsNumber(SlotIndex + 1));
		bHasInitializedNumber = true;
	}

	if (BackgroundImage)
	{
		BackgroundImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	if (SlotNumberText)
	{
		SlotNumberText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	if (CooldownOverlay)
	{
		const bool bShowOverlay = (CurrentState == EPlayerSkillSlotDisplayState::Cooldown || CurrentState == EPlayerSkillSlotDisplayState::Invalid);
		CooldownOverlay->SetVisibility(bShowOverlay ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (CooldownSweepImage)
	{
		const bool bShowSweep = (CurrentState == EPlayerSkillSlotDisplayState::Cooldown) && (CooldownSweepMID != nullptr);
		CooldownSweepImage->SetVisibility(bShowSweep ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

		if (CooldownSweepMID)
		{
			CooldownSweepMID->SetScalarParameterValue(CooldownPercentParameterName, CurrentCooldownPercent);
		}
	}
}

#if WITH_DEV_AUTOMATION_TESTS
void UPlayerSkillSlotWidget::SetTestControls(UImage* InBackground, UTextBlock* InSlotNumber, UWidget* InOverlay, UImage* InSweep)
{
	BackgroundImage = InBackground;
	SlotNumberText = InSlotNumber;
	CooldownOverlay = InOverlay;
	CooldownSweepImage = InSweep;
	bHasInitializedNumber = false;
	ApplyVisualState();
}

void UPlayerSkillSlotWidget::SetTestMID(UMaterialInstanceDynamic* InMID)
{
	CooldownSweepMID = InMID;
	ApplyVisualState();
}

bool UPlayerSkillSlotWidget::IsTestOverlayVisible() const
{
	return CooldownOverlay ? (CooldownOverlay->GetVisibility() == ESlateVisibility::HitTestInvisible || CooldownOverlay->GetVisibility() == ESlateVisibility::Visible) : false;
}

bool UPlayerSkillSlotWidget::IsTestSweepVisible() const
{
	return CooldownSweepImage ? (CooldownSweepImage->GetVisibility() == ESlateVisibility::HitTestInvisible || CooldownSweepImage->GetVisibility() == ESlateVisibility::Visible) : false;
}
#endif
