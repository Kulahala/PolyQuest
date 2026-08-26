#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"
#include "UObject/ObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class UAbilityTask_MeleeTraceWindow;
class UAnimNotifyState_AttackTraceWindow;
class UGameplayAbility;
class UGameplayEffect;

/**
 * Stateless private helper consolidating the repetitive mechanical task lifecycle
 * (closed check, keep open check, avatar/trace-source resolution, factory creation,
 * ReadyForActivation, activation failure cleanup, and close/clear) for player melee abilities.
 *
 * It holds no state, caches no UObjects, and leaves Task/Notify ownership with the caller.
 */
struct FMeleeTraceWindowLifecycle
{
	static void OpenOrKeepScalar(
		UGameplayAbility* OwningAbility,
		TObjectPtr<UAbilityTask_MeleeTraceWindow>& InOutTask,
		TSubclassOf<UGameplayEffect> DamageGameplayEffectClass,
		float AbilityLevel,
		FGameplayTag SetByCallerMagnitudeTag,
		float SetByCallerMagnitude,
		float GuardStaminaDamage,
		const TArray<FName>& TraceSourceNames);

	static void OpenOrKeepMagnitudes(
		UGameplayAbility* OwningAbility,
		TObjectPtr<UAbilityTask_MeleeTraceWindow>& InOutTask,
		TSubclassOf<UGameplayEffect> DamageGameplayEffectClass,
		float AbilityLevel,
		const TMap<FGameplayTag, float>& SetByCallerMagnitudes,
		const TArray<FName>& TraceSourceNames);

	static void CloseAndClear(
		TObjectPtr<UAbilityTask_MeleeTraceWindow>& InOutTask,
		TWeakObjectPtr<const UAnimNotifyState_AttackTraceWindow>& InOutActiveNotifyState);
};
