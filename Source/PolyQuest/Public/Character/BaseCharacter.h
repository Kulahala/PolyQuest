// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "Combat/Melee/CombatTeamAgent.h"
#include "GameFramework/Character.h"
#include "BaseCharacter.generated.h"

class AController;
class UAbilitySystemComponent;
class UCharacterAttributeSet;
class UGameplayAbility;
class UMeleeTraceSourceComponent;
struct FOnAttributeChangeData;

UCLASS(Abstract)
class POLYQUEST_API ABaseCharacter : public ACharacter, public IAbilitySystemInterface, public ICombatTeamAgent
{
	GENERATED_BODY()

public:
	ABaseCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual int32 GetCombatTeamId_Implementation() const override;
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Returns the single fixed-weapon sample provider used by active melee abilities. */
	UMeleeTraceSourceComponent* GetMeleeTraceSource() const;

protected:
	void InitializeAbilityActorInfo();
	void GrantStartupAbilities();
	void BindMoveSpeedAttribute();
	void UnbindMoveSpeedAttribute();
	void OnMoveSpeedAttributeChanged(const FOnAttributeChangeData& ChangeData);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS")
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

	/** Equal identifiers are intentionally treated as friendly by the narrow melee resolver. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Team", meta = (AllowPrivateAccess = "true"))
	int32 CombatTeamId = 0;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCharacterAttributeSet> Attributes;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Melee", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMeleeTraceSourceComponent> MeleeTraceSource;

	TWeakObjectPtr<UAbilitySystemComponent> MoveSpeedBoundAbilitySystemComponent;
	FDelegateHandle MoveSpeedAttributeChangedHandle;
};
