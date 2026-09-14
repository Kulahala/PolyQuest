#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/Tasks/AbilityTask_PlayActionMontage.h"
#include "Animation/Combat/AnimNotifyState_ActionWindows.h"
#include "AbilitySystem/Abilities/DodgeAbility.h"
#include "TestManagedMontageAbility.generated.h"

class AEnemyCharacter;

struct FManagedMontageTestHelpers
{
#if WITH_DEV_AUTOMATION_TESTS
	/** Execution callback fixtures need an actually active StanceBreak before the paired handshake. */
	static FGameplayAbilitySpecHandle ActivateExecutionStanceBreak(AEnemyCharacter* Enemy);
#endif
	static FGameplayAbilityTargetDataHandle MakeRateWindowTargetData(UAnimInstance* AnimInstance, int32 MontageInstanceID);
	static FGameplayEventData MakeRateWindowEventData(
		const FGameplayTag& EventTag,
		UAnimInstance* AnimInstance,
		int32 MontageInstanceID,
		float Rate = 0.5f,
		AActor* AvatarActor = nullptr,
		UObject* Animation = nullptr,
		const UObject* RateNotify = nullptr);

	static FGameplayAbilityTargetDataHandle MakeCancelWindowTargetData(
		UAnimInstance* AnimInstance,
		int32 MontageInstanceID,
		bool bReachedEnd = false);

	static FGameplayEventData MakeCancelWindowEventData(
		const FGameplayTag& EventTag,
		UAnimInstance* AnimInstance,
		int32 MontageInstanceID,
		AActor* AvatarActor = nullptr,
		UObject* Animation = nullptr,
		const UObject* NotifyState = nullptr,
		bool bReachedEnd = false);

	static bool ValidateCancelWindowConfiguration(
		const UGameplayAbility* SourceAbility,
		const UAnimMontage* Montage,
		const UAbilityTask_PlayActionMontage* ActualTask,
		EActionMontageCancelPolicy ExpectedPolicy,
		const TArray<const UGameplayAbility*>& TargetAbilities,
		FString& OutDiagnosticReason,
		const FGameplayTagContainer& RequiredSourceTags = FGameplayTagContainer());
};

/** Minimal action example: configuration, standard playback, and one end path. */
UCLASS()
class UTestConfigurationOnlyActionAbility : public UGameplayAbility
{
	GENERATED_BODY()
public:
	UTestConfigurationOnlyActionAbility();
	UPROPERTY()
	TObjectPtr<UAnimMontage> Montage;
	EActionMontageCancelPolicy CancelPolicy = EActionMontageCancelPolicy::DodgeAndDefense;
	UAbilityTask_PlayActionMontage* GetMontageTask() const { return MontageTask; }
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
private:
	UFUNCTION()
	void OnCompleted();
	UFUNCTION()
	void OnCancelled();
	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayActionMontage> MontageTask;
};

UCLASS()
class UTestManagedMontageAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UTestManagedMontageAbility();

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

	UAbilityTask_PlayActionMontage* PlayActionMontage(
		UAnimMontage* MontageToPlay,
		float Rate = 1.0f,
		FName StartSection = NAME_None,
		float AnimRootMotionTranslationScale = 1.0f,
		float StartTimeSeconds = 0.0f,
		bool bAllowInterruptAfterBlendOut = false,
		EActionMontageCancelPolicy CancelPolicy = EActionMontageCancelPolicy::None);

	void EndTestAbility()
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}

	void SetTestAbilityActive(bool bInActive) { bIsActive = bInActive; }
	void SetTestActorInfo(FGameplayAbilitySpecHandle InHandle, const FGameplayAbilityActorInfo* InActorInfo)
	{
		SetCurrentActorInfo(InHandle, InActorInfo);
	}

	UAbilityTask_PlayActionMontage* GetMontageTask() const { return MontageTask.Get(); }

	UFUNCTION()
	void OnMontageCompleted();

	UFUNCTION()
	void OnMontageBlendedIn();

	UFUNCTION()
	void OnMontageBlendOut();

	UFUNCTION()
	void OnMontageInterrupted();

	UFUNCTION()
	void OnMontageCancelled();

	UFUNCTION()
	void OnMontageFailed();

	UPROPERTY()
	TObjectPtr<UAnimMontage> TestMontage;

	float TestRate = 1.0f;
	float InitialRootMotionScale = 1.0f;
	bool bTestAllowInterruptAfterBlendOut = false;
	EActionMontageCancelPolicy TestCancelPolicy = EActionMontageCancelPolicy::None;

	bool bCompletedCalled = false;
	bool bBlendedInCalled = false;
	bool bBlendOutCalled = false;
	bool bInterruptedCalled = false;
	bool bCancelledCalled = false;
	bool bFailedCalled = false;

	int32 CompletedCallCount = 0;
	int32 BlendedInCallCount = 0;
	int32 BlendOutCallCount = 0;
	int32 InterruptedCallCount = 0;
	int32 CancelledCallCount = 0;
	int32 FailedCallCount = 0;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayActionMontage> MontageTask;
};

/**
 * Test ability that inherits from UDodgeAbility but fails CommitCheck.
 * Used to verify GAS positive/negative cancellation behavior.
 */
UCLASS()
class UTestCommitFailingDodgeAbility : public UDodgeAbility
{
	GENERATED_BODY()

public:
	int32 CommitCheckCallCount = 0;

	virtual bool CommitCheck(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) override
	{
		++CommitCheckCallCount;
		return false;
	}
};
