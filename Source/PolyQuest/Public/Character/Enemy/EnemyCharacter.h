#pragma once

#include "CoreMinimal.h"
#include "Character/BaseCharacter.h"
#include "EnemyCharacter.generated.h"

class UEnemyAttackProfile;
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

	/** Static authored attack input. Runtime cooldown and active attack state do not live on the Character. */
	UFUNCTION(BlueprintPure, Category = "Combat|Enemy")
	UEnemyAttackProfile* GetAttackProfile() const { return AttackProfile; }

	/** The ASC-owned terminal tag is the only gameplay source of truth for enemy death. */
	UFUNCTION(BlueprintPure, Category = "Combat|Enemy")
	bool IsDead() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void BindDeathEvents();
	void UnbindDeathEvents();
	void OnHealthAttributeChanged(const FOnAttributeChangeData& ChangeData);
	void OnDeadStateTagChanged(const FGameplayTag Tag, int32 NewCount);
	void SetDeadState();
	void HandleDeath();
	void StartDeathRagdoll();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Enemy", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnemyAttackProfile> AttackProfile;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Enemy|Death", meta = (AllowPrivateAccess = "true"))
	bool bUseRagdollOnDeath = true;

	FGameplayTag DeadStateTag;
	FDelegateHandle HealthAttributeChangedHandle;
	FDelegateHandle DeadStateTagChangedHandle;
	TWeakObjectPtr<UAbilitySystemComponent> DeathBoundAbilitySystemComponent;
	bool bDeathTeardownStarted = false;
	bool bDeathRagdollStarted = false;
};
