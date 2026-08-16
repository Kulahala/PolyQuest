#include "Combat/Equipment/WeaponEquipmentComponent.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Character/Player/PlayerCharacter.h"
#include "Combat/Equipment/CombatActionDefinition.h"
#include "Combat/Equipment/MeleeWeaponDefinition.h"
#include "Combat/Equipment/WeaponDefinition.h"
#include "Combat/Input/CombatLoadoutDefinition.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameplayAbilitySpec.h"
#include "PolyQuest.h"

UWeaponEquipmentComponent::UWeaponEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);

	AttackingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Attacking")), false);
	GuardingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Guarding")), false);
	ParryingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Parrying")), false);
	DodgingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Dodging")), false);
	PrimaryAttackAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Primary")), false);
	GuardInputTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Input.Guard")), false);
	ParryInputTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Input.Parry")), false);
	PrimaryAttackInputTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false);
	DefaultGuardAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Defense.Guard")), false);
	DefaultParryAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Defense.Parry")), false);

	PreparedSlotActions.Init(nullptr, PreparedSlotCount);
	PreparedSlotHandles = TArray<FGameplayAbilitySpecHandle>();
	PreparedSlotHandles.SetNum(PreparedSlotCount);
}

bool UWeaponEquipmentComponent::EquipWeapon(UWeaponDefinition* Definition)
{
	if (!Definition)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Weapon equipment on '%s' rejected a null weapon definition."), *GetNameSafe(GetOwner()));
		return false;
	}

	const bool bTargetIsMainHand = Definition->HandSlot != EWeaponHandSlot::OffHand;
	const bool bSameSlotDefinition = bTargetIsMainHand
		? CurrentMainHandWeapon == Definition
		: CurrentOffHandWeapon == Definition;
	if (bSameSlotDefinition)
	{
		return true;
	}

	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetOwner());
	UAbilitySystemComponent* CharacterASC = PlayerCharacter ? PlayerCharacter->GetAbilitySystemComponent() : nullptr;
	if (!CanSwapNow(CharacterASC))
	{
		if (!bSwapRefusalWarningIssued)
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("Weapon equipment on '%s' refused a swap while a combat action is active."), *GetNameSafe(GetOwner()));
			bSwapRefusalWarningIssued = true;
		}
		return false;
	}

	UWeaponDefinition* NewMainHand = bTargetIsMainHand ? Definition : CurrentMainHandWeapon.Get();
	UWeaponDefinition* NewOffHand = bTargetIsMainHand ? CurrentOffHandWeapon.Get() : Definition;

	TArray<UCombatActionDefinition*> ComputedPreparedActions;
	ComputeKeepIfCompatibleLayout(NewMainHand, NewOffHand, ComputedPreparedActions);
	FString PreflightReason;
	if (!RunPreflight(PlayerCharacter, CharacterASC, Definition, bTargetIsMainHand, ComputedPreparedActions, PreflightReason))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Weapon equipment on '%s' failed its preflight: %s"), *GetNameSafe(GetOwner()), *PreflightReason);
		return false;
	}

	// Snapshot before any mutation; the restore path rebuilds these identities.
	UWeaponDefinition* OldMainHand = CurrentMainHandWeapon;
	UWeaponDefinition* OldOffHand = CurrentOffHandWeapon;
	UCombatLoadoutDefinition* OldLoadout = PlayerCharacter ? PlayerCharacter->GetActiveCombatLoadout() : nullptr;
	TArray<UCombatActionDefinition*> OldPreparedActions;
	for (const TObjectPtr<UCombatActionDefinition>& SlotAction : PreparedSlotActions)
	{
		OldPreparedActions.Add(SlotAction.Get());
	}

	TeardownEquippedWeapons();

	if (!ApplyComposition(PlayerCharacter, CharacterASC, NewMainHand, NewOffHand, ComputedPreparedActions))
	{
		if (ApplyComposition(PlayerCharacter, CharacterASC, OldMainHand, OldOffHand, OldPreparedActions)
			&& (!OldLoadout || PlayerCharacter->SetActiveCombatLoadout(OldLoadout)))
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("Weapon equipment on '%s' failed mid-apply and restored the previous composition."), *GetNameSafe(GetOwner()));
			return false;
		}

		UE_LOG(LogPolyQuest, Error, TEXT("Weapon equipment on '%s' failed mid-apply and could not restore a previous composition; the player has no valid melee weapon. This is a fatal configuration error."), *GetNameSafe(GetOwner()));
		return false;
	}

	bSwapRefusalWarningIssued = false;
	return true;
}

UMeleeWeaponDefinition* UWeaponEquipmentComponent::GetEquippedMainHandMelee() const
{
	return Cast<UMeleeWeaponDefinition>(CurrentMainHandWeapon);
}

bool UWeaponEquipmentComponent::TryGetBladeMarkers(USceneComponent*& OutBladeBase, USceneComponent*& OutBladeTip) const
{
	OutBladeBase = MainHandBladeBaseMarker.Get();
	OutBladeTip = MainHandBladeTipMarker.Get();
	return Cast<UMeleeWeaponDefinition>(CurrentMainHandWeapon) && OutBladeBase && OutBladeTip;
}

bool UWeaponEquipmentComponent::TryResolveInputIntent(const FGameplayTag& InputIntentTag, FGameplayTag& OutAbilityTag) const
{
	OutAbilityTag = FGameplayTag();

	if (InputIntentTag == GuardInputTag || InputIntentTag == ParryInputTag)
	{
		return ResolveDefenseAbilityTag(InputIntentTag == GuardInputTag, OutAbilityTag);
	}

	if (InputIntentTag == PrimaryAttackInputTag)
	{
		const UCombatLoadoutDefinition* BaseInputProfile = CurrentMainHandWeapon ? CurrentMainHandWeapon->AssociatedLoadout : nullptr;
		return BaseInputProfile && BaseInputProfile->TryGetAbilityTagForInputIntent(InputIntentTag, OutAbilityTag);
	}

	return false;
}

bool UWeaponEquipmentComponent::TryGetSprintAttackAbilityTag(FGameplayTag& OutAbilityTag) const
{
	OutAbilityTag = FGameplayTag();
	const UCombatLoadoutDefinition* BaseInputProfile = CurrentMainHandWeapon ? CurrentMainHandWeapon->AssociatedLoadout : nullptr;
	return BaseInputProfile && BaseInputProfile->TryGetSprintAttackAbilityTag(OutAbilityTag);
}

bool UWeaponEquipmentComponent::TryActivatePreparedSlot(int32 SlotIndex)
{
	if (SlotIndex < 0 || SlotIndex >= PreparedSlotCount || !PreparedSlotActions.IsValidIndex(SlotIndex))
	{
		return false;
	}

	if (SlotIndex < 0 || SlotIndex >= PreparedSlotCount)
	{
		return false;
	}

	if (!PreparedSlotActions.IsValidIndex(SlotIndex) || !PreparedSlotActions[SlotIndex])
	{
		// An empty prepared slot is a legal no-op.
		return false;
	}

	UCombatActionDefinition* SlotAction = PreparedSlotActions[SlotIndex];
	const FGameplayAbilitySpecHandle& SlotHandle = PreparedSlotHandles[SlotIndex];
	UAbilitySystemComponent* CharacterASC = GetOwner() ? GetOwner()->FindComponentByClass<UAbilitySystemComponent>() : nullptr;
	if (!CharacterASC || !SlotHandle.IsValid() || !GrantedAbilitySpecHandles.Contains(SlotHandle))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Weapon equipment on '%s' refused prepared slot %d: its handle is not a current grant of this component."), *GetNameSafe(GetOwner()), SlotIndex + 1);
		return false;
	}

	const FGameplayAbilitySpec* SlotSpec = CharacterASC->FindAbilitySpecFromHandle(SlotHandle);
	if (!SlotSpec || SlotSpec->Ability != SlotAction->AbilityClass.GetDefaultObject())
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Weapon equipment on '%s' refused prepared slot %d: the granted ability no longer matches the slot's action."), *GetNameSafe(GetOwner()), SlotIndex + 1);
		return false;
	}

	return CharacterASC->TryActivateAbility(SlotHandle);
}

void UWeaponEquipmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	TeardownEquippedWeapons();
	Super::EndPlay(EndPlayReason);
}

bool UWeaponEquipmentComponent::CanSwapNow(const UAbilitySystemComponent* CharacterASC) const
{
	if (!CharacterASC
		|| (AttackingStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(AttackingStateTag))
		|| (GuardingStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(GuardingStateTag))
		|| (ParryingStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(ParryingStateTag))
		|| (DodgingStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(DodgingStateTag)))
	{
		return false;
	}

	// The Primary hold/release arbitration window owns only input-block tags, not a
	// combat action tag, so an active Primary spec is checked directly to keep a
	// swap from re-routing an in-flight attack decision.
	if (PrimaryAttackAbilityTag.IsValid())
	{
		FGameplayTagContainer PrimaryAbilityTags;
		PrimaryAbilityTags.AddTag(PrimaryAttackAbilityTag);
		TArray<FGameplayAbilitySpec*> PrimaryAbilitySpecs;
		CharacterASC->GetActivatableGameplayAbilitySpecsByAllMatchingTags(PrimaryAbilityTags, PrimaryAbilitySpecs, false);
		for (const FGameplayAbilitySpec* PrimaryAbilitySpec : PrimaryAbilitySpecs)
		{
			if (PrimaryAbilitySpec && PrimaryAbilitySpec->IsActive())
			{
				return false;
			}
		}
	}

	return true;
}

bool UWeaponEquipmentComponent::RunPreflight(APlayerCharacter* PlayerCharacter, UAbilitySystemComponent* CharacterASC, UWeaponDefinition* Definition, bool bTargetIsMainHand, const TArray<UCombatActionDefinition*>& ComputedPreparedActions, FString& OutReason) const
{
	OutReason.Empty();
	if (!Definition->IsValidWeaponDefinition(OutReason))
	{
		return false;
	}

	if (!PlayerCharacter)
	{
		OutReason = TEXT("the owning actor is not an APlayerCharacter.");
		return false;
	}

	const USkeletalMeshComponent* OwnerMesh = PlayerCharacter->GetMesh();
	if (!OwnerMesh)
	{
		OutReason = TEXT("the owner has no skeletal mesh.");
		return false;
	}

	if (!OwnerMesh->DoesSocketExist(Definition->AttachSocketName))
	{
		OutReason = FString::Printf(TEXT("the owner mesh does not contain socket '%s'."), *Definition->AttachSocketName.ToString());
		return false;
	}

	if (!CharacterASC || !PlayerCharacter->HasAuthority())
	{
		OutReason = TEXT("the owner has no authoritative Ability System Component.");
		return false;
	}

	UWeaponDefinition* NewMainHand = bTargetIsMainHand ? Definition : CurrentMainHandWeapon.Get();
	UWeaponDefinition* NewOffHand = bTargetIsMainHand ? CurrentOffHandWeapon.Get() : Definition;
	if (NewMainHand && NewMainHand->HandSlot == EWeaponHandSlot::MainHandTwoHanded && NewOffHand)
	{
		OutReason = TEXT("a TwoHanded main hand cannot coexist with an off-hand item; the combined drop-swap belongs to TODO-03A3.");
		return false;
	}

	// Collect every ability class the new composition would grant: both slots'
	// base grants plus the computed prepared layout, rejecting duplicates.
	TArray<TSubclassOf<UGameplayAbility>> GrantClasses;
	auto AppendBaseGrants = [&GrantClasses](const UWeaponDefinition* SlotDefinition, FString& Reason) -> bool
	{
		const UMeleeWeaponDefinition* MeleeDefinition = Cast<UMeleeWeaponDefinition>(SlotDefinition);
		if (!MeleeDefinition)
		{
			return true;
		}

		for (const TSubclassOf<UGameplayAbility>& AbilityClass : MeleeDefinition->GrantedWeaponAbilities)
		{
			if (GrantClasses.Contains(AbilityClass))
			{
				Reason = FString::Printf(TEXT("ability '%s' would be granted twice by the new composition."), *GetNameSafe(AbilityClass));
				return false;
			}

			GrantClasses.Add(AbilityClass);
		}

		return true;
	};

	if (!AppendBaseGrants(NewMainHand, OutReason) || !AppendBaseGrants(NewOffHand, OutReason))
	{
		return false;
	}

	for (UCombatActionDefinition* PreparedAction : ComputedPreparedActions)
	{
		if (!PreparedAction)
		{
			// An empty prepared slot is a legal no-op layout entry.
			continue;
		}

		if (!PreparedAction->AbilityClass)
		{
			OutReason = TEXT("the computed prepared layout contains an action without an ability class.");
			return false;
		}

		if (GrantClasses.Contains(PreparedAction->AbilityClass))
		{
			OutReason = FString::Printf(TEXT("ability '%s' would be granted twice by the new composition."), *GetNameSafe(PreparedAction->AbilityClass));
			return false;
		}

		GrantClasses.Add(PreparedAction->AbilityClass);
	}

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : GrantClasses)
	{
		if (PlayerCharacter->IsStartupAbilityClass(AbilityClass))
		{
			OutReason = FString::Printf(TEXT("ability '%s' is also granted by the character's StartupAbilities."), *GetNameSafe(AbilityClass));
			return false;
		}

		const UGameplayAbility* AbilityCDO = AbilityClass.GetDefaultObject();
		for (const FGameplayAbilitySpec& ExistingSpec : CharacterASC->GetActivatableAbilities())
		{
			if (ExistingSpec.Ability == AbilityCDO && !GrantedAbilitySpecHandles.Contains(ExistingSpec.Handle))
			{
				OutReason = FString::Printf(TEXT("ability '%s' already exists on the ASC outside this component's granted handles."), *GetNameSafe(AbilityClass));
				return false;
			}
		}
	}

	return true;
}

void UWeaponEquipmentComponent::TeardownEquippedWeapons()
{
	if (UAbilitySystemComponent* CharacterASC = GetOwner() ? GetOwner()->FindComponentByClass<UAbilitySystemComponent>() : nullptr)
	{
		for (const FGameplayAbilitySpecHandle& GrantedHandle : GrantedAbilitySpecHandles)
		{
			CharacterASC->ClearAbility(GrantedHandle);
		}
	}
	GrantedAbilitySpecHandles.Reset();

	PreparedSlotActions.Reset();
	PreparedSlotHandles.Reset();

	if (MainHandBladeBaseMarker)
	{
		MainHandBladeBaseMarker->DestroyComponent();
		MainHandBladeBaseMarker = nullptr;
	}

	if (MainHandBladeTipMarker)
	{
		MainHandBladeTipMarker->DestroyComponent();
		MainHandBladeTipMarker = nullptr;
	}

	if (MainHandDisplayComponent)
	{
		MainHandDisplayComponent->DestroyComponent();
		MainHandDisplayComponent = nullptr;
	}

	if (OffHandDisplayComponent)
	{
		OffHandDisplayComponent->DestroyComponent();
		OffHandDisplayComponent = nullptr;
	}

	CurrentMainHandWeapon = nullptr;
	CurrentOffHandWeapon = nullptr;
}

bool UWeaponEquipmentComponent::ApplyComposition(APlayerCharacter* PlayerCharacter, UAbilitySystemComponent* CharacterASC, UWeaponDefinition* MainHandDefinition, UWeaponDefinition* OffHandDefinition, const TArray<UCombatActionDefinition*>& PreparedActions)
{
	USkeletalMeshComponent* OwnerMesh = PlayerCharacter->GetMesh();

	TArray<TObjectPtr<UCombatActionDefinition>> NewPreparedActions;
	NewPreparedActions.Init(nullptr, PreparedSlotCount);
	TArray<FGameplayAbilitySpecHandle> NewPreparedHandles;
	NewPreparedHandles.SetNum(PreparedSlotCount);
	for (int32 SlotIndex = 0; SlotIndex < PreparedSlotCount && SlotIndex < PreparedActions.Num(); ++SlotIndex)
	{
		NewPreparedActions[SlotIndex] = PreparedActions[SlotIndex];
	}

	TArray<FGameplayAbilitySpecHandle> NewGrantedHandles;

	auto SpawnDisplay = [PlayerCharacter, OwnerMesh](UWeaponDefinition* Definition) -> UStaticMeshComponent*
	{
		if (!Definition || !Definition->WeaponMesh)
		{
			return nullptr;
		}

		UStaticMeshComponent* DisplayComponent = NewObject<UStaticMeshComponent>(PlayerCharacter, NAME_None, RF_Transient);
		DisplayComponent->SetStaticMesh(Definition->WeaponMesh);
		DisplayComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		DisplayComponent->SetGenerateOverlapEvents(false);
		DisplayComponent->RegisterComponent();
		DisplayComponent->AttachToComponent(OwnerMesh, FAttachmentTransformRules::KeepRelativeTransform, Definition->AttachSocketName);
		DisplayComponent->SetRelativeLocation(Definition->DisplayLocationOffset);
		DisplayComponent->SetRelativeRotation(Definition->DisplayRotationOffset);
		return DisplayComponent;
	};

	auto GrantClass = [CharacterASC, PlayerCharacter, &NewGrantedHandles](const TSubclassOf<UGameplayAbility>& AbilityClass) -> bool
	{
		const FGameplayAbilitySpecHandle GrantedHandle = CharacterASC->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, PlayerCharacter));
		if (!GrantedHandle.IsValid())
		{
			return false;
		}

		NewGrantedHandles.Add(GrantedHandle);
		return true;
	};

	UStaticMeshComponent* NewMainHandDisplay = SpawnDisplay(MainHandDefinition);
	UStaticMeshComponent* NewOffHandDisplay = SpawnDisplay(OffHandDefinition);

	USceneComponent* NewBladeBaseMarker = nullptr;
	USceneComponent* NewBladeTipMarker = nullptr;
	const UMeleeWeaponDefinition* MainHandMelee = Cast<UMeleeWeaponDefinition>(MainHandDefinition);
	if (MainHandMelee && NewMainHandDisplay)
	{
		NewBladeBaseMarker = NewObject<USceneComponent>(PlayerCharacter, NAME_None, RF_Transient);
		NewBladeBaseMarker->RegisterComponent();
		NewBladeBaseMarker->AttachToComponent(NewMainHandDisplay, FAttachmentTransformRules::KeepRelativeTransform);
		NewBladeBaseMarker->SetRelativeLocation(MainHandMelee->BladeBaseMarkerRelativeLocation);

		NewBladeTipMarker = NewObject<USceneComponent>(PlayerCharacter, NAME_None, RF_Transient);
		NewBladeTipMarker->RegisterComponent();
		NewBladeTipMarker->AttachToComponent(NewMainHandDisplay, FAttachmentTransformRules::KeepRelativeTransform);
		NewBladeTipMarker->SetRelativeLocation(MainHandMelee->BladeTipMarkerRelativeLocation);
	}

	auto BaseGrants = [&GrantClass](const UWeaponDefinition* SlotDefinition) -> bool
	{
		const UMeleeWeaponDefinition* MeleeDefinition = Cast<UMeleeWeaponDefinition>(SlotDefinition);
		if (!MeleeDefinition)
		{
			return true;
		}

		for (const TSubclassOf<UGameplayAbility>& AbilityClass : MeleeDefinition->GrantedWeaponAbilities)
		{
			if (!GrantClass(AbilityClass))
			{
				return false;
			}
		}

		return true;
	};

	bool bApplySucceeded = BaseGrants(MainHandDefinition) && BaseGrants(OffHandDefinition);
	if (bApplySucceeded)
	{
		for (int32 SlotIndex = 0; SlotIndex < PreparedSlotCount; ++SlotIndex)
		{
			UCombatActionDefinition* SlotAction = NewPreparedActions[SlotIndex];
			if (!SlotAction)
			{
				continue;
			}

			if (!GrantClass(SlotAction->AbilityClass))
			{
				bApplySucceeded = false;
				break;
			}

			NewPreparedHandles[SlotIndex] = NewGrantedHandles.Last();
		}
	}

	if (bApplySucceeded)
	{
		CurrentMainHandWeapon = MainHandDefinition;
		CurrentOffHandWeapon = OffHandDefinition;
		MainHandDisplayComponent = NewMainHandDisplay;
		OffHandDisplayComponent = NewOffHandDisplay;
		MainHandBladeBaseMarker = NewBladeBaseMarker;
		MainHandBladeTipMarker = NewBladeTipMarker;
		PreparedSlotActions = MoveTemp(NewPreparedActions);
		PreparedSlotHandles = MoveTemp(NewPreparedHandles);
		GrantedAbilitySpecHandles = MoveTemp(NewGrantedHandles);

		if (MainHandDefinition && MainHandDefinition->AssociatedLoadout && !PlayerCharacter->SetActiveCombatLoadout(MainHandDefinition->AssociatedLoadout))
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("Weapon equipment on '%s' could not activate the main hand's Base Input Profile."), *GetNameSafe(GetOwner()));
			TeardownEquippedWeapons();
			return false;
		}
	}
	else
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Weapon equipment on '%s' could not grant every composition ability."), *GetNameSafe(GetOwner()));

		for (const FGameplayAbilitySpecHandle& RollbackHandle : NewGrantedHandles)
		{
			CharacterASC->ClearAbility(RollbackHandle);
		}
		if (NewBladeBaseMarker)
		{
			NewBladeBaseMarker->DestroyComponent();
		}
		if (NewBladeTipMarker)
		{
			NewBladeTipMarker->DestroyComponent();
		}
		if (NewMainHandDisplay)
		{
			NewMainHandDisplay->DestroyComponent();
		}
		if (NewOffHandDisplay)
		{
			NewOffHandDisplay->DestroyComponent();
		}
	}

	return bApplySucceeded;
}

bool UWeaponEquipmentComponent::ComputeKeepIfCompatibleLayout(UWeaponDefinition* NewMainHand, UWeaponDefinition* NewOffHand, TArray<UCombatActionDefinition*>& OutPreparedActions) const
{
	OutPreparedActions.Init(nullptr, PreparedSlotCount);

	TArray<UCombatActionDefinition*> Candidates;
	for (const UWeaponDefinition* SlotDefinition : { NewMainHand, NewOffHand })
	{
		if (!SlotDefinition)
		{
			continue;
		}

		for (const TObjectPtr<UCombatActionDefinition>& Action : SlotDefinition->ReusableCombatActions)
		{
			Candidates.AddUnique(Action.Get());
		}
		for (const TObjectPtr<UCombatActionDefinition>& Action : SlotDefinition->ExclusiveCombatActions)
		{
			Candidates.AddUnique(Action.Get());
		}
	}

	// Keep an old slot entry only when its action is still a candidate with a usable ability class.
	for (int32 SlotIndex = 0; SlotIndex < PreparedSlotCount && SlotIndex < PreparedSlotActions.Num(); ++SlotIndex)
	{
		UCombatActionDefinition* OldAction = PreparedSlotActions[SlotIndex].Get();
		if (OldAction && OldAction->AbilityClass && Candidates.Contains(OldAction))
		{
			OutPreparedActions[SlotIndex] = OldAction;
		}
	}

	// Fill remaining empty slots from the composed defaults, MainHand first.
	for (const UWeaponDefinition* SlotDefinition : { NewMainHand, NewOffHand })
	{
		if (!SlotDefinition)
		{
			continue;
		}

		for (const TObjectPtr<UCombatActionDefinition>& DefaultAction : SlotDefinition->DefaultPreparedActions)
		{
			if (!DefaultAction || !DefaultAction->AbilityClass || OutPreparedActions.Contains(DefaultAction.Get()))
			{
				continue;
			}

			for (int32 SlotIndex = 0; SlotIndex < PreparedSlotCount; ++SlotIndex)
			{
				if (!OutPreparedActions[SlotIndex])
				{
					OutPreparedActions[SlotIndex] = DefaultAction.Get();
					break;
				}
			}
		}
	}

	return true;
}

bool UWeaponEquipmentComponent::ResolveDefenseAbilityTag(bool bGuardIntent, FGameplayTag& OutAbilityTag) const
{
	const FGameplayTag& DefaultTag = bGuardIntent ? DefaultGuardAbilityTag : DefaultParryAbilityTag;

	if (CurrentOffHandWeapon && CurrentOffHandWeapon->DefenseProfile)
	{
		OutAbilityTag = bGuardIntent ? CurrentOffHandWeapon->DefenseProfile->GuardAbilityTag : CurrentOffHandWeapon->DefenseProfile->ParryAbilityTag;
		return OutAbilityTag.IsValid();
	}

	if (CurrentMainHandWeapon && CurrentMainHandWeapon->DefenseProfile)
	{
		OutAbilityTag = bGuardIntent ? CurrentMainHandWeapon->DefenseProfile->GuardAbilityTag : CurrentMainHandWeapon->DefenseProfile->ParryAbilityTag;
		return OutAbilityTag.IsValid();
	}

	OutAbilityTag = DefaultTag;
	return OutAbilityTag.IsValid();
}
