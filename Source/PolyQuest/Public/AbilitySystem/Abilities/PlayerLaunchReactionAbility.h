#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayTagContainer.h"
#include "PlayerLaunchReactionAbility.generated.h"

class ACharacter;
class APlayerCharacter;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitDelay;
class UAbilityTask_WaitGameplayEvent;
class UAnimInstance;
class UAnimMontage;

/**
 * Server-authoritative, multi-phase player launch hit reaction.
 * Lifecycle: Takeoff -> AwaitingAirborne -> Airborne -> LandingRecovery.
 * Takeoff Montage provides full-body takeoff and airborne pose presentation.
 * On Event.Reaction.Launch.Commit, the Takeoff Montage is paused to hold the airborne flight silhouette,
 * while CharacterMovement exclusively drives all capsule displacement and physics falling.
 * Upon landing on ground, the paused Takeoff Montage is stopped and LandingRecovery Montage plays.
 */
UCLASS()
class POLYQUEST_API UPlayerLaunchReactionAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UPlayerLaunchReactionAbility();

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
	const FGameplayTagContainer& GetTestActivationBlockedTags() const { return ActivationBlockedTags; }
	const FGameplayTagContainer& GetTestBlockAbilitiesWithTag() const { return BlockAbilitiesWithTag; }
	const FGameplayTagContainer& GetTestAbilitiesToCancel() const { return AbilitiesToCancel; }
	const TArray<FAbilityTriggerData>& GetTestAbilityTriggers() const { return AbilityTriggers; }
	const FVector& GetImpactDirectionSnapshot() const { return ImpactDirectionSnapshot; }
	float GetLaunchHorizontalSpeed() const { return LaunchHorizontalSpeed; }
	float GetLaunchVerticalSpeed() const { return LaunchVerticalSpeed; }
	FGameplayTag GetTestDodgeCancelableStateTag() const { return DodgeCancelableStateTag; }
	bool GetTestDodgeCancelable() const { return bDodgeCancelable; }
	void SetTestLandingRecoveryMontage(UAnimMontage* Montage) { LandingRecoveryMontage = Montage; }
	void SetTestActiveMontage(UAnimMontage* Montage) { ActiveMontage = Montage; }
	void SetTestBoundAnimInstance(UAnimInstance* AnimInstance) { BoundAnimInstance = AnimInstance; }
	void SetTestCurrentPhaseToLandingRecovery() { CurrentPhase = ELaunchPhase::LandingRecovery; }
	void SetTestCurrentActorInfo(const FGameplayAbilityActorInfo* InActorInfo) { CurrentActorInfo = InActorInfo; }
	void SetTestBypassAnimInstanceActiveCheck(bool bBypass) { bTestBypassAnimInstanceActiveCheck = bBypass; }
	void TestOnCancelWindowBegin(const FGameplayEventData& Payload) { OnCancelWindowBegin(Payload); }
	void TestOnCancelWindowEnd(const FGameplayEventData& Payload) { OnCancelWindowEnd(Payload); }
	void TestSetDodgeCancelable(bool bShouldCancel) { SetDodgeCancelable(bShouldCancel); }
	bool Test_IsEventFromLandingRecoveryMontage(const FGameplayEventData& Payload) const { return IsEventFromLandingRecoveryMontage(Payload); }
#endif

private:
	enum class ELaunchPhase : uint8
	{
		None,
		Takeoff,
		AwaitingAirborne,
		Airborne,
		LandingRecovery
	};

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Player|Reaction", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> TakeoffMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Player|Reaction", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> LandingRecoveryMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Player|Reaction", meta = (AllowPrivateAccess = "true"))
	float LaunchHorizontalSpeed = 450.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Player|Reaction", meta = (AllowPrivateAccess = "true"))
	float LaunchVerticalSpeed = 550.0f;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> CommitEventTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitDelay> FallValidationTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> CancelBeginTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> CancelEndTask;

	UPROPERTY(Transient)
	TObjectPtr<UAnimInstance> BoundAnimInstance;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveMontage;

	UPROPERTY(Transient)
	TWeakObjectPtr<APlayerCharacter> BoundPlayerCharacter;

	FGameplayTag PlayerLaunchReactionAbilityTag;
	FGameplayTag PlayerLaunchReactionEventTag;
	FGameplayTag LaunchCommitEventTag;
	FGameplayTag CancelWindowBeginEventTag;
	FGameplayTag CancelWindowEndEventTag;
	FGameplayTag DodgeCancelableStateTag;
	FGameplayTag HitReactingStateTag;
	FGameplayTag StunnedStateTag;
	FGameplayTag DeadStateTag;
	FGameplayTag HyperArmorStateTag;
	FGameplayTagContainer AbilitiesToCancel;

	FVector ImpactDirectionSnapshot = FVector::ZeroVector;
	ELaunchPhase CurrentPhase = ELaunchPhase::None;
	bool bCommitHandled = false;
	bool bDodgeCancelable = false;
	bool bSavedCanWalkOffLedges = true;
	bool bLedgeSettingModified = false;
	bool bMovementModeDelegateBound = false;
	bool bEndAbilityRequested = false;
#if WITH_DEV_AUTOMATION_TESTS
	bool bTestBypassAnimInstanceActiveCheck = false;
#endif

	UFUNCTION()
	void OnLaunchCommitEventReceived(FGameplayEventData Payload);

	UFUNCTION()
	void OnFallValidationFinished();

	UFUNCTION()
	void OnActiveMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	void OnMovementModeChanged(ACharacter* Character, EMovementMode PrevMovementMode, uint8 PreviousCustomMode);

	UFUNCTION()
	void OnCancelWindowBegin(FGameplayEventData Payload);

	UFUNCTION()
	void OnCancelWindowEnd(FGameplayEventData Payload);

	bool ValidateActivationSetup(const FGameplayAbilityActorInfo* ActorInfo) const;
	bool IsEventFromTakeoffMontage(const FGameplayEventData& Payload) const;
	bool IsEventFromLandingRecoveryMontage(const FGameplayEventData& Payload) const;
	void SetDodgeCancelable(bool bShouldCancel);
	void EndFromMontage(bool bWasCancelled);
};
