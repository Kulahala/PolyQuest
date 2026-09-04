#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "UObject/WeakObjectPtr.h"
#include "PlayerFrontExecutionAbility.generated.h"

class AEnemyCharacter;
class APlayerCharacter;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAnimMontage;
class UExecutionLockContext;
class UGameplayEffect;
class UMeleeWeaponDefinition;
class UPlayerFrontExecutionAbility;

/**
 * Per-activation callback context ensuring asynchronous delegate isolation across activation generations.
 */
UCLASS(Transient)
class POLYQUEST_API UPlayerFrontExecutionContext : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(Transient)
	TWeakObjectPtr<UPlayerFrontExecutionAbility> OwningAbility;

	uint32 Token = 0;

	UFUNCTION()
	void OnMontageCompleted();

	UFUNCTION()
	void OnMontageBlendOut();

	UFUNCTION()
	void OnMontageInterrupted();

	UFUNCTION()
	void OnMontageCancelled();

	UFUNCTION()
	void OnHitEventReceived(FGameplayEventData Payload);

	UFUNCTION()
	void OnReleaseRequestEventReceived(FGameplayEventData Payload);

	UFUNCTION()
	void OnVictimStartEventReceived(FGameplayEventData Payload);

	UFUNCTION()
	void OnTargetDestroyed(AActor* DestroyedActor);
};

/**
 * Server-authoritative front execution ability for the player.
 * Triggered on PrimaryAttack input when facing a stunned enemy (real stance break).
 * Reuses FMeleeHitResolver and standard Damage GameplayEffect delivery under paired execution lock.
 */
UCLASS()
class POLYQUEST_API UPlayerFrontExecutionAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UPlayerFrontExecutionAbility();

	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

#if WITH_DEV_AUTOMATION_TESTS
	const FGameplayTagContainer& GetTestActivationOwnedTags() const { return ActivationOwnedTags; }
	const FGameplayTagContainer& GetTestActivationBlockedTags() const { return ActivationBlockedTags; }
	const FGameplayTagContainer& GetTestAbilityTags() const { return AbilityTags; }
	void SetTestExecutionMontage(UAnimMontage* Montage);
	UAnimMontage* GetTestActiveExecutionMontage() const { return ActiveExecutionMontage; }
	const UMeleeWeaponDefinition* GetTestActiveExecutionWeaponDefinition() const { return ActiveExecutionWeaponDefinition; }
	float GetTestActiveMinExecutionDistance() const { return ActiveMinExecutionDistance; }
	float GetTestActiveMaxExecutionDistance() const { return ActiveMaxExecutionDistance; }
	float GetTestActiveExecutionSnapDistance() const { return ActiveExecutionSnapDistance; }
	bool HasTestActiveExecutionDistanceSnapshot() const { return bHasActiveExecutionDistanceSnapshot; }
	void SetTestDamageGameplayEffectClass(TSubclassOf<UGameplayEffect> InClass) { DamageGameplayEffectClass = InClass; }
	void SetTestExecutionDistances(float InMin, float InMax);
	void SetTestMaxFrontAngleDegrees(float InAngle) { MaxFrontAngleDegrees = InAngle; }
	bool TestEvaluateFrontGeometry(const APlayerCharacter* Player, const AEnemyCharacter* Target, float& OutDist2D, float& OutAngleDegrees) const;
	static bool TestEvaluateFrontGeometryVectors(
		const FVector& PlayerLoc,
		const FVector& TargetLoc,
		const FVector& TargetForward,
		float MinDist,
		float MaxDist,
		float MaxAngle,
		float& OutDist2D,
		float& OutAngleDegrees);
	void TestTriggerHitEvent(const FGameplayEventData& Payload);
	void TestTriggerReleaseRequestEvent(const FGameplayEventData& Payload);
	void TestTriggerVictimStartEvent(const FGameplayEventData& Payload);
	void TestEndAbility(bool bWasCancelled = false) { EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled); }
	void SetTestSkipMontageTaskActivation(bool bSkip) { bTestSkipMontageTaskActivation = bSkip; }
	void SetTestInvalidateWaitHitEventTaskAfterReady(bool bInvalidate) { bTestInvalidateWaitHitEventTaskAfterReady = bInvalidate; }
	void SetTestInvalidateWaitReleaseRequestTaskAfterReady(bool bInvalidate) { bTestInvalidateWaitReleaseRequestTaskAfterReady = bInvalidate; }
	void SetTestInvalidateWaitVictimStartEventTaskAfterReady(bool bInvalidate) { bTestInvalidateWaitVictimStartEventTaskAfterReady = bInvalidate; }
	void SetTestEndAbilityDuringTaskReady(bool bEnable) { bTestEndAbilityDuringTaskReady = bEnable; }
	AEnemyCharacter* GetTestReservedTarget() const { return ReservedTarget.Get(); }
	bool IsTestDamageEventConsumed() const { return bDamageEventConsumed; }
	bool IsTestReleaseRequestLatched() const { return bReleaseRequestLatched; }
	bool IsTestVictimReleaseExpected() const { return bVictimReleaseExpected; }
	bool IsTestVictimStartForwarded() const { return bVictimStartForwarded; }
	uint32 GetTestActivationToken() const { return CurrentActivationToken; }
	UPlayerFrontExecutionContext* GetTestActiveContext() const { return ActiveContext; }
	UExecutionLockContext* GetTestExecutionContext() const { return ActiveExecutionContext.Get(); }
	void SetTestForceCommitAbilityFailure(bool bForce) { bTestForceCommitAbilityFailure = bForce; }

private:
	bool bTestSkipMontageTaskActivation = false;
	bool bTestInvalidateWaitHitEventTaskAfterReady = false;
	bool bTestInvalidateWaitReleaseRequestTaskAfterReady = false;
	bool bTestInvalidateWaitVictimStartEventTaskAfterReady = false;
	bool bTestEndAbilityDuringTaskReady = false;
	bool bTestForceCommitAbilityFailure = false;
public:
#endif

	void HandleMontageCompleted(uint32 InToken);
	void HandleMontageBlendOut(uint32 InToken);
	void HandleMontageInterrupted(uint32 InToken);
	void HandleMontageCancelled(uint32 InToken);
	void HandleHitEventReceived(FGameplayEventData Payload, uint32 InToken);
	void HandleReleaseRequestEventReceived(FGameplayEventData Payload, uint32 InToken);
	void HandleVictimStartEventReceived(FGameplayEventData Payload, uint32 InToken);
	void HandleTargetDestroyed(AActor* DestroyedActor, uint32 InToken);
	void HandleTargetTagChanged(const FGameplayTag Tag, int32 NewCount, uint32 InToken);

	bool SendFormalReleaseToVictim(bool bWasCancelled, bool bRequireResolvedHit);

protected:
	virtual bool CommitAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) override;

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Execution", meta = (ToolTip = "处决命中时通过 FMeleeHitResolver 应用的伤害 GameplayEffect 类。"))
	TSubclassOf<UGameplayEffect> DamageGameplayEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Execution", meta = (ClampMin = "0.0", ClampMax = "90.0", Units = "Degrees", ToolTip = "允许触发正面处决的目标正前方最大夹角（度）。"))
	float MaxFrontAngleDegrees = 60.0f;

private:
	bool TryResolveExecutionMontage(
		const FGameplayAbilityActorInfo* ActorInfo,
		const UMeleeWeaponDefinition*& OutWeaponDef,
		UAnimMontage*& OutMontage) const;

	bool ValidateTargetPrerequisites(const APlayerCharacter* PlayerCharacter, const AEnemyCharacter* TargetActor) const;
	bool CheckFrontGeometry(
		const APlayerCharacter* PlayerCharacter,
		const AEnemyCharacter* TargetActor,
		float MinDist,
		float MaxDist,
		float& OutDist2D,
		float& OutAngleDegrees) const;
	bool TryApplyExecutionSnap(APlayerCharacter* PlayerCharacter, AEnemyCharacter* TargetActor, const FVector& TargetForwardSnapshot);
	void BindTargetDelegates(AEnemyCharacter* TargetActor, uint32 InToken);
	void UnbindTargetDelegates();

	float ActiveMinExecutionDistance = 0.0f;
	float ActiveMaxExecutionDistance = 0.0f;
	float ActiveExecutionSnapDistance = 0.0f;
	bool bHasActiveExecutionDistanceSnapshot = false;

	UPROPERTY(Transient)
	TObjectPtr<const UMeleeWeaponDefinition> ActiveExecutionWeaponDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveExecutionMontage;

	UPROPERTY(Transient)
	TObjectPtr<UExecutionLockContext> ActiveExecutionContext;

	UPROPERTY(Transient)
	TObjectPtr<UPlayerFrontExecutionContext> ActiveContext;

	uint32 CurrentActivationToken = 0;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitHitEventTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitReleaseRequestEventTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitVictimStartEventTask;

	UPROPERTY(Transient)
	TWeakObjectPtr<AEnemyCharacter> ReservedTarget;

	FDelegateHandle TargetStunnedTagDelegateHandle;
	FDelegateHandle TargetDeadTagDelegateHandle;
	FDelegateHandle TargetVictimLockedTagDelegateHandle;
	TWeakObjectPtr<UAbilitySystemComponent> BoundTargetASC;

	bool bEndAbilityInProgress = false;
	bool bDamageEventConsumed = false;
	bool bReleaseRequestLatched = false;
	bool bVictimReleaseExpected = false;
	bool bVictimStartForwarded = false;
};
