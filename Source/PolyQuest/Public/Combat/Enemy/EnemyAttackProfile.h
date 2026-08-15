#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "EnemyAttackProfile.generated.h"

class UAnimMontage;
class UGameplayEffect;

/** Immutable authored attack input for one enemy. Runtime selection and cooldown remain on the controller and active Ability. */
UCLASS(BlueprintType)
class POLYQUEST_API UEnemyAttackProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Rejects incomplete authored data before AI logic or a melee Ability can use it. */
	bool IsValidAttackProfile() const;

	UAnimMontage* GetAttackMontage() const { return AttackMontage; }
	TSubclassOf<UGameplayEffect> GetDamageGameplayEffectClass() const { return DamageGameplayEffectClass; }
	float GetAttackRange() const { return AttackRange; }
	float GetCooldownAfterAttack() const { return CooldownAfterAttack; }

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Attack", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Attack", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> DamageGameplayEffectClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Attack", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", Units = "Centimeters"))
	float AttackRange = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Attack", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "Seconds"))
	float CooldownAfterAttack = 1.0f;
};
