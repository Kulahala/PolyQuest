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
	float GetGuardStaminaDamage() const { return GuardStaminaDamage; }

#if WITH_DEV_AUTOMATION_TESTS
	void SetTestMontage(UAnimMontage* InMontage) { AttackMontage = InMontage; }
	void SetTestDamageEffectClass(TSubclassOf<UGameplayEffect> InClass) { DamageGameplayEffectClass = InClass; }
	void SetTestAttackRange(float InRange) { AttackRange = InRange; }
	void SetTestCooldown(float InCooldown) { CooldownAfterAttack = InCooldown; }
	void SetTestGuardStaminaDamage(float InDamage) { GuardStaminaDamage = InDamage; }
#endif

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Attack", meta = (AllowPrivateAccess = "true", ToolTip = "该攻击动作播放的动画 Montage 资产。"))
	TObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Attack", meta = (AllowPrivateAccess = "true", ToolTip = "攻击命中时施加的伤害 GameplayEffect 类。"))
	TSubclassOf<UGameplayEffect> DamageGameplayEffectClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Attack", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", Units = "Centimeters", ToolTip = "AI 检查该攻击是否可执行并发起接近移动时使用的目标距离阈值（厘米），不是武器碰撞半径。"))
	float AttackRange = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Attack", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", Units = "Seconds", ToolTip = "该攻击完成后进入的冷却时间（秒）。"))
	float CooldownAfterAttack = 1.0f;

	/** Stamina removed when this attack is successfully guarded. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Attack", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ToolTip = "该攻击被玩家格挡防御成功时扣除玩家的体力值。"))
	float GuardStaminaDamage = 25.0f;
};
