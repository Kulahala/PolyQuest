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
#include "Combat/Reaction/HitReactionImpactResolver.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
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

			TestEqual(TEXT("PlayerLaunch BlockAbilitiesWithTag has exactly 11 tags"),
				PlayerLaunchCDO->GetTestBlockAbilitiesWithTag().Num(), 11);
			TestEqual(TEXT("PlayerLaunch AbilitiesToCancel has exactly 11 tags"),
				PlayerLaunchCDO->GetTestAbilitiesToCancel().Num(), 11);

			for (const FName& ActionTagName : ExpectedTargetActionTagNames)
			{
				const FGameplayTag ActionTag = FGameplayTag::RequestGameplayTag(ActionTagName, false);
				TestTrue(FString::Printf(TEXT("PlayerLaunch BlockAbilitiesWithTag contains '%s'"), *ActionTagName.ToString()),
					PlayerLaunchCDO->GetTestBlockAbilitiesWithTag().HasTagExact(ActionTag));
				TestTrue(FString::Printf(TEXT("PlayerLaunch AbilitiesToCancel contains '%s'"), *ActionTagName.ToString()),
					PlayerLaunchCDO->GetTestAbilitiesToCancel().HasTagExact(ActionTag));
			}

			TestEqual(TEXT("PlayerLaunch default horizontal speed is 450.0"), PlayerLaunchCDO->GetLaunchHorizontalSpeed(), 450.0f);
			TestEqual(TEXT("PlayerLaunch default vertical speed is 550.0"), PlayerLaunchCDO->GetLaunchVerticalSpeed(), 550.0f);
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

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APlayerCharacter* Player = World->SpawnActor<APlayerCharacter>(APlayerCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	TestNotNull(TEXT("Player spawned successfully"), Player);
	if (Player)
	{
		Player->DispatchBeginPlay();
	}

	AEnemyCharacter* Enemy = World->SpawnActor<AEnemyCharacter>(AEnemyCharacter::StaticClass(), FVector(100.0f, 0.0f, 0.0f), FRotator::ZeroRotator, SpawnParams);
	TestNotNull(TEXT("Enemy spawned successfully"), Enemy);
	if (Enemy)
	{
		Enemy->DispatchBeginPlay();
	}

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

		// 4.8 TryBuildLaunchVelocity Pure Function Tests
		{
			FVector OutVel = FVector::ZeroVector;

			// 4.8a Front attacker (1, 0, 0) with Yaw=0 -> Launch away = (-1, 0, 0) * 450, Z = 550
			TestTrue(TEXT("TryBuildLaunchVelocity succeeds for front attacker at Yaw=0"),
				FHitReactionImpactResolver::TryBuildLaunchVelocity(FVector(1.0f, 0.0f, 0.0f), 0.0f, 450.0f, 550.0f, OutVel));
			TestTrue(TEXT("Front attacker at Yaw=0 produces velocity (-450, 0, 550)"),
				OutVel.Equals(FVector(-450.0f, 0.0f, 550.0f), 0.01f));

			// 4.8b Front attacker (1, 0, 0) with Yaw=90 -> World away direction = (0, -1, 0)
			TestTrue(TEXT("TryBuildLaunchVelocity succeeds for front attacker at Yaw=90"),
				FHitReactionImpactResolver::TryBuildLaunchVelocity(FVector(1.0f, 0.0f, 0.0f), 90.0f, 450.0f, 550.0f, OutVel));
			TestTrue(TEXT("Front attacker at Yaw=90 produces velocity (0, -450, 550)"),
				OutVel.Equals(FVector(0.0f, -450.0f, 550.0f), 0.01f));

			// 4.8c Back attacker (-1, 0, 0) with Yaw=0 -> Launch away = (+1, 0, 0) * 450, Z = 550
			TestTrue(TEXT("TryBuildLaunchVelocity succeeds for back attacker at Yaw=0"),
				FHitReactionImpactResolver::TryBuildLaunchVelocity(FVector(-1.0f, 0.0f, 0.0f), 0.0f, 450.0f, 550.0f, OutVel));
			TestTrue(TEXT("Back attacker at Yaw=0 produces velocity (+450, 0, 550)"),
				OutVel.Equals(FVector(450.0f, 0.0f, 550.0f), 0.01f));

			// 4.8d Non-unit vector with non-zero Z -> planarized and normalized
			TestTrue(TEXT("TryBuildLaunchVelocity succeeds for unnormalized vector with Z component"),
				FHitReactionImpactResolver::TryBuildLaunchVelocity(FVector(10.0f, 0.0f, 50.0f), 0.0f, 450.0f, 550.0f, OutVel));
			TestTrue(TEXT("Unnormalized vector produces correct normalized velocity (-450, 0, 550)"),
				OutVel.Equals(FVector(-450.0f, 0.0f, 550.0f), 0.01f));

			// 4.8e Fail-Closed: Zero direction vector
			TestFalse(TEXT("Zero direction returns false"),
				FHitReactionImpactResolver::TryBuildLaunchVelocity(FVector::ZeroVector, 0.0f, 450.0f, 550.0f, OutVel));
			TestTrue(TEXT("Zero direction resets OutVelocity to ZeroVector"), OutVel.IsZero());

			// 4.8f Fail-Closed: NaN direction
			TestFalse(TEXT("NaN direction returns false"),
				FHitReactionImpactResolver::TryBuildLaunchVelocity(FVector(NAN, 0.0f, 0.0f), 0.0f, 450.0f, 550.0f, OutVel));
			TestTrue(TEXT("NaN direction resets OutVelocity to ZeroVector"), OutVel.IsZero());

			// 4.8g Fail-Closed: NaN Yaw
			TestFalse(TEXT("NaN Yaw returns false"),
				FHitReactionImpactResolver::TryBuildLaunchVelocity(FVector(1.0f, 0.0f, 0.0f), NAN, 450.0f, 550.0f, OutVel));
			TestTrue(TEXT("NaN Yaw resets OutVelocity to ZeroVector"), OutVel.IsZero());

			// 4.8h Fail-Closed: Zero / negative / non-finite HorizontalSpeed
			TestFalse(TEXT("Zero HorizontalSpeed returns false"),
				FHitReactionImpactResolver::TryBuildLaunchVelocity(FVector(1.0f, 0.0f, 0.0f), 0.0f, 0.0f, 550.0f, OutVel));
			TestFalse(TEXT("Negative HorizontalSpeed returns false"),
				FHitReactionImpactResolver::TryBuildLaunchVelocity(FVector(1.0f, 0.0f, 0.0f), 0.0f, -450.0f, 550.0f, OutVel));
			TestFalse(TEXT("NaN HorizontalSpeed returns false"),
				FHitReactionImpactResolver::TryBuildLaunchVelocity(FVector(1.0f, 0.0f, 0.0f), 0.0f, NAN, 550.0f, OutVel));

			// 4.8i Fail-Closed: Zero / negative / non-finite VerticalSpeed
			TestFalse(TEXT("Zero VerticalSpeed returns false"),
				FHitReactionImpactResolver::TryBuildLaunchVelocity(FVector(1.0f, 0.0f, 0.0f), 0.0f, 450.0f, 0.0f, OutVel));
			TestFalse(TEXT("Negative VerticalSpeed returns false"),
				FHitReactionImpactResolver::TryBuildLaunchVelocity(FVector(1.0f, 0.0f, 0.0f), 0.0f, 450.0f, -550.0f, OutVel));
			TestFalse(TEXT("NaN VerticalSpeed returns false"),
				FHitReactionImpactResolver::TryBuildLaunchVelocity(FVector(1.0f, 0.0f, 0.0f), 0.0f, 450.0f, NAN, OutVel));
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

	Player->Destroy();
	Enemy->Destroy();

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
