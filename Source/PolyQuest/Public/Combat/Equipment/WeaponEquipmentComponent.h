#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "WeaponEquipmentComponent.generated.h"

class UGameplayAbility;
class UMeleeWeaponDefinition;
class UWeaponDefinition;
class USceneComponent;
class UStaticMeshComponent;
class UAbilitySystemComponent;
class APlayerCharacter;

/**
 * Owns the player's equipped hand-slot weapons, their runtime-spawned display
 * and trace markers, every weapon-granted ability handle, the prepared 1-4
 * layout, and the single input-resolution path (Base Input Profile, Effective
 * Defense Profile, and exact-handle prepared activation). EquipWeapon is the
 * only public mutation; teardown is a private helper shared by a validated
 * swap and component destruction.
 */
UCLASS(ClassGroup = (Combat))
class POLYQUEST_API UWeaponEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	static constexpr int32 PreparedSlotCount = 4;

	UWeaponEquipmentComponent();

	/**
	 * Atomically swaps the definition's hand slot: full composition preflight
	 * (zero handle changes on failure), snapshot, teardown, keep-if-compatible
	 * prepared rebuild, apply, and identity-based restore with freshly
	 * re-granted handles on any apply failure. Re-equipping the same slot's
	 * definition is a no-op true.
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Equipment")
	bool EquipWeapon(UWeaponDefinition* Definition);

	UWeaponDefinition* GetCurrentMainHandWeapon() const { return CurrentMainHandWeapon; }
	UWeaponDefinition* GetCurrentOffHandWeapon() const { return CurrentOffHandWeapon; }

	/** The equipped main hand when it is a melee definition; null otherwise. */
	UMeleeWeaponDefinition* GetEquippedMainHandMelee() const;

	/** Returns the live blade markers of the equipped melee main hand for trace sampling. */
	bool TryGetBladeMarkers(USceneComponent*& OutBladeBase, USceneComponent*& OutBladeTip) const;

	/** The single input resolver: Guard/Parry through the Effective Defense Profile, Primary through the Base Input Profile. */
	bool TryResolveInputIntent(const FGameplayTag& InputIntentTag, FGameplayTag& OutAbilityTag) const;

	/** Resolves the Sprint Attack ability tag from the equipped main hand's Base Input Profile. */
	bool TryGetSprintAttackAbilityTag(FGameplayTag& OutAbilityTag) const;

	/** Activates a prepared slot through its exact spec handle after validating the binding is current. */
	bool TryActivatePreparedSlot(int32 SlotIndex);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool CanSwapNow(const UAbilitySystemComponent* CharacterASC) const;
	bool RunPreflight(APlayerCharacter* PlayerCharacter, UAbilitySystemComponent* CharacterASC, UWeaponDefinition* Definition, bool bTargetIsMainHand, const TArray<TSubclassOf<UGameplayAbility>>& ComputedPreparedClasses, FString& OutReason) const;
	void TeardownEquippedWeapons();
	bool ApplyComposition(APlayerCharacter* PlayerCharacter, UAbilitySystemComponent* CharacterASC, UWeaponDefinition* MainHandDefinition, UWeaponDefinition* OffHandDefinition, const TArray<TSubclassOf<UGameplayAbility>>& PreparedClasses);
	bool ComputeKeepIfCompatibleLayout(UWeaponDefinition* NewMainHand, UWeaponDefinition* NewOffHand, TArray<TSubclassOf<UGameplayAbility>>& OutPreparedClasses) const;
	bool ResolveDefenseAbilityTag(bool bGuardIntent, FGameplayTag& OutAbilityTag) const;

	UPROPERTY(Transient)
	TObjectPtr<UWeaponDefinition> CurrentMainHandWeapon;

	UPROPERTY(Transient)
	TObjectPtr<UWeaponDefinition> CurrentOffHandWeapon;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> MainHandDisplayComponent;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> OffHandDisplayComponent;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> MainHandBladeBaseMarker;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> MainHandBladeTipMarker;

	/** The prepared 1-4 ability-class identities; index-aligned with PreparedSlotHandles. */
	UPROPERTY(Transient)
	TArray<TSubclassOf<UGameplayAbility>> PreparedSlotClasses;

	/** The exact granted spec binding of each prepared slot; index-aligned with PreparedSlotClasses. */
	TArray<FGameplayAbilitySpecHandle> PreparedSlotHandles;

	UPROPERTY(Transient)
	TArray<FGameplayAbilitySpecHandle> GrantedAbilitySpecHandles;

	FGameplayTag AttackingStateTag;
	FGameplayTag GuardingStateTag;
	FGameplayTag ParryingStateTag;
	FGameplayTag DodgingStateTag;
	FGameplayTag PrimaryAttackAbilityTag;
	FGameplayTag GuardInputTag;
	FGameplayTag ParryInputTag;
	FGameplayTag PrimaryAttackInputTag;
	FGameplayTag DefaultGuardAbilityTag;
	FGameplayTag DefaultParryAbilityTag;
	bool bSwapRefusalWarningIssued = false;
};
