#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "AbilitySystem/CharacterAttributeSet.h"
#include "Character/Player/PlayerCharacter.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Combat/Equipment/BowWeaponDefinition.h"
#include "Combat/Equipment/ProjectileDefinition.h"
#include "Combat/Projectile/CombatProjectile.h"
#include "Combat/Projectile/CombatProjectileTargeting.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameplayTagContainer.h"
#include "Tests/CombatAutomationFixture.h"
#include "Tests/TestProjectileDamageGE.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectileTargetAssistAutomationTest, "PolyQuest.Projectile.TargetAssist", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectileTargetAssistAutomationTest::RunTest(const FString& Parameters)
{
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("TargetAssistTestWorld"));
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

	const FGameplayTag TagDead = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
	const FGameplayTag TagInvulnerable = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Invulnerable")), false);
	const FGameplayTag TagTeamPlayer = FGameplayTag::RequestGameplayTag(FName(TEXT("Team.Player")), false);
	const FGameplayTag TagTeamEnemy = FGameplayTag::RequestGameplayTag(FName(TEXT("Team.Enemy")), false);

	auto SpawnPlayerFixture = [World]()
	{
		return FCombatAutomationFixture::SpawnPlayer(World);
	};
	auto SpawnEnemyFixture = [World]()
	{
		return FCombatAutomationFixture::SpawnPassiveEnemy(World);
	};

	// -------------------------------------------------------------------------
	// SECTION 1: UProjectileDefinition TargetAssist & LimitedHoming Validation
	// -------------------------------------------------------------------------
	{
		UProjectileDefinition* Def = NewObject<UProjectileDefinition>(GetTransientPackage(), TEXT("Test_TargetAssistProjDef"));
		Def->InitialSpeed = 3000.0f;
		Def->MaxSpeed = 3000.0f;
		Def->LifespanSeconds = 5.0f;
		Def->CollisionRadius = 12.0f;
		Def->DamageGameplayEffectClass = UTestProjectileDamageGE::StaticClass();

		// Default (off) passes validation
		FString Reason;
		TestTrue(TEXT("Default ProjectileDefinition without target assist passes"), Def->IsValidProjectileDefinition(Reason));

		// 1.1 Homing enabled without TargetAssist enabled -> rejected
		Def->bEnableLimitedHoming = true;
		Def->bEnableTargetAssist = false;
		TestFalse(TEXT("Homing without TargetAssist is rejected"), Def->IsValidProjectileDefinition(Reason));

		// Enable both for valid baseline
		Def->bEnableTargetAssist = true;
		Def->TargetAssistMaxDistance = 1500.0f;
		Def->TargetAssistMaxAngleDegrees = 50.0f;
		Def->TargetAssistMaxHeightDelta = 250.0f;
		Def->TargetAssistMaxPitchDegrees = 45.0f;
		Def->HomingStartDelaySeconds = 0.06f;
		Def->HomingDurationSeconds = 0.75f;
		Def->HomingTurnRateDegreesPerSecond = 120.0f;
		Def->HomingMaxTotalTurnDegrees = 60.0f;
		TestTrue(TEXT("Valid TargetAssist and Homing definition passes"), Def->IsValidProjectileDefinition(Reason));

		// 1.2 Invalid TargetAssistMaxDistance
		Def->TargetAssistMaxDistance = 0.0f;
		TestFalse(TEXT("TargetAssistMaxDistance == 0 is rejected"), Def->IsValidProjectileDefinition(Reason));
		Def->TargetAssistMaxDistance = -100.0f;
		TestFalse(TEXT("Negative TargetAssistMaxDistance is rejected"), Def->IsValidProjectileDefinition(Reason));
		Def->TargetAssistMaxDistance = NAN;
		TestFalse(TEXT("NaN TargetAssistMaxDistance is rejected"), Def->IsValidProjectileDefinition(Reason));
		Def->TargetAssistMaxDistance = 1500.0f;

		// 1.3 Invalid TargetAssistMaxAngleDegrees
		Def->TargetAssistMaxAngleDegrees = 0.0f;
		TestFalse(TEXT("TargetAssistMaxAngleDegrees == 0 is rejected"), Def->IsValidProjectileDefinition(Reason));
		Def->TargetAssistMaxAngleDegrees = 181.0f;
		TestFalse(TEXT("TargetAssistMaxAngleDegrees > 180 is rejected"), Def->IsValidProjectileDefinition(Reason));
		Def->TargetAssistMaxAngleDegrees = 180.0f;
		TestTrue(TEXT("TargetAssistMaxAngleDegrees == 180.0 is valid"), Def->IsValidProjectileDefinition(Reason));
		Def->TargetAssistMaxAngleDegrees = 50.0f;

		// 1.4 Invalid TargetAssistMaxHeightDelta
		Def->TargetAssistMaxHeightDelta = 0.0f;
		TestFalse(TEXT("TargetAssistMaxHeightDelta == 0 is rejected"), Def->IsValidProjectileDefinition(Reason));
		Def->TargetAssistMaxHeightDelta = 250.0f;

		// 1.5 Invalid TargetAssistMaxPitchDegrees
		Def->TargetAssistMaxPitchDegrees = 0.0f;
		TestFalse(TEXT("TargetAssistMaxPitchDegrees == 0 is rejected"), Def->IsValidProjectileDefinition(Reason));
		Def->TargetAssistMaxPitchDegrees = 90.0f;
		TestFalse(TEXT("TargetAssistMaxPitchDegrees >= 90 is rejected"), Def->IsValidProjectileDefinition(Reason));
		Def->TargetAssistMaxPitchDegrees = 45.0f;

		// 1.6 Invalid HomingStartDelaySeconds
		Def->HomingStartDelaySeconds = -0.1f;
		TestFalse(TEXT("Negative HomingStartDelaySeconds is rejected"), Def->IsValidProjectileDefinition(Reason));
		Def->HomingStartDelaySeconds = NAN;
		TestFalse(TEXT("NaN HomingStartDelaySeconds is rejected"), Def->IsValidProjectileDefinition(Reason));
		Def->HomingStartDelaySeconds = 5.0f; // >= LifespanSeconds (5.0f)
		TestFalse(TEXT("HomingStartDelaySeconds >= LifespanSeconds is rejected"), Def->IsValidProjectileDefinition(Reason));
		Def->HomingStartDelaySeconds = 0.06f;

		// 1.7 Invalid HomingDurationSeconds
		Def->HomingDurationSeconds = 0.0f;
		TestFalse(TEXT("HomingDurationSeconds == 0 is rejected"), Def->IsValidProjectileDefinition(Reason));
		Def->HomingDurationSeconds = 6.0f; // > LifespanSeconds (5.0f)
		TestFalse(TEXT("HomingDurationSeconds > LifespanSeconds is rejected"), Def->IsValidProjectileDefinition(Reason));
		Def->HomingDurationSeconds = 5.0f; // == LifespanSeconds (5.0f)
		TestTrue(TEXT("HomingDurationSeconds == LifespanSeconds is valid"), Def->IsValidProjectileDefinition(Reason));
		Def->HomingDurationSeconds = 5.0f;

		// 1.8 Invalid HomingTurnRateDegreesPerSecond
		Def->HomingTurnRateDegreesPerSecond = 0.0f;
		TestFalse(TEXT("HomingTurnRateDegreesPerSecond == 0 is rejected"), Def->IsValidProjectileDefinition(Reason));
		Def->HomingTurnRateDegreesPerSecond = 120.0f;

		// 1.9 Invalid HomingMaxTotalTurnDegrees
		Def->HomingMaxTotalTurnDegrees = 0.0f;
		TestFalse(TEXT("HomingMaxTotalTurnDegrees == 0 is rejected"), Def->IsValidProjectileDefinition(Reason));
		Def->HomingMaxTotalTurnDegrees = -10.0f;
		TestFalse(TEXT("Negative HomingMaxTotalTurnDegrees is rejected"), Def->IsValidProjectileDefinition(Reason));
		Def->HomingMaxTotalTurnDegrees = NAN;
		TestFalse(TEXT("NaN HomingMaxTotalTurnDegrees is rejected"), Def->IsValidProjectileDefinition(Reason));
		Def->HomingMaxTotalTurnDegrees = 180.0f;
		TestTrue(TEXT("HomingMaxTotalTurnDegrees == 180.0 is valid"), Def->IsValidProjectileDefinition(Reason));
		Def->HomingMaxTotalTurnDegrees = 360.0f;
		TestTrue(TEXT("HomingMaxTotalTurnDegrees == 360.0 is valid"), Def->IsValidProjectileDefinition(Reason));
		Def->HomingMaxTotalTurnDegrees = 60.0f;

		TestTrue(TEXT("Restored definition passes validation"), Def->IsValidProjectileDefinition(Reason));
	}

	// -------------------------------------------------------------------------
	// SECTION 2: Aim Point & Candidate Qualification Gates
	// -------------------------------------------------------------------------
	{
		APlayerCharacter* Player = SpawnPlayerFixture();
		AEnemyCharacter* Enemy = SpawnEnemyFixture();
		TestNotNull(TEXT("Player spawned"), Player);
		TestNotNull(TEXT("Enemy spawned"), Enemy);

		if (Player && Enemy)
		{
			Player->SetActorLocation(FVector(0.0f, 0.0f, 100.0f));
			Enemy->SetActorLocation(FVector(500.0f, 0.0f, 100.0f));
			Player->SetTestCombatTeamTag(TagTeamPlayer);
			Enemy->SetTestCombatTeamTag(TagTeamEnemy);
			UAbilitySystemComponent* PlayerASC = Player->GetAbilitySystemComponent();
			UAbilitySystemComponent* EnemyASC = Enemy->GetAbilitySystemComponent();

			// 2.1 Aim point is upper torso (ActorLocation + Up * 0.5 * CapsuleHalfHeight)
			const float HalfHeight = Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			const FVector ExpectedAimPoint = Enemy->GetActorLocation() + FVector::UpVector * (0.5f * HalfHeight);
			const FVector ActualAimPoint = FCombatProjectileTargeting::GetTargetAimPoint(Enemy);
			TestEqual(TEXT("Aim point matches upper torso calculation"), ActualAimPoint, ExpectedAimPoint);

			// 2.2 Hostile alive enemy with ASC is valid candidate
			TestTrue(TEXT("Hostile alive enemy is valid candidate"), FCombatProjectileTargeting::IsValidTargetCandidate(Player, PlayerASC, Enemy));

			// 2.3 Self is rejected
			TestFalse(TEXT("Self is rejected as candidate"), FCombatProjectileTargeting::IsValidTargetCandidate(Player, PlayerASC, Player));

			// 2.4 Friendly is rejected
			APlayerCharacter* Friendly = SpawnPlayerFixture();
			if (Friendly)
			{
				Friendly->SetTestCombatTeamTag(TagTeamPlayer);
				TestFalse(TEXT("Friendly player is rejected as candidate"), FCombatProjectileTargeting::IsValidTargetCandidate(Player, PlayerASC, Friendly));
				Friendly->Destroy();
			}

			// 2.5 Dead target is rejected
			EnemyASC->AddLooseGameplayTag(TagDead);
			TestFalse(TEXT("Dead enemy is rejected as candidate"), FCombatProjectileTargeting::IsValidTargetCandidate(Player, PlayerASC, Enemy));
			EnemyASC->RemoveLooseGameplayTag(TagDead);

			// 2.6 Invulnerable target is rejected
			EnemyASC->AddLooseGameplayTag(TagInvulnerable);
			TestFalse(TEXT("Invulnerable enemy is rejected as candidate"), FCombatProjectileTargeting::IsValidTargetCandidate(Player, PlayerASC, Enemy));
			EnemyASC->RemoveLooseGameplayTag(TagInvulnerable);

			// 2.7 Dead source is rejected
			PlayerASC->AddLooseGameplayTag(TagDead);
			TestFalse(TEXT("Dead source rejected from targeting"), FCombatProjectileTargeting::IsValidTargetCandidate(Player, PlayerASC, Enemy));
			PlayerASC->RemoveLooseGameplayTag(TagDead);

			Player->Destroy();
			Enemy->Destroy();
		}
	}

	// -------------------------------------------------------------------------
	// SECTION 3: TryFindBestTargetCandidate & Sorting Gates
	// -------------------------------------------------------------------------
	{
		APlayerCharacter* Player = SpawnPlayerFixture();
		Player->SetActorLocation(FVector(0.0f, 0.0f, 100.0f));
		Player->SetTestCombatTeamTag(TagTeamPlayer);

		UAbilitySystemComponent* PlayerASC = Player->GetAbilitySystemComponent();

		FCombatProjectileTargetFilter Filter;
		Filter.MaxHorizontalDistance = 1500.0f;
		Filter.MaxAngleDegrees = 50.0f;
		Filter.MaxHeightDelta = 250.0f;
		Filter.MaxPitchDegrees = 45.0f;

		Filter.bBypassScreenFilterForTesting = true;

		const FVector LaunchLocation = FVector(0.0f, 0.0f, 100.0f);
		const FVector AimDirection = FVector(1.0f, 0.0f, 0.0f); // Looking +X

		// 3.1 Enemy beyond MaxHorizontalDistance (2000cm) -> rejected
		AEnemyCharacter* FarEnemy = SpawnEnemyFixture();
		FarEnemy->SetActorLocation(FVector(2000.0f, 0.0f, 100.0f));
		FarEnemy->SetTestCombatTeamTag(TagTeamEnemy);

		FCombatProjectileTargetCandidate CandidateResult;
		TestFalse(TEXT("Enemy beyond max distance is not selected"),
			FCombatProjectileTargeting::TryFindBestTargetCandidate(World, Player, PlayerASC, LaunchLocation, AimDirection, Filter, CandidateResult));

		FarEnemy->Destroy();

		// 3.2 Enemy beyond MaxAngleDegrees (e.g. 60 deg off forward) -> rejected
		// +X forward, 60 deg angle: X = 500 * cos(60) = 250, Y = 500 * sin(60) = 433
		AEnemyCharacter* WideAngleEnemy = SpawnEnemyFixture();
		WideAngleEnemy->SetActorLocation(FVector(250.0f, 433.0f, 100.0f));
		WideAngleEnemy->SetTestCombatTeamTag(TagTeamEnemy);

		TestFalse(TEXT("Enemy beyond max angle (60 deg) is not selected"),
			FCombatProjectileTargeting::TryFindBestTargetCandidate(World, Player, PlayerASC, LaunchLocation, AimDirection, Filter, CandidateResult));

		WideAngleEnemy->Destroy();

		// 3.3 Enemy beyond MaxHeightDelta (300cm above) -> rejected
		AEnemyCharacter* HighEnemy = SpawnEnemyFixture();
		HighEnemy->SetActorLocation(FVector(500.0f, 0.0f, 450.0f));
		HighEnemy->SetTestCombatTeamTag(TagTeamEnemy);

		TestFalse(TEXT("Enemy beyond max height delta is not selected"),
			FCombatProjectileTargeting::TryFindBestTargetCandidate(World, Player, PlayerASC, LaunchLocation, AimDirection, Filter, CandidateResult));

		HighEnemy->Destroy();

		// 3.4 Sorting: Tie-break prefers smaller angle
		AEnemyCharacter* EnemySmallAngle = SpawnEnemyFixture();
		EnemySmallAngle->Rename(TEXT("Enemy_SmallAngle"));
		EnemySmallAngle->SetActorLocation(FVector(600.0f, 50.0f, 100.0f)); // ~4.76 deg
		EnemySmallAngle->SetTestCombatTeamTag(TagTeamEnemy);

		AEnemyCharacter* EnemyLargeAngle = SpawnEnemyFixture();
		EnemyLargeAngle->Rename(TEXT("Enemy_LargeAngle"));
		EnemyLargeAngle->SetActorLocation(FVector(300.0f, 100.0f, 100.0f)); // ~18.43 deg, closer distance
		EnemyLargeAngle->SetTestCombatTeamTag(TagTeamEnemy);

		TestTrue(TEXT("Candidate found among two enemies"),
			FCombatProjectileTargeting::TryFindBestTargetCandidate(World, Player, PlayerASC, LaunchLocation, AimDirection, Filter, CandidateResult));
		TestEqual(TEXT("Sorting tie-break prefers smaller angle over closer distance"),
			CandidateResult.TargetActor.Get(), Cast<AActor>(EnemySmallAngle));

		EnemySmallAngle->Destroy();
		EnemyLargeAngle->Destroy();

		// 3.5 Sorting: Tie-break when angles equal prefers closer distance
		AEnemyCharacter* EnemyClose = SpawnEnemyFixture();
		EnemyClose->Rename(TEXT("Enemy_Close"));
		EnemyClose->SetActorLocation(FVector(400.0f, 0.0f, 100.0f)); // 0 deg, 400cm
		EnemyClose->SetTestCombatTeamTag(TagTeamEnemy);

		AEnemyCharacter* EnemyFar = SpawnEnemyFixture();
		EnemyFar->Rename(TEXT("Enemy_Far"));
		EnemyFar->SetActorLocation(FVector(800.0f, 0.0f, 100.0f)); // 0 deg, 800cm
		EnemyFar->SetTestCombatTeamTag(TagTeamEnemy);

		TestTrue(TEXT("Candidate found for distance tie-break"),
			FCombatProjectileTargeting::TryFindBestTargetCandidate(World, Player, PlayerASC, LaunchLocation, AimDirection, Filter, CandidateResult));
		TestEqual(TEXT("Sorting tie-break on same angle prefers closer distance"),
			CandidateResult.TargetActor.Get(), Cast<AActor>(EnemyClose));

		EnemyClose->Destroy();
		EnemyFar->Destroy();

		// 3.6 Screen Viewport Projection Filtering
		AEnemyCharacter* ScreenEnemy = SpawnEnemyFixture();
		ScreenEnemy->SetActorLocation(FVector(500.0f, 0.0f, 100.0f));
		ScreenEnemy->SetTestCombatTeamTag(TagTeamEnemy);

		// 3.6.1 In-screen projection -> selected
		FCombatProjectileTargetFilter InScreenFilter;
		InScreenFilter.TestScreenProjectionHook = [](const FVector& WorldPoint, FVector2D& OutScreenPos, FVector2D& OutViewportSize)
		{
			OutScreenPos = FVector2D(960.0f, 540.0f);
			OutViewportSize = FVector2D(1920.0f, 1080.0f);
			return true;
		};
		TestTrue(TEXT("In-screen candidate is selected"),
			FCombatProjectileTargeting::TryFindBestTargetCandidate(World, Player, PlayerASC, LaunchLocation, AimDirection, InScreenFilter, CandidateResult));

		// 3.6.2 Edge margin candidate (e.g. X = 1960px slightly past 1920px border, within 6% margin 2035px) -> selected
		FCombatProjectileTargetFilter EdgeMarginFilter;
		EdgeMarginFilter.ScreenMarginRatio = 0.06f;
		EdgeMarginFilter.TestScreenProjectionHook = [](const FVector& WorldPoint, FVector2D& OutScreenPos, FVector2D& OutViewportSize)
		{
			OutScreenPos = FVector2D(1960.0f, 540.0f); // ~2% past 1920 border, within 6% margin
			OutViewportSize = FVector2D(1920.0f, 1080.0f);
			return true;
		};
		TestTrue(TEXT("Near-edge candidate within ScreenMarginRatio is selected"),
			FCombatProjectileTargeting::TryFindBestTargetCandidate(World, Player, PlayerASC, LaunchLocation, AimDirection, EdgeMarginFilter, CandidateResult));

		// 3.6.3 Far off-screen right projection (X = 2500px > ViewportX + MarginX) -> rejected
		FCombatProjectileTargetFilter OffScreenRightFilter;
		OffScreenRightFilter.TestScreenProjectionHook = [](const FVector& WorldPoint, FVector2D& OutScreenPos, FVector2D& OutViewportSize)
		{
			OutScreenPos = FVector2D(2500.0f, 540.0f);
			OutViewportSize = FVector2D(1920.0f, 1080.0f);
			return true;
		};
		TestFalse(TEXT("Far off-screen candidate (X > ViewportX + MarginX) is rejected"),
			FCombatProjectileTargeting::TryFindBestTargetCandidate(World, Player, PlayerASC, LaunchLocation, AimDirection, OffScreenRightFilter, CandidateResult));

		// 3.6.4 Far off-screen left projection (X = -500px < -MarginX) -> rejected
		FCombatProjectileTargetFilter OffScreenLeftFilter;
		OffScreenLeftFilter.TestScreenProjectionHook = [](const FVector& WorldPoint, FVector2D& OutScreenPos, FVector2D& OutViewportSize)
		{
			OutScreenPos = FVector2D(-500.0f, 540.0f);
			OutViewportSize = FVector2D(1920.0f, 1080.0f);
			return true;
		};
		TestFalse(TEXT("Far off-screen candidate (X < -MarginX) is rejected"),
			FCombatProjectileTargeting::TryFindBestTargetCandidate(World, Player, PlayerASC, LaunchLocation, AimDirection, OffScreenLeftFilter, CandidateResult));

		// 3.6.5 Behind camera projection -> rejected
		FCombatProjectileTargetFilter BehindCameraFilter;
		BehindCameraFilter.TestScreenProjectionHook = [](const FVector& WorldPoint, FVector2D& OutScreenPos, FVector2D& OutViewportSize)
		{
			return false;
		};
		TestFalse(TEXT("Behind-camera candidate is rejected"),
			FCombatProjectileTargeting::TryFindBestTargetCandidate(World, Player, PlayerASC, LaunchLocation, AimDirection, BehindCameraFilter, CandidateResult));

		// 3.6.6 Fail-closed without PlayerController / Viewport (no crash)
		FCombatProjectileTargetFilter DefaultNoPCFilter;
		TestFalse(TEXT("Fail-closed when no local PlayerController/Viewport exists"),
			FCombatProjectileTargeting::TryFindBestTargetCandidate(World, Player, PlayerASC, LaunchLocation, AimDirection, DefaultNoPCFilter, CandidateResult));

		ScreenEnemy->Destroy();
		Player->Destroy();
	}

	// -------------------------------------------------------------------------
	// SECTION 4: APlayerCharacter Ray-Plane Intersection Mathematics
	// -------------------------------------------------------------------------
	{
		FVector Intersection = FVector::ZeroVector;

		// 4.1 Downward ray intersecting horizontal plane at Z = 100
		const FVector RayOrigin(0.0f, 0.0f, 1000.0f);
		const FVector RayDir = FVector(1.0f, 0.0f, -1.0f).GetSafeNormal();
		const float PlaneZ = 100.0f;
		const bool bHit = APlayerCharacter::CalculateRayPlaneIntersection(RayOrigin, RayDir, PlaneZ, Intersection);
		TestTrue(TEXT("Ray successfully intersects horizontal plane"), bHit);
		TestNearlyEqual(TEXT("Intersection Z equals plane Z"), Intersection.Z, 100.0, 0.01);
		TestTrue(TEXT("Intersection X is positive forward"), Intersection.X > 0.0f);

		// 4.2 Horizontal parallel ray (Z == 0) fails closed
		const FVector ParallelDir(1.0f, 0.0f, 0.0f);
		TestFalse(TEXT("Parallel ray (Z == 0) fails intersection"),
			APlayerCharacter::CalculateRayPlaneIntersection(RayOrigin, ParallelDir, PlaneZ, Intersection));

		// 4.3 Ray pointing away from plane (T < 0) fails closed
		const FVector UpwardDir = FVector(1.0f, 0.0f, 1.0f).GetSafeNormal();
		TestFalse(TEXT("Ray pointing away from plane (T < 0) fails intersection"),
			APlayerCharacter::CalculateRayPlaneIntersection(RayOrigin, UpwardDir, PlaneZ, Intersection));

		// 4.4 Non-finite inputs fail closed
		TestFalse(TEXT("NaN origin fails intersection"),
			APlayerCharacter::CalculateRayPlaneIntersection(FVector(NAN, 0.0f, 0.0f), RayDir, PlaneZ, Intersection));
		TestFalse(TEXT("NaN direction fails intersection"),
			APlayerCharacter::CalculateRayPlaneIntersection(RayOrigin, FVector(NAN, 0.0f, 0.0f), PlaneZ, Intersection));
		TestFalse(TEXT("Infinite plane Z fails intersection"),
			APlayerCharacter::CalculateRayPlaneIntersection(RayOrigin, RayDir, INFINITY, Intersection));
	}

	// -------------------------------------------------------------------------
	// SECTION 5: ACombatProjectile Limited Homing Dynamics & Straight-Flight Abort
	// -------------------------------------------------------------------------
	{
		APlayerCharacter* Player = SpawnPlayerFixture();
		AEnemyCharacter* Enemy = SpawnEnemyFixture();
		Player->SetActorLocation(FVector(0.0f, 0.0f, 100.0f));
		Enemy->SetActorLocation(FVector(800.0f, 200.0f, 100.0f));
		Player->SetTestCombatTeamTag(TagTeamPlayer);
		Enemy->SetTestCombatTeamTag(TagTeamEnemy);

		UProjectileDefinition* HomingDef = NewObject<UProjectileDefinition>(GetTransientPackage(), TEXT("Test_HomingDef"));
		HomingDef->InitialSpeed = 3000.0f;
		HomingDef->MaxSpeed = 3000.0f;
		HomingDef->LifespanSeconds = 5.0f;
		HomingDef->CollisionRadius = 12.0f;
		HomingDef->DamageGameplayEffectClass = UTestProjectileDamageGE::StaticClass();
		HomingDef->bEnableTargetAssist = true;
		HomingDef->TargetAssistMaxDistance = 1500.0f;
		HomingDef->TargetAssistMaxAngleDegrees = 50.0f;
		HomingDef->TargetAssistMaxHeightDelta = 250.0f;
		HomingDef->TargetAssistMaxPitchDegrees = 45.0f;
		HomingDef->bEnableLimitedHoming = true;
		HomingDef->HomingStartDelaySeconds = 0.06f;
		HomingDef->HomingDurationSeconds = 0.75f;
		HomingDef->HomingTurnRateDegreesPerSecond = 120.0f;
		HomingDef->HomingMaxTotalTurnDegrees = 60.0f;

		// 5.1 Launch request snapshot initialization with PointerDirection as initial flight direction
		ACombatProjectile* Projectile = World->SpawnActor<ACombatProjectile>();
		Projectile->SetActorLocation(FVector(0.0f, 0.0f, 100.0f));

		const FVector PointerDirection = FVector(1.0f, 0.0f, 0.0f); // Facing +X
		const FVector AimPoint = FCombatProjectileTargeting::GetTargetAimPoint(Enemy);

		FCombatProjectileLaunchRequest LaunchReq;
		LaunchReq.Definition = HomingDef;
		LaunchReq.SourceActor = Player;
		LaunchReq.SourceAbilitySystemComponent = Player->GetAbilitySystemComponent();
		LaunchReq.InitialFlightDirection = PointerDirection;
		LaunchReq.TargetActor = Enemy;
		LaunchReq.InitialTargetAimPoint = AimPoint;
		LaunchReq.bEnableLimitedHoming = true;
		LaunchReq.HomingStartDelaySeconds = HomingDef->HomingStartDelaySeconds;
		LaunchReq.HomingDurationSeconds = HomingDef->HomingDurationSeconds;
		LaunchReq.HomingTurnRateDegreesPerSecond = HomingDef->HomingTurnRateDegreesPerSecond;
		LaunchReq.HomingMaxTotalTurnDegrees = HomingDef->HomingMaxTotalTurnDegrees;
		LaunchReq.TargetAssistMaxDistance = HomingDef->TargetAssistMaxDistance;
		LaunchReq.TargetAssistMaxHeightDelta = HomingDef->TargetAssistMaxHeightDelta;

		TestTrue(TEXT("Initialize projectile with homing succeeds"), Projectile->InitializeProjectile(LaunchReq));
		TestTrue(TEXT("Homing is active after initialization"), Projectile->GetTestHomingActive());
		TestEqual(TEXT("InitialFlightDirection is PointerDirection (not overridden by aim point)"),
			Projectile->GetTestInitialLaunchDirection(), PointerDirection);
		TestEqual(TEXT("Target actor snapshot set"), Projectile->GetTestTargetActor(), Cast<AActor>(Enemy));
		TestNearlyEqual(TEXT("HomingStartDelaySeconds snapshot set"), Projectile->GetTestHomingStartDelaySeconds(), 0.06f, 0.001f);

		// 5.2 During initial delay (dt = 0.03s < 0.06s), Velocity and TotalTurnAngle remain unchanged
		Projectile->Tick(0.03f);
		TestTrue(TEXT("Homing remains active during delay period"), Projectile->GetTestHomingActive());
		TestNearlyEqual(TEXT("TotalTurnAngle remains 0.0 during delay period"), Projectile->GetTestTotalTurnAngleDegrees(), 0.0f, 0.001f);
		TestEqual(TEXT("Velocity direction unchanged during delay period"),
			Projectile->GetMovementComponent()->Velocity.GetSafeNormal(), PointerDirection);

		// 5.3 After delay elapsed (next dt = 0.05s, total = 0.08s > 0.06s), homing turns towards target
		Projectile->Tick(0.05f);
		TestTrue(TEXT("Homing remains active after delay"), Projectile->GetTestHomingActive());
		TestTrue(TEXT("Accumulated turn angle increased after delay elapsed"), Projectile->GetTestTotalTurnAngleDegrees() > 0.0f);
		const float ActiveTime = 0.08f - 0.06f; // 0.02s active
		TestTrue(TEXT("Turn angle does not exceed rate * effective dt"),
			Projectile->GetTestTotalTurnAngleDegrees() <= HomingDef->HomingTurnRateDegreesPerSecond * ActiveTime + 0.01f);

		// 5.4 Total turn angle cap (60 deg) causes straight-flight abort
		for (int32 i = 0; i < 20; ++i)
		{
			Projectile->Tick(0.1f);
			if (!Projectile->GetTestHomingActive())
			{
				break;
			}
		}
		TestFalse(TEXT("Homing stops once turn angle cap or timeout is reached"), Projectile->GetTestHomingActive());
		TestTrue(TEXT("Total turn angle does not exceed HomingMaxTotalTurnDegrees"),
			Projectile->GetTestTotalTurnAngleDegrees() <= HomingDef->HomingMaxTotalTurnDegrees + 0.01f);

		Projectile->Destroy();

		// 5.5 Target in rear hemisphere with <= 90 deg max turn causes immediate straight-flight abort
		ACombatProjectile* RearProj = World->SpawnActor<ACombatProjectile>();
		RearProj->SetActorLocation(FVector(500.0f, 0.0f, 100.0f));
		FCombatProjectileLaunchRequest RearReq = LaunchReq;
		RearReq.InitialFlightDirection = FVector(1.0f, 0.0f, 0.0f); // Facing +X
		RearProj->InitializeProjectile(RearReq);

		// Move enemy behind projectile (-X)
		Enemy->SetActorLocation(FVector(100.0f, 0.0f, 100.0f));
		RearProj->Tick(0.1f);

		TestFalse(TEXT("Target in rear hemisphere with <= 90 deg cap stops homing"), RearProj->GetTestHomingActive());
		RearProj->Destroy();

		// 5.6 Target in rear hemisphere with > 90 deg max turn (e.g. 180 deg) continues homing
		ACombatProjectile* UTurnProj = World->SpawnActor<ACombatProjectile>();
		UTurnProj->SetActorLocation(FVector(500.0f, 0.0f, 100.0f));
		FCombatProjectileLaunchRequest UTurnReq = LaunchReq;
		UTurnReq.HomingMaxTotalTurnDegrees = 180.0f;
		UTurnReq.InitialFlightDirection = FVector(1.0f, 0.0f, 0.0f);
		UTurnProj->InitializeProjectile(UTurnReq);

		Enemy->SetActorLocation(FVector(100.0f, 0.0f, 100.0f)); // Behind projectile
		UTurnProj->Tick(0.1f);
		TestTrue(TEXT("Target in rear hemisphere with 180 deg max turn continues homing"), UTurnProj->GetTestHomingActive());
		UTurnProj->Destroy();

		// 5.7 Target death causes immediate straight-flight abort
		ACombatProjectile* DeathProj = World->SpawnActor<ACombatProjectile>();
		DeathProj->SetActorLocation(FVector(0.0f, 0.0f, 100.0f));
		DeathProj->InitializeProjectile(LaunchReq);

		Enemy->SetActorLocation(FVector(800.0f, 200.0f, 100.0f));
		Enemy->GetAbilitySystemComponent()->AddLooseGameplayTag(TagDead);
		DeathProj->Tick(0.1f);

		TestFalse(TEXT("Target death immediately stops homing"), DeathProj->GetTestHomingActive());
		Enemy->GetAbilitySystemComponent()->RemoveLooseGameplayTag(TagDead);
		DeathProj->Destroy();

		// 5.8 Target destroyed causes immediate straight-flight abort
		ACombatProjectile* DestroyProj = World->SpawnActor<ACombatProjectile>();
		DestroyProj->SetActorLocation(FVector(0.0f, 0.0f, 100.0f));
		DestroyProj->InitializeProjectile(LaunchReq);

		Enemy->Destroy();
		DestroyProj->Tick(0.1f);

		TestFalse(TEXT("Target actor destruction immediately stops homing"), DestroyProj->GetTestHomingActive());
		DestroyProj->Destroy();

		Player->Destroy();
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
