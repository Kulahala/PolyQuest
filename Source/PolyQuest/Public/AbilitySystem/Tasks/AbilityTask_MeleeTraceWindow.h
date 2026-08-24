#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "GameplayTagContainer.h"
#include "AbilityTask_MeleeTraceWindow.generated.h"

class AActor;
class UGameplayAbility;
class UMeleeTraceSourceComponent;
class UMeleeWeaponTrailComponent;
class UGameplayEffect;

/**
 * An active-Ability-owned trace window. It keeps only prior blade samples and
 * targets successfully resolved during this one authored montage window.
 */
UCLASS()
class POLYQUEST_API UAbilityTask_MeleeTraceWindow : public UAbilityTask
{
	GENERATED_BODY()

public:
	UAbilityTask_MeleeTraceWindow();

	static UAbilityTask_MeleeTraceWindow* OpenMeleeTraceWindow(
		UGameplayAbility* OwningAbility,
		UMeleeTraceSourceComponent* InTraceSource,
		TSubclassOf<UGameplayEffect> InDamageGameplayEffectClass,
		float InAbilityLevel,
		FGameplayTag InSetByCallerMagnitudeTag,
		float InSetByCallerMagnitude,
		float InGuardStaminaDamage = 0.0f);

	static UAbilityTask_MeleeTraceWindow* OpenMeleeTraceWindow(
		UGameplayAbility* OwningAbility,
		UMeleeTraceSourceComponent* InTraceSource,
		TSubclassOf<UGameplayEffect> InDamageGameplayEffectClass,
		float InAbilityLevel,
		const TMap<FGameplayTag, float>& InSetByCallerMagnitudes);

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;
	virtual void OnDestroy(bool AbilityIsEnding) override;

	bool IsTraceWindowOpen() const { return bWindowOpen; }

private:
	bool CaptureCurrentBladeEndpoints(FVector& OutBladeBase, FVector& OutBladeTip) const;
	void TraceCurrentSegment();
	void ResetWindowState();

	UPROPERTY()
	TObjectPtr<UMeleeTraceSourceComponent> TraceSource;

	UPROPERTY()
	TSubclassOf<UGameplayEffect> DamageGameplayEffectClass;

	FGameplayTag SetByCallerMagnitudeTag;
	float AbilityLevel = 1.0f;
	float SetByCallerMagnitude = 0.0f;
	TMap<FGameplayTag, float> SetByCallerMagnitudes;
	float GuardStaminaDamage = 0.0f;
	FVector PreviousBladeBase = FVector::ZeroVector;
	FVector PreviousBladeTip = FVector::ZeroVector;
	TSet<TWeakObjectPtr<AActor>> DeliveredTargets;
	TWeakObjectPtr<UMeleeWeaponTrailComponent> CachedTrailComponent;
	bool bWindowOpen = false;
	bool bHasPreviousBladeSample = false;
};
