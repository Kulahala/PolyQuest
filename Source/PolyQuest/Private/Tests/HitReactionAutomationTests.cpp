#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystem/Abilities/EnemyHitReactionAbility.h"
#include "AbilitySystem/Abilities/EnemyLaunchReactionAbility.h"
#include "AbilitySystem/Abilities/EnemySmallHitReactionAbility.h"
#include "AbilitySystem/Abilities/EnemyStanceBreakAbility.h"
#include "AbilitySystem/Abilities/PlayerBigHitReactionAbility.h"
#include "AbilitySystem/Abilities/PlayerGuardBreakAbility.h"
#include "AbilitySystem/Abilities/PlayerLaunchReactionAbility.h"
#include "AbilitySystem/Abilities/PlayerSmallHitReactionAbility.h"
#include "AbilitySystem/CharacterAttributeSet.h"
#include "Animation/Combat/AnimNotify_ReactionLaunchCommit.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Character/Player/PlayerCharacter.h"
#include "Combat/Reaction/HitReactionClassifier.h"
#include "Combat/Reaction/HitReactionFourWayMontageSelector.h"
#include "Combat/Reaction/HitReactionImpactResolver.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "Tests/TestPoiseRecoveryGE.h"
#include "Tests/CombatAutomationFixture.h"
#include "Tests/TestProjectileDamageGE.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHitReactionAutomationTest, "PolyQuest.Combat.HitReaction", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHitReactionAutomationTest::RunTest(const FString& Parameters)
{
	// -------------------------------------------------------------------------
	// SECTION 1: Pure Classifier Logic (FHitReactionClassifier)
	// -------------------------------------------------------------------------
	const FGameplayTag TagDataSmall = FHitReactionClassifier::GetSmallReactionTag();
	const FGameplayTag TagDataBig = FHitReactionClassifier::GetBigReactionTag();
	const FGameplayTag TagDataLaunch = FHitReactionClassifier::GetLaunchReactionTag();

	TestTrue(TEXT("Tag Data.Reaction.Small is valid"), TagDataSmall.IsValid());
	TestTrue(TEXT("Tag Data.Reaction.Big is valid"), TagDataBig.IsValid());
	TestTrue(TEXT("Tag Data.Reaction.Launch is valid"), TagDataLaunch.IsValid());

	// 1.1 Empty tag container -> None
	{
		FGameplayTagContainer EmptyTags;
		TestEqual(TEXT("Empty tag container classifies as None"),
			FHitReactionClassifier::ClassifyReactionTier(EmptyTags), EHitReactionTier::None);
	}

	// 1.2 Unrelated tags only -> None
	{
		FGameplayTagContainer UnrelatedTags;
		UnrelatedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Light")), false));
		UnrelatedTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Attacking")), false));
		TestEqual(TEXT("Unrelated tags classify as None"),
			FHitReactionClassifier::ClassifyReactionTier(UnrelatedTags), EHitReactionTier::None);
	}

	// 1.3 Exact single reaction tags
	{
		FGameplayTagContainer SmallTags;
		SmallTags.AddTag(TagDataSmall);
		TestEqual(TEXT("Data.Reaction.Small classifies as Small"),
			FHitReactionClassifier::ClassifyReactionTier(SmallTags), EHitReactionTier::Small);

		FGameplayTagContainer BigTags;
		BigTags.AddTag(TagDataBig);
		TestEqual(TEXT("Data.Reaction.Big classifies as Big"),
			FHitReactionClassifier::ClassifyReactionTier(BigTags), EHitReactionTier::Big);

		FGameplayTagContainer LaunchTags;
		LaunchTags.AddTag(TagDataLaunch);
		TestEqual(TEXT("Data.Reaction.Launch classifies as Launch"),
			FHitReactionClassifier::ClassifyReactionTier(LaunchTags), EHitReactionTier::Launch);
	}

	// 1.4 Single reaction tag with unrelated tags
	{
		FGameplayTagContainer SmallWithOthers;
		SmallWithOthers.AddTag(TagDataSmall);
		SmallWithOthers.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Light")), false));
		TestEqual(TEXT("Small with unrelated tags classifies as Small"),
			FHitReactionClassifier::ClassifyReactionTier(SmallWithOthers), EHitReactionTier::Small);

		FGameplayTagContainer BigWithOthers;
		BigWithOthers.AddTag(TagDataBig);
		BigWithOthers.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Movement.Sprinting")), false));
		TestEqual(TEXT("Big with unrelated tags classifies as Big"),
			FHitReactionClassifier::ClassifyReactionTier(BigWithOthers), EHitReactionTier::Big);

		FGameplayTagContainer LaunchWithOthers;
		LaunchWithOthers.AddTag(TagDataLaunch);
		LaunchWithOthers.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Team.Player")), false));
		TestEqual(TEXT("Launch with unrelated tags classifies as Launch"),
			FHitReactionClassifier::ClassifyReactionTier(LaunchWithOthers), EHitReactionTier::Launch);
	}

	// 1.5 Multi-tier invalid combinations (Fail-Closed)
	{
		FGameplayTagContainer SmallAndBig;
		SmallAndBig.AddTag(TagDataSmall);
		SmallAndBig.AddTag(TagDataBig);
		TestEqual(TEXT("Small + Big classifies as Invalid"),
			FHitReactionClassifier::ClassifyReactionTier(SmallAndBig), EHitReactionTier::Invalid);

		FGameplayTagContainer SmallAndLaunch;
		SmallAndLaunch.AddTag(TagDataSmall);
		SmallAndLaunch.AddTag(TagDataLaunch);
		TestEqual(TEXT("Small + Launch classifies as Invalid"),
			FHitReactionClassifier::ClassifyReactionTier(SmallAndLaunch), EHitReactionTier::Invalid);

		FGameplayTagContainer BigAndLaunch;
		BigAndLaunch.AddTag(TagDataBig);
		BigAndLaunch.AddTag(TagDataLaunch);
		TestEqual(TEXT("Big + Launch classifies as Invalid"),
			FHitReactionClassifier::ClassifyReactionTier(BigAndLaunch), EHitReactionTier::Invalid);

		FGameplayTagContainer AllThree;
		AllThree.AddTag(TagDataSmall);
		AllThree.AddTag(TagDataBig);
		AllThree.AddTag(TagDataLaunch);
		TestEqual(TEXT("Small + Big + Launch classifies as Invalid"),
			FHitReactionClassifier::ClassifyReactionTier(AllThree), EHitReactionTier::Invalid);
	}

	// -------------------------------------------------------------------------
	// SECTION 2: CDO & Tag Contract Validation
	// -------------------------------------------------------------------------
	const FGameplayTag TagAbilityPlayerSmall = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Reaction.Player.Small")), false);
	const FGameplayTag TagEventPlayerSmall = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Player.Small")), false);
	const FGameplayTag TagAbilityPlayerBig = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Reaction.Player.Big")), false);
	const FGameplayTag TagEventPlayerBig = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Player.Big")), false);
	const FGameplayTag TagAbilityPlayerLaunch = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Reaction.Player.Launch")), false);
	const FGameplayTag TagEventPlayerLaunch = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Player.Launch")), false);
	const FGameplayTag TagAbilityEnemySmall = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Reaction.Enemy.Small")), false);
	const FGameplayTag TagEventEnemySmall = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Enemy.Small")), false);
	const FGameplayTag TagAbilityEnemyBig = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Reaction.Enemy.Big")), false);
	const FGameplayTag TagEventEnemyBig = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Enemy.Big")), false);
	const FGameplayTag TagAbilityEnemyLaunch = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Reaction.Enemy.Launch")), false);
	const FGameplayTag TagEventEnemyLaunch = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Enemy.Launch")), false);
	const FGameplayTag TagEventLaunchCommit = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Launch.Commit")), false);
	const FGameplayTag TagSmallHitReacting = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.SmallHitReacting")), false);
	const FGameplayTag TagHitReacting = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.HitReacting")), false);
	const FGameplayTag TagDead = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
	const FGameplayTag TagStunned = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
	const FGameplayTag TagHyperArmor = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.HyperArmor")), false);
	const FGameplayTag TagBlockMovement = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Movement")), false);
	const FGameplayTag TagBlockJump = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Jump")), false);
	const FGameplayTag TagEnemyMelee = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Enemy.Melee")), false);

	TestTrue(TEXT("Tag Ability.Reaction.Player.Small is valid"), TagAbilityPlayerSmall.IsValid());
	TestTrue(TEXT("Tag Event.Reaction.Player.Small is valid"), TagEventPlayerSmall.IsValid());
	TestTrue(TEXT("Tag Ability.Reaction.Player.Big is valid"), TagAbilityPlayerBig.IsValid());
	TestTrue(TEXT("Tag Event.Reaction.Player.Big is valid"), TagEventPlayerBig.IsValid());
	TestTrue(TEXT("Tag Ability.Reaction.Player.Launch is valid"), TagAbilityPlayerLaunch.IsValid());
	TestTrue(TEXT("Tag Event.Reaction.Player.Launch is valid"), TagEventPlayerLaunch.IsValid());
	TestTrue(TEXT("Tag Ability.Reaction.Enemy.Small is valid"), TagAbilityEnemySmall.IsValid());
	TestTrue(TEXT("Tag Event.Reaction.Enemy.Small is valid"), TagEventEnemySmall.IsValid());
	TestTrue(TEXT("Tag Ability.Reaction.Enemy.Big is valid"), TagAbilityEnemyBig.IsValid());
	TestTrue(TEXT("Tag Event.Reaction.Enemy.Big is valid"), TagEventEnemyBig.IsValid());
	TestTrue(TEXT("Tag Ability.Reaction.Enemy.Launch is valid"), TagAbilityEnemyLaunch.IsValid());
	TestTrue(TEXT("Tag Event.Reaction.Enemy.Launch is valid"), TagEventEnemyLaunch.IsValid());
	TestTrue(TEXT("Tag Event.Reaction.Launch.Commit is valid"), TagEventLaunchCommit.IsValid());
	TestTrue(TEXT("Tag State.Action.SmallHitReacting is valid"), TagSmallHitReacting.IsValid());
	TestTrue(TEXT("Tag State.Action.HitReacting is valid"), TagHitReacting.IsValid());

	// Hierarchy independence check: SmallHitReacting must NOT be a child of HitReacting
	TestFalse(TEXT("State.Action.SmallHitReacting is not a child of State.Action.HitReacting"),
		TagSmallHitReacting.MatchesTag(TagHitReacting));

	// 2.1 UPlayerSmallHitReactionAbility CDO checks
	{
		const UPlayerSmallHitReactionAbility* PlayerSmallCDO = UPlayerSmallHitReactionAbility::StaticClass()->GetDefaultObject<UPlayerSmallHitReactionAbility>();
		TestNotNull(TEXT("UPlayerSmallHitReactionAbility CDO exists"), PlayerSmallCDO);
		if (PlayerSmallCDO)
		{
			TestEqual(TEXT("PlayerSmall instancing is InstancedPerActor"),
				PlayerSmallCDO->GetInstancingPolicy(), EGameplayAbilityInstancingPolicy::InstancedPerActor);
			TestEqual(TEXT("PlayerSmall net execution is ServerOnly"),
				PlayerSmallCDO->GetNetExecutionPolicy(), EGameplayAbilityNetExecutionPolicy::ServerOnly);

			TestTrue(TEXT("PlayerSmall CDO has AbilityTags Ability.Reaction.Player.Small"),
				PlayerSmallCDO->AbilityTags.HasTagExact(TagAbilityPlayerSmall));
			TestTrue(TEXT("PlayerSmall CDO owns State.Action.SmallHitReacting"),
				PlayerSmallCDO->GetTestActivationOwnedTags().HasTagExact(TagSmallHitReacting));
			TestFalse(TEXT("PlayerSmall CDO does NOT own State.Action.HitReacting"),
				PlayerSmallCDO->GetTestActivationOwnedTags().HasTagExact(TagHitReacting));

			TestTrue(TEXT("PlayerSmall CDO blocked by State.Status.Dead"),
				PlayerSmallCDO->GetTestActivationBlockedTags().HasTagExact(TagDead));
			TestTrue(TEXT("PlayerSmall CDO blocked by State.Status.Stunned"),
				PlayerSmallCDO->GetTestActivationBlockedTags().HasTagExact(TagStunned));
			TestTrue(TEXT("PlayerSmall CDO blocked by State.Action.SmallHitReacting"),
				PlayerSmallCDO->GetTestActivationBlockedTags().HasTagExact(TagSmallHitReacting));

			TestFalse(TEXT("PlayerSmall CDO does NOT block movement"),
				PlayerSmallCDO->GetTestActivationBlockedTags().HasTagExact(TagBlockMovement));
			TestFalse(TEXT("PlayerSmall CDO does NOT block jump"),
				PlayerSmallCDO->GetTestActivationBlockedTags().HasTagExact(TagBlockJump));
			TestFalse(TEXT("PlayerSmall CDO does NOT block on State.Action.HitReacting"),
				PlayerSmallCDO->GetTestActivationBlockedTags().HasTagExact(TagHitReacting));

			TestTrue(TEXT("PlayerSmall CDO triggers on Event.Reaction.Player.Small"),
				PlayerSmallCDO->GetTestAbilityTriggers().ContainsByPredicate([&TagEventPlayerSmall](const FAbilityTriggerData& Trigger)
				{
					return Trigger.TriggerTag == TagEventPlayerSmall
						&& Trigger.TriggerSource == EGameplayAbilityTriggerSource::GameplayEvent;
				}));
		}
	}

	const TArray<FName> ExpectedTargetActionTagNames = {
		TEXT("Ability.Attack.Primary"),
		TEXT("Ability.Attack.Light"),
		TEXT("Ability.Attack.Charged"),
		TEXT("Ability.Attack.Sprint"),
		TEXT("Ability.Skill.Melee"),
		TEXT("Ability.Dodge"),
		TEXT("Ability.Movement.Sprint"),
		TEXT("Ability.Movement.Jump"),
		TEXT("Ability.Defense.Guard"),
		TEXT("Ability.Defense.Parry"),
		TEXT("Ability.Reaction.Player.Small")
	};

	// 2.2 UPlayerBigHitReactionAbility CDO checks
	{
		const UPlayerBigHitReactionAbility* PlayerBigCDO = UPlayerBigHitReactionAbility::StaticClass()->GetDefaultObject<UPlayerBigHitReactionAbility>();
		TestNotNull(TEXT("UPlayerBigHitReactionAbility CDO exists"), PlayerBigCDO);
		if (PlayerBigCDO)
		{
			TestEqual(TEXT("PlayerBig instancing is InstancedPerActor"),
				PlayerBigCDO->GetInstancingPolicy(), EGameplayAbilityInstancingPolicy::InstancedPerActor);
			TestEqual(TEXT("PlayerBig net execution is ServerOnly"),
				PlayerBigCDO->GetNetExecutionPolicy(), EGameplayAbilityNetExecutionPolicy::ServerOnly);

			TestTrue(TEXT("PlayerBig CDO has AbilityTags Ability.Reaction.Player.Big"),
				PlayerBigCDO->AbilityTags.HasTagExact(TagAbilityPlayerBig));
			TestTrue(TEXT("PlayerBig CDO owns State.Action.HitReacting"),
				PlayerBigCDO->GetTestActivationOwnedTags().HasTagExact(TagHitReacting));
			TestTrue(TEXT("PlayerBig CDO owns State.Input.Block.Movement"),
				PlayerBigCDO->GetTestActivationOwnedTags().HasTagExact(TagBlockMovement));
			TestTrue(TEXT("PlayerBig CDO owns State.Input.Block.Jump"),
				PlayerBigCDO->GetTestActivationOwnedTags().HasTagExact(TagBlockJump));

			TestTrue(TEXT("PlayerBig CDO blocked by State.Status.Dead"),
				PlayerBigCDO->GetTestActivationBlockedTags().HasTagExact(TagDead));
			TestTrue(TEXT("PlayerBig CDO blocked by State.Status.Stunned"),
				PlayerBigCDO->GetTestActivationBlockedTags().HasTagExact(TagStunned));
			TestTrue(TEXT("PlayerBig CDO blocked by State.Status.HyperArmor"),
				PlayerBigCDO->GetTestActivationBlockedTags().HasTagExact(TagHyperArmor));
			TestTrue(TEXT("PlayerBig CDO blocked by State.Action.HitReacting"),
				PlayerBigCDO->GetTestActivationBlockedTags().HasTagExact(TagHitReacting));

			TestTrue(TEXT("PlayerBig CDO triggers on Event.Reaction.Player.Big"),
				PlayerBigCDO->GetTestAbilityTriggers().ContainsByPredicate([&TagEventPlayerBig](const FAbilityTriggerData& Trigger)
				{
					return Trigger.TriggerTag == TagEventPlayerBig
						&& Trigger.TriggerSource == EGameplayAbilityTriggerSource::GameplayEvent;
				}));

			// BlockAbilitiesWithTag and AbilitiesToCancel must match exactly 11 tags

			TestEqual(TEXT("PlayerBig BlockAbilitiesWithTag has exactly 11 tags"),
				PlayerBigCDO->GetTestBlockAbilitiesWithTag().Num(), 11);
			TestEqual(TEXT("PlayerBig AbilitiesToCancel has exactly 11 tags"),
				PlayerBigCDO->GetTestAbilitiesToCancel().Num(), 11);

			for (const FName& ActionTagName : ExpectedTargetActionTagNames)
			{
				const FGameplayTag ActionTag = FGameplayTag::RequestGameplayTag(ActionTagName, false);
				TestTrue(FString::Printf(TEXT("Tag '%s' is valid"), *ActionTagName.ToString()), ActionTag.IsValid());
				TestTrue(FString::Printf(TEXT("PlayerBig BlockAbilitiesWithTag contains '%s'"), *ActionTagName.ToString()),
					PlayerBigCDO->GetTestBlockAbilitiesWithTag().HasTagExact(ActionTag));
				TestTrue(FString::Printf(TEXT("PlayerBig AbilitiesToCancel contains '%s'"), *ActionTagName.ToString()),
					PlayerBigCDO->GetTestAbilitiesToCancel().HasTagExact(ActionTag));
			}
		}
	}

	// 2.2b UPlayerLaunchReactionAbility CDO checks
	{
		const UPlayerLaunchReactionAbility* PlayerLaunchCDO = UPlayerLaunchReactionAbility::StaticClass()->GetDefaultObject<UPlayerLaunchReactionAbility>();
		TestNotNull(TEXT("UPlayerLaunchReactionAbility CDO exists"), PlayerLaunchCDO);
		if (PlayerLaunchCDO)
		{
			TestEqual(TEXT("PlayerLaunch instancing is InstancedPerActor"),
				PlayerLaunchCDO->GetInstancingPolicy(), EGameplayAbilityInstancingPolicy::InstancedPerActor);
			TestEqual(TEXT("PlayerLaunch net execution is ServerOnly"),
				PlayerLaunchCDO->GetNetExecutionPolicy(), EGameplayAbilityNetExecutionPolicy::ServerOnly);

			TestTrue(TEXT("PlayerLaunch CDO has AbilityTags Ability.Reaction.Player.Launch"),
				PlayerLaunchCDO->AbilityTags.HasTagExact(TagAbilityPlayerLaunch));
			TestTrue(TEXT("PlayerLaunch CDO owns State.Action.HitReacting"),
				PlayerLaunchCDO->GetTestActivationOwnedTags().HasTagExact(TagHitReacting));
			TestTrue(TEXT("PlayerLaunch CDO owns State.Input.Block.Movement"),
				PlayerLaunchCDO->GetTestActivationOwnedTags().HasTagExact(TagBlockMovement));
			TestTrue(TEXT("PlayerLaunch CDO owns State.Input.Block.Jump"),
				PlayerLaunchCDO->GetTestActivationOwnedTags().HasTagExact(TagBlockJump));

			TestTrue(TEXT("PlayerLaunch CDO blocked by State.Status.Dead"),
				PlayerLaunchCDO->GetTestActivationBlockedTags().HasTagExact(TagDead));
			TestTrue(TEXT("PlayerLaunch CDO blocked by State.Status.Stunned"),
				PlayerLaunchCDO->GetTestActivationBlockedTags().HasTagExact(TagStunned));
			TestTrue(TEXT("PlayerLaunch CDO blocked by State.Status.HyperArmor"),
				PlayerLaunchCDO->GetTestActivationBlockedTags().HasTagExact(TagHyperArmor));
			TestTrue(TEXT("PlayerLaunch CDO blocked by State.Action.HitReacting"),
				PlayerLaunchCDO->GetTestActivationBlockedTags().HasTagExact(TagHitReacting));

			TestTrue(TEXT("PlayerLaunch CDO triggers on Event.Reaction.Player.Launch"),
				PlayerLaunchCDO->GetTestAbilityTriggers().ContainsByPredicate([&TagEventPlayerLaunch](const FAbilityTriggerData& Trigger)
				{
					return Trigger.TriggerTag == TagEventPlayerLaunch
						&& Trigger.TriggerSource == EGameplayAbilityTriggerSource::GameplayEvent;
				}));

			const FGameplayTag TagAbilityDodge = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Dodge")), false);
			TestTrue(TEXT("Tag Ability.Dodge is valid"), TagAbilityDodge.IsValid());

			TestEqual(TEXT("PlayerLaunch BlockAbilitiesWithTag has exactly 10 tags"),
				PlayerLaunchCDO->GetTestBlockAbilitiesWithTag().Num(), 10);
			TestEqual(TEXT("PlayerLaunch AbilitiesToCancel has exactly 11 tags"),
				PlayerLaunchCDO->GetTestAbilitiesToCancel().Num(), 11);

			TestFalse(TEXT("PlayerLaunch BlockAbilitiesWithTag does NOT contain Ability.Dodge"),
				PlayerLaunchCDO->GetTestBlockAbilitiesWithTag().HasTagExact(TagAbilityDodge));
			TestTrue(TEXT("PlayerLaunch AbilitiesToCancel DOES contain Ability.Dodge"),
				PlayerLaunchCDO->GetTestAbilitiesToCancel().HasTagExact(TagAbilityDodge));

			for (const FName& ActionTagName : ExpectedTargetActionTagNames)
			{
				const FGameplayTag ActionTag = FGameplayTag::RequestGameplayTag(ActionTagName, false);
				if (ActionTagName != TEXT("Ability.Dodge"))
				{
					TestTrue(FString::Printf(TEXT("PlayerLaunch BlockAbilitiesWithTag contains '%s'"), *ActionTagName.ToString()),
						PlayerLaunchCDO->GetTestBlockAbilitiesWithTag().HasTagExact(ActionTag));
				}
				TestTrue(FString::Printf(TEXT("PlayerLaunch AbilitiesToCancel contains '%s'"), *ActionTagName.ToString()),
					PlayerLaunchCDO->GetTestAbilitiesToCancel().HasTagExact(ActionTag));
			}

			TestEqual(TEXT("PlayerLaunch default horizontal speed is 450.0"), PlayerLaunchCDO->GetLaunchHorizontalSpeed(), 450.0f);
			TestEqual(TEXT("PlayerLaunch default vertical speed is 550.0"), PlayerLaunchCDO->GetLaunchVerticalSpeed(), 550.0f);
			TestEqual(TEXT("PlayerLaunch default facing turn rate is 1440.0"), PlayerLaunchCDO->GetFacingTurnRateDegreesPerSecond(), 1440.0f);
			TestTrue(TEXT("PlayerLaunch default facing turn rate is finite positive"), FMath::IsFinite(PlayerLaunchCDO->GetFacingTurnRateDegreesPerSecond()) && PlayerLaunchCDO->GetFacingTurnRateDegreesPerSecond() > 0.0f);
		}
	}

	// 2.3 UEnemySmallHitReactionAbility CDO checks
	{
		const UEnemySmallHitReactionAbility* EnemySmallCDO = UEnemySmallHitReactionAbility::StaticClass()->GetDefaultObject<UEnemySmallHitReactionAbility>();
		TestNotNull(TEXT("UEnemySmallHitReactionAbility CDO exists"), EnemySmallCDO);
		if (EnemySmallCDO)
		{
			TestEqual(TEXT("EnemySmall instancing is InstancedPerActor"),
				EnemySmallCDO->GetInstancingPolicy(), EGameplayAbilityInstancingPolicy::InstancedPerActor);
			TestEqual(TEXT("EnemySmall net execution is ServerOnly"),
				EnemySmallCDO->GetNetExecutionPolicy(), EGameplayAbilityNetExecutionPolicy::ServerOnly);

			TestTrue(TEXT("EnemySmall CDO has AbilityTags Ability.Reaction.Enemy.Small"),
				EnemySmallCDO->AbilityTags.HasTagExact(TagAbilityEnemySmall));
			TestTrue(TEXT("EnemySmall CDO owns State.Action.SmallHitReacting"),
				EnemySmallCDO->GetTestActivationOwnedTags().HasTagExact(TagSmallHitReacting));
			TestFalse(TEXT("EnemySmall CDO does NOT own State.Action.HitReacting"),
				EnemySmallCDO->GetTestActivationOwnedTags().HasTagExact(TagHitReacting));

			TestTrue(TEXT("EnemySmall CDO blocked by State.Status.Dead"),
				EnemySmallCDO->GetTestActivationBlockedTags().HasTagExact(TagDead));
			TestTrue(TEXT("EnemySmall CDO blocked by State.Status.Stunned"),
				EnemySmallCDO->GetTestActivationBlockedTags().HasTagExact(TagStunned));
			TestTrue(TEXT("EnemySmall CDO blocked by State.Action.SmallHitReacting"),
				EnemySmallCDO->GetTestActivationBlockedTags().HasTagExact(TagSmallHitReacting));

			TestFalse(TEXT("EnemySmall CDO does NOT block movement"),
				EnemySmallCDO->GetTestActivationBlockedTags().HasTagExact(TagBlockMovement));
			TestFalse(TEXT("EnemySmall CDO does NOT block jump"),
				EnemySmallCDO->GetTestActivationBlockedTags().HasTagExact(TagBlockJump));
			TestFalse(TEXT("EnemySmall CDO does NOT block on State.Action.HitReacting"),
				EnemySmallCDO->GetTestActivationBlockedTags().HasTagExact(TagHitReacting));

			TestTrue(TEXT("EnemySmall CDO triggers on Event.Reaction.Enemy.Small"),
				EnemySmallCDO->GetTestAbilityTriggers().ContainsByPredicate([&TagEventEnemySmall](const FAbilityTriggerData& Trigger)
				{
					return Trigger.TriggerTag == TagEventEnemySmall
						&& Trigger.TriggerSource == EGameplayAbilityTriggerSource::GameplayEvent;
				}));
		}
	}

	// 2.4 UEnemyHitReactionAbility CDO checks (Enemy Big)
	{
		const UEnemyHitReactionAbility* EnemyBigCDO = UEnemyHitReactionAbility::StaticClass()->GetDefaultObject<UEnemyHitReactionAbility>();
		TestNotNull(TEXT("UEnemyHitReactionAbility CDO exists"), EnemyBigCDO);
		if (EnemyBigCDO)
		{
			TestTrue(TEXT("EnemyBig CDO has AbilityTags Ability.Reaction.Enemy.Big"),
				EnemyBigCDO->AbilityTags.HasTagExact(TagAbilityEnemyBig));
			TestTrue(TEXT("EnemyBig CDO owns State.Action.HitReacting"),
				EnemyBigCDO->GetTestActivationOwnedTags().HasTagExact(TagHitReacting));

			TestTrue(TEXT("EnemyBig CDO blocked by State.Status.Dead"),
				EnemyBigCDO->GetTestActivationBlockedTags().HasTagExact(TagDead));
			TestTrue(TEXT("EnemyBig CDO blocked by State.Status.Stunned"),
				EnemyBigCDO->GetTestActivationBlockedTags().HasTagExact(TagStunned));
			TestTrue(TEXT("EnemyBig CDO blocked by State.Action.HitReacting"),
				EnemyBigCDO->GetTestActivationBlockedTags().HasTagExact(TagHitReacting));
			TestTrue(TEXT("EnemyBig CDO blocked by State.Status.HyperArmor"),
				EnemyBigCDO->GetTestActivationBlockedTags().HasTagExact(TagHyperArmor));

			TestTrue(TEXT("EnemyBig CDO triggers on Event.Reaction.Enemy.Big"),
				EnemyBigCDO->GetTestAbilityTriggers().ContainsByPredicate([&TagEventEnemyBig](const FAbilityTriggerData& Trigger)
				{
					return Trigger.TriggerTag == TagEventEnemyBig
						&& Trigger.TriggerSource == EGameplayAbilityTriggerSource::GameplayEvent;
				}));

			TestTrue(TEXT("EnemyBig CDO cancels EnemyMelee"),
				EnemyBigCDO->GetTestAbilitiesToCancel().HasTagExact(TagEnemyMelee));
			TestTrue(TEXT("EnemyBig CDO cancels EnemySmall"),
				EnemyBigCDO->GetTestAbilitiesToCancel().HasTagExact(TagAbilityEnemySmall));
			TestEqual(TEXT("EnemyBig CDO AbilitiesToCancel has exactly 2 tags"),
				EnemyBigCDO->GetTestAbilitiesToCancel().Num(), 2);
		}
	}

	// 2.4b UEnemyLaunchReactionAbility CDO checks
	{
		const UEnemyLaunchReactionAbility* EnemyLaunchCDO = UEnemyLaunchReactionAbility::StaticClass()->GetDefaultObject<UEnemyLaunchReactionAbility>();
		TestNotNull(TEXT("UEnemyLaunchReactionAbility CDO exists"), EnemyLaunchCDO);
		if (EnemyLaunchCDO)
		{
			TestEqual(TEXT("EnemyLaunch instancing is InstancedPerActor"),
				EnemyLaunchCDO->GetInstancingPolicy(), EGameplayAbilityInstancingPolicy::InstancedPerActor);
			TestEqual(TEXT("EnemyLaunch net execution is ServerOnly"),
				EnemyLaunchCDO->GetNetExecutionPolicy(), EGameplayAbilityNetExecutionPolicy::ServerOnly);

			TestTrue(TEXT("EnemyLaunch CDO has AbilityTags Ability.Reaction.Enemy.Launch"),
				EnemyLaunchCDO->AbilityTags.HasTagExact(TagAbilityEnemyLaunch));
			TestTrue(TEXT("EnemyLaunch CDO owns State.Action.HitReacting"),
				EnemyLaunchCDO->GetTestActivationOwnedTags().HasTagExact(TagHitReacting));

			TestTrue(TEXT("EnemyLaunch CDO blocked by State.Status.Dead"),
				EnemyLaunchCDO->GetTestActivationBlockedTags().HasTagExact(TagDead));
			TestTrue(TEXT("EnemyLaunch CDO blocked by State.Status.Stunned"),
				EnemyLaunchCDO->GetTestActivationBlockedTags().HasTagExact(TagStunned));
			TestTrue(TEXT("EnemyLaunch CDO blocked by State.Status.HyperArmor"),
				EnemyLaunchCDO->GetTestActivationBlockedTags().HasTagExact(TagHyperArmor));
			TestTrue(TEXT("EnemyLaunch CDO blocked by State.Action.HitReacting"),
				EnemyLaunchCDO->GetTestActivationBlockedTags().HasTagExact(TagHitReacting));

			TestTrue(TEXT("EnemyLaunch CDO triggers on Event.Reaction.Enemy.Launch"),
				EnemyLaunchCDO->GetTestAbilityTriggers().ContainsByPredicate([&TagEventEnemyLaunch](const FAbilityTriggerData& Trigger)
				{
					return Trigger.TriggerTag == TagEventEnemyLaunch
						&& Trigger.TriggerSource == EGameplayAbilityTriggerSource::GameplayEvent;
				}));

			TestTrue(TEXT("EnemyLaunch CDO cancels EnemyMelee"),
				EnemyLaunchCDO->GetTestAbilitiesToCancel().HasTagExact(TagEnemyMelee));
			TestTrue(TEXT("EnemyLaunch CDO cancels EnemySmall"),
				EnemyLaunchCDO->GetTestAbilitiesToCancel().HasTagExact(TagAbilityEnemySmall));
			TestEqual(TEXT("EnemyLaunch CDO AbilitiesToCancel has exactly 2 tags"),
				EnemyLaunchCDO->GetTestAbilitiesToCancel().Num(), 2);

			TestEqual(TEXT("EnemyLaunch default horizontal speed is 450.0"), EnemyLaunchCDO->GetLaunchHorizontalSpeed(), 450.0f);
			TestEqual(TEXT("EnemyLaunch default vertical speed is 550.0"), EnemyLaunchCDO->GetLaunchVerticalSpeed(), 550.0f);
			TestEqual(TEXT("EnemyLaunch default facing turn rate is 1440.0"), EnemyLaunchCDO->GetFacingTurnRateDegreesPerSecond(), 1440.0f);
			TestTrue(TEXT("EnemyLaunch default facing turn rate is finite positive"), FMath::IsFinite(EnemyLaunchCDO->GetFacingTurnRateDegreesPerSecond()) && EnemyLaunchCDO->GetFacingTurnRateDegreesPerSecond() > 0.0f);
		}
	}

	// 2.5 Cancellation matrix on Stance Break & Guard Break
	{
		const UEnemyStanceBreakAbility* StanceBreakCDO = UEnemyStanceBreakAbility::StaticClass()->GetDefaultObject<UEnemyStanceBreakAbility>();
		TestNotNull(TEXT("UEnemyStanceBreakAbility CDO exists"), StanceBreakCDO);
		if (StanceBreakCDO)
		{
			TestTrue(TEXT("StanceBreak cancels EnemyMelee"),
				StanceBreakCDO->GetAbilitiesToCancel().HasTagExact(TagEnemyMelee));
			TestTrue(TEXT("StanceBreak cancels EnemyBig (Ability.Reaction.Enemy.Big)"),
				StanceBreakCDO->GetAbilitiesToCancel().HasTagExact(TagAbilityEnemyBig));
			TestTrue(TEXT("StanceBreak cancels EnemySmall (Ability.Reaction.Enemy.Small)"),
				StanceBreakCDO->GetAbilitiesToCancel().HasTagExact(TagAbilityEnemySmall));
			TestTrue(TEXT("StanceBreak cancels EnemyLaunch (Ability.Reaction.Enemy.Launch)"),
				StanceBreakCDO->GetAbilitiesToCancel().HasTagExact(TagAbilityEnemyLaunch));
			TestEqual(TEXT("StanceBreak AbilitiesToCancel has exactly 4 tags"),
				StanceBreakCDO->GetAbilitiesToCancel().Num(), 4);
		}

		const UPlayerGuardBreakAbility* GuardBreakCDO = UPlayerGuardBreakAbility::StaticClass()->GetDefaultObject<UPlayerGuardBreakAbility>();
		TestNotNull(TEXT("UPlayerGuardBreakAbility CDO exists"), GuardBreakCDO);
		if (GuardBreakCDO)
		{
			TestTrue(TEXT("GuardBreak cancels PlayerSmall (Ability.Reaction.Player.Small)"),
				GuardBreakCDO->GetAbilitiesToCancel().HasTagExact(TagAbilityPlayerSmall));
			TestTrue(TEXT("GuardBreak cancels PlayerBig (Ability.Reaction.Player.Big)"),
				GuardBreakCDO->GetAbilitiesToCancel().HasTagExact(TagAbilityPlayerBig));
			TestTrue(TEXT("GuardBreak cancels PlayerLaunch (Ability.Reaction.Player.Launch)"),
				GuardBreakCDO->GetAbilitiesToCancel().HasTagExact(TagAbilityPlayerLaunch));
			TestTrue(TEXT("GuardBreak cancels Guard (Ability.Defense.Guard)"),
				GuardBreakCDO->GetAbilitiesToCancel().HasTagExact(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Defense.Guard")), false)));
			TestEqual(TEXT("GuardBreak AbilitiesToCancel has exactly 10 tags"),
				GuardBreakCDO->GetAbilitiesToCancel().Num(), 10);
		}
	}

	// 2.6 UAnimNotify_ReactionLaunchCommit CDO check
	{
		const UAnimNotify_ReactionLaunchCommit* CommitNotifyCDO = UAnimNotify_ReactionLaunchCommit::StaticClass()->GetDefaultObject<UAnimNotify_ReactionLaunchCommit>();
		TestNotNull(TEXT("UAnimNotify_ReactionLaunchCommit CDO exists"), CommitNotifyCDO);
		if (CommitNotifyCDO)
		{
			TestEqual(TEXT("Commit Notify Name is 'Reaction Launch Commit'"),
				CommitNotifyCDO->GetNotifyName(), FString(TEXT("Reaction Launch Commit")));
		}
	}

	// -------------------------------------------------------------------------
	// SECTION 3: World Setup for Impact Resolver & Health Dispatch Tests
	// -------------------------------------------------------------------------
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("HitReactionTestWorld"));
	WorldContext.SetCurrentWorld(World);
	if (!TestNotNull(TEXT("Test World created successfully"), World))
	{
		return false;
	}

	FURL WorldURL;
	World->InitializeActorsForPlay(WorldURL);

	struct FTestScopeCleanup
	{
		UWorld* WorldToDestroy;
		~FTestScopeCleanup()
		{
			if (WorldToDestroy)
			{
				GEngine->DestroyWorldContext(WorldToDestroy);
				WorldToDestroy->DestroyWorld(false);
			}
		}
	} ScopeCleanup{ World };

	APlayerCharacter* Player = FCombatAutomationFixture::SpawnPlayer(World, FTransform(FRotator::ZeroRotator, FVector::ZeroVector));
	TestNotNull(TEXT("Player spawned successfully"), Player);

	AEnemyCharacter* Enemy = FCombatAutomationFixture::SpawnPassiveEnemy(World, FTransform(FRotator::ZeroRotator, FVector(100.0f, 0.0f, 0.0f)));
	TestNotNull(TEXT("Enemy spawned successfully"), Enemy);

	if (!Player || !Enemy)
	{
		return false;
	}

	// -------------------------------------------------------------------------
	// SECTION 4: Impact Resolver Logic (FHitReactionImpactResolver)
	// -------------------------------------------------------------------------
	{
		Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
		Player->SetActorRotation(FRotator(0.0f, 0.0f, 0.0f));

		// 4.1 Instigator priority over ImpactNormal: Instigator on right (+Y) with conflicting ImpactNormal in front (+X) -> strictly resolves to Right (+Y)
		Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
		Player->SetActorRotation(FRotator(0.0f, 0.0f, 0.0f));
		Enemy->SetActorLocation(FVector(0.0f, 100.0f, 0.0f)); // Instigator position (Right +Y)

		FHitResult FrontHit;
		FrontHit.ImpactNormal = FVector(1.0f, 0.0f, 0.0f); // Conflicting ImpactNormal in Front (+X)
		FGameplayEffectContext* ContextFront = new FGameplayEffectContext();
		ContextFront->AddHitResult(FrontHit, true);
		FGameplayEffectContextHandle ContextFrontHandle(ContextFront);

		const FVector DirInstigatorPriority = FHitReactionImpactResolver::ResolveImpactDirectionFromContext(ContextFrontHandle, Enemy, Player);
		TestTrue(TEXT("Valid Instigator planar offset (+Y) strictly overrides conflicting ImpactNormal (+X) to resolve Target-local Right (+Y)"),
			DirInstigatorPriority.Equals(FVector(0.0f, 1.0f, 0.0f), 0.001f));

		// 4.1b Fallback to ImpactNormal when Instigator is null
		const FVector DirImpactNormalFallbackNull = FHitReactionImpactResolver::ResolveImpactDirectionFromContext(ContextFrontHandle, nullptr, Player);
		TestTrue(TEXT("ImpactNormal (+X) is used as fallback when Instigator is null"),
			DirImpactNormalFallbackNull.Equals(FVector(1.0f, 0.0f, 0.0f), 0.001f));

		// 4.1c Fallback to ImpactNormal when Instigator is coincident with Target
		Enemy->SetActorLocation(FVector(0.0f, 0.0f, 0.0f)); // Coincident with Player
		const FVector DirImpactNormalFallbackCoincident = FHitReactionImpactResolver::ResolveImpactDirectionFromContext(ContextFrontHandle, Enemy, Player);
		TestTrue(TEXT("ImpactNormal (+X) is used as fallback when Instigator is coincident with Target"),
			DirImpactNormalFallbackCoincident.Equals(FVector(1.0f, 0.0f, 0.0f), 0.001f));

		// 4.2 Target rotated Yaw = 90 deg, HitNormal = (0, 1, 0) (world +Y -> in front of rotated player)
		Player->SetActorRotation(FRotator(0.0f, 90.0f, 0.0f));
		FHitResult RotatedFrontHit;
		RotatedFrontHit.ImpactNormal = FVector(0.0f, 1.0f, 0.0f);
		FGameplayEffectContext* ContextRotated = new FGameplayEffectContext();
		ContextRotated->AddHitResult(RotatedFrontHit, true);
		FGameplayEffectContextHandle ContextRotatedHandle(ContextRotated);

		const FVector DirRotated = FHitReactionImpactResolver::ResolveImpactDirectionFromContext(ContextRotatedHandle, nullptr, Player);
		TestTrue(TEXT("ImpactNormal (+Y) with Target Yaw=90 resolves to Target-local Front (+X)"),
			DirRotated.Equals(FVector(1.0f, 0.0f, 0.0f), 0.001f));

		// 4.3 Slanted ImpactNormal with Z component projects to planar XY
		Player->SetActorRotation(FRotator(0.0f, 0.0f, 0.0f));
		FHitResult SlantedHit;
		SlantedHit.ImpactNormal = FVector(10.0f, 0.0f, 50.0f);
		FGameplayEffectContext* ContextSlanted = new FGameplayEffectContext();
		ContextSlanted->AddHitResult(SlantedHit, true);
		FGameplayEffectContextHandle ContextSlantedHandle(ContextSlanted);

		const FVector DirSlanted = FHitReactionImpactResolver::ResolveImpactDirectionFromContext(ContextSlantedHandle, nullptr, Player);
		TestTrue(TEXT("Slanted ImpactNormal projects to planar (+X)"),
			DirSlanted.Equals(FVector(1.0f, 0.0f, 0.0f), 0.001f));

		// 4.4 InstigatorLocation - TargetLocation (Front +X)
		Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
		Player->SetActorRotation(FRotator(0.0f, 0.0f, 0.0f));
		Enemy->SetActorLocation(FVector(100.0f, 0.0f, 0.0f)); // Enemy in front of Player

		FGameplayEffectContextHandle EmptyContext;
		const FVector DirInstigatorFront = FHitReactionImpactResolver::ResolveImpactDirectionFromContext(EmptyContext, Enemy, Player);
		TestTrue(TEXT("Instigator-Target in front resolves to Target-local Front (+X)"),
			DirInstigatorFront.Equals(FVector(1.0f, 0.0f, 0.0f), 0.001f));

		// 4.5 Instigator with Enemy on Player's right (+Y)
		Enemy->SetActorLocation(FVector(0.0f, 100.0f, 0.0f));
		const FVector DirInstigatorRight = FHitReactionImpactResolver::ResolveImpactDirectionFromContext(EmptyContext, Enemy, Player);
		TestTrue(TEXT("Instigator-Target on right resolves to Target-local Right (+Y)"),
			DirInstigatorRight.Equals(FVector(0.0f, 1.0f, 0.0f), 0.001f));

		// 4.6 Direction consistency: ImpactNormal and Instigator produce identical Target -> Attacker direction
		TestTrue(TEXT("ImpactNormal and Instigator produce identical forward direction"),
			DirImpactNormalFallbackNull.Equals(DirInstigatorFront, 0.001f));

		// 4.7 Fail-Closed on NaN / Inf / Zero length / Null Actor / Null Context
		FHitResult NanHit;
		NanHit.ImpactNormal = FVector(NAN, 0.0f, 0.0f);
		FGameplayEffectContext* ContextNan = new FGameplayEffectContext();
		ContextNan->AddHitResult(NanHit, true);
		FGameplayEffectContextHandle ContextNanHandle(ContextNan);
		TestTrue(TEXT("NaN ImpactNormal returns ZeroVector"),
			FHitReactionImpactResolver::ResolveImpactDirectionFromContext(ContextNanHandle, nullptr, Player).IsZero());

		FHitResult InfHit;
		InfHit.ImpactNormal = FVector(INFINITY, 0.0f, 0.0f);
		FGameplayEffectContext* ContextInf = new FGameplayEffectContext();
		ContextInf->AddHitResult(InfHit, true);
		FGameplayEffectContextHandle ContextInfHandle(ContextInf);
		TestTrue(TEXT("Inf ImpactNormal returns ZeroVector"),
			FHitReactionImpactResolver::ResolveImpactDirectionFromContext(ContextInfHandle, nullptr, Player).IsZero());

		FHitResult ZeroHit;
		ZeroHit.ImpactNormal = FVector::ZeroVector;
		FGameplayEffectContext* ContextZero = new FGameplayEffectContext();
		ContextZero->AddHitResult(ZeroHit, true);
		FGameplayEffectContextHandle ContextZeroHandle(ContextZero);
		TestTrue(TEXT("Zero ImpactNormal and null instigator returns ZeroVector"),
			FHitReactionImpactResolver::ResolveImpactDirectionFromContext(ContextZeroHandle, nullptr, Player).IsZero());

		TestTrue(TEXT("Null Target returns ZeroVector"),
			FHitReactionImpactResolver::ResolveImpactDirectionFromContext(ContextFrontHandle, nullptr, nullptr).IsZero());

		// 4.8 TryBuildLaunchFacingAndVelocity Pure Function Tests
		{
			float OutFacingYaw = 0.0f;
			FVector OutVel = FVector::ZeroVector;

			// 4.8.1 Reference Yaw = 0 deg (Target facing +X)
			// a) Front attacker (1, 0, 0) -> Facing points Front (0 deg), Velocity launches away (-450, 0, 550)
			TestTrue(TEXT("TryBuildLaunchFacingAndVelocity succeeds for front attacker at Yaw=0"),
				FHitReactionImpactResolver::TryBuildLaunchFacingAndVelocity(FVector(1.0f, 0.0f, 0.0f), 0.0f, 450.0f, 550.0f, OutFacingYaw, OutVel));
			TestTrue(TEXT("Front attacker at Yaw=0 faces Front (Yaw ~0 deg)"),
				FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(OutFacingYaw, 0.0f), 0.01f));
			TestTrue(TEXT("Front attacker at Yaw=0 produces velocity (-450, 0, 550)"),
				OutVel.Equals(FVector(-450.0f, 0.0f, 550.0f), 0.01f));

			// b) Back attacker (-1, 0, 0) -> Facing points Back (180 deg), Velocity launches away (+450, 0, 550)
			TestTrue(TEXT("TryBuildLaunchFacingAndVelocity succeeds for back attacker at Yaw=0"),
				FHitReactionImpactResolver::TryBuildLaunchFacingAndVelocity(FVector(-1.0f, 0.0f, 0.0f), 0.0f, 450.0f, 550.0f, OutFacingYaw, OutVel));
			TestTrue(TEXT("Back attacker at Yaw=0 faces Back (Yaw ~180 deg)"),
				FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(OutFacingYaw, 180.0f), 0.01f));
			TestTrue(TEXT("Back attacker at Yaw=0 produces velocity (+450, 0, 550)"),
				OutVel.Equals(FVector(450.0f, 0.0f, 550.0f), 0.01f));

			// c) Right attacker (0, 1, 0) -> Facing points Right (90 deg), Velocity launches away (0, -450, 550)
			TestTrue(TEXT("TryBuildLaunchFacingAndVelocity succeeds for right attacker at Yaw=0"),
				FHitReactionImpactResolver::TryBuildLaunchFacingAndVelocity(FVector(0.0f, 1.0f, 0.0f), 0.0f, 450.0f, 550.0f, OutFacingYaw, OutVel));
			TestTrue(TEXT("Right attacker at Yaw=0 faces Right (Yaw ~90 deg)"),
				FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(OutFacingYaw, 90.0f), 0.01f));
			TestTrue(TEXT("Right attacker at Yaw=0 produces velocity (0, -450, 550)"),
				OutVel.Equals(FVector(0.0f, -450.0f, 550.0f), 0.01f));

			// d) Left attacker (0, -1, 0) -> Facing points Left (-90 deg), Velocity launches away (0, +450, 550)
			TestTrue(TEXT("TryBuildLaunchFacingAndVelocity succeeds for left attacker at Yaw=0"),
				FHitReactionImpactResolver::TryBuildLaunchFacingAndVelocity(FVector(0.0f, -1.0f, 0.0f), 0.0f, 450.0f, 550.0f, OutFacingYaw, OutVel));
			TestTrue(TEXT("Left attacker at Yaw=0 faces Left (Yaw ~ -90 deg)"),
				FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(OutFacingYaw, -90.0f), 0.01f));
			TestTrue(TEXT("Left attacker at Yaw=0 produces velocity (0, 450, 550)"),
				OutVel.Equals(FVector(0.0f, 450.0f, 550.0f), 0.01f));

			// 4.8.2 Rotated Reference Yaw Matrix (45 deg, 90 deg, 180 deg)
			// a) Reference Yaw = 45 deg
			TestTrue(TEXT("TryBuildLaunchFacingAndVelocity succeeds for front attacker at Yaw=45"),
				FHitReactionImpactResolver::TryBuildLaunchFacingAndVelocity(FVector(1.0f, 0.0f, 0.0f), 45.0f, 450.0f, 550.0f, OutFacingYaw, OutVel));
			TestTrue(TEXT("Front attacker at Yaw=45 faces 45 deg"),
				FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(OutFacingYaw, 45.0f), 0.01f));
			const float Cos45 = FMath::Cos(FMath::DegreesToRadians(45.0f));
			const float Sin45 = FMath::Sin(FMath::DegreesToRadians(45.0f));
			TestTrue(TEXT("Front attacker at Yaw=45 produces velocity (-450*cos45, -450*sin45, 550)"),
				OutVel.Equals(FVector(-450.0f * Cos45, -450.0f * Sin45, 550.0f), 0.05f));

			TestTrue(TEXT("TryBuildLaunchFacingAndVelocity succeeds for right attacker at Yaw=45"),
				FHitReactionImpactResolver::TryBuildLaunchFacingAndVelocity(FVector(0.0f, 1.0f, 0.0f), 45.0f, 450.0f, 550.0f, OutFacingYaw, OutVel));
			TestTrue(TEXT("Right attacker at Yaw=45 faces 135 deg"),
				FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(OutFacingYaw, 135.0f), 0.01f));
			const float Cos135 = FMath::Cos(FMath::DegreesToRadians(135.0f));
			const float Sin135 = FMath::Sin(FMath::DegreesToRadians(135.0f));
			TestTrue(TEXT("Right attacker at Yaw=45 produces velocity (-450*cos135, -450*sin135, 550)"),
				OutVel.Equals(FVector(-450.0f * Cos135, -450.0f * Sin135, 550.0f), 0.05f));

			// b) Reference Yaw = 90 deg (Target facing +Y)
			TestTrue(TEXT("TryBuildLaunchFacingAndVelocity succeeds for front attacker at Yaw=90"),
				FHitReactionImpactResolver::TryBuildLaunchFacingAndVelocity(FVector(1.0f, 0.0f, 0.0f), 90.0f, 450.0f, 550.0f, OutFacingYaw, OutVel));
			TestTrue(TEXT("Front attacker at Yaw=90 faces 90 deg"),
				FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(OutFacingYaw, 90.0f), 0.01f));
			TestTrue(TEXT("Front attacker at Yaw=90 produces velocity (0, -450, 550)"),
				OutVel.Equals(FVector(0.0f, -450.0f, 550.0f), 0.01f));

			TestTrue(TEXT("TryBuildLaunchFacingAndVelocity succeeds for back attacker at Yaw=90"),
				FHitReactionImpactResolver::TryBuildLaunchFacingAndVelocity(FVector(-1.0f, 0.0f, 0.0f), 90.0f, 450.0f, 550.0f, OutFacingYaw, OutVel));
			TestTrue(TEXT("Back attacker at Yaw=90 faces -90 deg"),
				FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(OutFacingYaw, -90.0f), 0.01f));
			TestTrue(TEXT("Back attacker at Yaw=90 produces velocity (0, 450, 550)"),
				OutVel.Equals(FVector(0.0f, 450.0f, 550.0f), 0.01f));

			TestTrue(TEXT("TryBuildLaunchFacingAndVelocity succeeds for right attacker at Yaw=90"),
				FHitReactionImpactResolver::TryBuildLaunchFacingAndVelocity(FVector(0.0f, 1.0f, 0.0f), 90.0f, 450.0f, 550.0f, OutFacingYaw, OutVel));
			TestTrue(TEXT("Right attacker at Yaw=90 faces 180 deg"),
				FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(OutFacingYaw, 180.0f), 0.01f));
			TestTrue(TEXT("Right attacker at Yaw=90 produces velocity (450, 0, 550)"),
				OutVel.Equals(FVector(450.0f, 0.0f, 550.0f), 0.01f));

			TestTrue(TEXT("TryBuildLaunchFacingAndVelocity succeeds for left attacker at Yaw=90"),
				FHitReactionImpactResolver::TryBuildLaunchFacingAndVelocity(FVector(0.0f, -1.0f, 0.0f), 90.0f, 450.0f, 550.0f, OutFacingYaw, OutVel));
			TestTrue(TEXT("Left attacker at Yaw=90 faces 0 deg"),
				FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(OutFacingYaw, 0.0f), 0.01f));
			TestTrue(TEXT("Left attacker at Yaw=90 produces velocity (-450, 0, 550)"),
				OutVel.Equals(FVector(-450.0f, 0.0f, 550.0f), 0.01f));

			// c) Reference Yaw = 180 deg (Target facing -X)
			TestTrue(TEXT("TryBuildLaunchFacingAndVelocity succeeds for front attacker at Yaw=180"),
				FHitReactionImpactResolver::TryBuildLaunchFacingAndVelocity(FVector(1.0f, 0.0f, 0.0f), 180.0f, 450.0f, 550.0f, OutFacingYaw, OutVel));
			TestTrue(TEXT("Front attacker at Yaw=180 faces 180 deg"),
				FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(OutFacingYaw, 180.0f), 0.01f));
			TestTrue(TEXT("Front attacker at Yaw=180 produces velocity (450, 0, 550)"),
				OutVel.Equals(FVector(450.0f, 0.0f, 550.0f), 0.01f));

			TestTrue(TEXT("TryBuildLaunchFacingAndVelocity succeeds for back attacker at Yaw=180"),
				FHitReactionImpactResolver::TryBuildLaunchFacingAndVelocity(FVector(-1.0f, 0.0f, 0.0f), 180.0f, 450.0f, 550.0f, OutFacingYaw, OutVel));
			TestTrue(TEXT("Back attacker at Yaw=180 faces 0 deg"),
				FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(OutFacingYaw, 0.0f), 0.01f));
			TestTrue(TEXT("Back attacker at Yaw=180 produces velocity (-450, 0, 550)"),
				OutVel.Equals(FVector(-450.0f, 0.0f, 550.0f), 0.01f));

			// 4.8.3 Unnormalized vector with non-zero Z -> planarized and normalized
			TestTrue(TEXT("TryBuildLaunchFacingAndVelocity succeeds for unnormalized vector with Z component"),
				FHitReactionImpactResolver::TryBuildLaunchFacingAndVelocity(FVector(10.0f, 0.0f, 50.0f), 0.0f, 450.0f, 550.0f, OutFacingYaw, OutVel));
			TestTrue(TEXT("Unnormalized vector at Yaw=0 faces Front (0 deg)"),
				FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(OutFacingYaw, 0.0f), 0.01f));
			TestTrue(TEXT("Unnormalized vector produces correct normalized velocity (-450, 0, 550)"),
				OutVel.Equals(FVector(-450.0f, 0.0f, 550.0f), 0.01f));

			// 4.8.4 Fail-Closed Tests for TryBuildLaunchFacingAndVelocity
			// a) Zero direction vector
			TestFalse(TEXT("Zero direction returns false"),
				FHitReactionImpactResolver::TryBuildLaunchFacingAndVelocity(FVector::ZeroVector, 0.0f, 450.0f, 550.0f, OutFacingYaw, OutVel));
			TestEqual(TEXT("Zero direction resets OutFacingYaw to 0.0f"), OutFacingYaw, 0.0f);
			TestTrue(TEXT("Zero direction resets OutVelocity to ZeroVector"), OutVel.IsZero());

			// b) NaN direction
			TestFalse(TEXT("NaN direction returns false"),
				FHitReactionImpactResolver::TryBuildLaunchFacingAndVelocity(FVector(NAN, 0.0f, 0.0f), 0.0f, 450.0f, 550.0f, OutFacingYaw, OutVel));
			TestEqual(TEXT("NaN direction resets OutFacingYaw to 0.0f"), OutFacingYaw, 0.0f);
			TestTrue(TEXT("NaN direction resets OutVelocity to ZeroVector"), OutVel.IsZero());

			// c) Inf direction
			TestFalse(TEXT("Inf direction returns false"),
				FHitReactionImpactResolver::TryBuildLaunchFacingAndVelocity(FVector(INFINITY, 0.0f, 0.0f), 0.0f, 450.0f, 550.0f, OutFacingYaw, OutVel));
			TestEqual(TEXT("Inf direction resets OutFacingYaw to 0.0f"), OutFacingYaw, 0.0f);
			TestTrue(TEXT("Inf direction resets OutVelocity to ZeroVector"), OutVel.IsZero());

			// d) NaN Yaw
			TestFalse(TEXT("NaN Yaw returns false"),
				FHitReactionImpactResolver::TryBuildLaunchFacingAndVelocity(FVector(1.0f, 0.0f, 0.0f), NAN, 450.0f, 550.0f, OutFacingYaw, OutVel));
			TestEqual(TEXT("NaN Yaw resets OutFacingYaw to 0.0f"), OutFacingYaw, 0.0f);
			TestTrue(TEXT("NaN Yaw resets OutVelocity to ZeroVector"), OutVel.IsZero());

			// e) Inf Yaw
			TestFalse(TEXT("Inf Yaw returns false"),
				FHitReactionImpactResolver::TryBuildLaunchFacingAndVelocity(FVector(1.0f, 0.0f, 0.0f), INFINITY, 450.0f, 550.0f, OutFacingYaw, OutVel));
			TestEqual(TEXT("Inf Yaw resets OutFacingYaw to 0.0f"), OutFacingYaw, 0.0f);
			TestTrue(TEXT("Inf Yaw resets OutVelocity to ZeroVector"), OutVel.IsZero());

			// f) Zero / negative / non-finite HorizontalSpeed
			TestFalse(TEXT("Zero HorizontalSpeed returns false"),
				FHitReactionImpactResolver::TryBuildLaunchFacingAndVelocity(FVector(1.0f, 0.0f, 0.0f), 0.0f, 0.0f, 550.0f, OutFacingYaw, OutVel));
			TestEqual(TEXT("Zero HorizontalSpeed resets OutFacingYaw to 0.0f"), OutFacingYaw, 0.0f);
			TestTrue(TEXT("Zero HorizontalSpeed resets OutVelocity to ZeroVector"), OutVel.IsZero());

			TestFalse(TEXT("Negative HorizontalSpeed returns false"),
				FHitReactionImpactResolver::TryBuildLaunchFacingAndVelocity(FVector(1.0f, 0.0f, 0.0f), 0.0f, -450.0f, 550.0f, OutFacingYaw, OutVel));
			TestEqual(TEXT("Negative HorizontalSpeed resets OutFacingYaw to 0.0f"), OutFacingYaw, 0.0f);
			TestTrue(TEXT("Negative HorizontalSpeed resets OutVelocity to ZeroVector"), OutVel.IsZero());

			TestFalse(TEXT("NaN HorizontalSpeed returns false"),
				FHitReactionImpactResolver::TryBuildLaunchFacingAndVelocity(FVector(1.0f, 0.0f, 0.0f), 0.0f, NAN, 550.0f, OutFacingYaw, OutVel));
			TestEqual(TEXT("NaN HorizontalSpeed resets OutFacingYaw to 0.0f"), OutFacingYaw, 0.0f);
			TestTrue(TEXT("NaN HorizontalSpeed resets OutVelocity to ZeroVector"), OutVel.IsZero());

			TestFalse(TEXT("Inf HorizontalSpeed returns false"),
				FHitReactionImpactResolver::TryBuildLaunchFacingAndVelocity(FVector(1.0f, 0.0f, 0.0f), 0.0f, INFINITY, 550.0f, OutFacingYaw, OutVel));
			TestEqual(TEXT("Inf HorizontalSpeed resets OutFacingYaw to 0.0f"), OutFacingYaw, 0.0f);
			TestTrue(TEXT("Inf HorizontalSpeed resets OutVelocity to ZeroVector"), OutVel.IsZero());

			// g) Zero / negative / non-finite VerticalSpeed
			TestFalse(TEXT("Zero VerticalSpeed returns false"),
				FHitReactionImpactResolver::TryBuildLaunchFacingAndVelocity(FVector(1.0f, 0.0f, 0.0f), 0.0f, 450.0f, 0.0f, OutFacingYaw, OutVel));
			TestEqual(TEXT("Zero VerticalSpeed resets OutFacingYaw to 0.0f"), OutFacingYaw, 0.0f);
			TestTrue(TEXT("Zero VerticalSpeed resets OutVelocity to ZeroVector"), OutVel.IsZero());

			TestFalse(TEXT("Negative VerticalSpeed returns false"),
				FHitReactionImpactResolver::TryBuildLaunchFacingAndVelocity(FVector(1.0f, 0.0f, 0.0f), 0.0f, 450.0f, -550.0f, OutFacingYaw, OutVel));
			TestEqual(TEXT("Negative VerticalSpeed resets OutFacingYaw to 0.0f"), OutFacingYaw, 0.0f);
			TestTrue(TEXT("Negative VerticalSpeed resets OutVelocity to ZeroVector"), OutVel.IsZero());

			TestFalse(TEXT("NaN VerticalSpeed returns false"),
				FHitReactionImpactResolver::TryBuildLaunchFacingAndVelocity(FVector(1.0f, 0.0f, 0.0f), 0.0f, 450.0f, NAN, OutFacingYaw, OutVel));
			TestEqual(TEXT("NaN VerticalSpeed resets OutFacingYaw to 0.0f"), OutFacingYaw, 0.0f);
			TestTrue(TEXT("NaN VerticalSpeed resets OutVelocity to ZeroVector"), OutVel.IsZero());

			TestFalse(TEXT("Inf VerticalSpeed returns false"),
				FHitReactionImpactResolver::TryBuildLaunchFacingAndVelocity(FVector(1.0f, 0.0f, 0.0f), 0.0f, 450.0f, INFINITY, OutFacingYaw, OutVel));
			TestEqual(TEXT("Inf VerticalSpeed resets OutFacingYaw to 0.0f"), OutFacingYaw, 0.0f);
			TestTrue(TEXT("Inf VerticalSpeed resets OutVelocity to ZeroVector"), OutVel.IsZero());

			// 4.8.5 TryBuildLaunchVelocity Forwarding Wrapper Regressions
			{
				FVector WrapperVel = FVector::ZeroVector;

				TestTrue(TEXT("TryBuildLaunchVelocity wrapper succeeds for front attacker at Yaw=0"),
					FHitReactionImpactResolver::TryBuildLaunchVelocity(FVector(1.0f, 0.0f, 0.0f), 0.0f, 450.0f, 550.0f, WrapperVel));
				TestTrue(TEXT("Wrapper front attacker at Yaw=0 produces velocity (-450, 0, 550)"),
					WrapperVel.Equals(FVector(-450.0f, 0.0f, 550.0f), 0.01f));

				TestTrue(TEXT("TryBuildLaunchVelocity wrapper succeeds for front attacker at Yaw=90"),
					FHitReactionImpactResolver::TryBuildLaunchVelocity(FVector(1.0f, 0.0f, 0.0f), 90.0f, 450.0f, 550.0f, WrapperVel));
				TestTrue(TEXT("Wrapper front attacker at Yaw=90 produces velocity (0, -450, 550)"),
					WrapperVel.Equals(FVector(0.0f, -450.0f, 550.0f), 0.01f));

				TestTrue(TEXT("TryBuildLaunchVelocity wrapper succeeds for back attacker at Yaw=0"),
					FHitReactionImpactResolver::TryBuildLaunchVelocity(FVector(-1.0f, 0.0f, 0.0f), 0.0f, 450.0f, 550.0f, WrapperVel));
				TestTrue(TEXT("Wrapper back attacker at Yaw=0 produces velocity (+450, 0, 550)"),
					WrapperVel.Equals(FVector(450.0f, 0.0f, 550.0f), 0.01f));

				TestTrue(TEXT("TryBuildLaunchVelocity wrapper succeeds for unnormalized vector"),
					FHitReactionImpactResolver::TryBuildLaunchVelocity(FVector(10.0f, 0.0f, 50.0f), 0.0f, 450.0f, 550.0f, WrapperVel));
				TestTrue(TEXT("Wrapper unnormalized vector produces normalized velocity (-450, 0, 550)"),
					WrapperVel.Equals(FVector(-450.0f, 0.0f, 550.0f), 0.01f));

				TestFalse(TEXT("Wrapper zero direction fails closed"),
					FHitReactionImpactResolver::TryBuildLaunchVelocity(FVector::ZeroVector, 0.0f, 450.0f, 550.0f, WrapperVel));
				TestTrue(TEXT("Wrapper zero direction resets OutVelocity"), WrapperVel.IsZero());

				TestFalse(TEXT("Wrapper NaN direction fails closed"),
					FHitReactionImpactResolver::TryBuildLaunchVelocity(FVector(NAN, 0.0f, 0.0f), 0.0f, 450.0f, 550.0f, WrapperVel));
				TestTrue(TEXT("Wrapper NaN direction resets OutVelocity"), WrapperVel.IsZero());

				TestFalse(TEXT("Wrapper NaN Yaw fails closed"),
					FHitReactionImpactResolver::TryBuildLaunchVelocity(FVector(1.0f, 0.0f, 0.0f), NAN, 450.0f, 550.0f, WrapperVel));
				TestTrue(TEXT("Wrapper NaN Yaw resets OutVelocity"), WrapperVel.IsZero());

				TestFalse(TEXT("Wrapper negative speed fails closed"),
					FHitReactionImpactResolver::TryBuildLaunchVelocity(FVector(1.0f, 0.0f, 0.0f), 0.0f, -450.0f, 550.0f, WrapperVel));
				TestTrue(TEXT("Wrapper negative speed resets OutVelocity"), WrapperVel.IsZero());
			}
		}

		// 4.9 Four-Way Montage Selector Pure Function Tests (FHitReactionFourWayMontageSelector)
		{
			UAnimMontage* DummyFront = NewObject<UAnimMontage>(GetTransientPackage());
			UAnimMontage* DummyBack = NewObject<UAnimMontage>(GetTransientPackage());
			UAnimMontage* DummyLeft = NewObject<UAnimMontage>(GetTransientPackage());
			UAnimMontage* DummyRight = NewObject<UAnimMontage>(GetTransientPackage());

			FHitReactionFourWayMontageSet FullSet;
			FullSet.Front = DummyFront;
			FullSet.Back = DummyBack;
			FullSet.Left = DummyLeft;
			FullSet.Right = DummyRight;

			// 4.9.1 Set Completeness Check (IsComplete)
			TestTrue(TEXT("Selector: FullSet is complete"), FullSet.IsComplete());

			FHitReactionFourWayMontageSet SetMissingFront;
			SetMissingFront.Back = DummyBack;
			SetMissingFront.Left = DummyLeft;
			SetMissingFront.Right = DummyRight;
			TestFalse(TEXT("Selector: Set missing Front is incomplete"), SetMissingFront.IsComplete());

			FHitReactionFourWayMontageSet SetMissingBack;
			SetMissingBack.Front = DummyFront;
			SetMissingBack.Left = DummyLeft;
			SetMissingBack.Right = DummyRight;
			TestFalse(TEXT("Selector: Set missing Back is incomplete"), SetMissingBack.IsComplete());

			FHitReactionFourWayMontageSet SetMissingLeft;
			SetMissingLeft.Front = DummyFront;
			SetMissingLeft.Back = DummyBack;
			SetMissingLeft.Right = DummyRight;
			TestFalse(TEXT("Selector: Set missing Left is incomplete"), SetMissingLeft.IsComplete());

			FHitReactionFourWayMontageSet SetMissingRight;
			SetMissingRight.Front = DummyFront;
			SetMissingRight.Back = DummyBack;
			SetMissingRight.Left = DummyLeft;
			TestFalse(TEXT("Selector: Set missing Right is incomplete"), SetMissingRight.IsComplete());

			FHitReactionFourWayMontageSet EmptySet;
			TestFalse(TEXT("Selector: EmptySet is incomplete"), EmptySet.IsComplete());

			// 4.9.2 Cardinal directions (FullSet)
			// a) Front (+X)
			TestEqual(TEXT("Selector: Cardinal Front (+X) returns Front montage"),
				FHitReactionFourWayMontageSelector::SelectFromLocalAttackerDirection(FVector(1.0f, 0.0f, 0.0f), FullSet), DummyFront);

			// b) Back (-X)
			TestEqual(TEXT("Selector: Cardinal Back (-X) returns Back montage"),
				FHitReactionFourWayMontageSelector::SelectFromLocalAttackerDirection(FVector(-1.0f, 0.0f, 0.0f), FullSet), DummyBack);

			// c) Right (+Y)
			TestEqual(TEXT("Selector: Cardinal Right (+Y) returns Right montage"),
				FHitReactionFourWayMontageSelector::SelectFromLocalAttackerDirection(FVector(0.0f, 1.0f, 0.0f), FullSet), DummyRight);

			// d) Left (-Y)
			TestEqual(TEXT("Selector: Cardinal Left (-Y) returns Left montage"),
				FHitReactionFourWayMontageSelector::SelectFromLocalAttackerDirection(FVector(0.0f, -1.0f, 0.0f), FullSet), DummyLeft);

			// 4.9.3 Exact 45-degree diagonal ties (X-axis priority rule)
			// a) Front-Right (+X, +Y, AbsX == AbsY) -> Front
			TestEqual(TEXT("Selector: Diagonal (+1, +1) tie-breaks to Front"),
				FHitReactionFourWayMontageSelector::SelectFromLocalAttackerDirection(FVector(1.0f, 1.0f, 0.0f), FullSet), DummyFront);

			// b) Front-Left (+X, -Y, AbsX == AbsY) -> Front
			TestEqual(TEXT("Selector: Diagonal (+1, -1) tie-breaks to Front"),
				FHitReactionFourWayMontageSelector::SelectFromLocalAttackerDirection(FVector(1.0f, -1.0f, 0.0f), FullSet), DummyFront);

			// c) Back-Right (-X, +Y, AbsX == AbsY) -> Back
			TestEqual(TEXT("Selector: Diagonal (-1, +1) tie-breaks to Back"),
				FHitReactionFourWayMontageSelector::SelectFromLocalAttackerDirection(FVector(-1.0f, 1.0f, 0.0f), FullSet), DummyBack);

			// d) Back-Left (-X, -Y, AbsX == AbsY) -> Back
			TestEqual(TEXT("Selector: Diagonal (-1, -1) tie-breaks to Back"),
				FHitReactionFourWayMontageSelector::SelectFromLocalAttackerDirection(FVector(-1.0f, -1.0f, 0.0f), FullSet), DummyBack);

			// 4.9.4 Boundary crossings (samples immediately on both sides of each 45-degree boundary)
			// a) Front / Right boundary: (1.001, 1.0) -> Front; (1.0, 1.001) -> Right
			TestEqual(TEXT("Selector: Front-leaning (+1.001, +1.0) selects Front"),
				FHitReactionFourWayMontageSelector::SelectFromLocalAttackerDirection(FVector(1.001f, 1.0f, 0.0f), FullSet), DummyFront);
			TestEqual(TEXT("Selector: Right-leaning (+1.0, +1.001) selects Right"),
				FHitReactionFourWayMontageSelector::SelectFromLocalAttackerDirection(FVector(1.0f, 1.001f, 0.0f), FullSet), DummyRight);

			// b) Front / Left boundary: (1.001, -1.0) -> Front; (1.0, -1.001) -> Left
			TestEqual(TEXT("Selector: Front-leaning (+1.001, -1.0) selects Front"),
				FHitReactionFourWayMontageSelector::SelectFromLocalAttackerDirection(FVector(1.001f, -1.0f, 0.0f), FullSet), DummyFront);
			TestEqual(TEXT("Selector: Left-leaning (+1.0, -1.001) selects Left"),
				FHitReactionFourWayMontageSelector::SelectFromLocalAttackerDirection(FVector(1.0f, -1.001f, 0.0f), FullSet), DummyLeft);

			// c) Back / Right boundary: (-1.001, 1.0) -> Back; (-1.0, 1.001) -> Right
			TestEqual(TEXT("Selector: Back-leaning (-1.001, +1.0) selects Back"),
				FHitReactionFourWayMontageSelector::SelectFromLocalAttackerDirection(FVector(-1.001f, 1.0f, 0.0f), FullSet), DummyBack);
			TestEqual(TEXT("Selector: Right-leaning (-1.0, +1.001) selects Right"),
				FHitReactionFourWayMontageSelector::SelectFromLocalAttackerDirection(FVector(-1.0f, 1.001f, 0.0f), FullSet), DummyRight);

			// d) Back / Left boundary: (-1.001, -1.0) -> Back; (-1.0, -1.001) -> Left
			TestEqual(TEXT("Selector: Back-leaning (-1.001, -1.0) selects Back"),
				FHitReactionFourWayMontageSelector::SelectFromLocalAttackerDirection(FVector(-1.001f, -1.0f, 0.0f), FullSet), DummyBack);
			TestEqual(TEXT("Selector: Left-leaning (-1.0, -1.001) selects Left"),
				FHitReactionFourWayMontageSelector::SelectFromLocalAttackerDirection(FVector(-1.0f, -1.001f, 0.0f), FullSet), DummyLeft);

			// 4.9.5 Unnormalized vector and non-zero Z input (XY projection)
			TestEqual(TEXT("Selector: Unnormalized with +Z (100, 0, 50) selects Front"),
				FHitReactionFourWayMontageSelector::SelectFromLocalAttackerDirection(FVector(100.0f, 0.0f, 50.0f), FullSet), DummyFront);
			TestEqual(TEXT("Selector: Unnormalized with -Z (-50, 0, -20) selects Back"),
				FHitReactionFourWayMontageSelector::SelectFromLocalAttackerDirection(FVector(-50.0f, 0.0f, -20.0f), FullSet), DummyBack);
			TestEqual(TEXT("Selector: Unnormalized with +Z (0, 80, 120) selects Right"),
				FHitReactionFourWayMontageSelector::SelectFromLocalAttackerDirection(FVector(0.0f, 80.0f, 120.0f), FullSet), DummyRight);
			TestEqual(TEXT("Selector: Unnormalized with -Z (0, -90, -30) selects Left"),
				FHitReactionFourWayMontageSelector::SelectFromLocalAttackerDirection(FVector(0.0f, -90.0f, -30.0f), FullSet), DummyLeft);
			TestEqual(TEXT("Selector: Unnormalized diagonal with huge Z (30, 60, 999) selects Right"),
				FHitReactionFourWayMontageSelector::SelectFromLocalAttackerDirection(FVector(30.0f, 60.0f, 999.0f), FullSet), DummyRight);
			TestEqual(TEXT("Selector: Unnormalized diagonal with huge -Z (-60, 30, -999) selects Back"),
				FHitReactionFourWayMontageSelector::SelectFromLocalAttackerDirection(FVector(-60.0f, 30.0f, -999.0f), FullSet), DummyBack);

			// 4.9.6 Zero, Near-Zero, NaN, and Inf inputs (fail-closed to nullptr)
			TestNull(TEXT("Selector: Zero direction returns nullptr"),
				FHitReactionFourWayMontageSelector::SelectFromLocalAttackerDirection(FVector::ZeroVector, FullSet));
			TestNull(TEXT("Selector: Near-zero direction (1e-5 on X) returns nullptr"),
				FHitReactionFourWayMontageSelector::SelectFromLocalAttackerDirection(FVector(1e-5f, 0.0f, 0.0f), FullSet));
			TestNull(TEXT("Selector: Near-zero direction (-1e-5 on Y) returns nullptr"),
				FHitReactionFourWayMontageSelector::SelectFromLocalAttackerDirection(FVector(0.0f, -1e-5f, 0.0f), FullSet));
			TestNull(TEXT("Selector: Near-zero direction (1e-5 on XY with non-zero Z) returns nullptr"),
				FHitReactionFourWayMontageSelector::SelectFromLocalAttackerDirection(FVector(1e-5f, 1e-5f, 50.0f), FullSet));
			TestNull(TEXT("Selector: NaN X returns nullptr"),
				FHitReactionFourWayMontageSelector::SelectFromLocalAttackerDirection(FVector(NAN, 0.0f, 0.0f), FullSet));
			TestNull(TEXT("Selector: NaN Y returns nullptr"),
				FHitReactionFourWayMontageSelector::SelectFromLocalAttackerDirection(FVector(0.0f, NAN, 0.0f), FullSet));
			TestNull(TEXT("Selector: Inf X returns nullptr"),
				FHitReactionFourWayMontageSelector::SelectFromLocalAttackerDirection(FVector(INFINITY, 0.0f, 0.0f), FullSet));
			TestNull(TEXT("Selector: Inf Y returns nullptr"),
				FHitReactionFourWayMontageSelector::SelectFromLocalAttackerDirection(FVector(0.0f, -INFINITY, 0.0f), FullSet));
			TestNull(TEXT("Selector: NaN X and Inf Y returns nullptr"),
				FHitReactionFourWayMontageSelector::SelectFromLocalAttackerDirection(FVector(NAN, INFINITY, 0.0f), FullSet));

			// 4.9.7 Incomplete sets return nullptr
			TestNull(TEXT("Selector: Incomplete set (missing Front) returns nullptr for front attacker"),
				FHitReactionFourWayMontageSelector::SelectFromLocalAttackerDirection(FVector(1.0f, 0.0f, 0.0f), SetMissingFront));
			TestNull(TEXT("Selector: Incomplete set (missing Front) returns nullptr for right attacker"),
				FHitReactionFourWayMontageSelector::SelectFromLocalAttackerDirection(FVector(0.0f, 1.0f, 0.0f), SetMissingFront));
			TestNull(TEXT("Selector: Incomplete set (missing Back) returns nullptr"),
				FHitReactionFourWayMontageSelector::SelectFromLocalAttackerDirection(FVector(-1.0f, 0.0f, 0.0f), SetMissingBack));
			TestNull(TEXT("Selector: Incomplete set (missing Left) returns nullptr"),
				FHitReactionFourWayMontageSelector::SelectFromLocalAttackerDirection(FVector(0.0f, -1.0f, 0.0f), SetMissingLeft));
			TestNull(TEXT("Selector: Incomplete set (missing Right) returns nullptr"),
				FHitReactionFourWayMontageSelector::SelectFromLocalAttackerDirection(FVector(0.0f, 1.0f, 0.0f), SetMissingRight));
			TestNull(TEXT("Selector: EmptySet returns nullptr for Front"),
				FHitReactionFourWayMontageSelector::SelectFromLocalAttackerDirection(FVector(1.0f, 0.0f, 0.0f), EmptySet));
			TestNull(TEXT("Selector: EmptySet returns nullptr for Back"),
				FHitReactionFourWayMontageSelector::SelectFromLocalAttackerDirection(FVector(-1.0f, 0.0f, 0.0f), EmptySet));
		}
	}

	// -------------------------------------------------------------------------
	// SECTION 5: Target-Side Health Attribute Change & Event Dispatch
	// -------------------------------------------------------------------------
	auto ApplyDamageWithTags = [](UAbilitySystemComponent* SourceASC, UAbilitySystemComponent* TargetASC, AActor* SourceActor, const FGameplayTagContainer& DynamicTags) -> bool
	{
		if (!SourceASC || !TargetASC)
		{
			return false;
		}

		FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
		Context.AddInstigator(SourceActor, SourceActor);

		FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(UTestProjectileDamageGE::StaticClass(), 1, Context);
		if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
		{
			return false;
		}

		SpecHandle.Data->AppendDynamicAssetTags(DynamicTags);
		return TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get()).WasSuccessfullyApplied();
	};

	// 5.1 Player Character Health Dispatch Tests
	{
		UAbilitySystemComponent* PlayerASC = Player->GetAbilitySystemComponent();
		TestNotNull(TEXT("Player ASC valid"), PlayerASC);

		if (PlayerASC)
		{
			PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 100.0f);
			PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxHealthAttribute(), 100.0f);

			int32 PlayerSmallEventCount = 0;
			int32 PlayerBigEventCount = 0;
			int32 PlayerLaunchEventCount = 0;
			float LastSmallMagnitude = 0.0f;
			float LastBigMagnitude = 0.0f;
			float LastLaunchMagnitude = 0.0f;
			const AActor* LastSmallInstigator = nullptr;
			const AActor* LastBigInstigator = nullptr;
			const AActor* LastLaunchInstigator = nullptr;

			PlayerASC->GenericGameplayEventCallbacks.FindOrAdd(TagEventPlayerSmall).AddLambda(
				[&PlayerSmallEventCount, &LastSmallMagnitude, &LastSmallInstigator](const FGameplayEventData* Payload)
				{
					if (Payload)
					{
						PlayerSmallEventCount++;
						LastSmallMagnitude = Payload->EventMagnitude;
						LastSmallInstigator = Payload->Instigator.Get();
					}
				});

			PlayerASC->GenericGameplayEventCallbacks.FindOrAdd(TagEventPlayerBig).AddLambda(
				[&PlayerBigEventCount, &LastBigMagnitude, &LastBigInstigator](const FGameplayEventData* Payload)
				{
					if (Payload)
					{
						PlayerBigEventCount++;
						LastBigMagnitude = Payload->EventMagnitude;
						LastBigInstigator = Payload->Instigator.Get();
					}
				});

			PlayerASC->GenericGameplayEventCallbacks.FindOrAdd(TagEventPlayerLaunch).AddLambda(
				[&PlayerLaunchEventCount, &LastLaunchMagnitude, &LastLaunchInstigator](const FGameplayEventData* Payload)
				{
					if (Payload)
					{
						PlayerLaunchEventCount++;
						LastLaunchMagnitude = Payload->EventMagnitude;
						LastLaunchInstigator = Payload->Instigator.Get();
					}
				});

			// 5.1a Apply Damage GE with Data.Reaction.Small -> dispatches Event.Reaction.Player.Small
			FGameplayTagContainer SmallTags;
			SmallTags.AddTag(TagDataSmall);
			TestTrue(TEXT("Damage GE with Data.Reaction.Small applied to Player"),
				ApplyDamageWithTags(PlayerASC, PlayerASC, Player, SmallTags));
			TestEqual(TEXT("Player received 1 Small reaction event"), PlayerSmallEventCount, 1);
			TestEqual(TEXT("Player Small EventMagnitude is 25.0"), LastSmallMagnitude, 25.0f);
			TestEqual(TEXT("Player Small Instigator is Player"), LastSmallInstigator, (const AActor*)Player);
			TestEqual(TEXT("Player Big count is 0 after Small GE"), PlayerBigEventCount, 0);
			TestEqual(TEXT("Player Launch count is 0 after Small GE"), PlayerLaunchEventCount, 0);

			// Restore Health to 100 for non-lethal branch isolation
			PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 100.0f);

			// 5.1b Apply Damage GE with Data.Reaction.Big -> dispatches Event.Reaction.Player.Big
			FGameplayTagContainer BigTags;
			BigTags.AddTag(TagDataBig);
			TestTrue(TEXT("Damage GE with Data.Reaction.Big applied to Player"),
				ApplyDamageWithTags(PlayerASC, PlayerASC, Player, BigTags));
			TestEqual(TEXT("Player received 1 Big reaction event"), PlayerBigEventCount, 1);
			TestEqual(TEXT("Player Big EventMagnitude is 25.0"), LastBigMagnitude, 25.0f);
			TestEqual(TEXT("Player Big Instigator is Player"), LastBigInstigator, (const AActor*)Player);
			TestEqual(TEXT("Player Small count remains 1 after Big GE"), PlayerSmallEventCount, 1);
			TestEqual(TEXT("Player Launch count remains 0 after Big GE"), PlayerLaunchEventCount, 0);

			// Restore Health to 100
			PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 100.0f);

			// 5.1c Apply Damage GE with Data.Reaction.Launch -> dispatches Event.Reaction.Player.Launch
			FGameplayTagContainer LaunchTags;
			LaunchTags.AddTag(TagDataLaunch);
			TestTrue(TEXT("Damage GE with Data.Reaction.Launch applied to Player"),
				ApplyDamageWithTags(PlayerASC, PlayerASC, Player, LaunchTags));
			TestEqual(TEXT("Player received 1 Launch reaction event"), PlayerLaunchEventCount, 1);
			TestEqual(TEXT("Player Launch EventMagnitude is 25.0"), LastLaunchMagnitude, 25.0f);
			TestEqual(TEXT("Player Launch Instigator is Player"), LastLaunchInstigator, (const AActor*)Player);
			TestEqual(TEXT("Player Small count remains 1 after Launch GE"), PlayerSmallEventCount, 1);
			TestEqual(TEXT("Player Big count remains 1 after Launch GE"), PlayerBigEventCount, 1);

			// Restore Health to 100
			PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 100.0f);

			// 5.1d Apply Damage GE with 0 reaction tags -> None (no event)
			FGameplayTagContainer NoReactionTags;
			TestTrue(TEXT("Damage GE with 0 reaction tags applied to Player"),
				ApplyDamageWithTags(PlayerASC, PlayerASC, Player, NoReactionTags));
			TestEqual(TEXT("Player received NO reaction event for unclassified GE"),
				PlayerSmallEventCount + PlayerBigEventCount + PlayerLaunchEventCount, 3);

			// Restore Health to 100
			PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 100.0f);

			// 5.1e Apply Damage GE with multi-tier invalid tags (Small + Big) -> Fail-Closed, no event
			FGameplayTagContainer InvalidTags;
			InvalidTags.AddTag(TagDataSmall);
			InvalidTags.AddTag(TagDataBig);
			TestTrue(TEXT("Damage GE with invalid multi-tier tags applied to Player"),
				ApplyDamageWithTags(PlayerASC, PlayerASC, Player, InvalidTags));
			TestEqual(TEXT("Player received NO reaction event for invalid multi-tier GE"),
				PlayerSmallEventCount + PlayerBigEventCount + PlayerLaunchEventCount, 3);

			// Restore Health to 100
			PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 100.0f);

			// 5.1f Apply Damage GE when Player is Stunned -> blocked, no event
			PlayerASC->AddLooseGameplayTag(TagStunned);
			TestTrue(TEXT("Damage GE with Big applied while Player is Stunned"),
				ApplyDamageWithTags(PlayerASC, PlayerASC, Player, BigTags));
			TestEqual(TEXT("Stunned Player received NO Big event"), PlayerBigEventCount, 1);
			PlayerASC->RemoveLooseGameplayTag(TagStunned);

			// Restore Health to 100
			PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 100.0f);

			// 5.1g Apply Damage GE when Player already has Dead tag -> blocked, no event
			PlayerASC->AddLooseGameplayTag(TagDead);
			TestTrue(TEXT("Damage GE with Big applied while Player has Dead tag"),
				ApplyDamageWithTags(PlayerASC, PlayerASC, Player, BigTags));
			TestEqual(TEXT("Dead Player received NO Big event"), PlayerBigEventCount, 1);
			PlayerASC->RemoveLooseGameplayTag(TagDead);

			// Restore Health to 100
			PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 100.0f);

			// 5.1h Apply Lethal Damage GE (Health 20 -> 0) -> Player sends NO reaction event
			PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 20.0f);
			TestTrue(TEXT("Lethal damage GE applied to Player"),
				ApplyDamageWithTags(PlayerASC, PlayerASC, Player, BigTags));
			TestEqual(TEXT("Lethal damage sent NO Big reaction event on Player"), PlayerBigEventCount, 1);
			TestEqual(TEXT("Player Health reached 0"),
				PlayerASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute()), 0.0f);
		}
	}

	// 5.2 Enemy Character Health Dispatch Tests
	{
		UAbilitySystemComponent* EnemyASC = Enemy->GetAbilitySystemComponent();
		TestNotNull(TEXT("Enemy ASC valid"), EnemyASC);

		if (EnemyASC)
		{
			EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 100.0f);
			EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxHealthAttribute(), 100.0f);
			EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetPoiseAttribute(), 100.0f);
			EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxPoiseAttribute(), 100.0f);

			int32 EnemySmallEventCount = 0;
			int32 EnemyBigEventCount = 0;
			int32 EnemyLaunchEventCount = 0;
			float LastEnemyEventMagnitude = 0.0f;
			float LastEnemyLaunchMagnitude = 0.0f;

			EnemyASC->GenericGameplayEventCallbacks.FindOrAdd(TagEventEnemySmall).AddLambda(
				[&EnemySmallEventCount, &LastEnemyEventMagnitude](const FGameplayEventData* Payload)
				{
					if (Payload)
					{
						EnemySmallEventCount++;
						LastEnemyEventMagnitude = Payload->EventMagnitude;
					}
				});

			EnemyASC->GenericGameplayEventCallbacks.FindOrAdd(TagEventEnemyBig).AddLambda(
				[&EnemyBigEventCount, &LastEnemyEventMagnitude](const FGameplayEventData* Payload)
				{
					if (Payload)
					{
						EnemyBigEventCount++;
						LastEnemyEventMagnitude = Payload->EventMagnitude;
					}
				});

			EnemyASC->GenericGameplayEventCallbacks.FindOrAdd(TagEventEnemyLaunch).AddLambda(
				[&EnemyLaunchEventCount, &LastEnemyLaunchMagnitude](const FGameplayEventData* Payload)
				{
					if (Payload)
					{
						EnemyLaunchEventCount++;
						LastEnemyLaunchMagnitude = Payload->EventMagnitude;
					}
				});

			// 5.2a Apply Damage GE with Data.Reaction.Small -> dispatches Event.Reaction.Enemy.Small
			FGameplayTagContainer SmallTags;
			SmallTags.AddTag(TagDataSmall);
			TestTrue(TEXT("Damage GE with Data.Reaction.Small applied to Enemy"),
				ApplyDamageWithTags(EnemyASC, EnemyASC, Enemy, SmallTags));
			TestEqual(TEXT("Enemy received 1 Small reaction event"), EnemySmallEventCount, 1);
			TestEqual(TEXT("Enemy Small EventMagnitude is 25.0"), LastEnemyEventMagnitude, 25.0f);
			TestEqual(TEXT("Enemy Big count is 0"), EnemyBigEventCount, 0);
			TestEqual(TEXT("Enemy Launch count is 0"), EnemyLaunchEventCount, 0);

			// Restore Health to 100 for non-lethal branch isolation
			EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 100.0f);

			// 5.2b Apply Damage GE with Data.Reaction.Big -> dispatches Event.Reaction.Enemy.Big
			FGameplayTagContainer BigTags;
			BigTags.AddTag(TagDataBig);
			TestTrue(TEXT("Damage GE with Data.Reaction.Big applied to Enemy"),
				ApplyDamageWithTags(EnemyASC, EnemyASC, Enemy, BigTags));
			TestEqual(TEXT("Enemy received 1 Big reaction event"), EnemyBigEventCount, 1);
			TestEqual(TEXT("Enemy Big EventMagnitude is 25.0"), LastEnemyEventMagnitude, 25.0f);
			TestEqual(TEXT("Enemy Small count remains 1 after Big GE"), EnemySmallEventCount, 1);
			TestEqual(TEXT("Enemy Launch count remains 0 after Big GE"), EnemyLaunchEventCount, 0);

			// Restore Health to 100
			EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 100.0f);

			// 5.2c Apply Damage GE with Data.Reaction.Launch -> dispatches Event.Reaction.Enemy.Launch
			FGameplayTagContainer LaunchTags;
			LaunchTags.AddTag(TagDataLaunch);
			TestTrue(TEXT("Damage GE with Data.Reaction.Launch applied to Enemy"),
				ApplyDamageWithTags(EnemyASC, EnemyASC, Enemy, LaunchTags));
			TestEqual(TEXT("Enemy received 1 Launch reaction event"), EnemyLaunchEventCount, 1);
			TestEqual(TEXT("Enemy Launch EventMagnitude is 25.0"), LastEnemyLaunchMagnitude, 25.0f);
			TestEqual(TEXT("Enemy Small count remains 1 after Launch GE"), EnemySmallEventCount, 1);
			TestEqual(TEXT("Enemy Big count remains 1 after Launch GE"), EnemyBigEventCount, 1);

			// Restore Health to 100
			EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 100.0f);

			// 5.2d Apply Damage GE with 0 reaction tags -> None (no event)
			FGameplayTagContainer NoReactionTags;
			TestTrue(TEXT("Damage GE with 0 reaction tags applied to Enemy"),
				ApplyDamageWithTags(EnemyASC, EnemyASC, Enemy, NoReactionTags));
			TestEqual(TEXT("Enemy received NO event for unclassified GE"), EnemySmallEventCount + EnemyBigEventCount + EnemyLaunchEventCount, 3);

			// Restore Health to 100
			EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 100.0f);

			// 5.2e Apply Damage GE with invalid multi-tier tags -> Fail-Closed, no event
			FGameplayTagContainer InvalidTags;
			InvalidTags.AddTag(TagDataSmall);
			InvalidTags.AddTag(TagDataBig);
			TestTrue(TEXT("Damage GE with invalid multi-tier tags applied to Enemy"),
				ApplyDamageWithTags(EnemyASC, EnemyASC, Enemy, InvalidTags));
			TestEqual(TEXT("Enemy received NO event for invalid multi-tier GE"), EnemySmallEventCount + EnemyBigEventCount + EnemyLaunchEventCount, 3);

			// Restore Health to 100
			EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 100.0f);

			// 5.2f Apply Damage GE when Enemy is Stunned -> blocked, no event
			EnemyASC->AddLooseGameplayTag(TagStunned);
			TestTrue(TEXT("Damage GE applied while Enemy is Stunned"),
				ApplyDamageWithTags(EnemyASC, EnemyASC, Enemy, SmallTags));
			TestEqual(TEXT("Stunned Enemy received NO Small event"), EnemySmallEventCount, 1);
			EnemyASC->RemoveLooseGameplayTag(TagStunned);

			// Restore Health to 100
			EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 100.0f);

			// 5.2g Apply Damage GE when Enemy is Poise Broken -> blocked, no reaction event
			EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetPoiseAttribute(), 0.0f);
			TestTrue(TEXT("Enemy IsPoiseBroken() is true when Poise is 0"), Enemy->IsPoiseBroken());
			TestTrue(TEXT("Damage GE applied while Enemy is Poise Broken"),
				ApplyDamageWithTags(EnemyASC, EnemyASC, Enemy, SmallTags));
			TestEqual(TEXT("Poise Broken Enemy received NO Small event"), EnemySmallEventCount, 1);
			EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetPoiseAttribute(), 100.0f);

			// 5.2h Apply Lethal Damage GE (Health 20 -> 0) -> Enemy sets dead state, sends NO reaction event
			EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 20.0f);
			TestTrue(TEXT("Lethal damage GE applied to Enemy"),
				ApplyDamageWithTags(EnemyASC, EnemyASC, Enemy, SmallTags));
			TestTrue(TEXT("Enemy is dead after lethal health depletion"), Enemy->IsDead());
			TestEqual(TEXT("Lethal damage sent NO additional reaction event"), EnemySmallEventCount, 1);
			TestEqual(TEXT("Enemy Big count remains 1"), EnemyBigEventCount, 1);
			TestEqual(TEXT("Enemy Launch count remains 1"), EnemyLaunchEventCount, 1);

			// 5.2i Apply Damage GE when Enemy is already dead -> blocked, no reaction event
			TestTrue(TEXT("Damage GE applied while Enemy is already dead"),
				ApplyDamageWithTags(EnemyASC, EnemyASC, Enemy, SmallTags));
			TestEqual(TEXT("Already Dead Enemy sent NO additional reaction event"), EnemySmallEventCount, 1);
			TestEqual(TEXT("Enemy Big count remains 1 after hit to dead enemy"), EnemyBigEventCount, 1);
			TestEqual(TEXT("Enemy Launch count remains 1 after hit to dead enemy"), EnemyLaunchEventCount, 1);
		}
	}

	// -------------------------------------------------------------------------
	// SECTION 6: Landing-Deferred Enemy Stance Break Lifecycle (TODO-02C3H)
	// -------------------------------------------------------------------------
	{
		AEnemyCharacter* DeferralEnemy = FCombatAutomationFixture::SpawnPassiveEnemy(World, FTransform(FRotator::ZeroRotator, FVector(300.0f, 0.0f, 0.0f)));
		TestNotNull(TEXT("DeferralEnemy spawned successfully"), DeferralEnemy);
		if (DeferralEnemy)
		{
			UAbilitySystemComponent* DeferralASC = DeferralEnemy->GetAbilitySystemComponent();
			TestNotNull(TEXT("DeferralEnemy ASC valid"), DeferralASC);

			if (DeferralASC)
			{
				const FGameplayTag TagStanceBreakEvent = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Enemy.StanceBreak")), false);
				TestTrue(TEXT("Tag Event.Reaction.Enemy.StanceBreak is valid"), TagStanceBreakEvent.IsValid());

				int32 DeferralStanceBreakCount = 0;
				FDelegateHandle StanceBreakDelegateHandle = DeferralASC->GenericGameplayEventCallbacks.FindOrAdd(TagStanceBreakEvent).AddLambda(
					[&DeferralStanceBreakCount](const FGameplayEventData* Payload)
					{
						if (Payload)
						{
							DeferralStanceBreakCount++;
						}
					});

				int32 LaunchEventCount = 0;
				FDelegateHandle LaunchDelegateHandle = DeferralASC->GenericGameplayEventCallbacks.FindOrAdd(TagEventEnemyLaunch).AddLambda(
					[&LaunchEventCount](const FGameplayEventData* Payload)
					{
						if (Payload)
						{
							LaunchEventCount++;
						}
					});

				auto ResetEnemyState = [&]()
				{
					DeferralASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 100.0f);
					DeferralASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxHealthAttribute(), 100.0f);
					DeferralASC->SetNumericAttributeBase(UCharacterAttributeSet::GetPoiseAttribute(), 100.0f);
					DeferralASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxPoiseAttribute(), 100.0f);
				};

				// 6.1 Ordinary Poise=0 (outside launch): schedules next-tick timer and dispatches Stance Break
				{
					ResetEnemyState();
					DeferralStanceBreakCount = 0;
					LaunchEventCount = 0;

					// Reduce Poise to 0
					DeferralASC->SetNumericAttributeBase(UCharacterAttributeSet::GetPoiseAttribute(), 0.0f);
					TestTrue(TEXT("6.1: Ordinary Poise=0 schedules next-tick timer"), DeferralEnemy->HasPendingStanceBreakTimer());
					TestFalse(TEXT("6.1: Launch deferral is not active"), DeferralEnemy->IsLaunchStanceBreakDeferralActive());
					TestFalse(TEXT("6.1: Pending deferred intent is false"), DeferralEnemy->HasPendingDeferredStanceBreak());
					TestEqual(TEXT("6.1: Stance break event count is 0 before dispatch"), DeferralStanceBreakCount, 0);

					// Dispatch the pending timer
					DeferralEnemy->DispatchTestPendingStanceBreak();
					TestEqual(TEXT("6.1: Exactly 1 Stance Break event dispatched by ordinary timer"), DeferralStanceBreakCount, 1);
					TestFalse(TEXT("6.1: Timer flag cleared after dispatch"), DeferralEnemy->HasPendingStanceBreakTimer());
				}

				// 6.2 Order 1: Poise reaches 0 BEFORE Launch starts -> Begin clears timer & records deferred intent
				{
					ResetEnemyState();
					DeferralStanceBreakCount = 0;
					LaunchEventCount = 0;

					// 1. Poise reaches 0 -> timer created
					DeferralASC->SetNumericAttributeBase(UCharacterAttributeSet::GetPoiseAttribute(), 0.0f);
					TestTrue(TEXT("6.2: Timer created before launch starts"), DeferralEnemy->HasPendingStanceBreakTimer());

					// 2. Launch starts (Takeoff Montage active) -> Begin called
					DeferralEnemy->BeginLaunchStanceBreakDeferral();
					TestFalse(TEXT("6.2: Begin cleared the ordinary next-tick timer"), DeferralEnemy->HasPendingStanceBreakTimer());
					TestTrue(TEXT("6.2: Launch deferral is now active"), DeferralEnemy->IsLaunchStanceBreakDeferralActive());
					TestTrue(TEXT("6.2: Deferred stance break intent is recorded"), DeferralEnemy->HasPendingDeferredStanceBreak());
					TestEqual(TEXT("6.2: No Stance Break event emitted during Launch setup/flight"), DeferralStanceBreakCount, 0);

					// 3. Natural Landing Recovery completes
					DeferralEnemy->CompleteLaunchStanceBreakDeferral();
					TestEqual(TEXT("6.2: Natural completion emitted exactly 1 Stance Break event"), DeferralStanceBreakCount, 1);
					TestFalse(TEXT("6.2: Deferral active cleared"), DeferralEnemy->IsLaunchStanceBreakDeferralActive());
					TestFalse(TEXT("6.2: Deferred intent cleared"), DeferralEnemy->HasPendingDeferredStanceBreak());
				}

				// 6.3 Order 2: Launch starts BEFORE Poise reaches 0 -> Poise=0 callback records intent directly without timer
				{
					ResetEnemyState();
					DeferralStanceBreakCount = 0;
					LaunchEventCount = 0;

					// 1. Launch starts
					DeferralEnemy->BeginLaunchStanceBreakDeferral();
					TestTrue(TEXT("6.3: Launch deferral is active"), DeferralEnemy->IsLaunchStanceBreakDeferralActive());
					TestFalse(TEXT("6.3: No pending deferred intent yet"), DeferralEnemy->HasPendingDeferredStanceBreak());
					TestFalse(TEXT("6.3: No ordinary timer created"), DeferralEnemy->HasPendingStanceBreakTimer());

					// 2. Damage applied during launch reduces Poise to 0
					DeferralASC->SetNumericAttributeBase(UCharacterAttributeSet::GetPoiseAttribute(), 0.0f);
					TestTrue(TEXT("6.3: Deferred intent recorded by Poise attribute change"), DeferralEnemy->HasPendingDeferredStanceBreak());
					TestFalse(TEXT("6.3: Ordinary timer was NOT created while launch deferral is active"), DeferralEnemy->HasPendingStanceBreakTimer());
					TestEqual(TEXT("6.3: No Stance Break event emitted during flight"), DeferralStanceBreakCount, 0);

					// 3. Natural Landing Recovery completes
					DeferralEnemy->CompleteLaunchStanceBreakDeferral();
					TestEqual(TEXT("6.3: Natural completion emitted exactly 1 Stance Break event"), DeferralStanceBreakCount, 1);
					TestFalse(TEXT("6.3: Deferral active cleared"), DeferralEnemy->IsLaunchStanceBreakDeferralActive());
					TestFalse(TEXT("6.3: Deferred intent cleared"), DeferralEnemy->HasPendingDeferredStanceBreak());
				}

				// 6.4 Duplicate Begin / Complete calls are idempotent and do NOT duplicate events
				{
					ResetEnemyState();
					DeferralStanceBreakCount = 0;
					LaunchEventCount = 0;

					DeferralEnemy->BeginLaunchStanceBreakDeferral();
					DeferralASC->SetNumericAttributeBase(UCharacterAttributeSet::GetPoiseAttribute(), 0.0f);
					TestTrue(TEXT("6.4: Deferred intent set"), DeferralEnemy->HasPendingDeferredStanceBreak());

					// Duplicate Begin
					DeferralEnemy->BeginLaunchStanceBreakDeferral();
					TestTrue(TEXT("6.4: Deferred intent remains true after duplicate Begin"), DeferralEnemy->HasPendingDeferredStanceBreak());
					TestFalse(TEXT("6.4: No ordinary timer created after duplicate Begin"), DeferralEnemy->HasPendingStanceBreakTimer());

					// Complete once
					DeferralEnemy->CompleteLaunchStanceBreakDeferral();
					TestEqual(TEXT("6.4: First Complete emits 1 event"), DeferralStanceBreakCount, 1);

					// Duplicate Complete
					DeferralEnemy->CompleteLaunchStanceBreakDeferral();
					TestEqual(TEXT("6.4: Duplicate Complete does NOT emit another event"), DeferralStanceBreakCount, 1);
				}

				// 6.5 Poise recovery during flight clears deferred intent
				{
					ResetEnemyState();
					DeferralStanceBreakCount = 0;
					LaunchEventCount = 0;

					DeferralEnemy->BeginLaunchStanceBreakDeferral();
					DeferralASC->SetNumericAttributeBase(UCharacterAttributeSet::GetPoiseAttribute(), 0.0f);
					TestTrue(TEXT("6.5: Deferred intent set at zero Poise"), DeferralEnemy->HasPendingDeferredStanceBreak());

					// Poise restored to positive during flight
					DeferralASC->SetNumericAttributeBase(UCharacterAttributeSet::GetPoiseAttribute(), 50.0f);
					TestFalse(TEXT("6.5: Positive Poise transition cleared deferred intent"), DeferralEnemy->HasPendingDeferredStanceBreak());

					// Natural completion
					DeferralEnemy->CompleteLaunchStanceBreakDeferral();
					TestEqual(TEXT("6.5: No Stance Break emitted when Poise recovered during flight"), DeferralStanceBreakCount, 0);
				}

				// 6.6 AbortLaunchStanceBreakDeferral restores Poise to max and emits no Stance Break
				{
					ResetEnemyState();
					DeferralStanceBreakCount = 0;
					LaunchEventCount = 0;

					DeferralEnemy->BeginLaunchStanceBreakDeferral();
					DeferralASC->SetNumericAttributeBase(UCharacterAttributeSet::GetPoiseAttribute(), 0.0f);
					TestTrue(TEXT("6.6: Deferred intent set"), DeferralEnemy->HasPendingDeferredStanceBreak());

					// Abort (e.g. premature falling, interrupted, cancelled)
					DeferralEnemy->AbortLaunchStanceBreakDeferral();
					TestEqual(TEXT("6.6: Abort emits NO Stance Break event"), DeferralStanceBreakCount, 0);
					TestFalse(TEXT("6.6: Deferral active cleared after Abort"), DeferralEnemy->IsLaunchStanceBreakDeferralActive());
					TestFalse(TEXT("6.6: Deferred intent cleared after Abort"), DeferralEnemy->HasPendingDeferredStanceBreak());
					TestEqual(TEXT("6.6: Abort restored Poise to MaxPoise (100.0)"),
						DeferralASC->GetNumericAttribute(UCharacterAttributeSet::GetPoiseAttribute()), 100.0f);
				}

				// 6.7 Death and EndPlay clear deferral without recovery or late Stance Break
				{
					AEnemyCharacter* DeathEnemy = FCombatAutomationFixture::SpawnPassiveEnemy(World, FTransform(FRotator::ZeroRotator, FVector(400.0f, 0.0f, 0.0f)));
					TestNotNull(TEXT("6.7: DeathEnemy spawned successfully"), DeathEnemy);
					if (DeathEnemy)
					{
						UAbilitySystemComponent* DeathASC = DeathEnemy->GetAbilitySystemComponent();
						TestNotNull(TEXT("6.7: DeathEnemy ASC valid"), DeathASC);
						if (DeathASC)
						{
							int32 DeathStanceBreakCount = 0;
							FDelegateHandle DeathStanceBreakHandle = DeathASC->GenericGameplayEventCallbacks.FindOrAdd(TagStanceBreakEvent).AddLambda(
								[&DeathStanceBreakCount](const FGameplayEventData* Payload)
								{
									if (Payload)
									{
										DeathStanceBreakCount++;
									}
								});

							DeathASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 100.0f);
							DeathASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxHealthAttribute(), 100.0f);
							DeathASC->SetNumericAttributeBase(UCharacterAttributeSet::GetPoiseAttribute(), 100.0f);
							DeathASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxPoiseAttribute(), 100.0f);

							DeathEnemy->BeginLaunchStanceBreakDeferral();
							DeathASC->SetNumericAttributeBase(UCharacterAttributeSet::GetPoiseAttribute(), 0.0f);
							TestTrue(TEXT("6.7: Deferred intent set"), DeathEnemy->HasPendingDeferredStanceBreak());

							// Simulate Death
							DeathASC->AddLooseGameplayTag(TagDead);
							TestTrue(TEXT("6.7: Enemy is dead"), DeathEnemy->IsDead());
							TestFalse(TEXT("6.7: Death cleared deferred intent"), DeathEnemy->HasPendingDeferredStanceBreak());
							TestFalse(TEXT("6.7: Death cleared deferral active"), DeathEnemy->IsLaunchStanceBreakDeferralActive());

							// Late complete attempt
							DeathEnemy->CompleteLaunchStanceBreakDeferral();
							TestEqual(TEXT("6.7: No late Stance Break event after death"), DeathStanceBreakCount, 0);

							DeathASC->GenericGameplayEventCallbacks.FindOrAdd(TagStanceBreakEvent).Remove(DeathStanceBreakHandle);
						}

						DeathEnemy->Destroy();
					}
				}

				// 6.8 Launch that never activated does not pollute ordinary Poise route
				{
					ResetEnemyState();
					DeferralStanceBreakCount = 0;
					LaunchEventCount = 0;

					// Poise goes to 0
					DeferralASC->SetNumericAttributeBase(UCharacterAttributeSet::GetPoiseAttribute(), 0.0f);
					TestTrue(TEXT("6.8: Ordinary timer created"), DeferralEnemy->HasPendingStanceBreakTimer());

					// If an ability attempted to activate but failed before Takeoff montage (so Begin was never called, only Abort called)
					DeferralEnemy->AbortLaunchStanceBreakDeferral();
					TestTrue(TEXT("6.8: Ordinary timer remains scheduled after no-op Abort"), DeferralEnemy->HasPendingStanceBreakTimer());

					// Ordinary timer can still dispatch normally
					DeferralEnemy->DispatchTestPendingStanceBreak();
					TestEqual(TEXT("6.8: Ordinary Stance Break dispatched successfully"), DeferralStanceBreakCount, 1);
				}

				// 6.9 State.Action.HitReacting must be absent during dispatch on natural completion
				{
					ResetEnemyState();
					DeferralStanceBreakCount = 0;
					LaunchEventCount = 0;

					DeferralEnemy->BeginLaunchStanceBreakDeferral();
					DeferralASC->SetNumericAttributeBase(UCharacterAttributeSet::GetPoiseAttribute(), 0.0f);

					// Launch ability would add HitReacting while active, and remove it in Super::EndAbility before calling Complete
					DeferralASC->AddLooseGameplayTag(TagHitReacting);
					TestTrue(TEXT("6.9: HitReacting present during flight"), DeferralASC->HasMatchingGameplayTag(TagHitReacting));

					// Super::EndAbility() removes HitReacting
					DeferralASC->RemoveLooseGameplayTag(TagHitReacting);
					TestFalse(TEXT("6.9: HitReacting is absent before Complete is called"), DeferralASC->HasMatchingGameplayTag(TagHitReacting));

					DeferralEnemy->CompleteLaunchStanceBreakDeferral();
					TestEqual(TEXT("6.9: Exactly 1 Stance Break dispatched when HitReacting is absent"), DeferralStanceBreakCount, 1);
				}

				// 6.10 Real GameplayEffect Spec transaction with Modifier Ordering (P1 Coverage)
				// 6.10a: Single GE with Poise modifier first (index 0), Health modifier second (index 1)
				{
					ResetEnemyState();
					DeferralStanceBreakCount = 0;
					LaunchEventCount = 0;

					// Apply UTestLaunchDamageGE_PoiseFirst (Poise -100 at index 0, Health -25 at index 1)
					FGameplayEffectContextHandle Context = DeferralASC->MakeEffectContext();
					Context.AddInstigator(DeferralEnemy, DeferralEnemy);
					FGameplayEffectSpecHandle SpecHandle = DeferralASC->MakeOutgoingSpec(UTestLaunchDamageGE_PoiseFirst::StaticClass(), 1, Context);
					TestTrue(TEXT("6.10a: SpecHandle valid"), SpecHandle.IsValid() && SpecHandle.Data.IsValid());
					if (SpecHandle.IsValid() && SpecHandle.Data.IsValid())
					{
						FGameplayTagContainer LaunchTags;
						LaunchTags.AddTag(TagDataLaunch);
						SpecHandle.Data->AppendDynamicAssetTags(LaunchTags);

						TestTrue(TEXT("6.10a: ApplyGameplayEffectSpecToSelf succeeded"),
							DeferralASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get()).WasSuccessfullyApplied());

						// Verify Poise reached 0 and Health reached 75
						TestEqual(TEXT("6.10a: Poise reached 0"), DeferralASC->GetNumericAttribute(UCharacterAttributeSet::GetPoiseAttribute()), 0.0f);
						TestEqual(TEXT("6.10a: Health reached 75"), DeferralASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute()), 75.0f);

						// Verify Launch event WAS dispatched because Poise was broken in the SAME GE transaction
						TestEqual(TEXT("6.10a: Launch reaction event dispatched"), LaunchEventCount, 1);

						// Simulate Launch ability start
						DeferralEnemy->BeginLaunchStanceBreakDeferral();
						TestFalse(TEXT("6.10a: Ordinary next-tick timer cleared by Begin"), DeferralEnemy->HasPendingStanceBreakTimer());
						TestTrue(TEXT("6.10a: Deferred stance break intent recorded"), DeferralEnemy->HasPendingDeferredStanceBreak());

						// Simulate natural LandingRecovery completion
						DeferralEnemy->CompleteLaunchStanceBreakDeferral();
						TestEqual(TEXT("6.10a: Exactly 1 Stance Break dispatched on landing"), DeferralStanceBreakCount, 1);
					}
				}

				// 6.10b: Single GE with Health modifier first (index 0), Poise modifier second (index 1)
				{
					ResetEnemyState();
					DeferralStanceBreakCount = 0;
					LaunchEventCount = 0;

					// Apply UTestLaunchDamageGE_HealthFirst (Health -25 at index 0, Poise -100 at index 1)
					FGameplayEffectContextHandle Context = DeferralASC->MakeEffectContext();
					Context.AddInstigator(DeferralEnemy, DeferralEnemy);
					FGameplayEffectSpecHandle SpecHandle = DeferralASC->MakeOutgoingSpec(UTestLaunchDamageGE_HealthFirst::StaticClass(), 1, Context);
					TestTrue(TEXT("6.10b: SpecHandle valid"), SpecHandle.IsValid() && SpecHandle.Data.IsValid());
					if (SpecHandle.IsValid() && SpecHandle.Data.IsValid())
					{
						FGameplayTagContainer LaunchTags;
						LaunchTags.AddTag(TagDataLaunch);
						SpecHandle.Data->AppendDynamicAssetTags(LaunchTags);

						TestTrue(TEXT("6.10b: ApplyGameplayEffectSpecToSelf succeeded"),
							DeferralASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get()).WasSuccessfullyApplied());

						TestEqual(TEXT("6.10b: Poise reached 0"), DeferralASC->GetNumericAttribute(UCharacterAttributeSet::GetPoiseAttribute()), 0.0f);
						TestEqual(TEXT("6.10b: Health reached 75"), DeferralASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute()), 75.0f);
						TestEqual(TEXT("6.10b: Launch reaction event dispatched"), LaunchEventCount, 1);

						DeferralEnemy->BeginLaunchStanceBreakDeferral();
						TestFalse(TEXT("6.10b: Ordinary next-tick timer cleared by Begin"), DeferralEnemy->HasPendingStanceBreakTimer());
						TestTrue(TEXT("6.10b: Deferred stance break intent recorded"), DeferralEnemy->HasPendingDeferredStanceBreak());

						DeferralEnemy->CompleteLaunchStanceBreakDeferral();
						TestEqual(TEXT("6.10b: Exactly 1 Stance Break dispatched on landing"), DeferralStanceBreakCount, 1);
					}
				}

				// 6.10c Negative Test: Enemy ALREADY at Poise=0 receives subsequent Health-only Launch hit
				// -> Launch is REJECTED, ordinary pending timer is preserved, exactly 1 ordinary Stance Break
				{
					ResetEnemyState();
					DeferralStanceBreakCount = 0;
					LaunchEventCount = 0;

					// 1. Initial hit reduces Poise to 0 (ordinary poise break)
					DeferralASC->SetNumericAttributeBase(UCharacterAttributeSet::GetPoiseAttribute(), 0.0f);
					TestTrue(TEXT("6.10c: Ordinary timer created for poise break"), DeferralEnemy->HasPendingStanceBreakTimer());

					// 2. Subsequent Health-only Launch effect arrives while Poise is already broken
					FGameplayEffectContextHandle Context = DeferralASC->MakeEffectContext();
					Context.AddInstigator(DeferralEnemy, DeferralEnemy);
					FGameplayEffectSpecHandle SpecHandle = DeferralASC->MakeOutgoingSpec(UTestProjectileDamageGE::StaticClass(), 1, Context);
					TestTrue(TEXT("6.10c: SpecHandle valid"), SpecHandle.IsValid() && SpecHandle.Data.IsValid());
					if (SpecHandle.IsValid() && SpecHandle.Data.IsValid())
					{
						FGameplayTagContainer LaunchTags;
						LaunchTags.AddTag(TagDataLaunch);
						SpecHandle.Data->AppendDynamicAssetTags(LaunchTags);

						TestTrue(TEXT("6.10c: ApplyGameplayEffectSpecToSelf succeeded"),
							DeferralASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get()).WasSuccessfullyApplied());

						// Health took damage
						TestEqual(TEXT("6.10c: Health took damage to 75"), DeferralASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute()), 75.0f);

						// Assert Launch event is 0 (REJECTED because poise was already broken from prior hit)
						TestEqual(TEXT("6.10c: Launch event REJECTED on already poise-broken enemy"), LaunchEventCount, 0);

						// Assert ordinary timer was NOT cancelled by the rejected Launch
						TestTrue(TEXT("6.10c: Ordinary next-tick timer remains intact"), DeferralEnemy->HasPendingStanceBreakTimer());
						TestFalse(TEXT("6.10c: Launch deferral was NOT activated"), DeferralEnemy->IsLaunchStanceBreakDeferralActive());
						TestFalse(TEXT("6.10c: No deferred stance break intent recorded"), DeferralEnemy->HasPendingDeferredStanceBreak());

						// Ordinary timer dispatches exactly 1 Stance Break
						DeferralEnemy->DispatchTestPendingStanceBreak();
						TestEqual(TEXT("6.10c: Exactly 1 ordinary Stance Break dispatched"), DeferralStanceBreakCount, 1);
					}
				}

				// 6.10d Real Poise-only GE: Poise break schedules ordinary timer, separate Health-only Launch is rejected
				{
					ResetEnemyState();
					DeferralStanceBreakCount = 0;
					LaunchEventCount = 0;

					// 1. Apply real UTestPoiseDamageOnlyGE (reduces Poise to 0 without Health damage)
					FGameplayEffectContextHandle Context = DeferralASC->MakeEffectContext();
					Context.AddInstigator(DeferralEnemy, DeferralEnemy);
					FGameplayEffectSpecHandle PoiseSpecHandle = DeferralASC->MakeOutgoingSpec(UTestPoiseDamageOnlyGE::StaticClass(), 1, Context);
					TestTrue(TEXT("6.10d: PoiseSpecHandle valid"), PoiseSpecHandle.IsValid() && PoiseSpecHandle.Data.IsValid());
					if (PoiseSpecHandle.IsValid() && PoiseSpecHandle.Data.IsValid())
					{
						TestTrue(TEXT("6.10d: PoiseDamageOnlyGE applied"),
							DeferralASC->ApplyGameplayEffectSpecToSelf(*PoiseSpecHandle.Data.Get()).WasSuccessfullyApplied());

						TestEqual(TEXT("6.10d: Poise reached 0"), DeferralASC->GetNumericAttribute(UCharacterAttributeSet::GetPoiseAttribute()), 0.0f);
						TestTrue(TEXT("6.10d: Ordinary timer scheduled for Poise-only break"), DeferralEnemy->HasPendingStanceBreakTimer());

						// 2. Subsequent separate Health-only Launch effect applied while timer is scheduled
						FGameplayEffectSpecHandle LaunchSpecHandle = DeferralASC->MakeOutgoingSpec(UTestProjectileDamageGE::StaticClass(), 1, Context);
						if (LaunchSpecHandle.IsValid() && LaunchSpecHandle.Data.IsValid())
						{
							FGameplayTagContainer LaunchTags;
							LaunchTags.AddTag(TagDataLaunch);
							LaunchSpecHandle.Data->AppendDynamicAssetTags(LaunchTags);

							TestTrue(TEXT("6.10d: Health-only Launch GE applied"),
								DeferralASC->ApplyGameplayEffectSpecToSelf(*LaunchSpecHandle.Data.Get()).WasSuccessfullyApplied());

							// Launch MUST be rejected because the cached Poise-breaking source belongs to PoiseSpec, NOT LaunchSpec.
							TestEqual(TEXT("6.10d: Health-only Launch rejected against different Poise-break spec"), LaunchEventCount, 0);
							TestTrue(TEXT("6.10d: Ordinary timer was NOT cancelled by rejected Launch"), DeferralEnemy->HasPendingStanceBreakTimer());
							TestFalse(TEXT("6.10d: Launch deferral was NOT activated"), DeferralEnemy->IsLaunchStanceBreakDeferralActive());
						}

						// 3. Dispatch the ordinary timer, which clears the cached Poise-breaking source and emits Stance Break.
					DeferralEnemy->DispatchTestPendingStanceBreak();
					TestEqual(TEXT("6.10d: Exactly 1 Stance Break event dispatched by timer"), DeferralStanceBreakCount, 1);
					TestFalse(TEXT("6.10d: Timer cleared"), DeferralEnemy->HasPendingStanceBreakTimer());
				}
			}

				// 6.10e Same GE definition with a different EffectContext must not impersonate the pending Poise break.
				{
					ResetEnemyState();
					DeferralStanceBreakCount = 0;
					LaunchEventCount = 0;

					FGameplayEffectContextHandle PoiseBreakContext = DeferralASC->MakeEffectContext();
					PoiseBreakContext.AddInstigator(DeferralEnemy, DeferralEnemy);
					FGameplayEffectSpecHandle PoiseBreakSpecHandle = DeferralASC->MakeOutgoingSpec(UTestLaunchDamageGE_PoiseFirst::StaticClass(), 1, PoiseBreakContext);
					TestTrue(TEXT("6.10e: Poise-break SpecHandle valid"), PoiseBreakSpecHandle.IsValid() && PoiseBreakSpecHandle.Data.IsValid());
					if (PoiseBreakSpecHandle.IsValid() && PoiseBreakSpecHandle.Data.IsValid())
					{
						FGameplayTagContainer LaunchTags;
						LaunchTags.AddTag(TagDataLaunch);
						PoiseBreakSpecHandle.Data->AppendDynamicAssetTags(LaunchTags);
						TestTrue(TEXT("6.10e: Poise-break Launch GE applied"),
							DeferralASC->ApplyGameplayEffectSpecToSelf(*PoiseBreakSpecHandle.Data.Get()).WasSuccessfullyApplied());
						TestEqual(TEXT("6.10e: Initial same-transaction Launch dispatched"), LaunchEventCount, 1);
						TestTrue(TEXT("6.10e: Ordinary timer scheduled for initial Launch break"), DeferralEnemy->HasPendingStanceBreakTimer());

						LaunchEventCount = 0;
						FGameplayEffectContextHandle LaterContext = DeferralASC->MakeEffectContext();
						LaterContext.AddInstigator(DeferralEnemy, DeferralEnemy);
						FGameplayEffectSpecHandle LaterSpecHandle = DeferralASC->MakeOutgoingSpec(UTestLaunchDamageGE_PoiseFirst::StaticClass(), 1, LaterContext);
						TestTrue(TEXT("6.10e: Later SpecHandle valid"), LaterSpecHandle.IsValid() && LaterSpecHandle.Data.IsValid());
						if (LaterSpecHandle.IsValid() && LaterSpecHandle.Data.IsValid())
						{
							LaterSpecHandle.Data->AppendDynamicAssetTags(LaunchTags);
							TestTrue(TEXT("6.10e: Same-definition later Launch GE applied"),
								DeferralASC->ApplyGameplayEffectSpecToSelf(*LaterSpecHandle.Data.Get()).WasSuccessfullyApplied());
							TestEqual(TEXT("6.10e: Same definition with a fresh context is rejected"), LaunchEventCount, 0);
							TestTrue(TEXT("6.10e: Rejected same-definition later Launch retains ordinary timer"), DeferralEnemy->HasPendingStanceBreakTimer());
							TestFalse(TEXT("6.10e: Rejected same-definition later Launch does not defer"), DeferralEnemy->IsLaunchStanceBreakDeferralActive());
						}

						DeferralEnemy->DispatchTestPendingStanceBreak();
						TestEqual(TEXT("6.10e: Ordinary timer dispatches exactly one Stance Break"), DeferralStanceBreakCount, 1);
					}
				}

				// 6.10f The same Definition and shared EffectContext still cannot impersonate a completed Poise break transaction.
				{
					ResetEnemyState();
					DeferralStanceBreakCount = 0;
					LaunchEventCount = 0;

					FGameplayEffectContextHandle SharedContext = DeferralASC->MakeEffectContext();
					SharedContext.AddInstigator(DeferralEnemy, DeferralEnemy);
					FGameplayEffectSpecHandle InitialSpecHandle = DeferralASC->MakeOutgoingSpec(UTestLaunchDamageGE_PoiseFirst::StaticClass(), 1, SharedContext);
					TestTrue(TEXT("6.10f: Initial SpecHandle valid"), InitialSpecHandle.IsValid() && InitialSpecHandle.Data.IsValid());
					if (InitialSpecHandle.IsValid() && InitialSpecHandle.Data.IsValid())
					{
						FGameplayTagContainer LaunchTags;
						LaunchTags.AddTag(TagDataLaunch);
						InitialSpecHandle.Data->AppendDynamicAssetTags(LaunchTags);
						TestTrue(TEXT("6.10f: Initial Poise-break Launch GE applied"),
							DeferralASC->ApplyGameplayEffectSpecToSelf(*InitialSpecHandle.Data.Get()).WasSuccessfullyApplied());
						TestEqual(TEXT("6.10f: Initial same-transaction Launch dispatched"), LaunchEventCount, 1);
						TestTrue(TEXT("6.10f: Ordinary timer scheduled for initial Launch break"), DeferralEnemy->HasPendingStanceBreakTimer());

						LaunchEventCount = 0;
						FGameplayEffectSpecHandle ReusedContextSpecHandle = DeferralASC->MakeOutgoingSpec(UTestLaunchDamageGE_PoiseFirst::StaticClass(), 1, SharedContext);
						TestTrue(TEXT("6.10f: Reused-context SpecHandle valid"), ReusedContextSpecHandle.IsValid() && ReusedContextSpecHandle.Data.IsValid());
						if (ReusedContextSpecHandle.IsValid() && ReusedContextSpecHandle.Data.IsValid())
						{
							ReusedContextSpecHandle.Data->AppendDynamicAssetTags(LaunchTags);
							TestTrue(TEXT("6.10f: Same-definition shared-context later GE applied"),
								DeferralASC->ApplyGameplayEffectSpecToSelf(*ReusedContextSpecHandle.Data.Get()).WasSuccessfullyApplied());
							TestEqual(TEXT("6.10f: Same definition and shared context are rejected after the initial Poise break"), LaunchEventCount, 0);
							TestTrue(TEXT("6.10f: Rejected shared-context later Launch retains ordinary timer"), DeferralEnemy->HasPendingStanceBreakTimer());
							TestFalse(TEXT("6.10f: Rejected shared-context later Launch does not defer"), DeferralEnemy->IsLaunchStanceBreakDeferralActive());
						}

						DeferralEnemy->DispatchTestPendingStanceBreak();
						TestEqual(TEXT("6.10f: Ordinary timer dispatches exactly one Stance Break"), DeferralStanceBreakCount, 1);
					}
				}

				// 6.11 Synthetic UEnemyLaunchReactionAbility EndAbility Bridge Test (P3 Coverage)
				{
					ResetEnemyState();
					DeferralStanceBreakCount = 0;
					LaunchEventCount = 0;

					UEnemyLaunchReactionAbility* LaunchAbilityInstance = NewObject<UEnemyLaunchReactionAbility>(DeferralEnemy);
					TestNotNull(TEXT("6.11: LaunchAbilityInstance created"), LaunchAbilityInstance);
					if (LaunchAbilityInstance)
					{
						FGameplayAbilityActorInfo ActorInfo;
						ActorInfo.InitFromActor(DeferralEnemy, DeferralEnemy, DeferralASC);
						FGameplayAbilityActivationInfo ActivationInfo;
						FGameplayAbilitySpecHandle SpecHandle;

						// 6.11a: Natural LandingRecovery -> triggers CompleteLaunchStanceBreakDeferral
						DeferralEnemy->BeginLaunchStanceBreakDeferral();
						DeferralASC->SetNumericAttributeBase(UCharacterAttributeSet::GetPoiseAttribute(), 0.0f);
						TestTrue(TEXT("6.11a: Intent recorded"), DeferralEnemy->HasPendingDeferredStanceBreak());

						LaunchAbilityInstance->SetTestLandingRecoveryCompletedNaturally(true);
						LaunchAbilityInstance->EndAbility(SpecHandle, &ActorInfo, ActivationInfo, false, false);
						TestEqual(TEXT("6.11a: Natural EndAbility called CompleteLaunchStanceBreakDeferral emitting 1 event"), DeferralStanceBreakCount, 1);

						// 6.11b: Interrupted / Aborted EndAbility -> triggers AbortLaunchStanceBreakDeferral
						ResetEnemyState();
						DeferralStanceBreakCount = 0;
						LaunchEventCount = 0;
						DeferralEnemy->BeginLaunchStanceBreakDeferral();
						DeferralASC->SetNumericAttributeBase(UCharacterAttributeSet::GetPoiseAttribute(), 0.0f);

						UEnemyLaunchReactionAbility* AbortedAbilityInstance = NewObject<UEnemyLaunchReactionAbility>(DeferralEnemy);
						TestNotNull(TEXT("6.11b: AbortedAbilityInstance created"), AbortedAbilityInstance);
						if (AbortedAbilityInstance)
						{
							AbortedAbilityInstance->SetTestLandingRecoveryCompletedNaturally(false);
							AbortedAbilityInstance->EndAbility(SpecHandle, &ActorInfo, ActivationInfo, false, true);
							TestEqual(TEXT("6.11b: Aborted EndAbility emitted 0 events"), DeferralStanceBreakCount, 0);
							TestEqual(TEXT("6.11b: Aborted EndAbility restored Poise to 100.0"),
								DeferralASC->GetNumericAttribute(UCharacterAttributeSet::GetPoiseAttribute()), 100.0f);
						}
					}
				}

				DeferralASC->GenericGameplayEventCallbacks.FindOrAdd(TagStanceBreakEvent).Remove(StanceBreakDelegateHandle);
				DeferralASC->GenericGameplayEventCallbacks.FindOrAdd(TagEventEnemyLaunch).Remove(LaunchDelegateHandle);
			}

			DeferralEnemy->Destroy();
		}
	}

	Player->Destroy();
	Enemy->Destroy();

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
