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

USTRUCT()
struct FEnemyStateTreeCondition_IsAttackOnCooldownInstanceData
{
	GENERATED_BODY()
};

/** Checks if the controller is currently within attack cooldown. */
USTRUCT(meta = (DisplayName = "Enemy Is Attack On Cooldown", Category = "AI|Enemy"))
struct POLYQUEST_API FEnemyStateTreeCondition_IsAttackOnCooldown : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FEnemyStateTreeCondition_IsAttackOnCooldownInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

private:
	TStateTreeExternalDataHandle<AEnemyAIController> EnemyAIControllerHandle;
};

USTRUCT()
struct FEnemyStateTreeCondition_CanRequestRepositionInstanceData
{
	GENERATED_BODY()
};

/** Checks if cooldown repositioning can be requested on the controller. */
USTRUCT(meta = (DisplayName = "Enemy Can Request Reposition", Category = "AI|Enemy"))
struct POLYQUEST_API FEnemyStateTreeCondition_CanRequestReposition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FEnemyStateTreeCondition_CanRequestRepositionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

private:
	TStateTreeExternalDataHandle<AEnemyAIController> EnemyAIControllerHandle;
};

USTRUCT()
struct FEnemyStateTreeCondition_IsAttackReadyInstanceData
{
	GENERATED_BODY()
};

/** Checks if attack cooldown is over and enemy is ready to attack. */
USTRUCT(meta = (DisplayName = "Enemy Is Attack Ready", Category = "AI|Enemy"))
struct POLYQUEST_API FEnemyStateTreeCondition_IsAttackReady : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FEnemyStateTreeCondition_IsAttackReadyInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

private:
	TStateTreeExternalDataHandle<AEnemyAIController> EnemyAIControllerHandle;
};

USTRUCT()
struct FEnemyStateTreeCondition_IsTargetOutsideMeleeRangeInstanceData
{
	GENERATED_BODY()
};

/** Checks if the combat target is currently outside melee engagement range. */
USTRUCT(meta = (DisplayName = "Enemy Target Is Outside Melee Range", Category = "AI|Enemy"))
struct POLYQUEST_API FEnemyStateTreeCondition_IsTargetOutsideMeleeRange : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FEnemyStateTreeCondition_IsTargetOutsideMeleeRangeInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

private:
	TStateTreeExternalDataHandle<AEnemyAIController> EnemyAIControllerHandle;
};
