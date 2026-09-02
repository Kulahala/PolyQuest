#pragma once

#include "CoreMinimal.h"
#include "TimerManager.h"
#include "GameplayEffectTypes.h"
#include "Character/BaseCharacter.h"
#include "EnemyCharacter.generated.h"

class UEnemyAIProfile;
class UEnemyAttackSet;
class UGameplayEffect;
class UAbilitySystemComponent;
class UWidgetComponent;
class UEnemyHealthBarWidget;
class USoundBase;
class UNiagaraSystem;
class UEnemyCombatFeedbackDataAsset;
enum class EHitReactionTier : uint8;
struct FGameplayEffectSpec;
struct FOnAttributeChangeData;

/**
 * First native enemy endpoint. It inherits the shared ASC, melee trace source,
 * and startup-ability grant lifecycle from ABaseCharacter.
 */
UCLASS(Blueprintable)
class POLYQUEST_API AEnemyCharacter : public ABaseCharacter
{
	GENERATED_BODY()

public:
	AEnemyCharacter();

	/** Static authored attack set. Runtime cooldown and active attack state do not live on the Character. */
	UFUNCTION(BlueprintPure, Category = "Combat|Enemy")
	UEnemyAttackSet* GetAttackSet() const { return AttackSet; }

	/** Static authored AI profile for combat spacing, repositioning, and leash parameters. */
	UFUNCTION(BlueprintPure, Category = "AI|Enemy")
	UEnemyAIProfile* GetAIProfile() const { return AIProfile; }

	/** Returns the configured enemy combat feedback data asset, or nullptr if unset or invalid type. */
	UEnemyCombatFeedbackDataAsset* GetEnemyCombatFeedbackData() const;

	/** C++-only presentation bridge for the local Player lock-on highlight. */
	void SetPlayerLockOnHighlighted(bool bHighlighted);

#if WITH_DEV_AUTOMATION_TESTS
	/** Configures the passive native Enemy fixture before BeginPlay without inventing AI or ragdoll coverage. */
	void ConfigureTestPassiveStartupFixture(TSubclassOf<UGameplayEffect> InPoiseRecoveryGameplayEffectClass);

	void SetTestAttackSet(UEnemyAttackSet* InSet) { AttackSet = InSet; }
	void SetTestAIProfile(UEnemyAIProfile* InProfile) { AIProfile = InProfile; }
	void SetTestPoiseRecoveryGameplayEffectClass(TSubclassOf<UGameplayEffect> InClass) { PoiseRecoveryGameplayEffectClass = InClass; }
	bool IsLaunchStanceBreakDeferralActive() const { return bLaunchStanceBreakDeferralActive; }
	bool HasPendingDeferredStanceBreak() const { return bPendingDeferredStanceBreak; }
	bool HasPendingStanceBreakTimer() const { return PendingStanceBreakTimerHandle.IsValid() || bStanceBreakDispatchPending; }
	void DispatchTestPendingStanceBreak() { DispatchPendingStanceBreak(); }
	UWidgetComponent* GetTestHealthBarWidgetComponent() const { return EnemyHealthBarWidgetComponent; }
	UEnemyHealthBarWidget* GetTestHealthBarWidget() const;
	void SetTestHealthBarWidget(UEnemyHealthBarWidget* InWidget);
	bool HasBoundUIHealthDelegates() const { return UIHealthAttributeChangedHandle.IsValid() && UIMaxHealthAttributeChangedHandle.IsValid(); }
	void TriggerTestBindUIHealthEvents() { BindUIHealthEvents(); }
	void TriggerTestUnbindUIHealthEvents() { UnbindUIHealthEvents(); }
	void TriggerTestRefreshEnemyHealthBar() { RefreshEnemyHealthBar(); }
	void TriggerTestUnPossessed() { UnPossessed(); }

	void ConfigureTestDeathRagdollImpact(
		FName InImpulseBoneName,
		float InHorizontalVelocityChange,
		float InUpwardVelocityChange);
	FVector GetTestLastDeathRagdollVelocityChange() const;
	FVector GetTestPendingDeathRagdollVelocityChange() const;
	int32 GetTestDeathRagdollCaptureCount() const;
	int32 GetTestDeathRagdollConsumeCount() const;

	int32 GetTestCombatImpactHitStopRequestCount() const { return TestCombatImpactHitStopRequestCount; }
	float GetTestLastImpactHitStopDuration() const { return TestLastImpactHitStopDuration; }
	float GetTestLastImpactHitStopTimeDilation() const { return TestLastImpactHitStopTimeDilation; }
	int32 GetTestImpactSoundDispatchCount() const { return TestImpactSoundDispatchCount; }
	FVector GetTestLastImpactSoundLocation() const { return TestLastImpactSoundLocation; }
	int32 GetTestImpactBloodDispatchCount() const { return TestImpactBloodDispatchCount; }
	FVector GetTestLastImpactBloodLocation() const { return TestLastImpactBloodLocation; }
	FVector GetTestLastImpactBloodNormal() const { return TestLastImpactBloodNormal; }
	FRotator GetTestLastImpactBloodRotation() const { return TestLastImpactBloodRotation; }
#endif

	/** Native-only lifecycle hooks for pairing with Launch hit reaction stance break deferral. */
	void BeginLaunchStanceBreakDeferral();
	void CompleteLaunchStanceBreakDeferral();
	void AbortLaunchStanceBreakDeferral();

	/** The ASC-owned terminal tag is the only gameplay source of truth for enemy death. */
	UFUNCTION(BlueprintPure, Category = "Combat|Enemy")
	bool IsDead() const;

	UFUNCTION(BlueprintPure, Category = "Combat|Enemy|Poise")
	bool IsPoiseBroken() const;

	UFUNCTION(BlueprintPure, Category = "Combat|Enemy|Poise")
	bool HasValidPoiseRecoveryConfiguration() const;

	/** Applies the authored recovery effect until the current Poise reaches MaxPoise. */
	bool RestorePoiseToMax();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void UnPossessed() override;

private:
	void BindDeathEvents();
	void UnbindDeathEvents();
	void OnHealthAttributeChanged(const FOnAttributeChangeData& ChangeData);
	void OnPoiseAttributeChanged(const FOnAttributeChangeData& ChangeData);
	void OnDeadStateTagChanged(const FGameplayTag Tag, int32 NewCount);
	void SetDeadState();
	void HandleDeath();
	void StartDeathRagdoll();
	void DispatchPendingStanceBreak();
	bool TryDispatchStanceBreak();
	void CacheActivePoiseBreakingEffectSource(const FGameplayEffectSpec& EffectSpec);
	bool MatchesActivePoiseBreakingEffectSource(const FGameplayEffectSpec& EffectSpec) const;
	void ClearActivePoiseBreakingEffectSource();
	void OnPoiseRecoveryTick();
	void StartPoiseRecovery();
	void ClearPoiseRecovery();
	bool ApplyPoiseRecoveryMagnitude(float Magnitude);

	void BindUIHealthEvents();
	void UnbindUIHealthEvents();
	void OnUIHealthAttributeChanged(const FOnAttributeChangeData& ChangeData);
	void OnUIMaxHealthAttributeChanged(const FOnAttributeChangeData& ChangeData);
	void RefreshEnemyHealthBar();
	void HideEnemyHealthBar();
	void HandleCombatImpactFeedback(const FGameplayEffectSpec& EffectSpec, EHitReactionTier ReactionTier);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI|Enemy", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWidgetComponent> EnemyHealthBarWidgetComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Enemy", meta = (AllowPrivateAccess = "true", ToolTip = "敌人使用的攻击集合配置资产（EnemyAttackSet）。"))
	TObjectPtr<UEnemyAttackSet> AttackSet;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Enemy", meta = (AllowPrivateAccess = "true", ToolTip = "敌人空间走位、重定位与警戒距离配置资产（EnemyAIProfile）。"))
	TObjectPtr<UEnemyAIProfile> AIProfile;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Enemy|Death", meta = (AllowPrivateAccess = "true", ToolTip = "死亡时是否尝试开启物理布娃娃模拟；需要角色网格体与有效 Physics Asset，缺失时跳过布娃娃并记录 Warning。"))
	bool bUseRagdollOnDeath = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Enemy|Death", meta = (AllowPrivateAccess = "true", ToolTip = "死亡击飞冲量施加的目标物理骨骼名称；NAME_None 表示不施加额外方向冲量（保持自然布娃娃下落）。"))
	FName DeathRagdollImpulseBoneName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Enemy|Death", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "CentimetersPerSecond", ToolTip = "死亡布娃娃远离攻击者的水平速度变化量（cm/s）。"))
	float DeathRagdollHorizontalVelocityChange = 2000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Enemy|Death", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "CentimetersPerSecond", ToolTip = "死亡布娃娃向上的垂直速度变化量（cm/s）。"))
	float DeathRagdollUpwardVelocityChange = 500.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Enemy|Poise", meta = (AllowPrivateAccess = "true", ToolTip = "用于定时恢复韧性值的 Instant GameplayEffect 类。"))
	TSubclassOf<UGameplayEffect> PoiseRecoveryGameplayEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Enemy|Poise", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "Seconds", ToolTip = "受到削韧攻击后重新启动韧性自然恢复的延迟时间（秒）。"))
	float PoiseRecoveryDelaySeconds = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Enemy|Poise", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ToolTip = "每秒恢复的韧性数值。"))
	float PoiseRecoveryRate = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Enemy|Poise", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", Units = "Seconds", ToolTip = "韧性恢复定时器的 Tick 触发间隔（秒）。"))
	float PoiseRecoveryTickIntervalSeconds = 0.1f;

	FGameplayTag DeadStateTag;
	FGameplayTag HitReactionEventTag;
	FGameplayTag SmallHitReactionEventTag;
	FGameplayTag LaunchReactionEventTag;
	FGameplayTag StanceBreakEventTag;
	FGameplayTag PoiseRecoveryDataTag;
	FGameplayTag StunnedStateTag;
	FDelegateHandle HealthAttributeChangedHandle;
	FDelegateHandle PoiseAttributeChangedHandle;
	FDelegateHandle DeadStateTagChangedHandle;
	TWeakObjectPtr<UAbilitySystemComponent> DeathBoundAbilitySystemComponent;
	FTimerHandle PoiseRecoveryTimerHandle;
	FTimerHandle PendingStanceBreakTimerHandle;

	FDelegateHandle UIHealthAttributeChangedHandle;
	FDelegateHandle UIMaxHealthAttributeChangedHandle;
	TWeakObjectPtr<UAbilitySystemComponent> UIBoundAbilitySystemComponent;
	TWeakObjectPtr<UEnemyHealthBarWidget> EnemyHealthBarWidget;

	bool bDeathTeardownStarted = false;
	bool bDeathRagdollStarted = false;
	bool bStanceBreakDispatchPending = false;
	bool bLaunchStanceBreakDeferralActive = false;
	bool bPendingDeferredStanceBreak = false;
	bool bHasLoggedInvalidPoiseRecoveryConfiguration = false;
	bool bHasLoggedInvalidUIWidgetClass = false;
	bool bPlayerLockOnHighlighted = false;
	bool bHasLoggedMissingCombatFeedbackData = false;
	TWeakObjectPtr<const UGameplayEffect> ActivePoiseBreakingEffectDefinition;
	FGameplayEffectContextHandle ActivePoiseBreakingEffectContext;

	FVector PendingDeathRagdollVelocityChange = FVector::ZeroVector;
	bool bHasLoggedInvalidDeathRagdollBone = false;
#if WITH_DEV_AUTOMATION_TESTS
	FVector LastDeathRagdollVelocityChange = FVector::ZeroVector;
	int32 DeathRagdollCaptureCount = 0;
	int32 DeathRagdollConsumeCount = 0;
	int32 TestCombatImpactHitStopRequestCount = 0;
	float TestLastImpactHitStopDuration = 0.0f;
	float TestLastImpactHitStopTimeDilation = 0.0f;
	int32 TestImpactSoundDispatchCount = 0;
	FVector TestLastImpactSoundLocation = FVector::ZeroVector;
	int32 TestImpactBloodDispatchCount = 0;
	FVector TestLastImpactBloodLocation = FVector::ZeroVector;
	FVector TestLastImpactBloodNormal = FVector::ZeroVector;
	FRotator TestLastImpactBloodRotation = FRotator::ZeroRotator;
#endif
};
