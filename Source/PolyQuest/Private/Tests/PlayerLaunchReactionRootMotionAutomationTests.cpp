#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/Abilities/PlayerLaunchReactionAbility.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Character/Player/PlayerCharacter.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayTagContainer.h"
#include "Tests/CombatAutomationFixture.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlayerLaunchReactionRootMotionAutomationTest,
	"PolyQuest.Combat.PlayerLaunchReactionRootMotion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	struct FPlayerRootMotionTestWorldScopeCleanup
	{
		UWorld* World = nullptr;
		~FPlayerRootMotionTestWorldScopeCleanup()
		{
			if (World)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
		}
	};

	class UTestPlayerLaunchAbilityAccessHelper : public UPlayerLaunchReactionAbility
	{
	public:
		static void CallAbilityActivation(
			UPlayerLaunchReactionAbility* Ability,
			const FGameplayAbilitySpecHandle Handle,
			const FGameplayAbilityActorInfo* ActorInfo,
			const FGameplayAbilityActivationInfo ActivationInfo,
			const FGameplayEventData* TriggerEventData)
		{
			if (Ability)
			{
				static_cast<UTestPlayerLaunchAbilityAccessHelper*>(Ability)->CallActivateAbility(
					Handle, ActorInfo, ActivationInfo, nullptr, TriggerEventData);
			}
		}
	};

	struct FTestPlayerAbilityFixtureScope
	{
		UAbilitySystemComponent* ASC = nullptr;
		FGameplayAbilitySpecHandle Handle;
		UPlayerLaunchReactionAbility* AbilityInstance = nullptr;

		FTestPlayerAbilityFixtureScope(UAbilitySystemComponent* InASC, APlayerCharacter* Player)
			: ASC(InASC)
		{
			if (ASC && Player)
			{
				FGameplayAbilitySpec Spec(UPlayerLaunchReactionAbility::StaticClass(), 1, INDEX_NONE, Player);
				Handle = ASC->GiveAbility(Spec);
				if (FGameplayAbilitySpec* FoundSpec = ASC->FindAbilitySpecFromHandle(Handle))
				{
					AbilityInstance = Cast<UPlayerLaunchReactionAbility>(FoundSpec->GetPrimaryInstance());
				}
			}
		}

		void Activate(const FGameplayEventData* TriggerPayload = nullptr)
		{
			if (AbilityInstance && ASC)
			{
				UTestPlayerLaunchAbilityAccessHelper::CallAbilityActivation(
					AbilityInstance, Handle, ASC->AbilityActorInfo.Get(), FGameplayAbilityActivationInfo(), TriggerPayload);
			}
		}

		~FTestPlayerAbilityFixtureScope()
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

	UAnimMontage* CreateSyntheticKnockdownMontage(
		UObject* Outer = nullptr,
		float Length = 1.5f,
		bool bHasRootMotion = true,
		UAnimSequence** OutSeq = nullptr)
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
		if (OutSeq)
		{
			*OutSeq = Seq;
		}
		return Montage;
	}
}

bool FPlayerLaunchReactionRootMotionAutomationTest::RunTest(const FString& Parameters)
{
	const FGameplayTag TagTeardownOnUnpossess = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.Teardown.OnUnpossess")), false);
	const FGameplayTag TagHitReacting = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.HitReacting")), false);
	const FGameplayTag TagBlockMovement = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Movement")), false);
	const FGameplayTag TagBlockJump = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Jump")), false);
	const FGameplayTag TagCanCancelDodge = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.CanCancel.Dodge")), false);
	const FGameplayTag TagCancelWindowBegin = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.CancelWindow.Dodge.Begin")), false);
	const FGameplayTag TagCancelWindowEnd = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.CancelWindow.Dodge.End")), false);
	const FGameplayTag TagLaunchCommit = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Launch.Commit")), false);

	AddExpectedErrorPlain(TEXT("SequencerDataModel"), EAutomationExpectedErrorFlags::Contains, -1);

	// -------------------------------------------------------------------------
	// SECTION 1: CDO Defaults and Static Interface Verification
	// -------------------------------------------------------------------------
	{
		const UPlayerLaunchReactionAbility* PlayerLaunchCDO = UPlayerLaunchReactionAbility::StaticClass()->GetDefaultObject<UPlayerLaunchReactionAbility>();
		TestNotNull(TEXT("1.1: PlayerLaunch CDO exists"), PlayerLaunchCDO);
		if (PlayerLaunchCDO)
		{
			TestNull(TEXT("1.2: CDO RootMotionKnockdownMontage defaults to nullptr"), PlayerLaunchCDO->GetTestRootMotionKnockdownMontage());
			TestTrue(TEXT("1.3: TagTeardownOnUnpossess is valid"), TagTeardownOnUnpossess.IsValid());
			TestTrue(TEXT("1.4: PlayerLaunch CDO AbilityTags contains Ability.Action.Teardown.OnUnpossess"),
				PlayerLaunchCDO->GetAssetTags().HasTagExact(TagTeardownOnUnpossess));
			TestTrue(TEXT("1.5: PlayerLaunch CDO ActivationOwnedTags contains HitReacting"),
				PlayerLaunchCDO->GetTestActivationOwnedTags().HasTagExact(TagHitReacting));
			TestTrue(TEXT("1.6: PlayerLaunch CDO ActivationOwnedTags contains Block.Movement"),
				PlayerLaunchCDO->GetTestActivationOwnedTags().HasTagExact(TagBlockMovement));
			TestTrue(TEXT("1.7: PlayerLaunch CDO ActivationOwnedTags contains Block.Jump"),
				PlayerLaunchCDO->GetTestActivationOwnedTags().HasTagExact(TagBlockJump));
			TestEqual(TEXT("1.8: PlayerLaunch CDO BlockAbilitiesWithTag has exactly 10 entries"),
				PlayerLaunchCDO->GetTestBlockAbilitiesWithTag().Num(), 10);
			TestEqual(TEXT("1.9: PlayerLaunch CDO AbilitiesToCancel has exactly 11 entries"),
				PlayerLaunchCDO->GetTestAbilitiesToCancel().Num(), 11);
			TestFalse(TEXT("1.10: PlayerLaunch CDO AbilitiesToCancel does NOT contain TeardownOnUnpossess"),
				PlayerLaunchCDO->GetTestAbilitiesToCancel().HasTag(TagTeardownOnUnpossess));
		}
	}

	// Setup Test World
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("PlayerLaunchRootMotionTestWorld"));
	WorldContext.SetCurrentWorld(World);
	if (!TestNotNull(TEXT("Test World created successfully"), World))
	{
		return false;
	}

	FURL WorldURL;
	World->InitializeActorsForPlay(WorldURL);
	FPlayerRootMotionTestWorldScopeCleanup ScopeCleanup{ World };

	APlayerCharacter* Player = FCombatAutomationFixture::SpawnPlayer(World, FTransform(FRotator::ZeroRotator, FVector::ZeroVector));
	if (!TestNotNull(TEXT("Player spawned successfully"), Player))
	{
		return false;
	}

	UAbilitySystemComponent* PlayerASC = Player->GetAbilitySystemComponent();
	UCharacterMovementComponent* MovementComponent = Player->GetCharacterMovement();
	if (!TestNotNull(TEXT("Player ASC valid"), PlayerASC) || !TestNotNull(TEXT("Player MovementComponent valid"), MovementComponent))
	{
		return false;
	}

	MovementComponent->SetMovementMode(MOVE_Walking);

	AEnemyCharacter* Attacker = FCombatAutomationFixture::SpawnPassiveEnemy(World, FTransform(FRotator::ZeroRotator, FVector(200.0f, 0.0f, 0.0f)));
	if (!TestNotNull(TEXT("Attacker spawned successfully"), Attacker))
	{
		return false;
	}

	FGameplayEventData DefaultTriggerPayload;
	DefaultTriggerPayload.Instigator = Attacker;
	DefaultTriggerPayload.Target = Player;

	UAnimInstance* MockAnimInstance = NewObject<UAnimInstance>(Player->GetMesh());
	if (!TestNotNull(TEXT("MockAnimInstance created"), MockAnimInstance))
	{
		return false;
	}

	UPlayerLaunchReactionAbility* PlayerLaunchCDO = UPlayerLaunchReactionAbility::StaticClass()->GetDefaultObject<UPlayerLaunchReactionAbility>();
	UAnimInstance* OriginalCDOBoundAnimInstance = PlayerLaunchCDO ? PlayerLaunchCDO->GetTestBoundAnimInstance() : nullptr;
	if (PlayerLaunchCDO)
	{
		PlayerLaunchCDO->SetTestBoundAnimInstance(MockAnimInstance);
	}

	struct FCDOAnimInstanceGuard
	{
		UPlayerLaunchReactionAbility* CDO = nullptr;
		UAnimInstance* Original = nullptr;
		~FCDOAnimInstanceGuard()
		{
			if (CDO)
			{
				CDO->SetTestBoundAnimInstance(Original);
			}
		}
	} CDOGuard{ PlayerLaunchCDO, OriginalCDOBoundAnimInstance };

	// -------------------------------------------------------------------------
	// SECTION 2: Candidate Matrix & Root-Only Fail-Closed Logic
	// -------------------------------------------------------------------------
	{
		UAnimMontage* ValidRootMontage = CreateSyntheticKnockdownMontage(Player, 1.5f, true);
		UAnimMontage* NonRootMontage = CreateSyntheticKnockdownMontage(Player, 1.5f, false);
		UAnimMontage* EmptySlotMontage = NewObject<UAnimMontage>(GetTransientPackage());
		UAnimMontage* ZeroLengthMontage = CreateSyntheticKnockdownMontage(Player, 0.0f, true);

		FTestPlayerAbilityFixtureScope Scope(PlayerASC, Player);
		TestNotNull(TEXT("2.1: AbilityInstance created"), Scope.AbilityInstance);
		if (Scope.AbilityInstance)
		{
			Scope.AbilityInstance->SetTestBoundAnimInstance(MockAnimInstance);

			// 2.2: Valid candidate passes
			Scope.AbilityInstance->SetTestRootMotionKnockdownMontage(ValidRootMontage);
			MovementComponent->SetMovementMode(MOVE_Walking);
			TestTrue(TEXT("2.2: Valid Root Motion candidate returns true"),
				Scope.AbilityInstance->CallTestIsRootMotionKnockdownCandidate(Player, MovementComponent));

			// 2.3: Nullptr montage returns false
			Scope.AbilityInstance->SetTestRootMotionKnockdownMontage(nullptr);
			TestFalse(TEXT("2.3: Nullptr montage returns false"),
				Scope.AbilityInstance->CallTestIsRootMotionKnockdownCandidate(Player, MovementComponent));

			// 2.4: Non root motion montage returns false
			Scope.AbilityInstance->SetTestRootMotionKnockdownMontage(NonRootMontage);
			TestFalse(TEXT("2.4: Non root motion montage returns false"),
				Scope.AbilityInstance->CallTestIsRootMotionKnockdownCandidate(Player, MovementComponent));

			// 2.5: Empty slot montage returns false
			Scope.AbilityInstance->SetTestRootMotionKnockdownMontage(EmptySlotMontage);
			TestFalse(TEXT("2.5: Empty slot montage returns false"),
				Scope.AbilityInstance->CallTestIsRootMotionKnockdownCandidate(Player, MovementComponent));

			// 2.6: Zero length montage returns false
			Scope.AbilityInstance->SetTestRootMotionKnockdownMontage(ZeroLengthMontage);
			TestFalse(TEXT("2.6: Zero length montage returns false"),
				Scope.AbilityInstance->CallTestIsRootMotionKnockdownCandidate(Player, MovementComponent));

			// 2.7: Non-walking movement mode returns false
			Scope.AbilityInstance->SetTestRootMotionKnockdownMontage(ValidRootMontage);
			MovementComponent->SetMovementMode(MOVE_Falling);
			TestFalse(TEXT("2.7: Falling movement mode returns false for Root Motion candidate"),
				Scope.AbilityInstance->CallTestIsRootMotionKnockdownCandidate(Player, MovementComponent));
			MovementComponent->SetMovementMode(MOVE_Walking);

			// 2.8: Fail-closed if Root Motion montage is null
			Scope.AbilityInstance->SetTestRootMotionKnockdownMontage(nullptr);
			TestFalse(TEXT("2.8: Fail-closed activation setup when root montage is null"),
				Scope.AbilityInstance->CallTestValidateActivationSetup(PlayerASC->AbilityActorInfo.Get()));
		}
	}

	// -------------------------------------------------------------------------
	// SECTION 3: Root Motion Activation, Task Isolation and Commit Ignore
	// -------------------------------------------------------------------------
	{
		UAnimMontage* ValidRootMontage = CreateSyntheticKnockdownMontage(Player, 2.0f, true);

		FTestPlayerAbilityFixtureScope Scope(PlayerASC, Player);
		TestNotNull(TEXT("3.1: AbilityInstance created"), Scope.AbilityInstance);
		if (Scope.AbilityInstance)
		{
			Scope.AbilityInstance->SetTestRootMotionKnockdownMontage(ValidRootMontage);
			Scope.AbilityInstance->SetTestBoundAnimInstance(MockAnimInstance);
			Scope.AbilityInstance->SetTestBypassMontageActiveCheck(true);
			MovementComponent->SetMovementMode(MOVE_Walking);

			Scope.Activate(&DefaultTriggerPayload);

			TestTrue(TEXT("3.2: Entered RootMotionKnockdown phase"),
				Scope.AbilityInstance->IsTestPhaseRootMotionKnockdown());

			TestTrue(TEXT("3.6: CancelBeginTask is active"),
				Scope.AbilityInstance->GetTestCancelBeginTaskActive());
			TestTrue(TEXT("3.7: CancelEndTask is active"),
				Scope.AbilityInstance->GetTestCancelEndTaskActive());

			TestTrue(TEXT("3.8: HitReacting state tag is owned"),
				PlayerASC->HasMatchingGameplayTag(TagHitReacting));
			TestTrue(TEXT("3.9: Block.Movement state tag is owned"),
				PlayerASC->HasMatchingGameplayTag(TagBlockMovement));
			TestTrue(TEXT("3.10: Block.Jump state tag is owned"),
				PlayerASC->HasMatchingGameplayTag(TagBlockJump));

			// Trigger Commit event via real ASC dispatch in Root Motion phase -> should be completely ignored
			FGameplayEventData CommitPayload;
			CommitPayload.Instigator = Player;
			CommitPayload.Target = Player;
			CommitPayload.OptionalObject = ValidRootMontage;
			PlayerASC->HandleGameplayEvent(TagLaunchCommit, &CommitPayload);

			TestTrue(TEXT("3.11: Commit event was ignored, still in RootMotionKnockdown phase"),
				Scope.AbilityInstance->IsTestPhaseRootMotionKnockdown());

			Scope.AbilityInstance->EndAbility(Scope.Handle, PlayerASC->AbilityActorInfo.Get(), FGameplayAbilityActivationInfo(), false, false);
		}
	}

	// -------------------------------------------------------------------------
	// SECTION 4: Attacker Impact Yaw & Velocity Stop & Ledge Protection
	// -------------------------------------------------------------------------
	{
		// 4.1: Yaw resolution unit tests
		float OutYaw = 0.0f;
		TestTrue(TEXT("4.1: TryResolveRootMotionFacingYaw succeeds with normal direction"),
			UPlayerLaunchReactionAbility::CallTestTryResolveRootMotionFacingYaw(FVector(1.0f, 0.0f, 0.0f), 0.0f, OutYaw));
		TestFalse(TEXT("4.2: TryResolveRootMotionFacingYaw fails with zero vector"),
			UPlayerLaunchReactionAbility::CallTestTryResolveRootMotionFacingYaw(FVector::ZeroVector, 0.0f, OutYaw));
		TestFalse(TEXT("4.3: TryResolveRootMotionFacingYaw fails with NaN"),
			UPlayerLaunchReactionAbility::CallTestTryResolveRootMotionFacingYaw(FVector(NAN, 0.0f, 0.0f), 0.0f, OutYaw));
		TestFalse(TEXT("4.4: TryResolveRootMotionFacingYaw fails with Inf"),
			UPlayerLaunchReactionAbility::CallTestTryResolveRootMotionFacingYaw(FVector(INFINITY, 0.0f, 0.0f), 0.0f, OutYaw));

		// 4.5: StopMovementImmediately and Ledge Walk-off protection
		MovementComponent->Velocity = FVector(300.0f, 400.0f, 0.0f);
		MovementComponent->bCanWalkOffLedges = true;

		UAnimMontage* ValidRootMontage = CreateSyntheticKnockdownMontage(Player, 2.0f, true);
		FTestPlayerAbilityFixtureScope Scope(PlayerASC, Player);
		if (Scope.AbilityInstance)
		{
			Scope.AbilityInstance->SetTestRootMotionKnockdownMontage(ValidRootMontage);
			Scope.AbilityInstance->SetTestBoundAnimInstance(MockAnimInstance);
			Scope.AbilityInstance->SetTestBypassMontageActiveCheck(true);
			MovementComponent->SetMovementMode(MOVE_Walking);

			Scope.Activate(&DefaultTriggerPayload);

			TestEqual(TEXT("4.6: Movement velocity zeroed by StopMovementImmediately"),
				MovementComponent->Velocity, FVector::ZeroVector);
			TestFalse(TEXT("4.7: bCanWalkOffLedges disabled during Root Motion"),
				MovementComponent->bCanWalkOffLedges);
			TestTrue(TEXT("4.8: Ledge setting marked as modified"),
				Scope.AbilityInstance->GetTestLedgeSettingModified());

			// End Ability -> verify ledge restored
			Scope.AbilityInstance->EndAbility(Scope.Handle, PlayerASC->AbilityActorInfo.Get(), FGameplayAbilityActivationInfo(), false, false);

			TestTrue(TEXT("4.9: bCanWalkOffLedges accurately restored to true"),
				MovementComponent->bCanWalkOffLedges);
			TestFalse(TEXT("4.10: Ledge setting flag reset"),
				Scope.AbilityInstance->GetTestLedgeSettingModified());
		}

		// 4.11: Initial bCanWalkOffLedges = false restoration
		MovementComponent->bCanWalkOffLedges = false;
		FTestPlayerAbilityFixtureScope ScopeFalseLedge(PlayerASC, Player);
		if (ScopeFalseLedge.AbilityInstance)
		{
			ScopeFalseLedge.AbilityInstance->SetTestRootMotionKnockdownMontage(ValidRootMontage);
			ScopeFalseLedge.AbilityInstance->SetTestBoundAnimInstance(MockAnimInstance);
			ScopeFalseLedge.AbilityInstance->SetTestBypassMontageActiveCheck(true);

			ScopeFalseLedge.Activate(&DefaultTriggerPayload);
			TestFalse(TEXT("4.12: bCanWalkOffLedges is false during ability"), MovementComponent->bCanWalkOffLedges);

			ScopeFalseLedge.AbilityInstance->EndAbility(ScopeFalseLedge.Handle, PlayerASC->AbilityActorInfo.Get(), FGameplayAbilityActivationInfo(), false, false);
			TestFalse(TEXT("4.13: bCanWalkOffLedges accurately restored to false"), MovementComponent->bCanWalkOffLedges);
		}
	}

	// -------------------------------------------------------------------------
	// SECTION 5: Persistent Dodge Cancel Window & Payload Validation
	// -------------------------------------------------------------------------
	{
		UAnimSequence* InnerSeq = nullptr;
		UAnimMontage* ValidRootMontage = CreateSyntheticKnockdownMontage(Player, 2.0f, true, &InnerSeq);
		TestNotNull(TEXT("5.0: Valid InnerSeq exists in synthetic montage"), InnerSeq);

		UAnimMontage* OtherMontage = CreateSyntheticKnockdownMontage(Player, 1.0f, true);

		FTestPlayerAbilityFixtureScope Scope(PlayerASC, Player);
		if (Scope.AbilityInstance)
		{
			Scope.AbilityInstance->SetTestRootMotionKnockdownMontage(ValidRootMontage);
			Scope.AbilityInstance->SetTestBoundAnimInstance(MockAnimInstance);
			Scope.AbilityInstance->SetTestBypassMontageActiveCheck(true);
			Scope.AbilityInstance->SetTestBypassAnimInstanceActiveCheck(true);
			MovementComponent->SetMovementMode(MOVE_Walking);

			Scope.Activate(&DefaultTriggerPayload);
			TestFalse(TEXT("5.1: Initial DodgeCancelable tag is false"),
				PlayerASC->HasMatchingGameplayTag(TagCanCancelDodge));

			// 5.2: Malformed payload (wrong target/instigator) -> Ignored
			FGameplayEventData WrongActorPayload;
			WrongActorPayload.Instigator = Attacker;
			WrongActorPayload.Target = Attacker;
			WrongActorPayload.OptionalObject = ValidRootMontage;
			Scope.AbilityInstance->TestOnCancelWindowBegin(WrongActorPayload);
			TestFalse(TEXT("5.2: Wrong instigator/target payload does NOT grant dodge cancel"),
				PlayerASC->HasMatchingGameplayTag(TagCanCancelDodge));

			// 5.3: Malformed payload (wrong montage) -> Ignored
			FGameplayEventData WrongMontagePayload;
			WrongMontagePayload.Instigator = Player;
			WrongMontagePayload.Target = Player;
			WrongMontagePayload.OptionalObject = OtherMontage;
			Scope.AbilityInstance->TestOnCancelWindowBegin(WrongMontagePayload);
			TestFalse(TEXT("5.3: Wrong montage payload does NOT grant dodge cancel"),
				PlayerASC->HasMatchingGameplayTag(TagCanCancelDodge));

			// 5.4: Correct payload with Montage -> Grants CanCancel.Dodge
			FGameplayEventData CorrectMontagePayload;
			CorrectMontagePayload.Instigator = Player;
			CorrectMontagePayload.Target = Player;
			CorrectMontagePayload.OptionalObject = ValidRootMontage;
			Scope.AbilityInstance->TestOnCancelWindowBegin(CorrectMontagePayload);
			TestTrue(TEXT("5.4: Valid Montage payload grants State.Action.CanCancel.Dodge"),
				PlayerASC->HasMatchingGameplayTag(TagCanCancelDodge));

			// 5.5: Duplicate Begin event -> Idempotent
			Scope.AbilityInstance->TestOnCancelWindowBegin(CorrectMontagePayload);
			TestTrue(TEXT("5.5: Duplicate Begin maintains State.Action.CanCancel.Dodge"),
				PlayerASC->HasMatchingGameplayTag(TagCanCancelDodge));

			// 5.6: End event -> Clears CanCancel.Dodge
			Scope.AbilityInstance->TestOnCancelWindowEnd(CorrectMontagePayload);
			TestFalse(TEXT("5.6: Valid End removes State.Action.CanCancel.Dodge"),
				PlayerASC->HasMatchingGameplayTag(TagCanCancelDodge));

			// 5.7: Persistent listener: Subsequent Begin event with Inner Sequence works!
			FGameplayEventData SequencePayload;
			SequencePayload.Instigator = Player;
			SequencePayload.Target = Player;
			SequencePayload.OptionalObject = InnerSeq;
			Scope.AbilityInstance->TestOnCancelWindowBegin(SequencePayload);
			TestTrue(TEXT("5.7: Persistent listener re-triggers with inner Sequence payload"),
				PlayerASC->HasMatchingGameplayTag(TagCanCancelDodge));

			// 5.8: End event with Sequence -> Clears
			Scope.AbilityInstance->TestOnCancelWindowEnd(SequencePayload);
			TestFalse(TEXT("5.8: End with Sequence removes State.Action.CanCancel.Dodge"),
				PlayerASC->HasMatchingGameplayTag(TagCanCancelDodge));

			// 5.9: End-to-end ASC GameplayEvent dispatch proving WaitGameplayEvent(false, true) persistence
			{
				Scope.AbilityInstance->TestSetDodgeCancelable(false);
				TestFalse(TEXT("5.9a: Initial dodge cancel tag false"),
					PlayerASC->HasMatchingGameplayTag(TagCanCancelDodge));

				// Send malformed event via production ASC dispatch -> rejected by listener
				PlayerASC->HandleGameplayEvent(TagCancelWindowBegin, &WrongActorPayload);
				TestFalse(TEXT("5.9b: Malformed event via ASC ignored (tag remains false)"),
					PlayerASC->HasMatchingGameplayTag(TagCanCancelDodge));

				// Send valid event via production ASC dispatch -> accepted by persistent listener
				PlayerASC->HandleGameplayEvent(TagCancelWindowBegin, &CorrectMontagePayload);
				TestTrue(TEXT("5.9c: Persistent listener re-triggers on subsequent valid event via ASC"),
					PlayerASC->HasMatchingGameplayTag(TagCanCancelDodge));

				// Send valid end event via production ASC dispatch -> tag removed
				PlayerASC->HandleGameplayEvent(TagCancelWindowEnd, &CorrectMontagePayload);
				TestFalse(TEXT("5.9d: Valid End event via ASC clears CanCancel.Dodge tag"),
					PlayerASC->HasMatchingGameplayTag(TagCanCancelDodge));
			}

			// 5.10: EndAbility unreservedly strips tag
			Scope.AbilityInstance->TestOnCancelWindowBegin(CorrectMontagePayload);
			TestTrue(TEXT("5.10a: CanCancel.Dodge present before EndAbility"),
				PlayerASC->HasMatchingGameplayTag(TagCanCancelDodge));
			Scope.AbilityInstance->EndAbility(Scope.Handle, PlayerASC->AbilityActorInfo.Get(), FGameplayAbilityActivationInfo(), false, false);
			TestFalse(TEXT("5.10b: CanCancel.Dodge stripped by EndAbility"),
				PlayerASC->HasMatchingGameplayTag(TagCanCancelDodge));
		}
	}

	// -------------------------------------------------------------------------
	// SECTION 6: MovementMode Changed & Fail-Closed
	// -------------------------------------------------------------------------
	{
		UAnimMontage* ValidRootMontage = CreateSyntheticKnockdownMontage(Player, 2.0f, true);
		FTestPlayerAbilityFixtureScope Scope(PlayerASC, Player);
		if (Scope.AbilityInstance)
		{
			Scope.AbilityInstance->SetTestRootMotionKnockdownMontage(ValidRootMontage);
			Scope.AbilityInstance->SetTestBoundAnimInstance(MockAnimInstance);
			Scope.AbilityInstance->SetTestBypassMontageActiveCheck(true);
			MovementComponent->SetMovementMode(MOVE_Walking);

			Scope.Activate(&DefaultTriggerPayload);
			TestTrue(TEXT("6.1: Active in RootMotionKnockdown"), Scope.AbilityInstance->IsTestPhaseRootMotionKnockdown());

			// Simulate transition from Walking to Falling during root motion slide
			MovementComponent->SetMovementMode(MOVE_Falling);
			Scope.AbilityInstance->TriggerTestMovementModeChanged(Player, MOVE_Walking, 0);

			TestTrue(TEXT("6.2: Transitioning to Falling aborts ability (phase None)"),
				Scope.AbilityInstance->IsTestPhaseNone());
		}
	}

	// -------------------------------------------------------------------------
	// SECTION 7: Montage Interrupted and Natural Completion
	// -------------------------------------------------------------------------
	{
		UAnimMontage* ValidRootMontage = CreateSyntheticKnockdownMontage(Player, 2.0f, true);

		// 7.1: Natural Completion
		{
			FTestPlayerAbilityFixtureScope Scope(PlayerASC, Player);
			if (Scope.AbilityInstance)
			{
				Scope.AbilityInstance->SetTestRootMotionKnockdownMontage(ValidRootMontage);
				Scope.AbilityInstance->SetTestBoundAnimInstance(MockAnimInstance);
				Scope.AbilityInstance->SetTestBypassMontageActiveCheck(true);
				MovementComponent->SetMovementMode(MOVE_Walking);

				Scope.Activate(&DefaultTriggerPayload);
				TestTrue(TEXT("7.1: Active in RootMotionKnockdown"), Scope.AbilityInstance->IsTestPhaseRootMotionKnockdown());

				Scope.AbilityInstance->TriggerTestActiveMontageEnded(ValidRootMontage, false);
				TestTrue(TEXT("7.2: Natural montage completion ends ability"), Scope.AbilityInstance->IsTestPhaseNone());
			}
		}

		// 7.3: Interrupted Completion
		{
			FTestPlayerAbilityFixtureScope Scope(PlayerASC, Player);
			if (Scope.AbilityInstance)
			{
				Scope.AbilityInstance->SetTestRootMotionKnockdownMontage(ValidRootMontage);
				Scope.AbilityInstance->SetTestBoundAnimInstance(MockAnimInstance);
				Scope.AbilityInstance->SetTestBypassMontageActiveCheck(true);
				MovementComponent->SetMovementMode(MOVE_Walking);

				Scope.Activate(&DefaultTriggerPayload);
				TestTrue(TEXT("7.3: Active in RootMotionKnockdown"), Scope.AbilityInstance->IsTestPhaseRootMotionKnockdown());

				Scope.AbilityInstance->TriggerTestActiveMontageEnded(ValidRootMontage, true);
				TestTrue(TEXT("7.4: Interrupted montage ends ability"), Scope.AbilityInstance->IsTestPhaseNone());
			}
		}
	}

	// -------------------------------------------------------------------------
	// SECTION 8: Teardown, UnPossess and Reentrancy
	// -------------------------------------------------------------------------
	{
		UAnimMontage* ValidRootMontage = CreateSyntheticKnockdownMontage(Player, 2.0f, true);

		// 8.1: Player UnPossessed cancels ability via TeardownOnUnpossess
		{
			FTestPlayerAbilityFixtureScope Scope(PlayerASC, Player);
			if (Scope.AbilityInstance)
			{
				Scope.AbilityInstance->SetTestRootMotionKnockdownMontage(ValidRootMontage);
				Scope.AbilityInstance->SetTestBoundAnimInstance(MockAnimInstance);
				Scope.AbilityInstance->SetTestBypassMontageActiveCheck(true);
				MovementComponent->SetMovementMode(MOVE_Walking);

				Scope.Activate(&DefaultTriggerPayload);
				TestTrue(TEXT("8.1: Active in RootMotionKnockdown"), Scope.AbilityInstance->IsTestPhaseRootMotionKnockdown());

				Player->TriggerTestUnPossessed();
				TestTrue(TEXT("8.2: Player UnPossessed triggered teardown and ended ability"),
					Scope.AbilityInstance->IsTestPhaseNone());
			}
		}

		// 8.3: bTestBypassMontageActiveCheck validation
		{
			FTestPlayerAbilityFixtureScope ScopeNoBypass(PlayerASC, Player);
			if (ScopeNoBypass.AbilityInstance)
			{
				ScopeNoBypass.AbilityInstance->SetTestRootMotionKnockdownMontage(ValidRootMontage);
				ScopeNoBypass.AbilityInstance->SetTestBoundAnimInstance(MockAnimInstance);
				ScopeNoBypass.AbilityInstance->SetTestBypassMontageActiveCheck(false);
				MovementComponent->SetMovementMode(MOVE_Walking);

				ScopeNoBypass.Activate(&DefaultTriggerPayload);
				TestTrue(TEXT("8.3: Inactive montage without bypass aborts immediately to phase None"),
					ScopeNoBypass.AbilityInstance->IsTestPhaseNone());
			}
		}

		// 8.4: Real ASC external cancellation (e.g. death / external interrupt)
		{
			FTestPlayerAbilityFixtureScope Scope(PlayerASC, Player);
			if (Scope.AbilityInstance)
			{
				Scope.AbilityInstance->SetTestRootMotionKnockdownMontage(ValidRootMontage);
				Scope.AbilityInstance->SetTestBoundAnimInstance(MockAnimInstance);
				Scope.AbilityInstance->SetTestBypassMontageActiveCheck(true);
				Scope.AbilityInstance->SetTestBypassAnimInstanceActiveCheck(true);
				MovementComponent->SetMovementMode(MOVE_Walking);
				MovementComponent->bCanWalkOffLedges = true;

				Scope.Activate(&DefaultTriggerPayload);
				TestTrue(TEXT("8.4a: Active in RootMotionKnockdown"), Scope.AbilityInstance->IsTestPhaseRootMotionKnockdown());
				TestFalse(TEXT("8.4b: Ledge walking disabled during RootMotion"), MovementComponent->bCanWalkOffLedges);

				// Trigger Dodge Cancel window to grant CanCancel.Dodge
				FGameplayEventData BeginPayload;
				BeginPayload.Instigator = Player;
				BeginPayload.Target = Player;
				BeginPayload.OptionalObject = ValidRootMontage;
				Scope.AbilityInstance->TestOnCancelWindowBegin(BeginPayload);
				TestTrue(TEXT("8.4c: CanCancel.Dodge tag granted"), PlayerASC->HasMatchingGameplayTag(TagCanCancelDodge));

				// External cancellation via ASC (simulating death or external interruption)
				PlayerASC->CancelAbilityHandle(Scope.Handle);

				TestTrue(TEXT("8.4d: Ability phase reset to None after external cancel"),
					Scope.AbilityInstance->IsTestPhaseNone());
				TestFalse(TEXT("8.4e: Ability no longer active"), Scope.AbilityInstance->IsActive());
				TestFalse(TEXT("8.4f: CanCancel.Dodge tag stripped by EndAbility"),
					PlayerASC->HasMatchingGameplayTag(TagCanCancelDodge));
				TestTrue(TEXT("8.4g: bCanWalkOffLedges restored to true"), MovementComponent->bCanWalkOffLedges);
				TestFalse(TEXT("8.4h: CancelBeginTask ended"), Scope.AbilityInstance->GetTestCancelBeginTaskActive());
				TestFalse(TEXT("8.4i: CancelEndTask ended"), Scope.AbilityInstance->GetTestCancelEndTaskActive());
			}
		}

		// 8.5: Destroy / invalid avatar host path (deferred callbacks do not crash or dereference dangling actor)
		{
			APlayerCharacter* TempPlayer = FCombatAutomationFixture::SpawnPlayer(World, FTransform(FRotator::ZeroRotator, FVector(500.0f, 0.0f, 0.0f)));
			if (TestNotNull(TEXT("8.5: TempPlayer spawned"), TempPlayer))
			{
				UAbilitySystemComponent* TempASC = TempPlayer->GetAbilitySystemComponent();
				UCharacterMovementComponent* TempMove = TempPlayer->GetCharacterMovement();
				TempMove->SetMovementMode(MOVE_Walking);
				TempMove->bCanWalkOffLedges = true;

				FTestPlayerAbilityFixtureScope ScopeTemp(TempASC, TempPlayer);
				if (ScopeTemp.AbilityInstance)
				{
					ScopeTemp.AbilityInstance->SetTestRootMotionKnockdownMontage(ValidRootMontage);
					ScopeTemp.AbilityInstance->SetTestBoundAnimInstance(MockAnimInstance);
					ScopeTemp.AbilityInstance->SetTestBypassMontageActiveCheck(true);
					ScopeTemp.AbilityInstance->SetTestBypassAnimInstanceActiveCheck(true);

					ScopeTemp.Activate(&DefaultTriggerPayload);
					TestTrue(TEXT("8.5a: Temp ability active in RootMotionKnockdown"),
						ScopeTemp.AbilityInstance->IsTestPhaseRootMotionKnockdown());

					// Destroy the Player Actor
					TempPlayer->Destroy();

					// Late / deferred callbacks arriving after destruction must not crash or dereference dangling actor
					ScopeTemp.AbilityInstance->TriggerTestMovementModeChanged(nullptr, MOVE_Walking, 0);
					ScopeTemp.AbilityInstance->TriggerTestMovementModeChanged(TempPlayer, MOVE_Falling, 0);
					ScopeTemp.AbilityInstance->TriggerTestActiveMontageEnded(ValidRootMontage, false);

					FGameplayEventData LatePayload;
					LatePayload.Instigator = TempPlayer;
					LatePayload.Target = TempPlayer;
					LatePayload.OptionalObject = ValidRootMontage;
					ScopeTemp.AbilityInstance->TestOnCancelWindowBegin(LatePayload);
					ScopeTemp.AbilityInstance->TestOnCancelWindowEnd(LatePayload);

					// Force cleanup if not already ended
					if (ScopeTemp.AbilityInstance->IsActive())
					{
						ScopeTemp.AbilityInstance->EndAbility(ScopeTemp.Handle, TempASC->AbilityActorInfo.Get(), FGameplayAbilityActivationInfo(), true, false);
					}

					TestTrue(TEXT("8.5b: Destroyed host path cleanly ended without crash"),
						ScopeTemp.AbilityInstance->IsTestPhaseNone());
				}
			}
		}
	}

	return true;
}

#endif
