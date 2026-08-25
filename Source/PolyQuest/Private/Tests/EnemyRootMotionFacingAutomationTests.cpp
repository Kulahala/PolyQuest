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
	FEnemyRootMotionFacingAutomationTest,
	"PolyQuest.Enemy.RootMotionFacing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyRootMotionFacingAutomationTest::RunTest(const FString& Parameters)
{
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("EnemyRootMotionFacingTestWorld"));
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
	UEnemyAttackProfile* AttackProfile = NewObject<UEnemyAttackProfile>(GetTransientPackage(), TEXT("Test_AttackProfile"));
	UAnimMontage* DummyMontage = NewObject<UAnimMontage>(GetTransientPackage(), TEXT("Test_DummyMontage"));
	AttackProfile->SetTestMontage(DummyMontage);
	AttackProfile->SetTestDamageEffectClass(UGameplayEffect::StaticClass());
	AttackProfile->SetTestAttackRange(180.0f);
	AttackProfile->SetTestCooldown(1.0f);
	AttackProfile->SetTestGuardStaminaDamage(25.0f);

	UEnemyAttackSet* AttackSet = NewObject<UEnemyAttackSet>(GetTransientPackage(), TEXT("Test_AttackSet"));
	AttackSet->SetTestEngagementRange(300.0f);
	AttackSet->AddTestEntry(AttackProfile, 1.0f);

	UEnemyAIProfile* AIProfile = NewObject<UEnemyAIProfile>(GetTransientPackage(), TEXT("Test_AIProfile"));
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
	// SECTION 1: Baseline Facing & Normal Controller Updates
	// -------------------------------------------------------------------------
	{
		Enemy->SetActorRotation(FRotator(0.0f, 90.0f, 0.0f));
		AIController->SetTestTargetForAutomation(Player);
		TestEqual(TEXT("Current combat target is Player"), AIController->GetCurrentTarget(), Player);

		AIController->TriggerTestUpdateControlRotation(0.1f, true);
		TestTrue(TEXT("Enemy normally faces Player target (Yaw ~0 deg)"),
			FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(Enemy->GetActorRotation().Yaw, 0.0f), 1.0f));
	}

	// -------------------------------------------------------------------------
	// SECTION 2: Active Root Motion Facing Ownership & Focus Suppression
	// -------------------------------------------------------------------------
	{
		TSharedPtr<FRootMotionSource_ConstantForce> RootMotionSource = MakeShared<FRootMotionSource_ConstantForce>();
		RootMotionSource->InstanceName = TEXT("EnemyRootMotionFacingTest");
		RootMotionSource->Priority = 500;
		RootMotionSource->Duration = 5.0f;
		RootMotionSource->AccumulateMode = ERootMotionAccumulateMode::Override;
		RootMotionSource->Force = FVector(100.0f, 0.0f, 0.0f);
		const uint16 RootMotionSourceId = EnemyMovement->ApplyRootMotionSource(RootMotionSource);
		Enemy->Tick(0.01f);

		TestTrue(TEXT("Root Motion source is active on Enemy"), Enemy->HasAnyRootMotion());

		// Externally modify Enemy yaw during active Root Motion
		Enemy->SetActorRotation(FRotator(0.0f, 90.0f, 0.0f));

		AIController->TriggerTestUpdateControlRotation(0.1f, true);

		TestEqual(TEXT("Target is retained during Root Motion"), AIController->GetCurrentTarget(), Player);
		TestTrue(TEXT("Enemy yaw is untouched during active Root Motion (remains 90 deg)"),
			FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(Enemy->GetActorRotation().Yaw, 90.0f), 0.01f));
		TestNull(TEXT("Gameplay focus is suppressed/cleared during Root Motion"), AIController->GetFocusActor());

		// ---------------------------------------------------------------------
		// SECTION 3: Root Motion Removal & 800 deg/s Smooth Recovery (No Snap)
		// ---------------------------------------------------------------------
		EnemyMovement->RemoveRootMotionSourceByID(RootMotionSourceId);
		EnemyMovement->CurrentRootMotion.Clear();
		TestFalse(TEXT("Root Motion is no longer active after removal"), Enemy->HasAnyRootMotion());

		// First update after RM ends (DT = 0.1s): turns at most 800 deg/s * 0.1s = 80 deg.
		// Current yaw is 90 deg, desired yaw is 0 deg. Expected yaw is ~10 deg.
		AIController->TriggerTestUpdateControlRotation(0.1f, true);

		TestEqual(TEXT("Gameplay focus is restored to Player exactly once"), AIController->GetFocusActor(), Cast<AActor>(Player));
		TestTrue(TEXT("Facing recovery state is active"), AIController->IsFacingRecoveryActiveForTest());
		TestFalse(TEXT("Enemy did NOT snap to target yaw on the first frame"),
			FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(Enemy->GetActorRotation().Yaw, 0.0f), 0.1f));
		TestNearlyEqual(TEXT("Enemy turned bounded 80 deg in 0.1s (Yaw ~10 deg)"),
			Enemy->GetActorRotation().Yaw, 10.0, 1.5);

		// Second update (DT = 0.05s): can turn up to 40 deg, remaining difference is 10 deg -> should converge.
		AIController->TriggerTestUpdateControlRotation(0.05f, true);

		TestTrue(TEXT("Facing recovery converged to target yaw within 0.01 deg"),
			FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(Enemy->GetActorRotation().Yaw, 0.0f), 0.05f));
		TestFalse(TEXT("Facing recovery ended after convergence"), AIController->IsFacingRecoveryActiveForTest());
	}

	// -------------------------------------------------------------------------
	// SECTION 4: Interruption by New Root Motion, Target Loss & UnPossess
	// -------------------------------------------------------------------------
	{
		// 4.1 Interruption by new Root Motion
		TSharedPtr<FRootMotionSource_ConstantForce> RMS1 = MakeShared<FRootMotionSource_ConstantForce>();
		RMS1->InstanceName = TEXT("RMS_Interruption_1");
		RMS1->Priority = 500;
		RMS1->Duration = 5.0f;
		RMS1->AccumulateMode = ERootMotionAccumulateMode::Override;
		RMS1->Force = FVector(100.0f, 0.0f, 0.0f);
		const uint16 RMS1_Id = EnemyMovement->ApplyRootMotionSource(RMS1);

		Enemy->SetActorRotation(FRotator(0.0f, 90.0f, 0.0f));
		AIController->TriggerTestUpdateControlRotation(0.1f, true); // Active RM
		EnemyMovement->RemoveRootMotionSourceByID(RMS1_Id); // RM ends
		EnemyMovement->CurrentRootMotion.Clear();

		AIController->TriggerTestUpdateControlRotation(0.01f, true); // Enters recovery
		TestTrue(TEXT("Recovery started after RM1 ended"), AIController->IsFacingRecoveryActiveForTest());

		// New Root Motion begins during recovery
		TSharedPtr<FRootMotionSource_ConstantForce> RMS2 = MakeShared<FRootMotionSource_ConstantForce>();
		RMS2->InstanceName = TEXT("RMS_Interruption_2");
		RMS2->Priority = 500;
		RMS2->Duration = 5.0f;
		RMS2->AccumulateMode = ERootMotionAccumulateMode::Override;
		RMS2->Force = FVector(100.0f, 0.0f, 0.0f);
		const uint16 RMS2_Id = EnemyMovement->ApplyRootMotionSource(RMS2);

		AIController->TriggerTestUpdateControlRotation(0.1f, true);
		TestFalse(TEXT("New Root Motion interval immediately cancels pending recovery"),
			AIController->IsFacingRecoveryActiveForTest());

		EnemyMovement->RemoveRootMotionSourceByID(RMS2_Id);
		EnemyMovement->CurrentRootMotion.Clear();
		AIController->TriggerTestUpdateControlRotation(0.5f, true); // Settle

		// 4.2 Target Loss during Root Motion
		TSharedPtr<FRootMotionSource_ConstantForce> RMS3 = MakeShared<FRootMotionSource_ConstantForce>();
		RMS3->InstanceName = TEXT("RMS_TargetLoss");
		RMS3->Priority = 500;
		RMS3->Duration = 5.0f;
		RMS3->AccumulateMode = ERootMotionAccumulateMode::Override;
		RMS3->Force = FVector(100.0f, 0.0f, 0.0f);
		const uint16 RMS3_Id = EnemyMovement->ApplyRootMotionSource(RMS3);

		AIController->TriggerTestUpdateControlRotation(0.1f, true); // Active RM
		Enemy->SetActorRotation(FRotator(0.0f, 45.0f, 0.0f));

		AIController->ClearTestTargetForAutomation(false);
		TestNull(TEXT("Target cleared during Root Motion"), AIController->GetCurrentTarget());

		EnemyMovement->RemoveRootMotionSourceByID(RMS3_Id); // RM ends without target
		EnemyMovement->CurrentRootMotion.Clear();

		AIController->TriggerTestUpdateControlRotation(0.1f, true);
		TestFalse(TEXT("No recovery started when no valid target exists at RM handoff"),
			AIController->IsFacingRecoveryActiveForTest());
		TestTrue(TEXT("Action-end yaw preserved when target was lost (remains 45 deg)"),
			FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(Enemy->GetActorRotation().Yaw, 45.0f), 0.01f));

		// 4.3 UnPossess resets handoff state
		AIController->SetTestTargetForAutomation(Player);
		TSharedPtr<FRootMotionSource_ConstantForce> RMS4 = MakeShared<FRootMotionSource_ConstantForce>();
		RMS4->InstanceName = TEXT("RMS_UnPossess");
		RMS4->Priority = 500;
		RMS4->Duration = 5.0f;
		RMS4->AccumulateMode = ERootMotionAccumulateMode::Override;
		RMS4->Force = FVector(100.0f, 0.0f, 0.0f);
		const uint16 RMS4_Id = EnemyMovement->ApplyRootMotionSource(RMS4);

		AIController->TriggerTestUpdateControlRotation(0.1f, true);
		EnemyMovement->RemoveRootMotionSourceByID(RMS4_Id);
		EnemyMovement->CurrentRootMotion.Clear();
		AIController->TriggerTestUpdateControlRotation(0.01f, true); // In recovery
		TestTrue(TEXT("In recovery before UnPossess"), AIController->IsFacingRecoveryActiveForTest());

		AIController->UnPossess();
		TestFalse(TEXT("UnPossess resets recovery state"), AIController->IsFacingRecoveryActiveForTest());
		TestNull(TEXT("UnPossess clears target"), AIController->GetCurrentTarget());
	}

	// -------------------------------------------------------------------------
	// SECTION 5: Tactical Reposition Pace Lifecycle & Re-entrancy
	// -------------------------------------------------------------------------
	{
		AIController->Possess(Enemy);
		EnemyMovement->MaxWalkSpeed = 550.0f;

		// 5.1 First pace override transition
		TestTrue(TEXT("BeginRepositionPaceOverride succeeds with valid movement component"),
			AIController->TriggerTestBeginCooldownRepositionPace());
		TestEqual(TEXT("MaxWalkSpeed is overridden to 300.0f"), EnemyMovement->MaxWalkSpeed, 300.0f);
		TestTrue(TEXT("Pace override flag is active"), AIController->IsRepositionPaceOverriddenForTest());
		TestEqual(TEXT("Original MaxWalkSpeed 550.0f captured"),
			AIController->GetCapturedRepositionMaxWalkSpeedForTest(), 550.0f);

		// 5.2 Re-entrant call must NOT overwrite original captured speed with 300.0f
		TestTrue(TEXT("Re-entrant BeginRepositionPaceOverride succeeds safely"),
			AIController->TriggerTestBeginCooldownRepositionPace());
		TestEqual(TEXT("MaxWalkSpeed remains 300.0f on re-entrancy"), EnemyMovement->MaxWalkSpeed, 300.0f);
		TestEqual(TEXT("Captured original speed is still 550.0f (not overwritten by 300.0f)"),
			AIController->GetCapturedRepositionMaxWalkSpeedForTest(), 550.0f);

		// 5.3 StopCooldownReposition restores original speed
		AIController->StopCooldownReposition(true);
		TestEqual(TEXT("StopCooldownReposition restores original MaxWalkSpeed 550.0f"),
			EnemyMovement->MaxWalkSpeed, 550.0f);
		TestFalse(TEXT("Pace override flag reset after StopCooldownReposition"),
			AIController->IsRepositionPaceOverriddenForTest());
		TestEqual(TEXT("Saved speed cleared to 0.0f after restore"),
			AIController->GetCapturedRepositionMaxWalkSpeedForTest(), 0.0f);

		// 5.4 Idempotent restore safety
		AIController->StopCooldownReposition(true);
		TestEqual(TEXT("Idempotent second StopCooldownReposition preserves 550.0f"),
			EnemyMovement->MaxWalkSpeed, 550.0f);
		TestFalse(TEXT("Pace override flag remains false on idempotent restore"),
			AIController->IsRepositionPaceOverriddenForTest());
	}

	return true;
}

#endif
