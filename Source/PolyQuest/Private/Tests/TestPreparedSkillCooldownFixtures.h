// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"
#include "TestPreparedSkillCooldownFixtures.generated.h"

/**
 * Test-only gameplay effect providing a deterministic 4.0s duration cooldown
 * tagged with the project's canonical Cooldown.Skill.Whirlwind tag.
 */
UCLASS()
class UTestPreparedSkillCooldownGE : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UTestPreparedSkillCooldownGE();

	static constexpr float DefaultTestDuration = 4.0f;
};

/**
 * Test-only gameplay ability that binds UTestPreparedSkillCooldownGE and returns
 * Cooldown.Skill.Whirlwind tags for authoritative GAS cooldown queries.
 */
UCLASS()
class UTestPreparedSkillCooldownAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UTestPreparedSkillCooldownAbility();

	virtual const FGameplayTagContainer* GetCooldownTags() const override;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	void SetTestCooldownDuration(float InDuration);

	UPROPERTY(EditAnywhere, Category = "Test")
	bool bAutoEndAbility = true;

private:
	mutable FGameplayTagContainer CachedCooldownTags;
};
