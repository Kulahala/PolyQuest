#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpecHandle.h"
#include "GameplayTagContainer.h"
#include "Combat/Equipment/WeaponDefinition.h"
#include "WeaponEquipmentComponent.generated.h"

class UGameplayAbility;
class UMeleeWeaponDefinition;
class UWeaponDefinition;
class USceneComponent;
class UStaticMeshComponent;
class UAbilitySystemComponent;
class APlayerCharacter;
class AWorldWeaponPickup;

/**
 * Owns the player's equipped hand-slot weapons, their runtime-spawned display
 * and trace markers, every weapon-granted ability handle, the prepared 1-4
 * layout, and the single input-resolution path (Base Input Profile, Effective
 * Defense Profile, and exact-handle prepared activation). EquipWeapon is the
 * direct/debug route; TryEquipWorldPickup is the transactional world-pickup route.
 */
UCLASS(ClassGroup = (Combat))
class POLYQUEST_API UWeaponEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	static constexpr int32 PreparedSlotCount = 4;

	UWeaponEquipmentComponent();

	/**
	 * Resolves the active locomotion presentation mode for the currently equipped main-hand weapon
	 * to one of Default, LightSword, HeavySword, or Bow without internal caching or state mutation.
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Equipment")
	EWeaponLocomotionMode GetResolvedLocomotionMode() const;

	/**
	 * Returns true only when the committed off-hand weapon is a valid UOffHandWeaponDefinition
	 * configured with bProvidesShieldPresentation = true.
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Equipment")
	bool HasShieldEquipped() const;

	/**
	 * Atomically swaps the definition's hand slot: full composition preflight
	 * (zero handle changes on failure), snapshot, teardown, keep-if-compatible
	 * prepared rebuild, apply, and identity-based restore with freshly
	 * re-granted handles on any apply failure. Re-equipping the same slot's
	 * definition is a no-op true.
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Equipment")
	bool EquipWeapon(UWeaponDefinition* Definition);

	/**
	 * Transactional world pickup equipment: builds target composition, runs full preflight,
	 * applies new composition, stages displaced drops, and commits or rolls back atomically.
	 * Returns true on success; false on failure, rejection, or same-definition no-op.
	 */
	bool TryEquipWorldPickup(AWorldWeaponPickup* SourcePickup);

	UWeaponDefinition* GetCurrentMainHandWeapon() const { return CurrentMainHandWeapon; }
	UWeaponDefinition* GetCurrentOffHandWeapon() const { return CurrentOffHandWeapon; }

	/** The equipped main hand when it is a melee definition; null otherwise. */
	UMeleeWeaponDefinition* GetEquippedMainHandMelee() const;

	/** Returns the live blade markers of the equipped melee main hand for trace sampling. */
	bool TryGetBladeMarkers(USceneComponent*& OutBladeBase, USceneComponent*& OutBladeTip) const;

	/** Resolves a named Socket on the current main-hand display mesh to a finite world transform. */
	bool TryGetEquippedMainHandDisplaySocketTransform(FName SocketName, FTransform& OutTransform) const;

	/** The single input resolver: Guard/Parry through the Effective Defense Profile, Primary through the Base Input Profile. */
	bool TryResolveInputIntent(const FGameplayTag& InputIntentTag, FGameplayTag& OutAbilityTag) const;

	/** Resolves the Sprint Attack ability tag from the equipped main hand's Base Input Profile. */
	bool TryGetSprintAttackAbilityTag(FGameplayTag& OutAbilityTag) const;

	/** Activates a prepared slot through its exact spec handle after validating the binding is current. */
	bool TryActivatePreparedSlot(int32 SlotIndex);

	/** The explicit canonical Unarmed fallback definition used during TwoHanded->OffHand swaps. */
	UMeleeWeaponDefinition* GetUnarmedFallbackDefinition() const { return UnarmedFallbackDefinition; }

	/** Constructs the full target MainHand/OffHand composition for an incoming weapon definition. */
	bool BuildTargetCompositionForIncoming(UWeaponDefinition* IncomingDefinition, UWeaponDefinition*& OutTargetMainHand, UWeaponDefinition*& OutTargetOffHand, FString& OutReason) const;

	/** Computes the set of equipped definitions displaced by a new composition (excluding the Unarmed fallback). */
	void CalculateDisplacedDefinitions(UWeaponDefinition* OldMainHand, UWeaponDefinition* OldOffHand, UWeaponDefinition* NewMainHand, UWeaponDefinition* NewOffHand, TArray<UWeaponDefinition*>& OutDisplacedDefinitions) const;

	/** Computes the keep-if-compatible prepared ability layout for a new weapon composition. */
	bool ComputeKeepIfCompatibleLayout(UWeaponDefinition* NewMainHand, UWeaponDefinition* NewOffHand, TArray<TSubclassOf<UGameplayAbility>>& OutPreparedClasses) const;

#if WITH_DEV_AUTOMATION_TESTS
public:
	void SetInjectApplyFailureOnce(bool bInject) { bInjectApplyFailureOnce = bInject; }
	void SetInjectDropFailureOnce(bool bInject) { bInjectDropFailureOnce = bInject; }
	bool TestDirectPreflight(UWeaponDefinition* MainHand, UWeaponDefinition* OffHand, FString& OutReason);

	/** Read-only test validation helper: verifies that a prepared slot has a valid, current ASC binding. */
	bool VerifyPreparedSlotBinding(int32 SlotIndex, TSubclassOf<UGameplayAbility> ExpectedClass, FString& OutDiagnostic) const;

	/** Read-only test validation helper: verifies that a component-owned base grant is currently bound on the ASC. */
	bool VerifyGrantedAbilityBinding(TSubclassOf<UGameplayAbility> ExpectedClass, FString& OutDiagnostic) const;

private:
	bool bInjectApplyFailureOnce = false;
	bool bInjectDropFailureOnce = false;
#endif

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** The explicit Unarmed definition asset; configured on BP_Player. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Equipment")
	TObjectPtr<UMeleeWeaponDefinition> UnarmedFallbackDefinition;

private:
	bool CanSwapNow(const UAbilitySystemComponent* CharacterASC) const;
	bool RunPreflight(APlayerCharacter* PlayerCharacter, UAbilitySystemComponent* CharacterASC, UWeaponDefinition* NewMainHand, UWeaponDefinition* NewOffHand, const TArray<TSubclassOf<UGameplayAbility>>& ComputedPreparedClasses, FString& OutReason) const;
	void TeardownEquippedWeapons();
	bool ApplyComposition(APlayerCharacter* PlayerCharacter, UAbilitySystemComponent* CharacterASC, UWeaponDefinition* MainHandDefinition, UWeaponDefinition* OffHandDefinition, const TArray<TSubclassOf<UGameplayAbility>>& PreparedClasses);
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
	FGameplayTag DeadStateTag;
	FGameplayTag PrimaryAttackAbilityTag;
	FGameplayTag GuardInputTag;
	FGameplayTag ParryInputTag;
	FGameplayTag PrimaryAttackInputTag;
	FGameplayTag DefaultGuardAbilityTag;
	FGameplayTag DefaultParryAbilityTag;
	bool bSwapRefusalWarningIssued = false;
};
