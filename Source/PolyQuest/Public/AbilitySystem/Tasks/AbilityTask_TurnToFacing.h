#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_TurnToFacing.generated.h"

class ACharacter;
class UGameplayAbility;

DECLARE_MULTICAST_DELEGATE(FFacingTurnCompletedDelegate);
DECLARE_MULTICAST_DELEGATE(FFacingTurnFailedDelegate);

/**
 * AbilityTask that turns a Character smoothly toward a target yaw at a fixed rate.
 * C++-only, Ability-owned, tick-driven task.
 */
UCLASS()
class POLYQUEST_API UAbilityTask_TurnToFacing : public UAbilityTask
{
	GENERATED_BODY()

public:
	UAbilityTask_TurnToFacing();

	static UAbilityTask_TurnToFacing* TurnToFacing(
		UGameplayAbility* OwningAbility,
		ACharacter* InTargetCharacter,
		float InStartYaw,
		float InTargetYaw,
		float InTurnRateDegreesPerSecond);

	virtual void TickTask(float DeltaTime) override;
	virtual void OnDestroy(bool AbilityIsEnding) override;

	FFacingTurnCompletedDelegate OnTurnCompleted;
	FFacingTurnFailedDelegate OnTurnFailed;

protected:
	virtual void Activate() override;

private:
	void BroadcastCompletionAndEnd();
	void BroadcastFailureAndEnd();

	TWeakObjectPtr<ACharacter> TargetCharacter;
	float StartYaw = 0.0f;
	float TargetYaw = 0.0f;
	float TurnRateDegreesPerSecond = 0.0f;
	bool bCompleted = false;
	bool bFailed = false;
};
