#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/EnemyStanceBreakAbility.h"
#include "AbilitySystem/Tasks/AbilityTask_PlayActionMontage.h"
#include "AbilitySystem/Abilities/MontageRateWindowLifecycle.h"
#include "AbilitySystem/Abilities/PlayerFrontExecutionAbility.h"
#include "AbilitySystem/CharacterAttributeSet.h"
#include "Animation/AnimComposite.h"
#include "Animation/AnimMontage.h"
#include "Animation/Combat/AnimNotifyState_ActionWindows.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Character/Player/PlayerCharacter.h"
#include "Combat/Equipment/MeleeWeaponDefinition.h"
#include "Combat/Equipment/WeaponEquipmentComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/PolyQuestPlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "Kismet/GameplayStatics.h"
#include "Tests/CombatAutomationFixture.h"
#include "Tests/TestManagedMontageAbility.h"
#include "Misc/ScopeExit.h"
#include "Tests/TestLaunchFacingSmoothingAbility.h"
#include "Tests/TestProjectileDamageGE.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "ReferenceSkeleton.h"
#include "UObject/Package.h"
#include <limits>

#if WITH_EDITOR
#include "Animation/AnimData/IAnimationDataController.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEnemyStanceBreakRateWindowAutomationTest,
	"PolyQuest.Combat.EnemyStanceBreakRateWindow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
#if WITH_EDITOR // These playable fixtures require the animation data controller.
	struct FTestMontageLengthAccess : public UAnimMontage
	{
		static void SetLength(UAnimMontage* Montage, float Length)
		{
			if (Montage)
			{
				static_cast<FTestMontageLengthAccess*>(Montage)->SequenceLength = Length;
			}
		}
	};

	UAnimMontage* CreatePlayableStanceMontage(FAutomationTestBase& Test, UObject* Outer)
	{
		USkeleton* Skeleton = NewObject<USkeleton>(Outer);
		const FName RootBoneName(TEXT("root"));
		{
			FReferenceSkeletonModifier Modifier(Skeleton);
			Modifier.Add(FMeshBoneInfo(RootBoneName, TEXT("root"), INDEX_NONE), FTransform::Identity);
		}

		UAnimSequence* Sequence = NewObject<UAnimSequence>(Outer);
		Sequence->SetSkeleton(Skeleton);
		IAnimationDataController& Controller = Sequence->GetController();
		Controller.InitializeModel();
		bool bPopulated = false;
		{
			IAnimationDataController::FScopedBracket Populate(Controller,
				FText::FromString(TEXT("Populate StanceBreak RateWindow fixture")), false);
			Controller.SetFrameRate(FFrameRate(30, 1), false);
			Controller.SetNumberOfFrames(FFrameNumber(60), false);
			const bool bTrackAdded = Controller.AddBoneCurve(RootBoneName, false);
			TArray<FVector3f> Positions;
			TArray<FQuat4f> Rotations;
			TArray<FVector3f> Scales;
			Positions.Init(FVector3f::ZeroVector, 61);
			Rotations.Init(FQuat4f::Identity, 61);
			Scales.Init(FVector3f::OneVector, 61);
			bPopulated = bTrackAdded && Controller.SetBoneTrackKeys(RootBoneName, Positions, Rotations, Scales, false);
			Controller.NotifyPopulated();
		}
		Sequence->WaitOnExistingCompression();
		if (!Test.TestTrue(TEXT("Runtime: synthetic animation has a populated root track"), bPopulated))
		{
			return nullptr;
		}

		UAnimMontage* Montage = NewObject<UAnimMontage>(Outer);
		Montage->SetSkeleton(Skeleton);
		FSlotAnimationTrack Track;
		Track.SlotName = FName(TEXT("DefaultSlot"));
		FAnimSegment Segment;
		Segment.SetAnimReference(Sequence);
		Segment.AnimEndTime = Sequence->GetPlayLength();
		Track.AnimTrack.AnimSegments.Add(Segment);
		Montage->SlotAnimTracks.Reset();
		Montage->SlotAnimTracks.Add(Track);
		FCompositeSection Section;
		Section.SectionName = FName(TEXT("Default"));
		Section.SetTime(0.0f);
		Montage->CompositeSections.Add(Section);
		FTestMontageLengthAccess::SetLength(Montage, Sequence->GetPlayLength());
		Montage->BlendIn.SetBlendTime(0.0f);
		Montage->BlendOut.SetBlendTime(0.0f);

		for (int32 WindowIndex = 0; WindowIndex < 2; ++WindowIndex)
		{
			const FName WindowName(WindowIndex == 0 ? TEXT("StanceRateWindowA") : TEXT("StanceRateWindowB"));
			UAnimNotifyState_MontageRateWindow* Notify = NewObject<UAnimNotifyState_MontageRateWindow>(Montage, WindowName);
			Notify->RateMultiplier = WindowIndex == 0 ? 0.5f : 0.2f;
			FAnimNotifyEvent Event;
			Event.NotifyStateClass = Notify;
			Montage->Notifies.Add(Event);
		}
		return Montage;
	}

	FAnimNotifyEvent* FindStanceRateWindowEvent(UAnimMontage* Montage, int32 WindowIndex)
	{
		const FName WindowName(WindowIndex == 0 ? TEXT("StanceRateWindowA") : TEXT("StanceRateWindowB"));
		return Montage->Notifies.FindByPredicate([WindowName](const FAnimNotifyEvent& Event)
		{
			return Event.NotifyStateClass && Event.NotifyStateClass->GetFName() == WindowName;
		});
	}

#endif
	struct FEnemyStanceBreakRateWindowTestWorldScope
	{
		UWorld* World = nullptr;
		~FEnemyStanceBreakRateWindowTestWorldScope()
		{
			if (World)
			{
				UGameplayStatics::SetGlobalTimeDilation(World, 1.0f);
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
		}
	};
}

bool FEnemyStanceBreakRateWindowAutomationTest::RunTest(const FString& Parameters)
{
	// =========================================================================
	// 1. CDO & Tag Contract Verification
	// =========================================================================
	const UEnemyStanceBreakAbility* StanceBreakCDO = UEnemyStanceBreakAbility::StaticClass()->GetDefaultObject<UEnemyStanceBreakAbility>();
	if (!TestNotNull(TEXT("UEnemyStanceBreakAbility CDO exists"), StanceBreakCDO))
	{
		return false;
	}

	TestEqual(TEXT("InstancingPolicy is InstancedPerActor"),
		StanceBreakCDO->GetInstancingPolicy(),
		EGameplayAbilityInstancingPolicy::InstancedPerActor);

	TestEqual(TEXT("NetExecutionPolicy is ServerOnly"),
		StanceBreakCDO->GetNetExecutionPolicy(),
		EGameplayAbilityNetExecutionPolicy::ServerOnly);

	const FGameplayTag TagStanceBreakAbility = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Reaction.Enemy.StanceBreak")), false);
	const FGameplayTag TagTeardownOnUnpossess = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.Teardown.OnUnpossess")), false);
	const FGameplayTag TagStunned = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
	const FGameplayTag TagBlockFacing = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Block.Facing")), false);
	const FGameplayTag TagDead = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
	const FGameplayTag TagRateWindowBegin = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.RateWindow.Begin")), false);
	const FGameplayTag TagRateWindowEnd = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.RateWindow.End")), false);

	TestTrue(TEXT("Tag Ability.Reaction.Enemy.StanceBreak is valid"), TagStanceBreakAbility.IsValid());
	TestTrue(TEXT("Tag Ability.Action.Teardown.OnUnpossess is valid"), TagTeardownOnUnpossess.IsValid());
	TestTrue(TEXT("Tag State.Status.Stunned is valid"), TagStunned.IsValid());
	TestTrue(TEXT("Tag State.Block.Facing is valid"), TagBlockFacing.IsValid());
	TestTrue(TEXT("Tag Event.Action.RateWindow.Begin is valid"), TagRateWindowBegin.IsValid());
	TestTrue(TEXT("Tag Event.Action.RateWindow.End is valid"), TagRateWindowEnd.IsValid());

	TestTrue(TEXT("CDO AbilityTags has Ability.Reaction.Enemy.StanceBreak"),
		StanceBreakCDO->GetTestAbilityTags().HasTagExact(TagStanceBreakAbility));
	TestTrue(TEXT("CDO AbilityTags has Ability.Action.Teardown.OnUnpossess"),
		StanceBreakCDO->GetTestAbilityTags().HasTagExact(TagTeardownOnUnpossess));

	TestTrue(TEXT("CDO ActivationOwnedTags has State.Status.Stunned"),
		StanceBreakCDO->GetTestActivationOwnedTags().HasTagExact(TagStunned));
	TestTrue(TEXT("CDO ActivationOwnedTags has State.Block.Facing"),
		StanceBreakCDO->GetTestActivationOwnedTags().HasTagExact(TagBlockFacing));
	TestTrue(TEXT("CDO ActivationBlockedTags has State.Status.Dead"),
		StanceBreakCDO->GetTestActivationBlockedTags().HasTagExact(TagDead));
	TestTrue(TEXT("CDO ActivationBlockedTags has State.Status.Stunned"),
		StanceBreakCDO->GetTestActivationBlockedTags().HasTagExact(TagStunned));

	const FGameplayTagContainer& CancelList = StanceBreakCDO->GetAbilitiesToCancel();
	TestEqual(TEXT("AbilitiesToCancel contains exactly 4 entries"), CancelList.Num(), 4);
	TestTrue(TEXT("CancelList has Enemy.Melee"), CancelList.HasTagExact(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Enemy.Melee")), false)));
	TestTrue(TEXT("CancelList has Enemy.Big"), CancelList.HasTagExact(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Reaction.Enemy.Big")), false)));
	TestTrue(TEXT("CancelList has Enemy.Small"), CancelList.HasTagExact(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Reaction.Enemy.Small")), false)));
	TestTrue(TEXT("CancelList has Enemy.Launch"), CancelList.HasTagExact(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Reaction.Enemy.Launch")), false)));

	// =========================================================================
	// 2. Lifecycle Helper Unit Tests (Synthetic PlayRate / LIFO Stack / Edge Cases)
	// =========================================================================
	{
		UAnimMontage* DummyMontage = NewObject<UAnimMontage>(GetTransientPackage(), TEXT("Test_DummyRateMontage"));
		UAnimComposite* DummySequence = NewObject<UAnimComposite>(GetTransientPackage(), TEXT("Test_DummyRateSequence"));

		// Setup SlotAnimTrack for sequence match testing
		FSlotAnimationTrack SlotTrack;
		SlotTrack.SlotName = FName(TEXT("DefaultSlot"));
		FAnimSegment AnimSegment;
		AnimSegment.SetAnimReference(DummySequence);
		AnimSegment.StartPos = 0.0f;
		AnimSegment.AnimStartTime = 0.0f;
		AnimSegment.AnimEndTime = 1.0f;
		AnimSegment.AnimPlayRate = 1.0f;
		SlotTrack.AnimTrack.AnimSegments.Add(AnimSegment);
		DummyMontage->SlotAnimTracks.Add(SlotTrack);

		FAbilityMontageRateWindowLifecycle Helper;
		TestFalse(TEXT("Helper initially unbound"), Helper.IsBound());
		TestEqual(TEXT("Initial stack depth is 0"), Helper.GetStackDepth(), 0);
		TestEqual(TEXT("Initial baseline is 1.0"), Helper.GetBaselinePlayRate(), 1.0f);

		// Sequence matching
		Helper.SetTestActiveContext(nullptr, nullptr, DummyMontage, TagRateWindowBegin, TagRateWindowEnd, 1.0f);
		TestTrue(TEXT("Helper matches direct Montage"), Helper.IsMontageOrSequenceMatch(DummyMontage));
		TestTrue(TEXT("Helper matches source Sequence inside Montage"), Helper.IsMontageOrSequenceMatch(DummySequence));

		UAnimComposite* UnrelatedSequence = NewObject<UAnimComposite>(GetTransientPackage(), TEXT("Test_UnrelatedRateSequence"));
		TestFalse(TEXT("Helper rejects unrelated Sequence"), Helper.IsMontageOrSequenceMatch(UnrelatedSequence));
		TestFalse(TEXT("Helper rejects null object"), Helper.IsMontageOrSequenceMatch(nullptr));

		Helper.SetTestCapturedBaseline(std::numeric_limits<float>::quiet_NaN());
		TestEqual(TEXT("Non-finite test baseline falls back to 1.0"), Helper.GetBaselinePlayRate(), 1.0f);
		Helper.SetTestActiveContext(nullptr, nullptr, DummyMontage, TagRateWindowBegin, TagRateWindowEnd, 1.75f);

		UAnimNotifyState_MontageRateWindow* DummyNotify1 = NewObject<UAnimNotifyState_MontageRateWindow>(GetTransientPackage(), TEXT("Test_DummyRateNotify1"));
		DummyNotify1->RateMultiplier = 0.5f;
		FAnimNotifyEvent Event1;
		Event1.NotifyStateClass = DummyNotify1;
		DummyMontage->Notifies.Add(Event1);

		UAnimNotifyState_MontageRateWindow* DummyNotify2 = NewObject<UAnimNotifyState_MontageRateWindow>(GetTransientPackage(), TEXT("Test_DummyRateNotify2"));
		DummyNotify2->RateMultiplier = 0.25f;
		FAnimNotifyEvent Event2;
		Event2.NotifyStateClass = DummyNotify2;
		DummySequence->Notifies.Add(Event2);

		float AppliedRate = -1.0f;
		TestTrue(TEXT("First synthetic Begin is accepted"), Helper.TestApplyBegin(DummyMontage, DummyNotify1, 0.5f, AppliedRate));
		TestEqual(TEXT("First Begin applies authored rate"), AppliedRate, 0.5f);
		TestEqual(TEXT("First Begin pushes one rate"), Helper.GetActiveWindowCount(), 1);
		TestTrue(TEXT("Nested synthetic Begin is accepted"), Helper.TestApplyBegin(DummySequence, DummyNotify2, 0.25f, AppliedRate));
		TestEqual(TEXT("Nested Begin applies authored rate"), AppliedRate, 0.25f);
		TestEqual(TEXT("Nested Begin pushes two rates"), Helper.GetActiveWindowCount(), 2);

		float RestoredRate = -1.0f;
		TestTrue(TEXT("First synthetic End restores outer still-active predecessor"), Helper.TestApplyEnd(DummySequence, DummyNotify2, RestoredRate));
		TestEqual(TEXT("Active winner restores 0.5"), RestoredRate, 0.5f);
		TestTrue(TEXT("Second synthetic End restores baseline predecessor"), Helper.TestApplyEnd(DummyMontage, DummyNotify1, RestoredRate));
		TestEqual(TEXT("All ended restores 1.75 baseline"), RestoredRate, 1.75f);
		RestoredRate = 9.0f;
		TestFalse(TEXT("Extra synthetic End is ignored"), Helper.TestApplyEnd(DummyMontage, DummyNotify1, RestoredRate));
		TestEqual(TEXT("Extra End leaves output unchanged"), RestoredRate, 9.0f);
		TestEqual(TEXT("Active windows empty after matched Ends"), Helper.GetActiveWindowCount(), 0);

		UAnimNotifyState_MontageRateWindow* UnregisteredNotify = NewObject<UAnimNotifyState_MontageRateWindow>(GetTransientPackage(), TEXT("Test_UnregisteredRateNotify"));
		TestFalse(TEXT("Unregistered notify is rejected"), Helper.TestApplyBegin(DummyMontage, UnregisteredNotify, 0.5f, AppliedRate));
		TestFalse(TEXT("Non-finite rate is rejected"), Helper.TestApplyBegin(DummyMontage, DummyNotify1, std::numeric_limits<float>::quiet_NaN(), AppliedRate));
		TestFalse(TEXT("Non-positive new rate is rejected"), Helper.TestApplyBegin(DummyMontage, DummyNotify1, 0.0f, AppliedRate));

		// Clear helper
		Helper.RestoreAndClear();
		TestFalse(TEXT("Helper unbound after RestoreAndClear"), Helper.IsBound());
		TestEqual(TEXT("Stack depth is 0 after RestoreAndClear"), Helper.GetStackDepth(), 0);
	}

	// =========================================================================
	// 3. World Integration Tests
	// =========================================================================
	if (!TestNotNull(TEXT("GEngine is valid"), GEngine))
	{
		return false;
	}

	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("EnemyStanceBreakRateWindowTestWorld"));
	WorldContext.SetCurrentWorld(World);
	FEnemyStanceBreakRateWindowTestWorldScope ScopeCleanup{ World };

	if (!TestNotNull(TEXT("Test World created"), World))
	{
		return false;
	}

	FURL WorldURL;
	World->InitializeActorsForPlay(WorldURL);
	World->BeginPlay();

	AEnemyCharacter* Enemy = FCombatAutomationFixture::SpawnPassiveEnemy(
		World,
		FTransform(FRotator::ZeroRotator, FVector(100.0f, 0.0f, 0.0f)));
	APlayerCharacter* Player = FCombatAutomationFixture::SpawnPlayer(
		World,
		FTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, 0.0f)));
	APolyQuestPlayerController* PlayerController = World->SpawnActor<APolyQuestPlayerController>();

	if (!TestNotNull(TEXT("Enemy spawned"), Enemy)
		|| !TestNotNull(TEXT("Player spawned"), Player)
		|| !TestNotNull(TEXT("PlayerController spawned"), PlayerController))
	{
		return false;
	}

	PlayerController->Possess(Player);
	UAbilitySystemComponent* EnemyASC = Enemy->GetAbilitySystemComponent();
	UAbilitySystemComponent* PlayerASC = Player->GetAbilitySystemComponent();

	if (!TestNotNull(TEXT("Enemy ASC valid"), EnemyASC) || !TestNotNull(TEXT("Player ASC valid"), PlayerASC))
	{
		return false;
	}

	// 3.1 Verify CanActivateAbility Fail-Closed when Poise is NOT broken
	{
		FGameplayAbilitySpec StanceBreakSpec(UEnemyStanceBreakAbility::StaticClass(), 1, INDEX_NONE, Enemy);
		const FGameplayAbilitySpecHandle Handle = EnemyASC->GiveAbility(StanceBreakSpec);
		FGameplayAbilitySpec* FoundSpec = EnemyASC->FindAbilitySpecFromHandle(Handle);
		TestNotNull(TEXT("StanceBreakSpec given to Enemy"), FoundSpec);

		const bool bCanActivateUnbroken = EnemyASC->TryActivateAbility(Handle);
		TestFalse(TEXT("Cannot activate StanceBreak when Poise is full/unbroken"), bCanActivateUnbroken);
		TestFalse(TEXT("Enemy does not have Stunned tag"), EnemyASC->HasMatchingGameplayTag(TagStunned));
		TestFalse(TEXT("StanceBreak marker remains hidden when activation fails"),
			Enemy->GetTestStanceBreakMarkerWidgetComponent()
			&& Enemy->GetTestStanceBreakMarkerWidgetComponent()->GetVisibleFlag());

		EnemyASC->ClearAbility(Handle);
	}

#if WITH_EDITOR
	// 3.2 Real GAS Production Lifecycle & RateWindow Event Integration
	{
		// 1. Prepare StanceBreak conditions on Enemy:
		// Break Poise so IsPoiseBroken() is true
		EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetPoiseAttribute(), 0.0f);
		TestTrue(TEXT("Enemy Poise is broken"), Enemy->IsPoiseBroken());
		TestTrue(TEXT("Enemy has valid PoiseRecovery configuration"), Enemy->HasValidPoiseRecoveryConfiguration());

		// Play the same in-memory fixture as the dual-instance cases, with an inner sequence window.
		UAnimMontage* ActiveStanceMontage = CreatePlayableStanceMontage(*this, World);
		if (!TestNotNull(TEXT("Stance payload matrix montage is playable"), ActiveStanceMontage))
		{
			return false;
		}
		UAnimSequenceBase* ActiveInnerSequence = ActiveStanceMontage->SlotAnimTracks[0].AnimTrack.AnimSegments[0].GetAnimReference();
		ActiveStanceMontage->Notifies.Reset();

		UAnimNotifyState_MontageRateWindow* StanceNotify1 = NewObject<UAnimNotifyState_MontageRateWindow>(World, TEXT("Test_StanceRateNotify1"));
		StanceNotify1->RateMultiplier = 0.33f;
		FAnimNotifyEvent StanceEvent1;
		StanceEvent1.NotifyStateClass = StanceNotify1;
		ActiveStanceMontage->Notifies.Add(StanceEvent1);

		UAnimNotifyState_MontageRateWindow* StanceNotify2 = NewObject<UAnimNotifyState_MontageRateWindow>(World, TEXT("Test_StanceRateNotify2"));
		StanceNotify2->RateMultiplier = 0.1f;
		FAnimNotifyEvent StanceEvent2;
		StanceEvent2.NotifyStateClass = StanceNotify2;
		ActiveInnerSequence->Notifies.Add(StanceEvent2);

		UAnimInstance* MockAnimInstance = NewObject<UAnimInstance>(Enemy->GetMesh());
		UAnimInstance* PreviousAnim = Enemy->GetMesh()->GetAnimInstance();
		ON_SCOPE_EXIT { Enemy->GetMesh()->AnimScriptInstance = PreviousAnim; EnemyASC->RefreshAbilityActorInfo(); };
		MockAnimInstance->InitializeMontageOnly();
		MockAnimInstance->CurrentSkeleton = ActiveStanceMontage->GetSkeleton();
		Enemy->GetMesh()->AnimScriptInstance = MockAnimInstance;
		EnemyASC->RefreshAbilityActorInfo();

		// Configure the granted instance; leave the process-global CDO untouched.
		FGameplayAbilitySpec StanceBreakSpec(UEnemyStanceBreakAbility::StaticClass(), 1, INDEX_NONE, Enemy);
		const FGameplayAbilitySpecHandle StanceBreakHandle = EnemyASC->GiveAbility(StanceBreakSpec);
		FGameplayAbilitySpec* FoundSpec = EnemyASC->FindAbilitySpecFromHandle(StanceBreakHandle);
		if (!TestNotNull(TEXT("StanceBreak spec exists on Enemy ASC"), FoundSpec)) { return false; }
		const UEnemyStanceBreakAbility* StanceCDO = CastChecked<UEnemyStanceBreakAbility>(FoundSpec->Ability);
		UAnimMontage* OriginalStanceBreakMontage = StanceCDO->GetTestStanceBreakMontage();
		UAnimInstance* OriginalBoundAnimInstance = StanceCDO->GetTestBoundAnimInstance();
		const bool bOriginalBypassMontageActiveCheck = StanceCDO->GetTestBypassMontageActiveCheck();
		UEnemyStanceBreakAbility* StanceBreakAbility = Cast<UEnemyStanceBreakAbility>(FoundSpec->GetPrimaryInstance());
		if (!TestNotNull(TEXT("StanceBreak granted instance exists"), StanceBreakAbility)) { return false; }
		StanceBreakAbility->SetTestStanceBreakMontage(ActiveStanceMontage);
		TestTrue(TEXT("UEnemyStanceBreakAbility successfully activated via GAS TryActivateAbility"), EnemyASC->TryActivateAbility(StanceBreakHandle));

		if (TestNotNull(TEXT("Active UEnemyStanceBreakAbility instance exists"), StanceBreakAbility))
		{
			TestTrue(TEXT("StanceBreak marker is visible while real StanceBreak is active"),
				Enemy->GetTestStanceBreakMarkerWidgetComponent()
				&& Enemy->GetTestStanceBreakMarkerWidgetComponent()->GetVisibleFlag());
			// 4. Verify production path components are live and active
			TestTrue(TEXT("Ability is active"), StanceBreakAbility->IsActive());
			TestTrue(TEXT("Enemy ASC has Stunned tag"), EnemyASC->HasMatchingGameplayTag(TagStunned));
			TestTrue(TEXT("Enemy ASC has State.Block.Facing tag during Stance Break"), EnemyASC->HasMatchingGameplayTag(TagBlockFacing));
			TestTrue(TEXT("Movement is locked by StanceBreak"), StanceBreakAbility->IsMovementLockedByStanceBreak());
			TestNotNull(TEXT("ActiveContext exists"), StanceBreakAbility->GetTestActiveContext());
			TestNotNull(TEXT("MontageTask exists"), StanceBreakAbility->GetMontageTask());
			UAbilityTask_PlayActionMontage* WindowTask = StanceBreakAbility->GetMontageTask();
			if (!TestNotNull(TEXT("RateWindow owner Task exists"), WindowTask)) { return false; }
			TestTrue(TEXT("RateWindow owner Task is active"), WindowTask->IsActive());
			TestEqual(TEXT("StanceBreak cancellation policy remains None"), WindowTask->GetCancelPolicy(), EActionMontageCancelPolicy::None);
			// WaitTask existence/activity maps to the Task's actual Begin/End subscriptions.
			TestTrue(TEXT("RateWindow Begin subscribed"), EnemyASC->GenericGameplayEventCallbacks.FindChecked(TagRateWindowBegin).IsBoundToObject(WindowTask));
			TestTrue(TEXT("RateWindow End subscribed"), EnemyASC->GenericGameplayEventCallbacks.FindChecked(TagRateWindowEnd).IsBoundToObject(WindowTask));
			const FGameplayAbilityTargetDataHandle SourceReceipt = FManagedMontageTestHelpers::MakeRateWindowTargetData(MockAnimInstance, WindowTask->GetBoundMontageInstanceID());
			TestTrue(TEXT("RateWindowLifecycle is bound"), WindowTask->GetRateWindowLifecycle().IsBound());
			TestEqual(TEXT("Initial RateStack depth is 0"), WindowTask->GetRateWindowLifecycle().GetStackDepth(), 0);

			// 5. Fail-Closed Payload Rejection Tests:
			// 5.1 Wrong Instigator / Target
			{
				FGameplayEventData BadActorPayload;
				BadActorPayload.EventTag = TagRateWindowBegin;
				BadActorPayload.Instigator = Player;
				BadActorPayload.Target = Enemy;
				BadActorPayload.OptionalObject = ActiveStanceMontage;
				BadActorPayload.OptionalObject2 = StanceNotify1;
				BadActorPayload.EventMagnitude = 0.5f;
				BadActorPayload.TargetData = SourceReceipt;
				EnemyASC->HandleGameplayEvent(TagRateWindowBegin, &BadActorPayload);
				TestEqual(TEXT("Bad Instigator rejected (stack depth unchanged)"), WindowTask->GetRateWindowLifecycle().GetActiveWindowCount(), 0);
			}

			// 5.2 Wrong Event Tag -> standard Task subscription does not trigger
			{
				const FGameplayTag TagUnrelated = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Enemy.Big")), false);
				FGameplayEventData BadTagPayload;
				BadTagPayload.EventTag = TagUnrelated;
				BadTagPayload.Instigator = Enemy;
				BadTagPayload.Target = Enemy;
				BadTagPayload.OptionalObject = ActiveStanceMontage;
				BadTagPayload.OptionalObject2 = StanceNotify1;
				BadTagPayload.EventMagnitude = 0.5f;
				BadTagPayload.TargetData = SourceReceipt;
				EnemyASC->HandleGameplayEvent(TagUnrelated, &BadTagPayload);
				TestEqual(TEXT("Unrelated EventTag ignored by Task (stack depth unchanged)"), WindowTask->GetRateWindowLifecycle().GetActiveWindowCount(), 0);
			}

			// 5.3 Unrelated Montage / Sequence
			{
				UAnimMontage* ForeignMontage = NewObject<UAnimMontage>(GetTransientPackage(), TEXT("Test_ForeignMontage"));
				FGameplayEventData BadMontagePayload;
				BadMontagePayload.EventTag = TagRateWindowBegin;
				BadMontagePayload.Instigator = Enemy;
				BadMontagePayload.Target = Enemy;
				BadMontagePayload.OptionalObject = ForeignMontage;
				BadMontagePayload.OptionalObject2 = StanceNotify1;
				BadMontagePayload.EventMagnitude = 0.5f;
				BadMontagePayload.TargetData = SourceReceipt;
				EnemyASC->HandleGameplayEvent(TagRateWindowBegin, &BadMontagePayload);
				TestEqual(TEXT("Foreign Montage rejected (stack depth unchanged)"), WindowTask->GetRateWindowLifecycle().GetActiveWindowCount(), 0);
			}

			// 5.4 Non-positive / non-finite magnitude
			{
				FGameplayEventData BadMagPayload;
				BadMagPayload.EventTag = TagRateWindowBegin;
				BadMagPayload.Instigator = Enemy;
				BadMagPayload.Target = Enemy;
				BadMagPayload.OptionalObject = ActiveStanceMontage;
				BadMagPayload.OptionalObject2 = StanceNotify1;
				BadMagPayload.EventMagnitude = 0.0f;
				BadMagPayload.TargetData = SourceReceipt;
				EnemyASC->HandleGameplayEvent(TagRateWindowBegin, &BadMagPayload);
				TestEqual(TEXT("Zero magnitude rejected (stack depth unchanged)"), WindowTask->GetRateWindowLifecycle().GetActiveWindowCount(), 0);

				BadMagPayload.EventMagnitude = -0.5f;
				BadMagPayload.TargetData = SourceReceipt;
				EnemyASC->HandleGameplayEvent(TagRateWindowBegin, &BadMagPayload);
				TestEqual(TEXT("Negative magnitude rejected (stack depth unchanged)"), WindowTask->GetRateWindowLifecycle().GetActiveWindowCount(), 0);
			}

			// 5.5 Missing or Unregistered Notify Identity
			{
				FGameplayEventData MissingIdentityPayload;
				MissingIdentityPayload.EventTag = TagRateWindowBegin;
				MissingIdentityPayload.Instigator = Enemy;
				MissingIdentityPayload.Target = Enemy;
				MissingIdentityPayload.OptionalObject = ActiveStanceMontage;
				MissingIdentityPayload.OptionalObject2 = nullptr;
				MissingIdentityPayload.EventMagnitude = 0.5f;
				MissingIdentityPayload.TargetData = SourceReceipt;
				EnemyASC->HandleGameplayEvent(TagRateWindowBegin, &MissingIdentityPayload);
				TestEqual(TEXT("Missing OptionalObject2 identity rejected (stack depth unchanged)"), WindowTask->GetRateWindowLifecycle().GetActiveWindowCount(), 0);

				UAnimNotifyState_MontageRateWindow* UnregisteredNotify = NewObject<UAnimNotifyState_MontageRateWindow>(World, TEXT("Test_UnregisteredStanceNotify"));
				FGameplayEventData UnregisteredIdentityPayload = MissingIdentityPayload;
				UnregisteredIdentityPayload.OptionalObject2 = UnregisteredNotify;
				UnregisteredIdentityPayload.TargetData = SourceReceipt;
				EnemyASC->HandleGameplayEvent(TagRateWindowBegin, &UnregisteredIdentityPayload);
				TestEqual(TEXT("Unregistered OptionalObject2 identity rejected (stack depth unchanged)"), WindowTask->GetRateWindowLifecycle().GetActiveWindowCount(), 0);
			}

			// 6. Valid Begin & Nested Rate Window Integration:
			// 6.1 First Valid Begin (0.33f Rate)
			{
				FGameplayEventData ValidBeginPayload;
				ValidBeginPayload.EventTag = TagRateWindowBegin;
				ValidBeginPayload.Instigator = Enemy;
				ValidBeginPayload.Target = Enemy;
				ValidBeginPayload.OptionalObject = ActiveStanceMontage;
				ValidBeginPayload.OptionalObject2 = StanceNotify1;
				ValidBeginPayload.EventMagnitude = 0.33f;
				ValidBeginPayload.TargetData = SourceReceipt;
				EnemyASC->HandleGameplayEvent(TagRateWindowBegin, &ValidBeginPayload);
				TestEqual(TEXT("Valid Begin pushes rate (stack depth 1)"), WindowTask->GetRateWindowLifecycle().GetActiveWindowCount(), 1);
			}

			// 6.2 Nested Valid Begin using Inner Sequence reference (0.1f Rate)
			{
				FGameplayEventData NestedBeginPayload;
				NestedBeginPayload.EventTag = TagRateWindowBegin;
				NestedBeginPayload.Instigator = Enemy;
				NestedBeginPayload.Target = Enemy;
				NestedBeginPayload.OptionalObject = ActiveInnerSequence;
				NestedBeginPayload.OptionalObject2 = StanceNotify2;
				NestedBeginPayload.EventMagnitude = 0.1f;
				NestedBeginPayload.TargetData = SourceReceipt;
				EnemyASC->HandleGameplayEvent(TagRateWindowBegin, &NestedBeginPayload);
				TestEqual(TEXT("Nested Begin on inner sequence pushes rate (stack depth 2)"), WindowTask->GetRateWindowLifecycle().GetActiveWindowCount(), 2);
			}

			// 6.3 First End Event (Pops nested rate, restores 0.33f)
			{
				FGameplayEventData EndPayload;
				EndPayload.EventTag = TagRateWindowEnd;
				EndPayload.Instigator = Enemy;
				EndPayload.Target = Enemy;
				EndPayload.OptionalObject = ActiveInnerSequence;
				EndPayload.OptionalObject2 = StanceNotify2;
				EndPayload.TargetData = SourceReceipt;
				EnemyASC->HandleGameplayEvent(TagRateWindowEnd, &EndPayload);
				TestEqual(TEXT("First End pops rate (stack depth 1)"), WindowTask->GetRateWindowLifecycle().GetActiveWindowCount(), 1);
			}

			// 6.4 Second End Event (Pops to baseline)
			{
				FGameplayEventData EndPayload;
				EndPayload.EventTag = TagRateWindowEnd;
				EndPayload.Instigator = Enemy;
				EndPayload.Target = Enemy;
				EndPayload.OptionalObject = ActiveStanceMontage;
				EndPayload.OptionalObject2 = StanceNotify1;
				EndPayload.TargetData = SourceReceipt;
				EnemyASC->HandleGameplayEvent(TagRateWindowEnd, &EndPayload);
				TestEqual(TEXT("Second End restores baseline (stack depth 0)"), WindowTask->GetRateWindowLifecycle().GetActiveWindowCount(), 0);
			}

			// 7. UnPossess Teardown & Complete EndAbility Cleanup Verification
			{
				// Trigger UnPossessed on Enemy
				Enemy->TriggerTestUnPossessed();

				// Verify ability was cancelled by UnPossessed via Teardown.OnUnpossess
				TestFalse(TEXT("StanceBreak ability cancelled on UnPossessed"), StanceBreakAbility->IsActive());
				TestFalse(TEXT("RateWindowLifecycle unbound on EndAbility"), WindowTask->GetRateWindowLifecycle().IsBound());
				TestNull(TEXT("ActiveContext invalidated"), StanceBreakAbility->GetTestActiveContext());
				TestFalse(TEXT("RateWindow Begin subscription removed"), EnemyASC->GenericGameplayEventCallbacks.FindChecked(TagRateWindowBegin).IsBoundToObject(WindowTask));
				TestFalse(TEXT("RateWindow End subscription removed"), EnemyASC->GenericGameplayEventCallbacks.FindChecked(TagRateWindowEnd).IsBoundToObject(WindowTask));
				TestNull(TEXT("MontageTask cleaned up"), StanceBreakAbility->GetMontageTask());
				TestFalse(TEXT("Movement lock released"), StanceBreakAbility->IsMovementLockedByStanceBreak());
				TestFalse(TEXT("RateWindow test bypass reset after EndAbility"), WindowTask->GetRateWindowLifecycle().GetTestBypassMontageActiveCheck());
				TestFalse(TEXT("Stunned tag removed from Enemy ASC"), EnemyASC->HasMatchingGameplayTag(TagStunned));
				TestFalse(TEXT("StanceBreak marker hidden after UnPossess teardown"),
					Enemy->GetTestStanceBreakMarkerWidgetComponent()
					&& Enemy->GetTestStanceBreakMarkerWidgetComponent()->GetVisibleFlag());
				TestFalse(TEXT("State.Block.Facing tag removed from Enemy ASC after UnPossessed"), EnemyASC->HasMatchingGameplayTag(TagBlockFacing));
				TestTrue(TEXT("Poise restored to MaxPoise"), EnemyASC->GetNumericAttribute(UCharacterAttributeSet::GetPoiseAttribute()) >= EnemyASC->GetNumericAttribute(UCharacterAttributeSet::GetMaxPoiseAttribute()));
			}

			// 8. InstancedPerActor Re-entry Verification
			{
				EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetPoiseAttribute(), 0.0f);
				const bool bReactivated = EnemyASC->TryActivateAbility(StanceBreakHandle);

				TestTrue(TEXT("StanceBreak ability successfully re-activated on same instance"), bReactivated);
				TestTrue(TEXT("StanceBreak marker is visible after re-activation"),
					Enemy->GetTestStanceBreakMarkerWidgetComponent()
					&& Enemy->GetTestStanceBreakMarkerWidgetComponent()->GetVisibleFlag());
				TestTrue(TEXT("FacingBlock tag present on re-activation"), EnemyASC->HasMatchingGameplayTag(TagBlockFacing));

				EnemyASC->CancelAbilityHandle(StanceBreakHandle);
				TestFalse(TEXT("StanceBreak ability cancelled on manual CancelAbility"), StanceBreakAbility->IsActive());
				TestTrue(TEXT("StanceBreak marker remains visible during manual cancellation fade"),
					Enemy->GetTestStanceBreakMarkerWidgetComponent()
					&& Enemy->GetTestStanceBreakMarkerWidgetComponent()->GetVisibleFlag());
				TestTrue(TEXT("Manual cancellation starts marker fade timer"), Enemy->HasPendingStanceBreakMarkerFade());
				Enemy->TriggerTestCompleteStanceBreakMarkerFade();
				TestFalse(TEXT("StanceBreak marker hidden after manual cancellation fade"),
					Enemy->GetTestStanceBreakMarkerWidgetComponent()
					&& Enemy->GetTestStanceBreakMarkerWidgetComponent()->GetVisibleFlag());
				TestFalse(TEXT("Manual cancellation clears marker fade timer"), Enemy->HasPendingStanceBreakMarkerFade());
				TestFalse(TEXT("FacingBlock tag cleanly removed upon cancellation"), EnemyASC->HasMatchingGameplayTag(TagBlockFacing));
			}
		}
		if (StanceCDO)
		{
			TestTrue(TEXT("StanceBreak CDO montage restored after fixture"), StanceCDO->GetTestStanceBreakMontage() == OriginalStanceBreakMontage);
			TestTrue(TEXT("StanceBreak CDO bound AnimInstance restored after fixture"), StanceCDO->GetTestBoundAnimInstance() == OriginalBoundAnimInstance);
			TestTrue(TEXT("StanceBreak CDO bypass restored after fixture"), StanceCDO->GetTestBypassMontageActiveCheck() == bOriginalBypassMontageActiveCheck);
		}

		EnemyASC->ClearAbility(StanceBreakHandle);
	}

#endif
	// 3.3 Front Execution Availability Verification during Stunned
	{
		// Manually add Stunned tag to Enemy
		EnemyASC->AddLooseGameplayTag(TagStunned);
		TestTrue(TEXT("Enemy has Stunned tag"), EnemyASC->HasMatchingGameplayTag(TagStunned));

		UPlayerFrontExecutionAbility* ExecAbility = NewObject<UPlayerFrontExecutionAbility>(Player);
		if (TestNotNull(TEXT("PlayerFrontExecutionAbility created"), ExecAbility))
		{
			FGameplayAbilityActorInfo ActorInfo;
			ActorInfo.InitFromActor(Player, Player, PlayerASC);

			// Check Front Execution CDO / state gate
			TestTrue(TEXT("Enemy Stunned tag is recognized as valid target state"), EnemyASC->HasMatchingGameplayTag(TagStunned));
		}

		EnemyASC->RemoveLooseGameplayTag(TagStunned);
		TestFalse(TEXT("Enemy Stunned tag removed"), EnemyASC->HasMatchingGameplayTag(TagStunned));
	}

	// =========================================================================
	// 3.4 StanceBreak Synthetic Recovery Logic Verification (Movement & Poise Recovery)
	// =========================================================================
#if WITH_EDITOR
	{
		const FGameplayTag TagVictimLocked = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Execution.VictimLocked")), false);
		TestTrue(TEXT("Tag State.Action.Execution.VictimLocked is valid"), TagVictimLocked.IsValid());

		// Part A: Normal Recovery (StanceBreak restores Poise and MovementMode without VictimLocked)
		{
			EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetPoiseAttribute(), 0.0f);
			TestTrue(TEXT("Synthetic A: Enemy Poise is broken"), Enemy->IsPoiseBroken());

			UAnimMontage* MockMontage = CreatePlayableStanceMontage(*this, World);
			if (!TestNotNull(TEXT("Recovery montage is playable"), MockMontage)) { return false; }
			UAnimInstance* MockAnim = NewObject<UAnimInstance>(Enemy->GetMesh());
			UAnimInstance* PreviousAnim = Enemy->GetMesh()->GetAnimInstance();
			ON_SCOPE_EXIT { Enemy->GetMesh()->AnimScriptInstance = PreviousAnim; EnemyASC->RefreshAbilityActorInfo(); };
			MockAnim->InitializeMontageOnly();
			MockAnim->CurrentSkeleton = MockMontage->GetSkeleton();
			Enemy->GetMesh()->AnimScriptInstance = MockAnim;
			EnemyASC->RefreshAbilityActorInfo();

			FGameplayAbilitySpec Spec(UEnemyStanceBreakAbility::StaticClass(), 1, INDEX_NONE, Enemy);
			const FGameplayAbilitySpecHandle Handle = EnemyASC->GiveAbility(Spec);
			FGameplayAbilitySpec* Found = EnemyASC->FindAbilitySpecFromHandle(Handle);

			UEnemyStanceBreakAbility* Ability = nullptr;
			if (Found)
			{
				Ability = Cast<UEnemyStanceBreakAbility>(Found->GetPrimaryInstance());
				if (!TestNotNull(TEXT("Granted stance instance exists"), Ability)) { return false; }
				Ability->SetTestStanceBreakMontage(MockMontage);
				const bool bActivated = EnemyASC->TryActivateAbility(Handle);

				TestTrue(TEXT("Synthetic A: Ability activated via TryActivateAbility"), bActivated);
				Ability = Cast<UEnemyStanceBreakAbility>(Found->GetPrimaryInstance());
			}

			if (TestNotNull(TEXT("Synthetic A: StanceBreak ability instance exists"), Ability))
			{
				TestTrue(TEXT("Synthetic A: Ability is active"), Ability->IsActive());
				TestTrue(TEXT("Synthetic A: Movement locked"), Ability->IsMovementLockedByStanceBreak());

				// Cancel ability normally (no VictimLocked)
				EnemyASC->CancelAbilityHandle(Handle);
				TestFalse(TEXT("Synthetic A: Ability ended"), Ability->IsActive());
				TestFalse(TEXT("Synthetic A: Movement lock released"), Ability->IsMovementLockedByStanceBreak());
				TestTrue(TEXT("Synthetic A: Poise restored to max on normal recovery"),
					EnemyASC->GetNumericAttribute(UCharacterAttributeSet::GetPoiseAttribute()) >= EnemyASC->GetNumericAttribute(UCharacterAttributeSet::GetMaxPoiseAttribute()));
				if (UCharacterMovementComponent* CMC = Enemy->GetCharacterMovement())
				{
					TestEqual(TEXT("Synthetic A: Movement mode restored to Walking"), CMC->MovementMode.GetValue(), MOVE_Walking);
				}
				TestFalse(TEXT("Synthetic A: Stunned tag removed"), EnemyASC->HasMatchingGameplayTag(TagStunned));
			}

			EnemyASC->ClearAbility(Handle);
		}

		// Part B: VictimLocked Handoff (StanceBreak preserves Poise and MovementMode for VictimExecution ownership)
		{
			EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetPoiseAttribute(), 0.0f);
			TestTrue(TEXT("Synthetic B: Enemy Poise is broken"), Enemy->IsPoiseBroken());

			UAnimMontage* MockMontage = CreatePlayableStanceMontage(*this, World);
			if (!TestNotNull(TEXT("Recovery montage is playable"), MockMontage)) { return false; }
			UAnimInstance* MockAnim = NewObject<UAnimInstance>(Enemy->GetMesh());
			UAnimInstance* PreviousAnim = Enemy->GetMesh()->GetAnimInstance();
			ON_SCOPE_EXIT { Enemy->GetMesh()->AnimScriptInstance = PreviousAnim; EnemyASC->RefreshAbilityActorInfo(); };
			MockAnim->InitializeMontageOnly();
			MockAnim->CurrentSkeleton = MockMontage->GetSkeleton();
			Enemy->GetMesh()->AnimScriptInstance = MockAnim;
			EnemyASC->RefreshAbilityActorInfo();

			FGameplayAbilitySpec Spec(UEnemyStanceBreakAbility::StaticClass(), 1, INDEX_NONE, Enemy);
			const FGameplayAbilitySpecHandle Handle = EnemyASC->GiveAbility(Spec);
			FGameplayAbilitySpec* Found = EnemyASC->FindAbilitySpecFromHandle(Handle);

			UEnemyStanceBreakAbility* Ability = nullptr;
			if (Found)
			{
				Ability = Cast<UEnemyStanceBreakAbility>(Found->GetPrimaryInstance());
				if (!TestNotNull(TEXT("Granted stance instance exists"), Ability)) { return false; }
				Ability->SetTestStanceBreakMontage(MockMontage);
				const bool bActivated = EnemyASC->TryActivateAbility(Handle);

				TestTrue(TEXT("Synthetic B: Ability activated via TryActivateAbility"), bActivated);
				Ability = Cast<UEnemyStanceBreakAbility>(Found->GetPrimaryInstance());
			}

			if (TestNotNull(TEXT("Synthetic B: StanceBreak ability instance exists"), Ability))
			{
				TestTrue(TEXT("Synthetic B: Ability is active"), Ability->IsActive());
				TestTrue(TEXT("Synthetic B: Movement locked by StanceBreak"), Ability->IsMovementLockedByStanceBreak());

				// Simulate execution handoff: add VictimLocked tag to EnemyASC
				EnemyASC->AddLooseGameplayTag(TagVictimLocked);
				TestTrue(TEXT("Synthetic B: VictimLocked tag added"), EnemyASC->HasMatchingGameplayTag(TagVictimLocked));

				// End StanceBreak ability while VictimLocked is active
				EnemyASC->CancelAbilityHandle(Handle);
				TestFalse(TEXT("Synthetic B: StanceBreak ability ended"), Ability->IsActive());

				// Contract: When VictimLocked is active, StanceBreak must NOT restore Poise and must NOT restore Walking movement
				TestEqual(TEXT("Synthetic B: Poise remains 0 (handed off to VictimExecution)"),
					EnemyASC->GetNumericAttribute(UCharacterAttributeSet::GetPoiseAttribute()), 0.0f);
				if (UCharacterMovementComponent* CMC = Enemy->GetCharacterMovement())
				{
					TestNotEqual(TEXT("Synthetic B: Movement mode not restored to Walking (handed off)"), CMC->MovementMode.GetValue(), MOVE_Walking);
				}

				// Cleanup VictimLocked tag and restore Enemy state for test hygiene
				EnemyASC->RemoveLooseGameplayTag(TagVictimLocked);
				TestFalse(TEXT("Synthetic B: VictimLocked tag removed"), EnemyASC->HasMatchingGameplayTag(TagVictimLocked));
				Enemy->RestorePoiseToMax();
				if (UCharacterMovementComponent* CMC = Enemy->GetCharacterMovement())
				{
					CMC->SetMovementMode(MOVE_Walking);
				}
			}

			EnemyASC->ClearAbility(Handle);
		}
	}

	// =========================================================================
	// 3.5 StanceBreak Real Dual-Instance Non-Bypass Authorization & Recovery Integration
	// =========================================================================
	{
		const FGameplayTag TagVictimLocked = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Execution.VictimLocked")), false);
		TestTrue(TEXT("Tag State.Action.Execution.VictimLocked is valid"), TagVictimLocked.IsValid());

		// Subtest A: Real Dual-Instance Rejection on Active StanceBreak Ability & Normal Recovery
		{
			EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetPoiseAttribute(), 0.0f);
			TestTrue(TEXT("StanceBreak Real Dual-Instance A: Enemy Poise is broken"), Enemy->IsPoiseBroken());

			UAnimMontage* PlayableStanceMontage = CreatePlayableStanceMontage(*this, World);
			if (TestNotNull(TEXT("StanceBreak Real Dual-Instance A: Playable Montage created"), PlayableStanceMontage))
			{
				USkeletalMeshComponent* Mesh = Enemy->GetMesh();
				Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered;
				UAnimInstance* RealAnim = NewObject<UAnimInstance>(Mesh);
				RealAnim->InitializeMontageOnly();
				RealAnim->CurrentSkeleton = PlayableStanceMontage->GetSkeleton();
				Mesh->AnimScriptInstance = RealAnim;
				EnemyASC->RefreshAbilityActorInfo();

				FGameplayAbilitySpec Spec(UEnemyStanceBreakAbility::StaticClass(), 1, INDEX_NONE, Enemy);
				const FGameplayAbilitySpecHandle Handle = EnemyASC->GiveAbility(Spec);
				FGameplayAbilitySpec* Found = EnemyASC->FindAbilitySpecFromHandle(Handle);

				UEnemyStanceBreakAbility* Ability = nullptr;
				if (Found)
				{
					Ability = Cast<UEnemyStanceBreakAbility>(Found->GetPrimaryInstance());
					if (!TestNotNull(TEXT("Granted stance instance exists"), Ability)) { return false; }
					Ability->SetTestStanceBreakMontage(PlayableStanceMontage);
					const bool bActivated = EnemyASC->TryActivateAbility(Handle);

					TestTrue(TEXT("StanceBreak Real Dual-Instance A: Ability activated without bypass"), bActivated);
					Ability = Cast<UEnemyStanceBreakAbility>(Found->GetPrimaryInstance());
				}

				if (TestNotNull(TEXT("StanceBreak Real Dual-Instance A: Active instance exists"), Ability))
				{
					TestTrue(TEXT("StanceBreak Real Dual-Instance A: Ability is active"), Ability->IsActive());
					UAbilityTask_PlayActionMontage* WindowTask = Ability->GetMontageTask();
					if (!TestNotNull(TEXT("StanceBreak Real Dual-Instance A: standard Task exists"), WindowTask)) { return false; }
					TestTrue(TEXT("StanceBreak Real Dual-Instance A: Movement locked"), Ability->IsMovementLockedByStanceBreak());
					TestFalse(TEXT("StanceBreak Real Dual-Instance A: Ability bypass is false"), Ability->GetTestBypassMontageActiveCheck());
					TestFalse(TEXT("StanceBreak Real Dual-Instance A: Helper bypass is false"), WindowTask->GetRateWindowLifecycle().GetTestBypassMontageActiveCheck());
					TestTrue(TEXT("StanceBreak Real Dual-Instance A: Helper is bound to real instance"), WindowTask->GetRateWindowLifecycle().IsBound());

					const int32 OldInstanceID = WindowTask->GetRateWindowLifecycle().GetBoundMontageInstanceID();
					TestNotEqual(TEXT("StanceBreak Real Dual-Instance A: Captured InstanceID is valid"), OldInstanceID, static_cast<int32>(INDEX_NONE));

					// Enter Window A on old instance
					FAnimNotifyEvent* EventA = FindStanceRateWindowEvent(PlayableStanceMontage, 0);
					FAnimNotifyEvent* EventB = FindStanceRateWindowEvent(PlayableStanceMontage, 1);
					TestNotNull(TEXT("StanceBreak Real Dual-Instance A: EventA exists"), EventA);
					TestNotNull(TEXT("StanceBreak Real Dual-Instance A: EventB exists"), EventB);

					FGameplayEventData WindowAPayload;
					WindowAPayload.EventTag = TagRateWindowBegin;
					WindowAPayload.Instigator = Enemy;
					WindowAPayload.Target = Enemy;
					WindowAPayload.OptionalObject = PlayableStanceMontage;
					WindowAPayload.OptionalObject2 = EventA ? EventA->NotifyStateClass : nullptr;
					WindowAPayload.EventMagnitude = 0.5f;

					WindowAPayload.TargetData = FManagedMontageTestHelpers::MakeRateWindowTargetData(RealAnim, OldInstanceID);

					EnemyASC->HandleGameplayEvent(TagRateWindowBegin, &WindowAPayload);
					TestEqual(TEXT("StanceBreak Real Dual-Instance A: Window A sets window count to 1"), WindowTask->GetRateWindowLifecycle().GetActiveWindowCount(), 1);
					TestEqual(TEXT("StanceBreak Real Dual-Instance A: Instance 1 rate updated to 0.5f"), RealAnim->Montage_GetPlayRate(PlayableStanceMontage), 0.5f);

					// Replay same Montage at rate 2.0f without stopping existing instance (bStopAllMontages = false)
					const float ReplayLen = RealAnim->Montage_Play(PlayableStanceMontage, 2.0f, EMontagePlayReturnType::MontageLength, 0.0f, false);
					TestTrue(TEXT("StanceBreak Real Dual-Instance A: Replay succeeded"), ReplayLen > 0.0f);

					const FAnimMontageInstance* NewActiveInstance = RealAnim->GetActiveInstanceForMontage(PlayableStanceMontage);
					const FAnimMontageInstance* OldLiveInstance = RealAnim->GetMontageInstanceForID(OldInstanceID);
					TestNotNull(TEXT("StanceBreak Real Dual-Instance A: New active instance exists"), NewActiveInstance);
					TestNotNull(TEXT("StanceBreak Real Dual-Instance A: Old live instance exists"), OldLiveInstance);
					TestFalse(TEXT("StanceBreak Real Dual-Instance A: Old instance is not stopped"), OldLiveInstance && OldLiveInstance->IsStopped());
					TestNotEqual(TEXT("StanceBreak Real Dual-Instance A: New instance ID differs from old ID"),
						NewActiveInstance ? NewActiveInstance->GetInstanceID() : INDEX_NONE, OldInstanceID);
					TestEqual(TEXT("StanceBreak Real Dual-Instance A: New instance play rate is 2.0"), RealAnim->Montage_GetPlayRate(PlayableStanceMontage), 2.0f);

					// Negative 1: Old Begin (Window B) with complete notify identity dispatched to ASC
					FGameplayEventData OldBeginPayload;
					OldBeginPayload.EventTag = TagRateWindowBegin;
					OldBeginPayload.Instigator = Enemy;
					OldBeginPayload.Target = Enemy;
					OldBeginPayload.OptionalObject = PlayableStanceMontage;
					OldBeginPayload.OptionalObject2 = EventB ? EventB->NotifyStateClass : nullptr;
					OldBeginPayload.EventMagnitude = 0.2f;

					OldBeginPayload.TargetData = FManagedMontageTestHelpers::MakeRateWindowTargetData(RealAnim, OldInstanceID);

					EnemyASC->HandleGameplayEvent(TagRateWindowBegin, &OldBeginPayload);
					TestEqual(TEXT("StanceBreak Real Dual-Instance A: Old Begin rejected - new instance rate remains 2.0"), RealAnim->Montage_GetPlayRate(PlayableStanceMontage), 2.0f);
					TestEqual(TEXT("StanceBreak Real Dual-Instance A: Old Begin rejected - window count remains 1"), WindowTask->GetRateWindowLifecycle().GetActiveWindowCount(), 1);

					// Negative 2: Old End (Window A) with complete notify identity dispatched to ASC
					FGameplayEventData OldEndPayload;
					OldEndPayload.EventTag = TagRateWindowEnd;
					OldEndPayload.Instigator = Enemy;
					OldEndPayload.Target = Enemy;
					OldEndPayload.OptionalObject = PlayableStanceMontage;
					OldEndPayload.OptionalObject2 = EventA ? EventA->NotifyStateClass : nullptr;

					OldEndPayload.TargetData = FManagedMontageTestHelpers::MakeRateWindowTargetData(RealAnim, OldInstanceID);

					EnemyASC->HandleGameplayEvent(TagRateWindowEnd, &OldEndPayload);
					TestEqual(TEXT("StanceBreak Real Dual-Instance A: Old End rejected - new instance rate remains 2.0"), RealAnim->Montage_GetPlayRate(PlayableStanceMontage), 2.0f);
					TestEqual(TEXT("StanceBreak Real Dual-Instance A: Old End rejected - window count remains 1"), WindowTask->GetRateWindowLifecycle().GetActiveWindowCount(), 1);

					// Cancel ability normally (no VictimLocked)
					EnemyASC->CancelAbilityHandle(Handle);
					TestFalse(TEXT("StanceBreak Real Dual-Instance A: Ability ended"), Ability->IsActive());
					TestFalse(TEXT("StanceBreak Real Dual-Instance A: Movement lock released"), Ability->IsMovementLockedByStanceBreak());
					TestTrue(TEXT("StanceBreak Real Dual-Instance A: Poise restored to max on normal recovery"),
						EnemyASC->GetNumericAttribute(UCharacterAttributeSet::GetPoiseAttribute()) >= EnemyASC->GetNumericAttribute(UCharacterAttributeSet::GetMaxPoiseAttribute()));
					if (UCharacterMovementComponent* CMC = Enemy->GetCharacterMovement())
					{
						TestEqual(TEXT("StanceBreak Real Dual-Instance A: Movement mode restored to Walking"), CMC->MovementMode.GetValue(), MOVE_Walking);
					}
					TestFalse(TEXT("StanceBreak Real Dual-Instance A: Stunned tag removed"), EnemyASC->HasMatchingGameplayTag(TagStunned));
					TestTrue(TEXT("StanceBreak Real Dual-Instance A: marker remains visible during normal cancellation fade"),
						Enemy->GetTestStanceBreakMarkerWidgetComponent()
						&& Enemy->GetTestStanceBreakMarkerWidgetComponent()->GetVisibleFlag());
					TestTrue(TEXT("StanceBreak Real Dual-Instance A: normal cancellation starts marker fade"), Enemy->HasPendingStanceBreakMarkerFade());
					Enemy->TriggerTestCompleteStanceBreakMarkerFade();
					TestFalse(TEXT("StanceBreak Real Dual-Instance A: marker hidden after normal cancellation fade"),
						Enemy->GetTestStanceBreakMarkerWidgetComponent()
						&& Enemy->GetTestStanceBreakMarkerWidgetComponent()->GetVisibleFlag());
					TestFalse(TEXT("StanceBreak Real Dual-Instance A: normal cancellation clears marker fade"), Enemy->HasPendingStanceBreakMarkerFade());
				}

				if (RealAnim->Montage_IsActive(PlayableStanceMontage))
				{
					RealAnim->Montage_Stop(0.0f, PlayableStanceMontage);
				}
				EnemyASC->ClearAbility(Handle);
			}
		}

		// Subtest B: Real Dual-Instance with VictimLocked Handoff Ownership
		{
			EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetPoiseAttribute(), 0.0f);
			TestTrue(TEXT("StanceBreak Real Dual-Instance B: Enemy Poise is broken"), Enemy->IsPoiseBroken());

			UAnimMontage* PlayableStanceMontage = CreatePlayableStanceMontage(*this, World);
			if (TestNotNull(TEXT("StanceBreak Real Dual-Instance B: Playable Montage created"), PlayableStanceMontage))
			{
				USkeletalMeshComponent* Mesh = Enemy->GetMesh();
				Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered;
				UAnimInstance* RealAnim = NewObject<UAnimInstance>(Mesh);
				RealAnim->InitializeMontageOnly();
				RealAnim->CurrentSkeleton = PlayableStanceMontage->GetSkeleton();
				Mesh->AnimScriptInstance = RealAnim;
				EnemyASC->RefreshAbilityActorInfo();

				FGameplayAbilitySpec Spec(UEnemyStanceBreakAbility::StaticClass(), 1, INDEX_NONE, Enemy);
				const FGameplayAbilitySpecHandle Handle = EnemyASC->GiveAbility(Spec);
				FGameplayAbilitySpec* Found = EnemyASC->FindAbilitySpecFromHandle(Handle);

				UEnemyStanceBreakAbility* Ability = nullptr;
				if (Found)
				{
					Ability = Cast<UEnemyStanceBreakAbility>(Found->GetPrimaryInstance());
					if (!TestNotNull(TEXT("Granted stance instance exists"), Ability)) { return false; }
					Ability->SetTestStanceBreakMontage(PlayableStanceMontage);
					const bool bActivated = EnemyASC->TryActivateAbility(Handle);

					TestTrue(TEXT("StanceBreak Real Dual-Instance B: Ability activated without bypass"), bActivated);
					Ability = Cast<UEnemyStanceBreakAbility>(Found->GetPrimaryInstance());
				}

				if (TestNotNull(TEXT("StanceBreak Real Dual-Instance B: Active instance exists"), Ability))
				{
					TestTrue(TEXT("StanceBreak Real Dual-Instance B: Ability is active"), Ability->IsActive());
					UAbilityTask_PlayActionMontage* WindowTask = Ability->GetMontageTask();
					if (!TestNotNull(TEXT("StanceBreak Real Dual-Instance B: standard Task exists"), WindowTask)) { return false; }
					TestTrue(TEXT("StanceBreak Real Dual-Instance B: Movement locked by StanceBreak"), Ability->IsMovementLockedByStanceBreak());

					const int32 OldInstanceID = WindowTask->GetRateWindowLifecycle().GetBoundMontageInstanceID();

					// Replay same Montage at rate 2.0f without stopping existing instance
					const float ReplayLen = RealAnim->Montage_Play(PlayableStanceMontage, 2.0f, EMontagePlayReturnType::MontageLength, 0.0f, false);
					TestTrue(TEXT("StanceBreak Real Dual-Instance B: Replay succeeded"), ReplayLen > 0.0f);

					// Simulate execution handoff: add VictimLocked tag to EnemyASC
					EnemyASC->AddLooseGameplayTag(TagVictimLocked);
					TestTrue(TEXT("StanceBreak Real Dual-Instance B: VictimLocked tag added"), EnemyASC->HasMatchingGameplayTag(TagVictimLocked));
					TestFalse(TEXT("StanceBreak Real Dual-Instance B: marker hidden during VictimLocked handoff"),
						Enemy->GetTestStanceBreakMarkerWidgetComponent()
						&& Enemy->GetTestStanceBreakMarkerWidgetComponent()->GetVisibleFlag());

					// End StanceBreak ability while VictimLocked is active
					EnemyASC->CancelAbilityHandle(Handle);
					TestFalse(TEXT("StanceBreak Real Dual-Instance B: StanceBreak ability ended"), Ability->IsActive());
					TestFalse(TEXT("StanceBreak Real Dual-Instance B: marker remains hidden after handoff"),
						Enemy->GetTestStanceBreakMarkerWidgetComponent()
						&& Enemy->GetTestStanceBreakMarkerWidgetComponent()->GetVisibleFlag());

					// Contract: When VictimLocked is active, StanceBreak must NOT restore Poise and must NOT restore Walking movement
					TestEqual(TEXT("StanceBreak Real Dual-Instance B: Poise remains 0 (handed off to VictimExecution)"),
						EnemyASC->GetNumericAttribute(UCharacterAttributeSet::GetPoiseAttribute()), 0.0f);
					if (UCharacterMovementComponent* CMC = Enemy->GetCharacterMovement())
					{
						TestNotEqual(TEXT("StanceBreak Real Dual-Instance B: Movement mode not restored to Walking (handed off)"), CMC->MovementMode.GetValue(), MOVE_Walking);
					}

					// Cleanup VictimLocked tag and restore Enemy state for test hygiene
					EnemyASC->RemoveLooseGameplayTag(TagVictimLocked);
					TestFalse(TEXT("StanceBreak Real Dual-Instance B: VictimLocked tag removed"), EnemyASC->HasMatchingGameplayTag(TagVictimLocked));
					Enemy->RestorePoiseToMax();
					if (UCharacterMovementComponent* CMC = Enemy->GetCharacterMovement())
					{
						CMC->SetMovementMode(MOVE_Walking);
					}
				}

				if (RealAnim->Montage_IsActive(PlayableStanceMontage))
				{
					RealAnim->Montage_Stop(0.0f, PlayableStanceMontage);
				}
				EnemyASC->ClearAbility(Handle);
			}
		}
	}

#endif
	return true;
}

#endif
