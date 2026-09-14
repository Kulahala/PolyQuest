#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayTagContainer.h"
#include "AbilitySystem/Abilities/MontageRateWindowLifecycle.h"
#include "PlayerLaunchReactionAbility.generated.h"

class ACharacter;
class APlayerCharacter;
class UAbilityTask_PlayActionMontage;
class UAnimInstance;
class UAnimMontage;

/**
 * Server-authoritative, single-phase grounded Root Motion knockdown and recovery hit reaction.
 * Plays authored Root Motion knockdown montage under CMC MOVE_Walking, uses standard Task RateWindow/DodgeOnly windows,
 * enforces ledge walk-off protection guard, and converges on an idempotent EndAbility cleanup path.
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
	FGameplayTag GetTestDodgeCancelableStateTag() const { return DodgeCancelableStateTag; }

	UAnimMontage* GetTestRootMotionKnockdownMontage() const { return RootMotionKnockdownMontage.Get(); }
	void SetTestRootMotionKnockdownMontage(UAnimMontage* Montage) { RootMotionKnockdownMontage = Montage; }
	void SetTestActiveMontage(UAnimMontage* Montage) { ActiveMontage = Montage; }
	void SetTestBoundAnimInstance(UAnimInstance* AnimInstance) { BoundAnimInstance = AnimInstance; }
	void SetTestCurrentPhaseToRootMotionKnockdown() { CurrentPhase = ELaunchPhase::RootMotionKnockdown; }
	void SetTestCurrentActorInfo(const FGameplayAbilityActorInfo* InActorInfo) { CurrentActorInfo = InActorInfo; }
	bool IsTestPhaseRootMotionKnockdown() const;
	bool IsTestPhaseNone() const;
	uint8 GetTestCurrentPhaseRaw() const;
	bool GetTestLedgeSettingModified() const { return bLedgeSettingModified; }
	void SetTestLedgeSettingModified(bool bModified) { bLedgeSettingModified = bModified; }
	bool GetTestSavedCanWalkOffLedges() const { return bSavedCanWalkOffLedges; }
	void SetTestSavedCanWalkOffLedges(bool bValue) { bSavedCanWalkOffLedges = bValue; }
	UAnimInstance* GetTestBoundAnimInstance() const { return BoundAnimInstance.Get(); }
	bool CallTestIsRootMotionKnockdownCandidate(const APlayerCharacter* InPlayer, const class UCharacterMovementComponent* InMovement) const
	{
		return IsRootMotionKnockdownCandidate(InPlayer, InMovement);
	}
	bool CallTestValidateActivationSetup(const FGameplayAbilityActorInfo* ActorInfo) const { return ValidateActivationSetup(ActorInfo); }
	void TriggerTestMovementModeChanged(ACharacter* Character, EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
	{
		OnMovementModeChanged(Character, PrevMovementMode, PreviousCustomMode);
	}
	void TriggerTestActiveMontageEnded(UAnimMontage* Montage, bool bInterrupted)
	{
		OnActiveMontageEnded(Montage, bInterrupted);
	}
	static bool CallTestTryResolveRootMotionFacingYaw(const FVector& LocalAttackerDirection, float ImpactReferenceYaw, float& OutFacingYaw)
	{
		return TryResolveRootMotionFacingYaw(LocalAttackerDirection, ImpactReferenceYaw, OutFacingYaw);
	}
#endif

private:
	enum class ELaunchPhase : uint8
	{
		None,
		RootMotionKnockdown
	};

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Player|Reaction", meta = (AllowPrivateAccess = "true", ToolTip = "玩家击飞受击地面 Root Motion 击倒动画 Montage 资产。"))
	TObjectPtr<UAnimMontage> RootMotionKnockdownMontage;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayActionMontage> MontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAnimInstance> BoundAnimInstance;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveMontage;

	UPROPERTY(Transient)
	TWeakObjectPtr<APlayerCharacter> BoundPlayerCharacter;

	FGameplayTag PlayerLaunchReactionAbilityTag;
	FGameplayTag PlayerLaunchReactionEventTag;
	FGameplayTag DodgeCancelableStateTag;
	FGameplayTag HitReactingStateTag;
	FGameplayTag StunnedStateTag;
	FGameplayTag DeadStateTag;
	FGameplayTag HyperArmorStateTag;
	FGameplayTag TeardownOnUnpossessTag;
	FGameplayTagContainer AbilitiesToCancel;

	FVector ImpactDirectionSnapshot = FVector::ZeroVector;
	float ImpactReferenceYawSnapshot = 0.0f;
	ELaunchPhase CurrentPhase = ELaunchPhase::None;
	bool bSavedCanWalkOffLedges = true;
	bool bLedgeSettingModified = false;
	bool bMovementModeDelegateBound = false;
	int32 ActiveMontageInstanceID = INDEX_NONE;
	bool bEndAbilityRequested = false;

	UFUNCTION()
	void OnMontageFailed();

	UFUNCTION()
	void OnActiveMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	void OnMovementModeChanged(ACharacter* Character, EMovementMode PrevMovementMode, uint8 PreviousCustomMode);

	bool ValidateActivationSetup(const FGameplayAbilityActorInfo* ActorInfo) const;
	bool IsRootMotionKnockdownCandidate(const APlayerCharacter* PlayerCharacter, const class UCharacterMovementComponent* MovementComponent) const;
	static bool TryResolveRootMotionFacingYaw(const FVector& LocalAttackerDirection, float ImpactReferenceYaw, float& OutFacingYaw);
	void EndFromMontage(bool bWasCancelled);
#if WITH_DEV_AUTOMATION_TESTS
public:
	UAbilityTask_PlayActionMontage* GetTestMontageTask() const { return MontageTask.Get(); }
	void SetTestMontageTask(UAbilityTask_PlayActionMontage* Task) { MontageTask = Task; }
	const FAbilityMontageRateWindowLifecycle& GetTestRateWindowLifecycle() const;
	FAbilityMontageRateWindowLifecycle& GetTestRateWindowLifecycle_Mutable();
	int32 GetTestRateWindowMontageInstanceID() const;
	bool HasTestRateWindowTasks() const;
	bool GetTestDodgeCancelable() const;
#endif
};
