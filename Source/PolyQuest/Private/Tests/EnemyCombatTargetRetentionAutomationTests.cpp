#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "AI/EnemyAIController.h"
#include "AI/EnemyAIProfile.h"
#include "Animation/AnimMontage.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Character/Player/PlayerCharacter.h"
#include "Combat/Enemy/EnemyAttackProfile.h"
#include "Combat/Enemy/EnemyAttackSet.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "GameplayEffect.h"
#include "Tests/CombatAutomationFixture.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEnemyCombatTargetRetentionAutomationTest,
	"PolyQuest.Enemy.CombatTargetRetention",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyCombatTargetRetentionAutomationTest::RunTest(const FString& Parameters)
{
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("EnemyCombatTargetRetentionTestWorld"));
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

	// 1. Transient data asset authoring for clean OnPossess initialization
	UEnemyAttackProfile* AttackProfile = NewObject<UEnemyAttackProfile>(GetTransientPackage(), TEXT("Test_AttackProfile_Retention"));
	UAnimMontage* DummyMontage = NewObject<UAnimMontage>(GetTransientPackage(), TEXT("Test_DummyMontage_Retention"));
	AttackProfile->SetTestMontage(DummyMontage);
	AttackProfile->SetTestDamageEffectClass(UGameplayEffect::StaticClass());
	AttackProfile->SetTestAttackRange(180.0f);
	AttackProfile->SetTestCooldown(1.0f);
	AttackProfile->SetTestGuardStaminaDamage(25.0f);

	UEnemyAttackSet* AttackSet = NewObject<UEnemyAttackSet>(GetTransientPackage(), TEXT("Test_AttackSet_Retention"));
	AttackSet->SetTestEngagementRange(300.0f);
	AttackSet->AddTestEntry(AttackProfile, 1.0f);

	UEnemyAIProfile* AIProfile = NewObject<UEnemyAIProfile>(GetTransientPackage(), TEXT("Test_AIProfile_Retention"));
	AIProfile->SetTestPreferredCombatDistance(180.0f);
	AIProfile->SetTestLateralRepositionDistance(120.0f);
	AIProfile->SetTestRepositionAcceptanceRadius(40.0f);
	AIProfile->SetTestRepositionRetryDelay(0.25f);
	AIProfile->SetTestLeashRadius(2500.0f);
	AIProfile->SetTestApproachTimeout(3.0f);

	// 2. Spawn actors and initialize controller
	APlayerCharacter* Player = FCombatAutomationFixture::SpawnPlayer(World, FTransform(FRotator::ZeroRotator, FVector(300.0f, 0.0f, 0.0f)));
	AEnemyCharacter* Enemy = FCombatAutomationFixture::SpawnPassiveEnemy(World, FTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, 0.0f)));

	if (!TestNotNull(TEXT("Player spawned successfully"), Player) || !TestNotNull(TEXT("Enemy spawned successfully"), Enemy))
	{
		return false;
	}

	Enemy->SetTestAttackSet(AttackSet);
	Enemy->SetTestAIProfile(AIProfile);

	AEnemyAIController* AIController = World->SpawnActor<AEnemyAIController>(AEnemyAIController::StaticClass());
	if (!TestNotNull(TEXT("EnemyAIController spawned successfully"), AIController))
	{
		return false;
	}

	AIController->Possess(Enemy);
	TestTrue(TEXT("Controller has valid AttackSet after possess"), AIController->HasValidAttackSet());
	TestTrue(TEXT("Controller has valid AIProfile after possess"), AIController->HasValidAIProfile());

	UCharacterMovementComponent* EnemyMovement = Enemy->GetCharacterMovement();
	if (!TestNotNull(TEXT("Enemy character movement component valid"), EnemyMovement))
	{
		return false;
	}

	// -------------------------------------------------------------------------
	// SECTION 1: Initial Perception & Target Acquisition
	// -------------------------------------------------------------------------
	{
		AIController->TriggerTestProcessTargetPerception(Player, true);
		TestEqual(TEXT("Positive perception acquires Player as CurrentTarget"), AIController->GetCurrentTarget(), Player);
		TestFalse(TEXT("Retention flag is false upon initial positive perception"),
			AIController->IsTargetRetainedWithoutSightForTest());
	}

	// -------------------------------------------------------------------------
	// SECTION 2: Bounded Retention within LoseSightRadius & Leash
	// -------------------------------------------------------------------------
	{
		// Player is at (300, 0, 0), within LoseSightRadius (1800) and Enemy at (0, 0, 0) is at Home (Leash = 2500)
		AIController->TriggerTestProcessTargetPerception(Player, false);

		TestEqual(TEXT("Negative perception retains Player target within LoseSightRadius & Leash"),
			AIController->GetCurrentTarget(), Player);
		TestTrue(TEXT("Target is marked as retained without sight"),
			AIController->IsTargetRetainedWithoutSightForTest());

		// Revalidation while still in range preserves target
		AIController->TriggerTestRevalidateRetainedTarget();
		TestEqual(TEXT("Target survives revalidation while within LoseSightRadius & Leash"),
			AIController->GetCurrentTarget(), Player);
		TestTrue(TEXT("Target remains marked as retained without sight after revalidation"),
			AIController->IsTargetRetainedWithoutSightForTest());
	}

	// -------------------------------------------------------------------------
	// SECTION 3: Visual Re-acquisition Clears Retention Flag
	// -------------------------------------------------------------------------
	{
		AIController->TriggerTestProcessTargetPerception(Player, true);
		TestEqual(TEXT("CurrentTarget remains Player after visual re-acquisition"),
			AIController->GetCurrentTarget(), Player);
		TestFalse(TEXT("Retention flag is cleared upon positive perception"),
			AIController->IsTargetRetainedWithoutSightForTest());
	}

	// -------------------------------------------------------------------------
	// SECTION 4: Escape Distance & Home Leash Break Clears Target
	// -------------------------------------------------------------------------
	{
		// 4.1 Exceeding LoseSightRadius (1800)
		AIController->TriggerTestProcessTargetPerception(Player, false);
		TestTrue(TEXT("Target in retention state before moving out of range"),
			AIController->IsTargetRetainedWithoutSightForTest());

		Player->SetActorLocation(FVector(2000.0f, 0.0f, 0.0f)); // Distance = 2000 > 1800
		AIController->TriggerTestRevalidateRetainedTarget();

		TestNull(TEXT("Target cleared when Player exceeds LoseSightRadius"), AIController->GetCurrentTarget());
		TestFalse(TEXT("Retention flag reset after target clear from distance"),
			AIController->IsTargetRetainedWithoutSightForTest());

		// 4.2 Exceeding LeashRadius (2500)
		Player->SetActorLocation(FVector(300.0f, 0.0f, 0.0f));
		AIController->TriggerTestProcessTargetPerception(Player, true);
		TestEqual(TEXT("Player re-acquired after returning to range"), AIController->GetCurrentTarget(), Player);

		AIController->TriggerTestProcessTargetPerception(Player, false);
		TestTrue(TEXT("Target in retention state before leash break"),
			AIController->IsTargetRetainedWithoutSightForTest());

		Enemy->SetActorLocation(FVector(2600.0f, 0.0f, 0.0f)); // Distance from Home (0,0,0) = 2600 > 2500
		AIController->TriggerTestRevalidateRetainedTarget();

		TestNull(TEXT("Target cleared when Enemy exceeds LeashRadius"), AIController->GetCurrentTarget());
		TestFalse(TEXT("Retention flag reset after leash break"),
			AIController->IsTargetRetainedWithoutSightForTest());

		Enemy->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
	}

	// -------------------------------------------------------------------------
	// SECTION 5: Negative Perception for Non-Target Actor is No-Op
	// -------------------------------------------------------------------------
	{
		AIController->TriggerTestProcessTargetPerception(Player, true);
		TestEqual(TEXT("Player acquired as target"), AIController->GetCurrentTarget(), Player);

		APlayerCharacter* OtherPlayer = FCombatAutomationFixture::SpawnPlayer(World, FTransform(FRotator::ZeroRotator, FVector(400.0f, 0.0f, 0.0f)));
		if (TestNotNull(TEXT("OtherPlayer spawned successfully"), OtherPlayer))
		{
			AIController->TriggerTestProcessTargetPerception(OtherPlayer, false);
			TestEqual(TEXT("CurrentTarget unaffected by negative perception on non-target"),
				AIController->GetCurrentTarget(), Player);
			TestFalse(TEXT("Retention flag unaffected by negative perception on non-target"),
				AIController->IsTargetRetainedWithoutSightForTest());
		}
	}

	// -------------------------------------------------------------------------
	// SECTION 6: Explicit Clear & UnPossess Reset Retention State
	// -------------------------------------------------------------------------
	{
		AIController->TriggerTestProcessTargetPerception(Player, false);
		TestTrue(TEXT("Retention flag active before explicit clear"),
			AIController->IsTargetRetainedWithoutSightForTest());

		AIController->ClearTestTargetForAutomation(false);
		TestNull(TEXT("Target cleared by ClearCurrentTarget"), AIController->GetCurrentTarget());
		TestFalse(TEXT("Retention flag reset by ClearCurrentTarget"),
			AIController->IsTargetRetainedWithoutSightForTest());

		// Re-acquire then test UnPossess
		AIController->TriggerTestProcessTargetPerception(Player, true);
		AIController->TriggerTestProcessTargetPerception(Player, false);
		TestTrue(TEXT("Retention flag active before UnPossess"),
			AIController->IsTargetRetainedWithoutSightForTest());

		AIController->UnPossess();
		TestNull(TEXT("Target cleared after UnPossess"), AIController->GetCurrentTarget());
		TestFalse(TEXT("Retention flag reset after UnPossess"),
			AIController->IsTargetRetainedWithoutSightForTest());

		// Late positive perception while unpossessed must fail-closed
		AIController->TriggerTestProcessTargetPerception(Player, true);
		TestNull(TEXT("CurrentTarget remains null on late perception after UnPossess"),
			AIController->GetCurrentTarget());
		TestFalse(TEXT("Retention flag remains false after unpossessed perception"),
			AIController->IsTargetRetainedWithoutSightForTest());

		// Re-possess the same Enemy: no stale target restored
		AIController->Possess(Enemy);
		TestNull(TEXT("No stale target restored upon re-possessing Enemy"),
			AIController->GetCurrentTarget());

		// Valid positive perception after re-possess acquires Player normally
		AIController->TriggerTestProcessTargetPerception(Player, true);
		TestEqual(TEXT("CurrentTarget acquired as Player after valid re-possess"),
			AIController->GetCurrentTarget(), Player);
	}

	// -------------------------------------------------------------------------
	// SECTION 7: Active Root Motion Interaction with Retention
	// -------------------------------------------------------------------------
	{
		if (AIController->GetPawn() != Enemy)
		{
			AIController->Possess(Enemy);
		}
		Player->SetActorLocation(FVector(300.0f, 0.0f, 0.0f));
		Enemy->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
		AIController->TriggerTestProcessTargetPerception(Player, true);

		TSharedPtr<FRootMotionSource_ConstantForce> RootMotionSource = MakeShared<FRootMotionSource_ConstantForce>();
		RootMotionSource->InstanceName = TEXT("RetentionRootMotionTest");
		RootMotionSource->Priority = 500;
		RootMotionSource->Duration = 5.0f;
		RootMotionSource->AccumulateMode = ERootMotionAccumulateMode::Override;
		RootMotionSource->Force = FVector(100.0f, 0.0f, 0.0f);
		const uint16 RootMotionSourceId = EnemyMovement->ApplyRootMotionSource(RootMotionSource);

		Enemy->SetActorRotation(FRotator(0.0f, 90.0f, 0.0f));

		// Negative perception while Root Motion is active
		AIController->TriggerTestProcessTargetPerception(Player, false);

		TestEqual(TEXT("Target is retained during Root Motion"), AIController->GetCurrentTarget(), Player);
		TestTrue(TEXT("Retention flag is active during Root Motion"),
			AIController->IsTargetRetainedWithoutSightForTest());

		// Update control rotation while RM is active
		AIController->TriggerTestUpdateControlRotation(0.1f, true);

		TestTrue(TEXT("Enemy yaw untouched during active Root Motion (remains 90 deg)"),
			FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(Enemy->GetActorRotation().Yaw, 90.0f), 0.01f));
		TestNull(TEXT("Gameplay focus suppressed during active Root Motion"), AIController->GetFocusActor());

		// Root Motion ends
		EnemyMovement->RemoveRootMotionSourceByID(RootMotionSourceId);
		EnemyMovement->CurrentRootMotion.Clear();

		// Update control rotation after RM ends
		AIController->TriggerTestUpdateControlRotation(0.1f, true);

		TestEqual(TEXT("Focus restored to retained Player after Root Motion ends"),
			AIController->GetFocusActor(), Cast<AActor>(Player));
		TestTrue(TEXT("Facing recovery is active towards retained Player"),
			AIController->IsFacingRecoveryActiveForTest());
	}

	return true;
}

#endif
