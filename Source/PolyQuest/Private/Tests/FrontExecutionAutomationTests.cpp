#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/EnemyStanceBreakAbility.h"
#include "AbilitySystem/Abilities/EnemyVictimExecutionAbility.h"
#include "AbilitySystem/Abilities/PlayerFrontExecutionAbility.h"
#include "AbilitySystem/CharacterAttributeSet.h"
#include "Animation/AnimMontage.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Character/Player/PlayerCharacter.h"
#include "Combat/Equipment/MeleeWeaponDefinition.h"
#include "Combat/Equipment/WeaponEquipmentComponent.h"
#include "Combat/Melee/MeleeHitResolver.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/PolyQuestPlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "Kismet/GameplayStatics.h"
#include "Tests/CombatAutomationFixture.h"
#include "Tests/TestProjectileDamageGE.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFrontExecutionAutomationTest,
	"PolyQuest.Combat.FrontExecution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	struct FFrontExecutionTestWorldScope
	{
		UWorld* World = nullptr;
		~FFrontExecutionTestWorldScope()
		{
			if (World)
			{
				UGameplayStatics::SetGlobalTimeDilation(World, 1.0f);
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
		}
	};

	void TickFrontExecutionTestWorld(UWorld* World, float DeltaSeconds)
	{
		if (World)
		{
			World->Tick(ELevelTick::LEVELTICK_All, DeltaSeconds);
			++GFrameCounter;
		}
	}
}

bool FFrontExecutionAutomationTest::RunTest(const FString& Parameters)
{
	// =========================================================================
	// 1. CDO & Tag Contract Verification
	// =========================================================================
	const UPlayerFrontExecutionAbility* ExecutionCDO = UPlayerFrontExecutionAbility::StaticClass()->GetDefaultObject<UPlayerFrontExecutionAbility>();
	if (!TestNotNull(TEXT("UPlayerFrontExecutionAbility CDO exists"), ExecutionCDO))
	{
		return false;
	}

	TestEqual(TEXT("InstancingPolicy is InstancedPerActor"),
		ExecutionCDO->GetInstancingPolicy(),
		EGameplayAbilityInstancingPolicy::InstancedPerActor);

	TestEqual(TEXT("NetExecutionPolicy is ServerOnly"),
		ExecutionCDO->GetNetExecutionPolicy(),
		EGameplayAbilityNetExecutionPolicy::ServerOnly);

	const FGameplayTag TagExecutionFront = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.Execution.Front")), false);
	const FGameplayTag TagCancelByDodge = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.CancelableBy.Dodge")), false);
	const FGameplayTag TagCancelByDefense = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.CancelableBy.Defense")), false);
	const FGameplayTag TagCancelByReaction = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.CancelableBy.Reaction")), false);
	const FGameplayTag TagTeardownOnUnpossess = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.Teardown.OnUnpossess")), false);

	TestTrue(TEXT("Tag Ability.Action.Execution.Front is registered"), TagExecutionFront.IsValid());
	TestTrue(TEXT("AbilityTags has Ability.Action.Execution.Front"), ExecutionCDO->GetTestAbilityTags().HasTagExact(TagExecutionFront));
	TestFalse(TEXT("AbilityTags has NO CancelableBy.Dodge"), ExecutionCDO->GetTestAbilityTags().HasTagExact(TagCancelByDodge));
	TestFalse(TEXT("AbilityTags has NO CancelableBy.Defense"), ExecutionCDO->GetTestAbilityTags().HasTagExact(TagCancelByDefense));
	TestFalse(TEXT("AbilityTags has NO CancelableBy.Reaction"), ExecutionCDO->GetTestAbilityTags().HasTagExact(TagCancelByReaction));
	TestTrue(TEXT("AbilityTags has Teardown.OnUnpossess"), ExecutionCDO->GetTestAbilityTags().HasTagExact(TagTeardownOnUnpossess));

	const FGameplayTag TagAttacking = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Attacking")), false);
	const FGameplayTag TagPlayerLocked = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Execution.PlayerLocked")), false);
	const FGameplayTag TagInvulnerable = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Invulnerable")), false);
	const FGameplayTag TagBlockMove = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Movement")), false);
	const FGameplayTag TagBlockJump = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Jump")), false);

	TestTrue(TEXT("ActivationOwnedTags has State.Action.Attacking"), ExecutionCDO->GetTestActivationOwnedTags().HasTagExact(TagAttacking));
	TestTrue(TEXT("ActivationOwnedTags has State.Action.Execution.PlayerLocked"), ExecutionCDO->GetTestActivationOwnedTags().HasTagExact(TagPlayerLocked));
	TestTrue(TEXT("ActivationOwnedTags has State.Status.Invulnerable"), ExecutionCDO->GetTestActivationOwnedTags().HasTagExact(TagInvulnerable));
	TestTrue(TEXT("ActivationOwnedTags has State.Input.Block.Movement"), ExecutionCDO->GetTestActivationOwnedTags().HasTagExact(TagBlockMove));
	TestTrue(TEXT("ActivationOwnedTags has State.Input.Block.Jump"), ExecutionCDO->GetTestActivationOwnedTags().HasTagExact(TagBlockJump));

	const FGameplayTag TagDead = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
	const FGameplayTag TagStunned = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
	const FGameplayTag TagExhausted = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Exhausted")), false);
	const FGameplayTag TagSprinting = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Movement.Sprinting")), false);

	TestTrue(TEXT("ActivationBlockedTags has State.Status.Dead"), ExecutionCDO->GetTestActivationBlockedTags().HasTagExact(TagDead));
	TestTrue(TEXT("ActivationBlockedTags has State.Status.Stunned"), ExecutionCDO->GetTestActivationBlockedTags().HasTagExact(TagStunned));
	TestTrue(TEXT("ActivationBlockedTags has State.Status.Exhausted"), ExecutionCDO->GetTestActivationBlockedTags().HasTagExact(TagExhausted));
	TestTrue(TEXT("ActivationBlockedTags has State.Movement.Sprinting"), ExecutionCDO->GetTestActivationBlockedTags().HasTagExact(TagSprinting));

	const FGameplayTag TagHitEvent = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Hit")), false);
	TestTrue(TEXT("Tag Event.Action.Execution.Hit is registered"), TagHitEvent.IsValid());

	// =========================================================================
	// 2. Fail-Closed Default Configuration Verification
	// =========================================================================
	{
		TestFalse(TEXT("CDO default configuration fails CanActivateAbility"),
			ExecutionCDO->CanActivateAbility(FGameplayAbilitySpecHandle(), nullptr));
	}

	// =========================================================================
	// 3. World Test Setup
	// =========================================================================
	if (!TestNotNull(TEXT("GEngine is valid"), GEngine))
	{
		return false;
	}

	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("FrontExecutionTestWorld"));
	WorldContext.SetCurrentWorld(World);
	FFrontExecutionTestWorldScope ScopeCleanup{ World };

	if (!TestNotNull(TEXT("Test World created"), World))
	{
		return false;
	}

	FURL WorldURL;
	World->InitializeActorsForPlay(WorldURL);
	World->BeginPlay();

	APlayerCharacter* Player = FCombatAutomationFixture::SpawnPlayer(
		World,
		FTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, 0.0f)));
	AEnemyCharacter* Enemy = FCombatAutomationFixture::SpawnPassiveEnemy(
		World,
		FTransform(FRotator(0.0f, 180.0f, 0.0f), FVector(150.0f, 0.0f, 0.0f))); // Facing Player at origin
	APolyQuestPlayerController* Controller = World->SpawnActor<APolyQuestPlayerController>();

	if (!TestNotNull(TEXT("Player spawned"), Player)
		|| !TestNotNull(TEXT("Enemy spawned"), Enemy)
		|| !TestNotNull(TEXT("Controller spawned"), Controller))
	{
		return false;
	}

	Controller->Possess(Player);
	UAbilitySystemComponent* PlayerASC = Player->GetAbilitySystemComponent();
	UAbilitySystemComponent* EnemyASC = Enemy->GetAbilitySystemComponent();

	if (!TestNotNull(TEXT("Player ASC valid"), PlayerASC) || !TestNotNull(TEXT("Enemy ASC valid"), EnemyASC))
	{
		return false;
	}

	// Setup Combat Team Tags
	const FGameplayTag TagTeamPlayer = FGameplayTag::RequestGameplayTag(FName(TEXT("Team.Player")), false);
	const FGameplayTag TagTeamEnemy = FGameplayTag::RequestGameplayTag(FName(TEXT("Team.Enemy")), false);
	Player->SetTestCombatTeamTag(TagTeamPlayer);
	Enemy->SetTestCombatTeamTag(TagTeamEnemy);

	FGameplayAbilitySpec VictimSpec(UEnemyVictimExecutionAbility::StaticClass(), 1, INDEX_NONE, Enemy);
	EnemyASC->GiveAbility(VictimSpec);

	auto GrantAndConfigureExecAbility = [&](APlayerCharacter* InPlayer, UAnimMontage* Montage, TSubclassOf<UGameplayEffect> DamageClass, float MinDist, float MaxDist, float MaxAngle, bool bWarp = false) -> TPair<FGameplayAbilitySpecHandle, UPlayerFrontExecutionAbility*>
	{
		UAbilitySystemComponent* ASC = InPlayer->GetAbilitySystemComponent();
		FGameplayAbilitySpec Spec(UPlayerFrontExecutionAbility::StaticClass(), 1, INDEX_NONE, InPlayer);
		const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);
		FGameplayAbilitySpec* FoundSpec = ASC->FindAbilitySpecFromHandle(Handle);
		UPlayerFrontExecutionAbility* Instance = FoundSpec ? Cast<UPlayerFrontExecutionAbility>(FoundSpec->GetPrimaryInstance()) : nullptr;
		if (Instance)
		{
			Instance->SetTestSkipMontageTaskActivation(true);
			Instance->SetTestExecutionMontage(Montage);
			Instance->SetTestDamageGameplayEffectClass(DamageClass);
			Instance->SetTestExecutionDistances(MinDist, MaxDist);
			Instance->SetTestMaxFrontAngleDegrees(MaxAngle);
			if (bWarp)
			{
				Instance->SetTestMotionWarpConfig(true, FName(TEXT("MeleeContact")), 50.0f, 150.0f, 300.0f, 60.0f);
			}
		}
		return { Handle, Instance };
	};

	auto GrantEnemyStanceBreakAbility = [&](AEnemyCharacter* InEnemy) -> FGameplayAbilitySpecHandle
	{
		UAbilitySystemComponent* ASC = InEnemy->GetAbilitySystemComponent();
		FGameplayAbilitySpec Spec(UEnemyStanceBreakAbility::StaticClass(), 1, INDEX_NONE, InEnemy);
		const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);
		if (FGameplayAbilitySpec* FoundSpec = ASC->FindAbilitySpecFromHandle(Handle))
		{
			FoundSpec->ActivationInfo.SetActivationConfirmed();
			FoundSpec->ActiveCount = 1;
		}
		return Handle;
	};

	// =========================================================================
	// 4. Front Geometry Unit Tests (Stateless Check)
	// =========================================================================
	{
		UAnimMontage* SyntheticMontage = NewObject<UAnimMontage>();
		TSubclassOf<UGameplayEffect> DamageGEClass = UTestProjectileDamageGE::StaticClass();

		auto [ExecHandle, ExecAbility] = GrantAndConfigureExecAbility(Player, SyntheticMontage, DamageGEClass, 50.0f, 250.0f, 60.0f);
		if (TestNotNull(TEXT("ExecAbility instance valid for geometry check"), ExecAbility))
		{
			// Case 4.1: Perfect front alignment (Enemy at (150,0,0) facing (0,0,0), Player at (0,0,0))
			Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
			Enemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
			Enemy->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f)); // Enemy forward is (-1, 0, 0), towards player

			float Dist2D = 0.0f, AngleDeg = 0.0f;
			const bool bFrontGeoPass = ExecAbility->TestEvaluateFrontGeometry(Player, Enemy, Dist2D, AngleDeg);
			TestTrue(TEXT("Front geometry passes for directly facing player"), bFrontGeoPass);
			TestEqual(TEXT("Distance is approx 150"), FMath::RoundToInt(Dist2D), 150);
			TestTrue(TEXT("Angle is approx 0 degrees"), AngleDeg <= 1.0f);

			// Case 4.2: Angle at 45 degrees (< 60 degrees threshold)
			Enemy->SetActorRotation(FRotator(0.0f, 135.0f, 0.0f));
			TestTrue(TEXT("Front geometry passes for 45 deg angle"),
				ExecAbility->TestEvaluateFrontGeometry(Player, Enemy, Dist2D, AngleDeg));
			TestTrue(TEXT("Angle is approx 45 degrees"), FMath::IsNearlyEqual(AngleDeg, 45.0f, 1.0f));

			// Case 4.3: Angle at 75 degrees (> 60 degrees threshold)
			Enemy->SetActorRotation(FRotator(0.0f, 105.0f, 0.0f));
			TestFalse(TEXT("Front geometry rejected for 75 deg angle (> 60)"),
				ExecAbility->TestEvaluateFrontGeometry(Player, Enemy, Dist2D, AngleDeg));

			// Case 4.4: Directly from behind (Enemy facing away from Player at 0 deg Yaw)
			Enemy->SetActorRotation(FRotator(0.0f, 0.0f, 0.0f)); // Enemy forward is (+1, 0, 0), away from player
			TestFalse(TEXT("Front geometry rejected from behind (180 deg)"),
				ExecAbility->TestEvaluateFrontGeometry(Player, Enemy, Dist2D, AngleDeg));
			TestTrue(TEXT("Back angle is approx 180 degrees"), AngleDeg >= 179.0f);

			// Case 4.5: Distance too close (< 50cm) or too far (> 250cm)
			Enemy->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f));
			Enemy->SetActorLocation(FVector(30.0f, 0.0f, 0.0f));
			TestFalse(TEXT("Front geometry rejected when too close (30cm < 50cm)"),
				ExecAbility->TestEvaluateFrontGeometry(Player, Enemy, Dist2D, AngleDeg));

			Enemy->SetActorLocation(FVector(300.0f, 0.0f, 0.0f));
			TestFalse(TEXT("Front geometry rejected when too far (300cm > 250cm)"),
				ExecAbility->TestEvaluateFrontGeometry(Player, Enemy, Dist2D, AngleDeg));

			// Case 4.6: NaN, Inf, and zero-vector robustness (Fail-Closed Finite Contract)
			float BadDist = 0.0f, BadAngle = 0.0f;
			const FVector GoodPlayerLoc(0.0f, 0.0f, 0.0f);
			const FVector GoodEnemyLoc(150.0f, 0.0f, 0.0f);
			const FVector GoodEnemyForward(-1.0f, 0.0f, 0.0f);

			// Player Location NaN / Inf
			TestFalse(TEXT("Front geometry rejected for NaN Player X location"),
				UPlayerFrontExecutionAbility::TestEvaluateFrontGeometryVectors(
					FVector(NAN, 0.0f, 0.0f), GoodEnemyLoc, GoodEnemyForward, 50.0f, 250.0f, 60.0f, BadDist, BadAngle));

			TestFalse(TEXT("Front geometry rejected for Inf Player Y location"),
				UPlayerFrontExecutionAbility::TestEvaluateFrontGeometryVectors(
					FVector(0.0f, INFINITY, 0.0f), GoodEnemyLoc, GoodEnemyForward, 50.0f, 250.0f, 60.0f, BadDist, BadAngle));

			// Enemy Location NaN / Inf
			TestFalse(TEXT("Front geometry rejected for NaN Enemy Z location"),
				UPlayerFrontExecutionAbility::TestEvaluateFrontGeometryVectors(
					GoodPlayerLoc, FVector(150.0f, 0.0f, NAN), GoodEnemyForward, 50.0f, 250.0f, 60.0f, BadDist, BadAngle));

			// Enemy Forward Vector NaN / Inf / Zero
			TestFalse(TEXT("Front geometry rejected for NaN Enemy Forward"),
				UPlayerFrontExecutionAbility::TestEvaluateFrontGeometryVectors(
					GoodPlayerLoc, GoodEnemyLoc, FVector(NAN, 0.0f, 0.0f), 50.0f, 250.0f, 60.0f, BadDist, BadAngle));

			TestFalse(TEXT("Front geometry rejected for zero Enemy Forward vector"),
				UPlayerFrontExecutionAbility::TestEvaluateFrontGeometryVectors(
					GoodPlayerLoc, GoodEnemyLoc, FVector(0.0f, 0.0f, 0.0f), 50.0f, 250.0f, 60.0f, BadDist, BadAngle));

			// Coincident positions (zero-distance vector between Player and Enemy)
			TestFalse(TEXT("Front geometry rejected for coincident locations (zero distance)"),
				UPlayerFrontExecutionAbility::TestEvaluateFrontGeometryVectors(
					FVector(100.0f, 100.0f, 0.0f), FVector(100.0f, 100.0f, 0.0f), GoodEnemyForward, 50.0f, 250.0f, 60.0f, BadDist, BadAngle));

			// Case 4.7: Exact 0-degree threshold contract [0, 90]
			float ZeroAngleDist = 0.0f, ZeroAngleDeg = 0.0f;
			TestTrue(TEXT("Front geometry passes for exact 0-degree threshold when perfectly aligned"),
				UPlayerFrontExecutionAbility::TestEvaluateFrontGeometryVectors(
					GoodPlayerLoc, GoodEnemyLoc, GoodEnemyForward, 50.0f, 250.0f, 0.0f, ZeroAngleDist, ZeroAngleDeg));
			TestTrue(TEXT("Angle is within 0-deg boundary"), ZeroAngleDeg <= 0.001f);

			// Slightly off-angle (e.g. 5 deg offset) should fail when threshold is 0 deg
			const FVector OffForward = FRotator(0.0f, 175.0f, 0.0f).Vector();
			TestFalse(TEXT("Front geometry rejects off-angle when threshold is exact 0 deg"),
				UPlayerFrontExecutionAbility::TestEvaluateFrontGeometryVectors(
					GoodPlayerLoc, GoodEnemyLoc, OffForward, 50.0f, 250.0f, 0.0f, ZeroAngleDist, ZeroAngleDeg));

			// Negative threshold (< 0) must be rejected
			TestFalse(TEXT("Front geometry rejects negative max angle threshold"),
				UPlayerFrontExecutionAbility::TestEvaluateFrontGeometryVectors(
					GoodPlayerLoc, GoodEnemyLoc, GoodEnemyForward, 50.0f, 250.0f, -1.0f, ZeroAngleDist, ZeroAngleDeg));

			// Reset enemy location & rotation
			Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
			Enemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
			Enemy->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f));
		}
		PlayerASC->ClearAbility(ExecHandle);
	}

	// =========================================================================
	// 5. Stance Break Prerequisite & Loose Tag Rejection Gate
	// =========================================================================
	{
		UAnimMontage* SyntheticMontage = NewObject<UAnimMontage>();
		TSubclassOf<UGameplayEffect> DamageGEClass = UTestProjectileDamageGE::StaticClass();

		auto [ExecHandle, ExecAbility] = GrantAndConfigureExecAbility(Player, SyntheticMontage, DamageGEClass, 50.0f, 250.0f, 60.0f);

		FGameplayAbilityActorInfo ActorInfo;
		ActorInfo.InitFromActor(Player, Player, PlayerASC);

		// 5.1 No target locked -> CanActivateAbility should fail
		Player->TestClearLockedTarget();
		TestFalse(TEXT("Fails activation when no target locked"),
			ExecAbility->CanActivateAbility(ExecHandle, &ActorInfo));

		// 5.2 Target has NO Stunned tag -> Should fail
		Player->SetTestLockedTarget(Enemy);
		TestFalse(TEXT("Fails activation when target has no Stunned tag"),
			ExecAbility->CanActivateAbility(ExecHandle, &ActorInfo));

		// 5.3 Target is Invulnerable -> Should fail
		EnemyASC->AddLooseGameplayTag(TagStunned);
		EnemyASC->AddLooseGameplayTag(TagInvulnerable);
		TestFalse(TEXT("Fails activation when target is Invulnerable"),
			ExecAbility->CanActivateAbility(ExecHandle, &ActorInfo));
		EnemyASC->RemoveLooseGameplayTag(TagInvulnerable);

		// 5.4 Target has Stunned tag and valid front geometry -> CanActivateAbility should SUCCEED
		TestTrue(TEXT("CanActivateAbility succeeds with valid Stunned tag and front geometry"),
			ExecAbility->CanActivateAbility(ExecHandle, &ActorInfo));

		// Cleanup
		PlayerASC->ClearAbility(ExecHandle);
		EnemyASC->RemoveLooseGameplayTag(TagStunned);
	}

	// =========================================================================
	// 6. Activation & Target Reservation Gate
	// =========================================================================
	{
		UAnimMontage* SyntheticMontage = NewObject<UAnimMontage>();
		TSubclassOf<UGameplayEffect> DamageGEClass = UTestProjectileDamageGE::StaticClass();

		auto [ExecHandle, ExecAbility] = GrantAndConfigureExecAbility(Player, SyntheticMontage, DamageGEClass, 50.0f, 250.0f, 60.0f);

		EnemyASC->AddLooseGameplayTag(TagStunned);
		if (UCharacterAttributeSet* EnemyAttribs = const_cast<UCharacterAttributeSet*>(EnemyASC->GetSet<UCharacterAttributeSet>()))
		{
			EnemyAttribs->SetPoise(0.0f);
		}
		const FGameplayAbilitySpecHandle StanceBreakHandle = GrantEnemyStanceBreakAbility(Enemy);

		Player->SetTestLockedTarget(Enemy);

		// Activate via primary attack input intent
		Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));

		TestNotNull(TEXT("Execution ability reserved the valid enemy target"),
			ExecAbility->GetTestReservedTarget());
		TestEqual(TEXT("Reserved target is indeed Enemy"),
			ExecAbility->GetTestReservedTarget(), Enemy);

		// End ability
		ExecAbility->TestEndAbility();
		TestNull(TEXT("Target reservation cleared on EndAbility"),
			ExecAbility->GetTestReservedTarget());

		PlayerASC->ClearAbility(ExecHandle);
		EnemyASC->ClearAbility(StanceBreakHandle);
		EnemyASC->RemoveLooseGameplayTag(TagStunned);
	}

	// =========================================================================
	// 7. Hit Event Routing, Exactly-Once Consumption & Damage Resolution
	// =========================================================================
	{
		UAnimMontage* SyntheticMontage = NewObject<UAnimMontage>();
		TSubclassOf<UGameplayEffect> DamageGEClass = UTestProjectileDamageGE::StaticClass();

		auto [ExecHandle, ExecAbility] = GrantAndConfigureExecAbility(Player, SyntheticMontage, DamageGEClass, 50.0f, 250.0f, 60.0f);

		// Prepare Enemy
		EnemyASC->AddLooseGameplayTag(TagStunned);
		if (UCharacterAttributeSet* EnemyAttribs = const_cast<UCharacterAttributeSet*>(EnemyASC->GetSet<UCharacterAttributeSet>()))
		{
			EnemyAttribs->SetPoise(0.0f);
			EnemyAttribs->SetHealth(100.0f);
		}
		const FGameplayAbilitySpecHandle StanceBreakHandle = GrantEnemyStanceBreakAbility(Enemy);

		Player->SetTestLockedTarget(Enemy);
		Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));

		// 7.1 Send mismatched notify (wrong event tag) -> No damage
		FGameplayEventData BadPayload;
		BadPayload.EventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Attack.Light.Combo.BranchWindow.Begin")), false);
		BadPayload.Instigator = Player;
		BadPayload.Target = Player;
		BadPayload.OptionalObject = SyntheticMontage;
		ExecAbility->TestTriggerHitEvent(BadPayload);
		TestFalse(TEXT("Wrong event tag does not consume damage event"), ExecAbility->IsTestDamageEventConsumed());

		// 7.2 Send mismatched notify (wrong instigator) -> No damage
		BadPayload.EventTag = TagHitEvent;
		BadPayload.Instigator = Enemy;
		ExecAbility->TestTriggerHitEvent(BadPayload);
		TestFalse(TEXT("Wrong instigator does not consume damage event"), ExecAbility->IsTestDamageEventConsumed());

		// 7.3 Send mismatched notify (wrong target) -> No damage
		BadPayload.Instigator = Player;
		BadPayload.Target = Enemy;
		ExecAbility->TestTriggerHitEvent(BadPayload);
		TestFalse(TEXT("Wrong target does not consume damage event"), ExecAbility->IsTestDamageEventConsumed());

		// 7.4 Send mismatched notify (wrong montage) -> No damage
		UAnimMontage* WrongMontage = NewObject<UAnimMontage>();
		BadPayload.Target = Player;
		BadPayload.OptionalObject = WrongMontage;
		ExecAbility->TestTriggerHitEvent(BadPayload);
		TestFalse(TEXT("Wrong montage does not consume damage event"), ExecAbility->IsTestDamageEventConsumed());

		// Record initial target health before correct hit
		const UCharacterAttributeSet* EnemyAttribSet = EnemyASC->GetSet<UCharacterAttributeSet>();
		const float HealthBeforeHit = EnemyAttribSet ? EnemyAttribSet->GetHealth() : 0.0f;
		TestTrue(TEXT("Enemy initial health is positive before execution hit"), HealthBeforeHit > 0.0f);

		// 7.5 Correct Hit Event Payload -> Consumes damage event and resolves hit exactly once!
		FGameplayEventData GoodPayload;
		GoodPayload.EventTag = TagHitEvent;
		GoodPayload.Instigator = Player;
		GoodPayload.Target = Player;
		GoodPayload.OptionalObject = SyntheticMontage;
		ExecAbility->TestTriggerHitEvent(GoodPayload);
		TestTrue(TEXT("Correct hit event consumed damage event exactly once"), ExecAbility->IsTestDamageEventConsumed());

		const float HealthAfterFirstHit = EnemyAttribSet ? EnemyAttribSet->GetHealth() : 0.0f;
		TestTrue(TEXT("First correct hit event applied damage and reduced health"), HealthAfterFirstHit < HealthBeforeHit);

		// 7.6 Second hit event must be ignored (idempotent / exactly-once damage guarantee)
		ExecAbility->TestTriggerHitEvent(GoodPayload);
		TestTrue(TEXT("Damage event remains consumed, not re-triggered"), ExecAbility->IsTestDamageEventConsumed());

		const float HealthAfterSecondHit = EnemyAttribSet ? EnemyAttribSet->GetHealth() : 0.0f;
		TestEqual(TEXT("Second hit event does not apply additional damage"), HealthAfterSecondHit, HealthAfterFirstHit);

		// Cleanup
		ExecAbility->TestEndAbility();
		PlayerASC->ClearAbility(ExecHandle);
		EnemyASC->ClearAbility(StanceBreakHandle);
		EnemyASC->RemoveLooseGameplayTag(TagStunned);

		// 7.7 TryResolveHit returns false (e.g. Friendly Fire / Same Team) -> EndAbility cleans up and prevents retry
		{
			auto [FailedExecHandle, FailedExecAbility] = GrantAndConfigureExecAbility(Player, SyntheticMontage, DamageGEClass, 50.0f, 250.0f, 60.0f);
			EnemyASC->AddLooseGameplayTag(TagStunned);
			const FGameplayAbilitySpecHandle EnemySBHandle = GrantEnemyStanceBreakAbility(Enemy);
			if (UCharacterAttributeSet* EnemyAttribs = const_cast<UCharacterAttributeSet*>(EnemyASC->GetSet<UCharacterAttributeSet>()))
			{
				EnemyAttribs->SetPoise(0.0f);
				EnemyAttribs->SetHealth(100.0f);
			}

			Player->SetTestLockedTarget(Enemy);
			Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));

			TestNotNull(TEXT("Ability active with reserved target before failed resolve"), FailedExecAbility->GetTestReservedTarget());

			// Set Enemy team to match Player team (Team.Player), causing FMeleeHitResolver::TryResolveHit to return false
			const FGameplayTag OriginalEnemyTeam = ICombatTeamAgent::Execute_GetCombatTeamTag(Enemy);
			const FGameplayTag PlayerTeamTag = ICombatTeamAgent::Execute_GetCombatTeamTag(Player);
			Enemy->SetTestCombatTeamTag(PlayerTeamTag);

			const float HealthBeforeFailedHit = EnemyAttribSet ? EnemyAttribSet->GetHealth() : 0.0f;
			FailedExecAbility->TestTriggerHitEvent(GoodPayload);

			// Failed hit resolution immediately invoked EndAbility, clearing target reservation and stopping ability
			TestNull(TEXT("Failed hit resolution ended ability and cleared target reservation"), FailedExecAbility->GetTestReservedTarget());
			TestFalse(TEXT("Failed hit resolution terminated active ability"), FailedExecAbility->IsActive());
			TestEqual(TEXT("Same team target took no damage"), EnemyAttribSet ? EnemyAttribSet->GetHealth() : 0.0f, HealthBeforeFailedHit);

			// Attempting a second hit event must not apply damage / must not retry
			FailedExecAbility->TestTriggerHitEvent(GoodPayload);
			TestEqual(TEXT("Subsequent hit event after EndAbility does not apply damage"),
				EnemyAttribSet ? EnemyAttribSet->GetHealth() : 0.0f, HealthBeforeFailedHit);

			Enemy->SetTestCombatTeamTag(OriginalEnemyTeam);
			EnemyASC->RemoveLooseGameplayTag(TagStunned);
			EnemyASC->ClearAbility(EnemySBHandle);
			PlayerASC->ClearAbility(FailedExecHandle);
		}
	}

	// =========================================================================
	// 8. Teardown, Stunned-Loss & Motion-Warp Cleanup Verification
	// =========================================================================
	{
		UAnimMontage* SyntheticMontage = NewObject<UAnimMontage>();
		TSubclassOf<UGameplayEffect> DamageGEClass = UTestProjectileDamageGE::StaticClass();

		auto [ExecHandle, ExecAbility] = GrantAndConfigureExecAbility(Player, SyntheticMontage, DamageGEClass, 50.0f, 250.0f, 60.0f, true);

		EnemyASC->AddLooseGameplayTag(TagStunned);
		if (UCharacterAttributeSet* EnemyAttribs = const_cast<UCharacterAttributeSet*>(EnemyASC->GetSet<UCharacterAttributeSet>()))
		{
			EnemyAttribs->SetPoise(0.0f);
		}
		const FGameplayAbilitySpecHandle StanceBreakHandle = GrantEnemyStanceBreakAbility(Enemy);

		Player->SetTestLockedTarget(Enemy);
		Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));

		TestNotNull(TEXT("Execution active with reserved target"), ExecAbility->GetTestReservedTarget());

		// Removing target Stunned / ending victim ability should end the player execution ability fail-closed
		EnemyASC->RemoveLooseGameplayTag(TagStunned);
		EnemyASC->CancelAbilityHandle(StanceBreakHandle);
		for (const FGameplayAbilitySpec& Spec : EnemyASC->GetActivatableAbilities())
		{
			if (Spec.Ability && Spec.Ability->IsA<UEnemyVictimExecutionAbility>() && Spec.IsActive())
			{
				if (UEnemyVictimExecutionAbility* VictimInst = Cast<UEnemyVictimExecutionAbility>(Spec.GetPrimaryInstance()))
				{
					VictimInst->TestEndAbility(true);
				}
			}
		}
		TickFrontExecutionTestWorld(World, 0.01f);

		TestNull(TEXT("Execution ended and target reservation cleared when Stunned was removed"),
			ExecAbility->GetTestReservedTarget());

		PlayerASC->ClearAbility(ExecHandle);
		EnemyASC->ClearAbility(StanceBreakHandle);
	}

	// =========================================================================
	// 9. ReadyForActivation Synchronous Re-entry Fail-Closed & Re-activation Gate
	// =========================================================================
	{
		UAnimMontage* SyntheticMontage = NewObject<UAnimMontage>();
		TSubclassOf<UGameplayEffect> DamageGEClass = UTestProjectileDamageGE::StaticClass();

		auto [ReentryHandle, ReentryAbility] = GrantAndConfigureExecAbility(Player, SyntheticMontage, DamageGEClass, 50.0f, 250.0f, 60.0f, true);

		EnemyASC->AddLooseGameplayTag(TagStunned);
		if (UCharacterAttributeSet* EnemyAttribs = const_cast<UCharacterAttributeSet*>(EnemyASC->GetSet<UCharacterAttributeSet>()))
		{
			EnemyAttribs->SetPoise(0.0f);
		}
		const FGameplayAbilitySpecHandle StanceBreakHandle = GrantEnemyStanceBreakAbility(Enemy);

		Player->SetTestLockedTarget(Enemy);

		// Configure simulated synchronous EndAbility during task ReadyForActivation
		ReentryAbility->SetTestEndAbilityDuringTaskReady(true);
		Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));

		// Verify ability ended cleanly without double-EndAbility crash, and tags/context are cleanly reset
		TestFalse(TEXT("Synchronous EndAbility in ReadyForActivation terminated ability cleanly"), ReentryAbility->IsActive());
		TestNull(TEXT("Target reservation cleared after synchronous end"), ReentryAbility->GetTestReservedTarget());
		TestFalse(TEXT("PlayerLocked tag not leaked after synchronous end"),
			PlayerASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Execution.PlayerLocked")), false)));

		// Re-enable normal activation and re-establish Front Execution prerequisites on Enemy (Poise 0 + active StanceBreak)
		ReentryAbility->SetTestEndAbilityDuringTaskReady(false);
		if (UCharacterAttributeSet* EnemyAttribs = const_cast<UCharacterAttributeSet*>(EnemyASC->GetSet<UCharacterAttributeSet>()))
		{
			EnemyAttribs->SetPoise(0.0f);
		}
		const FGameplayAbilitySpecHandle StanceBreakHandle2 = GrantEnemyStanceBreakAbility(Enemy);

		Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));
		TestTrue(TEXT("Subsequent activation after synchronous end succeeds"), ReentryAbility->IsActive());
		TestNotNull(TEXT("Subsequent activation reserved target"), ReentryAbility->GetTestReservedTarget());

		ReentryAbility->TestEndAbility();
		EnemyASC->RemoveLooseGameplayTag(TagStunned);
		PlayerASC->ClearAbility(ReentryHandle);
		EnemyASC->ClearAbility(StanceBreakHandle);
		EnemyASC->ClearAbility(StanceBreakHandle2);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
