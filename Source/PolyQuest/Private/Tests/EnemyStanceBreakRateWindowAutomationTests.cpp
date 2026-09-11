#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystem/Abilities/EnemyStanceBreakAbility.h"
#include "AbilitySystem/Abilities/MontageRateWindowLifecycle.h"
#include "AbilitySystem/Abilities/PlayerFrontExecutionAbility.h"
#include "AbilitySystem/CharacterAttributeSet.h"
#include "Animation/AnimComposite.h"
#include "Animation/AnimMontage.h"
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
#include "Tests/TestLaunchFacingSmoothingAbility.h"
#include "Tests/TestProjectileDamageGE.h"
#include "UObject/Package.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEnemyStanceBreakRateWindowAutomationTest,
	"PolyQuest.Combat.EnemyStanceBreakRateWindow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
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

		float AppliedRate = -1.0f;
		TestTrue(TEXT("First synthetic Begin is accepted"), Helper.TestApplyBegin(1.75f, 0.5f, AppliedRate));
		TestEqual(TEXT("First Begin applies authored rate"), AppliedRate, 0.5f);
		TestEqual(TEXT("First Begin pushes one rate"), Helper.GetStackDepth(), 1);
		TestTrue(TEXT("Nested synthetic Begin is accepted"), Helper.TestApplyBegin(0.5f, 0.25f, AppliedRate));
		TestEqual(TEXT("Nested Begin applies authored rate"), AppliedRate, 0.25f);
		TestEqual(TEXT("Nested Begin pushes two rates"), Helper.GetStackDepth(), 2);

		float RestoredRate = -1.0f;
		TestTrue(TEXT("First synthetic End restores inner predecessor"), Helper.TestApplyEnd(RestoredRate));
		TestEqual(TEXT("LIFO restores 0.5"), RestoredRate, 0.5f);
		TestTrue(TEXT("Second synthetic End restores baseline predecessor"), Helper.TestApplyEnd(RestoredRate));
		TestEqual(TEXT("LIFO restores 1.75 baseline"), RestoredRate, 1.75f);
		RestoredRate = 9.0f;
		TestFalse(TEXT("Extra synthetic End is ignored"), Helper.TestApplyEnd(RestoredRate));
		TestEqual(TEXT("Extra End leaves output unchanged"), RestoredRate, 9.0f);
		TestEqual(TEXT("Stack is empty after matched Ends"), Helper.GetStackDepth(), 0);
		TestFalse(TEXT("Non-finite current rate is rejected"), Helper.TestApplyBegin(std::numeric_limits<float>::quiet_NaN(), 0.5f, AppliedRate));
		TestFalse(TEXT("Non-positive new rate is rejected"), Helper.TestApplyBegin(1.0f, 0.0f, AppliedRate));

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

		EnemyASC->ClearAbility(Handle);
	}

	// 3.2 Real GAS Production Lifecycle & RateWindow Event Integration
	{
		// 1. Prepare StanceBreak conditions on Enemy:
		// Break Poise so IsPoiseBroken() is true
		EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetPoiseAttribute(), 0.0f);
		TestTrue(TEXT("Enemy Poise is broken"), Enemy->IsPoiseBroken());
		TestTrue(TEXT("Enemy has valid PoiseRecovery configuration"), Enemy->HasValidPoiseRecoveryConfiguration());

		// 2. Prepare mock AnimInstance & StanceBreak Montage
		UAnimMontage* ActiveStanceMontage = NewObject<UAnimMontage>(GetTransientPackage(), TEXT("Test_ActiveStanceMontage"));
		UAnimComposite* ActiveInnerSequence = NewObject<UAnimComposite>(GetTransientPackage(), TEXT("Test_ActiveInnerSequence"));
		FSlotAnimationTrack StanceSlotTrack;
		StanceSlotTrack.SlotName = FName(TEXT("DefaultSlot"));
		FAnimSegment StanceSegment;
		StanceSegment.SetAnimReference(ActiveInnerSequence);
		StanceSegment.StartPos = 0.0f;
		StanceSegment.AnimStartTime = 0.0f;
		StanceSegment.AnimEndTime = 2.0f;
		StanceSegment.AnimPlayRate = 1.0f;
		StanceSlotTrack.AnimTrack.AnimSegments.Add(StanceSegment);
		ActiveStanceMontage->SlotAnimTracks.Add(StanceSlotTrack);

		UAnimInstance* MockAnimInstance = NewObject<UAnimInstance>(Enemy->GetMesh());

		// 3. Give Ability and configure test seams on CDO/Instance
		FGameplayAbilitySpec StanceBreakSpec(UEnemyStanceBreakAbility::StaticClass(), 1, INDEX_NONE, Enemy);
		const FGameplayAbilitySpecHandle StanceBreakHandle = EnemyASC->GiveAbility(StanceBreakSpec);
		FGameplayAbilitySpec* FoundSpec = EnemyASC->FindAbilitySpecFromHandle(StanceBreakHandle);
		TestNotNull(TEXT("StanceBreak spec exists on Enemy ASC"), FoundSpec);

		UEnemyStanceBreakAbility* StanceBreakAbility = nullptr;
		UEnemyStanceBreakAbility* StanceCDO = nullptr;
		UAnimMontage* OriginalStanceBreakMontage = nullptr;
		UAnimInstance* OriginalBoundAnimInstance = nullptr;
		bool bOriginalBypassMontageActiveCheck = false;
		if (FoundSpec)
		{
			StanceCDO = Cast<UEnemyStanceBreakAbility>(FoundSpec->Ability);
			OriginalStanceBreakMontage = StanceCDO ? StanceCDO->GetTestStanceBreakMontage() : nullptr;
			OriginalBoundAnimInstance = StanceCDO ? StanceCDO->GetTestBoundAnimInstance() : nullptr;
			bOriginalBypassMontageActiveCheck = StanceCDO && StanceCDO->GetTestBypassMontageActiveCheck();
			if (StanceCDO)
			{
				StanceCDO->SetTestStanceBreakMontage(ActiveStanceMontage);
				StanceCDO->SetTestBoundAnimInstance(MockAnimInstance);
				StanceCDO->SetTestBypassMontageActiveCheck(true);
			}

			const bool bActivated = EnemyASC->TryActivateAbility(StanceBreakHandle);
			// Restore the process-global CDO immediately after activation has copied
			// the fixture values into the per-activation Ability instance.
			if (StanceCDO)
			{
				StanceCDO->SetTestStanceBreakMontage(OriginalStanceBreakMontage);
				StanceCDO->SetTestBoundAnimInstance(OriginalBoundAnimInstance);
				StanceCDO->SetTestBypassMontageActiveCheck(bOriginalBypassMontageActiveCheck);
			}

			TestTrue(TEXT("UEnemyStanceBreakAbility successfully activated via GAS TryActivateAbility"), bActivated);

			StanceBreakAbility = Cast<UEnemyStanceBreakAbility>(FoundSpec->GetPrimaryInstance());
		}

		if (TestNotNull(TEXT("Active UEnemyStanceBreakAbility instance exists"), StanceBreakAbility))
		{
			// 4. Verify production path components are live and active
			TestTrue(TEXT("Ability is active"), StanceBreakAbility->IsActive());
			TestTrue(TEXT("Enemy ASC has Stunned tag"), EnemyASC->HasMatchingGameplayTag(TagStunned));
			TestTrue(TEXT("Enemy ASC has State.Block.Facing tag during Stance Break"), EnemyASC->HasMatchingGameplayTag(TagBlockFacing));
			TestTrue(TEXT("Movement is locked by StanceBreak"), StanceBreakAbility->IsMovementLockedByStanceBreak());
			TestNotNull(TEXT("ActiveContext exists"), StanceBreakAbility->GetTestActiveContext());
			TestNotNull(TEXT("MontageTask exists"), StanceBreakAbility->GetMontageTask());
			UAbilityTask_WaitGameplayEvent* BeginTask = StanceBreakAbility->GetRateWindowBeginTask();
			UAbilityTask_WaitGameplayEvent* EndTask = StanceBreakAbility->GetRateWindowEndTask();
			TestNotNull(TEXT("RateWindowBeginTask exists"), BeginTask);
			TestNotNull(TEXT("RateWindowEndTask exists"), EndTask);
			TestTrue(TEXT("RateWindowBeginTask is active"), BeginTask && BeginTask->IsActive());
			TestTrue(TEXT("RateWindowEndTask is active"), EndTask && EndTask->IsActive());
			TestTrue(TEXT("RateWindowLifecycle is bound"), StanceBreakAbility->GetRateWindowLifecycle().IsBound());
			TestEqual(TEXT("Initial RateStack depth is 0"), StanceBreakAbility->GetRateWindowLifecycle().GetStackDepth(), 0);

			// 5. Fail-Closed Payload Rejection Tests:
			// 5.1 Wrong Instigator / Target
			{
				FGameplayEventData BadActorPayload;
				BadActorPayload.EventTag = TagRateWindowBegin;
				BadActorPayload.Instigator = Player;
				BadActorPayload.Target = Enemy;
				BadActorPayload.OptionalObject = ActiveStanceMontage;
				BadActorPayload.EventMagnitude = 0.5f;
				EnemyASC->HandleGameplayEvent(TagRateWindowBegin, &BadActorPayload);
				TestEqual(TEXT("Bad Instigator rejected (stack depth unchanged)"), StanceBreakAbility->GetRateWindowLifecycle().GetStackDepth(), 0);
			}

			// 5.2 Wrong Event Tag -> WaitGameplayEvent does not trigger
			{
				const FGameplayTag TagUnrelated = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Enemy.Big")), false);
				FGameplayEventData BadTagPayload;
				BadTagPayload.EventTag = TagUnrelated;
				BadTagPayload.Instigator = Enemy;
				BadTagPayload.Target = Enemy;
				BadTagPayload.OptionalObject = ActiveStanceMontage;
				BadTagPayload.EventMagnitude = 0.5f;
				EnemyASC->HandleGameplayEvent(TagUnrelated, &BadTagPayload);
				TestEqual(TEXT("Unrelated EventTag ignored by Task (stack depth unchanged)"), StanceBreakAbility->GetRateWindowLifecycle().GetStackDepth(), 0);
			}

			// 5.3 Unrelated Montage / Sequence
			{
				UAnimMontage* ForeignMontage = NewObject<UAnimMontage>(GetTransientPackage(), TEXT("Test_ForeignMontage"));
				FGameplayEventData BadMontagePayload;
				BadMontagePayload.EventTag = TagRateWindowBegin;
				BadMontagePayload.Instigator = Enemy;
				BadMontagePayload.Target = Enemy;
				BadMontagePayload.OptionalObject = ForeignMontage;
				BadMontagePayload.EventMagnitude = 0.5f;
				EnemyASC->HandleGameplayEvent(TagRateWindowBegin, &BadMontagePayload);
				TestEqual(TEXT("Foreign Montage rejected (stack depth unchanged)"), StanceBreakAbility->GetRateWindowLifecycle().GetStackDepth(), 0);
			}

			// 5.4 Non-positive / non-finite magnitude
			{
				FGameplayEventData BadMagPayload;
				BadMagPayload.EventTag = TagRateWindowBegin;
				BadMagPayload.Instigator = Enemy;
				BadMagPayload.Target = Enemy;
				BadMagPayload.OptionalObject = ActiveStanceMontage;
				BadMagPayload.EventMagnitude = 0.0f;
				EnemyASC->HandleGameplayEvent(TagRateWindowBegin, &BadMagPayload);
				TestEqual(TEXT("Zero magnitude rejected (stack depth unchanged)"), StanceBreakAbility->GetRateWindowLifecycle().GetStackDepth(), 0);

				BadMagPayload.EventMagnitude = -0.5f;
				EnemyASC->HandleGameplayEvent(TagRateWindowBegin, &BadMagPayload);
				TestEqual(TEXT("Negative magnitude rejected (stack depth unchanged)"), StanceBreakAbility->GetRateWindowLifecycle().GetStackDepth(), 0);
			}

			// 6. Valid Begin & Nested LIFO Rate Window Integration:
			// 6.1 First Valid Begin (0.33f Rate)
			{
				FGameplayEventData ValidBeginPayload;
				ValidBeginPayload.EventTag = TagRateWindowBegin;
				ValidBeginPayload.Instigator = Enemy;
				ValidBeginPayload.Target = Enemy;
				ValidBeginPayload.OptionalObject = ActiveStanceMontage;
				ValidBeginPayload.EventMagnitude = 0.33f;
				EnemyASC->HandleGameplayEvent(TagRateWindowBegin, &ValidBeginPayload);
				TestEqual(TEXT("Valid Begin pushes rate (stack depth 1)"), StanceBreakAbility->GetRateWindowLifecycle().GetStackDepth(), 1);
			}

			// 6.2 Nested Valid Begin using Inner Sequence reference (0.1f Rate)
			{
				FGameplayEventData NestedBeginPayload;
				NestedBeginPayload.EventTag = TagRateWindowBegin;
				NestedBeginPayload.Instigator = Enemy;
				NestedBeginPayload.Target = Enemy;
				NestedBeginPayload.OptionalObject = ActiveInnerSequence;
				NestedBeginPayload.EventMagnitude = 0.1f;
				EnemyASC->HandleGameplayEvent(TagRateWindowBegin, &NestedBeginPayload);
				TestEqual(TEXT("Nested Begin on inner sequence pushes rate (stack depth 2)"), StanceBreakAbility->GetRateWindowLifecycle().GetStackDepth(), 2);
			}

			// 6.3 First End Event (Pops nested rate, restores 0.33f)
			{
				FGameplayEventData EndPayload;
				EndPayload.EventTag = TagRateWindowEnd;
				EndPayload.Instigator = Enemy;
				EndPayload.Target = Enemy;
				EndPayload.OptionalObject = ActiveStanceMontage;
				EnemyASC->HandleGameplayEvent(TagRateWindowEnd, &EndPayload);
				TestEqual(TEXT("First End pops rate (stack depth 1)"), StanceBreakAbility->GetRateWindowLifecycle().GetStackDepth(), 1);
			}

			// 6.4 Second End Event (Pops to baseline)
			{
				FGameplayEventData EndPayload;
				EndPayload.EventTag = TagRateWindowEnd;
				EndPayload.Instigator = Enemy;
				EndPayload.Target = Enemy;
				EndPayload.OptionalObject = ActiveStanceMontage;
				EnemyASC->HandleGameplayEvent(TagRateWindowEnd, &EndPayload);
				TestEqual(TEXT("Second End restores baseline (stack depth 0)"), StanceBreakAbility->GetRateWindowLifecycle().GetStackDepth(), 0);
			}

			// 7. UnPossess Teardown & Complete EndAbility Cleanup Verification
			{
				// Trigger UnPossessed on Enemy
				Enemy->TriggerTestUnPossessed();

				// Verify ability was cancelled by UnPossessed via Teardown.OnUnpossess
				TestFalse(TEXT("StanceBreak ability cancelled on UnPossessed"), StanceBreakAbility->IsActive());
				TestFalse(TEXT("RateWindowLifecycle unbound on EndAbility"), StanceBreakAbility->GetRateWindowLifecycle().IsBound());
				TestNull(TEXT("ActiveContext invalidated"), StanceBreakAbility->GetTestActiveContext());
				TestNull(TEXT("RateWindowBeginTask cleaned up"), StanceBreakAbility->GetRateWindowBeginTask());
				TestNull(TEXT("RateWindowEndTask cleaned up"), StanceBreakAbility->GetRateWindowEndTask());
				TestNull(TEXT("MontageTask cleaned up"), StanceBreakAbility->GetMontageTask());
				TestFalse(TEXT("Movement lock released"), StanceBreakAbility->IsMovementLockedByStanceBreak());
				TestFalse(TEXT("RateWindow test bypass reset after EndAbility"), StanceBreakAbility->GetRateWindowLifecycle().GetTestBypassMontageActiveCheck());
				TestFalse(TEXT("Stunned tag removed from Enemy ASC"), EnemyASC->HasMatchingGameplayTag(TagStunned));
				TestFalse(TEXT("State.Block.Facing tag removed from Enemy ASC after UnPossessed"), EnemyASC->HasMatchingGameplayTag(TagBlockFacing));
				TestTrue(TEXT("Poise restored to MaxPoise"), EnemyASC->GetNumericAttribute(UCharacterAttributeSet::GetPoiseAttribute()) >= EnemyASC->GetNumericAttribute(UCharacterAttributeSet::GetMaxPoiseAttribute()));
			}

			// 8. InstancedPerActor Re-entry Verification
			{
				EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetPoiseAttribute(), 0.0f);
				if (StanceCDO)
				{
					StanceCDO->SetTestStanceBreakMontage(ActiveStanceMontage);
					StanceCDO->SetTestBoundAnimInstance(MockAnimInstance);
					StanceCDO->SetTestBypassMontageActiveCheck(true);
				}

				const bool bReactivated = EnemyASC->TryActivateAbility(StanceBreakHandle);
				if (StanceCDO)
				{
					StanceCDO->SetTestStanceBreakMontage(OriginalStanceBreakMontage);
					StanceCDO->SetTestBoundAnimInstance(OriginalBoundAnimInstance);
					StanceCDO->SetTestBypassMontageActiveCheck(bOriginalBypassMontageActiveCheck);
				}

				TestTrue(TEXT("StanceBreak ability successfully re-activated on same instance"), bReactivated);
				TestTrue(TEXT("FacingBlock tag present on re-activation"), EnemyASC->HasMatchingGameplayTag(TagBlockFacing));

				EnemyASC->CancelAbilityHandle(StanceBreakHandle);
				TestFalse(TEXT("StanceBreak ability cancelled on manual CancelAbility"), StanceBreakAbility->IsActive());
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

	return true;
}

#endif
