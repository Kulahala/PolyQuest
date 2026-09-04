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
#include "Components/BoxComponent.h"
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

	auto ResetWeaponExecutionMontages = [&](APlayerCharacter* InPlayer)
	{
		if (UWeaponEquipmentComponent* EquipComp = InPlayer ? InPlayer->FindComponentByClass<UWeaponEquipmentComponent>() : nullptr)
		{
			if (UMeleeWeaponDefinition* WeaponDef = EquipComp->GetEquippedMainHandMelee())
			{
				WeaponDef->MinExecutionDistance = 0.0f;
				WeaponDef->MaxExecutionDistance = 250.0f;
				WeaponDef->ExecutionSnapDistance = 190.0f;
				WeaponDef->FrontExecutionMontage = nullptr;
				WeaponDef->BackstabExecutionMontage = nullptr;
			}
		}
	};

	auto GrantAndConfigureExecAbility = [&](APlayerCharacter* InPlayer, UAnimMontage* Montage, TSubclassOf<UGameplayEffect> DamageClass, float MinDist, float MaxDist, float MaxAngle) -> TPair<FGameplayAbilitySpecHandle, UPlayerFrontExecutionAbility*>
	{
		ResetWeaponExecutionMontages(InPlayer);
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
		ResetWeaponExecutionMontages(Player);
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

		// 5.5 Weapon ExecutionSnapDistance range check gate [MinExecutionDistance, MaxExecutionDistance]
		{
			UWeaponEquipmentComponent* EquipComp = Player->FindComponentByClass<UWeaponEquipmentComponent>();
			UMeleeWeaponDefinition* WeaponDef = EquipComp ? EquipComp->GetEquippedMainHandMelee() : nullptr;
			if (TestNotNull(TEXT("Main hand melee weapon exists for snap check"), WeaponDef))
			{
				const float OriginalSnapDist = WeaponDef->ExecutionSnapDistance;

				WeaponDef->ExecutionSnapDistance = 300.0f;
				TestFalse(TEXT("CanActivateAbility fails when ExecutionSnapDistance > MaxExecutionDistance"),
					ExecAbility->CanActivateAbility(ExecHandle, &ActorInfo));

				WeaponDef->ExecutionSnapDistance = 20.0f;
				TestFalse(TEXT("CanActivateAbility fails when ExecutionSnapDistance < MinExecutionDistance"),
					ExecAbility->CanActivateAbility(ExecHandle, &ActorInfo));

				WeaponDef->ExecutionSnapDistance = -10.0f;
				TestFalse(TEXT("CanActivateAbility fails when ExecutionSnapDistance is negative"),
					ExecAbility->CanActivateAbility(ExecHandle, &ActorInfo));

				WeaponDef->ExecutionSnapDistance = OriginalSnapDist;
				TestTrue(TEXT("CanActivateAbility succeeds after restoring valid ExecutionSnapDistance"),
					ExecAbility->CanActivateAbility(ExecHandle, &ActorInfo));
			}
		}

		// 5.6 Weapon Execution Montage Gate: Missing FrontExecutionMontage rejects CanActivateAbility
		{
			ResetWeaponExecutionMontages(Player);
			TestFalse(TEXT("CanActivateAbility fails when FrontExecutionMontage is null"),
				ExecAbility->CanActivateAbility(ExecHandle, &ActorInfo));

			// Even if BackstabExecutionMontage is set, Front execution must reject it
			UAnimMontage* BackstabOnlyMontage = NewObject<UAnimMontage>();
			if (UWeaponEquipmentComponent* EquipComp = Player->FindComponentByClass<UWeaponEquipmentComponent>())
			{
				if (UMeleeWeaponDefinition* WeaponDef = EquipComp->GetEquippedMainHandMelee())
				{
					WeaponDef->BackstabExecutionMontage = BackstabOnlyMontage;
				}
			}
			TestFalse(TEXT("Front CanActivateAbility fails when only BackstabExecutionMontage is configured"),
				ExecAbility->CanActivateAbility(ExecHandle, &ActorInfo));

			// Restore FrontExecutionMontage
			ExecAbility->SetTestExecutionMontage(SyntheticMontage);
			TestTrue(TEXT("CanActivateAbility succeeds after restoring FrontExecutionMontage"),
				ExecAbility->CanActivateAbility(ExecHandle, &ActorInfo));
		}

		// Cleanup
		PlayerASC->ClearAbility(ExecHandle);
		EnemyASC->RemoveLooseGameplayTag(TagStunned);
		ResetWeaponExecutionMontages(Player);
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

		TestTrue(TEXT("Front execution ability is active"), ExecAbility->IsActive());
		TestEqual(TEXT("Active execution montage snapshot matches SyntheticMontage"),
			ExecAbility->GetTestActiveExecutionMontage(), SyntheticMontage);
		UWeaponEquipmentComponent* PlayerEquip = Player->FindComponentByClass<UWeaponEquipmentComponent>();
		const UMeleeWeaponDefinition* ExpectedWeapon = PlayerEquip ? PlayerEquip->GetEquippedMainHandMelee() : nullptr;
		TestEqual(TEXT("Active execution weapon definition snapshot matches equipped melee"),
			ExecAbility->GetTestActiveExecutionWeaponDefinition(), ExpectedWeapon);

		// Post-activation weapon mutation does NOT alter active snapshot
		UAnimMontage* MutatedMontage = NewObject<UAnimMontage>();
		if (UMeleeWeaponDefinition* WeaponDef = PlayerEquip ? PlayerEquip->GetEquippedMainHandMelee() : nullptr)
		{
			WeaponDef->FrontExecutionMontage = MutatedMontage;
			WeaponDef->MinExecutionDistance = 30.0f;
			WeaponDef->MaxExecutionDistance = 220.0f;
			WeaponDef->ExecutionSnapDistance = 160.0f;
		}
		TestEqual(TEXT("Active execution montage snapshot remains stable after weapon definition mutation"),
			ExecAbility->GetTestActiveExecutionMontage(), SyntheticMontage);
		TestEqual(TEXT("Active execution min distance snapshot remains stable after weapon definition mutation"),
			ExecAbility->GetTestActiveMinExecutionDistance(), 50.0f);
		TestEqual(TEXT("Active execution max distance snapshot remains stable after weapon definition mutation"),
			ExecAbility->GetTestActiveMaxExecutionDistance(), 250.0f);
		TestEqual(TEXT("Active execution snap distance snapshot remains stable after weapon definition mutation"),
			ExecAbility->GetTestActiveExecutionSnapDistance(), 190.0f);
		TestTrue(TEXT("Active execution distance snapshot flag is set"),
			ExecAbility->HasTestActiveExecutionDistanceSnapshot());

		// Equipment swap gating: EquipWeapon and TryEquipWorldPickup must be refused while ability is active
		UMeleeWeaponDefinition* SecondaryWeapon = NewObject<UMeleeWeaponDefinition>(Player, NAME_None, RF_Transient);
		SecondaryWeapon->HandSlot = EWeaponHandSlot::MainHandOneHanded;
		SecondaryWeapon->AttachSocketName = FName(TEXT("Weapon_R"));
		SecondaryWeapon->bUseOwnerMeshSocketForTrace = true;
		TestFalse(TEXT("EquipWeapon refused while front execution is active"),
			PlayerEquip->EquipWeapon(SecondaryWeapon));
		TestFalse(TEXT("TryEquipWorldPickup refused while front execution is active"),
			PlayerEquip->TryEquipWorldPickup(nullptr));

		// Verify Snap Transform: Enemy is at (150, 0, 0) facing (-1, 0, 0), Distance is 190
		// Expected Player Location: (150 - 190, 0, 0) = (-40, 0, 0)
		// Expected Player Facing: -(-1, 0, 0) = (+1, 0, 0) -> Yaw = 0 deg
		const FVector ExpectedSnapLoc(-40.0f, 0.0f, 0.0f);
		TestTrue(TEXT("Player location snapped to target front within 1cm"),
			FVector::Dist(Player->GetActorLocation(), ExpectedSnapLoc) <= 1.0f);
		TestTrue(TEXT("Player rotation snapped facing target within 1deg"),
			FMath::Abs(FMath::FindDeltaAngleDegrees(Player->GetActorRotation().Yaw, 0.0f)) <= 1.0f);

		// Verify Enemy entered MOVE_None under victim execution lock
		if (UCharacterMovementComponent* EnemyMove = Enemy->GetCharacterMovement())
		{
			TestEqual(TEXT("Enemy movement mode is MOVE_None under execution lock"),
				EnemyMove->MovementMode, MOVE_None);
		}

		// End ability
		ExecAbility->TestEndAbility();
		TestNull(TEXT("Target reservation cleared on EndAbility"),
			ExecAbility->GetTestReservedTarget());
		TestNull(TEXT("Active execution montage snapshot cleared on EndAbility"),
			ExecAbility->GetTestActiveExecutionMontage());
		TestNull(TEXT("Active execution weapon definition snapshot cleared on EndAbility"),
			ExecAbility->GetTestActiveExecutionWeaponDefinition());

		// Verify Enemy restored to MOVE_Walking after release
		if (UCharacterMovementComponent* EnemyMove = Enemy->GetCharacterMovement())
		{
			TestEqual(TEXT("Enemy movement mode restored to MOVE_Walking after release"),
				EnemyMove->MovementMode, MOVE_Walking);
		}

		// Reset Player location for subsequent sections
		Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
		Player->SetActorRotation(FRotator::ZeroRotator);

		PlayerASC->ClearAbility(ExecHandle);
		EnemyASC->ClearAbility(StanceBreakHandle);
		EnemyASC->RemoveLooseGameplayTag(TagStunned);
		ResetWeaponExecutionMontages(Player);
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
			ResetWeaponExecutionMontages(Player);
		}
	}

	// =========================================================================
	// 8. Teardown, Stunned-Loss & Motion-Warp Cleanup Verification
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
		ResetWeaponExecutionMontages(Player);
	}

	// =========================================================================
	// 9. ReadyForActivation Synchronous Re-entry Fail-Closed & Re-activation Gate
	// =========================================================================
	{
		UAnimMontage* SyntheticMontage = NewObject<UAnimMontage>();
		TSubclassOf<UGameplayEffect> DamageGEClass = UTestProjectileDamageGE::StaticClass();

		auto [ReentryHandle, ReentryAbility] = GrantAndConfigureExecAbility(Player, SyntheticMontage, DamageGEClass, 50.0f, 250.0f, 60.0f);

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
		ResetWeaponExecutionMontages(Player);
	}

	// =========================================================================
	// 10. Environment Blocking Snap Rollback, Zero Velocity & Formal Release Gate
	// =========================================================================
	{
		UAnimMontage* SyntheticMontage = NewObject<UAnimMontage>();
		TSubclassOf<UGameplayEffect> DamageGEClass = UTestProjectileDamageGE::StaticClass();

		Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
		Player->SetActorRotation(FRotator::ZeroRotator);
		Enemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
		Enemy->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f)); // Enemy forward is (-1, 0, 0)

		auto [BlockExecHandle, BlockExecAbility] = GrantAndConfigureExecAbility(Player, SyntheticMontage, DamageGEClass, 50.0f, 250.0f, 60.0f);

		EnemyASC->AddLooseGameplayTag(TagStunned);
		if (UCharacterAttributeSet* EnemyAttribs = const_cast<UCharacterAttributeSet*>(EnemyASC->GetSet<UCharacterAttributeSet>()))
		{
			EnemyAttribs->SetPoise(0.0f);
		}
		const FGameplayAbilitySpecHandle StanceBreakHandle = GrantEnemyStanceBreakAbility(Enemy);

		Player->SetTestLockedTarget(Enemy);

		// Spawn temporary blocking actor at the snap target location (-40, 0, 0)
		AActor* BlockingActor = World->SpawnActor<AActor>();
		if (TestNotNull(TEXT("Temporary blocking actor spawned"), BlockingActor))
		{
			UBoxComponent* BoxComp = NewObject<UBoxComponent>(BlockingActor);
			BoxComp->InitBoxExtent(FVector(50.0f, 50.0f, 100.0f));
			BoxComp->SetCollisionProfileName(TEXT("BlockAll"));
			BlockingActor->SetRootComponent(BoxComp);
			BoxComp->RegisterComponent();
			BlockingActor->SetActorLocation(FVector(-40.0f, 0.0f, 0.0f));

			const FVector OriginalPlayerLoc = Player->GetActorLocation();
			const FRotator OriginalPlayerRot = Player->GetActorRotation();

			// Trigger execution attempt into the blocking actor
			Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));

			// Snap must fail, ability must end via cancel, and player transform rolled back
			TestFalse(TEXT("Execution aborted cleanly when snap is blocked"), BlockExecAbility->IsActive());
			TestEqual(TEXT("Player location rolled back to original on block"), Player->GetActorLocation(), OriginalPlayerLoc);
			TestTrue(TEXT("Player rotation rolled back to original on block"),
				FMath::Abs(FMath::FindDeltaAngleDegrees(Player->GetActorRotation().Yaw, OriginalPlayerRot.Yaw)) <= 1.0f);
			TestTrue(TEXT("Player velocity cleared on block rollback"), Player->GetVelocity().IsNearlyZero());
			TestNull(TEXT("Target reservation cleared after blocked snap abort"), BlockExecAbility->GetTestReservedTarget());

			// Victim must be formally released and restored to MOVE_Walking
			if (UCharacterMovementComponent* EnemyMove = Enemy->GetCharacterMovement())
			{
				TestEqual(TEXT("Victim restored to MOVE_Walking after blocked execution cancel"), EnemyMove->MovementMode, MOVE_Walking);
			}

			BlockingActor->Destroy();
		}

		EnemyASC->RemoveLooseGameplayTag(TagStunned);
		PlayerASC->ClearAbility(BlockExecHandle);
		EnemyASC->ClearAbility(StanceBreakHandle);
		ResetWeaponExecutionMontages(Player);
	}

	// =========================================================================
	// 11. Preserving Other Abilities' Motion Warp Targets Across EndAbility
	// =========================================================================
	{
		UAnimMontage* SyntheticMontage = NewObject<UAnimMontage>();
		TSubclassOf<UGameplayEffect> DamageGEClass = UTestProjectileDamageGE::StaticClass();

		Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
		Player->SetActorRotation(FRotator::ZeroRotator);
		Enemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
		Enemy->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f));

		const FName RegularWarpTargetName(TEXT("MeleeContact"));
		Player->SetMeleeMotionWarpTarget(RegularWarpTargetName, FTransform(FRotator::ZeroRotator, FVector(500.0f, 0.0f, 0.0f)));
		TestTrue(TEXT("Regular motion warp target registered before execution"),
			Player->HasTestMeleeMotionWarpTarget(RegularWarpTargetName));

		auto [ExecHandle, ExecAbility] = GrantAndConfigureExecAbility(Player, SyntheticMontage, DamageGEClass, 50.0f, 250.0f, 60.0f);

		EnemyASC->AddLooseGameplayTag(TagStunned);
		if (UCharacterAttributeSet* EnemyAttribs = const_cast<UCharacterAttributeSet*>(EnemyASC->GetSet<UCharacterAttributeSet>()))
		{
			EnemyAttribs->SetPoise(0.0f);
		}
		const FGameplayAbilitySpecHandle StanceBreakHandle = GrantEnemyStanceBreakAbility(Enemy);

		Player->SetTestLockedTarget(Enemy);
		Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));

		TestTrue(TEXT("Execution activated successfully"), ExecAbility->IsActive());

		// End ability
		ExecAbility->TestEndAbility();
		TestFalse(TEXT("Execution ability ended"), ExecAbility->IsActive());

		// Invariant: Regular attack's Motion Warp target must NOT have been cleared!
		TestTrue(TEXT("Regular motion warp target preserved after execution EndAbility"),
			Player->HasTestMeleeMotionWarpTarget(RegularWarpTargetName));

		// Cleanup
		Player->ClearMeleeMotionWarpTargets();
		EnemyASC->RemoveLooseGameplayTag(TagStunned);
		PlayerASC->ClearAbility(ExecHandle);
		EnemyASC->ClearAbility(StanceBreakHandle);
		ResetWeaponExecutionMontages(Player);
	}

	// =========================================================================
	// 12. Missing Direction Fallback, Live Identity Mismatch & Force Commit Failure Gates
	// =========================================================================
	{
		// 12.1 Missing FrontExecutionMontage: Input does not activate Front execution, does not establish lock
		{
			ResetWeaponExecutionMontages(Player);
			EnemyASC->AddLooseGameplayTag(TagStunned);
			if (UCharacterAttributeSet* EnemyAttribs = const_cast<UCharacterAttributeSet*>(EnemyASC->GetSet<UCharacterAttributeSet>()))
			{
				EnemyAttribs->SetPoise(0.0f);
			}
			const FGameplayAbilitySpecHandle StanceBreakHandle = GrantEnemyStanceBreakAbility(Enemy);
			Player->SetTestLockedTarget(Enemy);

			// Weapon has NO FrontExecutionMontage (only BackstabExecutionMontage configured)
			UAnimMontage* BackstabOnlyMontage = NewObject<UAnimMontage>();
			if (UWeaponEquipmentComponent* EquipComp = Player->FindComponentByClass<UWeaponEquipmentComponent>())
			{
				if (UMeleeWeaponDefinition* WeaponDef = EquipComp->GetEquippedMainHandMelee())
				{
					WeaponDef->BackstabExecutionMontage = BackstabOnlyMontage;
				}
			}

			FGameplayAbilitySpec Spec(UPlayerFrontExecutionAbility::StaticClass(), 1, INDEX_NONE, Player);
			const FGameplayAbilitySpecHandle ExecHandle = PlayerASC->GiveAbility(Spec);

			// Trigger PrimaryAttack input intent
			Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));

			FGameplayAbilitySpec* FoundSpec = PlayerASC->FindAbilitySpecFromHandle(ExecHandle);
			UPlayerFrontExecutionAbility* ExecAbility = FoundSpec ? Cast<UPlayerFrontExecutionAbility>(FoundSpec->GetPrimaryInstance()) : nullptr;
			if (ExecAbility)
			{
				TestFalse(TEXT("Front execution does not activate when FrontExecutionMontage is missing"), ExecAbility->IsActive());
				TestNull(TEXT("No target reservation when FrontExecutionMontage is missing"), ExecAbility->GetTestReservedTarget());
			}

			if (UCharacterMovementComponent* EnemyMove = Enemy->GetCharacterMovement())
			{
				TestTrue(TEXT("Enemy movement mode is NOT MOVE_None when front execution rejected"), EnemyMove->MovementMode != MOVE_None);
			}

			PlayerASC->ClearAbility(ExecHandle);
			EnemyASC->ClearAbility(StanceBreakHandle);
			EnemyASC->RemoveLooseGameplayTag(TagStunned);
			ResetWeaponExecutionMontages(Player);
		}

		// 12.2 Force Commit Failure (Fail-Closed Gate): Aborts immediately without establishing reservation or victim lock
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

			ExecAbility->SetTestForceCommitAbilityFailure(true);

			Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));

			TestFalse(TEXT("Ability aborted when Commit fails"), ExecAbility->IsActive());
			TestNull(TEXT("No reservation created when Commit fails"), ExecAbility->GetTestReservedTarget());
			TestNull(TEXT("Active montage snapshot cleared on commit failure abort"), ExecAbility->GetTestActiveExecutionMontage());
			TestNull(TEXT("Active weapon snapshot cleared on commit failure abort"), ExecAbility->GetTestActiveExecutionWeaponDefinition());
			TestFalse(TEXT("Active distance snapshot flag cleared on commit failure abort"), ExecAbility->HasTestActiveExecutionDistanceSnapshot());

			if (UCharacterMovementComponent* EnemyMove = Enemy->GetCharacterMovement())
			{
				TestTrue(TEXT("Enemy movement mode is NOT MOVE_None on commit failure"), EnemyMove->MovementMode != MOVE_None);
			}

			ExecAbility->SetTestForceCommitAbilityFailure(false);
			PlayerASC->ClearAbility(ExecHandle);
			EnemyASC->ClearAbility(StanceBreakHandle);
			EnemyASC->RemoveLooseGameplayTag(TagStunned);
			ResetWeaponExecutionMontages(Player);
		}

		// 12.3 Invalid Weapon Execution Distance Range (Fail-Closed Gate): Rejects activation before reservation or lock
		{
			UAnimMontage* SyntheticMontage = NewObject<UAnimMontage>();
			TSubclassOf<UGameplayEffect> DamageGEClass = UTestProjectileDamageGE::StaticClass();

			auto [ExecHandle, ExecAbility] = GrantAndConfigureExecAbility(Player, SyntheticMontage, DamageGEClass, 0.0f, 250.0f, 60.0f);

			// Corrupt weapon distance range: Max > 250cm violates project hard cap
			if (UWeaponEquipmentComponent* EquipComp = Player->FindComponentByClass<UWeaponEquipmentComponent>())
			{
				if (UMeleeWeaponDefinition* WeaponDef = EquipComp->GetEquippedMainHandMelee())
				{
					WeaponDef->MaxExecutionDistance = 300.0f;
				}
			}

			EnemyASC->AddLooseGameplayTag(TagStunned);
			if (UCharacterAttributeSet* EnemyAttribs = const_cast<UCharacterAttributeSet*>(EnemyASC->GetSet<UCharacterAttributeSet>()))
			{
				EnemyAttribs->SetPoise(0.0f);
			}
			const FGameplayAbilitySpecHandle StanceBreakHandle = GrantEnemyStanceBreakAbility(Enemy);
			Player->SetTestLockedTarget(Enemy);

			Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));

			TestFalse(TEXT("Front execution does not activate when weapon distance range is invalid"), ExecAbility->IsActive());
			TestNull(TEXT("No target reservation when weapon distance range is invalid"), ExecAbility->GetTestReservedTarget());
			TestFalse(TEXT("No active distance snapshot when weapon distance range is invalid"), ExecAbility->HasTestActiveExecutionDistanceSnapshot());

			if (UCharacterMovementComponent* EnemyMove = Enemy->GetCharacterMovement())
			{
				TestTrue(TEXT("Enemy movement mode is NOT MOVE_None when distance range invalid"), EnemyMove->MovementMode != MOVE_None);
			}

			PlayerASC->ClearAbility(ExecHandle);
			EnemyASC->ClearAbility(StanceBreakHandle);
			EnemyASC->RemoveLooseGameplayTag(TagStunned);
			ResetWeaponExecutionMontages(Player);
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
