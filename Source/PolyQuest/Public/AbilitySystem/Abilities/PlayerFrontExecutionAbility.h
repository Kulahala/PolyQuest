#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayTagContainer.h"
#include "UObject/WeakObjectPtr.h"
#include "PlayerFrontExecutionAbility.generated.h"

class AEnemyCharacter;
class APlayerCharacter;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAnimMontage;
class UGameplayEffect;

/**
 * Server-authoritative front execution ability for the player.
 * Triggered on PrimaryAttack input when facing a stunned enemy (real stance break).
 * Reuses FMeleeHitResolver and standard Damage GameplayEffect delivery.
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
	void SetTestExecutionMontage(UAnimMontage* Montage) { ExecutionMontage = Montage; }
	void SetTestDamageGameplayEffectClass(TSubclassOf<UGameplayEffect> InClass) { DamageGameplayEffectClass = InClass; }
	void SetTestExecutionDistances(float InMin, float InMax) { MinExecutionDistance = InMin; MaxExecutionDistance = InMax; }
	void SetTestMaxFrontAngleDegrees(float InAngle) { MaxFrontAngleDegrees = InAngle; }
	void SetTestMotionWarpConfig(bool bEnabled, FName InTargetName, float InMin, float InStop, float InMax, float InAngle)
	{
		bUseMotionWarping = bEnabled;
		WarpTargetName = InTargetName;
		MinTriggerDistance = InMin;
		WarpStopDistance = InStop;
		MaxTriggerDistance = InMax;
		MaxWarpAngleDegrees = InAngle;
	}
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
	void TestTriggerHitEvent(const FGameplayEventData& Payload) { OnHitEventReceived(Payload); }
	void TestEndAbility(bool bWasCancelled = false) { EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bWasCancelled); }
	void SetTestSkipMontageTaskActivation(bool bSkip) { bTestSkipMontageTaskActivation = bSkip; }
	AEnemyCharacter* GetTestReservedTarget() const { return ReservedTarget.Get(); }
	bool IsTestDamageEventConsumed() const { return bDamageEventConsumed; }

private:
	bool bTestSkipMontageTaskActivation = false;
public:
#endif

protected:
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Execution", meta = (ToolTip = "正面处决播放的玩家动画 Montage。"))
	TObjectPtr<UAnimMontage> ExecutionMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Execution", meta = (ToolTip = "处决命中时通过 FMeleeHitResolver 应用的伤害 GameplayEffect 类。"))
	TSubclassOf<UGameplayEffect> DamageGameplayEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Execution", meta = (ClampMin = "0.0", Units = "Centimeters", ToolTip = "允许触发正面处决的最小水平距离（cm）。"))
	float MinExecutionDistance = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Execution", meta = (ClampMin = "0.0", Units = "Centimeters", ToolTip = "允许触发正面处决的最大水平距离（cm）。默认0保持fail-closed。"))
	float MaxExecutionDistance = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Execution", meta = (ClampMin = "0.0", ClampMax = "90.0", Units = "Degrees", ToolTip = "允许触发正面处决的目标正前方最大夹角（度）。"))
	float MaxFrontAngleDegrees = 60.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|MotionWarp", meta = (ToolTip = "是否在处决前摇启用 Motion Warping 贴近对齐目标。"))
	bool bUseMotionWarping = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|MotionWarp", meta = (EditCondition = "bUseMotionWarping", ToolTip = "处决 Motion Warp 目标标识名称。"))
	FName WarpTargetName = FName(TEXT("MeleeContact"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|MotionWarp", meta = (EditCondition = "bUseMotionWarping", ClampMin = "0.0", Units = "Centimeters", ToolTip = "处决 Motion Warp 最小触发距离。"))
	float MinTriggerDistance = 190.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|MotionWarp", meta = (EditCondition = "bUseMotionWarping", ClampMin = "0.0", Units = "Centimeters", ToolTip = "处决 Motion Warp 期望停止距离。"))
	float WarpStopDistance = 190.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|MotionWarp", meta = (EditCondition = "bUseMotionWarping", ClampMin = "0.0", Units = "Centimeters", ToolTip = "处决 Motion Warp 最大触发距离。"))
	float MaxTriggerDistance = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|MotionWarp", meta = (EditCondition = "bUseMotionWarping", ClampMin = "0.0", ClampMax = "180.0", Units = "Degrees", ToolTip = "处决 Motion Warp 最大允许角度偏转。"))
	float MaxWarpAngleDegrees = 60.0f;

private:
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
	void OnTargetDestroyed(AActor* DestroyedActor);

	void OnTargetTagChanged(const FGameplayTag Tag, int32 NewCount);

	bool ValidateTargetPrerequisites(const APlayerCharacter* PlayerCharacter, const AEnemyCharacter* TargetActor) const;
	bool CheckFrontGeometry(const APlayerCharacter* PlayerCharacter, const AEnemyCharacter* TargetActor, float& OutDist2D, float& OutAngleDegrees) const;
	void BindTargetDelegates(AEnemyCharacter* TargetActor);
	void UnbindTargetDelegates();

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitHitEventTask;

	UPROPERTY(Transient)
	TWeakObjectPtr<AEnemyCharacter> ReservedTarget;

	FDelegateHandle TargetStunnedTagDelegateHandle;
	FDelegateHandle TargetDeadTagDelegateHandle;
	TWeakObjectPtr<UAbilitySystemComponent> BoundTargetASC;

	bool bEndAbilityInProgress = false;
	bool bDamageEventConsumed = false;
};
