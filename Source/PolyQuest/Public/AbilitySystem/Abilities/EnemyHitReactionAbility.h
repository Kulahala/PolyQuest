#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystem/Abilities/MontageRateWindowLifecycle.h"
#include "GameplayTagContainer.h"
#include "EnemyHitReactionAbility.generated.h"

class ACharacter;
class AEnemyCharacter;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAnimInstance;
class UAnimMontage;
class UEnemyHitReactionAbility;

/**
 * Transient context for per-activation RateWindow event isolation.
 */
UCLASS(Transient)
class POLYQUEST_API UEnemyHitReactionRateWindowContext : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(Transient)
	TWeakObjectPtr<UEnemyHitReactionAbility> OwningAbility;

	uint32 Token = 0;

	UFUNCTION()
	void OnRateWindowBegin(FGameplayEventData Payload);

	UFUNCTION()
	void OnRateWindowEnd(FGameplayEventData Payload);
};

/**
 * Server-authoritative, non-lethal enemy big hit reaction. Damage delivery stays
 * in the shared resolver; this ability owns the accepted interruption, Root Motion displacement,
 * ledge safety, and teardown.
 */
UCLASS()
class POLYQUEST_API UEnemyHitReactionAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UEnemyHitReactionAbility();

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
	const FGameplayTag& GetTestRateWindowBeginEventTag() const { return RateWindowBeginEventTag; }
	const FGameplayTag& GetTestRateWindowEndEventTag() const { return RateWindowEndEventTag; }
	const FAbilityMontageRateWindowLifecycle& GetTestRateWindowLifecycle() const { return RateWindowLifecycle; }
	FAbilityMontageRateWindowLifecycle& GetTestRateWindowLifecycle_Mutable() { return RateWindowLifecycle; }
	int32 GetTestActiveMontageInstanceID() const { return ActiveMontageInstanceID; }
	void SetTestActiveMontageInstanceID(int32 InID) { ActiveMontageInstanceID = InID; }
	uint32 GetTestCurrentActivationToken() const { return CurrentActivationToken; }
	UEnemyHitReactionRateWindowContext* GetTestActiveRateWindowContext() const { return ActiveRateWindowContext.Get(); }
	void SetTestBypassMontageActiveCheck(bool bBypass)
	{
		bTestBypassMontageActiveCheck = bBypass;
		RateWindowLifecycle.SetTestBypassMontageActiveCheck(bBypass);
	}
	bool GetTestBypassMontageActiveCheck() const { return bTestBypassMontageActiveCheck; }
	void SetTestAbilityActive(bool bInActive) { bIsActive = bInActive; }
	void SetTestActorInfo(FGameplayAbilitySpecHandle InHandle, const FGameplayAbilityActorInfo* InActorInfo)
	{
		SetCurrentActorInfo(InHandle, InActorInfo);
	}
#endif

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Enemy|Reaction", meta = (AllowPrivateAccess = "true", ToolTip = "攻击者位于受击者本地前方时播放的受击硬直动画 Montage 资产。此名称描述攻击者来源方向，非受击者位移方向。四个方向属性（Front、Back、Left、Right）必须完整同时配置。"))
	TObjectPtr<UAnimMontage> FrontHitReactionMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Enemy|Reaction", meta = (AllowPrivateAccess = "true", ToolTip = "攻击者位于受击者本地后方时播放的受击硬直动画 Montage 资产。此名称描述攻击者来源方向，非受击者位移方向。四个方向属性（Front、Back、Left、Right）必须完整同时配置。"))
	TObjectPtr<UAnimMontage> BackHitReactionMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Enemy|Reaction", meta = (AllowPrivateAccess = "true", ToolTip = "攻击者位于受击者本地左方时播放的受击硬直动画 Montage 资产。此名称描述攻击者来源方向，非受击者位移方向。四个方向属性（Front、Back、Left、Right）必须完整同时配置。"))
	TObjectPtr<UAnimMontage> LeftHitReactionMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Enemy|Reaction", meta = (AllowPrivateAccess = "true", ToolTip = "攻击者位于受击者本地右方时播放的受击硬直动画 Montage 资产。此名称描述攻击者来源方向，非受击者位移方向。四个方向属性（Front、Back、Left、Right）必须完整同时配置。"))
	TObjectPtr<UAnimMontage> RightHitReactionMontage;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> RateWindowBeginTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> RateWindowEndTask;

	UPROPERTY(Transient)
	TObjectPtr<UEnemyHitReactionRateWindowContext> ActiveRateWindowContext;

	UPROPERTY(Transient)
	TObjectPtr<UAnimInstance> BoundAnimInstance;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveMontage;

	UPROPERTY(Transient)
	TWeakObjectPtr<AEnemyCharacter> BoundEnemyCharacter;

	FAbilityMontageRateWindowLifecycle RateWindowLifecycle;

	FGameplayTag HitReactionAbilityTag;
	FGameplayTag HitReactionEventTag;
	FGameplayTag HitReactingStateTag;
	FGameplayTag StunnedStateTag;
	FGameplayTag HyperArmorStateTag;
	FGameplayTag EnemyMeleeAbilityTag;
	FGameplayTag EnemySmallHitReactionAbilityTag;
	FGameplayTag RateWindowBeginEventTag;
	FGameplayTag RateWindowEndEventTag;
	FGameplayTag TeardownOnUnpossessTag;
	FGameplayTag FacingBlockedStateTag;
	FGameplayTagContainer AbilitiesToCancel;

	FVector ImpactDirectionSnapshot = FVector::ZeroVector;
	uint32 CurrentActivationToken = 0;
	int32 ActiveMontageInstanceID = INDEX_NONE;
	bool bSavedCanWalkOffLedges = true;
	bool bLedgeSettingModified = false;
	bool bMovementModeDelegateBound = false;
	bool bEndAbilityRequested = false;

#if WITH_DEV_AUTOMATION_TESTS
	bool bTestBypassMontageActiveCheck = false;
#endif

	UFUNCTION()
	void OnActiveMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	void OnMovementModeChanged(ACharacter* Character, EMovementMode PrevMovementMode, uint8 PreviousCustomMode);

	void OnRateWindowBegin(const FGameplayEventData& Payload);
	void OnRateWindowEnd(const FGameplayEventData& Payload);
	void ClearRateWindow(bool bRestoreRate);

	bool ValidateActivationSetup(const FGameplayAbilityActorInfo* ActorInfo) const;
	void EndFromMontage(bool bWasCancelled);

	friend class UEnemyHitReactionRateWindowContext;
};
