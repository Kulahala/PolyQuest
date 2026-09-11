#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayTagContainer.h"
#include "EnemyLaunchReactionAbility.generated.h"

class ACharacter;
class AEnemyCharacter;
class UAbilityTask_PlayMontageAndWait;
class UAnimInstance;
class UAnimMontage;

/**
 * Server-authoritative grounded root motion knockdown enemy launch hit reaction.
 * Lifecycle: None -> RootMotionKnockdown -> EndAbility.
 * Uses authoritatively driven root motion knockdown montage on MOVE_Walking ground.
 */
UCLASS()
class POLYQUEST_API UEnemyLaunchReactionAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UEnemyLaunchReactionAbility();

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
	const FGameplayTagContainer& GetTestAbilitiesToCancel() const { return AbilitiesToCancel; }
	const TArray<FAbilityTriggerData>& GetTestAbilityTriggers() const { return AbilityTriggers; }
	const FVector& GetImpactDirectionSnapshot() const { return ImpactDirectionSnapshot; }
	bool GetTestRootMotionKnockdownCompletedNaturally() const { return bRootMotionKnockdownCompletedNaturally; }
	void SetTestRootMotionKnockdownCompletedNaturally(bool bValue) { bRootMotionKnockdownCompletedNaturally = bValue; }

	UAnimMontage* GetTestRootMotionKnockdownMontage() const { return RootMotionKnockdownMontage.Get(); }
	void SetTestRootMotionKnockdownMontage(UAnimMontage* Montage) { RootMotionKnockdownMontage = Montage; }
	bool IsTestPhaseRootMotionKnockdown() const;
	bool IsTestPhaseNone() const;
	uint8 GetTestCurrentPhaseRaw() const;
	bool GetTestLedgeSettingModified() const { return bLedgeSettingModified; }
	void SetTestLedgeSettingModified(bool bModified) { bLedgeSettingModified = bModified; }
	bool GetTestSavedCanWalkOffLedges() const { return bSavedCanWalkOffLedges; }
	void SetTestSavedCanWalkOffLedges(bool bValue) { bSavedCanWalkOffLedges = bValue; }
	void SetTestBypassMontageActiveCheck(bool bBypass) { bTestBypassMontageActiveCheck = bBypass; }
	bool GetTestBypassMontageActiveCheck() const { return bTestBypassMontageActiveCheck; }
	UAnimInstance* GetTestBoundAnimInstance() const { return BoundAnimInstance.Get(); }
	void SetTestBoundAnimInstance(UAnimInstance* AnimInst) { BoundAnimInstance = AnimInst; }
	bool CallTestIsRootMotionKnockdownCandidate(const AEnemyCharacter* InEnemy, const class UCharacterMovementComponent* InMovement) const
	{
		return IsRootMotionKnockdownCandidate(InEnemy, InMovement);
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Enemy|Reaction", meta = (AllowPrivateAccess = "true", ToolTip = "敌人击飞受击地面 Root Motion 击倒动画 Montage 资产。"))
	TObjectPtr<UAnimMontage> RootMotionKnockdownMontage;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAnimInstance> BoundAnimInstance;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveMontage;

	UPROPERTY(Transient)
	TWeakObjectPtr<AEnemyCharacter> BoundEnemyCharacter;

	FGameplayTag EnemyLaunchReactionAbilityTag;
	FGameplayTag EnemyLaunchReactionEventTag;
	FGameplayTag HitReactingStateTag;
	FGameplayTag StunnedStateTag;
	FGameplayTag DeadStateTag;
	FGameplayTag HyperArmorStateTag;
	FGameplayTag EnemyMeleeAbilityTag;
	FGameplayTag EnemySmallHitReactionAbilityTag;
	FGameplayTag TeardownOnUnpossessTag;
	FGameplayTag FacingBlockedStateTag;
	FGameplayTagContainer AbilitiesToCancel;

	FVector ImpactDirectionSnapshot = FVector::ZeroVector;
	float ImpactReferenceYawSnapshot = 0.0f;
	ELaunchPhase CurrentPhase = ELaunchPhase::None;
	bool bSavedCanWalkOffLedges = true;
	bool bLedgeSettingModified = false;
	bool bMovementModeDelegateBound = false;
	bool bEndAbilityRequested = false;
	bool bRootMotionKnockdownCompletedNaturally = false;

	UFUNCTION()
	void OnActiveMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	void OnMovementModeChanged(ACharacter* Character, EMovementMode PrevMovementMode, uint8 PreviousCustomMode);

	bool ValidateActivationSetup(const FGameplayAbilityActorInfo* ActorInfo) const;
	bool IsRootMotionKnockdownCandidate(const AEnemyCharacter* EnemyCharacter, const class UCharacterMovementComponent* MovementComponent) const;
	static bool TryResolveRootMotionFacingYaw(const FVector& LocalAttackerDirection, float ImpactReferenceYaw, float& OutFacingYaw);
	void EndFromMontage(bool bWasCancelled);

#if WITH_DEV_AUTOMATION_TESTS
	bool bTestBypassMontageActiveCheck = false;
#endif
};
