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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI|Enemy", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWidgetComponent> EnemyHealthBarWidgetComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Enemy", meta = (AllowPrivateAccess = "true", ToolTip = "敌人使用的攻击集合配置资产（EnemyAttackSet）。"))
	TObjectPtr<UEnemyAttackSet> AttackSet;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Enemy", meta = (AllowPrivateAccess = "true", ToolTip = "敌人空间走位、重定位与警戒距离配置资产（EnemyAIProfile）。"))
	TObjectPtr<UEnemyAIProfile> AIProfile;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Enemy|Death", meta = (AllowPrivateAccess = "true", ToolTip = "死亡时是否尝试开启物理布娃娃模拟；需要角色网格体与有效 Physics Asset，缺失时跳过布娃娃并记录 Warning。"))
	bool bUseRagdollOnDeath = true;

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
	TWeakObjectPtr<const UGameplayEffect> ActivePoiseBreakingEffectDefinition;
	FGameplayEffectContextHandle ActivePoiseBreakingEffectContext;
};
