#include "Combat/Enemy/EnemyAttackProfile.h"

#include "Animation/AnimMontage.h"
#include "GameplayEffect.h"

bool UEnemyAttackProfile::IsValidAttackProfile() const
{
	return AttackMontage != nullptr && DamageGameplayEffectClass != nullptr
		&& FMath::IsFinite(AttackRange) && AttackRange > 0.0f
		&& FMath::IsFinite(CooldownAfterAttack) && CooldownAfterAttack >= 0.0f
		&& FMath::IsFinite(GuardStaminaDamage) && GuardStaminaDamage >= 0.0f;
}
