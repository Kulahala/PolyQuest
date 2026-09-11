#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/Abilities/EnemyLaunchReactionAbility.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/CharacterAttributeSet.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Character/Player/PlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "Tests/CombatAutomationFixture.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEnemyLaunchReactionRootMotionAutomationTest,
	"PolyQuest.Combat.EnemyLaunchReactionRootMotion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	const FGameplayTag TagBlockFacing = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Block.Facing")), false);
	const FGameplayTag TagTeardownOnUnpossess = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.Teardown.OnUnpossess")), false);

	struct FEnemyRootMotionTestWorldScopeCleanup
	{
		UWorld* World = nullptr;
		~FEnemyRootMotionTestWorldScopeCleanup()
		{
			if (World)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
		}
	};

	class UTestLaunchAbilityAccessHelper : public UEnemyLaunchReactionAbility
	{
	public:
		static void CallAbilityActivation(
			UEnemyLaunchReactionAbility* Ability,
			const FGameplayAbilitySpecHandle Handle,
			const FGameplayAbilityActorInfo* ActorInfo,
			const FGameplayAbilityActivationInfo ActivationInfo,
			const FGameplayEventData* TriggerEventData)
		{
			if (Ability)
			{
				static_cast<UTestLaunchAbilityAccessHelper*>(Ability)->CallActivateAbility(
					Handle, ActorInfo, ActivationInfo, nullptr, TriggerEventData);
			}
		}
	};

	struct FTestAbilityFixtureScope
	{
		UAbilitySystemComponent* ASC = nullptr;
		FGameplayAbilitySpecHandle Handle;
		UEnemyLaunchReactionAbility* AbilityInstance = nullptr;

		FTestAbilityFixtureScope(UAbilitySystemComponent* InASC, AEnemyCharacter* Enemy)
			: ASC(InASC)
		{
			if (ASC && Enemy)
			{
				FGameplayAbilitySpec Spec(UEnemyLaunchReactionAbility::StaticClass(), 1, INDEX_NONE, Enemy);
				Handle = ASC->GiveAbility(Spec);
				if (FGameplayAbilitySpec* FoundSpec = ASC->FindAbilitySpecFromHandle(Handle))
				{
					AbilityInstance = Cast<UEnemyLaunchReactionAbility>(FoundSpec->GetPrimaryInstance());
				}
			}
		}

		void Activate(const FGameplayEventData* TriggerPayload = nullptr)
		{
			if (AbilityInstance && ASC)
			{
				UTestLaunchAbilityAccessHelper::CallAbilityActivation(
					AbilityInstance, Handle, ASC->AbilityActorInfo.Get(), FGameplayAbilityActivationInfo(), TriggerPayload);
			}
		}

		~FTestAbilityFixtureScope()
		{
			if (ASC && Handle.IsValid())
			{
				ASC->ClearAbility(Handle);
			}
		}
	};

	class UTestMontageAccessHelper : public UAnimMontage
	{
	public:
		static void SetMontageLength(UAnimMontage* Montage, float Length)
		{
			if (Montage)
			{
				static_cast<UTestMontageAccessHelper*>(Montage)->SequenceLength = Length;
			}
		}
	};

	UAnimMontage* CreateSyntheticKnockdownMontage(UObject* Outer = nullptr, float Length = 1.5f, bool bHasRootMotion = true)
	{
		(void)Outer;
		UAnimMontage* Montage = NewObject<UAnimMontage>(GetTransientPackage());
		UAnimSequence* Seq = NewObject<UAnimSequence>(GetTransientPackage());
		Seq->bEnableRootMotion = bHasRootMotion;

		FSlotAnimationTrack Track;
		Track.SlotName = FName(TEXT("DefaultSlot"));
		FAnimSegment Segment;
		Segment.SetAnimReference(Seq);
		Segment.StartPos = 0.0f;
		Segment.AnimStartTime = 0.0f;
		Segment.AnimEndTime = Length;
		Segment.AnimPlayRate = 1.0f;
		Track.AnimTrack.AnimSegments.Add(Segment);
		Montage->SlotAnimTracks.Add(Track);

		UTestMontageAccessHelper::SetMontageLength(Montage, Length);
		return Montage;
	}
}

bool FEnemyLaunchReactionRootMotionAutomationTest::RunTest(const FString& Parameters)
{
	AddExpectedErrorPlain(TEXT("SequencerDataModel"), EAutomationExpectedErrorFlags::Contains, -1);

	// -------------------------------------------------------------------------
	// SECTION 1: CDO Defaults and Static Interface Verification
	// -------------------------------------------------------------------------
	{
		const UEnemyLaunchReactionAbility* EnemyLaunchCDO = UEnemyLaunchReactionAbility::StaticClass()->GetDefaultObject<UEnemyLaunchReactionAbility>();
		TestNotNull(TEXT("1.1: EnemyLaunch CDO exists"), EnemyLaunchCDO);
		if (EnemyLaunchCDO)
		{
			TestTrue(TEXT("1.2: CDO bUseGroundedRootMotionKnockdown defaults to true"), EnemyLaunchCDO->GetUseGroundedRootMotionKnockdown());
			TestNull(TEXT("1.3: CDO RootMotionKnockdownMontage defaults to nullptr"), EnemyLaunchCDO->GetTestRootMotionKnockdownMontage());
			TestTrue(TEXT("1.4: TagBlockFacing is valid"), TagBlockFacing.IsValid());
			TestTrue(TEXT("1.5: TagTeardownOnUnpossess is valid"), TagTeardownOnUnpossess.IsValid());
			TestTrue(TEXT("1.6: EnemyLaunch CDO ActivationOwnedTags contains State.Block.Facing"),
				EnemyLaunchCDO->GetTestActivationOwnedTags().HasTagExact(TagBlockFacing));
			TestTrue(TEXT("1.7: EnemyLaunch CDO AbilityTags contains Ability.Action.Teardown.OnUnpossess"),
				EnemyLaunchCDO->GetAssetTags().HasTagExact(TagTeardownOnUnpossess));
			TestEqual(TEXT("1.8: EnemyLaunch CDO AbilitiesToCancel has exactly 2 entries"),
				EnemyLaunchCDO->GetTestAbilitiesToCancel().Num(), 2);
			TestFalse(TEXT("1.9: EnemyLaunch CDO AbilitiesToCancel does NOT contain TeardownOnUnpossess"),
				EnemyLaunchCDO->GetTestAbilitiesToCancel().HasTag(TagTeardownOnUnpossess));
		}
	}

	// Setup Test World
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("EnemyLaunchRootMotionTestWorld"));
	WorldContext.SetCurrentWorld(World);
	if (!TestNotNull(TEXT("Test World created successfully"), World))
	{
		return false;
	}

	FURL WorldURL;
	World->InitializeActorsForPlay(WorldURL);
	FEnemyRootMotionTestWorldScopeCleanup ScopeCleanup{ World };

	AEnemyCharacter* Enemy = FCombatAutomationFixture::SpawnPassiveEnemy(World, FTransform(FRotator::ZeroRotator, FVector::ZeroVector));
	if (!TestNotNull(TEXT("Enemy spawned successfully"), Enemy))
	{
		return false;
	}

	UAbilitySystemComponent* EnemyASC = Enemy->GetAbilitySystemComponent();
	UCharacterMovementComponent* MovementComponent = Enemy->GetCharacterMovement();
	if (!TestNotNull(TEXT("Enemy ASC valid"), EnemyASC) || !TestNotNull(TEXT("Enemy MovementComponent valid"), MovementComponent))
	{
		return false;
	}

	MovementComponent->SetMovementMode(MOVE_Walking);

	APlayerCharacter* Attacker = FCombatAutomationFixture::SpawnPlayer(World, FTransform(FRotator::ZeroRotator, FVector(200.0f, 0.0f, 0.0f)));
	if (!TestNotNull(TEXT("Attacker spawned successfully"), Attacker))
	{
		return false;
	}

	FGameplayEventData DefaultTriggerPayload;
	DefaultTriggerPayload.Instigator = Attacker;
	DefaultTriggerPayload.Target = Enemy;

	UAnimInstance* MockAnimInstance = NewObject<UAnimInstance>(Enemy->GetMesh());
	if (!TestNotNull(TEXT("MockAnimInstance created"), MockAnimInstance))
	{
		return false;
	}

	UEnemyLaunchReactionAbility* EnemyLaunchCDO = UEnemyLaunchReactionAbility::StaticClass()->GetDefaultObject<UEnemyLaunchReactionAbility>();
	UAnimInstance* OriginalCDOBoundAnimInstance = EnemyLaunchCDO ? EnemyLaunchCDO->GetTestBoundAnimInstance() : nullptr;
	if (EnemyLaunchCDO)
	{
		EnemyLaunchCDO->SetTestBoundAnimInstance(MockAnimInstance);
	}

	struct FCDOAnimInstanceGuard
	{
		UEnemyLaunchReactionAbility* CDO = nullptr;
		UAnimInstance* Original = nullptr;
		~FCDOAnimInstanceGuard()
		{
			if (CDO)
			{
				CDO->SetTestBoundAnimInstance(Original);
			}
		}
	} CDOGuard{ EnemyLaunchCDO, OriginalCDOBoundAnimInstance };

	// -------------------------------------------------------------------------
	// SECTION 2: SelectsRootMotionBranchWhenConfigured
	// -------------------------------------------------------------------------
	{
		UAnimMontage* ValidRootMotionMontage = CreateSyntheticKnockdownMontage(Enemy, 2.0f, true);
		TestNotNull(TEXT("2.1: ValidRootMotionMontage created"), ValidRootMotionMontage);
		TestTrue(TEXT("2.2: ValidRootMotionMontage HasRootMotion is true"), ValidRootMotionMontage->HasRootMotion());

		FTestAbilityFixtureScope Scope(EnemyASC, Enemy);
		TestNotNull(TEXT("2.3: AbilityInstance created"), Scope.AbilityInstance);
		if (Scope.AbilityInstance)
		{
			Scope.AbilityInstance->SetTestRootMotionKnockdownMontage(ValidRootMotionMontage);
			Scope.AbilityInstance->SetTestUseGroundedRootMotionKnockdown(true);
			Scope.AbilityInstance->SetTestBoundAnimInstance(MockAnimInstance);
			Scope.AbilityInstance->SetTestBypassMontageActiveCheck(true);

			TestTrue(TEXT("2.4: CanActivateAbility passes with Root Motion configuration alone"),
				Scope.AbilityInstance->CanActivateAbility(Scope.Handle, EnemyASC->AbilityActorInfo.Get()));

			// Activate Ability
			Scope.Activate(&DefaultTriggerPayload);

			TestTrue(TEXT("2.5: Ability entered RootMotionKnockdown phase"),
				Scope.AbilityInstance->IsTestPhaseRootMotionKnockdown());
			TestTrue(TEXT("2.5b: Root Motion branch grants State.Block.Facing"),
				EnemyASC->HasMatchingGameplayTag(TagBlockFacing));

			TestFalse(TEXT("2.6: Root Motion branch does NOT create CommitEventTask"), Scope.AbilityInstance->GetTestCommitEventTaskActive());
			TestFalse(TEXT("2.7: Root Motion branch does NOT create FacingTurnTask"), Scope.AbilityInstance->GetTestFacingTurnTaskActive());
			TestFalse(TEXT("2.8: Root Motion branch does NOT create FallValidationTask"), Scope.AbilityInstance->GetTestFallValidationTaskActive());

			Scope.AbilityInstance->EndAbility(Scope.Handle, EnemyASC->AbilityActorInfo.Get(), FGameplayAbilityActivationInfo(), false, false);
			TestFalse(TEXT("2.9: Root Motion branch EndAbility removes State.Block.Facing"),
				EnemyASC->HasMatchingGameplayTag(TagBlockFacing));
		}
	}

	// -------------------------------------------------------------------------
	// SECTION 3: FallsBackToPhysicsLaunchWhenDisabledOrInvalid
	// -------------------------------------------------------------------------
	{
		UAnimMontage* ValidRootMontage = CreateSyntheticKnockdownMontage(Enemy, 2.0f, true);
		UAnimMontage* TakeoffMontage = CreateSyntheticKnockdownMontage(Enemy, 1.0f, false);
		UAnimMontage* LandingMontage = CreateSyntheticKnockdownMontage(Enemy, 1.0f, false);

		// 3.1 Switch disabled (bUseGroundedRootMotionKnockdown == false) -> falls back to Legacy Takeoff
		{
			FTestAbilityFixtureScope Scope(EnemyASC, Enemy);
			TestNotNull(TEXT("3.1: DisabledAbility created"), Scope.AbilityInstance);
			if (Scope.AbilityInstance)
			{
				Scope.AbilityInstance->SetTestRootMotionKnockdownMontage(ValidRootMontage);
				Scope.AbilityInstance->SetTestTakeoffMontage(TakeoffMontage);
				Scope.AbilityInstance->SetTestLandingRecoveryMontage(LandingMontage);
				Scope.AbilityInstance->SetTestUseGroundedRootMotionKnockdown(false);
				Scope.AbilityInstance->SetTestBoundAnimInstance(MockAnimInstance);
				Scope.AbilityInstance->SetTestBypassMontageActiveCheck(true);

				Scope.Activate(nullptr);

				TestTrue(TEXT("3.1: Disabled Root Motion falls back to Legacy Takeoff phase"),
					Scope.AbilityInstance->IsTestPhaseTakeoff());
				TestTrue(TEXT("3.1b: Legacy branch grants State.Block.Facing"),
					EnemyASC->HasMatchingGameplayTag(TagBlockFacing));
				TestTrue(TEXT("3.1c: Legacy branch creates CommitEventTask"), Scope.AbilityInstance->GetTestCommitEventTaskActive());

				Scope.AbilityInstance->EndAbility(Scope.Handle, EnemyASC->AbilityActorInfo.Get(), FGameplayAbilityActivationInfo(), false, true);
				TestFalse(TEXT("3.1d: Legacy branch EndAbility removes State.Block.Facing"),
					EnemyASC->HasMatchingGameplayTag(TagBlockFacing));
			}
		}

		// 3.2 RootMotionKnockdownMontage is null -> falls back to Legacy Takeoff
		{
			FTestAbilityFixtureScope Scope(EnemyASC, Enemy);
			TestNotNull(TEXT("3.2: NullMontageAbility created"), Scope.AbilityInstance);
			if (Scope.AbilityInstance)
			{
				Scope.AbilityInstance->SetTestRootMotionKnockdownMontage(nullptr);
				Scope.AbilityInstance->SetTestTakeoffMontage(TakeoffMontage);
				Scope.AbilityInstance->SetTestLandingRecoveryMontage(LandingMontage);
				Scope.AbilityInstance->SetTestUseGroundedRootMotionKnockdown(true);
				Scope.AbilityInstance->SetTestBoundAnimInstance(MockAnimInstance);
				Scope.AbilityInstance->SetTestBypassMontageActiveCheck(true);

				Scope.Activate(nullptr);

				TestTrue(TEXT("3.2: Null Root Motion montage falls back to Legacy Takeoff phase"),
					Scope.AbilityInstance->IsTestPhaseTakeoff());

				Scope.AbilityInstance->EndAbility(Scope.Handle, EnemyASC->AbilityActorInfo.Get(), FGameplayAbilityActivationInfo(), false, true);
			}
		}

		// 3.3 RootMotionKnockdownMontage has NO root motion -> falls back to Legacy Takeoff
		{
			UAnimMontage* NoRootMotionMontage = CreateSyntheticKnockdownMontage(Enemy, 1.5f, false);
			FTestAbilityFixtureScope Scope(EnemyASC, Enemy);
			TestNotNull(TEXT("3.3: NoRMAbility created"), Scope.AbilityInstance);
			if (Scope.AbilityInstance)
			{
				Scope.AbilityInstance->SetTestRootMotionKnockdownMontage(NoRootMotionMontage);
				Scope.AbilityInstance->SetTestTakeoffMontage(TakeoffMontage);
				Scope.AbilityInstance->SetTestLandingRecoveryMontage(LandingMontage);
				Scope.AbilityInstance->SetTestUseGroundedRootMotionKnockdown(true);
				Scope.AbilityInstance->SetTestBoundAnimInstance(MockAnimInstance);
				Scope.AbilityInstance->SetTestBypassMontageActiveCheck(true);

				Scope.Activate(nullptr);

				TestTrue(TEXT("3.3: Montage without root motion falls back to Legacy Takeoff phase"),
					Scope.AbilityInstance->IsTestPhaseTakeoff());

				Scope.AbilityInstance->EndAbility(Scope.Handle, EnemyASC->AbilityActorInfo.Get(), FGameplayAbilityActivationInfo(), false, true);
			}
		}

		// 3.4 Both branches invalid -> CanActivateAbility fails closed
		{
			FTestAbilityFixtureScope Scope(EnemyASC, Enemy);
			TestNotNull(TEXT("3.4: InvalidAbility created"), Scope.AbilityInstance);
			if (Scope.AbilityInstance)
			{
				Scope.AbilityInstance->SetTestRootMotionKnockdownMontage(nullptr);
				Scope.AbilityInstance->SetTestTakeoffMontage(nullptr);
				Scope.AbilityInstance->SetTestLandingRecoveryMontage(nullptr);
				Scope.AbilityInstance->SetTestBoundAnimInstance(MockAnimInstance);

				TestFalse(TEXT("3.4: Both branches invalid fails activation closed"),
					Scope.AbilityInstance->CanActivateAbility(Scope.Handle, EnemyASC->AbilityActorInfo.Get()));
			}
		}
	}

	// -------------------------------------------------------------------------
	// SECTION 4: InstantFacingAppliedBeforeMontageActivation
	// -------------------------------------------------------------------------
	{
		// 4.1 Pure Yaw resolver calculations
		float OutFacingYaw = 0.0f;

		// Attacker directly in front (+X), RefYaw = 0 -> FacingYaw = 0
		TestTrue(TEXT("4.1a: Front attacker resolves successfully"),
			UEnemyLaunchReactionAbility::CallTestTryResolveRootMotionFacingYaw(FVector(1.0f, 0.0f, 0.0f), 0.0f, OutFacingYaw));
		TestEqual(TEXT("4.1a: Front attacker resolves to 0 deg"), OutFacingYaw, 0.0f);

		// Attacker directly to right (+Y), RefYaw = 0 -> FacingYaw = 90
		TestTrue(TEXT("4.1b: Right attacker resolves successfully"),
			UEnemyLaunchReactionAbility::CallTestTryResolveRootMotionFacingYaw(FVector(0.0f, 1.0f, 0.0f), 0.0f, OutFacingYaw));
		TestEqual(TEXT("4.1b: Right attacker resolves to 90 deg"), OutFacingYaw, 90.0f);

		// Attacker with non-zero Z offset, planar projection resolves accurately
		TestTrue(TEXT("4.1c: Attacker with vertical offset resolves successfully"),
			UEnemyLaunchReactionAbility::CallTestTryResolveRootMotionFacingYaw(FVector(1.0f, 0.0f, 100.0f), 0.0f, OutFacingYaw));
		TestEqual(TEXT("4.1c: Vertical offset ignored in planar yaw"), OutFacingYaw, 0.0f);

		// Attacker directly to right (+Y) with RefYaw = 45 -> FacingYaw = 135
		TestTrue(TEXT("4.1d: Transformed attacker resolves successfully"),
			UEnemyLaunchReactionAbility::CallTestTryResolveRootMotionFacingYaw(FVector(0.0f, 1.0f, 0.0f), 45.0f, OutFacingYaw));
		TestEqual(TEXT("4.1d: Reference yaw combined correctly (135 deg)"), OutFacingYaw, 135.0f);

		// Degenerate inputs fail closed
		TestFalse(TEXT("4.1e: Zero vector fails"),
			UEnemyLaunchReactionAbility::CallTestTryResolveRootMotionFacingYaw(FVector::ZeroVector, 0.0f, OutFacingYaw));
		TestFalse(TEXT("4.1f: NaN vector fails"),
			UEnemyLaunchReactionAbility::CallTestTryResolveRootMotionFacingYaw(FVector(NAN, 0.0f, 0.0f), 0.0f, OutFacingYaw));

		// 4.2 Applied instant facing on Actor
		Enemy->SetActorRotation(FRotator(0.0f, 30.0f, 0.0f));

		UAnimMontage* ValidRootMontage = CreateSyntheticKnockdownMontage(Enemy, 2.0f, true);
		FTestAbilityFixtureScope Scope(EnemyASC, Enemy);
		TestNotNull(TEXT("4.2: FacingAbility created"), Scope.AbilityInstance);
		if (Scope.AbilityInstance)
		{
			Scope.AbilityInstance->SetTestRootMotionKnockdownMontage(ValidRootMontage);
			Scope.AbilityInstance->SetTestUseGroundedRootMotionKnockdown(true);
			Scope.AbilityInstance->SetTestBoundAnimInstance(MockAnimInstance);
			Scope.AbilityInstance->SetTestBypassMontageActiveCheck(true);

			// Payload with Instigator at local +Y (to the right)
			APlayerCharacter* Player = FCombatAutomationFixture::SpawnPlayer(World, FTransform(FRotator::ZeroRotator, FVector(0.0f, 200.0f, 0.0f)));
			TestNotNull(TEXT("4.2: Player spawned as attacker"), Player);

			FGameplayEventData TriggerPayload;
			TriggerPayload.Instigator = Player;
			TriggerPayload.Target = Enemy;

			Scope.Activate(&TriggerPayload);

			TestTrue(TEXT("4.2: Enemy yaw rotated to face attacker directly"),
				FMath::IsNearlyEqual(Enemy->GetActorRotation().Yaw, 90.0f, 1.0f));
			TestTrue(TEXT("4.2: Velocity remains zero (no Launch applied)"),
				MovementComponent->Velocity.IsNearlyZero());

			Scope.AbilityInstance->EndAbility(Scope.Handle, EnemyASC->AbilityActorInfo.Get(), FGameplayAbilityActivationInfo(), false, false);
			if (Player)
			{
				Player->Destroy();
			}
		}
	}

	// -------------------------------------------------------------------------
	// SECTION 5: LedgeProtectionRestoredOnAllExitPaths
	// -------------------------------------------------------------------------
	{
		UAnimMontage* ValidRootMontage = CreateSyntheticKnockdownMontage(Enemy, 2.0f, true);

		// 5.1 Natural Completion restores ledge walk-off
		{
			MovementComponent->bCanWalkOffLedges = true;

			FTestAbilityFixtureScope Scope(EnemyASC, Enemy);
			TestNotNull(TEXT("5.1: NaturalAbility created"), Scope.AbilityInstance);
			if (Scope.AbilityInstance)
			{
				Scope.AbilityInstance->SetTestRootMotionKnockdownMontage(ValidRootMontage);
				Scope.AbilityInstance->SetTestUseGroundedRootMotionKnockdown(true);
				Scope.AbilityInstance->SetTestBoundAnimInstance(MockAnimInstance);
				Scope.AbilityInstance->SetTestBypassMontageActiveCheck(true);

				Scope.Activate(&DefaultTriggerPayload);

				TestFalse(TEXT("5.1a: bCanWalkOffLedges disabled upon activation"), MovementComponent->bCanWalkOffLedges);
				TestTrue(TEXT("5.1b: bLedgeSettingModified marked true"), Scope.AbilityInstance->GetTestLedgeSettingModified());
				TestTrue(TEXT("5.1b2: State.Block.Facing active during reaction"), EnemyASC->HasMatchingGameplayTag(TagBlockFacing));

				// Natural montage end
				Scope.AbilityInstance->SetTestLandingRecoveryCompletedNaturally(true);
				Scope.AbilityInstance->EndAbility(Scope.Handle, EnemyASC->AbilityActorInfo.Get(), FGameplayAbilityActivationInfo(), false, false);

				TestTrue(TEXT("5.1c: bCanWalkOffLedges restored to true on natural end"), MovementComponent->bCanWalkOffLedges);
				TestFalse(TEXT("5.1d: bLedgeSettingModified reset to false"), Scope.AbilityInstance->GetTestLedgeSettingModified());
				TestFalse(TEXT("5.1e: State.Block.Facing removed on natural end"), EnemyASC->HasMatchingGameplayTag(TagBlockFacing));
			}
		}

		// 5.2 Interrupted / Cancelled restores ledge walk-off
		{
			MovementComponent->bCanWalkOffLedges = true;

			FTestAbilityFixtureScope Scope(EnemyASC, Enemy);
			TestNotNull(TEXT("5.2: InterruptedAbility created"), Scope.AbilityInstance);
			if (Scope.AbilityInstance)
			{
				Scope.AbilityInstance->SetTestRootMotionKnockdownMontage(ValidRootMontage);
				Scope.AbilityInstance->SetTestUseGroundedRootMotionKnockdown(true);
				Scope.AbilityInstance->SetTestBoundAnimInstance(MockAnimInstance);
				Scope.AbilityInstance->SetTestBypassMontageActiveCheck(true);

				Scope.Activate(&DefaultTriggerPayload);

				TestFalse(TEXT("5.2a: bCanWalkOffLedges disabled upon activation"), MovementComponent->bCanWalkOffLedges);
				TestTrue(TEXT("5.2a2: State.Block.Facing active during reaction"), EnemyASC->HasMatchingGameplayTag(TagBlockFacing));

				// Interrupted cancel
				Scope.AbilityInstance->EndAbility(Scope.Handle, EnemyASC->AbilityActorInfo.Get(), FGameplayAbilityActivationInfo(), false, true);

				TestTrue(TEXT("5.2b: bCanWalkOffLedges restored to true on interrupted cancel"), MovementComponent->bCanWalkOffLedges);
				TestFalse(TEXT("5.2c: State.Block.Facing removed on interrupted cancel"), EnemyASC->HasMatchingGameplayTag(TagBlockFacing));
			}
		}
	}

	// -------------------------------------------------------------------------
	// SECTION 6: PrematureFallingAbortsReactionAndStanceBreak
	// -------------------------------------------------------------------------
	{
		UAnimMontage* ValidRootMontage = CreateSyntheticKnockdownMontage(Enemy, 2.0f, true);
		FTestAbilityFixtureScope Scope(EnemyASC, Enemy);
		TestNotNull(TEXT("6.1: FallingAbility created"), Scope.AbilityInstance);
		if (Scope.AbilityInstance)
		{
			Scope.AbilityInstance->SetTestRootMotionKnockdownMontage(ValidRootMontage);
			Scope.AbilityInstance->SetTestUseGroundedRootMotionKnockdown(true);
			Scope.AbilityInstance->SetTestBoundAnimInstance(MockAnimInstance);
			Scope.AbilityInstance->SetTestBypassMontageActiveCheck(true);

			Scope.Activate(&DefaultTriggerPayload);

			TestTrue(TEXT("6.1: Active phase is RootMotionKnockdown"),
				Scope.AbilityInstance->IsTestPhaseRootMotionKnockdown());

			// Arm stance break deferral and zero poise
			Enemy->BeginLaunchStanceBreakDeferral();
			EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetPoiseAttribute(), 0.0f);
			TestTrue(TEXT("6.2: Stance break deferral active"), Enemy->IsLaunchStanceBreakDeferralActive());

			// Trigger premature falling
			MovementComponent->SetMovementMode(MOVE_Falling);
			Scope.AbilityInstance->TriggerTestMovementModeChanged(Enemy, MOVE_Walking, 0);

			// Verify Ability ended and deferral was aborted
			TestTrue(TEXT("6.3: Ability ended back to None phase on premature falling"),
				Scope.AbilityInstance->IsTestPhaseNone());
			TestFalse(TEXT("6.4: Stance break deferral aborted upon premature falling"),
				Enemy->IsLaunchStanceBreakDeferralActive());
			TestEqual(TEXT("6.5: Poise restored to 100 on abort"),
				EnemyASC->GetNumericAttribute(UCharacterAttributeSet::GetPoiseAttribute()), 100.0f);

			MovementComponent->SetMovementMode(MOVE_Walking);
		}
	}

	// -------------------------------------------------------------------------
	// SECTION 7: CommitNotifyIsNonBlocking
	// -------------------------------------------------------------------------
	{
		UAnimMontage* ValidRootMontage = CreateSyntheticKnockdownMontage(Enemy, 2.0f, true);
		FTestAbilityFixtureScope Scope(EnemyASC, Enemy);
		TestNotNull(TEXT("7.1: NonBlockingAbility created"), Scope.AbilityInstance);
		if (Scope.AbilityInstance)
		{
			Scope.AbilityInstance->SetTestRootMotionKnockdownMontage(ValidRootMontage);
			Scope.AbilityInstance->SetTestUseGroundedRootMotionKnockdown(true);
			Scope.AbilityInstance->SetTestBoundAnimInstance(MockAnimInstance);
			Scope.AbilityInstance->SetTestBypassMontageActiveCheck(true);

			Scope.Activate(&DefaultTriggerPayload);

			TestTrue(TEXT("7.1: Active phase is RootMotionKnockdown"),
				Scope.AbilityInstance->IsTestPhaseRootMotionKnockdown());

			// Dispatch Commit GameplayEvent / Callback directly
			FGameplayEventData CommitPayload;
			CommitPayload.OptionalObject = ValidRootMontage;
			Scope.AbilityInstance->TriggerTestLaunchCommitEvent(CommitPayload);

			TestTrue(TEXT("7.2: Commit event ignored in RootMotionKnockdown phase"),
				Scope.AbilityInstance->IsTestPhaseRootMotionKnockdown());
			TestTrue(TEXT("7.3: Velocity remains zero, LaunchCharacter not invoked"),
				MovementComponent->Velocity.IsNearlyZero());

			// Montage finishes naturally without dependency on commit
			Enemy->BeginLaunchStanceBreakDeferral();
			EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetPoiseAttribute(), 0.0f);

			Scope.AbilityInstance->TriggerTestActiveMontageEnded(ValidRootMontage, false);

			TestTrue(TEXT("7.4: Ability successfully ended on natural montage completion"),
				Scope.AbilityInstance->IsTestPhaseNone());
			TestFalse(TEXT("7.5: Stance break completed naturally and deferral cleared"),
				Enemy->IsLaunchStanceBreakDeferralActive());
		}
	}

	// -------------------------------------------------------------------------
	// SECTION 8: State.Block.Facing Lifecycle, UnPossess Teardown, and Cancel
	// -------------------------------------------------------------------------
	{
		UAnimMontage* ValidRootMontage = CreateSyntheticKnockdownMontage(Enemy, 2.0f, true);

		// 8.1 UnPossess Teardown cancels ability and removes State.Block.Facing
		{
			FTestAbilityFixtureScope Scope(EnemyASC, Enemy);
			TestNotNull(TEXT("8.1: TeardownAbility created"), Scope.AbilityInstance);
			if (Scope.AbilityInstance)
			{
				Scope.AbilityInstance->SetTestRootMotionKnockdownMontage(ValidRootMontage);
				Scope.AbilityInstance->SetTestUseGroundedRootMotionKnockdown(true);
				Scope.AbilityInstance->SetTestBoundAnimInstance(MockAnimInstance);
				Scope.AbilityInstance->SetTestBypassMontageActiveCheck(true);

				Scope.Activate(&DefaultTriggerPayload);

				TestTrue(TEXT("8.1a: Ability active"), Scope.AbilityInstance->IsActive());
				TestTrue(TEXT("8.1b: State.Block.Facing granted upon activation"), EnemyASC->HasMatchingGameplayTag(TagBlockFacing));

				// Re-entrancy blocked while reaction is active
				TestFalse(TEXT("8.1c: Re-entry blocked while reaction is active"),
					Scope.AbilityInstance->CanActivateAbility(Scope.Handle, EnemyASC->AbilityActorInfo.Get()));

				// UnPossess Teardown
				Enemy->TriggerTestUnPossessed();

				TestFalse(TEXT("8.1d: Ability cancelled by UnPossessed via TeardownOnUnpossess"), Scope.AbilityInstance->IsActive());
				TestFalse(TEXT("8.1e: State.Block.Facing removed upon UnPossess"), EnemyASC->HasMatchingGameplayTag(TagBlockFacing));
			}
		}

		// 8.2 Explicit cancel cleans up State.Block.Facing
		{
			FTestAbilityFixtureScope Scope2(EnemyASC, Enemy);
			TestNotNull(TEXT("8.2: Scope2 Ability created"), Scope2.AbilityInstance);
			if (Scope2.AbilityInstance)
			{
				Scope2.AbilityInstance->SetTestRootMotionKnockdownMontage(ValidRootMontage);
				Scope2.AbilityInstance->SetTestUseGroundedRootMotionKnockdown(true);
				Scope2.AbilityInstance->SetTestBoundAnimInstance(MockAnimInstance);
				Scope2.AbilityInstance->SetTestBypassMontageActiveCheck(true);

				Scope2.Activate(&DefaultTriggerPayload);

				TestTrue(TEXT("8.2a: Ability active"), Scope2.AbilityInstance->IsActive());
				TestTrue(TEXT("8.2b: State.Block.Facing granted"), EnemyASC->HasMatchingGameplayTag(TagBlockFacing));

				EnemyASC->CancelAbilityHandle(Scope2.Handle);

				TestFalse(TEXT("8.2c: Ability cancelled"), Scope2.AbilityInstance->IsActive());
				TestFalse(TEXT("8.2d: State.Block.Facing removed on cancel"), EnemyASC->HasMatchingGameplayTag(TagBlockFacing));
			}
		}
	}

	if (Attacker)
	{
		Attacker->Destroy();
	}
	Enemy->Destroy();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
