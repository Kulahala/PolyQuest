#include "Combat/Enemy/EnemyAttackProfile.h"

#include "Animation/AnimMontage.h"
#include "GameplayEffect.h"

bool UEnemyAttackProfile::IsValidAttackProfile() const
{
	return AttackMontage != nullptr && DamageGameplayEffectClass != nullptr && AttackRange > 0.0f && CooldownAfterAttack >= 0.0f && GuardStaminaDamage >= 0.0f;
}
