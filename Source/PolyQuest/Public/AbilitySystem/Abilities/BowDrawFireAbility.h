#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "BowDrawFireAbility.generated.h"

class ACombatProjectile;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAnimMontage;

/**
 * GAS ability governing player bow draw, hold, and projectile release lifecycle.
 * Activated via Input.PrimaryAttack -> Ability.Attack.Primary while a bow is equipped.
 * Spawns a straight combat projectile upon receiving the semantic Release event from the active bow montage.
 */
UCLASS()
class POLYQUEST_API UBowDrawFireAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UBowDrawFireAbility();

	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

#if WITH_DEV_AUTOMATION_TESTS
	const FGameplayTagContainer& GetTestActivationOwnedTags() const { return ActivationOwnedTags; }
	uint8 GetTestBowState() const { return static_cast<uint8>(BowState); }
	bool GetTestReleaseRequested() const { return bReleaseRequested; }
	bool GetTestSpawnedProjectile() const { return bSpawnedProjectile; }
	void TestOnDrawReadyEvent(const FGameplayEventData& Payload) { OnDrawReadyEvent(Payload); }
	void TestOnReleaseAnimEvent(const FGameplayEventData& Payload) { OnReleaseAnimEvent(Payload); }
	void TestOnInputReleased(const FGameplayEventData& Payload) { OnInputReleased(Payload); }
	void TestOnInputCanceled(const FGameplayEventData& Payload) { OnInputCanceled(Payload); }
	void SetTestBowStateDrawing() { BowState = EBowState::Drawing; }
	void SetTestBowStateHolding() { BowState = EBowState::Holding; }
	void SetTestBowStateReleasing() { BowState = EBowState::Releasing; }
	void SetTestBowMontage(UAnimMontage* Montage) { BowMontage = Montage; }
	void SetTestCurrentActorInfo(const FGameplayAbilityActorInfo* InActorInfo) { CurrentActorInfo = InActorInfo; }
	void TestSpawnProjectile() { SpawnProjectile(); }
#endif

protected:
	/** Authored bow animation montage (Draw -> Hold -> Release). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Animation")
	TObjectPtr<UAnimMontage> BowMontage;

	/** Section name for the initial draw animation phase. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Animation")
	FName DrawSectionName = TEXT("Draw");

	/** Section name for the held draw animation loop. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Animation")
	FName HoldSectionName = TEXT("Hold");

	/** Section name for the release animation phase. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Animation")
	FName ReleaseSectionName = TEXT("Release");

	/** Optional projectile Actor class override; defaults to ACombatProjectile. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Projectile")
	TSubclassOf<ACombatProjectile> ProjectileClass;

private:
	enum class EBowState : uint8
	{
		Inactive,
		Drawing,
		Holding,
		Releasing
	};

	bool IsValidAvatarEventPayload(const FGameplayEventData& Payload) const;

	UFUNCTION()
	void OnMontageCompleted();

	UFUNCTION()
	void OnMontageBlendOut();

	UFUNCTION()
	void OnMontageInterrupted();

	UFUNCTION()
	void OnMontageCancelled();

	UFUNCTION()
	void OnDrawReadyEvent(FGameplayEventData Payload);

	UFUNCTION()
	void OnReleaseAnimEvent(FGameplayEventData Payload);

	UFUNCTION()
	void OnInputReleased(FGameplayEventData Payload);

	UFUNCTION()
	void OnInputCanceled(FGameplayEventData Payload);

	void TriggerRelease();
	void SpawnProjectile();

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitDrawReadyTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitReleaseTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitInputReleasedTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitInputCanceledTask;

	FGameplayTag PrimaryAttackInputTag;
	FGameplayTag DrawReadyEventTag;
	FGameplayTag ReleaseEventTag;
	FGameplayTag InputReleasedEventTag;
	FGameplayTag InputCanceledEventTag;

	EBowState BowState = EBowState::Inactive;
	bool bReleaseRequested = false;
	bool bSpawnedProjectile = false;
	bool bEndAbilityInProgress = false;
};
