// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "PlayerSkillBarHUDWidget.generated.h"

class UPlayerSkillSlotWidget;
class UWeaponEquipmentComponent;
class UAbilitySystemComponent;

/**
 * Native C++ aggregation widget managing prepared skill slots 1..4.
 * Interrogates UWeaponEquipmentComponent for bindings and UAbilitySystemComponent
 * for authoritative cooldown state without maintaining local timers.
 */
UCLASS(Blueprintable)
class POLYQUEST_API UPlayerSkillBarHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPlayerSkillBarHUDWidget(const FObjectInitializer& ObjectInitializer);

	/** Binds to a player's equipment and ASC, registers delegates, and forces an immediate refresh. */
	void BindToEquipmentAndASC(UWeaponEquipmentComponent* InEquipmentComp, UAbilitySystemComponent* InASC);

	/** Unbinds delegates, clears weak references, and resets slots to safe neutral states. */
	void Unbind();

	/** Refreshes all four slots from live equipment and ASC state. */
	void RefreshAllSlots();

	UWeaponEquipmentComponent* GetBoundEquipmentComponent() const { return BoundEquipmentComp.Get(); }
	UAbilitySystemComponent* GetBoundAbilitySystemComponent() const { return BoundASC.Get(); }
	bool IsPlayerDead() const { return bIsPlayerDead; }

#if WITH_DEV_AUTOMATION_TESTS
	void SetTestSlots(UPlayerSkillSlotWidget* S1, UPlayerSkillSlotWidget* S2, UPlayerSkillSlotWidget* S3, UPlayerSkillSlotWidget* S4);
	UPlayerSkillSlotWidget* GetTestSlot(int32 InSlotIndex) const;
	void SimulateTickForTesting(float InDeltaTime);
#endif

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPlayerSkillSlotWidget> Slot_1;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPlayerSkillSlotWidget> Slot_2;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPlayerSkillSlotWidget> Slot_3;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPlayerSkillSlotWidget> Slot_4;

private:
	void OnEquipmentChanged();
	void OnDeadTagChanged(const FGameplayTag Tag, int32 NewCount);
	TArray<UPlayerSkillSlotWidget*> GetSlotWidgets() const;

	TWeakObjectPtr<UWeaponEquipmentComponent> BoundEquipmentComp;
	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	FDelegateHandle EquipmentChangedHandle;
	FDelegateHandle DeadTagChangedHandle;
	FGameplayTag DeadStateTag;
	bool bIsPlayerDead = false;
};
