#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayTagContainer.h"
#include "BowDrawFireAbility.generated.h"

class ACombatProjectile;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAnimMontage;
class UGameplayEffect;

/**
 * GAS ability governing player bow draw, hold, and projectile release lifecycle.
 * Activated via Input.PrimaryAttack -> Ability.Attack.Primary while a bow is equipped.
 * Spawns a straight combat projectile upon receiving the semantic Release event from the active bow montage.
 */
UCLASS()
class POLYQUEST_API UBowDrawFireAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UBowDrawFireAbility();

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
	uint8 GetTestBowState() const { return static_cast<uint8>(BowState); }
	bool GetTestReleaseRequested() const { return bReleaseRequested; }
	bool GetTestSpawnedProjectile() const { return bSpawnedProjectile; }
	bool GetTestDodgeCancelable() const { return bDodgeCancelable; }
	bool GetTestChargingApplied() const { return bChargingApplied; }
	bool GetTestRateWindowApplied() const { return bRateWindowApplied; }
	void TestOnDrawReadyEvent(const FGameplayEventData& Payload) { OnDrawReadyEvent(Payload); }
	void TestOnReleaseAnimEvent(const FGameplayEventData& Payload) { OnReleaseAnimEvent(Payload); }
	void TestOnInputReleased(const FGameplayEventData& Payload) { OnInputReleased(Payload); }
	void TestOnInputCanceled(const FGameplayEventData& Payload) { OnInputCanceled(Payload); }
	void TestOnDodgeCancelWindowBegin(const FGameplayEventData& Payload) { OnDodgeCancelWindowBegin(Payload); }
	void TestOnDodgeCancelWindowEnd(const FGameplayEventData& Payload) { OnDodgeCancelWindowEnd(Payload); }
	void TestOnRateWindowBegin(const FGameplayEventData& Payload) { OnRateWindowBegin(Payload); }
	void TestOnRateWindowEnd(const FGameplayEventData& Payload) { OnRateWindowEnd(Payload); }
	void SetTestBowStateDrawing() { BowState = EBowState::Drawing; }
	void SetTestBowStateHolding() { BowState = EBowState::Holding; }
	void SetTestBowStateReleasing() { BowState = EBowState::Releasing; }
	void SetTestBowMontage(UAnimMontage* Montage) { BowMontage = Montage; }
	void SetTestCurrentActorInfo(const FGameplayAbilityActorInfo* InActorInfo) { CurrentActorInfo = InActorInfo; }
	void SetTestCurrentSpecHandle(const FGameplayAbilitySpecHandle InHandle) { CurrentSpecHandle = InHandle; }
	void TestSetCharging(bool bShouldCharge) { SetCharging(bShouldCharge); }
	void TestSetDodgeCancelable(bool bShouldCancel) { SetDodgeCancelable(bShouldCancel); }
	void TestRestoreBaselineMontageRate() { RestoreBaselineMontageRate(); }
	void TestSpawnProjectile() { SpawnProjectile(); }
	void SetTestTargetAssistScreenProjectionHook(TFunction<bool(const FVector&, FVector2D&, FVector2D&)> InHook) { TestTargetAssistScreenProjectionHook = MoveTemp(InHook); }
	bool Test_IsGameplayEventFromActiveMontage(const FGameplayEventData& Payload) const { return IsGameplayEventFromActiveMontage(Payload); }
	void SetTestMobileBowMoveSpeedGameplayEffectClass(TSubclassOf<UGameplayEffect> InClass) { MobileBowMoveSpeedGameplayEffectClass = InClass; }
	bool TestStartMobileBowMoveSpeedEffect() { return StartMobileBowMoveSpeedEffect(); }
	void TestClearMobileBowMoveSpeedEffect() { ClearMobileBowMoveSpeedEffect(); }
	bool HasTestMobileBowMoveSpeedEffectHandle() const { return MobileBowMoveSpeedEffectHandle.IsValid(); }
	FActiveGameplayEffectHandle GetTestMobileBowMoveSpeedEffectHandle() const { return MobileBowMoveSpeedEffectHandle; }
#endif

protected:
	/** Authored bow animation montage (Draw -> Hold -> Release). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Animation", meta = (ToolTip = "弓箭拉弓、蓄力与释放完整流程的动画 Montage。"))
	TObjectPtr<UAnimMontage> BowMontage;

	/** Section name for the initial draw animation phase. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Animation", meta = (ToolTip = "拉弓起手阶段在 Montage 中的 Section 名称。"))
	FName DrawSectionName = TEXT("Draw");

	/** Section name for the held draw animation loop. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Animation", meta = (ToolTip = "拉满蓄力维持阶段在 Montage 中的 Section 名称。"))
	FName HoldSectionName = TEXT("Hold");

	/** Section name for the release animation phase. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Animation", meta = (ToolTip = "松手射出阶段在 Montage 中的 Section 名称。"))
	FName ReleaseSectionName = TEXT("Release");

	/** Optional projectile Actor class override; defaults to ACombatProjectile. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Projectile", meta = (ToolTip = "可选的投射物 Actor 类重写，默认使用 ACombatProjectile。"))
	TSubclassOf<ACombatProjectile> ProjectileClass;

	/** Authored continuous move-speed GameplayEffect applied during the active mobile Bow lifecycle. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bow|Movement", meta = (ToolTip = "拉弓移动期间施加的移动速度修饰 GameplayEffect 类。"))
	TSubclassOf<UGameplayEffect> MobileBowMoveSpeedGameplayEffectClass;

private:
	enum class EBowState : uint8
	{
		Inactive,
		Drawing,
		Holding,
		Releasing
	};

	bool IsValidAvatarEventPayload(const FGameplayEventData& Payload) const;
	bool IsGameplayEventFromActiveMontage(const FGameplayEventData& Payload) const;

	UFUNCTION()
	void OnMontageCompleted();

	UFUNCTION()
	void OnMontageBlendOut();

	UFUNCTION()
	void OnMontageInterrupted();

	UFUNCTION()
	void OnMontageCancelled();

	UFUNCTION()
	void OnDrawReadyEvent(FGameplayEventData Payload);

	UFUNCTION()
	void OnReleaseAnimEvent(FGameplayEventData Payload);

	UFUNCTION()
	void OnInputReleased(FGameplayEventData Payload);

	UFUNCTION()
	void OnInputCanceled(FGameplayEventData Payload);

	UFUNCTION()
	void OnDodgeCancelWindowBegin(FGameplayEventData Payload);

	UFUNCTION()
	void OnDodgeCancelWindowEnd(FGameplayEventData Payload);

	UFUNCTION()
	void OnRateWindowBegin(FGameplayEventData Payload);

	UFUNCTION()
	void OnRateWindowEnd(FGameplayEventData Payload);

	void TriggerRelease();
	void SpawnProjectile();
	void SetCharging(bool bShouldCharge);
	void SetDodgeCancelable(bool bShouldBeCancelable);
	void RestoreBaselineMontageRate();
	bool StartMobileBowMoveSpeedEffect();
	void ClearMobileBowMoveSpeedEffect();

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitDrawReadyTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitReleaseTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitInputReleasedTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitInputCanceledTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> DodgeCancelWindowBeginTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> DodgeCancelWindowEndTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> RateWindowBeginTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> RateWindowEndTask;

	FGameplayTag PrimaryAttackInputTag;
	FGameplayTag DrawReadyEventTag;
	FGameplayTag ReleaseEventTag;
	FGameplayTag InputReleasedEventTag;
	FGameplayTag InputCanceledEventTag;
	FGameplayTag DodgeCancelWindowBeginEventTag;
	FGameplayTag DodgeCancelWindowEndEventTag;
	FGameplayTag RateWindowBeginEventTag;
	FGameplayTag RateWindowEndEventTag;
	FGameplayTag DodgeCancelableStateTag;
	FGameplayTag DefenseCancelableStateTag;
	FGameplayTag ChargingStateTag;

	EBowState BowState = EBowState::Inactive;
	bool bReleaseRequested = false;
	bool bSpawnedProjectile = false;
	bool bEndAbilityInProgress = false;
	bool bDodgeCancelable = false;
	bool bChargingApplied = false;
	bool bRateWindowApplied = false;
	FActiveGameplayEffectHandle MobileBowMoveSpeedEffectHandle;

#if WITH_DEV_AUTOMATION_TESTS
	TFunction<bool(const FVector&, FVector2D&, FVector2D&)> TestTargetAssistScreenProjectionHook;
#endif
};
