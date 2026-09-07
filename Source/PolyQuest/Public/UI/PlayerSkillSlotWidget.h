// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerSkillSlotWidget.generated.h"

class UImage;
class UTextBlock;
class UMaterialInstanceDynamic;

/**
 * Display states for a prepared skill slot.
 */
UENUM(BlueprintType)
enum class EPlayerSkillSlotDisplayState : uint8
{
	Empty,
	Ready,
	Cooldown,
	Invalid
};

/**
 * Native C++ widget representing a single prepared skill slot (1..4).
 * Enforces the four-state display model and single-creation MID contract.
 */
UCLASS(Blueprintable)
class POLYQUEST_API UPlayerSkillSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	static const FName CooldownPercentParameterName;

	UPlayerSkillSlotWidget(const FObjectInitializer& ObjectInitializer);

	/** Initializes the display slot index (0..3, rendered as 1..4). */
	void InitializeSlot(int32 InSlotIndex);

	/**
	 * Updates the slot display state and normalized cooldown percent [0.0, 1.0].
	 * InCooldownPercent is clamped and validated for finiteness.
	 */
	void UpdateSlotState(EPlayerSkillSlotDisplayState NewState, float InCooldownPercent);

	int32 GetSlotIndex() const { return SlotIndex; }
	EPlayerSkillSlotDisplayState GetCurrentDisplayState() const { return CurrentState; }
	float GetCurrentCooldownPercent() const { return CurrentCooldownPercent; }

#if WITH_DEV_AUTOMATION_TESTS
	void SetTestControls(UImage* InBackground, UTextBlock* InSlotNumber, UWidget* InOverlay, UImage* InSweep);
	void SetTestMID(UMaterialInstanceDynamic* InMID);
	UMaterialInstanceDynamic* GetTestMID() const { return CooldownSweepMID; }
	bool IsTestOverlayVisible() const;
	bool IsTestSweepVisible() const;
#endif

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> BackgroundImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SlotNumberText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> CooldownOverlay;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> CooldownSweepImage;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CooldownSweepMID;

private:
	void EnsureMIDCreated();
	void ApplyVisualState();

	int32 SlotIndex = INDEX_NONE;
	EPlayerSkillSlotDisplayState CurrentState = EPlayerSkillSlotDisplayState::Empty;
	float CurrentCooldownPercent = 0.0f;
	bool bHasInitializedNumber = false;
};
