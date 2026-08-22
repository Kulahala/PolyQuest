#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystem/Abilities/EnemyHitReactionAbility.h"
#include "AbilitySystem/Abilities/EnemySmallHitReactionAbility.h"
#include "AbilitySystem/Abilities/EnemyStanceBreakAbility.h"
#include "AbilitySystem/Abilities/PlayerGuardBreakAbility.h"
#include "AbilitySystem/Abilities/PlayerSmallHitReactionAbility.h"
#include "AbilitySystem/CharacterAttributeSet.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Character/Player/PlayerCharacter.h"
#include "Combat/Reaction/HitReactionClassifier.h"
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
	const FGameplayTag TagAbilityEnemySmall = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Reaction.Enemy.Small")), false);
	const FGameplayTag TagEventEnemySmall = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Enemy.Small")), false);
	const FGameplayTag TagAbilityEnemyBig = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Reaction.Enemy.Big")), false);
	const FGameplayTag TagEventEnemyBig = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Enemy.Big")), false);
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
	TestTrue(TEXT("Tag Ability.Reaction.Enemy.Small is valid"), TagAbilityEnemySmall.IsValid());
	TestTrue(TEXT("Tag Event.Reaction.Enemy.Small is valid"), TagEventEnemySmall.IsValid());
	TestTrue(TEXT("Tag Ability.Reaction.Enemy.Big is valid"), TagAbilityEnemyBig.IsValid());
	TestTrue(TEXT("Tag Event.Reaction.Enemy.Big is valid"), TagEventEnemyBig.IsValid());
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

	// 2.2 UEnemySmallHitReactionAbility CDO checks
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

	// 2.3 UEnemyHitReactionAbility CDO checks (Enemy Big)
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
		}
	}

	// 2.4 Cancellation matrix on Stance Break & Guard Break
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
		}

		const UPlayerGuardBreakAbility* GuardBreakCDO = UPlayerGuardBreakAbility::StaticClass()->GetDefaultObject<UPlayerGuardBreakAbility>();
		TestNotNull(TEXT("UPlayerGuardBreakAbility CDO exists"), GuardBreakCDO);
		if (GuardBreakCDO)
		{
			TestTrue(TEXT("GuardBreak cancels PlayerSmall (Ability.Reaction.Player.Small)"),
				GuardBreakCDO->GetAbilitiesToCancel().HasTagExact(TagAbilityPlayerSmall));
			TestTrue(TEXT("GuardBreak cancels Guard (Ability.Defense.Guard)"),
				GuardBreakCDO->GetAbilitiesToCancel().HasTagExact(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Defense.Guard")), false)));
		}
	}

	// -------------------------------------------------------------------------
	// SECTION 3: Target-Side Health Attribute Change & Event Dispatch
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

	// Helper lambda to apply damage with custom dynamic asset tags
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

	// 3.1 Player Character Health Dispatch Tests
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		APlayerCharacter* Player = World->SpawnActor<APlayerCharacter>(APlayerCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
		TestNotNull(TEXT("Player spawned successfully"), Player);

		if (Player)
		{
			Player->DispatchBeginPlay();
			UAbilitySystemComponent* PlayerASC = Player->GetAbilitySystemComponent();
			TestNotNull(TEXT("Player ASC valid"), PlayerASC);

			if (PlayerASC)
			{
				PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 100.0f);
				PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxHealthAttribute(), 100.0f);

				int32 PlayerSmallEventCount = 0;
				float LastEventMagnitude = 0.0f;
				const AActor* LastInstigator = nullptr;

				PlayerASC->GenericGameplayEventCallbacks.FindOrAdd(TagEventPlayerSmall).AddLambda(
					[&PlayerSmallEventCount, &LastEventMagnitude, &LastInstigator](const FGameplayEventData* Payload)
					{
						if (Payload)
						{
							PlayerSmallEventCount++;
							LastEventMagnitude = Payload->EventMagnitude;
							LastInstigator = Payload->Instigator.Get();
						}
					});

				// 3.1a Apply Damage GE with Data.Reaction.Small -> dispatches Event.Reaction.Player.Small
				FGameplayTagContainer SmallTags;
				SmallTags.AddTag(TagDataSmall);
				TestTrue(TEXT("Damage GE with Data.Reaction.Small applied to Player"),
					ApplyDamageWithTags(PlayerASC, PlayerASC, Player, SmallTags));
				TestEqual(TEXT("Player received 1 Small reaction event"), PlayerSmallEventCount, 1);
				TestEqual(TEXT("EventMagnitude matches Health delta (25.0)"), LastEventMagnitude, 25.0f);
				TestEqual(TEXT("Instigator is Player"), LastInstigator, (const AActor*)Player);

				// Restore Health to 100 for non-lethal branch isolation
				PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 100.0f);

				// 3.1b Apply Damage GE with Data.Reaction.Big -> Player Big is a legal C3E no-op
				FGameplayTagContainer BigTags;
				BigTags.AddTag(TagDataBig);
				TestTrue(TEXT("Damage GE with Data.Reaction.Big applied to Player"),
					ApplyDamageWithTags(PlayerASC, PlayerASC, Player, BigTags));
				TestEqual(TEXT("Player received NO additional reaction event for Big"), PlayerSmallEventCount, 1);

				// Restore Health to 100
				PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 100.0f);

				// 3.1c Apply Damage GE with Data.Reaction.Launch -> Player Launch is a legal C3E no-op
				FGameplayTagContainer LaunchTags;
				LaunchTags.AddTag(TagDataLaunch);
				TestTrue(TEXT("Damage GE with Data.Reaction.Launch applied to Player"),
					ApplyDamageWithTags(PlayerASC, PlayerASC, Player, LaunchTags));
				TestEqual(TEXT("Player received NO additional reaction event for Launch"), PlayerSmallEventCount, 1);

				// Restore Health to 100
				PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 100.0f);

				// 3.1d Apply Damage GE with 0 reaction tags -> None (no event)
				FGameplayTagContainer NoReactionTags;
				TestTrue(TEXT("Damage GE with 0 reaction tags applied to Player"),
					ApplyDamageWithTags(PlayerASC, PlayerASC, Player, NoReactionTags));
				TestEqual(TEXT("Player received NO reaction event for unclassified GE"), PlayerSmallEventCount, 1);

				// Restore Health to 100
				PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 100.0f);

				// 3.1e Apply Damage GE with multi-tier invalid tags (Small + Big) -> Fail-Closed, no event
				FGameplayTagContainer InvalidTags;
				InvalidTags.AddTag(TagDataSmall);
				InvalidTags.AddTag(TagDataBig);
				TestTrue(TEXT("Damage GE with invalid multi-tier tags applied to Player"),
					ApplyDamageWithTags(PlayerASC, PlayerASC, Player, InvalidTags));
				TestEqual(TEXT("Player received NO reaction event for invalid multi-tier GE"), PlayerSmallEventCount, 1);

				// Restore Health to 100
				PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 100.0f);

				// 3.1f Apply Damage GE when Player is Stunned -> blocked, no event
				PlayerASC->AddLooseGameplayTag(TagStunned);
				TestTrue(TEXT("Damage GE with Small applied while Player is Stunned"),
					ApplyDamageWithTags(PlayerASC, PlayerASC, Player, SmallTags));
				TestEqual(TEXT("Stunned Player received NO reaction event"), PlayerSmallEventCount, 1);
				PlayerASC->RemoveLooseGameplayTag(TagStunned);

				// Restore Health to 100
				PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 100.0f);

				// 3.1g Apply Damage GE when Player already has Dead tag -> blocked, no event
				PlayerASC->AddLooseGameplayTag(TagDead);
				TestTrue(TEXT("Damage GE with Small applied while Player has Dead tag"),
					ApplyDamageWithTags(PlayerASC, PlayerASC, Player, SmallTags));
				TestEqual(TEXT("Dead Player received NO reaction event"), PlayerSmallEventCount, 1);
				PlayerASC->RemoveLooseGameplayTag(TagDead);

				// 3.1h Apply Lethal Damage GE (Health 20 -> 0) -> Player sends NO reaction event
				PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 20.0f);
				TestTrue(TEXT("Lethal damage GE applied to Player"),
					ApplyDamageWithTags(PlayerASC, PlayerASC, Player, SmallTags));
				TestEqual(TEXT("Lethal damage sent NO reaction event on Player"), PlayerSmallEventCount, 1);
				TestEqual(TEXT("Player Health reached 0"),
					PlayerASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute()), 0.0f);
			}

			Player->Destroy();
		}
	}

	// 3.2 Enemy Character Health Dispatch Tests
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AEnemyCharacter* Enemy = World->SpawnActor<AEnemyCharacter>(AEnemyCharacter::StaticClass(), FVector(100.0f, 0.0f, 0.0f), FRotator::ZeroRotator, SpawnParams);
		TestNotNull(TEXT("Enemy spawned successfully"), Enemy);

		if (Enemy)
		{
			Enemy->DispatchBeginPlay();
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
				float LastEnemyEventMagnitude = 0.0f;

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

				// 3.2a Apply Damage GE with Data.Reaction.Small -> dispatches Event.Reaction.Enemy.Small
				FGameplayTagContainer SmallTags;
				SmallTags.AddTag(TagDataSmall);
				TestTrue(TEXT("Damage GE with Data.Reaction.Small applied to Enemy"),
					ApplyDamageWithTags(EnemyASC, EnemyASC, Enemy, SmallTags));
				TestEqual(TEXT("Enemy received 1 Small reaction event"), EnemySmallEventCount, 1);
				TestEqual(TEXT("Enemy Small EventMagnitude is 25.0"), LastEnemyEventMagnitude, 25.0f);
				TestEqual(TEXT("Enemy Big count is 0"), EnemyBigEventCount, 0);

				// Restore Health to 100 for non-lethal branch isolation
				EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 100.0f);

				// 3.2b Apply Damage GE with Data.Reaction.Big -> dispatches Event.Reaction.Enemy.Big
				FGameplayTagContainer BigTags;
				BigTags.AddTag(TagDataBig);
				TestTrue(TEXT("Damage GE with Data.Reaction.Big applied to Enemy"),
					ApplyDamageWithTags(EnemyASC, EnemyASC, Enemy, BigTags));
				TestEqual(TEXT("Enemy received 1 Big reaction event"), EnemyBigEventCount, 1);
				TestEqual(TEXT("Enemy Big EventMagnitude is 25.0"), LastEnemyEventMagnitude, 25.0f);

				// Restore Health to 100
				EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 100.0f);

				// 3.2c Apply Damage GE with Data.Reaction.Launch -> Enemy Launch is legal C3E no-op
				FGameplayTagContainer LaunchTags;
				LaunchTags.AddTag(TagDataLaunch);
				TestTrue(TEXT("Damage GE with Data.Reaction.Launch applied to Enemy"),
					ApplyDamageWithTags(EnemyASC, EnemyASC, Enemy, LaunchTags));
				TestEqual(TEXT("Enemy received NO additional event for Launch"), EnemySmallEventCount + EnemyBigEventCount, 2);

				// Restore Health to 100
				EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 100.0f);

				// 3.2d Apply Damage GE with 0 reaction tags -> None (no event)
				FGameplayTagContainer NoReactionTags;
				TestTrue(TEXT("Damage GE with 0 reaction tags applied to Enemy"),
					ApplyDamageWithTags(EnemyASC, EnemyASC, Enemy, NoReactionTags));
				TestEqual(TEXT("Enemy received NO event for unclassified GE"), EnemySmallEventCount + EnemyBigEventCount, 2);

				// Restore Health to 100
				EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 100.0f);

				// 3.2e Apply Damage GE with invalid multi-tier tags -> Fail-Closed, no event
				FGameplayTagContainer InvalidTags;
				InvalidTags.AddTag(TagDataSmall);
				InvalidTags.AddTag(TagDataBig);
				TestTrue(TEXT("Damage GE with invalid multi-tier tags applied to Enemy"),
					ApplyDamageWithTags(EnemyASC, EnemyASC, Enemy, InvalidTags));
				TestEqual(TEXT("Enemy received NO event for invalid multi-tier GE"), EnemySmallEventCount + EnemyBigEventCount, 2);

				// Restore Health to 100
				EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 100.0f);

				// 3.2f Apply Damage GE when Enemy is Stunned -> blocked, no event
				EnemyASC->AddLooseGameplayTag(TagStunned);
				TestTrue(TEXT("Damage GE applied while Enemy is Stunned"),
					ApplyDamageWithTags(EnemyASC, EnemyASC, Enemy, SmallTags));
				TestEqual(TEXT("Stunned Enemy received NO Small event"), EnemySmallEventCount, 1);
				EnemyASC->RemoveLooseGameplayTag(TagStunned);

				// Restore Health to 100
				EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 100.0f);

				// 3.2g Apply Damage GE when Enemy is Poise Broken -> blocked, no reaction event
				EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetPoiseAttribute(), 0.0f);
				TestTrue(TEXT("Enemy IsPoiseBroken() is true when Poise is 0"), Enemy->IsPoiseBroken());
				TestTrue(TEXT("Damage GE applied while Enemy is Poise Broken"),
					ApplyDamageWithTags(EnemyASC, EnemyASC, Enemy, SmallTags));
				TestEqual(TEXT("Poise Broken Enemy received NO Small event"), EnemySmallEventCount, 1);
				EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetPoiseAttribute(), 100.0f);

				// 3.2h Apply Lethal Damage GE (Health 20 -> 0) -> Enemy sets dead state, sends NO reaction event
				EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 20.0f);
				TestTrue(TEXT("Lethal damage GE applied to Enemy"),
					ApplyDamageWithTags(EnemyASC, EnemyASC, Enemy, SmallTags));
				TestTrue(TEXT("Enemy is dead after lethal health depletion"), Enemy->IsDead());
				TestEqual(TEXT("Lethal damage sent NO additional reaction event"), EnemySmallEventCount, 1);
				TestEqual(TEXT("Enemy Big count remains 1"), EnemyBigEventCount, 1);

				// 3.2i Apply Damage GE when Enemy is already dead -> blocked, no reaction event
				TestTrue(TEXT("Damage GE applied while Enemy is already dead"),
					ApplyDamageWithTags(EnemyASC, EnemyASC, Enemy, SmallTags));
				TestEqual(TEXT("Already Dead Enemy sent NO additional reaction event"), EnemySmallEventCount, 1);
				TestEqual(TEXT("Enemy Big count remains 1 after hit to dead enemy"), EnemyBigEventCount, 1);
			}

			Enemy->Destroy();
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
