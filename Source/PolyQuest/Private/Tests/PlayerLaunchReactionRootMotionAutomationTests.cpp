#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/Abilities/PlayerLaunchReactionAbility.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/CharacterAttributeSet.h"
#include "GameplayEffect.h"
#include "UObject/UnrealType.h"
#include "AbilitySystem/Tasks/AbilityTask_PlayActionMontage.h"
#include "Animation/Combat/AnimNotifyState_ActionWindows.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/Skeleton.h"
#include "ReferenceSkeleton.h"
#include "Tests/TestManagedMontageAbility.h"
#include "UObject/StrongObjectPtr.h"
#include "Misc/ScopeExit.h"
#if WITH_EDITOR
#include "Animation/AnimData/IAnimationDataController.h"
#endif
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

	struct FTestPlayerAbilityFixtureScope
	{
		UAbilitySystemComponent* ASC = nullptr;
		FGameplayAbilitySpecHandle Handle;
		UPlayerLaunchReactionAbility* AbilityInstance = nullptr;
		TStrongObjectPtr<UAbilityTask_PlayActionMontage> WindowTask;
		UAnimInstance* WindowAnim = nullptr;
		int32 WindowID = INDEX_NONE;

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

		void Activate(const FGameplayEventData* TriggerPayload = nullptr, bool bFlushStopped = true)
		{
			if (!AbilityInstance || !ASC) return;
			auto* Player = Cast<APlayerCharacter>(ASC->GetAvatarActor());
			if (!Player) return;
			UAnimInstance* Anim = Player->GetMesh()->GetAnimInstance();
			if (!Anim)
			{
				Anim = NewObject<UAnimInstance>(Player->GetMesh());
				Anim->InitializeMontageOnly();
				Player->GetMesh()->AnimScriptInstance = Anim;
			}
			// Flush the preceding stopped fixture before changing its skeleton.
			if (bFlushStopped)
			{
				Anim->TickMontageOnly(0.01f);
				Anim->DispatchQueuedAnimEvents();
			}
			UAnimMontage* Montage = AbilityInstance->GetTestRootMotionKnockdownMontage();
			Anim->CurrentSkeleton = Montage ? Montage->GetSkeleton() : nullptr;
			ASC->RefreshAbilityActorInfo();
			const FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Reaction.Player.Launch"));
			ASC->TriggerAbilityFromGameplayEvent(Handle, ASC->AbilityActorInfo.Get(), EventTag, TriggerPayload, *ASC);
			WindowTask.Reset(AbilityInstance->GetTestMontageTask());
			WindowAnim = Anim;
			WindowID = WindowTask ? WindowTask->GetBoundMontageInstanceID() : INDEX_NONE;
		}

		void PrepareCancel(FGameplayEventData& Payload, bool bBegin) const
		{
			Payload.EventTag = FGameplayTag::RequestGameplayTag(bBegin
				? TEXT("Event.Action.CancelWindow.Dodge.Begin") : TEXT("Event.Action.CancelWindow.Dodge.End"));
			Payload.TargetData = FManagedMontageTestHelpers::MakeCancelWindowTargetData(WindowAnim, WindowID, !bBegin);
			const auto* Animation = Cast<UAnimSequenceBase>(Payload.OptionalObject);
			const FAnimNotifyEvent* Event = Animation ? Animation->Notifies.FindByPredicate([](const FAnimNotifyEvent& Candidate)
			{
				return Cast<UAnimNotifyState_ActionDodgeCancelWindow>(Candidate.NotifyStateClass) != nullptr;
			}) : nullptr;
			Payload.OptionalObject2 = Event ? Event->NotifyStateClass.Get() : nullptr;
		}

		void SendCancel(FGameplayEventData& Payload, bool bBegin) const
		{
			PrepareCancel(Payload, bBegin);
			if (!WindowTask) return;
			if (bBegin) WindowTask->TestInvokeCancelBegin(Payload);
			else WindowTask->TestInvokeCancelEnd(Payload);
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
		UObject* EffectiveOuter = Outer ? Outer : GetTransientPackage();
		UAnimMontage* Montage = NewObject<UAnimMontage>(EffectiveOuter);
		UAnimSequence* Seq = NewObject<UAnimSequence>(EffectiveOuter);
		USkeleton* Skeleton = NewObject<USkeleton>(EffectiveOuter);
		const FName RootBone(TEXT("root"));
		{
			FReferenceSkeletonModifier Modifier(Skeleton);
			Modifier.Add(FMeshBoneInfo(RootBone, TEXT("root"), INDEX_NONE), FTransform::Identity);
		}
		Seq->SetSkeleton(Skeleton);
		Montage->SetSkeleton(Skeleton);
		Seq->bEnableRootMotion = bHasRootMotion;
#if WITH_EDITOR
		IAnimationDataController& Controller = Seq->GetController();
		Controller.InitializeModel();
		{
			IAnimationDataController::FScopedBracket Populate(Controller, FText::FromString(TEXT("Populate Launch fixture")), false);
			const int32 Frames = FMath::Max(1, FMath::RoundToInt(Length * 30.0f));
			Controller.SetFrameRate(FFrameRate(30, 1), false);
			Controller.SetNumberOfFrames(FFrameNumber(Frames), false);
			Controller.AddBoneCurve(RootBone, false);
			TArray<FVector3f> Positions;
			TArray<FQuat4f> Rotations;
			TArray<FVector3f> Scales;
			for (int32 Frame = 0; Frame <= Frames; ++Frame)
				Positions.Add(FVector3f(-100.0f * Frame / Frames, 0.0f, 0.0f));
			Rotations.Init(FQuat4f::Identity, Frames + 1);
			Scales.Init(FVector3f::OneVector, Frames + 1);
			Controller.SetBoneTrackKeys(RootBone, Positions, Rotations, Scales, false);
			Controller.NotifyPopulated();
		}
		Seq->WaitOnExistingCompression();
#endif
		for (UAnimSequenceBase* Animation : TArray<UAnimSequenceBase*>{ Montage, Seq })
		{
			FAnimNotifyEvent Event;
			Event.NotifyStateClass = NewObject<UAnimNotifyState_ActionDodgeCancelWindow>(Animation);
			Animation->Notifies.Add(Event);
		}

		FSlotAnimationTrack Track;
		Track.SlotName = FName(TEXT("DefaultSlot"));
		FAnimSegment Segment;
		Segment.SetAnimReference(Seq);
		Segment.StartPos = 0.0f;
		Segment.AnimStartTime = 0.0f;
		Segment.AnimEndTime = Length;
		Segment.AnimPlayRate = 1.0f;
		Track.AnimTrack.AnimSegments.Add(Segment);
		// UAnimMontage already creates DefaultSlot; replace it with the populated track.
		Montage->SlotAnimTracks.Reset();
		Montage->SlotAnimTracks.Add(Track);

		FCompositeSection Section;
		Section.SectionName = TEXT("Default");
		Section.SetTime(0.0f);
		Montage->CompositeSections.Add(Section);
		Montage->BlendIn.SetBlendTime(0.0f);
		Montage->BlendOut.SetBlendTime(0.0f);
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
	World->BeginPlay();
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

	MockAnimInstance->InitializeMontageOnly();
	Player->GetMesh()->AnimScriptInstance = MockAnimInstance;
	PlayerASC->RefreshAbilityActorInfo();

	// -------------------------------------------------------------------------
	// SECTION 2: Candidate Matrix & Root-Only Fail-Closed Logic
	// -------------------------------------------------------------------------
	{
		UAnimMontage* ValidRootMontage = CreateSyntheticKnockdownMontage(Player, 1.5f, true);
		if (!TestEqual(TEXT("2.0a: Synthetic montage has exactly one slot"), ValidRootMontage->SlotAnimTracks.Num(), 1)
			|| !TestTrue(TEXT("2.0b: DefaultSlot contains the playable animation"), ValidRootMontage->IsValidSlot(TEXT("DefaultSlot")))) return false;
		UAnimMontage* NonRootMontage = CreateSyntheticKnockdownMontage(Player, 1.5f, false);
		UAnimMontage* EmptySlotMontage = NewObject<UAnimMontage>(GetTransientPackage());
		UAnimMontage* ZeroLengthMontage = CreateSyntheticKnockdownMontage(Player, 0.0f, true);

		FTestPlayerAbilityFixtureScope Scope(PlayerASC, Player);
		TestNotNull(TEXT("2.1: AbilityInstance created"), Scope.AbilityInstance);
		if (Scope.AbilityInstance)
		{

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
			MovementComponent->SetMovementMode(MOVE_Walking);

			Scope.Activate(&DefaultTriggerPayload);

			TestTrue(TEXT("3.2: Entered RootMotionKnockdown phase"),
				Scope.AbilityInstance->IsTestPhaseRootMotionKnockdown());

			TestTrue(TEXT("3.6: CancelBeginTask is active"),
				Scope.AbilityInstance->GetTestMontageTask() != nullptr);
			TestTrue(TEXT("3.7: CancelEndTask is active"),
				Scope.AbilityInstance->HasTestRateWindowTasks());

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
			MovementComponent->SetMovementMode(MOVE_Walking);

			Scope.Activate(&DefaultTriggerPayload);
			TestFalse(TEXT("5.1: Initial DodgeCancelable tag is false"),
				PlayerASC->HasMatchingGameplayTag(TagCanCancelDodge));

			// 5.2: Malformed payload (wrong target/instigator) -> Ignored
			FGameplayEventData WrongActorPayload;
			WrongActorPayload.Instigator = Attacker;
			WrongActorPayload.Target = Attacker;
			WrongActorPayload.OptionalObject = ValidRootMontage;
			Scope.SendCancel(WrongActorPayload, true);
			TestFalse(TEXT("5.2: Wrong instigator/target payload does NOT grant dodge cancel"),
				PlayerASC->HasMatchingGameplayTag(TagCanCancelDodge));

			// 5.3: Malformed payload (wrong montage) -> Ignored
			FGameplayEventData WrongMontagePayload;
			WrongMontagePayload.Instigator = Player;
			WrongMontagePayload.Target = Player;
			WrongMontagePayload.OptionalObject = OtherMontage;
			Scope.SendCancel(WrongMontagePayload, true);
			TestFalse(TEXT("5.3: Wrong montage payload does NOT grant dodge cancel"),
				PlayerASC->HasMatchingGameplayTag(TagCanCancelDodge));

			// 5.4: Correct payload with Montage -> Grants CanCancel.Dodge
			FGameplayEventData CorrectMontagePayload;
			CorrectMontagePayload.Instigator = Player;
			CorrectMontagePayload.Target = Player;
			CorrectMontagePayload.OptionalObject = ValidRootMontage;
			Scope.SendCancel(CorrectMontagePayload, true);
			TestTrue(TEXT("5.4: Valid Montage payload grants State.Action.CanCancel.Dodge"),
				PlayerASC->HasMatchingGameplayTag(TagCanCancelDodge));

			// 5.5: Duplicate Begin event -> Idempotent
			Scope.SendCancel(CorrectMontagePayload, true);
			TestTrue(TEXT("5.5: Duplicate Begin maintains State.Action.CanCancel.Dodge"),
				PlayerASC->HasMatchingGameplayTag(TagCanCancelDodge));

			// 5.6: End event -> Clears CanCancel.Dodge
			Scope.SendCancel(CorrectMontagePayload, false);
			TestFalse(TEXT("5.6: Valid End removes State.Action.CanCancel.Dodge"),
				PlayerASC->HasMatchingGameplayTag(TagCanCancelDodge));

			// 5.7: Persistent listener: Subsequent Begin event with Inner Sequence works!
			FGameplayEventData SequencePayload;
			SequencePayload.Instigator = Player;
			SequencePayload.Target = Player;
			SequencePayload.OptionalObject = InnerSeq;
			Scope.SendCancel(SequencePayload, true);
			TestTrue(TEXT("5.7: Persistent listener re-triggers with inner Sequence payload"),
				PlayerASC->HasMatchingGameplayTag(TagCanCancelDodge));

			// 5.8: End event with Sequence -> Clears
			Scope.SendCancel(SequencePayload, false);
			TestFalse(TEXT("5.8: End with Sequence removes State.Action.CanCancel.Dodge"),
				PlayerASC->HasMatchingGameplayTag(TagCanCancelDodge));

			// 5.9: End-to-end ASC GameplayEvent dispatch proving WaitGameplayEvent(false, true) persistence
			{
				TestFalse(TEXT("5.9a: Initial dodge cancel tag false"),
					PlayerASC->HasMatchingGameplayTag(TagCanCancelDodge));

				// Send malformed event via production ASC dispatch -> rejected by listener
				Scope.PrepareCancel(WrongActorPayload, true);
				PlayerASC->HandleGameplayEvent(TagCancelWindowBegin, &WrongActorPayload);
				TestFalse(TEXT("5.9b: Malformed event via ASC ignored (tag remains false)"),
					PlayerASC->HasMatchingGameplayTag(TagCanCancelDodge));

				// Send valid event via production ASC dispatch -> accepted by persistent listener
				Scope.PrepareCancel(CorrectMontagePayload, true);
				PlayerASC->HandleGameplayEvent(TagCancelWindowBegin, &CorrectMontagePayload);
				TestTrue(TEXT("5.9c: Persistent listener re-triggers on subsequent valid event via ASC"),
					PlayerASC->HasMatchingGameplayTag(TagCanCancelDodge));

				// Send valid end event via production ASC dispatch -> tag removed
				Scope.PrepareCancel(CorrectMontagePayload, false);
				PlayerASC->HandleGameplayEvent(TagCancelWindowEnd, &CorrectMontagePayload);
				TestFalse(TEXT("5.9d: Valid End event via ASC clears CanCancel.Dodge tag"),
					PlayerASC->HasMatchingGameplayTag(TagCanCancelDodge));
			}

			// 5.10: EndAbility unreservedly strips tag
			Scope.SendCancel(CorrectMontagePayload, true);
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
				MovementComponent->SetMovementMode(MOVE_Walking);

				Scope.Activate(&DefaultTriggerPayload);
				TestTrue(TEXT("7.1: Active in RootMotionKnockdown"), Scope.AbilityInstance->IsTestPhaseRootMotionKnockdown());

				FAnimMontageInstance* Ending = MockAnimInstance->GetActiveInstanceForMontage(ValidRootMontage);
				if (!TestNotNull(TEXT("7.1a: Natural completion owns actual playback"), Ending)) return false;
				Ending->Stop(FAlphaBlend(0.0f), false);
				MockAnimInstance->TickMontageOnly(0.01f);
				MockAnimInstance->DispatchQueuedAnimEvents();
				TestTrue(TEXT("7.2: Natural montage completion ends ability"), Scope.AbilityInstance->IsTestPhaseNone());
			}
		}

		// 7.3: Interrupted Completion
		{
			FTestPlayerAbilityFixtureScope Scope(PlayerASC, Player);
			if (Scope.AbilityInstance)
			{
				Scope.AbilityInstance->SetTestRootMotionKnockdownMontage(ValidRootMontage);
				MovementComponent->SetMovementMode(MOVE_Walking);

				Scope.Activate(&DefaultTriggerPayload);
				TestTrue(TEXT("7.3: Active in RootMotionKnockdown"), Scope.AbilityInstance->IsTestPhaseRootMotionKnockdown());

				MockAnimInstance->Montage_Stop(0.2f, ValidRootMontage);
				TestTrue(TEXT("7.3a: Launch remains active through interrupted blend"), Scope.AbilityInstance->IsActive());
				for (int32 Step = 0; Step < 6; ++Step)
				{
					MockAnimInstance->TickMontageOnly(0.05f);
					MockAnimInstance->DispatchQueuedAnimEvents();
				}
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
				MovementComponent->SetMovementMode(MOVE_Walking);

				Scope.Activate(&DefaultTriggerPayload);
				TestTrue(TEXT("8.1: Active in RootMotionKnockdown"), Scope.AbilityInstance->IsTestPhaseRootMotionKnockdown());

				Player->TriggerTestUnPossessed();
				TestTrue(TEXT("8.2: Player UnPossessed triggered teardown and ended ability"),
					Scope.AbilityInstance->IsTestPhaseNone());
			}
		}

		// 8.3: Real playback failure without a bypass
		{
			FTestPlayerAbilityFixtureScope ScopeNoBypass(PlayerASC, Player);
			if (ScopeNoBypass.AbilityInstance)
			{
				UAnimMontage* Unplayable = DuplicateObject<UAnimMontage>(ValidRootMontage, Player);
				Unplayable->SetSkeleton(nullptr);
				ScopeNoBypass.AbilityInstance->SetTestRootMotionKnockdownMontage(Unplayable);
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
				Scope.SendCancel(BeginPayload, true);
				TestTrue(TEXT("8.4c: CanCancel.Dodge tag granted"), PlayerASC->HasMatchingGameplayTag(TagCanCancelDodge));

				// External cancellation via ASC (simulating death or external interruption)
				PlayerASC->CancelAbilityHandle(Scope.Handle);

				TestTrue(TEXT("8.4d: Ability phase reset to None after external cancel"),
					Scope.AbilityInstance->IsTestPhaseNone());
				TestFalse(TEXT("8.4e: Ability no longer active"), Scope.AbilityInstance->IsActive());
				TestFalse(TEXT("8.4f: CanCancel.Dodge tag stripped by EndAbility"),
					PlayerASC->HasMatchingGameplayTag(TagCanCancelDodge));
				TestTrue(TEXT("8.4g: bCanWalkOffLedges restored to true"), MovementComponent->bCanWalkOffLedges);
				TestFalse(TEXT("8.4h: CancelBeginTask ended"), Scope.AbilityInstance->GetTestMontageTask() != nullptr);
				TestFalse(TEXT("8.4i: CancelEndTask ended"), Scope.AbilityInstance->HasTestRateWindowTasks());
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
					ScopeTemp.SendCancel(LatePayload, true);
					ScopeTemp.SendCancel(LatePayload, false);

					TestFalse(TEXT("8.5c: Destroy itself ends GAS ability before any fallback"), ScopeTemp.AbilityInstance->IsActive());
					TestTrue(TEXT("8.5d: Old callback Task terminated"), ScopeTemp.WindowTask && ScopeTemp.WindowTask->IsTerminated());
					TestFalse(TEXT("8.5e: Late callbacks cannot restore permission"), TempASC->HasMatchingGameplayTag(TagCanCancelDodge));

					TestTrue(TEXT("8.5b: Destroyed host path cleanly ended without crash"),
						ScopeTemp.AbilityInstance->IsTestPhaseNone());
				}
			}
		}
	}

#if WITH_EDITOR
	// SECTION 9: Standard Task adoption through actual animation advancement.
	{
		APlayerCharacter* RuntimePlayer = FCombatAutomationFixture::SpawnPlayer(World);
		if (!TestNotNull(TEXT("9.0: Runtime player"), RuntimePlayer)) return false;
		auto* RuntimeASC = RuntimePlayer->GetAbilitySystemComponent();
		auto* Mesh = RuntimePlayer->GetMesh();
		auto* Movement = RuntimePlayer->GetCharacterMovement();
		Movement->SetMovementMode(MOVE_Walking);
		RuntimeASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxStaminaAttribute(), 100.0f);
		RuntimeASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 100.0f);
		UAnimSequence* Sequence = nullptr;
		UAnimMontage* Timeline = CreateSyntheticKnockdownMontage(RuntimePlayer, 2.0f, true, &Sequence);
		if (!TestNotNull(TEXT("9.0a: Root track"), Sequence)) return false;
		TestTrue(TEXT("9.0b: Actual root translation exists"),
			!Sequence->ExtractRootMotionFromRange(0.0, 0.5, FAnimExtractContext(0.0, true)).GetTranslation().IsNearlyZero());
		Timeline->Notifies.Reset();
		Sequence->Notifies.Reset();
		Timeline->BlendOut.SetBlendTime(0.2f);
		const auto AddWindow = [&](UAnimNotifyState* Notify, float Start, float Finish)
		{
			FAnimNotifyEvent& Event = Timeline->Notifies.AddDefaulted_GetRef();
			Event.NotifyStateClass = Notify;
			Event.MontageTickType = EMontageNotifyTickType::Queued;
			Event.Link(Timeline, Start);
			Event.SetTime(Start);
			Event.SetDuration(Finish - Start);
			Event.EndLink.Link(Timeline, Finish);
			Event.EndLink.SetTime(Finish);
		};
		auto* RateNotify = NewObject<UAnimNotifyState_MontageRateWindow>(Timeline);
		RateNotify->RateMultiplier = 0.5f;
		AddWindow(RateNotify, 0.1f, 0.2f);
		auto* CancelNotify = NewObject<UAnimNotifyState_ActionDodgeCancelWindow>(Timeline);
		AddWindow(CancelNotify, 0.1f, 0.3f);
		Timeline->SortNotifies();
		FTestPlayerAbilityFixtureScope Scope(RuntimeASC, RuntimePlayer);
		if (!TestNotNull(TEXT("9.0c: Runtime Launch"), Scope.AbilityInstance)) return false;
		Scope.AbilityInstance->SetTestRootMotionKnockdownMontage(Timeline);
		FGameplayEventData Trigger = DefaultTriggerPayload;
		Trigger.Target = RuntimePlayer;
		const auto Activate = [&](bool bFlushStopped = true)
		{
			Movement->SetMovementMode(MOVE_Walking);
			Scope.Activate(&Trigger, bFlushStopped);
			return TestTrue(TEXT("9: Real ASC activation"), Scope.AbilityInstance->IsActive() && Scope.WindowTask);
		};
		if (!Activate()) return false;
		UAnimInstance* Anim = Mesh->GetAnimInstance();
		const bool bMeshTick = Mesh->IsComponentTickEnabled();
		const bool bMovementTick = Movement->IsComponentTickEnabled();
		const bool bAutonomousPose = Mesh->bIsAutonomousTickPose;
		Mesh->SetComponentTickEnabled(false);
		Movement->SetComponentTickEnabled(false);
		ON_SCOPE_EXIT
		{
			Mesh->SetComponentTickEnabled(bMeshTick);
			Movement->SetComponentTickEnabled(bMovementTick);
			Mesh->bIsAutonomousTickPose = bAutonomousPose;
		};
		class FProxyAccess : public UAnimInstance
		{
		public:
			static FAnimInstanceProxy& Get(UAnimInstance* InAnim) { return *GetProxyOnGameThreadStatic<FAnimInstanceProxy>(InAnim); }
		};
		FAnimInstanceProxy& Proxy = FProxyAccess::Get(Anim);
		const FName Slot(TEXT("DefaultSlot"));
		Proxy.RegisterSlotNodeWithAnimInstance(Slot);
		const auto Advance = [&](float Seconds)
		{
			while (Seconds > KINDA_SMALL_NUMBER)
			{
				const float Step = FMath::Min(Seconds, 0.05f);
				FCombatAutomationFixture::TickWorld(World, Step);
				Proxy.UpdateSlotNodeWeight(Slot, 1.0f, 1.0f);
				Proxy.FlipBufferWriteIndex();
				Proxy.UpdateSlotNodeWeight(Slot, 1.0f, 1.0f);
				Proxy.FlipBufferWriteIndex();
				Mesh->bIsAutonomousTickPose = true;
				Anim->TickMontageOnly(Step);
				Anim->DispatchQueuedAnimEvents();
				Mesh->bIsAutonomousTickPose = bAutonomousPose;
				Seconds -= Step;
			}
		};
		const FGameplayTag DefenseTag = FGameplayTag::RequestGameplayTag(TEXT("State.Action.CanCancel.Defense"));
		const FGameplayAbilitySpecHandle DodgeHandle = RuntimeASC->GiveAbility(FGameplayAbilitySpec(UDodgeAbility::StaticClass(), 1, INDEX_NONE, RuntimePlayer));
		const FGameplayAbilitySpecHandle FailedDodgeHandle = RuntimeASC->GiveAbility(FGameplayAbilitySpec(UTestCommitFailingDodgeAbility::StaticClass(), 1, INDEX_NONE, RuntimePlayer));
		for (const FGameplayAbilitySpecHandle Handle : { DodgeHandle, FailedDodgeHandle })
		{
			auto* Ability = RuntimeASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance();
			const auto SetObject = [&](FName Name, UObject* Value)
			{
				auto* Property = FindFProperty<FObjectPropertyBase>(Ability->GetClass(), Name);
				if (!TestNotNull(TEXT("9: Dodge fixture property"), Property)) return false;
				Property->SetObjectPropertyValue_InContainer(Ability, Value);
				return true;
			};
			if (!SetObject(TEXT("DodgeMontage"), Timeline)) return false;
			for (FName Name : { FName(TEXT("CostGameplayEffectClass")), FName(TEXT("StaminaRegenDelayGameplayEffectClass")), FName(TEXT("InvulnerabilityGameplayEffectClass")) })
				if (!SetObject(Name, UGameplayEffect::StaticClass())) return false;
		}
		ON_SCOPE_EXIT
		{
			RuntimeASC->ClearAbility(DodgeHandle);
			RuntimeASC->ClearAbility(FailedDodgeHandle);
		};
		const FGameplayTag RateBeginTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Action.RateWindow.Begin"));
		const FGameplayTag RateEndTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Action.RateWindow.End"));
		TMap<FGameplayTag, int32> Receipts;
		TArray<TPair<FGameplayTag, FDelegateHandle>> ReceiptHandles;
		int32 ExpectedReceiptID = INDEX_NONE;
		for (FGameplayTag Tag : { RateBeginTag, RateEndTag, TagCancelWindowBegin, TagCancelWindowEnd })
		{
			const FDelegateHandle Receipt = RuntimeASC->GenericGameplayEventCallbacks.FindOrAdd(Tag).AddLambda(
				[&, Tag](const FGameplayEventData* Event)
				{
					if (!Event || Event->EventTag != Tag || Event->OptionalObject != Timeline || !Event->TargetData.IsValid(0)) return;
					const UObject* ExpectedNotify = (Tag == RateBeginTag || Tag == RateEndTag)
						? static_cast<UObject*>(RateNotify) : static_cast<UObject*>(CancelNotify);
					if (Event->OptionalObject2 != ExpectedNotify) return;
					const auto* Base = Event->TargetData.Get(0);
					if (Base->GetScriptStruct() != FGameplayAbilityTargetData_MontageRateWindowSource::StaticStruct()) return;
					const auto* Source = static_cast<const FGameplayAbilityTargetData_MontageRateWindowSource*>(Base);
					if (Source->AnimInstance.Get() == Anim && Source->MontageInstanceID == ExpectedReceiptID)
						++Receipts.FindOrAdd(Tag);
				});
			ReceiptHandles.Emplace(Tag, Receipt);
		}
		ON_SCOPE_EXIT
		{
			for (const auto& Receipt : ReceiptHandles)
				RuntimeASC->GenericGameplayEventCallbacks.FindOrAdd(Receipt.Key).Remove(Receipt.Value);
		};
		for (int32 Exit = 0; Exit < 5; ++Exit)
		{
			if (Exit > 0 && !Activate()) return false;
			TStrongObjectPtr<UAbilityTask_PlayActionMontage> Task(Scope.WindowTask.Get());
			const int32 ID = Task->GetBoundMontageInstanceID();
			ExpectedReceiptID = ID;
			Receipts.Reset();
			TestEqual(TEXT("9: DodgeOnly policy"), Task->GetCancelPolicy(), EActionMontageCancelPolicy::DodgeOnly);
			TestFalse(TEXT("9: No Task playback bypass"), Task->GetTestBypassMontageActiveCheck());
			TestEqual(TEXT("9: Playback starts at zero"), Anim->Montage_GetPosition(Timeline), 0.0f);
			TestEqual(TEXT("9: Root Motion scale is one"), RuntimePlayer->GetAnimRootMotionTranslationScale(), 1.0f);
			TestFalse(TEXT("9: Dodge rejected outside window"), RuntimeASC->TryActivateAbility(DodgeHandle));
			TestTrue(TEXT("9: Outside rejection retains Launch"), Scope.AbilityInstance->IsActive());
			Advance(0.15f);
			if (!TestEqual(TEXT("9: Native Rate Begin"), Anim->Montage_GetPlayRate(Timeline), 0.5f)
				|| !TestEqual(TEXT("9: Native Cancel Begin"), Task->GetActiveCancelWindowCount(), 1)) return false;
			TestEqual(TEXT("9: Rate Begin carries actual animation/notify/instance source"), Receipts.FindRef(RateBeginTag), 1);
			TestEqual(TEXT("9: Cancel Begin carries actual animation/notify/instance source"), Receipts.FindRef(TagCancelWindowBegin), 1);
			TestTrue(TEXT("9: Native window grants Dodge"), RuntimeASC->HasMatchingGameplayTag(TagCanCancelDodge));
			TestFalse(TEXT("9: Launch never grants Defense"), RuntimeASC->HasMatchingGameplayTag(DefenseTag));
			if (Exit == 0)
			{
				for (int32 Step = 0; Step < 12 && Anim->Montage_GetPosition(Timeline) < 0.35f; ++Step) Advance(0.05f);
				if (!TestTrue(TEXT("9: Same live Task crossed actual window end"),
					Anim->Montage_GetPosition(Timeline) >= 0.35f && Scope.AbilityInstance->IsActive()
					&& Scope.AbilityInstance->GetTestMontageTask() == Task.Get())) return false;
				TestEqual(TEXT("9: Native Rate End restores baseline"), Anim->Montage_GetPlayRate(Timeline), 1.0f);
				TestEqual(TEXT("9: Native Cancel End clears window"), Task->GetActiveCancelWindowCount(), 0);
				TestFalse(TEXT("9: Native End removes Dodge"), RuntimeASC->HasMatchingGameplayTag(TagCanCancelDodge));
				TestEqual(TEXT("9: Native Rate End source receipt"), Receipts.FindRef(RateEndTag), 1);
				TestEqual(TEXT("9: Native Cancel End source receipt"), Receipts.FindRef(TagCancelWindowEnd), 1);
				Advance(3.0f);
			}
			else if (Exit == 2)
			{
				Anim->Montage_Stop(0.2f, Timeline);
				TestTrue(TEXT("9: Interrupted blend retains Launch"), Scope.AbilityInstance->IsActive());
				Advance(0.25f);
			}
			else if (Exit == 4)
			{
				RuntimeASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 0.0f);
				TestFalse(TEXT("9: CanActivate failure rejects Dodge"), RuntimeASC->TryActivateAbility(DodgeHandle));
				TestTrue(TEXT("9: CanActivate failure retains source"), Scope.AbilityInstance->IsActive());
				RuntimeASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 100.0f);
				RuntimeASC->SetLooseGameplayTagCount(FGameplayTag::RequestGameplayTag(TEXT("State.Status.Exhausted")), 0);
				RuntimeASC->TryActivateAbility(FailedDodgeHandle);
				auto* FailedDodge = CastChecked<UTestCommitFailingDodgeAbility>(RuntimeASC->FindAbilitySpecFromHandle(FailedDodgeHandle)->GetPrimaryInstance());
				TestTrue(TEXT("9: Actual Commit failure reached"), FailedDodge->CommitCheckCallCount > 0);
				TestFalse(TEXT("9: Failed Dodge ended"), FailedDodge->IsActive());
				TestTrue(TEXT("9: Commit failure retains same source Task"), Scope.AbilityInstance->IsActive() && Scope.AbilityInstance->GetTestMontageTask() == Task.Get());
				TestTrue(TEXT("9: Commit failure preserves window"), RuntimeASC->HasMatchingGameplayTag(TagCanCancelDodge));
				TestTrue(TEXT("9: Successful Dodge activates in window"), RuntimeASC->TryActivateAbility(DodgeHandle));
				TestFalse(TEXT("9: Successful Dodge cancels Launch"), Scope.AbilityInstance->IsActive());
				RuntimeASC->CancelAbilityHandle(DodgeHandle);
			}
			else
			{
				RuntimeASC->CancelAbilityHandle(Scope.Handle);
				if (Exit == 3)
				{
					if (!Activate(false)) return false;
					TestNotEqual(TEXT("9: Reactivation owns new instance"), Scope.WindowTask->GetBoundMontageInstanceID(), ID);
					TestTrue(TEXT("9: Reactivation owns new Task"), Scope.WindowTask.Get() != Task.Get());
					Advance(0.15f);
					TestTrue(TEXT("9: Old tail does not end new Launch"), Scope.AbilityInstance->IsActive());
					TestEqual(TEXT("9: New window survives old tail"), Anim->Montage_GetPlayRate(Timeline), 0.5f);
					RuntimeASC->CancelAbilityHandle(Scope.Handle);
				}
			}
			TestFalse(TEXT("9: Exit ends Launch"), Scope.AbilityInstance->IsActive());
			TestTrue(TEXT("9: Exit terminates old Task"), Task->IsTerminated());
			TestFalse(TEXT("9: Exit clears rate binding"), Task->GetRateWindowLifecycle().IsBound());
			TestFalse(TEXT("9: Exit clears Dodge contribution"), RuntimeASC->HasMatchingGameplayTag(TagCanCancelDodge));
			TestFalse(TEXT("9: Exit clears reaction tag"), RuntimeASC->HasMatchingGameplayTag(TagHitReacting));
			TestTrue(TEXT("9: Exit restores ledge setting"), Movement->bCanWalkOffLedges);
			Advance(0.3f);
		}
	}
#endif

	return true;
}

#endif
