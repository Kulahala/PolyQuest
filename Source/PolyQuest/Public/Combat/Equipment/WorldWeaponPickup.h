#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WorldWeaponPickup.generated.h"

class UWeaponDefinition;
class USphereComponent;
class UStaticMeshComponent;
class APlayerCharacter;
class UWeaponEquipmentComponent;

/**
 * A world representation of an unequipped weapon definition.
 * Interacted with via player overlap and interaction input (E);
 * materializes displaced weapons upon successful equipment swap.
 * Owns no combat stats, damage calculations, or ability grants.
 */
UCLASS(BlueprintType)
class POLYQUEST_API AWorldWeaponPickup : public AActor
{
	GENERATED_BODY()

	friend class UWeaponEquipmentComponent;
	friend class APlayerCharacter;

public:
	AWorldWeaponPickup();

	virtual void OnConstruction(const FTransform& Transform) override;

	/** Returns true when the requester is valid, alive, not the former owner within the cooldown, and interaction is not in progress. */
	UFUNCTION(BlueprintPure, Category = "Equipment|Pickup")
	bool CanInteract(const APlayerCharacter* Requester) const;

	/** Returns the immutable weapon definition represented by this pickup. */
	UFUNCTION(BlueprintPure, Category = "Equipment|Pickup")
	UWeaponDefinition* GetWeaponDefinition() const { return WeaponDefinition; }

	/** Sets the weapon definition and updates the visual mesh representation. */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Pickup")
	void SetWeaponDefinition(UWeaponDefinition* InDefinition);

	/** Initializes a newly materialized dropped pickup before FinishSpawning. */
	void InitializeDroppedPickup(UWeaponDefinition* InDefinition, APlayerCharacter* InFormerOwner, float InRejectDurationSeconds);

	/** Marks interaction in progress to prevent reentrant swap requests. */
	void BeginInteraction();

	/** Clears interaction-in-progress guard on swap failure or cancellation. */
	void EndInteraction();

	/** Enables or disables the interaction sphere collision for transactional staging. */
	void SetInteractionEnabled(bool bEnabled);

	/** Projects a point down onto ground geometry using ECC_Visibility (down 300cm, +2cm normal offset). */
	static bool ProjectLocationToGround(UWorld* World, const FVector& SourceLocation, FVector& OutGroundLocation, const AActor* IgnoreActor = nullptr);

#if WITH_DEV_AUTOMATION_TESTS
	int32 GetTestOverlappingPlayerCount() const { return OverlappingPlayers.Num(); }
	float GetTestFormerOwnerRemainingTime(const APlayerCharacter* Requester) const { return GetFormerOwnerRemainingTime(Requester); }
	void TriggerTestNotifyOverlappingPlayersStateChanged() { NotifyOverlappingPlayersStateChanged(); }
	void AddTestOverlappingPlayer(APlayerCharacter* InPlayer);
	void RemoveTestOverlappingPlayer(APlayerCharacter* InPlayer);
#endif

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Stages deferred drop pickups for displaced definitions near the player's feet. */
	bool StageDisplacedDrops(APlayerCharacter* PlayerCharacter, const TArray<UWeaponDefinition*>& DisplacedDefinitions, TArray<AWorldWeaponPickup*>& OutProvisionalDrops);

	/** Optional presentation hook invoked before destroying a consumed pickup. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Equipment|Pickup", meta = (DisplayName = "OnPickupConsumed"))
	void OnPickupConsumed();

	UFUNCTION()
	void HandleInteractionSphereBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleInteractionSphereEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equipment|Pickup", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> InteractionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equipment|Pickup", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> PickupMeshComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment|Pickup")
	TObjectPtr<UWeaponDefinition> WeaponDefinition;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment|Pickup", meta = (ClampMin = "0.0"))
	float InteractionRadius = 120.0f;

	/** Duration (seconds) during which the former owner cannot immediately re-pick this dropped item. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment|Pickup", meta = (ClampMin = "0.0"))
	float FormerOwnerRejectDuration = 0.5f;

private:
	void UpdateVisualMesh();
	void NotifyOverlappingPlayersStateChanged();
	float GetFormerOwnerRemainingTime(const APlayerCharacter* Requester) const;

	TSet<TWeakObjectPtr<APlayerCharacter>> OverlappingPlayers;
	TWeakObjectPtr<APlayerCharacter> FormerOwner;
	float RejectUntilTime = 0.0f;
	bool bInteractionInProgress = false;
};
