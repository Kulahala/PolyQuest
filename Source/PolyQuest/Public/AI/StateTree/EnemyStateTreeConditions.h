#pragma once

#include "CoreMinimal.h"
#include "StateTreeConditionBase.h"
#include "StateTreeExecutionTypes.h"
#include "EnemyStateTreeConditions.generated.h"

class AEnemyAIController;
struct FStateTreeExecutionContext;
struct FStateTreeLinker;

USTRUCT()
struct FEnemyStateTreeCondition_HasValidTargetInstanceData
{
	GENERATED_BODY()
};

/** Checks the controller-owned player target without introducing an AI enum or Blackboard key. */
USTRUCT(meta = (DisplayName = "Enemy Has Valid Target", Category = "AI|Enemy"))
struct POLYQUEST_API FEnemyStateTreeCondition_HasValidTarget : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FEnemyStateTreeCondition_HasValidTargetInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

private:
	TStateTreeExternalDataHandle<AEnemyAIController> EnemyAIControllerHandle;
};

USTRUCT()
struct FEnemyStateTreeCondition_IsTargetInMeleeRangeInstanceData
{
	GENERATED_BODY()
};

/** Uses the controller's one melee range for Alert/Chase/Combat selection. */
USTRUCT(meta = (DisplayName = "Enemy Target Is In Melee Range", Category = "AI|Enemy"))
struct POLYQUEST_API FEnemyStateTreeCondition_IsTargetInMeleeRange : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FEnemyStateTreeCondition_IsTargetInMeleeRangeInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

private:
	TStateTreeExternalDataHandle<AEnemyAIController> EnemyAIControllerHandle;
};
