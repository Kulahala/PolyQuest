// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include <limits>

#include "Camera/CameraComponent.h"
#include "AbilitySystemComponent.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Character/Player/PlayerCharacter.h"
#include "Character/Player/PlayerLockOnTargeting.h"
#include "Components/Image.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/RootMotionSource.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameplayTagContainer.h"
#include "UI/EnemyHealthBarWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerLockOnAutomationTest, "PolyQuest.Player.LockOn", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlayerLockOnAutomationTest::RunTest(const FString&)
{
	if (!TestNotNull(TEXT("Engine is available for transient lock-on fixtures"), GEngine))
	{
		return false;
	}

	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("PlayerLockOnTestWorld"));
	WorldContext.SetCurrentWorld(World);
	if (!TestNotNull(TEXT("Transient lock-on test world created"), World))
	{
		return false;
	}

	const FURL WorldURL;
	World->InitializeActorsForPlay(WorldURL);
	World->BeginPlay();

	struct FTestScopeCleanup
	{
		UWorld* WorldToDestroy = nullptr;

		~FTestScopeCleanup()
		{
			if (WorldToDestroy)
			{
				GEngine->DestroyWorldContext(WorldToDestroy);
				WorldToDestroy->DestroyWorld(false);
			}
		}
	} ScopeCleanup{ World };

	auto SpawnEnemy = [World](const FName Name, const FVector& Location)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = Name;
		AEnemyCharacter* Enemy = World->SpawnActor<AEnemyCharacter>(AEnemyCharacter::StaticClass(), Location, FRotator::ZeroRotator, SpawnParameters);
		if (Enemy)
		{
			Enemy->SetTestCombatTeamTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Team.Enemy")), false));
			Enemy->DispatchBeginPlay();
		}
		return Enemy;
	};

	AEnemyCharacter* EnemyRight = SpawnEnemy(TEXT("LockOn_Right"), FVector(500.0f, 0.0f, 100.0f));
	AEnemyCharacter* EnemyBottom = SpawnEnemy(TEXT("LockOn_Bottom"), FVector(500.0f, 200.0f, 100.0f));
	AEnemyCharacter* EnemyLeft = SpawnEnemy(TEXT("LockOn_Left"), FVector(-500.0f, 0.0f, 100.0f));
	AEnemyCharacter* EnemyTop = SpawnEnemy(TEXT("LockOn_Top"), FVector(0.0f, -500.0f, 100.0f));
	AEnemyCharacter* EnemyTie = SpawnEnemy(TEXT("LockOn_Tie"), FVector(600.0f, 0.0f, 100.0f));

	TestNotNull(TEXT("Right candidate spawned"), EnemyRight);
	TestNotNull(TEXT("Bottom candidate spawned"), EnemyBottom);
	TestNotNull(TEXT("Left candidate spawned"), EnemyLeft);
	TestNotNull(TEXT("Top candidate spawned"), EnemyTop);
	TestNotNull(TEXT("Tie candidate spawned"), EnemyTie);

	// -------------------------------------------------------------------------
	// SECTION 1: Pure strict-viewport, nearest, ordering, and cycling rules
	// -------------------------------------------------------------------------
	{
		const FVector2D ViewportSize(1920.0f, 1080.0f);
		TestTrue(TEXT("Strict viewport accepts an interior point"), FPlayerLockOnTargeting::IsStrictlyWithinViewport(FVector2D(960.0f, 540.0f), ViewportSize));
		TestFalse(TEXT("Strict viewport rejects left edge"), FPlayerLockOnTargeting::IsStrictlyWithinViewport(FVector2D(0.0f, 540.0f), ViewportSize));
		TestFalse(TEXT("Strict viewport rejects right edge"), FPlayerLockOnTargeting::IsStrictlyWithinViewport(FVector2D(1920.0f, 540.0f), ViewportSize));
		TestFalse(TEXT("Strict viewport rejects top edge"), FPlayerLockOnTargeting::IsStrictlyWithinViewport(FVector2D(960.0f, 0.0f), ViewportSize));
		TestFalse(TEXT("Strict viewport rejects bottom edge"), FPlayerLockOnTargeting::IsStrictlyWithinViewport(FVector2D(960.0f, 1080.0f), ViewportSize));
		TestFalse(TEXT("Strict viewport rejects non-finite projection"), FPlayerLockOnTargeting::IsStrictlyWithinViewport(FVector2D(std::numeric_limits<float>::quiet_NaN(), 540.0f), ViewportSize));

		float ClockwiseAngle = 0.0f;
		TestTrue(TEXT("Clockwise angle resolves screen-right"), FPlayerLockOnTargeting::TryCalculateClockwiseAngle(FVector2D(960.0f, 540.0f), FVector2D(1160.0f, 540.0f), ClockwiseAngle));
		TestTrue(TEXT("Screen-right is the zero-degree cycle origin"), FMath::IsNearlyZero(ClockwiseAngle));
		TestTrue(TEXT("Clockwise angle resolves screen-bottom"), FPlayerLockOnTargeting::TryCalculateClockwiseAngle(FVector2D(960.0f, 540.0f), FVector2D(960.0f, 740.0f), ClockwiseAngle));
		TestTrue(TEXT("Screen-bottom follows screen-right clockwise"), FMath::IsNearlyEqual(ClockwiseAngle, PI * 0.5f));
		TestFalse(TEXT("Clockwise angle rejects non-finite input"), FPlayerLockOnTargeting::TryCalculateClockwiseAngle(FVector2D(std::numeric_limits<float>::infinity(), 540.0f), FVector2D(960.0f, 740.0f), ClockwiseAngle));

		auto MakeCandidate = [](AEnemyCharacter* Target, const FVector2D ScreenPosition, const float Angle, const float DistanceSquared, const TCHAR* StableKey)
		{
			FPlayerLockOnCandidate Candidate;
			Candidate.TargetActor = Target;
			Candidate.ScreenPosition = ScreenPosition;
			Candidate.ClockwiseAngleRadians = Angle;
			Candidate.PlayerScreenDistanceSquared = DistanceSquared;
			Candidate.StableKey = StableKey;
			return Candidate;
		};

		TArray<FPlayerLockOnCandidate> Candidates;
		Candidates.Add(MakeCandidate(EnemyTop, FVector2D(960.0f, 340.0f), PI * 1.5f, 40000.0f, TEXT("Top")));
		Candidates.Add(MakeCandidate(EnemyLeft, FVector2D(760.0f, 540.0f), PI, 40000.0f, TEXT("Left")));
		Candidates.Add(MakeCandidate(EnemyBottom, FVector2D(960.0f, 740.0f), PI * 0.5f, 40000.0f, TEXT("Bottom")));
		Candidates.Add(MakeCandidate(EnemyRight, FVector2D(1160.0f, 540.0f), 0.0f, 40000.0f, TEXT("Right")));

		FPlayerLockOnTargeting::SortClockwise(Candidates);
		TestEqual(TEXT("Clockwise ordering begins at screen-right"), Candidates[0].TargetActor.Get(), EnemyRight);
		TestEqual(TEXT("Clockwise ordering moves to screen-bottom"), Candidates[1].TargetActor.Get(), EnemyBottom);
		TestEqual(TEXT("Clockwise ordering continues to screen-left"), Candidates[2].TargetActor.Get(), EnemyLeft);
		TestEqual(TEXT("Clockwise ordering ends at screen-top"), Candidates[3].TargetActor.Get(), EnemyTop);
		TestEqual(TEXT("Positive cycle advances clockwise"), FPlayerLockOnTargeting::FindCycledTargetIndex(Candidates, EnemyRight, 1), 1);
		TestEqual(TEXT("Negative cycle advances counter-clockwise"), FPlayerLockOnTargeting::FindCycledTargetIndex(Candidates, EnemyRight, -1), 3);
		TestEqual(TEXT("No lock has no cycle destination"), FPlayerLockOnTargeting::FindCycledTargetIndex(Candidates, nullptr, 1), INDEX_NONE);
		TestEqual(TEXT("Death successor advances from its cached clockwise anchor"), FPlayerLockOnTargeting::FindClockwiseSuccessorIndex(Candidates, Candidates[0]), 1);
		TestEqual(TEXT("Death successor wraps after the final clockwise candidate"), FPlayerLockOnTargeting::FindClockwiseSuccessorIndex(Candidates, Candidates[3]), 0);
		FPlayerLockOnCandidate InvalidDeathAnchor = Candidates[0];
		InvalidDeathAnchor.ClockwiseAngleRadians = std::numeric_limits<float>::quiet_NaN();
		TestEqual(TEXT("Death successor rejects a non-finite cached anchor"), FPlayerLockOnTargeting::FindClockwiseSuccessorIndex(Candidates, InvalidDeathAnchor), INDEX_NONE);

		TestEqual(TEXT("Mouse-nearest selection chooses closest valid candidate"), FPlayerLockOnTargeting::FindNearestToCursor(Candidates, FVector2D(970.0f, 720.0f)), 1);

		TArray<FPlayerLockOnCandidate> TieCandidates;
		TieCandidates.Add(MakeCandidate(EnemyTie, FVector2D(1160.0f, 540.0f), 0.0f, 40000.0f, TEXT("Z_Tie")));
		TieCandidates.Add(MakeCandidate(EnemyRight, FVector2D(1160.0f, 540.0f), 0.0f, 40000.0f, TEXT("A_Tie")));
		FPlayerLockOnTargeting::SortClockwise(TieCandidates);
		TestEqual(TEXT("Exact cycle tie falls back to stable key"), TieCandidates[0].TargetActor.Get(), EnemyRight);
		TestEqual(TEXT("Exact cursor-distance tie falls back to stable key"), FPlayerLockOnTargeting::FindNearestToCursor(TieCandidates, FVector2D(960.0f, 540.0f)), 0);
		TestEqual(TEXT("Death successor honors stable-key ordering for an equal-angle anchor"), FPlayerLockOnTargeting::FindClockwiseSuccessorIndex(TieCandidates, TieCandidates[0]), 1);
		TestEqual(TEXT("Death successor rejects an empty candidate list"), FPlayerLockOnTargeting::FindClockwiseSuccessorIndex(TArray<FPlayerLockOnCandidate>(), TieCandidates[0]), INDEX_NONE);
	}

	// -------------------------------------------------------------------------
	// SECTION 2: Player-owned target lifecycle, highlight handoff, and facing
	// -------------------------------------------------------------------------
	{
		APlayerCharacter* Player = World->SpawnActor<APlayerCharacter>(APlayerCharacter::StaticClass(), FVector(0.0f, 0.0f, 100.0f), FRotator::ZeroRotator);
		APlayerController* PlayerController = World->SpawnActor<APlayerController>();
		TestNotNull(TEXT("Player spawned for lock lifecycle"), Player);
		TestNotNull(TEXT("PlayerController spawned for lock lifecycle"), PlayerController);

		if (Player && PlayerController && EnemyRight && EnemyBottom && EnemyTie)
		{
			Player->SetTestCombatTeamTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Team.Player")), false));
			Player->DispatchBeginPlay();
			PlayerController->Possess(Player);

			UEnemyHealthBarWidget* RightWidget = NewObject<UEnemyHealthBarWidget>(EnemyRight);
			UEnemyHealthBarWidget* BottomWidget = NewObject<UEnemyHealthBarWidget>(EnemyBottom);
			UEnemyHealthBarWidget* TieWidget = NewObject<UEnemyHealthBarWidget>(EnemyTie);
			UImage* RightHighlight = NewObject<UImage>(RightWidget);
			UImage* BottomHighlight = NewObject<UImage>(BottomWidget);
			UImage* TieHighlight = NewObject<UImage>(TieWidget);
			RightHighlight->SetVisibility(ESlateVisibility::Collapsed);
			BottomHighlight->SetVisibility(ESlateVisibility::Collapsed);
			TieHighlight->SetVisibility(ESlateVisibility::Collapsed);
			RightWidget->SetTestTargetHighlightImage(RightHighlight);
			BottomWidget->SetTestTargetHighlightImage(BottomHighlight);
			TieWidget->SetTestTargetHighlightImage(TieHighlight);
			EnemyRight->SetTestHealthBarWidget(RightWidget);
			EnemyBottom->SetTestHealthBarWidget(BottomWidget);
			EnemyTie->SetTestHealthBarWidget(TieWidget);

			Player->SetTestLockOnProjectionHook([](const FVector& WorldPoint, FVector2D& OutScreenPosition, FVector2D& OutViewportSize)
			{
				OutViewportSize = FVector2D(1920.0f, 1080.0f);
				if (WorldPoint.Y > 100.0f)
				{
					OutScreenPosition = FVector2D(960.0f, 720.0f);
				}
				else if (WorldPoint.X < -100.0f)
				{
					OutScreenPosition = FVector2D(760.0f, 540.0f);
				}
				else if (WorldPoint.Y < -100.0f)
				{
					OutScreenPosition = FVector2D(960.0f, 340.0f);
				}
				else if (WorldPoint.X > 550.0f)
				{
					OutScreenPosition = FVector2D(1400.0f, 540.0f);
				}
				else if (WorldPoint.X > 100.0f)
				{
					OutScreenPosition = FVector2D(1100.0f, 540.0f);
				}
				else
				{
					OutScreenPosition = FVector2D(960.0f, 540.0f);
				}
				return true;
			});
			Player->SetTestLockOnCursorPosition(FVector2D(1110.0f, 540.0f));
			TestTrue(TEXT("World candidate query acquires the mouse-nearest target"), Player->TriggerTestAcquireLockOnTarget());
			TestEqual(TEXT("World candidate query locks the right-side mouse-nearest target"), Player->GetLockedTarget(), EnemyRight);
			TestEqual(TEXT("World candidate query highlights the acquired target"), RightHighlight->GetVisibility(), ESlateVisibility::HitTestInvisible);
			Player->SetTestLockedTarget(nullptr);

			Player->SetTestLockedTarget(EnemyRight);
			TestEqual(TEXT("Player exposes the newly locked target through the C++ query"), Player->GetLockedTarget(), EnemyRight);
			TestEqual(TEXT("Newly locked enemy shows its existing bar highlight"), RightHighlight->GetVisibility(), ESlateVisibility::HitTestInvisible);
			TestEqual(TEXT("Non-locked enemy bar stays unhighlighted"), BottomHighlight->GetVisibility(), ESlateVisibility::Collapsed);
			TestTrue(TEXT("Valid locked target survives explicit validation"), Player->TriggerTestValidateCurrentLockedTarget());

			Player->SetTestLockedTarget(EnemyBottom);
			TestEqual(TEXT("Switching targets clears the old bar highlight"), RightHighlight->GetVisibility(), ESlateVisibility::Collapsed);
			TestEqual(TEXT("Switching targets highlights the new bar"), BottomHighlight->GetVisibility(), ESlateVisibility::HitTestInvisible);

			const FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
			const FGameplayTag InvulnerableTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Invulnerable")), false);
			Player->SetTestLockedTarget(EnemyTie);
			TestTrue(TEXT("The death handoff fixture caches the current target screen candidate"), Player->TriggerTestValidateCurrentLockedTarget());
			if (UAbilitySystemComponent* TieAsc = EnemyTie->GetAbilitySystemComponent())
			{
				TieAsc->AddLooseGameplayTag(DeadTag);
				TestTrue(TEXT("A dead target completes one legal replacement attempt"), Player->TriggerTestValidateCurrentLockedTarget());
				TestEqual(TEXT("A dead target hands off to the next clockwise candidate"), Player->GetLockedTarget(), EnemyBottom);
				TestEqual(TEXT("Dead target bar highlight is cleared during handoff"), TieHighlight->GetVisibility(), ESlateVisibility::Collapsed);
				TestEqual(TEXT("Death handoff highlights the replacement target"), BottomHighlight->GetVisibility(), ESlateVisibility::HitTestInvisible);
				TestTrue(TEXT("The replacement remains locked on later validation without a second handoff"), Player->TriggerTestValidateCurrentLockedTarget());
				TestEqual(TEXT("Later validation retains the single death replacement"), Player->GetLockedTarget(), EnemyBottom);
				TieAsc->RemoveLooseGameplayTag(DeadTag);

				Player->SetTestLockedTarget(EnemyBottom);
				if (UAbilitySystemComponent* BottomAsc = EnemyBottom->GetAbilitySystemComponent())
				{
					BottomAsc->AddLooseGameplayTag(InvulnerableTag);
					TestFalse(TEXT("An invulnerable target clears without replacing it"), Player->TriggerTestValidateCurrentLockedTarget());
					TestNull(TEXT("Invulnerable target does not auto-retarget"), Player->GetLockedTarget());
					BottomAsc->RemoveLooseGameplayTag(InvulnerableTag);
				}
			}

			if (UAbilitySystemComponent* TopAsc = EnemyTop ? EnemyTop->GetAbilitySystemComponent() : nullptr)
			{
				for (AEnemyCharacter* OtherCandidate : { EnemyRight, EnemyBottom, EnemyLeft, EnemyTie })
				{
					if (UAbilitySystemComponent* OtherAsc = OtherCandidate ? OtherCandidate->GetAbilitySystemComponent() : nullptr)
					{
						OtherAsc->AddLooseGameplayTag(InvulnerableTag);
					}
				}

				Player->SetTestLockedTarget(EnemyTop);
				TestTrue(TEXT("The no-candidate death fixture caches its locked target"), Player->TriggerTestValidateCurrentLockedTarget());
				TopAsc->AddLooseGameplayTag(DeadTag);
				TestFalse(TEXT("A dead target clears when no valid clockwise replacement exists"), Player->TriggerTestValidateCurrentLockedTarget());
				TestNull(TEXT("No-candidate death handoff leaves no lock"), Player->GetLockedTarget());
				TopAsc->RemoveLooseGameplayTag(DeadTag);

				for (AEnemyCharacter* OtherCandidate : { EnemyRight, EnemyBottom, EnemyLeft, EnemyTie })
				{
					if (UAbilitySystemComponent* OtherAsc = OtherCandidate ? OtherCandidate->GetAbilitySystemComponent() : nullptr)
					{
						OtherAsc->RemoveLooseGameplayTag(InvulnerableTag);
					}
				}
			}

			Player->SetTestLockedTarget(EnemyRight);
			Player->SetTestLockOnProjectionHook([](const FVector&, FVector2D& OutScreenPosition, FVector2D& OutViewportSize)
			{
				OutScreenPosition = FVector2D(0.0f, 540.0f);
				OutViewportSize = FVector2D(1920.0f, 1080.0f);
				return true;
			});
			TestFalse(TEXT("A strict-viewport boundary projection clears the target"), Player->TriggerTestValidateCurrentLockedTarget());
			TestNull(TEXT("Off-screen target does not auto-retarget"), Player->GetLockedTarget());
			TestEqual(TEXT("Off-screen target highlight is cleared"), RightHighlight->GetVisibility(), ESlateVisibility::Collapsed);

			Player->SetTestLockOnProjectionHook([](const FVector&, FVector2D& OutScreenPosition, FVector2D& OutViewportSize)
			{
				OutScreenPosition = FVector2D(960.0f, 540.0f);
				OutViewportSize = FVector2D(1920.0f, 1080.0f);
				return true;
			});
			Player->SetTestLockedTarget(EnemyRight);
			if (UCharacterMovementComponent* MovementComponent = Player->GetCharacterMovement())
			{
				MovementComponent->SetMovementMode(MOVE_Walking);
				Player->SetActorRotation(FRotator(0.0f, 90.0f, 0.0f));
				Player->Tick(0.1f);
				TestTrue(TEXT("Locked ordinary locomotion uses the 800 degree-per-second default RotationRate"),
					FMath::IsNearlyEqual(Player->GetActorRotation().Yaw, 10.0f, 0.01f));
				TestFalse(TEXT("Locked ordinary locomotion disables movement-facing rotation"), MovementComponent->bOrientRotationToMovement);

				if (UAbilitySystemComponent* PlayerAsc = Player->GetAbilitySystemComponent())
				{
					const FGameplayTag SprintTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Movement.Sprinting")), false);
					const FGameplayTag GuardTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Guarding")), false);
					const FGameplayTag ParryTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Parrying")), false);
					const FGameplayTag AttackingTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Attacking")), false);
					const FGameplayTag DodgeTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Dodging")), false);
					const FGameplayTag HitReactingTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.HitReacting")), false);
					const FGameplayTag SmallHitReactingTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.SmallHitReacting")), false);

					PlayerAsc->AddLooseGameplayTag(GuardTag);
					Player->SetActorRotation(FRotator(0.0f, 90.0f, 0.0f));
					Player->Tick(0.2f);
					TestTrue(TEXT("Locked Guard continuously faces the target"), FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(Player->GetActorRotation().Yaw, 0.0f)));
					PlayerAsc->RemoveLooseGameplayTag(GuardTag);

					PlayerAsc->AddLooseGameplayTag(ParryTag);
					MovementComponent->SetMovementMode(MOVE_None);
					Player->SetActorRotation(FRotator(0.0f, 90.0f, 0.0f));
					Player->Tick(0.2f);
					TestTrue(TEXT("Locked Parry faces the target during its MOVE_None lock"), FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(Player->GetActorRotation().Yaw, 0.0f)));
					MovementComponent->SetMovementMode(MOVE_Walking);
					PlayerAsc->RemoveLooseGameplayTag(ParryTag);

					PlayerAsc->AddLooseGameplayTag(SprintTag);
					Player->SetActorRotation(FRotator(0.0f, 90.0f, 0.0f));
					Player->Tick(0.2f);
					TestTrue(TEXT("Locked Sprint preserves free-run facing"), FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(Player->GetActorRotation().Yaw, 90.0f)));
					TestTrue(TEXT("Locked Sprint restores movement-facing rotation"), MovementComponent->bOrientRotationToMovement);
					PlayerAsc->RemoveLooseGameplayTag(SprintTag);
					Player->Tick(0.2f);
					TestTrue(TEXT("Lock-facing resumes after Sprint ends"), FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(Player->GetActorRotation().Yaw, 0.0f)));

					for (const FGameplayTag& YawOwnerTag : { AttackingTag, DodgeTag, HitReactingTag, SmallHitReactingTag })
					{
						PlayerAsc->AddLooseGameplayTag(YawOwnerTag);
						Player->SetActorRotation(FRotator(0.0f, 90.0f, 0.0f));
						Player->Tick(0.2f);
						TestTrue(*FString::Printf(TEXT("Lock Tick does not overwrite %s yaw ownership"), *YawOwnerTag.ToString()),
							FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(Player->GetActorRotation().Yaw, 90.0f)));
						PlayerAsc->RemoveLooseGameplayTag(YawOwnerTag);
					}
				}

				TSharedPtr<FRootMotionSource_ConstantForce> RootMotionSource = MakeShared<FRootMotionSource_ConstantForce>();
				RootMotionSource->InstanceName = TEXT("LockOnAutomationRootMotion");
				RootMotionSource->Priority = 500;
				RootMotionSource->Duration = 1.0f;
				RootMotionSource->AccumulateMode = ERootMotionAccumulateMode::Override;
				RootMotionSource->Force = FVector(100.0f, 0.0f, 0.0f);
				const uint16 RootMotionSourceId = MovementComponent->ApplyRootMotionSource(RootMotionSource);
				TestTrue(TEXT("Transient Root Motion source activates for lock-facing protection"), Player->HasAnyRootMotion());
				Player->SetActorRotation(FRotator(0.0f, 90.0f, 0.0f));
				Player->Tick(0.2f);
				TestTrue(TEXT("Lock Tick does not overwrite active Root Motion yaw"), FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(Player->GetActorRotation().Yaw, 90.0f)));
				MovementComponent->RemoveRootMotionSourceByID(RootMotionSourceId);

				UObject* BowRequester = Player->GetFollowCamera();
				if (TestNotNull(TEXT("Player provides a concrete Bow requester fixture"), BowRequester))
				{
					TestTrue(TEXT("Bow requester registers for lock-facing regression coverage"), Player->RegisterBowAimRequester(BowRequester));
					Player->Tick(0.0f);
					const float BowYawBeforeLockTick = Player->GetActorRotation().Yaw;
					Player->Tick(0.2f);
					TestTrue(TEXT("Lock Tick does not overwrite active Bow aiming yaw"), FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(Player->GetActorRotation().Yaw, BowYawBeforeLockTick)));
					Player->UnregisterBowAimRequester(BowRequester);
				}
			}

			Player->SetTestLockedTarget(EnemyRight);
			if (UAbilitySystemComponent* PlayerAsc = Player->GetAbilitySystemComponent())
			{
				PlayerAsc->AddLooseGameplayTag(DeadTag);
				TestFalse(TEXT("A dead player clears the current lock"), Player->TriggerTestValidateCurrentLockedTarget());
				TestNull(TEXT("Player death never auto-retargets"), Player->GetLockedTarget());
				PlayerAsc->RemoveLooseGameplayTag(DeadTag);
			}

			Player->SetTestLockedTarget(EnemyTie);
			EnemyTie->Destroy();
			EnemyTie = nullptr;
			Player->Tick(0.0f);
			TestNull(TEXT("A destroyed target clears on Player Tick without auto-retarget"), Player->GetLockedTarget());

			Player->SetTestLockedTarget(EnemyRight);
			Player->SetTestBypassLockOnValidation(true);
			if (USpringArmComponent* CameraBoom = Player->GetCameraBoom())
			{
				CameraBoom->SetUsingAbsoluteRotation(true);
				CameraBoom->SetWorldRotation(FRotator(-55.0f, -45.0f, 0.0f));
			}
			else
			{
				AddError(TEXT("Lock-on directional Dodge fixture requires the Player CameraBoom."));
			}
			Player->SetActorRotation(FRotator(0.0f, 90.0f, 0.0f));
			Player->ApplyLockAwareActionFacing();
			TestTrue(TEXT("Locked attack-facing turns once toward the target"), FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(Player->GetActorRotation().Yaw, 0.0f)));

			Player->SetTestCurrentMoveInput(FVector2D(-1.0f, 0.0f));
			const FVector MovementFacing = Player->GetActionWorldDirection();
			Player->ApplyDodgeFacing();
			TestTrue(
				TEXT("Directional dodge keeps camera-relative movement facing over the lock"),
				FVector::DotProduct(Player->GetActorForwardVector().GetSafeNormal2D(), MovementFacing) > 0.9999f);

			Player->SetTestCurrentMoveInput(FVector2D::ZeroVector);
			Player->SetActorRotation(FRotator(0.0f, 90.0f, 0.0f));
			Player->ApplyDodgeFacing();
			TestTrue(TEXT("No-input dodge falls back to the locked target"), FMath::IsNearlyZero(FMath::FindDeltaAngleDegrees(Player->GetActorRotation().Yaw, 0.0f)));
			Player->SetTestBypassLockOnValidation(false);

			Player->SetTestLockedTarget(nullptr);
			TestEqual(TEXT("Explicit clear removes the final target highlight"), RightHighlight->GetVisibility(), ESlateVisibility::Collapsed);
			PlayerController->Destroy();
			Player->Destroy();
		}
	}

	if (EnemyRight)
	{
		EnemyRight->Destroy();
	}
	if (EnemyBottom)
	{
		EnemyBottom->Destroy();
	}
	if (EnemyLeft)
	{
		EnemyLeft->Destroy();
	}
	if (EnemyTop)
	{
		EnemyTop->Destroy();
	}
	if (EnemyTie)
	{
		EnemyTie->Destroy();
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
