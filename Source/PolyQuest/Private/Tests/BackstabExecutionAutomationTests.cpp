#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/EnemyStanceBreakAbility.h"
#include "AbilitySystem/Abilities/EnemyVictimExecutionAbility.h"
#include "AbilitySystem/Abilities/PlayerBackstabExecutionAbility.h"
#include "AbilitySystem/Abilities/PlayerFrontExecutionAbility.h"
#include "AbilitySystem/Abilities/PrimaryAttackAbility.h"
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
	FBackstabExecutionAutomationTest,
	"PolyQuest.Combat.Backstab",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	struct FBackstabExecutionTestWorldScope
	{
		UWorld* World = nullptr;
		~FBackstabExecutionTestWorldScope()
		{
			if (World)
			{
				UGameplayStatics::SetGlobalTimeDilation(World, 1.0f);
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
		}
	};

	void TickBackstabExecutionTestWorld(UWorld* World, float DeltaSeconds)
	{
		if (World)
		{
			World->Tick(ELevelTick::LEVELTICK_All, DeltaSeconds);
			++GFrameCounter;
		}
	}
}

bool FBackstabExecutionAutomationTest::RunTest(const FString& Parameters)
{
	// =========================================================================
	// 1. CDO & Tag Contract Verification
	// =========================================================================
	const UPlayerBackstabExecutionAbility* BackstabCDO = UPlayerBackstabExecutionAbility::StaticClass()->GetDefaultObject<UPlayerBackstabExecutionAbility>();
	if (!TestNotNull(TEXT("UPlayerBackstabExecutionAbility CDO exists"), BackstabCDO))
	{
		return false;
	}

	TestEqual(TEXT("InstancingPolicy is InstancedPerActor"),
		BackstabCDO->GetInstancingPolicy(),
		EGameplayAbilityInstancingPolicy::InstancedPerActor);

	TestEqual(TEXT("NetExecutionPolicy is ServerOnly"),
		BackstabCDO->GetNetExecutionPolicy(),
		EGameplayAbilityNetExecutionPolicy::ServerOnly);

	const FGameplayTag TagExecutionBackstab = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.Execution.Backstab")), false);
	const FGameplayTag TagCancelByDodge = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.CancelableBy.Dodge")), false);
	const FGameplayTag TagCancelByDefense = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.CancelableBy.Defense")), false);
	const FGameplayTag TagCancelByReaction = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.CancelableBy.Reaction")), false);
	const FGameplayTag TagTeardownOnUnpossess = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.Teardown.OnUnpossess")), false);

	TestTrue(TEXT("Tag Ability.Action.Execution.Backstab is registered"), TagExecutionBackstab.IsValid());
	TestTrue(TEXT("AbilityTags has Ability.Action.Execution.Backstab"), BackstabCDO->GetTestAbilityTags().HasTagExact(TagExecutionBackstab));
	TestFalse(TEXT("AbilityTags has NO CancelableBy.Dodge"), BackstabCDO->GetTestAbilityTags().HasTagExact(TagCancelByDodge));
	TestFalse(TEXT("AbilityTags has NO CancelableBy.Defense"), BackstabCDO->GetTestAbilityTags().HasTagExact(TagCancelByDefense));
	TestFalse(TEXT("AbilityTags has NO CancelableBy.Reaction"), BackstabCDO->GetTestAbilityTags().HasTagExact(TagCancelByReaction));
	TestTrue(TEXT("AbilityTags has Teardown.OnUnpossess"), BackstabCDO->GetTestAbilityTags().HasTagExact(TagTeardownOnUnpossess));

	const FGameplayTag TagAttacking = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Attacking")), false);
	const FGameplayTag TagPlayerLocked = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Execution.PlayerLocked")), false);
	const FGameplayTag TagInvulnerable = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Invulnerable")), false);
	const FGameplayTag TagBlockMove = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Movement")), false);
	const FGameplayTag TagBlockJump = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Jump")), false);

	TestTrue(TEXT("ActivationOwnedTags has State.Action.Attacking"), BackstabCDO->GetTestActivationOwnedTags().HasTagExact(TagAttacking));
	TestTrue(TEXT("ActivationOwnedTags has State.Action.Execution.PlayerLocked"), BackstabCDO->GetTestActivationOwnedTags().HasTagExact(TagPlayerLocked));
	TestTrue(TEXT("ActivationOwnedTags has State.Status.Invulnerable"), BackstabCDO->GetTestActivationOwnedTags().HasTagExact(TagInvulnerable));
	TestTrue(TEXT("ActivationOwnedTags has State.Input.Block.Movement"), BackstabCDO->GetTestActivationOwnedTags().HasTagExact(TagBlockMove));
	TestTrue(TEXT("ActivationOwnedTags has State.Input.Block.Jump"), BackstabCDO->GetTestActivationOwnedTags().HasTagExact(TagBlockJump));

	const FGameplayTag TagDead = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
	const FGameplayTag TagStunned = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
	const FGameplayTag TagExhausted = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Exhausted")), false);
	const FGameplayTag TagSprinting = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Movement.Sprinting")), false);

	TestTrue(TEXT("ActivationBlockedTags has State.Status.Dead"), BackstabCDO->GetTestActivationBlockedTags().HasTagExact(TagDead));
	TestTrue(TEXT("ActivationBlockedTags has State.Status.Stunned"), BackstabCDO->GetTestActivationBlockedTags().HasTagExact(TagStunned));
	TestTrue(TEXT("ActivationBlockedTags has State.Status.Exhausted"), BackstabCDO->GetTestActivationBlockedTags().HasTagExact(TagExhausted));
	TestTrue(TEXT("ActivationBlockedTags has State.Movement.Sprinting"), BackstabCDO->GetTestActivationBlockedTags().HasTagExact(TagSprinting));

	const FGameplayTag TagHitEvent = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Hit")), false);
	TestTrue(TEXT("Tag Event.Action.Execution.Hit is registered"), TagHitEvent.IsValid());

	// =========================================================================
	// 2. Fail-Closed Default Configuration Verification
	// =========================================================================
	{
		TestFalse(TEXT("CDO default configuration fails CanActivateAbility"),
			BackstabCDO->CanActivateAbility(FGameplayAbilitySpecHandle(), nullptr));
	}

	// =========================================================================
	// 3. World Test Setup
	// =========================================================================
	if (!TestNotNull(TEXT("GEngine is valid"), GEngine))
	{
		return false;
	}

	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("BackstabExecutionTestWorld"));
	WorldContext.SetCurrentWorld(World);
	FBackstabExecutionTestWorldScope ScopeCleanup{ World };

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
		FTransform(FRotator(0.0f, 0.0f, 0.0f), FVector(150.0f, 0.0f, 0.0f))); // Enemy at (150, 0, 0) facing (+1, 0, 0) -> Player is directly behind Enemy!
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

	auto GrantAndConfigureBackstabAbility = [&](APlayerCharacter* InPlayer, UAnimMontage* Montage, TSubclassOf<UGameplayEffect> DamageClass, float MinDist, float MaxDist, float MaxAngle, bool bWarp = false) -> TPair<FGameplayAbilitySpecHandle, UPlayerBackstabExecutionAbility*>
	{
		UAbilitySystemComponent* ASC = InPlayer->GetAbilitySystemComponent();
		FGameplayAbilitySpec Spec(UPlayerBackstabExecutionAbility::StaticClass(), 1, INDEX_NONE, InPlayer);
		const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);
		FGameplayAbilitySpec* FoundSpec = ASC->FindAbilitySpecFromHandle(Handle);
		UPlayerBackstabExecutionAbility* Instance = FoundSpec ? Cast<UPlayerBackstabExecutionAbility>(FoundSpec->GetPrimaryInstance()) : nullptr;
		if (Instance)
		{
			Instance->SetTestSkipMontageTaskActivation(true);
			Instance->SetTestExecutionMontage(Montage);
			Instance->SetTestDamageGameplayEffectClass(DamageClass);
			Instance->SetTestExecutionDistances(MinDist, MaxDist);
			Instance->SetTestMaxBackAngleDegrees(MaxAngle);
			if (bWarp)
			{
				Instance->SetTestMotionWarpConfig(true, FName(TEXT("MeleeContact")), 50.0f, 150.0f, 300.0f, 60.0f);
			}
		}
		return { Handle, Instance };
	};

	// =========================================================================
	// 4. Backstab Geometry Unit Tests (Stateless Check)
	// =========================================================================
	{
		UAnimMontage* SyntheticMontage = NewObject<UAnimMontage>();
		TSubclassOf<UGameplayEffect> DamageGEClass = UTestProjectileDamageGE::StaticClass();

		auto [BackstabHandle, BackstabAbility] = GrantAndConfigureBackstabAbility(Player, SyntheticMontage, DamageGEClass, 50.0f, 250.0f, 60.0f);
		if (TestNotNull(TEXT("BackstabAbility instance valid for geometry check"), BackstabAbility))
		{
			// Case 4.1: Perfect back alignment (Enemy at (150,0,0) facing (+1,0,0), Player at (0,0,0) -> Player is directly behind Enemy)
			Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
			Enemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
			Enemy->SetActorRotation(FRotator(0.0f, 0.0f, 0.0f)); // Enemy forward is (+1, 0, 0), away from player

			float Dist2D = 0.0f, AngleDeg = 0.0f;
			const bool bBackGeoPass = BackstabAbility->TestEvaluateBackstabGeometry(Player, Enemy, Dist2D, AngleDeg);
			TestTrue(TEXT("Backstab geometry passes for directly behind target"), bBackGeoPass);
			TestEqual(TEXT("Distance is approx 150"), FMath::RoundToInt(Dist2D), 150);
			TestTrue(TEXT("Angle is approx 0 degrees"), AngleDeg <= 1.0f);

			// Case 4.2: Angle at 45 degrees (< 60 degrees threshold)
			Enemy->SetActorRotation(FRotator(0.0f, 45.0f, 0.0f));
			TestTrue(TEXT("Backstab geometry passes for 45 deg angle (< 60)"),
				BackstabAbility->TestEvaluateBackstabGeometry(Player, Enemy, Dist2D, AngleDeg));
			TestTrue(TEXT("Angle is approx 45 degrees"), FMath::IsNearlyEqual(AngleDeg, 45.0f, 1.0f));

			// Case 4.3: Angle at 75 degrees (> 60 degrees threshold)
			Enemy->SetActorRotation(FRotator(0.0f, 75.0f, 0.0f));
			TestFalse(TEXT("Backstab geometry rejected for 75 deg angle (> 60)"),
				BackstabAbility->TestEvaluateBackstabGeometry(Player, Enemy, Dist2D, AngleDeg));

			// Case 4.4: Directly from front (Enemy facing towards Player at 180 deg Yaw)
			Enemy->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f)); // Enemy forward is (-1, 0, 0), facing player
			TestFalse(TEXT("Backstab geometry rejected from front (180 deg)"),
				BackstabAbility->TestEvaluateBackstabGeometry(Player, Enemy, Dist2D, AngleDeg));
			TestTrue(TEXT("Front angle relative to back is approx 180 degrees"), AngleDeg >= 179.0f);

			// Case 4.5: Distance too close (< 50cm) or too far (> 250cm)
			Enemy->SetActorRotation(FRotator(0.0f, 0.0f, 0.0f));
			Enemy->SetActorLocation(FVector(30.0f, 0.0f, 0.0f));
			TestFalse(TEXT("Backstab geometry rejected when too close (30cm < 50cm)"),
				BackstabAbility->TestEvaluateBackstabGeometry(Player, Enemy, Dist2D, AngleDeg));

			Enemy->SetActorLocation(FVector(300.0f, 0.0f, 0.0f));
			TestFalse(TEXT("Backstab geometry rejected when too far (300cm > 250cm)"),
				BackstabAbility->TestEvaluateBackstabGeometry(Player, Enemy, Dist2D, AngleDeg));

			// Case 4.6: NaN, Inf, and zero-vector robustness (Fail-Closed Finite Contract)
			float BadDist = 0.0f, BadAngle = 0.0f;
			const FVector GoodPlayerLoc(0.0f, 0.0f, 0.0f);
			const FVector GoodEnemyLoc(150.0f, 0.0f, 0.0f);
			const FVector GoodEnemyForward(1.0f, 0.0f, 0.0f);

			// Player Location NaN / Inf
			TestFalse(TEXT("Backstab geometry rejected for NaN Player X location"),
				UPlayerBackstabExecutionAbility::TestEvaluateBackstabGeometryVectors(
					FVector(NAN, 0.0f, 0.0f), GoodEnemyLoc, GoodEnemyForward, 50.0f, 250.0f, 60.0f, BadDist, BadAngle));

			TestFalse(TEXT("Backstab geometry rejected for Inf Player Y location"),
				UPlayerBackstabExecutionAbility::TestEvaluateBackstabGeometryVectors(
					FVector(0.0f, INFINITY, 0.0f), GoodEnemyLoc, GoodEnemyForward, 50.0f, 250.0f, 60.0f, BadDist, BadAngle));

			// Enemy Location NaN / Inf
			TestFalse(TEXT("Backstab geometry rejected for NaN Enemy Z location"),
				UPlayerBackstabExecutionAbility::TestEvaluateBackstabGeometryVectors(
					GoodPlayerLoc, FVector(150.0f, 0.0f, NAN), GoodEnemyForward, 50.0f, 250.0f, 60.0f, BadDist, BadAngle));

			// Enemy Forward Vector NaN / Inf / Zero
			TestFalse(TEXT("Backstab geometry rejected for NaN Enemy Forward"),
				UPlayerBackstabExecutionAbility::TestEvaluateBackstabGeometryVectors(
					GoodPlayerLoc, GoodEnemyLoc, FVector(NAN, 0.0f, 0.0f), 50.0f, 250.0f, 60.0f, BadDist, BadAngle));

			TestFalse(TEXT("Backstab geometry rejected for zero Enemy Forward vector"),
				UPlayerBackstabExecutionAbility::TestEvaluateBackstabGeometryVectors(
					GoodPlayerLoc, GoodEnemyLoc, FVector(0.0f, 0.0f, 0.0f), 50.0f, 250.0f, 60.0f, BadDist, BadAngle));

			// Coincident positions (zero-distance vector between Player and Enemy)
			TestFalse(TEXT("Backstab geometry rejected for coincident locations (zero distance)"),
				UPlayerBackstabExecutionAbility::TestEvaluateBackstabGeometryVectors(
					FVector(100.0f, 100.0f, 0.0f), FVector(100.0f, 100.0f, 0.0f), GoodEnemyForward, 50.0f, 250.0f, 60.0f, BadDist, BadAngle));

			// Case 4.7: Exact 0-degree threshold contract [0, 90]
			float ZeroAngleDist = 0.0f, ZeroAngleDeg = 0.0f;
			TestTrue(TEXT("Backstab geometry passes for exact 0-degree threshold when perfectly aligned"),
				UPlayerBackstabExecutionAbility::TestEvaluateBackstabGeometryVectors(
					GoodPlayerLoc, GoodEnemyLoc, GoodEnemyForward, 50.0f, 250.0f, 0.0f, ZeroAngleDist, ZeroAngleDeg));
			TestTrue(TEXT("Angle is within 0-deg boundary"), ZeroAngleDeg <= 0.001f);

			// Slightly off-angle (e.g. 5 deg offset) should fail when threshold is 0 deg
			const FVector OffForward = FRotator(0.0f, 5.0f, 0.0f).Vector();
			TestFalse(TEXT("Backstab geometry rejects off-angle when threshold is exact 0 deg"),
				UPlayerBackstabExecutionAbility::TestEvaluateBackstabGeometryVectors(
					GoodPlayerLoc, GoodEnemyLoc, OffForward, 50.0f, 250.0f, 0.0f, ZeroAngleDist, ZeroAngleDeg));

			// Negative threshold (< 0) must be rejected
			TestFalse(TEXT("Backstab geometry rejects negative max angle threshold"),
				UPlayerBackstabExecutionAbility::TestEvaluateBackstabGeometryVectors(
					GoodPlayerLoc, GoodEnemyLoc, GoodEnemyForward, 50.0f, 250.0f, -1.0f, ZeroAngleDist, ZeroAngleDeg));

			// Reset enemy location & rotation
			Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
			Enemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
			Enemy->SetActorRotation(FRotator(0.0f, 0.0f, 0.0f));
		}
		PlayerASC->ClearAbility(BackstabHandle);
	}

	// =========================================================================
	// 5. Target Prerequisites & Stunned Exclusion Gate
	// =========================================================================
	{
		UAnimMontage* SyntheticMontage = NewObject<UAnimMontage>();
		TSubclassOf<UGameplayEffect> DamageGEClass = UTestProjectileDamageGE::StaticClass();

		auto [BackstabHandle, BackstabAbility] = GrantAndConfigureBackstabAbility(Player, SyntheticMontage, DamageGEClass, 50.0f, 250.0f, 60.0f);

		FGameplayAbilityActorInfo ActorInfo;
		ActorInfo.InitFromActor(Player, Player, PlayerASC);

		// 5.1 No target locked -> CanActivateAbility should fail
		Player->TestClearLockedTarget();
		TestFalse(TEXT("Fails activation when no target locked"),
			BackstabAbility->CanActivateAbility(BackstabHandle, &ActorInfo));

		// 5.2 Target is Stunned -> Backstab MUST FAIL (Backstab explicitly excludes Stunned targets!)
		Player->SetTestLockedTarget(Enemy);
		EnemyASC->AddLooseGameplayTag(TagStunned);
		TestFalse(TEXT("Fails activation when target is Stunned (Backstab excludes Stunned)"),
			BackstabAbility->CanActivateAbility(BackstabHandle, &ActorInfo));

		// 5.3 Target is NOT Stunned, living, valid lock & behind geometry -> CanActivateAbility should SUCCEED
		EnemyASC->RemoveLooseGameplayTag(TagStunned);
		TestTrue(TEXT("CanActivateAbility succeeds for living non-stunned target from behind"),
			BackstabAbility->CanActivateAbility(BackstabHandle, &ActorInfo));

		// 5.4 Target is Invulnerable -> Should fail
		EnemyASC->AddLooseGameplayTag(TagInvulnerable);
		TestFalse(TEXT("Fails activation when target is Invulnerable"),
			BackstabAbility->CanActivateAbility(BackstabHandle, &ActorInfo));
		EnemyASC->RemoveLooseGameplayTag(TagInvulnerable);

		// Cleanup
		PlayerASC->ClearAbility(BackstabHandle);
	}

	// =========================================================================
	// 6. Activation & Target Reservation Gate
	// =========================================================================
	{
		UAnimMontage* SyntheticMontage = NewObject<UAnimMontage>();
		TSubclassOf<UGameplayEffect> DamageGEClass = UTestProjectileDamageGE::StaticClass();

		auto [BackstabHandle, BackstabAbility] = GrantAndConfigureBackstabAbility(Player, SyntheticMontage, DamageGEClass, 50.0f, 250.0f, 60.0f);

		Player->SetTestLockedTarget(Enemy);

		// Activate via primary attack input intent
		Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));

		TestNotNull(TEXT("Backstab ability reserved the valid enemy target"),
			BackstabAbility->GetTestReservedTarget());
		TestEqual(TEXT("Reserved target is indeed Enemy"),
			BackstabAbility->GetTestReservedTarget(), Enemy);

		// End ability
		BackstabAbility->TestEndAbility();
		TestNull(TEXT("Target reservation cleared on EndAbility"),
			BackstabAbility->GetTestReservedTarget());

		PlayerASC->ClearAbility(BackstabHandle);
	}

	// =========================================================================
	// 7. Hit Event Routing, Exactly-Once Consumption & Damage Resolution
	// =========================================================================
	{
		UAnimMontage* SyntheticMontage = NewObject<UAnimMontage>();
		TSubclassOf<UGameplayEffect> DamageGEClass = UTestProjectileDamageGE::StaticClass();

		auto [BackstabHandle, BackstabAbility] = GrantAndConfigureBackstabAbility(Player, SyntheticMontage, DamageGEClass, 50.0f, 250.0f, 60.0f);

		// Prepare Enemy
		if (UCharacterAttributeSet* EnemyAttribs = const_cast<UCharacterAttributeSet*>(EnemyASC->GetSet<UCharacterAttributeSet>()))
		{
			EnemyAttribs->SetHealth(100.0f);
		}

		Player->SetTestLockedTarget(Enemy);
		Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));

		// 7.1 Send mismatched notify (wrong event tag) -> No damage
		FGameplayEventData BadPayload;
		BadPayload.EventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Attack.Light.Combo.BranchWindow.Begin")), false);
		BadPayload.Instigator = Player;
		BadPayload.Target = Player;
		BadPayload.OptionalObject = SyntheticMontage;
		BackstabAbility->TestTriggerHitEvent(BadPayload);
		TestFalse(TEXT("Wrong event tag does not consume damage event"), BackstabAbility->IsTestDamageEventConsumed());

		// 7.2 Send mismatched notify (wrong instigator) -> No damage
		BadPayload.EventTag = TagHitEvent;
		BadPayload.Instigator = Enemy;
		BackstabAbility->TestTriggerHitEvent(BadPayload);
		TestFalse(TEXT("Wrong instigator does not consume damage event"), BackstabAbility->IsTestDamageEventConsumed());

		// 7.3 Send mismatched notify (wrong target) -> No damage
		BadPayload.Instigator = Player;
		BadPayload.Target = Enemy;
		BackstabAbility->TestTriggerHitEvent(BadPayload);
		TestFalse(TEXT("Wrong target does not consume damage event"), BackstabAbility->IsTestDamageEventConsumed());

		// 7.4 Send mismatched notify (wrong montage) -> No damage
		UAnimMontage* WrongMontage = NewObject<UAnimMontage>();
		BadPayload.Target = Player;
		BadPayload.OptionalObject = WrongMontage;
		BackstabAbility->TestTriggerHitEvent(BadPayload);
		TestFalse(TEXT("Wrong montage does not consume damage event"), BackstabAbility->IsTestDamageEventConsumed());

		// 7.5 Strict Montage Check: Missing OptionalObject (nullptr) -> No damage
		BadPayload.OptionalObject = nullptr;
		BackstabAbility->TestTriggerHitEvent(BadPayload);
		TestFalse(TEXT("Missing OptionalObject (nullptr) does not consume damage event"), BackstabAbility->IsTestDamageEventConsumed());

		// Record initial target health before correct hit
		const UCharacterAttributeSet* EnemyAttribSet = EnemyASC->GetSet<UCharacterAttributeSet>();
		const float HealthBeforeHit = EnemyAttribSet ? EnemyAttribSet->GetHealth() : 0.0f;
		TestTrue(TEXT("Enemy initial health is positive before backstab hit"), HealthBeforeHit > 0.0f);

		// 7.6 Correct Hit Event Payload -> Consumes damage event and resolves hit exactly once!
		FGameplayEventData GoodPayload;
		GoodPayload.EventTag = TagHitEvent;
		GoodPayload.Instigator = Player;
		GoodPayload.Target = Player;
		GoodPayload.OptionalObject = SyntheticMontage;
		BackstabAbility->TestTriggerHitEvent(GoodPayload);
		TestTrue(TEXT("Correct hit event consumed damage event exactly once"), BackstabAbility->IsTestDamageEventConsumed());

		const float HealthAfterFirstHit = EnemyAttribSet ? EnemyAttribSet->GetHealth() : 0.0f;
		TestTrue(TEXT("First correct hit event applied damage and reduced health"), HealthAfterFirstHit < HealthBeforeHit);

		// 7.7 Second hit event must be ignored (idempotent / exactly-once damage guarantee)
		BackstabAbility->TestTriggerHitEvent(GoodPayload);
		TestTrue(TEXT("Damage event remains consumed, not re-triggered"), BackstabAbility->IsTestDamageEventConsumed());

		const float HealthAfterSecondHit = EnemyAttribSet ? EnemyAttribSet->GetHealth() : 0.0f;
		TestEqual(TEXT("Second hit event does not apply additional damage"), HealthAfterSecondHit, HealthAfterFirstHit);

		// Cleanup
		BackstabAbility->TestEndAbility();
		PlayerASC->ClearAbility(BackstabHandle);

		// 7.8 TryResolveHit returns false (e.g. Friendly Fire / Same Team) -> EndAbility cleans up and prevents retry
		{
			auto [FailedHandle, FailedAbility] = GrantAndConfigureBackstabAbility(Player, SyntheticMontage, DamageGEClass, 50.0f, 250.0f, 60.0f);
			if (UCharacterAttributeSet* EnemyAttribs = const_cast<UCharacterAttributeSet*>(EnemyASC->GetSet<UCharacterAttributeSet>()))
			{
				EnemyAttribs->SetHealth(100.0f);
			}

			Player->SetTestLockedTarget(Enemy);
			Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));

			TestNotNull(TEXT("Ability active with reserved target before failed resolve"), FailedAbility->GetTestReservedTarget());

			// Set Enemy team to match Player team (Team.Player), causing FMeleeHitResolver::TryResolveHit to return false
			const FGameplayTag OriginalEnemyTeam = ICombatTeamAgent::Execute_GetCombatTeamTag(Enemy);
			const FGameplayTag PlayerTeamTag = ICombatTeamAgent::Execute_GetCombatTeamTag(Player);
			Enemy->SetTestCombatTeamTag(PlayerTeamTag);

			const float HealthBeforeFailedHit = EnemyAttribSet ? EnemyAttribSet->GetHealth() : 0.0f;
			FailedAbility->TestTriggerHitEvent(GoodPayload);

			// Failed hit resolution immediately invoked EndAbility, clearing target reservation and stopping ability
			TestNull(TEXT("Failed hit resolution ended ability and cleared target reservation"), FailedAbility->GetTestReservedTarget());
			TestFalse(TEXT("Failed hit resolution terminated active ability"), FailedAbility->IsActive());
			TestEqual(TEXT("Same team target took no damage"), EnemyAttribSet ? EnemyAttribSet->GetHealth() : 0.0f, HealthBeforeFailedHit);

			// Attempting a second hit event after EndAbility must not apply damage / must not retry
			FailedAbility->TestTriggerHitEvent(GoodPayload);
			TestEqual(TEXT("Subsequent hit event after EndAbility does not apply damage"),
				EnemyAttribSet ? EnemyAttribSet->GetHealth() : 0.0f, HealthBeforeFailedHit);

			Enemy->SetTestCombatTeamTag(OriginalEnemyTeam);
			PlayerASC->ClearAbility(FailedHandle);
		}
	}

	// =========================================================================
	// 8. Snapshot Orientation Guarantee (Target Turns After Activation)
	// =========================================================================
	{
		UAnimMontage* SyntheticMontage = NewObject<UAnimMontage>();
		TSubclassOf<UGameplayEffect> DamageGEClass = UTestProjectileDamageGE::StaticClass();

		auto [BackstabHandle, BackstabAbility] = GrantAndConfigureBackstabAbility(Player, SyntheticMontage, DamageGEClass, 50.0f, 250.0f, 60.0f);

		if (UCharacterAttributeSet* EnemyAttribs = const_cast<UCharacterAttributeSet*>(EnemyASC->GetSet<UCharacterAttributeSet>()))
		{
			EnemyAttribs->SetHealth(100.0f);
		}

		// Player at (0,0,0), Enemy at (150,0,0) facing (+1,0,0) -> Player is behind Enemy
		Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
		Enemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
		Enemy->SetActorRotation(FRotator(0.0f, 0.0f, 0.0f));

		Player->SetTestLockedTarget(Enemy);
		Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));

		TestTrue(TEXT("Backstab activated on initial snapshot"), BackstabAbility->IsActive());

		// Enemy now turns around 180 degrees during windup (facing Player at 180 deg)
		Enemy->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f));

		const UCharacterAttributeSet* EnemyAttribSet = EnemyASC->GetSet<UCharacterAttributeSet>();
		const float HealthBeforeTurnHit = EnemyAttribSet ? EnemyAttribSet->GetHealth() : 0.0f;

		// Hit event arrives -> Hit should still SUCCEED per snapshot orientation contract
		FGameplayEventData GoodPayload;
		GoodPayload.EventTag = TagHitEvent;
		GoodPayload.Instigator = Player;
		GoodPayload.Target = Player;
		GoodPayload.OptionalObject = SyntheticMontage;
		BackstabAbility->TestTriggerHitEvent(GoodPayload);

		TestTrue(TEXT("Backstab successfully hit turned enemy via snapshot orientation contract"),
			BackstabAbility->IsTestDamageEventConsumed());
		TestTrue(TEXT("Enemy health was reduced despite turning during execution windup"),
			(EnemyAttribSet ? EnemyAttribSet->GetHealth() : 0.0f) < HealthBeforeTurnHit);

		// Reset enemy rotation
		Enemy->SetActorRotation(FRotator(0.0f, 0.0f, 0.0f));
		BackstabAbility->TestEndAbility();
		PlayerASC->ClearAbility(BackstabHandle);
	}

	// =========================================================================
	// 9. Teardown, Stunned-Gain & Range-Loss Fail-Closed Gates
	// =========================================================================
	{
		UAnimMontage* SyntheticMontage = NewObject<UAnimMontage>();
		TSubclassOf<UGameplayEffect> DamageGEClass = UTestProjectileDamageGE::StaticClass();

		// Case 9.1: Target loses VictimLocked tag during execution -> Ability ends immediately
		{
			auto [BackstabHandle, BackstabAbility] = GrantAndConfigureBackstabAbility(Player, SyntheticMontage, DamageGEClass, 50.0f, 250.0f, 60.0f);
			Player->SetTestLockedTarget(Enemy);
			Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));

			TestNotNull(TEXT("Backstab active with reserved target"), BackstabAbility->GetTestReservedTarget());

			const FGameplayTag TagVictimLocked = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Execution.VictimLocked")), false);
			EnemyASC->RemoveLooseGameplayTag(TagVictimLocked);
			TickBackstabExecutionTestWorld(World, 0.01f);

			TestNull(TEXT("Backstab ended and target reservation cleared when target lost VictimLocked"),
				BackstabAbility->GetTestReservedTarget());

			PlayerASC->ClearAbility(BackstabHandle);
		}

		// Case 9.2: Target escapes distance range before hit event -> Hit rejected without damage
		{
			auto [BackstabHandle, BackstabAbility] = GrantAndConfigureBackstabAbility(Player, SyntheticMontage, DamageGEClass, 50.0f, 250.0f, 60.0f);
			if (UCharacterAttributeSet* EnemyAttribs = const_cast<UCharacterAttributeSet*>(EnemyASC->GetSet<UCharacterAttributeSet>()))
			{
				EnemyAttribs->SetHealth(100.0f);
			}

			Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
			Enemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
			Enemy->SetActorRotation(FRotator(0.0f, 0.0f, 0.0f));

			Player->SetTestLockedTarget(Enemy);
			Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));

			// Enemy escapes to 400cm (> 250cm max distance)
			Enemy->SetActorLocation(FVector(400.0f, 0.0f, 0.0f));

			const UCharacterAttributeSet* EnemyAttribSet = EnemyASC->GetSet<UCharacterAttributeSet>();
			const float HealthBeforeEscape = EnemyAttribSet ? EnemyAttribSet->GetHealth() : 0.0f;

			FGameplayEventData GoodPayload;
			GoodPayload.EventTag = TagHitEvent;
			GoodPayload.Instigator = Player;
			GoodPayload.Target = Player;
			GoodPayload.OptionalObject = SyntheticMontage;
			BackstabAbility->TestTriggerHitEvent(GoodPayload);

			TestEqual(TEXT("Enemy out of range took no damage on hit event"),
				EnemyAttribSet ? EnemyAttribSet->GetHealth() : 0.0f, HealthBeforeEscape);

			// Reset Enemy location
			Enemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
			BackstabAbility->TestEndAbility();
			PlayerASC->ClearAbility(BackstabHandle);
		}
	}

	// =========================================================================
	// 10. Input Routing Priority & Fallback Gates with Direct Primary Assertion
	// =========================================================================
	{
		UAnimMontage* SyntheticMontage = NewObject<UAnimMontage>();
		TSubclassOf<UGameplayEffect> DamageGEClass = UTestProjectileDamageGE::StaticClass();
		const FGameplayTag PrimaryAttackInputTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false);

		// 10.1 Front Execution takes precedence over Backstab when target is Stunned in Front
		{
			// Grant both Front Execution and Backstab Execution
			FGameplayAbilitySpec FrontSpec(UPlayerFrontExecutionAbility::StaticClass(), 1, INDEX_NONE, Player);
			const FGameplayAbilitySpecHandle FrontHandle = PlayerASC->GiveAbility(FrontSpec);
			FGameplayAbilitySpec* FoundFrontSpec = PlayerASC->FindAbilitySpecFromHandle(FrontHandle);
			UPlayerFrontExecutionAbility* FrontInstance = FoundFrontSpec ? Cast<UPlayerFrontExecutionAbility>(FoundFrontSpec->GetPrimaryInstance()) : nullptr;
			if (FrontInstance)
			{
				FrontInstance->SetTestSkipMontageTaskActivation(true);
				FrontInstance->SetTestExecutionMontage(SyntheticMontage);
				FrontInstance->SetTestDamageGameplayEffectClass(DamageGEClass);
				FrontInstance->SetTestExecutionDistances(50.0f, 250.0f);
				FrontInstance->SetTestMaxFrontAngleDegrees(60.0f);
			}

			auto [BackstabHandle, BackstabAbility] = GrantAndConfigureBackstabAbility(Player, SyntheticMontage, DamageGEClass, 50.0f, 250.0f, 60.0f);

			// Target facing Player (180 deg) and Stunned with Poise=0 and active StanceBreak
			Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
			Enemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
			Enemy->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f));
			EnemyASC->AddLooseGameplayTag(TagStunned);
			if (UCharacterAttributeSet* EnemyAttribs = const_cast<UCharacterAttributeSet*>(EnemyASC->GetSet<UCharacterAttributeSet>()))
			{
				EnemyAttribs->SetPoise(0.0f);
			}
			FGameplayAbilitySpec StanceBreakSpec(UEnemyStanceBreakAbility::StaticClass(), 1, INDEX_NONE, Enemy);
			const FGameplayAbilitySpecHandle EnemySBHandle = EnemyASC->GiveAbility(StanceBreakSpec);
			if (FGameplayAbilitySpec* FoundSBSpec = EnemyASC->FindAbilitySpecFromHandle(EnemySBHandle))
			{
				FoundSBSpec->ActivationInfo.SetActivationConfirmed();
				FoundSBSpec->ActiveCount = 1;
			}

			Player->SetTestLockedTarget(Enemy);
			Player->TriggerTestRequestAbilityForInputIntent(PrimaryAttackInputTag);

			TestTrue(TEXT("Front Execution activated in precedence over Backstab"), FrontInstance ? FrontInstance->IsActive() : false);
			TestFalse(TEXT("Backstab was not activated when Front Execution was eligible"), BackstabAbility->IsActive());

			// Cleanup
			if (FrontInstance) FrontInstance->TestEndAbility();
			BackstabAbility->TestEndAbility();
			PlayerASC->ClearAbility(FrontHandle);
			PlayerASC->ClearAbility(BackstabHandle);
			EnemyASC->ClearAbility(EnemySBHandle);
			EnemyASC->RemoveLooseGameplayTag(TagStunned);
		}

		// 10.2 Stunned target attacked from behind -> Front & Backstab both reject -> smoothly falls back to direct Primary!
		{
			FGameplayAbilitySpec FrontSpec(UPlayerFrontExecutionAbility::StaticClass(), 1, INDEX_NONE, Player);
			const FGameplayAbilitySpecHandle FrontHandle = PlayerASC->GiveAbility(FrontSpec);
			auto [BackstabHandle, BackstabAbility] = GrantAndConfigureBackstabAbility(Player, SyntheticMontage, DamageGEClass, 50.0f, 250.0f, 60.0f);

			// Enemy facing away (Yaw 0), Player at (0,0,0) (Player is behind Enemy), Enemy is Stunned
			Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
			Enemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
			Enemy->SetActorRotation(FRotator(0.0f, 0.0f, 0.0f));
			EnemyASC->AddLooseGameplayTag(TagStunned);

			Player->SetTestLockedTarget(Enemy);

			// Establish held-input prerequisite and trigger input handling
			Player->TriggerTestHandleCombatInputStarted(PrimaryAttackInputTag);

			FGameplayAbilitySpec* FoundFrontSpec = PlayerASC->FindAbilitySpecFromHandle(FrontHandle);
			UPlayerFrontExecutionAbility* FrontInstance = FoundFrontSpec ? Cast<UPlayerFrontExecutionAbility>(FoundFrontSpec->GetPrimaryInstance()) : nullptr;

			TestFalse(TEXT("Front execution rejected for behind geometry on Stunned target"), FrontInstance ? FrontInstance->IsActive() : false);
			TestFalse(TEXT("Backstab rejected because target is Stunned"), BackstabAbility->IsActive());

			// Assert direct Primary attack ability spec is accepted and active
			FGameplayAbilitySpec* PrimaryAttackSpec = nullptr;
			for (FGameplayAbilitySpec& Spec : PlayerASC->GetActivatableAbilities())
			{
				if (Spec.Ability && Spec.Ability->IsA<UPrimaryAttackAbility>())
				{
					PrimaryAttackSpec = &Spec;
					break;
				}
			}
			TestNotNull(TEXT("Direct PrimaryAttack ability spec exists on Player ASC"), PrimaryAttackSpec);
			TestTrue(TEXT("Direct PrimaryAttack ability is actively running upon execution fallback (Stunned target behind)"),
				PrimaryAttackSpec && PrimaryAttackSpec->IsActive());

			// End input state and clean up
			Player->TriggerTestHandleCombatInputEnded(PrimaryAttackInputTag);
			TickBackstabExecutionTestWorld(World, 0.01f);

			PlayerASC->ClearAbility(FrontHandle);
			PlayerASC->ClearAbility(BackstabHandle);
			EnemyASC->RemoveLooseGameplayTag(TagStunned);
		}

		// 10.3 No target locked -> Front & Backstab both reject -> fallback to direct Primary
		{
			FGameplayAbilitySpec FrontSpec(UPlayerFrontExecutionAbility::StaticClass(), 1, INDEX_NONE, Player);
			const FGameplayAbilitySpecHandle FrontHandle = PlayerASC->GiveAbility(FrontSpec);
			auto [BackstabHandle, BackstabAbility] = GrantAndConfigureBackstabAbility(Player, SyntheticMontage, DamageGEClass, 50.0f, 250.0f, 60.0f);

			Player->TestClearLockedTarget();

			// Establish held-input prerequisite and trigger input handling
			Player->TriggerTestHandleCombatInputStarted(PrimaryAttackInputTag);

			FGameplayAbilitySpec* FoundFrontSpec = PlayerASC->FindAbilitySpecFromHandle(FrontHandle);
			UPlayerFrontExecutionAbility* FrontInstance = FoundFrontSpec ? Cast<UPlayerFrontExecutionAbility>(FoundFrontSpec->GetPrimaryInstance()) : nullptr;

			TestFalse(TEXT("Front execution rejected when no target locked"), FrontInstance ? FrontInstance->IsActive() : false);
			TestFalse(TEXT("Backstab rejected when no target locked"), BackstabAbility->IsActive());

			// Assert direct Primary attack ability spec is accepted and active
			FGameplayAbilitySpec* PrimaryAttackSpec = nullptr;
			for (FGameplayAbilitySpec& Spec : PlayerASC->GetActivatableAbilities())
			{
				if (Spec.Ability && Spec.Ability->IsA<UPrimaryAttackAbility>())
				{
					PrimaryAttackSpec = &Spec;
					break;
				}
			}
			TestNotNull(TEXT("Direct PrimaryAttack ability spec exists on Player ASC"), PrimaryAttackSpec);
			TestTrue(TEXT("Direct PrimaryAttack ability is actively running upon execution fallback (No target locked)"),
				PrimaryAttackSpec && PrimaryAttackSpec->IsActive());

			// End input state and clean up
			Player->TriggerTestHandleCombatInputEnded(PrimaryAttackInputTag);
			TickBackstabExecutionTestWorld(World, 0.01f);

			PlayerASC->ClearAbility(FrontHandle);
			PlayerASC->ClearAbility(BackstabHandle);
		}
	}

	// =========================================================================
	// 11. Generation Isolation, Reentrancy & Stale Callback Rejection Gate
	// =========================================================================
	{
		UAnimMontage* SyntheticMontage = NewObject<UAnimMontage>();
		TSubclassOf<UGameplayEffect> DamageGEClass = UTestProjectileDamageGE::StaticClass();

		auto [BackstabHandle, BackstabAbility] = GrantAndConfigureBackstabAbility(Player, SyntheticMontage, DamageGEClass, 50.0f, 250.0f, 60.0f);

		Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
		Enemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
		Enemy->SetActorRotation(FRotator(0.0f, 0.0f, 0.0f));
		Player->SetTestLockedTarget(Enemy);

		// --- Generation 1 (Activation A) ---
		Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));
		TestTrue(TEXT("Activation A is active"), BackstabAbility->IsActive());

		const uint32 TokenA = BackstabAbility->GetTestActivationToken();
		UPlayerBackstabExecutionContext* ContextA = BackstabAbility->GetTestActiveContext();
		TestNotNull(TEXT("Context A exists for Generation A"), ContextA);
		TestEqual(TEXT("Context A token matches Token A"), ContextA ? ContextA->Token : 0, TokenA);

		// End Activation A
		BackstabAbility->TestEndAbility();
		TestFalse(TEXT("Activation A ended"), BackstabAbility->IsActive());
		TestNull(TEXT("Context A cleared on EndAbility"), BackstabAbility->GetTestActiveContext());

		// Verify ActivationOwnedTags cleanly removed after EndAbility
		TestFalse(TEXT("EndAbility cleared State.Action.Attacking"),
			PlayerASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Attacking")), false)));
		TestFalse(TEXT("EndAbility cleared State.Input.Block.Movement"),
			PlayerASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Movement")), false)));
		TestFalse(TEXT("EndAbility cleared State.Input.Block.Jump"),
			PlayerASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Jump")), false)));

		// --- Generation 2 (Activation B) with identical target and montage pointers ---
		Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));
		TestTrue(TEXT("Activation B is active"), BackstabAbility->IsActive());

		const uint32 TokenB = BackstabAbility->GetTestActivationToken();
		UPlayerBackstabExecutionContext* ContextB = BackstabAbility->GetTestActiveContext();
		TestNotNull(TEXT("Context B exists for Generation B"), ContextB);
		TestTrue(TEXT("Token B is greater than Token A"), TokenB > TokenA);

		// Case 11.1: Stale Montage completed from Generation A must NOT terminate Activation B
		if (ContextA)
		{
			ContextA->OnMontageCompleted();
			TestTrue(TEXT("Stale MontageCompleted from Generation A was rejected; Activation B remains active"),
				BackstabAbility->IsActive());

			// Case 11.2: Stale TargetDestroyed from Generation A must NOT terminate Activation B
			ContextA->OnTargetDestroyed(Enemy);
			TestTrue(TEXT("Stale TargetDestroyed from Generation A was rejected; Activation B remains active"),
				BackstabAbility->IsActive());

			// Case 11.3: Stale Hit Event from Generation A must NOT trigger or consume in Activation B
			FGameplayEventData StalePayload;
			StalePayload.EventTag = TagHitEvent;
			StalePayload.Instigator = Player;
			StalePayload.Target = Player;
			StalePayload.OptionalObject = SyntheticMontage;
			ContextA->OnHitEventReceived(StalePayload);
			TestFalse(TEXT("Stale Hit Event from Generation A was rejected and did not consume damage event"),
				BackstabAbility->IsTestDamageEventConsumed());
		}

		// Case 11.4: Valid Montage completed from Generation B MUST terminate Activation B
		if (ContextB)
		{
			ContextB->OnMontageCompleted();
			TestFalse(TEXT("Valid MontageCompleted from Generation B terminated Activation B"),
				BackstabAbility->IsActive());
		}

		// Case 11.5: Re-activation with failed prerequisite must fail-closed without leaking owned tags
		EnemyASC->AddLooseGameplayTag(TagStunned); // Invalid prerequisite for Backstab
		Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));

		TestFalse(TEXT("Re-activation with Stunned target failed activation"), BackstabAbility->IsActive());
		TestFalse(TEXT("Failed activation did not leak State.Action.Attacking tag"),
			PlayerASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Attacking")), false)));
		TestFalse(TEXT("Failed activation did not leak State.Input.Block.Movement tag"),
			PlayerASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Movement")), false)));
		TestFalse(TEXT("Failed activation did not leak State.Input.Block.Jump tag"),
			PlayerASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Jump")), false)));

		EnemyASC->RemoveLooseGameplayTag(TagStunned);
		PlayerASC->ClearAbility(BackstabHandle);
	}

	// =========================================================================
	// 12. Active Ability + Inactive Startup Task Must Converge Through Cleanup
	// =========================================================================
	{
		UAnimMontage* SyntheticMontage = NewObject<UAnimMontage>();
		TSubclassOf<UGameplayEffect> DamageGEClass = UTestProjectileDamageGE::StaticClass();
		auto [FailureHandle, FailureAbility] = GrantAndConfigureBackstabAbility(Player, SyntheticMontage, DamageGEClass, 50.0f, 250.0f, 60.0f);

		if (TestNotNull(TEXT("Backstab ability instance valid for inactive-task cleanup test"), FailureAbility))
		{
			FailureAbility->SetTestInvalidateWaitHitEventTaskAfterReady(true);
			Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
			Enemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
			Enemy->SetActorRotation(FRotator(0.0f, 0.0f, 0.0f));
			Player->SetTestLockedTarget(Enemy);
			Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));

			TestFalse(TEXT("Inactive WaitGameplayEvent task ends the active Backstab ability"), FailureAbility->IsActive());
			TestNull(TEXT("Inactive-task failure clears the reserved target"), FailureAbility->GetTestReservedTarget());
			TestFalse(TEXT("Inactive-task failure clears State.Action.Attacking"),
				PlayerASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Attacking")), false)));
			TestFalse(TEXT("Inactive-task failure clears State.Input.Block.Movement"),
				PlayerASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Movement")), false)));
			TestFalse(TEXT("Inactive-task failure clears State.Input.Block.Jump"),
				PlayerASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Jump")), false)));
		}

		PlayerASC->ClearAbility(FailureHandle);
	}

	// =========================================================================
	// 13. ReadyForActivation Synchronous Re-entry Fail-Closed & Re-activation Gate
	// =========================================================================
	{
		UAnimMontage* SyntheticMontage = NewObject<UAnimMontage>();
		TSubclassOf<UGameplayEffect> DamageGEClass = UTestProjectileDamageGE::StaticClass();

		auto [ReentryHandle, ReentryAbility] = GrantAndConfigureBackstabAbility(Player, SyntheticMontage, DamageGEClass, 50.0f, 250.0f, 60.0f);

		Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
		Enemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
		Enemy->SetActorRotation(FRotator(0.0f, 0.0f, 0.0f));
		Player->SetTestLockedTarget(Enemy);

		// Configure simulated synchronous EndAbility during task ReadyForActivation
		ReentryAbility->SetTestEndAbilityDuringTaskReady(true);
		Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));

		// Verify ability ended cleanly without double-EndAbility crash, and tags/context are cleanly reset
		TestFalse(TEXT("Synchronous EndAbility in ReadyForActivation terminated Backstab cleanly"), ReentryAbility->IsActive());
		TestNull(TEXT("Target reservation cleared after synchronous end"), ReentryAbility->GetTestReservedTarget());
		TestFalse(TEXT("PlayerLocked tag not leaked after synchronous end"),
			PlayerASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Execution.PlayerLocked")), false)));

		// Re-enable normal activation and verify next activation succeeds cleanly
		ReentryAbility->SetTestEndAbilityDuringTaskReady(false);
		Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));
		TestTrue(TEXT("Subsequent activation after synchronous end succeeds"), ReentryAbility->IsActive());
		TestNotNull(TEXT("Subsequent activation reserved target"), ReentryAbility->GetTestReservedTarget());

		ReentryAbility->TestEndAbility();
		PlayerASC->ClearAbility(ReentryHandle);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
