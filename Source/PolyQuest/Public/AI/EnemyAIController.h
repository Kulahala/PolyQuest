#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GameplayTagContainer.h"
#include "Perception/AIPerceptionTypes.h"
#include "EnemyAIController.generated.h"

class APlayerCharacter;
class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UStateTreeAIComponent;

/**
 * Owns the first enemy's Sight target, focus, home location, and StateTree
 * lifecycle. StateTree tasks query this controller rather than storing a
 * competing AI state or target variable.
 */
UCLASS(Blueprintable)
class POLYQUEST_API AEnemyAIController : public AAIController
{
	GENERATED_BODY()

public:
	AEnemyAIController();

	UFUNCTION(BlueprintPure, Category = "AI|Target")
	APlayerCharacter* GetCurrentTarget() const;

	UFUNCTION(BlueprintPure, Category = "AI|Home")
	FVector GetHomeLocation() const { return HomeLocation; }

	UFUNCTION(BlueprintPure, Category = "AI|Combat")
	float GetMeleeRange() const { return MeleeRange; }

	/** True only after OnPossess has accepted and cached the possessed enemy's authored Profile. */
	bool HasValidAttackProfile() const;

	/** The active Ability starts this only after its Montage was confirmed to have started and then cleaned up. */
	void StartMeleeAttackCooldown(float CooldownAfterAttack);

	UFUNCTION(BlueprintPure, Category = "AI|Combat")
	bool IsMeleeAttackOnCooldown() const;

	UFUNCTION(BlueprintPure, Category = "AI|Home")
	float GetHomeAcceptanceRadius() const { return HomeAcceptanceRadius; }

	/** True only while this controller owns a valid Player target and pawn. */
	UFUNCTION(BlueprintPure, Category = "AI|Target")
	bool HasValidCombatTarget() const;

	/** Uses the same range that the authored Chase MoveTo task should bind as its acceptance radius. */
	UFUNCTION(BlueprintPure, Category = "AI|Combat")
	bool IsCombatTargetInMeleeRange() const;

	/** Instant StateTree Alert action: stop stale movement while retaining the current target focus. */
	void BeginAlert();

	/** Requests the single configured enemy melee ability only when the controller target remains in range. */
	bool TryRequestMeleeAttack();

	/** Reads the authoritative GAS action tag; it never infers attack state from montage playback. */
	bool IsEnemyMeleeAttackActive() const;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

private:
	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	void ConfigureSight();
	void SetCurrentTarget(APlayerCharacter* NewTarget);
	void ClearCurrentTarget(bool bSendTargetLostEvent);
	void SendStateTreeEvent(const FGameplayTag& EventTag) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAIPerceptionComponent> EnemyPerceptionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStateTreeAIComponent> StateTreeComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Sight", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "Centimeters"))
	float SightRadius = 1500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Sight", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "Centimeters"))
	float LoseSightRadius = 1800.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Sight", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "180.0", Units = "Degrees"))
	float PeripheralVisionHalfAngleDegrees = 70.0f;

	/** Profile-owned runtime reach. StateTree binds through GetMeleeRange rather than treating this as CDO tuning. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "AI|Combat", meta = (AllowPrivateAccess = "true", Units = "Centimeters"))
	float MeleeRange = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Home", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "Centimeters"))
	float HomeAcceptanceRadius = 50.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "AI|Target", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<APlayerCharacter> CurrentTarget;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "AI|Home", meta = (AllowPrivateAccess = "true"))
	FVector HomeLocation = FVector::ZeroVector;

	FGameplayTag EnemyMeleeAbilityTag;
	FGameplayTag AttackingStateTag;
	FGameplayTag TargetAcquiredEventTag;
	FGameplayTag TargetLostEventTag;
	float MeleeAttackCooldownEndTime = 0.0f;
	bool bHasValidAttackProfile = false;
	bool bHasLoggedInvalidAttackProfile = false;
};
