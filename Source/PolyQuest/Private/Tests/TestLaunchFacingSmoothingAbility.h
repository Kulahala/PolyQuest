#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/Abilities/LaunchFacingSmoothingState.h"
#include "TestLaunchFacingSmoothingAbility.generated.h"

class ACharacter;
class UAbilityTask_TurnToFacing;

/**
 * Test-only Gameplay Ability that hosts UAbilityTask_TurnToFacing and FLaunchFacingSmoothingState
 * for native automation testing with a real ASC and World ticks.
 */
UCLASS()
class UTestLaunchFacingSmoothingAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UTestLaunchFacingSmoothingAbility();

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

	void StartTurnTask(float InStartYaw, float InTargetYaw, float InTurnRate);
	void EndTurnTask();
	void ResetCounters();

	UAbilityTask_TurnToFacing* GetActiveTurnTask() const { return TurnTask; }
	bool WasCompleted() const { return bTaskCompleted; }
	bool WasFailed() const { return bTaskFailed; }
	int32 GetCompletedCount() const { return CompletedCount; }
	int32 GetFailedCount() const { return FailedCount; }

	FLaunchFacingSmoothingState& GetSmoothingState() { return SmoothingState; }
	const FLaunchFacingSmoothingState& GetSmoothingState() const { return SmoothingState; }

	float TestStartYaw = 0.0f;
	float TestTargetYaw = 180.0f;
	float TestTurnRate = 1440.0f;
	bool bStartTaskOnActivate = false;

private:
	void HandleTurnCompleted();
	void HandleTurnFailed();

	UPROPERTY()
	TObjectPtr<UAbilityTask_TurnToFacing> TurnTask;

	FLaunchFacingSmoothingState SmoothingState;
	bool bTaskCompleted = false;
	bool bTaskFailed = false;
	int32 CompletedCount = 0;
	int32 FailedCount = 0;
};
