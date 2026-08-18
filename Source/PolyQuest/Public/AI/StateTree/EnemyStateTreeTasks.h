#pragma once

#include "CoreMinimal.h"
#include "StateTreeExecutionTypes.h"
#include "Tasks/StateTreeAITask.h"
#include "EnemyStateTreeTasks.generated.h"

class AEnemyAIController;
struct FStateTreeExecutionContext;
struct FStateTreeLinker;

USTRUCT()
struct FEnemyStateTreeTask_BeginAlertInstanceData
{
	GENERATED_BODY()
};

/** Stops stale path following at the Alert boundary without issuing combat mutations. */
USTRUCT(meta = (DisplayName = "Enemy Begin Alert", Category = "AI|Enemy"))
struct POLYQUEST_API FEnemyStateTreeTask_BeginAlert : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FEnemyStateTreeTask_BeginAlertInstanceData;

	FEnemyStateTreeTask_BeginAlert();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

private:
	TStateTreeExternalDataHandle<AEnemyAIController> EnemyAIControllerHandle;
};

USTRUCT()
struct FEnemyStateTreeTask_RequestMeleeAttackInstanceData
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	bool bObservedAttacking = false;
};

/** Requests the GAS ability once, then waits for its owned action tag to clear naturally. */
USTRUCT(meta = (DisplayName = "Enemy Request Melee Attack", Category = "AI|Enemy"))
struct POLYQUEST_API FEnemyStateTreeTask_RequestMeleeAttack : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FEnemyStateTreeTask_RequestMeleeAttackInstanceData;

	FEnemyStateTreeTask_RequestMeleeAttack();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

private:
	TStateTreeExternalDataHandle<AEnemyAIController> EnemyAIControllerHandle;
};

USTRUCT()
struct FEnemyStateTreeTask_RepositionDuringCooldownInstanceData
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	bool bIssuedMoveRequest = false;
};

/** Executes a bounded reposition move during melee cooldown and returns upon move completion, failure, or cooldown expiry. */
USTRUCT(meta = (DisplayName = "Enemy Reposition During Cooldown", Category = "AI|Enemy"))
struct POLYQUEST_API FEnemyStateTreeTask_RepositionDuringCooldown : public FStateTreeAITaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FEnemyStateTreeTask_RepositionDuringCooldownInstanceData;

	FEnemyStateTreeTask_RepositionDuringCooldown();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

private:
	TStateTreeExternalDataHandle<AEnemyAIController> EnemyAIControllerHandle;
};
