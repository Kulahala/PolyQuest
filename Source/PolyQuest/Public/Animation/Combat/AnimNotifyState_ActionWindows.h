#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_ActionWindows.generated.h"

class UAnimInstance;
struct FBranchingPointNotifyPayload;

/**
 * Target data carrying local source playback identity (AnimInstance and MontageInstanceID)
 * for Montage RateWindow and CancelWindow event routing and fail-closed validation.
 */
USTRUCT()
struct POLYQUEST_API FGameplayAbilityTargetData_MontageRateWindowSource : public FGameplayAbilityTargetData
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<UAnimInstance> AnimInstance;

	UPROPERTY()
	int32 MontageInstanceID = INDEX_NONE;

	/** This window reached its boundary: verified sampling time for Queued, native payload for BranchingPoint. */
	UPROPERTY()
	bool bReachedEnd = false;

	/** Raw engine result for diagnostics; Queued notify contexts may share this flag across different events. */
	UPROPERTY()
	bool bNativeReachedEnd = false;

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return StaticStruct();
	}
};

UCLASS()
class POLYQUEST_API UAnimNotifyState_ActionDodgeCancelWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual void BranchingPointNotifyBegin(FBranchingPointNotifyPayload& BranchingPointPayload) override;
	virtual void BranchingPointNotifyEnd(FBranchingPointNotifyPayload& BranchingPointPayload) override;
	virtual FString GetNotifyName_Implementation() const override;
};

UCLASS()
class POLYQUEST_API UAnimNotifyState_DodgeInvulnerability : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
};

/** Sends the charged attack's semantic hold-ready timing event. */
UCLASS()
class POLYQUEST_API UAnimNotify_PlayerChargedAttackHoldReady : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
};

/** Sends only the start/end timing for an Ability-owned melee trace window. */
UCLASS()
class POLYQUEST_API UAnimNotifyState_AttackTraceWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

	const TArray<FName>& GetTraceSourceNames() const { return TraceSourceNames; }

	/** Explicit contact sources to trace during this window (e.g. RightFist, LeftFist). Empty resolves default weapon source. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trace", meta = (AllowPrivateAccess = "true"))
	TArray<FName> TraceSourceNames;
};

UCLASS()
class POLYQUEST_API UAnimNotifyState_PlayerComboInputWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
};

UCLASS()
class POLYQUEST_API UAnimNotifyState_PlayerComboBranchWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
};

/** Sends only the start/end timing for the parry-ability's active defense window. */
UCLASS()
class POLYQUEST_API UAnimNotifyState_PlayerParryWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
};

/** Sends only the start/end timing for an enemy melee Hyper Armor window. */
UCLASS()
class POLYQUEST_API UAnimNotifyState_EnemyHyperArmor : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
};

/** Sends the authored playback-rate override window for the active attack Montage, carrying window identity in OptionalObject2. */
UCLASS()
class POLYQUEST_API UAnimNotifyState_MontageRateWindow : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual void BranchingPointNotifyBegin(FBranchingPointNotifyPayload& BranchingPointPayload) override;
	virtual void BranchingPointNotifyEnd(FBranchingPointNotifyPayload& BranchingPointPayload) override;
	virtual FString GetNotifyName_Implementation() const override;

	/** Playback-rate multiplier the identity-matched active Montage applies while this window is open. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Rate", meta = (ClampMin = "0.01"))
	float RateMultiplier = 1.0f;
};
