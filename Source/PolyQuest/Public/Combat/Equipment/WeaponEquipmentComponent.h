#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "WeaponEquipmentComponent.generated.h"

class UMeleeWeaponDefinition;
class USceneComponent;
class UStaticMeshComponent;
class UAbilitySystemComponent;
class APlayerCharacter;

/**
 * Owns the player's single equipped melee weapon: the equipped definition,
 * its runtime-spawned display and trace markers, and the ability handles it
 * granted. EquipWeapon is the only public operation; teardown is a private
 * helper shared by a validated swap and component destruction.
 */
UCLASS(ClassGroup = (Combat))
class POLYQUEST_API UWeaponEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWeaponEquipmentComponent();

	/**
	 * Atomically swaps to the validated new weapon: full preflight, snapshot,
	 * old teardown, new application, and a gate-free snapshot restore on any
	 * post-preflight failure. Re-equipping the current weapon is a no-op true.
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Equipment")
	bool EquipWeapon(UMeleeWeaponDefinition* Definition);

	/** The currently equipped definition; null only before the first equip or after a failed first equip. */
	UMeleeWeaponDefinition* GetCurrentWeapon() const { return CurrentWeapon; }

	/** Returns the live marker components of the equipped weapon for trace sampling. */
	bool TryGetBladeMarkers(USceneComponent*& OutBladeBase, USceneComponent*& OutBladeTip) const;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool CanSwapNow(const UAbilitySystemComponent* CharacterASC);
	bool RunPreflight(APlayerCharacter* PlayerCharacter, UAbilitySystemComponent* CharacterASC, const UMeleeWeaponDefinition* Definition, FString& OutReason) const;
	void TeardownEquippedWeapon();
	bool ApplyEquippedWeapon(APlayerCharacter* PlayerCharacter, UAbilitySystemComponent* CharacterASC, UMeleeWeaponDefinition* Definition);

	UPROPERTY(Transient)
	TObjectPtr<UMeleeWeaponDefinition> CurrentWeapon;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> EquippedDisplayComponent;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> EquippedBladeBaseMarker;

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> EquippedBladeTipMarker;

	UPROPERTY(Transient)
	TArray<FGameplayAbilitySpecHandle> GrantedAbilitySpecHandles;

	FGameplayTag AttackingStateTag;
	FGameplayTag GuardingStateTag;
	FGameplayTag ParryingStateTag;
	FGameplayTag DodgingStateTag;
	FGameplayTag PrimaryAttackAbilityTag;
	bool bSwapRefusalWarningIssued = false;
};
