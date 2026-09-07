// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/PlayerSkillBarHUDWidget.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayAbilitySpec.h"
#include "GameplayTagsManager.h"
#include "Combat/Equipment/WeaponEquipmentComponent.h"
#include "UI/PlayerSkillSlotWidget.h"

UPlayerSkillBarHUDWidget::UPlayerSkillBarHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetVisibility(ESlateVisibility::HitTestInvisible);
	DeadStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
}

void UPlayerSkillBarHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	const TArray<UPlayerSkillSlotWidget*> Slots = GetSlotWidgets();
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		if (Slots[Index])
		{
			Slots[Index]->InitializeSlot(Index);
		}
	}

	RefreshAllSlots();
}

void UPlayerSkillBarHUDWidget::NativeDestruct()
{
	Unbind();
	Super::NativeDestruct();
}

void UPlayerSkillBarHUDWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!BoundEquipmentComp.IsValid() || !BoundASC.IsValid() || !IsVisible())
	{
		return;
	}

	RefreshAllSlots();
}

void UPlayerSkillBarHUDWidget::BindToEquipmentAndASC(UWeaponEquipmentComponent* InEquipmentComp, UAbilitySystemComponent* InASC)
{
	Unbind();

	if (!InEquipmentComp || !InASC)
	{
		return;
	}

	if (InEquipmentComp->GetOwner() != InASC->GetOwnerActor())
	{
		return;
	}

	BoundEquipmentComp = InEquipmentComp;
	BoundASC = InASC;

	EquipmentChangedHandle = InEquipmentComp->OnPreparedSlotsChanged().AddUObject(this, &UPlayerSkillBarHUDWidget::OnEquipmentChanged);

	if (!DeadStateTag.IsValid())
	{
		DeadStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
	}

	if (DeadStateTag.IsValid())
	{
		DeadTagChangedHandle = InASC->RegisterGameplayTagEvent(DeadStateTag, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &UPlayerSkillBarHUDWidget::OnDeadTagChanged);
		bIsPlayerDead = InASC->HasMatchingGameplayTag(DeadStateTag);
	}

	RefreshAllSlots();
}

void UPlayerSkillBarHUDWidget::Unbind()
{
	if (BoundEquipmentComp.IsValid() && EquipmentChangedHandle.IsValid())
	{
		BoundEquipmentComp->OnPreparedSlotsChanged().Remove(EquipmentChangedHandle);
	}
	EquipmentChangedHandle.Reset();
	BoundEquipmentComp.Reset();

	if (BoundASC.IsValid() && DeadTagChangedHandle.IsValid())
	{
		BoundASC->RegisterGameplayTagEvent(DeadStateTag, EGameplayTagEventType::NewOrRemoved).Remove(DeadTagChangedHandle);
	}
	DeadTagChangedHandle.Reset();
	BoundASC.Reset();

	bIsPlayerDead = false;

	const TArray<UPlayerSkillSlotWidget*> Slots = GetSlotWidgets();
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		if (Slots[Index])
		{
			Slots[Index]->UpdateSlotState(EPlayerSkillSlotDisplayState::Empty, 0.0f);
		}
	}
}

void UPlayerSkillBarHUDWidget::OnEquipmentChanged()
{
	RefreshAllSlots();
}

void UPlayerSkillBarHUDWidget::OnDeadTagChanged(const FGameplayTag Tag, const int32 NewCount)
{
	bIsPlayerDead = (NewCount > 0);
	RefreshAllSlots();
}

TArray<UPlayerSkillSlotWidget*> UPlayerSkillBarHUDWidget::GetSlotWidgets() const
{
	return { Slot_1, Slot_2, Slot_3, Slot_4 };
}

void UPlayerSkillBarHUDWidget::RefreshAllSlots()
{
	const TArray<UPlayerSkillSlotWidget*> Slots = GetSlotWidgets();
	for (int32 SlotIndex = 0; SlotIndex < Slots.Num() && SlotIndex < UWeaponEquipmentComponent::PreparedSlotCount; ++SlotIndex)
	{
		UPlayerSkillSlotWidget* SlotWidget = Slots[SlotIndex];
		if (!SlotWidget)
		{
			continue;
		}

		SlotWidget->InitializeSlot(SlotIndex);

		if (!BoundEquipmentComp.IsValid() || !BoundASC.IsValid())
		{
			SlotWidget->UpdateSlotState(EPlayerSkillSlotDisplayState::Empty, 0.0f);
			continue;
		}

		TSubclassOf<UGameplayAbility> SlotClass;
		FGameplayAbilitySpecHandle SlotHandle;
		const bool bIsEmpty = BoundEquipmentComp->IsPreparedSlotEmpty(SlotIndex);
		const bool bHasValidBinding = BoundEquipmentComp->TryGetPreparedSlotBinding(SlotIndex, SlotClass, SlotHandle);

		if (!bHasValidBinding)
		{
			SlotWidget->UpdateSlotState(bIsEmpty ? EPlayerSkillSlotDisplayState::Empty : EPlayerSkillSlotDisplayState::Invalid, 0.0f);
			continue;
		}

		if (bIsPlayerDead)
		{
			SlotWidget->UpdateSlotState(EPlayerSkillSlotDisplayState::Invalid, 0.0f);
			continue;
		}

		UAbilitySystemComponent* ASC = BoundASC.Get();
		const FGameplayAbilityActorInfo* ActorInfo = ASC ? ASC->AbilityActorInfo.Get() : nullptr;
		if (!ActorInfo || ActorInfo->AbilitySystemComponent != ASC || !ActorInfo->OwnerActor.IsValid() || !ActorInfo->AvatarActor.IsValid())
		{
			SlotWidget->UpdateSlotState(EPlayerSkillSlotDisplayState::Invalid, 0.0f);
			continue;
		}

		const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(SlotHandle);
		if (!Spec || Spec->PendingRemove || !Spec->Ability || Spec->Ability->GetClass() != SlotClass)
		{
			SlotWidget->UpdateSlotState(EPlayerSkillSlotDisplayState::Invalid, 0.0f);
			continue;
		}

		float Remaining = 0.0f;
		float Duration = 0.0f;
		Spec->Ability->GetCooldownTimeRemainingAndDuration(SlotHandle, ActorInfo, Remaining, Duration);

		if (!FMath::IsFinite(Remaining) || !FMath::IsFinite(Duration))
		{
			SlotWidget->UpdateSlotState(EPlayerSkillSlotDisplayState::Invalid, 0.0f);
			continue;
		}

		if (Duration < 0.0f || Remaining < -0.05f)
		{
			SlotWidget->UpdateSlotState(EPlayerSkillSlotDisplayState::Invalid, 0.0f);
			continue;
		}

		if (Duration > 0.0f && Remaining > 0.001f)
		{
			const float Percent = FMath::Clamp(Remaining / Duration, 0.0f, 1.0f);
			SlotWidget->UpdateSlotState(EPlayerSkillSlotDisplayState::Cooldown, Percent);
		}
		else if (Remaining <= 0.001f && Duration >= 0.0f)
		{
			SlotWidget->UpdateSlotState(EPlayerSkillSlotDisplayState::Ready, 0.0f);
		}
		else
		{
			SlotWidget->UpdateSlotState(EPlayerSkillSlotDisplayState::Invalid, 0.0f);
		}
	}
}

#if WITH_DEV_AUTOMATION_TESTS
void UPlayerSkillBarHUDWidget::SetTestSlots(UPlayerSkillSlotWidget* S1, UPlayerSkillSlotWidget* S2, UPlayerSkillSlotWidget* S3, UPlayerSkillSlotWidget* S4)
{
	Slot_1 = S1;
	Slot_2 = S2;
	Slot_3 = S3;
	Slot_4 = S4;

	const TArray<UPlayerSkillSlotWidget*> Slots = GetSlotWidgets();
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		if (Slots[Index])
		{
			Slots[Index]->InitializeSlot(Index);
		}
	}
	RefreshAllSlots();
}

UPlayerSkillSlotWidget* UPlayerSkillBarHUDWidget::GetTestSlot(const int32 InSlotIndex) const
{
	switch (InSlotIndex)
	{
	case 0: return Slot_1;
	case 1: return Slot_2;
	case 2: return Slot_3;
	case 3: return Slot_4;
	default: return nullptr;
	}
}

void UPlayerSkillBarHUDWidget::SimulateTickForTesting(const float InDeltaTime)
{
	RefreshAllSlots();
}
#endif
