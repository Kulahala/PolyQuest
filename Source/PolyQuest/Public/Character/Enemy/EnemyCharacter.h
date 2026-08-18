#pragma once

#include "CoreMinimal.h"
#include "TimerManager.h"
#include "Character/BaseCharacter.h"
#include "EnemyCharacter.generated.h"

class UEnemyAttackSet;
class UGameplayEffect;
class UAbilitySystemComponent;
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

#if WITH_DEV_AUTOMATION_TESTS
	void SetTestAttackSet(UEnemyAttackSet* InSet) { AttackSet = InSet; }
#endif

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
	void OnPoiseRecoveryTick();
	void StartPoiseRecovery();
	void ClearPoiseRecovery();
	bool ApplyPoiseRecoveryMagnitude(float Magnitude);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Enemy", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnemyAttackSet> AttackSet;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Enemy|Death", meta = (AllowPrivateAccess = "true"))
	bool bUseRagdollOnDeath = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Enemy|Poise", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> PoiseRecoveryGameplayEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Enemy|Poise", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "Seconds"))
	float PoiseRecoveryDelaySeconds = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Enemy|Poise", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float PoiseRecoveryRate = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Enemy|Poise", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", Units = "Seconds"))
	float PoiseRecoveryTickIntervalSeconds = 0.1f;

	FGameplayTag DeadStateTag;
	FGameplayTag HitReactionEventTag;
	FGameplayTag InterruptReactionDataTag;
	FGameplayTag StanceBreakEventTag;
	FGameplayTag PoiseRecoveryDataTag;
	FGameplayTag StunnedStateTag;
	FDelegateHandle HealthAttributeChangedHandle;
	FDelegateHandle PoiseAttributeChangedHandle;
	FDelegateHandle DeadStateTagChangedHandle;
	TWeakObjectPtr<UAbilitySystemComponent> DeathBoundAbilitySystemComponent;
	FTimerHandle PoiseRecoveryTimerHandle;
	FTimerHandle PendingStanceBreakTimerHandle;
	bool bDeathTeardownStarted = false;
	bool bDeathRagdollStarted = false;
	bool bStanceBreakDispatchPending = false;
	bool bHasLoggedInvalidPoiseRecoveryConfiguration = false;
};
