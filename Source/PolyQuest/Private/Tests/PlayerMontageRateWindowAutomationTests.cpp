// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include <limits>

#include "AbilitySystemComponent.h"
#include "AbilitySystem/CharacterAttributeSet.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystem/Abilities/LightAttackAbility.h"
#include "AbilitySystem/Abilities/PlayerMeleeSkillAbility.h"
#include "AbilitySystem/Abilities/SprintAttackAbility.h"
#include "AbilitySystem/Tasks/AbilityTask_PlayActionMontage.h"
#include "Tests/TestManagedMontageAbility.h"
#include "AbilitySystem/Abilities/ChargedAttackAbility.h"
#include "AbilitySystem/Abilities/BowDrawFireAbility.h"
#include "AbilitySystem/Abilities/DodgeAbility.h"
#include "AbilitySystem/Abilities/PlayerBigHitReactionAbility.h"
#include "Combat/Equipment/BowWeaponDefinition.h"
#include "Combat/Reaction/HitReactionImpactResolver.h"
#include "Combat/Equipment/ProjectileDefinition.h"
#include "Combat/Equipment/WeaponEquipmentComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Tests/TestMobileBowMoveSpeedGE.h"
#include "UObject/UnrealType.h"
#include "UObject/StrongObjectPtr.h"
#include <type_traits>
#include "AbilitySystem/Abilities/MontageRateWindowLifecycle.h"
#include "Animation/AnimComposite.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifyQueue.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Animation/Combat/AnimNotifyState_ActionWindows.h"
#include "Animation/ActiveMontageInstanceScope.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Character/Player/PlayerCharacter.h"
#include "Combat/ComboChainDataAsset.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameplayTagContainer.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ScopeExit.h"
#include "ReferenceSkeleton.h"
#include "Tests/CombatAutomationFixture.h"
#include "UObject/Package.h"

#if WITH_EDITOR
#include "Animation/AnimData/IAnimationDataController.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlayerMontageRateWindowAutomationTest,
	"PolyQuest.Combat.PlayerMontageRateWindow.Light",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace PlayerMontageRateWindowAutomation
{
#if WITH_EDITOR
	class UTestMontageLengthAccess : public UAnimMontage
	{
	public:
		static void SetLength(UAnimMontage* Montage, float Length)
		{
			static_cast<UTestMontageLengthAccess*>(Montage)->SequenceLength = Length;
		}
	};

	UAnimMontage* CreatePlayableRateMontage(FAutomationTestBase& Test, UObject* Outer, const FString& Suffix, USkeleton* ExistingSkeleton = nullptr)
	{
		USkeleton* Skeleton = ExistingSkeleton ? ExistingSkeleton : NewObject<USkeleton>(Outer);
		const FName RootBoneName(TEXT("root"));
		if (!ExistingSkeleton)
		{
			FReferenceSkeletonModifier Modifier(Skeleton);
			Modifier.Add(FMeshBoneInfo(RootBoneName, TEXT("root"), INDEX_NONE), FTransform::Identity);
		}

		UAnimSequence* Sequence = NewObject<UAnimSequence>(Outer, *FString::Printf(TEXT("Seq_%s"), *Suffix));
		Sequence->SetSkeleton(Skeleton);
		IAnimationDataController& Controller = Sequence->GetController();
		Controller.InitializeModel();
		bool bPopulated = false;
		{
			IAnimationDataController::FScopedBracket Populate(Controller,
				FText::FromString(TEXT("Populate RateWindow runtime fixture")), false);
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

		UAnimMontage* Montage = NewObject<UAnimMontage>(Outer, *FString::Printf(TEXT("Montage_%s"), *Suffix));
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
		UTestMontageLengthAccess::SetLength(Montage, Sequence->GetPlayLength());
		Montage->BlendIn.SetBlendTime(0.0f);
		Montage->BlendOut.SetBlendTime(0.0f);

		for (int32 WindowIndex = 0; WindowIndex < 2; ++WindowIndex)
		{
			const FName WindowName(WindowIndex == 0 ? TEXT("RateWindowA") : TEXT("RateWindowB"));
			UAnimNotifyState_MontageRateWindow* Notify = NewObject<UAnimNotifyState_MontageRateWindow>(Montage, WindowName);
			Notify->RateMultiplier = WindowIndex == 0 ? 0.5f : 0.2f;
			FAnimNotifyEvent Event;
			Event.NotifyStateClass = Notify;
			Montage->Notifies.Add(Event);
		}
		return Montage;
	}

	FAnimNotifyEvent* FindRateWindowEvent(UAnimMontage* Montage, int32 WindowIndex)
	{
		const FName WindowName(WindowIndex == 0 ? TEXT("RateWindowA") : TEXT("RateWindowB"));
		return Montage->Notifies.FindByPredicate([WindowName](const FAnimNotifyEvent& Event)
		{
			return Event.NotifyStateClass && Event.NotifyStateClass->GetFName() == WindowName;
		});
	}
#endif

	struct FTestMontageSetup
	{
		UAnimMontage* Montage = nullptr;
		UAnimComposite* Sequence = nullptr;
		UAnimNotifyState_MontageRateWindow* NotifyA = nullptr;
		UAnimNotifyState_MontageRateWindow* NotifyB = nullptr;
		UAnimNotifyState_MontageRateWindow* NotifyC = nullptr;
	};

	FTestMontageSetup CreateTestMontage(UObject* Outer, const FName& SlotName = FName(TEXT("DefaultSlot")))
	{
		UObject* EffectiveOuter = Outer ? Outer : GetTransientPackage();
		UAnimMontage* Montage = NewObject<UAnimMontage>(EffectiveOuter);
		UAnimComposite* Sequence = NewObject<UAnimComposite>(EffectiveOuter);

		FSlotAnimationTrack SlotTrack;
		SlotTrack.SlotName = SlotName;
		FAnimSegment Segment;
		Segment.SetAnimReference(Sequence);
		Segment.StartPos = 0.0f;
		Segment.AnimStartTime = 0.0f;
		Segment.AnimEndTime = 1.0f;
		Segment.AnimPlayRate = 1.0f;
		SlotTrack.AnimTrack.AnimSegments.Add(Segment);
		Montage->SlotAnimTracks.Add(SlotTrack);

		UAnimNotifyState_MontageRateWindow* NotifyA = NewObject<UAnimNotifyState_MontageRateWindow>(EffectiveOuter, TEXT("Test_PlayerNotifyA"));
		NotifyA->RateMultiplier = 0.5f;
		FAnimNotifyEvent EventA;
		EventA.NotifyStateClass = NotifyA;
		Montage->Notifies.Add(EventA);

		UAnimNotifyState_MontageRateWindow* NotifyB = NewObject<UAnimNotifyState_MontageRateWindow>(EffectiveOuter, TEXT("Test_PlayerNotifyB"));
		NotifyB->RateMultiplier = 0.2f;
		FAnimNotifyEvent EventB;
		EventB.NotifyStateClass = NotifyB;
		Montage->Notifies.Add(EventB);

		UAnimNotifyState_MontageRateWindow* NotifyC = NewObject<UAnimNotifyState_MontageRateWindow>(EffectiveOuter, TEXT("Test_PlayerNotifyC"));
		NotifyC->RateMultiplier = 0.8f;
		FAnimNotifyEvent EventC;
		EventC.NotifyStateClass = NotifyC;
		Montage->Notifies.Add(EventC);

		return FTestMontageSetup{ Montage, Sequence, NotifyA, NotifyB, NotifyC };
	}

	UAnimMontage* CreateDummyMontage(UObject* Outer, const FName& SlotName = FName(TEXT("DefaultSlot")))
	{
		UObject* EffectiveOuter = Outer ? Outer : GetTransientPackage();
		UAnimMontage* Montage = NewObject<UAnimMontage>(EffectiveOuter);
		UAnimComposite* Sequence = NewObject<UAnimComposite>(EffectiveOuter);

		FSlotAnimationTrack SlotTrack;
		SlotTrack.SlotName = SlotName;
		FAnimSegment Segment;
		Segment.SetAnimReference(Sequence);
		Segment.StartPos = 0.0f;
		Segment.AnimStartTime = 0.0f;
		Segment.AnimEndTime = 1.0f;
		Segment.AnimPlayRate = 1.0f;
		SlotTrack.AnimTrack.AnimSegments.Add(Segment);
		Montage->SlotAnimTracks.Add(SlotTrack);
		return Montage;
	}

	UComboChainDataAsset* CreateTestComboDefinition(UObject* Outer, UAnimMontage* Entry0Montage, UAnimMontage* Entry1Montage, UAnimMontage* Entry2Montage)
	{
		UObject* EffectiveOuter = Outer ? Outer : GetTransientPackage();
		UComboChainDataAsset* ComboAsset = NewObject<UComboChainDataAsset>(EffectiveOuter);

		if (Entry0Montage)
		{
			FComboChainEntry Entry0;
			Entry0.Montage = Entry0Montage;
			ComboAsset->Entries.Add(Entry0);
		}
		if (Entry1Montage)
		{
			FComboChainEntry Entry1;
			Entry1.Montage = Entry1Montage;
			ComboAsset->Entries.Add(Entry1);
		}
		if (Entry2Montage)
		{
			FComboChainEntry Entry2;
			Entry2.Montage = Entry2Montage;
			ComboAsset->Entries.Add(Entry2);
		}

		return ComboAsset;
	}

	struct FTestWorldScope
	{
		UWorld* World = nullptr;
		~FTestWorldScope()
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

bool FPlayerMontageRateWindowAutomationTest::RunTest(const FString& Parameters)
{
	// =========================================================================
	// 1. CDO & Tag Contract Verification
	// =========================================================================
	const FGameplayTag TagRateWindowBegin = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.RateWindow.Begin")), false);
	const FGameplayTag TagRateWindowEnd = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.RateWindow.End")), false);
	const FGameplayTag TagAbilityLight = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Light")), false);

	TestTrue(TEXT("Tag Event.Action.RateWindow.Begin is valid"), TagRateWindowBegin.IsValid());
	TestTrue(TEXT("Tag Event.Action.RateWindow.End is valid"), TagRateWindowEnd.IsValid());
	TestTrue(TEXT("Tag Ability.Attack.Light is valid"), TagAbilityLight.IsValid());

	const ULightAttackAbility* LightCDO = ULightAttackAbility::StaticClass()->GetDefaultObject<ULightAttackAbility>();
	TestNotNull(TEXT("ULightAttackAbility CDO exists"), LightCDO);
	if (LightCDO)
	{
		TestEqual(TEXT("LightAttack instancing is InstancedPerActor"),
			LightCDO->GetInstancingPolicy(), EGameplayAbilityInstancingPolicy::InstancedPerActor);
		TestEqual(TEXT("LightAttack net execution is ServerOnly"),
			LightCDO->GetNetExecutionPolicy(), EGameplayAbilityNetExecutionPolicy::ServerOnly);
		TestTrue(TEXT("LightAttack carries Ability.Attack.Light tag"),
			LightCDO->AbilityTags.HasTagExact(TagAbilityLight));
		TestTrue(TEXT("Standard RateWindow Begin tag exists"),
			TagRateWindowBegin.IsValid());
		TestTrue(TEXT("Standard RateWindow End tag exists"),
			TagRateWindowEnd.IsValid());
		TestNull(TEXT("LightAttack CDO has null MontageTask"), LightCDO->GetTestMontageTask());
		TestFalse(TEXT("LightAttack CDO has no active rate binding"), LightCDO->GetTestRateWindowLifecycle().IsBound());
		TestEqual(TEXT("LightAttack CDO has default ActiveMontageInstanceID INDEX_NONE"), LightCDO->GetTestActiveMontageInstanceID(), INDEX_NONE);
	}

	// =========================================================================
	// 2. World, Player and ASC Setup
	// =========================================================================
	if (!TestNotNull(TEXT("GEngine is valid"), GEngine))
	{
		return false;
	}

	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("PlayerMontageRateWindowTestWorld"));
	WorldContext.SetCurrentWorld(World);
	PlayerMontageRateWindowAutomation::FTestWorldScope ScopeCleanup{ World };

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
		FTransform(FRotator::ZeroRotator, FVector(100.0f, 0.0f, 0.0f)));

	if (!TestNotNull(TEXT("Player spawned"), Player) || !TestNotNull(TEXT("Enemy spawned"), Enemy))
	{
		return false;
	}

	UAbilitySystemComponent* PlayerASC = Player->GetAbilitySystemComponent();
	if (!TestNotNull(TEXT("Player ASC valid"), PlayerASC))
	{
		return false;
	}

	PlayerMontageRateWindowAutomation::FTestMontageSetup Entry0Setup = PlayerMontageRateWindowAutomation::CreateTestMontage(World);
	PlayerMontageRateWindowAutomation::FTestMontageSetup Entry1Setup = PlayerMontageRateWindowAutomation::CreateTestMontage(World);
	PlayerMontageRateWindowAutomation::FTestMontageSetup Entry2Setup = PlayerMontageRateWindowAutomation::CreateTestMontage(World);
	UAnimMontage* ForeignMontage = PlayerMontageRateWindowAutomation::CreateDummyMontage(World);

	UAnimNotifyState_MontageRateWindow* ForeignNotify = NewObject<UAnimNotifyState_MontageRateWindow>(World, TEXT("Test_ForeignNotify"));
	ForeignNotify->RateMultiplier = 0.5f;
	FAnimNotifyEvent ForeignEvent;
	ForeignEvent.NotifyStateClass = ForeignNotify;
	ForeignMontage->Notifies.Add(ForeignEvent);

	UComboChainDataAsset* ComboAsset = PlayerMontageRateWindowAutomation::CreateTestComboDefinition(
		World, Entry0Setup.Montage, Entry1Setup.Montage, Entry2Setup.Montage);
	TestNotNull(TEXT("ComboDataAsset created with 3 entries"), ComboAsset);

	UAnimInstance* MockAnimInstance = NewObject<UAnimInstance>(Player->GetMesh());
	const auto SendRate = [&](UAbilityTask_PlayActionMontage* SourceTask, FGameplayEventData Payload)
	{
		Payload.TargetData = FManagedMontageTestHelpers::MakeRateWindowEventData(
			Payload.EventTag, SourceTask->GetBoundAnimInstance(), SourceTask->GetBoundMontageInstanceID(),
			Payload.EventMagnitude, Player, SourceTask->GetMontageToPlay(), Payload.OptionalObject2.Get()).TargetData;
		PlayerASC->HandleGameplayEvent(Payload.EventTag, &Payload);
	};


	// =========================================================================
	// 3-6. Isolated Logic Fixture Tests: RateWindow Math, Overlap & Rejection
	//      (Isolated unit tests validating rate stack math, priority, and rejection.
	//       Explicitly uses SetTestAbilityActive(true) on the local test fixture
	//       without full ASC activation overhead; Section 7 serves as the runtime gate)
	// =========================================================================
	// 3. LightAttackAbility RateWindow Lifecycle: Baseline, Single Window & Overlap
	// =========================================================================
	{
		ULightAttackAbility* LightAbility = NewObject<ULightAttackAbility>(Player);
		LightAbility->SetTestActorInfo(PlayerASC->AbilityActorInfo.Get());
		LightAbility->SetTestComboDefinition(ComboAsset);
		LightAbility->SetTestBoundAnimInstance(MockAnimInstance);
		LightAbility->SetTestBypassMontageActiveCheck(true);
		LightAbility->SetTestAbilityActive(true);

		// Start Combo Entry 0
		const bool bStarted0 = LightAbility->TestStartComboEntry(0);
		if (!TestTrue(TEXT("LightAttack: Entry 0 started successfully"), bStarted0))
		{
			return false;
		}
		TestEqual(TEXT("LightAttack: ActiveEntryIndex is 0"), LightAbility->GetTestActiveEntryIndex(), 0);

		const auto Lifecycle = [&]() -> const FAbilityMontageRateWindowLifecycle& { return LightAbility->GetTestRateWindowLifecycle(); };
		TestTrue(TEXT("LightAttack: RateWindowLifecycle is bound after Entry 0 start"), Lifecycle().IsBound());
		TestEqual(TEXT("LightAttack: Initial active window count is 0"), Lifecycle().GetActiveWindowCount(), 0);
		TestEqual(TEXT("LightAttack: Baseline play rate is 1.0"), Lifecycle().GetBaselinePlayRate(), 1.0f);
		TestTrue(TEXT("LightAttack: current Task is active"), LightAbility->GetTestMontageTask()->IsActive());
		UAbilityTask_PlayActionMontage* Task0 = LightAbility->GetTestMontageTask();
		if (!TestNotNull(TEXT("LightAttack: MontageTask is valid"), Task0))
		{
			return false;
		}
		TestEqual(TEXT("LightAttack: ActiveMontageInstanceID remains INDEX_NONE under test bypass without real instance"), LightAbility->GetTestActiveMontageInstanceID(), INDEX_NONE);

		// 3.1 Single Window: Begin -> End (Rate changed and restored)
		FGameplayEventData BeginA;
		BeginA.EventTag = TagRateWindowBegin;
		BeginA.Instigator = Player;
		BeginA.Target = Player;
		BeginA.OptionalObject = Entry0Setup.Montage;
		BeginA.OptionalObject2 = Entry0Setup.NotifyA;
		BeginA.EventMagnitude = 0.5f;

		SendRate(Task0, BeginA);
		TestEqual(TEXT("LightAttack: Active window count is 1 after BeginA"), Lifecycle().GetActiveWindowCount(), 1);
		TestEqual(TEXT("LightAttack: Current target rate is 0.5"), Lifecycle().GetCurrentTargetRate(), 0.5f);

		FGameplayEventData EndA;
		EndA.EventTag = TagRateWindowEnd;
		EndA.Instigator = Player;
		EndA.Target = Player;
		EndA.OptionalObject = Entry0Setup.Montage;
		EndA.OptionalObject2 = Entry0Setup.NotifyA;

		SendRate(Task0, EndA);
		TestEqual(TEXT("LightAttack: Active window count is 0 after EndA"), Lifecycle().GetActiveWindowCount(), 0);
		TestEqual(TEXT("LightAttack: Target rate restored to baseline 1.0"), Lifecycle().GetCurrentTargetRate(), 1.0f);

		// 3.2 Overlapping Windows: Nested (A then B, B ends before A) -> Last-Active-Wins
		BeginA.EventMagnitude = 0.5f;
		SendRate(Task0, BeginA);
		TestEqual(TEXT("LightAttack: BeginA target rate 0.5"), Lifecycle().GetCurrentTargetRate(), 0.5f);

		FGameplayEventData BeginB;
		BeginB.EventTag = TagRateWindowBegin;
		BeginB.Instigator = Player;
		BeginB.Target = Player;
		BeginB.OptionalObject = Entry0Setup.Montage;
		BeginB.OptionalObject2 = Entry0Setup.NotifyB;
		BeginB.EventMagnitude = 0.2f;

		SendRate(Task0, BeginB);
		TestEqual(TEXT("LightAttack: BeginB overrides to target rate 0.2 (Last-Active-Wins)"), Lifecycle().GetCurrentTargetRate(), 0.2f);
		TestEqual(TEXT("LightAttack: Stack depth is 2"), Lifecycle().GetStackDepth(), 2);

		// End B -> Restores to A (0.5), not baseline
		FGameplayEventData EndB;
		EndB.EventTag = TagRateWindowEnd;
		EndB.Instigator = Player;
		EndB.Target = Player;
		EndB.OptionalObject = Entry0Setup.Montage;
		EndB.OptionalObject2 = Entry0Setup.NotifyB;

		SendRate(Task0, EndB);
		TestEqual(TEXT("LightAttack: After EndB, target rate restores to A (0.5)"), Lifecycle().GetCurrentTargetRate(), 0.5f);
		TestEqual(TEXT("LightAttack: Stack depth is 1"), Lifecycle().GetStackDepth(), 1);

		// End A -> Restores to baseline (1.0)
		SendRate(Task0, EndA);
		TestEqual(TEXT("LightAttack: After EndA, target rate restores to baseline 1.0"), Lifecycle().GetCurrentTargetRate(), 1.0f);
		TestEqual(TEXT("LightAttack: Stack depth is 0"), Lifecycle().GetStackDepth(), 0);

		// 3.3 Overlapping Windows: Interleaved (A then B, A ends before B) -> B continues until EndB
		SendRate(Task0, BeginA);
		SendRate(Task0, BeginB);
		TestEqual(TEXT("LightAttack: Interleaved setup has rate 0.2"), Lifecycle().GetCurrentTargetRate(), 0.2f);

		// A ends first -> B is still active, rate must remain 0.2
		SendRate(Task0, EndA);
		TestEqual(TEXT("LightAttack: After EndA, B remains top with rate 0.2"), Lifecycle().GetCurrentTargetRate(), 0.2f);
		TestEqual(TEXT("LightAttack: Stack depth is 1 after removing A"), Lifecycle().GetStackDepth(), 1);

		// B ends -> restores to baseline
		SendRate(Task0, EndB);
		TestEqual(TEXT("LightAttack: After EndB, rate restores to baseline 1.0"), Lifecycle().GetCurrentTargetRate(), 1.0f);

		// =====================================================================
		// 4. Rejection Matrix (Negative Cases Fail-Closed)
		// =====================================================================
		// 4.1 Wrong Avatar
		{
			FGameplayEventData WrongAvatarPayload = BeginA;
			WrongAvatarPayload.Instigator = Enemy;
			WrongAvatarPayload.Target = Enemy;
			SendRate(Task0, WrongAvatarPayload);
			TestEqual(TEXT("Reject: Wrong Avatar does not add window"), Lifecycle().GetActiveWindowCount(), 0);
		}

		// 4.2 Non-matching Tag (Partial / Parent tag)
		{
			FGameplayEventData ParentTagPayload = BeginA;
			ParentTagPayload.EventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.RateWindow")), false);
			SendRate(Task0, ParentTagPayload);
			TestEqual(TEXT("Reject: Parent tag does not trigger exact match"), Lifecycle().GetActiveWindowCount(), 0);
		}

		// 4.3 Foreign Montage (Not current active entry montage and not embedded sequence)
		{
			FGameplayEventData ForeignMontagePayload = BeginA;
			ForeignMontagePayload.OptionalObject = ForeignMontage;
			ForeignMontagePayload.OptionalObject2 = ForeignNotify;
			SendRate(Task0, ForeignMontagePayload);
			TestEqual(TEXT("Reject: Foreign Montage rejected"), Lifecycle().GetActiveWindowCount(), 0);
		}

		// 4.4 Missing or Invalid Notify State (nullptr OptionalObject2)
		{
			FGameplayEventData NullNotifyPayload = BeginA;
			NullNotifyPayload.OptionalObject2 = nullptr;
			SendRate(Task0, NullNotifyPayload);
			TestEqual(TEXT("Reject: Null OptionalObject2 rejected"), Lifecycle().GetActiveWindowCount(), 0);
		}

		// 4.5 Foreign Notify not declared in source animation
		{
			FGameplayEventData ForeignNotifyPayload = BeginA;
			ForeignNotifyPayload.OptionalObject2 = ForeignNotify; // Belonging to ForeignMontage, not Entry0Setup
			SendRate(Task0, ForeignNotifyPayload);
			TestEqual(TEXT("Reject: Undeclared notify rejected"), Lifecycle().GetActiveWindowCount(), 0);
		}

		// 4.6 Non-positive or non-finite rate magnitude
		{
			FGameplayEventData ZeroRatePayload = BeginA;
			ZeroRatePayload.EventMagnitude = 0.0f;
			SendRate(Task0, ZeroRatePayload);
			TestEqual(TEXT("Reject: Zero magnitude rejected"), Lifecycle().GetActiveWindowCount(), 0);

			FGameplayEventData NegativeRatePayload = BeginA;
			NegativeRatePayload.EventMagnitude = -1.5f;
			SendRate(Task0, NegativeRatePayload);
			TestEqual(TEXT("Reject: Negative magnitude rejected"), Lifecycle().GetActiveWindowCount(), 0);

			FGameplayEventData NanRatePayload = BeginA;
			NanRatePayload.EventMagnitude = std::numeric_limits<float>::quiet_NaN();
			SendRate(Task0, NanRatePayload);
			TestEqual(TEXT("Reject: NaN magnitude rejected"), Lifecycle().GetActiveWindowCount(), 0);

			FGameplayEventData InfRatePayload = BeginA;
			InfRatePayload.EventMagnitude = std::numeric_limits<float>::infinity();
			SendRate(Task0, InfRatePayload);
			TestEqual(TEXT("Reject: Infinity magnitude rejected"), Lifecycle().GetActiveWindowCount(), 0);
		}

		// 4.7 Duplicate Begin (Idempotent)
		{
			SendRate(Task0, BeginA);
			TestEqual(TEXT("BeginA opens 1 window"), Lifecycle().GetActiveWindowCount(), 1);

			// Duplicate BeginA must be ignored without stacking or priority refresh
			SendRate(Task0, BeginA);
			TestEqual(TEXT("Duplicate BeginA does not increase window count"), Lifecycle().GetActiveWindowCount(), 1);
			TestEqual(TEXT("Rate remains 0.5"), Lifecycle().GetCurrentTargetRate(), 0.5f);

			SendRate(Task0, EndA);
			TestEqual(TEXT("EndA cleans up"), Lifecycle().GetActiveWindowCount(), 0);
		}

		// 4.8 Unknown End does not affect other active windows
		{
			SendRate(Task0, BeginA);
			TestEqual(TEXT("Active window count is 1"), Lifecycle().GetActiveWindowCount(), 1);

			// Fire EndB while only A is active
			SendRate(Task0, EndB);
			TestEqual(TEXT("Unknown EndB does not affect active window A"), Lifecycle().GetActiveWindowCount(), 1);
			TestEqual(TEXT("Target rate remains 0.5"), Lifecycle().GetCurrentTargetRate(), 0.5f);

			SendRate(Task0, EndA);
			TestEqual(TEXT("EndA properly closes window"), Lifecycle().GetActiveWindowCount(), 0);
		}

		// 4.9 End before Begin is fail-closed
		{
			SendRate(Task0, EndA);
			TestEqual(TEXT("End before Begin remains 0"), Lifecycle().GetActiveWindowCount(), 0);
			TestEqual(TEXT("Rate remains baseline"), Lifecycle().GetCurrentTargetRate(), 1.0f);
		}

		// =====================================================================
		// 5. Exclusive Regression: Light Combo Entry Transition Lifecycle
		// =====================================================================
		// 5.1 Open window in Entry 0 -> Transition to Entry 1
		SendRate(Task0, BeginA);
		TestEqual(TEXT("ComboRegress: Window A active in Entry 0"), Lifecycle().GetActiveWindowCount(), 1);
		TestEqual(TEXT("ComboRegress: Rate is 0.5 in Entry 0"), Lifecycle().GetCurrentTargetRate(), 0.5f);

		UAbilityTask_PlayActionMontage* OldTask0 = LightAbility->GetTestMontageTask();
		const int32 OldTaskID0 = OldTask0->GetBoundMontageInstanceID();
		TestNotEqual(TEXT("ComboRegress: Entry 0 has a fixture source ID"), OldTaskID0, static_cast<int32>(INDEX_NONE));

		// Switch to Entry 1: must cleanly teardown old window and establish new binding
		const bool bStarted1 = LightAbility->TestStartComboEntry(1);
		if (!TestTrue(TEXT("ComboRegress: Entry 1 started successfully"), bStarted1))
		{
			return false;
		}
		TestEqual(TEXT("ComboRegress: ActiveEntryIndex updated to 1"), LightAbility->GetTestActiveEntryIndex(), 1);

		// Old window from Entry 0 must be cleared and baseline restored on new entry
		TestEqual(TEXT("ComboRegress: Active window count reset to 0 in Entry 1"), Lifecycle().GetActiveWindowCount(), 0);
		TestEqual(TEXT("ComboRegress: Target rate reset to baseline 1.0 in Entry 1"), Lifecycle().GetCurrentTargetRate(), 1.0f);

		// Task identities must be distinct and isolated
		UAbilityTask_PlayActionMontage* NewTask1 = LightAbility->GetTestMontageTask();
		const int32 NewTaskID1 = NewTask1->GetBoundMontageInstanceID();
		if (!TestNotNull(TEXT("ComboRegress: New Task created for Entry 1"), NewTask1))
		{
			return false;
		}
		TestTrue(TEXT("ComboRegress: Task pointer changed between entries"), NewTask1 != OldTask0);
		TestNotEqual(TEXT("ComboRegress: Entry 1 has a distinct fixture source ID"), NewTaskID1, OldTaskID0);
		TestTrue(TEXT("ComboRegress: old Task terminated"), OldTask0->IsTerminated());

		// 5.2 Stale callback from Entry 0 Task must be discarded
		SendRate(OldTask0, BeginA);
		TestEqual(TEXT("ComboRegress: Stale callback from Entry 0 discarded, window count remains 0"), Lifecycle().GetActiveWindowCount(), 0);

		// 5.3 Entry 1 receives its own valid RateWindow event
		FGameplayEventData BeginEntry1;
		BeginEntry1.EventTag = TagRateWindowBegin;
		BeginEntry1.Instigator = Player;
		BeginEntry1.Target = Player;
		BeginEntry1.OptionalObject = Entry1Setup.Montage;
		BeginEntry1.OptionalObject2 = Entry1Setup.NotifyA;
		BeginEntry1.EventMagnitude = 0.4f;

		SendRate(NewTask1, BeginEntry1);
		TestEqual(TEXT("ComboRegress: Entry 1 accepts its own window"), Lifecycle().GetActiveWindowCount(), 1);
		TestEqual(TEXT("ComboRegress: Entry 1 rate modified to 0.4"), Lifecycle().GetCurrentTargetRate(), 0.4f);

		// Transition to Entry 2
		const bool bStarted2 = LightAbility->TestStartComboEntry(2);
		if (!TestTrue(TEXT("ComboRegress: Entry 2 started successfully"), bStarted2))
		{
			return false;
		}
		TestEqual(TEXT("ComboRegress: ActiveEntryIndex updated to 2"), LightAbility->GetTestActiveEntryIndex(), 2);
		TestEqual(TEXT("ComboRegress: Entry 2 window count reset to 0"), Lifecycle().GetActiveWindowCount(), 0);
		TestNotEqual(TEXT("ComboRegress: Entry 2 has a distinct Task"), LightAbility->GetTestMontageTask(), NewTask1);

		// =====================================================================
		// 6. EndAbility Teardown & Convergence
		// =====================================================================
		// Open window in Entry 2
		FGameplayEventData BeginEntry2;
		BeginEntry2.EventTag = TagRateWindowBegin;
		BeginEntry2.Instigator = Player;
		BeginEntry2.Target = Player;
		BeginEntry2.OptionalObject = Entry2Setup.Montage;
		BeginEntry2.OptionalObject2 = Entry2Setup.NotifyA;
		BeginEntry2.EventMagnitude = 0.6f;

		UAbilityTask_PlayActionMontage* Task2 = LightAbility->GetTestMontageTask();
		if (!TestNotNull(TEXT("Teardown: MontageTask valid for Entry 2"), Task2))
		{
			return false;
		}
		SendRate(Task2, BeginEntry2);
		TestEqual(TEXT("Teardown: Active window count is 1 before EndAbility"), Lifecycle().GetActiveWindowCount(), 1);
		TestEqual(TEXT("Teardown: Target rate is 0.6 before EndAbility"), Lifecycle().GetCurrentTargetRate(), 0.6f);

		// EndAbility
		LightAbility->EndAbility(FGameplayAbilitySpecHandle(), PlayerASC->AbilityActorInfo.Get(), FGameplayAbilityActivationInfo(), true, false);

		TestFalse(TEXT("Teardown: Lifecycle is no longer bound after EndAbility"), Lifecycle().IsBound());
		TestEqual(TEXT("Teardown: Active window count is 0 after EndAbility"), Lifecycle().GetActiveWindowCount(), 0);
		TestNull(TEXT("Teardown: MontageTask is cleared to nullptr"), LightAbility->GetTestMontageTask());
		TestEqual(TEXT("Teardown: ActiveMontageInstanceID reset to INDEX_NONE"), LightAbility->GetTestActiveMontageInstanceID(), INDEX_NONE);
		TestFalse(TEXT("Teardown: Bypass flag reset after EndAbility"), LightAbility->GetTestBypassMontageActiveCheck());
	}

	// =========================================================================
	// 7. Runtime Integration Gate: Real ASC, Real Playable Montage & Zero Bypass
	// =========================================================================
#if WITH_EDITOR
	{
		// 7.1 Setup 3 playable montages sharing the same skeleton for Entry 0, 1, 2
		UAnimMontage* PlayableMontage0 = PlayerMontageRateWindowAutomation::CreatePlayableRateMontage(*this, World, TEXT("Playable0"));
		UAnimMontage* PlayableMontage1 = PlayerMontageRateWindowAutomation::CreatePlayableRateMontage(*this, World, TEXT("Playable1"), PlayableMontage0 ? PlayableMontage0->GetSkeleton() : nullptr);
		UAnimMontage* PlayableMontage2 = PlayerMontageRateWindowAutomation::CreatePlayableRateMontage(*this, World, TEXT("Playable2"), PlayableMontage0 ? PlayableMontage0->GetSkeleton() : nullptr);
		if (!TestNotNull(TEXT("Runtime: PlayableMontage0 created"), PlayableMontage0)
			|| !TestNotNull(TEXT("Runtime: PlayableMontage1 created"), PlayableMontage1)
			|| !TestNotNull(TEXT("Runtime: PlayableMontage2 created"), PlayableMontage2))
		{
			return false;
		}

		UComboChainDataAsset* PlayableComboAsset = PlayerMontageRateWindowAutomation::CreateTestComboDefinition(
			World, PlayableMontage0, PlayableMontage1, PlayableMontage2);

		USkeletalMeshComponent* Mesh = Player->GetMesh();
		if (!TestNotNull(TEXT("Runtime: Player mesh exists"), Mesh))
		{
			return false;
		}

		Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered;
		UAnimInstance* Anim = NewObject<UAnimInstance>(Mesh);
		Anim->InitializeMontageOnly();
		Anim->CurrentSkeleton = PlayableMontage0->GetSkeleton();
		Mesh->AnimScriptInstance = Anim;
		PlayerASC->RefreshAbilityActorInfo();

		if (!TestTrue(TEXT("Runtime: ASC resolves the montage-only AnimInstance"),
			PlayerASC->AbilityActorInfo.IsValid() && PlayerASC->AbilityActorInfo->GetAnimInstance() == Anim))
		{
			return false;
		}

		// Ensure Player has sufficient Stamina for cost checks and commit
		PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 100.0f);
		PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxStaminaAttribute(), 100.0f);

		const FGameplayTag TagStateAttacking = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Attacking")));
		const FGameplayTag TagInputBlockMove = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Movement")));
		const FGameplayTag TagInputBlockJump = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Jump")));
		const FGameplayTag TagBranchBegin = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Attack.Light.Combo.BranchWindow.Begin")));
		const FGameplayTag TagInputPressed = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Input.Pressed")));
		const FGameplayTag TagInputPrimaryAttack = FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")));

		// Give ability to PlayerASC
		const FGameplayAbilitySpecHandle Handle = PlayerASC->GiveAbility(
			FGameplayAbilitySpec(ULightAttackAbility::StaticClass(), 1, INDEX_NONE, Player));
		FGameplayAbilitySpec* Spec = PlayerASC->FindAbilitySpecFromHandle(Handle);
		ULightAttackAbility* Ability = Spec ? Cast<ULightAttackAbility>(Spec->GetPrimaryInstance()) : nullptr;
		if (!TestNotNull(TEXT("Runtime: ASC created the instanced ability"), Ability))
		{
			return false;
		}

		// Configure required production assets/effects on the ability instance
		Ability->SetTestComboDefinition(PlayableComboAsset);
		Ability->SetTestCostGameplayEffectClass(UGameplayEffect::StaticClass());
		Ability->SetTestDamageGameplayEffectClass(UGameplayEffect::StaticClass());
		Ability->SetTestStaminaRegenDelayGameplayEffectClass(UGameplayEffect::StaticClass());

		// ZERO bypass, NO manual SetTestAbilityActive
		Ability->SetTestBypassMontageActiveCheck(false);

		// ---------------------------------------------------------------------
		// 7.2 Real ASC Activation Entry: PlayerASC->TryActivateAbility
		// ---------------------------------------------------------------------
		const bool bActivated = PlayerASC->TryActivateAbility(Handle);
		if (!TestTrue(TEXT("Runtime: TryActivateAbility succeeded through real ASC pipeline"), bActivated))
		{
			return false;
		}

		// Verify ability and spec are genuinely active
		TestTrue(TEXT("Runtime: Ability is active via GAS"), Ability->IsActive());
		TestTrue(TEXT("Runtime: Spec is active via GAS"), Spec->IsActive());
		TestTrue(TEXT("Runtime: PlayerASC granted State.Action.Attacking"), PlayerASC->HasMatchingGameplayTag(TagStateAttacking));
		TestTrue(TEXT("Runtime: PlayerASC granted State.Input.Block.Movement"), PlayerASC->HasMatchingGameplayTag(TagInputBlockMove));
		TestTrue(TEXT("Runtime: PlayerASC granted State.Input.Block.Jump"), PlayerASC->HasMatchingGameplayTag(TagInputBlockJump));

		// Verify Entry 0 state
		TestEqual(TEXT("Runtime: ActiveEntryIndex is 0"), Ability->GetTestActiveEntryIndex(), 0);
		TestTrue(TEXT("Runtime: initial Task is active"), Ability->GetTestMontageTask()->IsActive());
		TestTrue(TEXT("Runtime: PlayableMontage0 is playing on AnimInstance"), Anim->Montage_IsActive(PlayableMontage0));

		const FAnimMontageInstance* RealInstance0 = Anim->GetActiveInstanceForMontage(PlayableMontage0);
		if (TestNotNull(TEXT("Runtime: Real montage instance exists for Entry 0"), RealInstance0))
		{
			TestEqual(TEXT("Runtime: Captured ActiveMontageInstanceID matches real instance ID"),
				Ability->GetTestActiveMontageInstanceID(), RealInstance0->GetInstanceID());
		}

		const auto MakeNotifyReference = [&](FAnimNotifyEvent* Event, UAnimMontage* Montage)
		{
			FAnimNotifyEventReference Reference(Event, Montage);
			Reference.AddContextData<UE::Anim::FAnimNotifyMontageInstanceContext>(Ability->GetTestActiveMontageInstanceID());
			return Reference;
		};
		const auto RuntimeLifecycle = [&]() -> const FAbilityMontageRateWindowLifecycle& { return Ability->GetTestRateWindowLifecycle(); };
		TestTrue(TEXT("Runtime: Lifecycle is bound"), RuntimeLifecycle().IsBound());
		TestEqual(TEXT("Runtime: Captured baseline rate for Entry 0 is 1.0"), RuntimeLifecycle().GetBaselinePlayRate(), 1.0f);
		TestEqual(TEXT("Runtime: Initial active windows is 0"), RuntimeLifecycle().GetActiveWindowCount(), 0);

		UAbilityTask_PlayActionMontage* Task0 = Ability->GetTestMontageTask();
		if (!TestNotNull(TEXT("Runtime: Task0 is valid"), Task0))
		{
			return false;
		}

		// ---------------------------------------------------------------------
		// 7.3 Real RateWindow Delivery: NotifyBegin -> Rate Window -> NotifyEnd
		// ---------------------------------------------------------------------
		FAnimNotifyEvent* NotifyEvent0A = PlayerMontageRateWindowAutomation::FindRateWindowEvent(PlayableMontage0, 0);
		if (TestNotNull(TEXT("Runtime: RateWindowA notify event exists on PlayableMontage0"), NotifyEvent0A))
		{
			UAnimNotifyState_MontageRateWindow* RateNotify0A = Cast<UAnimNotifyState_MontageRateWindow>(NotifyEvent0A->NotifyStateClass);
			if (TestNotNull(TEXT("Runtime: RateNotify0A exists"), RateNotify0A))
			{
				FAnimNotifyEventReference EventRef0A(NotifyEvent0A, PlayableMontage0);
				EventRef0A.AddContextData<UE::Anim::FAnimNotifyMontageInstanceContext>(Ability->GetTestActiveMontageInstanceID());

				// Fire NotifyBegin
				RateNotify0A->NotifyBegin(Mesh, PlayableMontage0, 1.0f, EventRef0A);
				TestEqual(TEXT("Runtime: Window count is 1 after real NotifyBegin"), RuntimeLifecycle().GetActiveWindowCount(), 1);
				TestEqual(TEXT("Runtime: Actual montage play rate adjusted to 0.5"), Anim->Montage_GetPlayRate(PlayableMontage0), 0.5f);

				// Fire NotifyEnd
				RateNotify0A->NotifyEnd(Mesh, PlayableMontage0, EventRef0A);
				TestEqual(TEXT("Runtime: Window count is 0 after real NotifyEnd"), RuntimeLifecycle().GetActiveWindowCount(), 0);
				TestEqual(TEXT("Runtime: Actual montage play rate restored to baseline 1.0"), Anim->Montage_GetPlayRate(PlayableMontage0), 1.0f);
			}
		}

		// ---------------------------------------------------------------------
		// 7.4 Real Combo Branching: Entry 0 -> Entry 1 (Non-1.0 Baseline)
		// ---------------------------------------------------------------------
		// Advance combo via production event path: BranchWindow.Begin + Input.Pressed(Input.PrimaryAttack)
		FGameplayEventData BranchEvent0;
		BranchEvent0.EventTag = TagBranchBegin;
		BranchEvent0.Instigator = Player;
		BranchEvent0.Target = Player;
		BranchEvent0.OptionalObject = PlayableMontage0;
		PlayerASC->HandleGameplayEvent(TagBranchBegin, &BranchEvent0);

		FGameplayEventData InputEvent0;
		InputEvent0.EventTag = TagInputPressed;
		InputEvent0.Instigator = Player;
		InputEvent0.Target = Player;
		InputEvent0.InstigatorTags.AddTag(TagInputPrimaryAttack);
		PlayerASC->HandleGameplayEvent(TagInputPressed, &InputEvent0);

		// Verify transition to Entry 1
		TestEqual(TEXT("Runtime: ActiveEntryIndex advanced to 1"), Ability->GetTestActiveEntryIndex(), 1);
		TestTrue(TEXT("Runtime: old entry Task terminated on transition"), Task0->IsTerminated());
		TestTrue(TEXT("Runtime: PlayableMontage1 is active"), Anim->Montage_IsActive(PlayableMontage1));

		UAbilityTask_PlayActionMontage* Task1 = Ability->GetTestMontageTask();
		TestNotNull(TEXT("Runtime: Task1 is valid"), Task1);
		TestTrue(TEXT("Runtime: Task1 is a distinct instance from Task0"), Task1 != Task0);
		TestFalse(TEXT("Runtime: old Task rate binding removed"), Task0->GetRateWindowLifecycle().IsBound());

		// Stale callback from Task0 must be safely discarded
		if (NotifyEvent0A)
		{
			FGameplayEventData StaleBegin;
			StaleBegin.EventTag = TagRateWindowBegin;
			StaleBegin.Instigator = Player;
			StaleBegin.Target = Player;
			StaleBegin.OptionalObject = PlayableMontage0;
			StaleBegin.OptionalObject2 = NotifyEvent0A->NotifyStateClass;
			StaleBegin.EventMagnitude = 0.5f;
			SendRate(Task0, StaleBegin);
			TestEqual(TEXT("Runtime: Stale callback from Task0 discarded"), RuntimeLifecycle().GetActiveWindowCount(), 0);
		}

		// Exercise capture of a non-unit rate on the real playing montage instance
		Ability->GetTestRateWindowLifecycle_Mutable().RestoreAndClear();
		Anim->Montage_SetPlayRate(PlayableMontage1, 1.25f);
		Ability->GetTestRateWindowLifecycle_Mutable().BindAndCapture(Ability, Anim, PlayableMontage1, TagRateWindowBegin, TagRateWindowEnd);
		TestEqual(TEXT("Runtime: BindAndCapture reads actual 1.25 baseline"), RuntimeLifecycle().GetBaselinePlayRate(), 1.25f);

		// Fire RateWindow on Entry 1 (rate multiplier 0.5)
		FAnimNotifyEvent* NotifyEvent1A = PlayerMontageRateWindowAutomation::FindRateWindowEvent(PlayableMontage1, 0);
		if (TestNotNull(TEXT("Runtime: RateWindowA notify event exists on PlayableMontage1"), NotifyEvent1A))
		{
			UAnimNotifyState_MontageRateWindow* RateNotify1A = Cast<UAnimNotifyState_MontageRateWindow>(NotifyEvent1A->NotifyStateClass);
			if (TestNotNull(TEXT("Runtime: RateNotify1A exists"), RateNotify1A))
			{
				FAnimNotifyEventReference EventRef1A(NotifyEvent1A, PlayableMontage1);
				EventRef1A.AddContextData<UE::Anim::FAnimNotifyMontageInstanceContext>(Ability->GetTestActiveMontageInstanceID());

				RateNotify1A->NotifyBegin(Mesh, PlayableMontage1, 1.0f, EventRef1A);
				TestEqual(TEXT("Runtime: Window count is 1 for Entry 1"), RuntimeLifecycle().GetActiveWindowCount(), 1);
				TestEqual(TEXT("Runtime: Actual play rate adjusted to 0.5"), Anim->Montage_GetPlayRate(PlayableMontage1), 0.5f);

				RateNotify1A->NotifyEnd(Mesh, PlayableMontage1, EventRef1A);
				TestEqual(TEXT("Runtime: Window count is 0 after NotifyEnd"), RuntimeLifecycle().GetActiveWindowCount(), 0);
				TestEqual(TEXT("Runtime: Actual play rate restored to non-1.0 baseline 1.25"),
					Anim->Montage_GetPlayRate(PlayableMontage1), 1.25f);
			}
		}

		// ---------------------------------------------------------------------
		// 7.5 Real Combo Branching: Entry 1 -> Entry 2
		// ---------------------------------------------------------------------
		FGameplayEventData BranchEvent1;
		BranchEvent1.EventTag = TagBranchBegin;
		BranchEvent1.Instigator = Player;
		BranchEvent1.Target = Player;
		BranchEvent1.OptionalObject = PlayableMontage1;
		PlayerASC->HandleGameplayEvent(TagBranchBegin, &BranchEvent1);

		FGameplayEventData InputEvent1;
		InputEvent1.EventTag = TagInputPressed;
		InputEvent1.Instigator = Player;
		InputEvent1.Target = Player;
		InputEvent1.InstigatorTags.AddTag(TagInputPrimaryAttack);
		PlayerASC->HandleGameplayEvent(TagInputPressed, &InputEvent1);

		TestEqual(TEXT("Runtime: ActiveEntryIndex advanced to 2"), Ability->GetTestActiveEntryIndex(), 2);
		TestTrue(TEXT("Runtime: second entry Task terminated on transition"), Task1->IsTerminated());
		TestTrue(TEXT("Runtime: PlayableMontage2 is active"), Anim->Montage_IsActive(PlayableMontage2));

		UAbilityTask_PlayActionMontage* Task2 = Ability->GetTestMontageTask();
		TestNotNull(TEXT("Runtime: Task2 is valid"), Task2);
		TestTrue(TEXT("Runtime: Task2 is distinct from Task1"), Task2 != Task1);
		TestFalse(TEXT("Runtime: second Task rate binding removed"), Task1->GetRateWindowLifecycle().IsBound());
		TestEqual(TEXT("Runtime: Captured baseline rate for Entry 2 is 1.0"), RuntimeLifecycle().GetBaselinePlayRate(), 1.0f);
		FAnimNotifyEvent* NotifyEvent2A = PlayerMontageRateWindowAutomation::FindRateWindowEvent(PlayableMontage2, 0);
		if (!TestNotNull(TEXT("Runtime: Entry 2 notify exists"), NotifyEvent2A)) return false;
		auto* RateNotify2A = CastChecked<UAnimNotifyState_MontageRateWindow>(NotifyEvent2A->NotifyStateClass);
		RateNotify2A->NotifyBegin(Mesh, PlayableMontage2, 1.0f, MakeNotifyReference(NotifyEvent2A, PlayableMontage2));
		TestEqual(TEXT("Runtime: Entry 2 new listener receives NotifyBegin"), Anim->Montage_GetPlayRate(PlayableMontage2), 0.5f);

		// ---------------------------------------------------------------------
		// 7.6 ASC Cancellation: PlayerASC->CancelAbilityHandle
		// ---------------------------------------------------------------------
		PlayerASC->CancelAbilityHandle(Handle);

		// Verify complete teardown through real GAS cancellation
		TestFalse(TEXT("Runtime: Ability inactive after CancelAbilityHandle"), Ability->IsActive());
		TestFalse(TEXT("Runtime: Spec inactive after CancelAbilityHandle"), Spec->IsActive());
		TestFalse(TEXT("Runtime: State.Action.Attacking tag removed after cancel"), PlayerASC->HasMatchingGameplayTag(TagStateAttacking));
		TestFalse(TEXT("Runtime: State.Input.Block.Movement tag removed after cancel"), PlayerASC->HasMatchingGameplayTag(TagInputBlockMove));
		TestFalse(TEXT("Runtime: State.Input.Block.Jump tag removed after cancel"), PlayerASC->HasMatchingGameplayTag(TagInputBlockJump));
		TestFalse(TEXT("Runtime: Lifecycle is unbound after cancel"), RuntimeLifecycle().IsBound());
		TestNull(TEXT("Runtime: MontageTask cleared to null after cancel"), Ability->GetTestMontageTask());
		TestEqual(TEXT("Runtime: ActiveMontageInstanceID reset to INDEX_NONE"), Ability->GetTestActiveMontageInstanceID(), INDEX_NONE);

		// ---------------------------------------------------------------------
		// 7.7 Reactivation Gate: Clean Re-activation & Natural Completion
		// ---------------------------------------------------------------------
		const bool bReactivated = PlayerASC->TryActivateAbility(Handle);
		TestTrue(TEXT("Runtime: Reactivation via TryActivateAbility succeeded"), bReactivated);
		TestTrue(TEXT("Runtime: Ability is active on reactivation"), Ability->IsActive());
		TestTrue(TEXT("Runtime: Spec is active on reactivation"), Spec->IsActive());
		TestTrue(TEXT("Runtime: State.Action.Attacking reapplied"), PlayerASC->HasMatchingGameplayTag(TagStateAttacking));
		TestEqual(TEXT("Runtime: Re-activation starts at Entry 0"), Ability->GetTestActiveEntryIndex(), 0);
		TestTrue(TEXT("Runtime: Lifecycle rebound on reactivation"), RuntimeLifecycle().IsBound());
		TestEqual(TEXT("Runtime: Re-activation baseline rate is 1.0"), RuntimeLifecycle().GetBaselinePlayRate(), 1.0f);

		// End-delegate routing with an active window; not timeline-driven completion.
		CastChecked<UAnimNotifyState_MontageRateWindow>(NotifyEvent0A->NotifyStateClass)->NotifyBegin(
			Mesh, PlayableMontage0, 1.0f, MakeNotifyReference(NotifyEvent0A, PlayableMontage0));
		TestEqual(TEXT("Runtime: Window active before end callback"), RuntimeLifecycle().GetActiveWindowCount(), 1);
		FAnimMontageInstance* EndingInstance = Anim->GetMontageInstanceForID(Ability->GetTestActiveMontageInstanceID());
		if (!TestNotNull(TEXT("Runtime: ending instance exists"), EndingInstance)) return false;
		EndingInstance->Stop(FAlphaBlend(0.0f), false);
		Anim->TickMontageOnly(0.001f);
		Anim->DispatchQueuedAnimEvents();

		TestFalse(TEXT("Runtime: Ability inactive after natural montage end"), Ability->IsActive());
		TestFalse(TEXT("Runtime: Spec inactive after natural montage end"), Spec->IsActive());
		TestFalse(TEXT("Runtime: State.Action.Attacking tag removed after natural end"), PlayerASC->HasMatchingGameplayTag(TagStateAttacking));
		TestFalse(TEXT("Runtime: Lifecycle is unbound after natural end"), RuntimeLifecycle().IsBound());
		TestNull(TEXT("Runtime: MontageTask cleared to null after natural end"), Ability->GetTestMontageTask());

		// 7.8 Same-asset replacement while the old instance is still live (no montage bypass).
		PlayableMontage0->bEnableRootMotionTranslation = false;
		PlayableMontage0->bEnableRootMotionRotation = false;
		if (!TestTrue(TEXT("Replacement: ASC reactivates Light"), PlayerASC->TryActivateAbility(Handle) && Ability->IsActive())) return false;
		UAbilityTask_PlayActionMontage* ReplacedTask = Ability->GetTestMontageTask();
		if (!TestNotNull(TEXT("Replacement: old Task exists"), ReplacedTask)) return false;
		TStrongObjectPtr<UAbilityTask_PlayActionMontage> KeepReplacedTask(ReplacedTask);
		const int32 ReplacedID = Ability->GetTestActiveMontageInstanceID();
		FAnimNotifyEvent* ReplacementEventA = PlayerMontageRateWindowAutomation::FindRateWindowEvent(PlayableMontage0, 0);
		FAnimNotifyEvent* ReplacementEventB = PlayerMontageRateWindowAutomation::FindRateWindowEvent(PlayableMontage0, 1);
		if (!TestNotNull(TEXT("Replacement: A exists"), ReplacementEventA) || !TestNotNull(TEXT("Replacement: B exists"), ReplacementEventB)) return false;
		CastChecked<UAnimNotifyState_MontageRateWindow>(ReplacementEventA->NotifyStateClass)->NotifyBegin(
			Mesh, PlayableMontage0, 1.0f, MakeNotifyReference(ReplacementEventA, PlayableMontage0));
		TestEqual(TEXT("Replacement: old window is active"), Anim->Montage_GetPlayRate(PlayableMontage0), 0.5f);
		// This fixture has no root-motion override that would force the old instance to stop.
		TestTrue(TEXT("Replacement: replay without stopping the old instance succeeds"),
			Anim->Montage_Play(PlayableMontage0, 2.0f, EMontagePlayReturnType::MontageLength, 0.0f, false) > 0.0f);
		const FAnimMontageInstance* OldLiveInstance = Anim->GetMontageInstanceForID(ReplacedID);
		const FAnimMontageInstance* NewLiveInstance = Anim->GetActiveInstanceForMontage(PlayableMontage0);
		if (!TestNotNull(TEXT("Replacement: old instance still exists"), OldLiveInstance)
			|| !TestNotNull(TEXT("Replacement: new current instance exists"), NewLiveInstance)) return false;
		TestFalse(TEXT("Replacement: old instance is not stopped"), OldLiveInstance->IsStopped());
		TestNotEqual(TEXT("Replacement: current instance differs from bound ID"), NewLiveInstance->GetInstanceID(), ReplacedID);
		FGameplayEventData ReplacedPayload;
		ReplacedPayload.Instigator = Player;
		ReplacedPayload.Target = Player;
		ReplacedPayload.OptionalObject = PlayableMontage0;
		ReplacedPayload.OptionalObject2 = ReplacementEventB->NotifyStateClass;
		ReplacedPayload.EventTag = TagRateWindowBegin;
		ReplacedPayload.EventMagnitude = 0.2f;
		SendRate(ReplacedTask, ReplacedPayload);
		TestEqual(TEXT("Replacement: old Begin cannot alter new rate"), Anim->Montage_GetPlayRate(PlayableMontage0), 2.0f);
		ReplacedPayload.OptionalObject2 = ReplacementEventA->NotifyStateClass;
		ReplacedPayload.EventTag = TagRateWindowEnd;
		SendRate(ReplacedTask, ReplacedPayload);
		TestEqual(TEXT("Replacement: old End cannot alter new rate"), Anim->Montage_GetPlayRate(PlayableMontage0), 2.0f);
		ReplacedTask->TestCleanupTask(false);
		TestEqual(TEXT("Replacement: old cleanup cannot restore over new rate"), Anim->Montage_GetPlayRate(PlayableMontage0), 2.0f);
		TestFalse(TEXT("Replacement: cleanup clears old binding"), RuntimeLifecycle().IsBound());
		TestTrue(TEXT("Replacement: cleanup terminates old Task"), ReplacedTask->IsTerminated());
		PlayerASC->CancelAbilityHandle(Handle);

		// 7.9 Native timeline gate. Reuse these memory-only montages with one animation driver.
		UCharacterMovementComponent* Movement = Player->GetCharacterMovement();
		if (!TestNotNull(TEXT("Timeline: movement exists"), Movement)) return false;
		const bool bMeshTick = Mesh->IsComponentTickEnabled();
		const bool bMovementTick = Movement->IsComponentTickEnabled();
		const bool bAutonomousPose = Mesh->bIsAutonomousTickPose;
		Mesh->SetComponentTickEnabled(false);
		Movement->SetComponentTickEnabled(false);
		ON_SCOPE_EXIT
		{
			PlayerASC->CancelAbilityHandle(Handle);
			Mesh->SetComponentTickEnabled(bMeshTick);
			Movement->SetComponentTickEnabled(bMovementTick);
			Mesh->bIsAutonomousTickPose = bAutonomousPose;
		};
		class FProxyAccess : public UAnimInstance
		{
		public:
			static FAnimInstanceProxy& Get(UAnimInstance* Instance) { return *GetProxyOnGameThreadStatic<FAnimInstanceProxy>(Instance); }
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
		Anim->Montage_Stop(0.0f);
		Advance(0.001f);
		for (UAnimMontage* Montage : {PlayableMontage0, PlayableMontage1, PlayableMontage2})
		{
			Montage->Notifies.Reset();
			Montage->BlendOut.SetBlendTime(0.2f);
			const auto AddWindow = [&](UAnimNotifyState* Notify, float Begin, float End)
			{
				FAnimNotifyEvent& Event = Montage->Notifies.AddDefaulted_GetRef();
				Event.NotifyStateClass = Notify;
				Event.MontageTickType = EMontageNotifyTickType::Queued;
				Event.Link(Montage, Begin);
				Event.SetTime(Begin);
				Event.SetDuration(End - Begin);
				Event.EndLink.Link(Montage, End);
				Event.EndLink.SetTime(End);
			};
			auto* RateNotify = NewObject<UAnimNotifyState_MontageRateWindow>(Montage);
			RateNotify->RateMultiplier = 0.5f;
			AddWindow(RateNotify, 0.1f, 0.2f);
			AddWindow(NewObject<UAnimNotifyState_ActionDodgeCancelWindow>(Montage), 0.1f, 0.3f);
			Montage->SortNotifies();
		}
		const FGameplayTag DodgePermission = FGameplayTag::RequestGameplayTag(TEXT("State.Action.CanCancel.Dodge"));
		const FGameplayTag DefensePermission = FGameplayTag::RequestGameplayTag(TEXT("State.Action.CanCancel.Defense"));
		PlayableMontage1->BlendIn.SetBlendTime(0.2f);
		// Natural completion, cancel, interrupted blend, reactivation, failed playback, handoff cancellation.
		for (int32 Exit = 0; Exit < 6; ++Exit)
		{
			const FString Label = FString::Printf(TEXT("Timeline exit %d: "), Exit);
			if (!TestTrue(*(Label + TEXT("real activation")), PlayerASC->TryActivateAbility(Handle) && Ability->IsActive())) return false;
			auto* FirstTask = Ability->GetTestMontageTask();
			if (!TestNotNull(*(Label + TEXT("standard Task")), FirstTask)) return false;
			TestEqual(*(Label + TEXT("DodgeAndDefense policy")), FirstTask->GetCancelPolicy(), EActionMontageCancelPolicy::DodgeAndDefense);
			TestFalse(*(Label + TEXT("no bypass")), FirstTask->GetTestBypassMontageActiveCheck());
			TestEqual(*(Label + TEXT("starts at zero")), Anim->Montage_GetPosition(PlayableMontage0), 0.0f);
			TestEqual(*(Label + TEXT("root motion scale")), Player->GetAnimRootMotionTranslationScale(), 1.0f);
			TestTrue(*(Label + TEXT("Begin subscription")), PlayerASC->GenericGameplayEventCallbacks.FindChecked(TagRateWindowBegin).IsBoundToObject(FirstTask));
			TestTrue(*(Label + TEXT("End subscription")), PlayerASC->GenericGameplayEventCallbacks.FindChecked(TagRateWindowEnd).IsBoundToObject(FirstTask));
			TestFalse(*(Label + TEXT("no Dodge outside window")), PlayerASC->HasMatchingGameplayTag(DodgePermission));
			TestFalse(*(Label + TEXT("no Defense outside window")), PlayerASC->HasMatchingGameplayTag(DefensePermission));
			Advance(0.15f);
			TestEqual(*(Label + TEXT("native rate begin")), Anim->Montage_GetPlayRate(PlayableMontage0), 0.5f);
			TestEqual(*(Label + TEXT("native cancel begin")), FirstTask->GetActiveCancelWindowCount(), 1);
			TestTrue(*(Label + TEXT("native Dodge permission")), PlayerASC->HasMatchingGameplayTag(DodgePermission));
			TestTrue(*(Label + TEXT("native Defense permission")), PlayerASC->HasMatchingGameplayTag(DefensePermission));
			TestFalse(*(Label + TEXT("invalid entry rejected")), Ability->TestStartComboEntry(-1));
			TestTrue(*(Label + TEXT("preflight failure preserves current Task")), Ability->GetTestMontageTask() == FirstTask && FirstTask->IsActive());
			TestEqual(*(Label + TEXT("preflight failure preserves rate")), Anim->Montage_GetPlayRate(PlayableMontage0), 0.5f);
			TestTrue(*(Label + TEXT("preflight failure preserves cancel window")), PlayerASC->HasMatchingGameplayTag(DodgePermission));
			if (Exit == 5)
			{
				const FDelegateHandle CancelOnWindowClose = PlayerASC->RegisterGameplayTagEvent(DodgePermission, EGameplayTagEventType::NewOrRemoved).AddLambda(
					[&](const FGameplayTag, int32 Count) { if (Count == 0) PlayerASC->CancelAbilityHandle(Handle); });
				PlayerASC->HandleGameplayEvent(TagBranchBegin, &BranchEvent0);
				PlayerASC->HandleGameplayEvent(TagInputPressed, &InputEvent0);
				PlayerASC->RegisterGameplayTagEvent(DodgePermission, EGameplayTagEventType::NewOrRemoved).Remove(CancelOnWindowClose);
				TestFalse(*(Label + TEXT("window release cancellation ends ability")), Ability->IsActive());
				TestTrue(*(Label + TEXT("cancelled handoff terminates old Task")), FirstTask->IsTerminated());
				TestFalse(*(Label + TEXT("cancelled handoff stops old montage")), Anim->Montage_IsPlaying(PlayableMontage0));
				TestNull(*(Label + TEXT("cancelled handoff cannot install new Task")), Ability->GetTestMontageTask());
				TestFalse(*(Label + TEXT("cancelled handoff clears Defense")), PlayerASC->HasMatchingGameplayTag(DefensePermission));
				Advance(0.01f);
				continue;
			}

			// Branch through the existing production input/event route while both windows are active.
			const int32 PreviousInstanceID = FirstTask->GetBoundMontageInstanceID();
			const FAnimMontageInstance* BeforeTransition = Anim->GetMontageInstanceForID(PreviousInstanceID);
			if (!TestNotNull(*(Label + TEXT("old instance before transition")), BeforeTransition)) return false;
			const float PreviousWeight = BeforeTransition->GetWeight();
			TestTrue(*(Label + TEXT("old pose has weight before transition")), PreviousWeight > 0.0f);
			PlayerASC->HandleGameplayEvent(TagBranchBegin, &BranchEvent0);
			PlayerASC->HandleGameplayEvent(TagInputPressed, &InputEvent0);
			auto* SecondTask = Ability->GetTestMontageTask();
			if (!TestNotNull(*(Label + TEXT("second Task")), SecondTask)) return false;
			TestEqual(*(Label + TEXT("advanced to entry 1")), Ability->GetTestActiveEntryIndex(), 1);
			TestTrue(*(Label + TEXT("old Task terminated")), FirstTask->IsTerminated());
			TestFalse(*(Label + TEXT("old rate binding cleared")), FirstTask->GetRateWindowLifecycle().IsBound());
			TestFalse(*(Label + TEXT("old cancel permission cleared")), PlayerASC->HasMatchingGameplayTag(DodgePermission));
			const FAnimMontageInstance* BlendingPrevious = Anim->GetMontageInstanceForID(PreviousInstanceID);
			if (!TestNotNull(*(Label + TEXT("old pose retained for crossfade")), BlendingPrevious)) return false;
			TestEqual(*(Label + TEXT("transition uses next montage blend-in duration")), BlendingPrevious->GetBlendTime(), 0.2f);
			TestEqual(*(Label + TEXT("transition preserves old pose weight")), BlendingPrevious->GetWeight(), PreviousWeight);
			Advance(0.15f);
			TestTrue(*(Label + TEXT("old End cannot finish new entry")), Ability->IsActive() && Ability->GetTestMontageTask() == SecondTask);
			TestEqual(*(Label + TEXT("second entry native rate")), Anim->Montage_GetPlayRate(PlayableMontage1), 0.5f);
			TestTrue(*(Label + TEXT("second entry native cancel")), PlayerASC->HasMatchingGameplayTag(DefensePermission));
			if (Exit == 0)
			{
				Advance(0.5f);
				TestEqual(*(Label + TEXT("native rate end restores baseline")), Anim->Montage_GetPlayRate(PlayableMontage1), 1.0f);
				TestFalse(*(Label + TEXT("native cancel end revokes permission")), PlayerASC->HasMatchingGameplayTag(DodgePermission));
				for (int32 Step = 0; Step < 60; ++Step)
				{
					const FAnimMontageInstance* Instance = Anim->GetMontageInstanceForID(Ability->GetTestActiveMontageInstanceID());
					if (!Instance || Instance->IsStopped()) break;
					Advance(0.025f);
				}
				TestTrue(*(Label + TEXT("natural blend retains business lifetime")), Ability->IsActive());
				Advance(0.3f);
			}
			else if (Exit == 2)
			{
				Anim->Montage_Stop(0.2f, PlayableMontage1);
				Advance(0.05f);
				TestTrue(*(Label + TEXT("interrupted blend retains business lifetime")), Ability->IsActive());
				Advance(0.3f);
			}
			else if (Exit == 4)
			{
				UAnimMontage* InvalidMontage = NewObject<UAnimMontage>(World);
				PlayableComboAsset->Entries[2].Montage = InvalidMontage;
				TestFalse(*(Label + TEXT("unplayable entry fails")), Ability->TestStartComboEntry(2));
				TestNull(*(Label + TEXT("failed playback does not restore old Task")), Ability->GetTestMontageTask());
				TestFalse(*(Label + TEXT("failed handoff stops previous montage")), Anim->Montage_IsPlaying(PlayableMontage1));
				PlayableComboAsset->Entries[2].Montage = PlayableMontage2;
			}
			else
			{
				PlayerASC->CancelAbilityHandle(Handle);
				if (Exit == 3)
				{
					if (!TestTrue(*(Label + TEXT("immediate reactivation")), PlayerASC->TryActivateAbility(Handle) && Ability->IsActive())) return false;
					auto* ReactivatedTask = Ability->GetTestMontageTask();
					Advance(0.05f);
					TestTrue(*(Label + TEXT("old end cannot cancel reactivation")), Ability->IsActive() && Ability->GetTestMontageTask() == ReactivatedTask);
					PlayerASC->CancelAbilityHandle(Handle);
				}
			}
			TestFalse(*(Label + TEXT("ability ended")), Ability->IsActive());
			TestTrue(*(Label + TEXT("second Task cleaned")), SecondTask->IsTerminated());
			TestFalse(*(Label + TEXT("no Dodge residue")), PlayerASC->HasMatchingGameplayTag(DodgePermission));
			TestFalse(*(Label + TEXT("no Defense residue")), PlayerASC->HasMatchingGameplayTag(DefensePermission));
			TestFalse(*(Label + TEXT("no attacking residue")), PlayerASC->HasMatchingGameplayTag(TagStateAttacking));
			Advance(0.01f);
		}

		// Clean up ability from ASC
		PlayerASC->ClearAbility(Handle);
	}
#endif

	return true;
}


#if WITH_EDITOR
namespace PlayerMontageRateWindowAutomation
{
	bool SetFixtureObject(UObject* Owner, const FName PropertyName, UObject* Value)
	{
		FObjectPropertyBase* Property = Owner ? FindFProperty<FObjectPropertyBase>(Owner->GetClass(), PropertyName) : nullptr;
		if (!Property)
		{
			return false;
		}
		Property->SetObjectPropertyValue_InContainer(Owner, Value);
		return true;
	}

	template<typename TAbility>
	bool RunConsumer(FAutomationTestBase& Test, const TCHAR* MontageProperty, const TCHAR* Name)
	{
		FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		WorldContext.SetCurrentWorld(World);
		FTestWorldScope Cleanup{ World };
		if (!Test.TestNotNull(TEXT("Consumer world exists"), World)) return false;
		FURL URL;
		World->InitializeActorsForPlay(URL);
		World->BeginPlay();
		APlayerCharacter* Player = FCombatAutomationFixture::SpawnPlayer(World);
		if (!Test.TestNotNull(TEXT("Player exists"), Player)) return false;
		UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
		if (!Test.TestNotNull(TEXT("ASC exists"), ASC)) return false;
		Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxStaminaAttribute(), 100.0f);
		ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 100.0f);

		UAnimMontage* Montage = CreatePlayableRateMontage(Test, World, Name);
		if (!Test.TestNotNull(TEXT("Playable consumer montage exists"), Montage)) return false;
		UAnimInstance* Anim = NewObject<UAnimInstance>(Player->GetMesh());
		Anim->InitializeMontageOnly();
		Anim->CurrentSkeleton = Montage->GetSkeleton();
		Player->GetMesh()->AnimScriptInstance = Anim;
		ASC->RefreshAbilityActorInfo();
		const FGameplayTag PrimaryInput = FGameplayTag::RequestGameplayTag(TEXT("Input.PrimaryAttack"));
		const FGameplayTag BeginTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Action.RateWindow.Begin"));
		const FGameplayTag EndTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Action.RateWindow.End"));
		if constexpr (std::is_same_v<TAbility, USprintAttackAbility>)
		{
			// Set the prerequisite through GAS; this suite tests SprintAttack, not Sprint input acquisition.
			ASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Movement.Sprinting")));
			Player->SetTestCurrentMoveInput(FVector2D(0.0f, 1.0f));
		}
		if constexpr (std::is_same_v<TAbility, UBowDrawFireAbility>)
		{
			Player->TriggerTestHandleCombatInputStarted(PrimaryInput);
			ASC->CancelAllAbilities(); // Keep held input, finish the separate PrimaryAttack router fixture.
		}
		if constexpr (std::is_same_v<TAbility, UBowDrawFireAbility>)
		{
			UBowWeaponDefinition* Bow = NewObject<UBowWeaponDefinition>(World);
			Bow->DefaultProjectileDefinition = NewObject<UProjectileDefinition>(Bow);
			if (!Test.TestTrue(TEXT("Bow fixture installs the equipped definition"),
				SetFixtureObject(Player->FindComponentByClass<UWeaponEquipmentComponent>(), TEXT("CurrentMainHandWeapon"), Bow))) return false;
			Montage->CompositeSections.Reset();
			const FName Sections[] = { TEXT("Draw"), TEXT("Hold"), TEXT("Release") };
			for (int32 Index = 0; Index < 3; ++Index)
			{
				FCompositeSection Section;
				Section.SectionName = Sections[Index];
				Section.SetTime(Index * 0.5f);
				Montage->CompositeSections.Add(Section);
			}
		}

		const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(FGameplayAbilitySpec(TAbility::StaticClass(), 1, INDEX_NONE, Player));
		TAbility* Ability = Cast<TAbility>(ASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance());
		if (!Test.TestNotNull(TEXT("ASC instanced the consumer"), Ability)) return false;
		ON_SCOPE_EXIT { if (IsValid(ASC)) { ASC->CancelAbilityHandle(Handle); ASC->ClearAbility(Handle); } };
		if (!Test.TestTrue(TEXT("Fixture assigns the authored montage property"), SetFixtureObject(Ability, MontageProperty, Montage))) return false;
		for (const FName EffectProperty : { FName(TEXT("CostGameplayEffectClass")), FName(TEXT("CooldownGameplayEffectClass")),
			FName(TEXT("DamageGameplayEffectClass")), FName(TEXT("StaminaRegenDelayGameplayEffectClass")), FName(TEXT("InvulnerabilityGameplayEffectClass")) })
		{
			SetFixtureObject(Ability, EffectProperty, UGameplayEffect::StaticClass());
		}
		if constexpr (std::is_same_v<TAbility, UBowDrawFireAbility>)
		{
			Ability->SetTestMobileBowMoveSpeedGameplayEffectClass(UTestMobileBowMoveSpeedGE::StaticClass());
		}

		const FGameplayTag CleanupTags[] = {
			FGameplayTag::RequestGameplayTag(TEXT("State.Action.Attacking")),
			FGameplayTag::RequestGameplayTag(TEXT("State.Action.Dodging")),
			FGameplayTag::RequestGameplayTag(TEXT("State.Input.Block.Movement")),
			FGameplayTag::RequestGameplayTag(TEXT("State.Input.Block.Jump")),
			FGameplayTag::RequestGameplayTag(TEXT("State.Action.Charging")),
			FGameplayTag::RequestGameplayTag(TEXT("State.Action.CanCancel.Dodge")) };
		TArray<int32> InitialTagCounts;
		for (const FGameplayTag Tag : CleanupTags) InitialTagCounts.Add(ASC->GetTagCount(Tag));
		const auto Receiver = [&]() { return Ability->GetTestMontageTask(); };
		// Snapshot actual registered callbacks to exercise the old Task's own guards.
		const auto CaptureBegin = [&]()
		{
			return [Callback = ASC->GenericGameplayEventCallbacks.FindChecked(BeginTag)](const FGameplayEventData& Event) { Callback.Broadcast(&Event); };
		};
		const auto CaptureEnd = [&]()
		{
			return [Callback = ASC->GenericGameplayEventCallbacks.FindChecked(EndTag)](const FGameplayEventData& Event) { Callback.Broadcast(&Event); };
		};
		const auto ClearRate = [&]() { if (auto* Task = Receiver()) Task->TestCleanupTask(false); };
		const auto Lifecycle = [&]() -> const FAbilityMontageRateWindowLifecycle& { return Ability->GetTestRateWindowLifecycle(); };
		const auto CheckEnded = [&]()
		{
			Test.TestFalse(TEXT("GAS ended the ability"), Ability->IsActive());
			Test.TestFalse(TEXT("GAS ended the spec"), ASC->FindAbilitySpecFromHandle(Handle)->IsActive());
			Test.TestFalse(TEXT("Rate lifecycle unbound"), Ability->GetTestRateWindowLifecycle().IsBound());
			Test.TestFalse(TEXT("Both RateWindow listeners removed"), Ability->HasTestRateWindowTasks());
			Test.TestNull(TEXT("RateWindow receiver removed"), Receiver());
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(CleanupTags); ++Index)
			{
				Test.TestEqual(TEXT("Action tag restored: ") + CleanupTags[Index].ToString(), ASC->GetTagCount(CleanupTags[Index]), InitialTagCounts[Index]);
			}
		};
		const auto Activate = [&]()
		{
			ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 100.0f);
			if constexpr (std::is_same_v<TAbility, UBowDrawFireAbility>)
			{
				if (!Player->IsCombatInputHeld(PrimaryInput))
				{
					Player->TriggerTestHandleCombatInputStarted(PrimaryInput);
					ASC->CancelAllAbilities(); // Finish only the input-router fixture before the tested activation.
				}
			}
			const bool bActivated = ASC->TryActivateAbility(Handle);
			return Test.TestTrue(TEXT("Real ASC activation keeps the consumer active"), bActivated && Ability->IsActive()
				&& ASC->FindAbilitySpecFromHandle(Handle)->IsActive() && Ability->GetTestRateWindowLifecycle().IsBound());
		};
		if (!Activate()) return false;
		const FAnimMontageInstance* Instance = Anim->GetActiveInstanceForMontage(Montage);
		if (!Test.TestNotNull(TEXT("Actual montage instance exists"), Instance)) return false;
		Test.TestEqual(TEXT("Consumer owns the actual instance ID"), Ability->GetTestRateWindowMontageInstanceID(), Instance->GetInstanceID());
		auto* OldContext = Receiver();
		if (!Test.TestNotNull(TEXT("Bound receiver exists"), OldContext)) return false;
		TStrongObjectPtr<UObject> KeepOldContext(OldContext);
		const auto OldBegin = CaptureBegin();
		const auto OldEnd = CaptureEnd();

		// Explicit fixture rebind reads the actual instance's rate, without overriding gameplay activation.
		auto& InitialLifecycle = Ability->GetTestRateWindowLifecycle_Mutable();
		InitialLifecycle.RestoreAndClear();
		Anim->Montage_SetPlayRate(Montage, 1.25f);
		InitialLifecycle.BindAndCapture(Ability, Anim, Montage, BeginTag, EndTag);
		Test.TestEqual(TEXT("Captured non-unit baseline"), Lifecycle().GetBaselinePlayRate(), 1.25f);
		FAnimNotifyEvent* EventA = FindRateWindowEvent(Montage, 0);
		FAnimNotifyEvent* EventB = FindRateWindowEvent(Montage, 1);
		if (!Test.TestNotNull(TEXT("Notify A"), EventA) || !Test.TestNotNull(TEXT("Notify B"), EventB)) return false;
		auto* NotifyA = CastChecked<UAnimNotifyState_MontageRateWindow>(EventA->NotifyStateClass);
		auto* NotifyB = CastChecked<UAnimNotifyState_MontageRateWindow>(EventB->NotifyStateClass);
		const auto CurrentReference = [&](FAnimNotifyEvent* Event)
		{
			FAnimNotifyEventReference Ref(Event, Montage);
			Ref.AddContextData<UE::Anim::FAnimNotifyMontageInstanceContext>(Ability->GetTestRateWindowMontageInstanceID());
			return Ref;
		};
		const auto BeginA = [&]() { NotifyA->NotifyBegin(Player->GetMesh(), Montage, 1.0f, CurrentReference(EventA)); };
		const auto BeginB = [&]() { NotifyB->NotifyBegin(Player->GetMesh(), Montage, 1.0f, CurrentReference(EventB)); };
		const auto EndA = [&]() { NotifyA->NotifyEnd(Player->GetMesh(), Montage, CurrentReference(EventA)); };
		const auto EndB = [&]() { NotifyB->NotifyEnd(Player->GetMesh(), Montage, CurrentReference(EventB)); };
		const auto Rate = [&](float Expected) { Test.TestEqual(TEXT("Actual montage rate"), Anim->Montage_GetPlayRate(Montage), Expected); };
		BeginA(); Rate(0.5f); BeginB(); Rate(0.2f); EndA(); Rate(0.2f); EndB(); Rate(1.25f);
		BeginA(); BeginB(); EndB(); Rate(0.5f); EndA(); Rate(1.25f);
		// Adjacent windows: both possible orders of the boundary's two notifications.
		BeginA(); EndA(); Rate(1.25f); BeginB(); Rate(0.2f); EndB(); Rate(1.25f);
		BeginA(); BeginB(); EndA(); Rate(0.2f); EndB(); Rate(1.25f);
		BeginA(); BeginA(); EndB(); Rate(0.5f);
		Test.TestEqual(TEXT("Duplicate Begin/unknown End preserve one window"), Lifecycle().GetActiveWindowCount(), 1);
		EndA(); EndA(); Rate(1.25f);

		FGameplayEventData Valid;
		Valid.EventTag = BeginTag;
		Valid.Instigator = Player;
		Valid.Target = Player;
		Valid.OptionalObject = Montage;
		Valid.OptionalObject2 = NotifyA;
		Valid.EventMagnitude = 0.5f;
		Valid.TargetData = FManagedMontageTestHelpers::MakeRateWindowTargetData(Anim, Ability->GetTestRateWindowMontageInstanceID());
		UAnimMontage* Foreign = CreatePlayableRateMontage(Test, World, FString(Name) + TEXT("Foreign"), Montage->GetSkeleton());
		if (!Test.TestNotNull(TEXT("Foreign montage"), Foreign)) return false;
		TArray<FGameplayEventData> Rejected;
		for (float Magnitude : { 0.0f, -1.0f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity() })
		{
			auto Payload = Valid; Payload.EventMagnitude = Magnitude; Rejected.Add(Payload);
		}
		auto Payload = Valid; Payload.OptionalObject2 = nullptr; Rejected.Add(Payload);
		Payload = Valid; Payload.Target = nullptr; Rejected.Add(Payload);
		Payload = Valid; Payload.Instigator = nullptr; Rejected.Add(Payload);
		Payload = Valid; Payload.OptionalObject2 = NewObject<UAnimNotifyState_MontageRateWindow>(World); Rejected.Add(Payload);
		Payload = Valid; Payload.OptionalObject = Foreign; Payload.OptionalObject2 = FindRateWindowEvent(Foreign, 0)->NotifyStateClass; Rejected.Add(Payload);
		for (auto& RejectedPayload : Rejected)
		{
			ASC->HandleGameplayEvent(BeginTag, &RejectedPayload);
			Test.TestEqual(TEXT("Invalid Begin does not create a window"), Lifecycle().GetActiveWindowCount(), 0);
			Rate(1.25f);
		}
		BeginA(); Rate(0.5f); // Prove the same live context still accepts a valid payload.
		Payload = Valid; Payload.EventTag = EndTag; Payload.OptionalObject2 = nullptr;
		ASC->HandleGameplayEvent(EndTag, &Payload); Rate(0.5f);
		Payload = Valid; Payload.EventTag = EndTag; Payload.OptionalObject = Foreign;
		Payload.OptionalObject2 = FindRateWindowEvent(Foreign, 0)->NotifyStateClass;
		ASC->HandleGameplayEvent(EndTag, &Payload); Rate(0.5f);
		EndA(); Rate(1.25f);

		// Embedded Sequence notifies use their own declared identity through the production transport.
		UAnimSequenceBase* Sequence = Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[0].GetAnimReference();
		auto* SequenceNotify = NewObject<UAnimNotifyState_MontageRateWindow>(Sequence);
		SequenceNotify->RateMultiplier = 0.3f;
		FAnimNotifyEvent SequenceEvent;
		SequenceEvent.NotifyStateClass = SequenceNotify;
		Sequence->Notifies.Add(SequenceEvent);
		FAnimNotifyEventReference SequenceRef(&Sequence->Notifies.Last(), Sequence);
		SequenceRef.AddContextData<UE::Anim::FAnimNotifyMontageInstanceContext>(Ability->GetTestRateWindowMontageInstanceID());
		SequenceNotify->NotifyBegin(Player->GetMesh(), Sequence, 1.0f, SequenceRef); Rate(0.3f);
		SequenceNotify->NotifyEnd(Player->GetMesh(), Sequence, SequenceRef); Rate(1.25f);
		UAnimSequenceBase* ForeignSequence = Foreign->SlotAnimTracks[0].AnimTrack.AnimSegments[0].GetAnimReference();
		FAnimNotifyEvent ForeignEvent;
		ForeignEvent.NotifyStateClass = NewObject<UAnimNotifyState_MontageRateWindow>(ForeignSequence);
		ForeignSequence->Notifies.Add(ForeignEvent);
		Payload = Valid; Payload.OptionalObject = ForeignSequence; Payload.OptionalObject2 = ForeignEvent.NotifyStateClass;
		ASC->HandleGameplayEvent(BeginTag, &Payload); Rate(1.25f);
		Payload = Valid; Payload.EventTag = EndTag;
		OldBegin(Payload); Rate(1.25f); // Correct identity, wrong callback tag.


		if constexpr (std::is_same_v<TAbility, UBowDrawFireAbility>)
		{
			// Early input release waits for DrawReady, then uses the real Section jump.
			Player->TriggerTestHandleCombatInputEnded(PrimaryInput);
			Test.TestTrue(TEXT("Early release is buffered"), Ability->GetTestReleaseRequested());
			Payload = Valid; Payload.EventTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Attack.Bow.DrawReady"));
			ASC->HandleGameplayEvent(Payload.EventTag, &Payload);
			Test.TestEqual(TEXT("Bow jumped to Release"), Anim->Montage_GetCurrentSection(Montage), FName(TEXT("Release")));
			Test.TestEqual(TEXT("Section jump does not recapture baseline"), Lifecycle().GetBaselinePlayRate(), 1.25f);
			Test.TestFalse(TEXT("Input release alone does not spawn a projectile"), Ability->GetTestSpawnedProjectile());
		}
		if constexpr (std::is_same_v<TAbility, UDodgeAbility>)
		{
			BeginA();
			const int32 PreviousID = Ability->GetTestRateWindowMontageInstanceID();
			UAnimNotifyState_ActionDodgeCancelWindow* CancelNotify = NewObject<UAnimNotifyState_ActionDodgeCancelWindow>(Montage);
			FAnimNotifyEvent CancelEvent;
			CancelEvent.NotifyStateClass = CancelNotify;
			Montage->Notifies.Add(CancelEvent);
			// Adding a notify can move the array: refresh the earlier rate event pointers.
			EventA = FindRateWindowEvent(Montage, 0);
			EventB = FindRateWindowEvent(Montage, 1);
			FAnimNotifyEventReference CancelRef(&Montage->Notifies.Last(), Montage);
			CancelRef.AddContextData<UE::Anim::FAnimNotifyMontageInstanceContext>(PreviousID);
			CancelNotify->NotifyBegin(Player->GetMesh(), Montage, 1.0f, CancelRef);
			Test.TestTrue(TEXT("Dodge window permits native retrigger"), Ability->GetTestDodgeCancelable());
			if (!Activate()) return false; // True ASC retrigger: old EndAbility, then new activation.
			Test.TestFalse(TEXT("Immediate second Dodge is rejected outside the new window"), ASC->TryActivateAbility(Handle));
			Test.TestTrue(TEXT("Rejected rapid request preserves current Dodge"), Ability->IsActive());
			Test.TestNotEqual(TEXT("Dodge retrigger creates a new montage instance"), Ability->GetTestRateWindowMontageInstanceID(), PreviousID);
			Test.TestEqual(TEXT("Retrigger has its own baseline"), Lifecycle().GetBaselinePlayRate(), 1.0f);
			Payload = Valid; Payload.EventTag = EndTag;
			BeginA(); OldEnd(Payload); Rate(0.5f); EndA(); Rate(1.0f);
		}

		// A new play of the same asset must not inherit writes from the old binding.
		BeginA(); Rate(0.5f);
		auto* ReplacedContext = Receiver();
		TStrongObjectPtr<UObject> KeepReplacedContext(ReplacedContext);
		const auto ReplacedBegin = CaptureBegin();
		const int32 ReplacedID = Ability->GetTestRateWindowMontageInstanceID();
		Test.TestTrue(TEXT("Independent replay starts"), Anim->Montage_Play(Montage, 2.0f) > 0.0f);
		const FAnimMontageInstance* Replacement = Anim->GetActiveInstanceForMontage(Montage);
		if (!Test.TestNotNull(TEXT("Replacement instance exists"), Replacement)) return false;
		Test.TestNotEqual(TEXT("Replacement identity differs from old binding"), Replacement->GetInstanceID(), ReplacedID);
		ReplacedBegin(Valid);
		Rate(2.0f);
		ClearRate(); // Exercise only the consumer's rate cleanup, not its unrelated montage stop.
		Rate(2.0f);
		Test.TestFalse(TEXT("Replacement cleanup removes old binding"), Lifecycle().IsBound());
		ASC->CancelAbilityHandle(Handle);
		CheckEnded();
		if constexpr (std::is_same_v<TAbility, UBowDrawFireAbility>)
		{
			Player->TriggerTestHandleCombatInputStarted(PrimaryInput);
			ASC->CancelAllAbilities(); // Keep held input, finish the separate PrimaryAttack router fixture.
		}
		if (!Activate()) return false;
		Test.TestEqual(TEXT("Next activation captures native baseline"), Lifecycle().GetBaselinePlayRate(), 1.0f);
		BeginA();
		OldBegin(Valid);
		Payload = Valid; Payload.EventTag = EndTag; OldEnd(Payload);
		Rate(0.5f);
		EndA(); Rate(1.0f);
		// End-delegate routing is distinct from a timeline-driven completion test.
		BeginA(); Rate(0.5f);
		if constexpr (std::is_same_v<TAbility, UDodgeAbility> || std::is_same_v<TAbility, UBowDrawFireAbility>)
		{
			FOnMontageEnded* Delegate = Anim->Montage_GetEndedDelegate(Montage);
			if (!Test.TestTrue(TEXT("Production instance end delegate is bound"), Delegate && Delegate->IsBound())) return false;
			const FOnMontageEnded EndDelegate = *Delegate; // Cleanup can remove the original delegate.
			EndDelegate.ExecuteIfBound(Montage, false);
		}
		else
		{
			FAnimMontageInstance* Ending = Anim->GetActiveInstanceForMontage(Montage);
			if (!Test.TestNotNull(TEXT("Natural end instance exists"), Ending)) return false;
			Ending->Stop(FAlphaBlend(0.0f), false);
			Anim->TickMontageOnly(0.01f);
			Anim->DispatchQueuedAnimEvents();
		}
		CheckEnded();
		ClearRate();
		CheckEnded(); // Idempotent cleanup after the normal GAS end.

		if (!Activate()) return false;
		BeginA(); Rate(0.5f);
		ASC->CancelAbilityHandle(Handle);
		CheckEnded(); // Cancellation begins with a live window, not an already-cleared fixture.

		if (!Activate()) return false;
		BeginA(); Rate(0.5f);
		const int32 InterruptedInstanceID = Ability->GetTestRateWindowMontageInstanceID();
		FAnimMontageInstance* InterruptedInstance = Anim->GetMontageInstanceForID(InterruptedInstanceID);
		if (!Test.TestNotNull(TEXT("Interrupted instance exists before stop"), InterruptedInstance)) return false;
		Anim->Montage_Stop(0.0f, Montage);
		Test.TestTrue(TEXT("Interrupted instance received the stop"), InterruptedInstance->IsStopped());
		// Use the engine update so Ended is queued until Terminate invalidates the instance.
		// Calling FAnimMontageInstance::Advance directly can broadcast while it is still valid.
		Anim->TickMontageOnly(0.01f);
		Anim->DispatchQueuedAnimEvents();
		CheckEnded(); // Real stop plus queued engine dispatch delivers the end callback.

		if constexpr (std::is_same_v<TAbility, UPlayerMeleeSkillAbility>)
		{
			if (!Activate()) return false;
			BeginA(); Rate(0.5f);
			Player->TriggerTestUnPossessed();
			CheckEnded(); // This consumer opts in to the existing OnUnpossess cancellation selector.
		}

		// Playback failure must converge without creating RateWindow tasks or state.
		UAnimMontage* EmptyMontage = NewObject<UAnimMontage>(World);
		EmptyMontage->SetSkeleton(Montage->GetSkeleton());
		EmptyMontage->CompositeSections = Montage->CompositeSections;
		SetFixtureObject(Ability, MontageProperty, EmptyMontage);
		ASC->TryActivateAbility(Handle);
		CheckEnded();
		SetFixtureObject(Ability, MontageProperty, Montage);
		if constexpr (std::is_same_v<TAbility, UPlayerMeleeSkillAbility>)
		{
			// Use a separate actor: zero stamina intentionally leaves its native exhaustion state active.
			APlayerCharacter* FailurePlayer = FCombatAutomationFixture::SpawnPlayer(World);
			if (!Test.TestNotNull(TEXT("Commit-failure player exists"), FailurePlayer)) return false;
			UAbilitySystemComponent* FailureASC = FailurePlayer->GetAbilitySystemComponent();
			FailurePlayer->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			UAnimInstance* FailureAnim = NewObject<UAnimInstance>(FailurePlayer->GetMesh());
			FailureAnim->InitializeMontageOnly();
			FailureAnim->CurrentSkeleton = Montage->GetSkeleton();
			FailurePlayer->GetMesh()->AnimScriptInstance = FailureAnim;
			FailureASC->RefreshAbilityActorInfo();
			const FGameplayAbilitySpecHandle FailureHandle = FailureASC->GiveAbility(FGameplayAbilitySpec(TAbility::StaticClass(), 1, INDEX_NONE, FailurePlayer));
			TAbility* FailureAbility = Cast<TAbility>(FailureASC->FindAbilitySpecFromHandle(FailureHandle)->GetPrimaryInstance());
			if (!Test.TestNotNull(TEXT("Commit-failure ability exists"), FailureAbility)) return false;
			SetFixtureObject(FailureAbility, MontageProperty, Montage);
			for (const FName EffectProperty : { FName(TEXT("CostGameplayEffectClass")), FName(TEXT("CooldownGameplayEffectClass")),
				FName(TEXT("DamageGameplayEffectClass")), FName(TEXT("StaminaRegenDelayGameplayEffectClass")) })
			{
				SetFixtureObject(FailureAbility, EffectProperty, UGameplayEffect::StaticClass());
			}
			// PreActivate notifies after CanActivate, before ActivateAbility: force the real Commit check to fail.
			bool bCommitFailureActivationReached = false;
			const FDelegateHandle ActivationHook = FailureASC->AbilityActivatedCallbacks.AddLambda([&](UGameplayAbility* Activated)
			{
				if (Activated == FailureAbility)
				{
					bCommitFailureActivationReached = true;
					FailureASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 0.0f);
				}
			});
			FailureASC->TryActivateAbility(FailureHandle);
			FailureASC->AbilityActivatedCallbacks.Remove(ActivationHook);
			Test.TestTrue(TEXT("Commit failure was reached after CanActivate succeeded"), bCommitFailureActivationReached);
			Test.TestFalse(TEXT("Commit failure ends the ability"), FailureAbility->IsActive());
			Test.TestFalse(TEXT("Commit failure ends the spec"), FailureASC->FindAbilitySpecFromHandle(FailureHandle)->IsActive());
			Test.TestFalse(TEXT("Commit failure leaves no rate binding"), FailureAbility->GetTestRateWindowLifecycle().IsBound());
			Test.TestFalse(TEXT("Commit failure leaves no listeners"), FailureAbility->HasTestRateWindowTasks());
			Test.TestNull(TEXT("Commit failure leaves no Task"), FailureAbility->GetTestMontageTask());
			for (const FGameplayTag Tag : CleanupTags)
			{
				Test.TestEqual(TEXT("Commit failure leaves no action tag: ") + Tag.ToString(), FailureASC->GetTagCount(Tag), 0);
			}
			FailureASC->ClearAbility(FailureHandle);
			FailurePlayer->Destroy();
		}

		// =====================================================================
		// SHRINK: Shared FMontageRateWindowBinding Rebind, Failure, and Inactive Rejection
		// =====================================================================
		if (!Activate()) return false;
		auto* FirstBoundContext = Receiver();
		if (!Test.TestNotNull(TEXT("SHRINK: Active ability has valid context"), FirstBoundContext)) return false;
		TStrongObjectPtr<UObject> KeepFirstBoundContext(FirstBoundContext);

		{
			const auto FirstBegin = CaptureBegin();
			const auto FirstEnd = CaptureEnd();
			const int32 FirstID = Ability->GetTestRateWindowMontageInstanceID();
			ASC->CancelAbilityHandle(Handle);
			if (!Activate()) return false;
			auto* SecondTask = Receiver();
			Test.TestNotNull(TEXT("SHRINK: Reactivation produces valid new Task"), SecondTask);
			Test.TestTrue(TEXT("SHRINK: Reactivation generates independent Task instance"), SecondTask != FirstBoundContext);
			Test.TestNotEqual(TEXT("SHRINK: Reactivation generates independent montage instance"), Ability->GetTestRateWindowMontageInstanceID(), FirstID);
			Test.TestTrue(TEXT("SHRINK: Reactivation maintains active rate lifecycle"), Lifecycle().IsBound());
			Test.TestTrue(TEXT("SHRINK: Reactivation maintains active listeners"), Ability->HasTestRateWindowTasks());
			Test.TestTrue(TEXT("SHRINK: Old Task terminated"), FirstBoundContext->IsTerminated());
			FGameplayEventData Current = Valid;
			Current.TargetData = FManagedMontageTestHelpers::MakeRateWindowTargetData(Anim, Ability->GetTestRateWindowMontageInstanceID());
			FirstBegin(Current); // Even a new valid source cannot revive the old callback.
			Test.TestEqual(TEXT("SHRINK: Stale first Task callback rejected, window count remains 0"), Lifecycle().GetActiveWindowCount(), 0);
			CaptureBegin()(Current);
			Test.TestEqual(TEXT("SHRINK: Second Task valid callback accepted, window count is 1"), Lifecycle().GetActiveWindowCount(), 1);
			Current.EventTag = EndTag;
			FirstEnd(Current);
			Test.TestEqual(TEXT("SHRINK: Stale end cannot close the new window"), Lifecycle().GetActiveWindowCount(), 1);
			CaptureEnd()(Current);
			Test.TestEqual(TEXT("SHRINK: Second Task valid end callback accepted, window count is 0"), Lifecycle().GetActiveWindowCount(), 0);

			// Binding is now part of activation: a missing AnimInstance must fail closed there.
			ASC->CancelAbilityHandle(Handle);
			Player->GetMesh()->AnimScriptInstance = nullptr;
			ASC->RefreshAbilityActorInfo();
			ASC->TryActivateAbility(Handle);
			CheckEnded();
			Player->GetMesh()->AnimScriptInstance = Anim;
			ASC->RefreshAbilityActorInfo();
			// No new subscription may be created by late callbacks on an ended ability.
			FirstBegin(Current);
			FirstEnd(Current);
			Test.TestFalse(TEXT("SHRINK: Inactive Task rejects binding recreation"), FirstBoundContext->GetRateWindowLifecycle().IsBound());
			CheckEnded();
		}

		{
			// Same controlled animation driver as Light 7.9 and Managed Cancel section 9.
			USkeletalMeshComponent* Mesh = Player->GetMesh();
			UCharacterMovementComponent* Movement = Player->GetCharacterMovement();
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
			Anim->Montage_Stop(0.0f);
			Advance(0.001f);
			// Separate memory montage keeps the earlier white-box notify pointers intact.
			UAnimMontage* Timeline = CreatePlayableRateMontage(Test, World, FString(Name) + TEXT("Timeline"), Montage->GetSkeleton());
			if (!Test.TestNotNull(TEXT("Timeline montage exists"), Timeline)) return false;
			if constexpr (std::is_same_v<TAbility, UBowDrawFireAbility>)
			{
				Timeline->CompositeSections = Montage->CompositeSections;
				Timeline->CompositeSections[0].NextSectionName = TEXT("Hold");
				Timeline->CompositeSections[1].NextSectionName = TEXT("Release");
			}
			Timeline->Notifies.Reset();
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
			auto* TimelineRate = NewObject<UAnimNotifyState_MontageRateWindow>(Timeline);
			TimelineRate->RateMultiplier = 0.5f;
			AddWindow(TimelineRate, 0.1f, 0.2f);
			AddWindow(NewObject<UAnimNotifyState_ActionDodgeCancelWindow>(Timeline), 0.1f, 0.3f);
			Timeline->SortNotifies();
			SetFixtureObject(Ability, MontageProperty, Timeline);
			ON_SCOPE_EXIT { SetFixtureObject(Ability, MontageProperty, Montage); };
			const FGameplayTag DodgePermission = FGameplayTag::RequestGameplayTag(TEXT("State.Action.CanCancel.Dodge"));
			const FGameplayTag DefensePermission = FGameplayTag::RequestGameplayTag(TEXT("State.Action.CanCancel.Defense"));
			for (int32 Exit = 0; Exit < 4; ++Exit)
			{
				if (!Activate()) return false;
				auto* Task = Receiver();
				TStrongObjectPtr<UAbilityTask_PlayActionMontage> KeepTask(Task);
				Test.TestEqual(TEXT("Timeline: declared cancel policy"), Task->GetCancelPolicy(), std::is_same_v<TAbility, UDodgeAbility>
					? EActionMontageCancelPolicy::DodgeOnly : EActionMontageCancelPolicy::DodgeAndDefense);
				Test.TestFalse(TEXT("Timeline: no bypass"), Task->GetTestBypassMontageActiveCheck());
				Test.TestEqual(TEXT("Timeline: starts at zero"), Anim->Montage_GetPosition(Timeline), 0.0f);
				Test.TestEqual(TEXT("Timeline: Root Motion scale"), Player->GetAnimRootMotionTranslationScale(), 1.0f);
				Test.TestFalse(TEXT("Timeline: no Dodge permission outside window"), ASC->HasMatchingGameplayTag(DodgePermission));
				Test.TestFalse(TEXT("Timeline: no Defense permission outside window"), ASC->HasMatchingGameplayTag(DefensePermission));
				const int32 ID = Task->GetBoundMontageInstanceID();
				Advance(0.15f);
				Test.TestEqual(TEXT("Timeline: native Rate Begin"), Anim->Montage_GetPlayRate(Timeline), 0.5f);
				Test.TestEqual(TEXT("Timeline: native Cancel Begin"), Task->GetActiveCancelWindowCount(), 1);
				Test.TestTrue(TEXT("Timeline: Dodge permission"), ASC->HasMatchingGameplayTag(DodgePermission));
				Test.TestEqual(TEXT("Timeline: Defense follows declared policy"), ASC->HasMatchingGameplayTag(DefensePermission), !std::is_same_v<TAbility, UDodgeAbility>);
				if (Exit == 0)
				{
					// RateWindow slows montage time. Check an actual timeline milestone,
					// with a finite frame budget, before asserting that Cancel End was dispatched.
					constexpr float AfterCancelWindow = 0.35f; // Authored Cancel End is 0.3s.
					for (int32 Step = 0; Step < 12 && Anim->Montage_GetPosition(Timeline) < AfterCancelWindow; ++Step)
					{
						Advance(0.05f);
					}
					if (!Test.TestTrue(TEXT("Timeline: playback crossed Cancel End while the same action remains active"),
						Anim->Montage_GetPosition(Timeline) >= AfterCancelWindow
						&& Ability->IsActive() && Receiver() == Task && !Task->IsTerminated())) return false;
					Test.TestEqual(TEXT("Timeline: native Rate End restores baseline"), Anim->Montage_GetPlayRate(Timeline), 1.0f);
					Test.TestEqual(TEXT("Timeline: native Cancel End"), Task->GetActiveCancelWindowCount(), 0);
					Test.TestFalse(TEXT("Timeline: Dodge removed at End"), ASC->HasMatchingGameplayTag(DodgePermission));
					Test.TestFalse(TEXT("Timeline: Defense removed at End"), ASC->HasMatchingGameplayTag(DefensePermission));
					Advance(3.0f);
				}
				else if (Exit == 2)
				{
					Anim->Montage_Stop(0.2f, Timeline);
					if constexpr (std::is_same_v<TAbility, UPlayerMeleeSkillAbility>)
						Test.TestTrue(TEXT("Timeline: MeleeSkill remains active through interrupted blend"), Ability->IsActive());
					else Test.TestFalse(TEXT("Timeline: consumer ends at interrupted blend start"), Ability->IsActive());
					Advance(0.25f);
				}
				else
				{
					ASC->CancelAbilityHandle(Handle);
					if constexpr (std::is_same_v<TAbility, UBowDrawFireAbility>)
					{
						auto* Stopped = Anim->GetMontageInstanceForID(ID);
						if (!Test.TestNotNull(TEXT("Timeline: Bow stopped instance"), Stopped)) return false;
						Test.TestTrue(TEXT("Timeline: Bow stop emitted"), Stopped->IsStopped());
						Test.TestEqual(TEXT("Timeline: Bow preserves 0.1s stop blend"), Stopped->GetBlendTime(), 0.1f);
					}
					if (Exit == 3)
					{
						if (!Activate()) return false;
						Test.TestTrue(TEXT("Timeline: reactivation has new Task"), Receiver() != Task);
						Test.TestNotEqual(TEXT("Timeline: reactivation has new instance"), Receiver()->GetBoundMontageInstanceID(), ID);
						Advance(0.15f);
						Test.TestTrue(TEXT("Timeline: old tail cannot end new activation"), Ability->IsActive());
						Test.TestEqual(TEXT("Timeline: new window survives old tail"), Anim->Montage_GetPlayRate(Timeline), 0.5f);
						ASC->CancelAbilityHandle(Handle);
					}
				}
				CheckEnded();
				Test.TestTrue(TEXT("Timeline: old Task terminated"), Task->IsTerminated());
				Test.TestFalse(TEXT("Timeline: old windows cleared"), Task->GetRateWindowLifecycle().IsBound());
				Test.TestFalse(TEXT("Timeline: no residual Defense contribution"), ASC->HasMatchingGameplayTag(DefensePermission));
				Advance(0.3f);
			}
		}

		if (!Activate()) return false;
		BeginA(); Rate(0.5f);
		auto* DestroyedContext = Receiver();
		TStrongObjectPtr<UObject> KeepDestroyedContext(DestroyedContext);
		const auto DestroyedBegin = CaptureBegin();
		const auto DestroyedEnd = CaptureEnd();
		TStrongObjectPtr<TAbility> KeepAbility(Ability);
		Player->Destroy();
		Test.TestFalse(TEXT("Destroy ends the ability"), Ability->IsActive());
		Test.TestFalse(TEXT("Destroy clears the rate lifecycle"), Lifecycle().IsBound());
		Test.TestFalse(TEXT("Destroy removes listeners"), Ability->HasTestRateWindowTasks());
		{
			Test.TestTrue(TEXT("Destroy terminates callback owner"), DestroyedContext->IsTerminated());
			Test.TestNull(TEXT("Destroy clears callback animation owner"), DestroyedContext->GetBoundAnimInstance());
			Test.TestEqual(TEXT("Destroy invalidates callback instance"), DestroyedContext->GetBoundMontageInstanceID(), INDEX_NONE);
		}
		DestroyedBegin(Valid);
		Payload = Valid; Payload.EventTag = EndTag; DestroyedEnd(Payload);
		Test.TestFalse(TEXT("Late callbacks cannot recreate a destroyed binding"), Lifecycle().IsBound());
		Test.AddInfo(FString::Printf(TEXT("%s: real ASC activation, identity windows, phase checks and cleanup executed"), Name));
		return true;
	}

	bool RunPlayerBigHitReactionWindowsTest(FAutomationTestBase& Test)
	{
		FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		WorldContext.SetCurrentWorld(World);
		FTestWorldScope Cleanup{ World };
		if (!Test.TestNotNull(TEXT("BigReaction world exists"), World)) return false;

		FURL URL;
		World->InitializeActorsForPlay(URL);
		World->BeginPlay();

		APlayerCharacter* Player = FCombatAutomationFixture::SpawnPlayer(World, FTransform(FRotator::ZeroRotator, FVector::ZeroVector));
		AEnemyCharacter* Enemy = FCombatAutomationFixture::SpawnPassiveEnemy(World, FTransform(FRotator::ZeroRotator, FVector(200.0f, 0.0f, 0.0f)));
		if (!Test.TestNotNull(TEXT("Player exists"), Player) || !Test.TestNotNull(TEXT("Enemy exists"), Enemy)) return false;

		UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
		if (!Test.TestNotNull(TEXT("Player ASC exists"), ASC)) return false;

		UCharacterMovementComponent* Movement = Player->GetCharacterMovement();
		if (!Test.TestNotNull(TEXT("Movement component exists"), Movement)) return false;
		Movement->SetMovementMode(MOVE_Walking);
		ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 100.0f);
		ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxStaminaAttribute(), 100.0f);

		UAnimMontage* MontageFront = CreatePlayableRateMontage(Test, World, TEXT("BigHit_F"));
		USkeleton* SharedSkeleton = MontageFront ? MontageFront->GetSkeleton() : nullptr;
		UAnimMontage* MontageBack = CreatePlayableRateMontage(Test, World, TEXT("BigHit_B"), SharedSkeleton);
		UAnimMontage* MontageLeft = CreatePlayableRateMontage(Test, World, TEXT("BigHit_L"), SharedSkeleton);
		UAnimMontage* MontageRight = CreatePlayableRateMontage(Test, World, TEXT("BigHit_R"), SharedSkeleton);
		UAnimMontage* DodgeMontage = CreatePlayableRateMontage(Test, World, TEXT("Dodge_Synth"), SharedSkeleton);
		UAnimMontage* ForeignMontage = CreateDummyMontage(World);

		if (!Test.TestNotNull(TEXT("MontageFront exists"), MontageFront)
			|| !Test.TestNotNull(TEXT("MontageBack exists"), MontageBack)
			|| !Test.TestNotNull(TEXT("MontageLeft exists"), MontageLeft)
			|| !Test.TestNotNull(TEXT("MontageRight exists"), MontageRight)
			|| !Test.TestNotNull(TEXT("DodgeMontage exists"), DodgeMontage))
		{
			return false;
		}

		FAnimNotifyEvent* RateEventFrontA = FindRateWindowEvent(MontageFront, 0);
		if (RateEventFrontA)
		{
			RateEventFrontA->SetTime(0.05f);
			RateEventFrontA->SetDuration(0.5f);
			RateEventFrontA->Link(MontageFront, 0.05f);
			RateEventFrontA->EndLink.Link(MontageFront, 0.55f);
		}

		UAnimNotifyState_ActionDodgeCancelWindow* FrontCancelNotify = NewObject<UAnimNotifyState_ActionDodgeCancelWindow>(MontageFront, TEXT("DodgeCancelFront"));
		FAnimNotifyEvent FrontCancelEvent;
		FrontCancelEvent.NotifyStateClass = FrontCancelNotify;
		FrontCancelEvent.SetTime(0.15f);
		FrontCancelEvent.SetDuration(0.5f);
		FrontCancelEvent.Link(MontageFront, 0.15f);
		FrontCancelEvent.EndLink.Link(MontageFront, 0.65f);
		MontageFront->Notifies.Add(FrontCancelEvent);

		for (UAnimMontage* DirectionMontage : { MontageBack, MontageLeft, MontageRight })
		{
			if (FAnimNotifyEvent* SiblingRateEvent = FindRateWindowEvent(DirectionMontage, 0))
			{
				SiblingRateEvent->SetTime(0.05f);
				SiblingRateEvent->SetDuration(0.5f);
				SiblingRateEvent->Link(DirectionMontage, 0.05f);
				SiblingRateEvent->EndLink.Link(DirectionMontage, 0.55f);
			}

			UAnimNotifyState_ActionDodgeCancelWindow* SiblingCancelNotify = NewObject<UAnimNotifyState_ActionDodgeCancelWindow>(DirectionMontage, TEXT("DodgeCancelSibling"));
			FAnimNotifyEvent SiblingCancelEvent;
			SiblingCancelEvent.NotifyStateClass = SiblingCancelNotify;
			SiblingCancelEvent.SetTime(0.15f);
			SiblingCancelEvent.SetDuration(0.5f);
			SiblingCancelEvent.Link(DirectionMontage, 0.15f);
			SiblingCancelEvent.EndLink.Link(DirectionMontage, 0.65f);
			DirectionMontage->Notifies.Add(SiblingCancelEvent);
		}

		UAnimInstance* Anim = NewObject<UAnimInstance>(Player->GetMesh());
		Anim->InitializeMontageOnly();
		Anim->CurrentSkeleton = MontageFront->GetSkeleton();
		Player->GetMesh()->AnimScriptInstance = Anim;
		ASC->RefreshAbilityActorInfo();

		const FGameplayAbilitySpecHandle DodgeHandle = ASC->GiveAbility(FGameplayAbilitySpec(UDodgeAbility::StaticClass(), 1, INDEX_NONE, Player));
		UDodgeAbility* DodgeAbility = Cast<UDodgeAbility>(ASC->FindAbilitySpecFromHandle(DodgeHandle)->GetPrimaryInstance());
		if (!Test.TestNotNull(TEXT("Dodge ability instanced"), DodgeAbility)) return false;

		SetFixtureObject(DodgeAbility, TEXT("DodgeMontage"), DodgeMontage);
		for (const FName EffectProperty : { FName(TEXT("CostGameplayEffectClass")), FName(TEXT("StaminaRegenDelayGameplayEffectClass")), FName(TEXT("InvulnerabilityGameplayEffectClass")) })
		{
			SetFixtureObject(DodgeAbility, EffectProperty, UGameplayEffect::StaticClass());
		}

		const FGameplayAbilitySpecHandle BigReactionHandle = ASC->GiveAbility(FGameplayAbilitySpec(UPlayerBigHitReactionAbility::StaticClass(), 1, INDEX_NONE, Player));
		UPlayerBigHitReactionAbility* BigReactionAbility = Cast<UPlayerBigHitReactionAbility>(ASC->FindAbilitySpecFromHandle(BigReactionHandle)->GetPrimaryInstance());
		if (!Test.TestNotNull(TEXT("BigReaction ability instanced"), BigReactionAbility)) return false;

		BigReactionAbility->SetTestMontages(MontageFront, MontageBack, MontageLeft, MontageRight);

		ON_SCOPE_EXIT
		{
			if (IsValid(ASC))
			{
				ASC->ClearAbility(DodgeHandle);
				ASC->ClearAbility(BigReactionHandle);
			}
		};

		const FGameplayTag TagEventPlayerBig = FGameplayTag::RequestGameplayTag(TEXT("Event.Reaction.Player.Big"));
		const FGameplayTag TagHitReacting = FGameplayTag::RequestGameplayTag(TEXT("State.Action.HitReacting"));
		const FGameplayTag TagCanCancelDodge = FGameplayTag::RequestGameplayTag(TEXT("State.Action.CanCancel.Dodge"));
		const FGameplayTag TagCancelBegin = FGameplayTag::RequestGameplayTag(TEXT("Event.Action.CancelWindow.Dodge.Begin"));
		const FGameplayTag TagCancelEnd = FGameplayTag::RequestGameplayTag(TEXT("Event.Action.CancelWindow.Dodge.End"));
		const FGameplayTag TagRateBegin = FGameplayTag::RequestGameplayTag(TEXT("Event.Action.RateWindow.Begin"));
		const FGameplayTag TagRateEnd = FGameplayTag::RequestGameplayTag(TEXT("Event.Action.RateWindow.End"));

		auto ActivateBigReaction = [&](const FVector& EnemyRelativeOffset) -> bool
		{
			ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 100.0f);
			const FGameplayTag TagExhausted = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Exhausted")), false);
			if (TagExhausted.IsValid())
			{
				ASC->SetLooseGameplayTagCount(TagExhausted, 0);
			}
			Movement->SetMovementMode(MOVE_Walking);
			Enemy->SetActorLocation(Player->GetActorLocation() + EnemyRelativeOffset);
			FGameplayEventData TriggerData;
			TriggerData.EventTag = TagEventPlayerBig;
			TriggerData.Instigator = Enemy;
			TriggerData.Target = Player;
			const int32 ActivatedCount = ASC->HandleGameplayEvent(TagEventPlayerBig, &TriggerData);
			return ActivatedCount > 0 && BigReactionAbility->IsActive();
		};

		const auto GetMontagePlayRate = [&](UAnimMontage* M) -> float
		{
			const FAnimMontageInstance* Inst = Anim->GetActiveInstanceForMontage(M);
			return Inst ? Inst->GetPlayRate() : 0.0f;
		};

		USkeletalMeshComponent* Mesh = Player->GetMesh();
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
		const auto AdvanceMontage = [&](float Seconds)
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

		auto ActivateAndOpenWindows = [&]() -> bool
		{
			if (!ActivateBigReaction(FVector(200.0f, 0.0f, 0.0f)))
			{
				return false;
			}
			AdvanceMontage(0.08f);
			AdvanceMontage(0.20f);
			return BigReactionAbility->IsActive()
				&& BigReactionAbility->GetTestDodgeCancelable()
				&& ASC->HasMatchingGameplayTag(TagCanCancelDodge)
				&& FMath::IsNearlyEqual(GetMontagePlayRate(MontageFront), 0.5f, 0.01f);
		};

		// ---------------------------------------------------------------------
		// 1. Four-Way Selection & Initial Window-Closed Rejection
		// ---------------------------------------------------------------------
		Test.TestTrue(TEXT("1.1: Activate Front Big Reaction"), ActivateBigReaction(FVector(200.0f, 0.0f, 0.0f)));
		Test.TestEqual(TEXT("1.2: Front montage selected"), BigReactionAbility->GetTestActiveMontage(), MontageFront);
		auto* InitialTask = BigReactionAbility->GetTestMontageTask();
		if (!Test.TestNotNull(TEXT("1.2a: Big uses standard Task"), InitialTask)) return false;
		Test.TestEqual(TEXT("1.2b: Big policy"), InitialTask->GetCancelPolicy(), EActionMontageCancelPolicy::DodgeOnly);
		Test.TestFalse(TEXT("1.2c: Big has no playback bypass"), InitialTask->GetTestBypassMontageActiveCheck());
		Test.TestEqual(TEXT("1.2d: Big starts at zero"), Anim->Montage_GetPosition(MontageFront), 0.0f);
		Test.TestEqual(TEXT("1.2e: Big Root Motion scale"), Player->GetAnimRootMotionTranslationScale(), 1.0f);
		Test.TestTrue(TEXT("1.3: ASC owns HitReacting tag"), ASC->HasMatchingGameplayTag(TagHitReacting));
		Test.TestFalse(TEXT("1.4: DodgeCancelable initially false"), BigReactionAbility->GetTestDodgeCancelable());
		Test.TestFalse(TEXT("1.5: ASC does not have CanCancel.Dodge yet"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));

		// Outside window: Dodge activation is rejected
		const bool bDodgeOutsideResult = ASC->TryActivateAbility(DodgeHandle);
		Test.TestFalse(TEXT("1.6: Dodge rejected outside cancel window"), bDodgeOutsideResult);
		Test.TestTrue(TEXT("1.7: Big reaction remains active"), BigReactionAbility->IsActive());

		// ---------------------------------------------------------------------
		// 2. Real Time Advancement & Automatic Notify Delivery (P2-1)
		// ---------------------------------------------------------------------
		AdvanceMontage(0.08f);
		if (!Test.TestEqual(TEXT("2.1: Time advance triggered RateWindow (play rate 0.5f)"), GetMontagePlayRate(MontageFront), 0.5f)) return false;
		Test.TestFalse(TEXT("2.2: Cancel window still closed before its time threshold"), BigReactionAbility->GetTestDodgeCancelable());
		Test.TestFalse(TEXT("2.3: Dodge still rejected when only RateWindow is active"), ASC->TryActivateAbility(DodgeHandle));

		AdvanceMontage(0.20f);
		if (!Test.TestTrue(TEXT("2.4: Time advance opened cancel window"), BigReactionAbility->GetTestDodgeCancelable())) return false;
		Test.TestTrue(TEXT("2.5: ASC has CanCancel.Dodge tag"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
		Test.TestFalse(TEXT("2.6: Big never grants Defense"), ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Action.CanCancel.Defense"))));

		// ---------------------------------------------------------------------
		// 3. In-Window Dodge Cancels Big Reaction (End-to-End Closure)
		// ---------------------------------------------------------------------
		const bool bDodgeInWindowResult = ASC->TryActivateAbility(DodgeHandle);
		Test.TestTrue(TEXT("3.1: Dodge activated successfully in cancel window"), bDodgeInWindowResult);
		Test.TestFalse(TEXT("3.2: Big reaction cancelled by dodge"), BigReactionAbility->IsActive());
		Test.TestFalse(TEXT("3.3: HitReacting state cleared"), ASC->HasMatchingGameplayTag(TagHitReacting));
		Test.TestEqual(TEXT("3.4: CanCancel.Dodge count cleared to 0"), ASC->GetTagCount(TagCanCancelDodge), 0);
		Test.TestFalse(TEXT("3.5: Rate lifecycle cleared"), BigReactionAbility->GetTestRateWindowLifecycle().IsBound());
		Test.TestFalse(TEXT("3.6: Cancel tasks cleared"), BigReactionAbility->GetTestMontageTask() != nullptr);

		ASC->CancelAbilityHandle(DodgeHandle);

		// ---------------------------------------------------------------------
		// 4. Verification across Back, Left, Right Directions
		// ---------------------------------------------------------------------
		struct FDirectionTestCase
		{
			const TCHAR* Label;
			FVector Offset;
			UAnimMontage* ExpectedMontage;
		};
		const FDirectionTestCase SiblingDirections[] = {
			{ TEXT("Back"), FVector(-200.0f, 0.0f, 0.0f), MontageBack },
			{ TEXT("Left"), FVector(0.0f, -200.0f, 0.0f), MontageLeft },
			{ TEXT("Right"), FVector(0.0f, 200.0f, 0.0f), MontageRight }
		};

		for (const auto& Case : SiblingDirections)
		{
			Test.TestTrue(FString::Printf(TEXT("4.1 [%s]: Activate"), Case.Label), ActivateBigReaction(Case.Offset));
			Test.TestEqual(FString::Printf(TEXT("4.2 [%s]: Correct montage selected"), Case.Label),
				BigReactionAbility->GetTestActiveMontage(), Case.ExpectedMontage);

			AdvanceMontage(0.28f);

			Test.TestTrue(FString::Printf(TEXT("4.3 [%s]: Window open"), Case.Label), BigReactionAbility->GetTestDodgeCancelable());
			Test.TestTrue(FString::Printf(TEXT("4.4 [%s]: Dodge cancels reaction"), Case.Label), ASC->TryActivateAbility(DodgeHandle));
			Test.TestFalse(FString::Printf(TEXT("4.5 [%s]: Big reaction ended"), Case.Label), BigReactionAbility->IsActive());
			Test.TestEqual(FString::Printf(TEXT("4.6 [%s]: CanCancel.Dodge cleared"), Case.Label), ASC->GetTagCount(TagCanCancelDodge), 0);

			ASC->CancelAbilityHandle(DodgeHandle);
		}

		// ---------------------------------------------------------------------
		// 5. Activation Cost Rejection vs True Commit Failure (P2-2)
		// ---------------------------------------------------------------------
		Test.TestTrue(TEXT("5.1: Reactivate Front for failure boundary tests"), ActivateBigReaction(FVector(200.0f, 0.0f, 0.0f)));
		AdvanceMontage(0.28f);
		Test.TestTrue(TEXT("5.2: Cancel window open"), BigReactionAbility->GetTestDodgeCancelable());

		// 5A: CanActivate Cost Rejection (Stamina = 0 beforehand)
		ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 0.0f);
		const bool bDodgeCostRejectResult = ASC->TryActivateAbility(DodgeHandle);
		Test.TestFalse(TEXT("5.3: Dodge rejected by CanActivate cost check when stamina is zero"), bDodgeCostRejectResult);
		Test.TestTrue(TEXT("5.4: Big reaction retained on CanActivate cost rejection"), BigReactionAbility->IsActive());
		Test.TestTrue(TEXT("5.5: HitReacting tag preserved"), ASC->HasMatchingGameplayTag(TagHitReacting));

		// 5B: True Commit Failure (CanActivate succeeds with 100 stamina; hook drops stamina before ActivateAbility commits)
		ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 100.0f);
		const FGameplayTag TagExhausted = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Exhausted")), false);
		if (TagExhausted.IsValid())
		{
			ASC->SetLooseGameplayTagCount(TagExhausted, 0);
		}
		bool bCommitFailureReached = false;
		const FDelegateHandle ActivationHook = ASC->AbilityActivatedCallbacks.AddLambda([&](UGameplayAbility* Activated)
		{
			if (Activated == DodgeAbility)
			{
				bCommitFailureReached = true;
				ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 0.0f);
			}
		});
		ASC->TryActivateAbility(DodgeHandle);
		ASC->AbilityActivatedCallbacks.Remove(ActivationHook);

		Test.TestTrue(TEXT("5.6: Commit failure was reached after CanActivate succeeded"), bCommitFailureReached);
		Test.TestFalse(TEXT("5.7: Dodge spec ended on commit failure"), ASC->FindAbilitySpecFromHandle(DodgeHandle)->IsActive());
		Test.TestFalse(TEXT("5.8: Dodge ended on commit failure"), DodgeAbility->IsActive());
		Test.TestTrue(TEXT("5.9: Big reaction retained on Dodge Commit failure"), BigReactionAbility->IsActive());
		Test.TestTrue(TEXT("5.10: HitReacting tag preserved on Dodge Commit failure"), ASC->HasMatchingGameplayTag(TagHitReacting));
		Test.TestTrue(TEXT("5.11: Cancel window remains open after failed Dodge commit"), BigReactionAbility->GetTestDodgeCancelable());

		// Restore stamina and close window
		ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 100.0f);
		if (TagExhausted.IsValid())
		{
			ASC->SetLooseGameplayTagCount(TagExhausted, 0);
		}
		FAnimNotifyEventReference WindowRefEnd(&FrontCancelEvent, MontageFront);
		WindowRefEnd.AddContextData<UE::Anim::FAnimNotifyMontageInstanceContext>(BigReactionAbility->GetTestRateWindowMontageInstanceID());
		FrontCancelNotify->NotifyEnd(Player->GetMesh(), MontageFront, WindowRefEnd);
		Test.TestFalse(TEXT("5.12: Window closed after NotifyEnd"), BigReactionAbility->GetTestDodgeCancelable());

		// ---------------------------------------------------------------------
		// 6. Idempotence & Foreign Source Filtering (P2-2)
		// ---------------------------------------------------------------------
		// 6A: In CLOSED state, verify illegal Begin payloads are rejected and do NOT open the window
		Test.TestFalse(TEXT("6.1: Cancel window initially closed"), BigReactionAbility->GetTestDodgeCancelable());
		Test.TestEqual(TEXT("6.2: Initial tag count is 0"), ASC->GetTagCount(TagCanCancelDodge), 0);

		FGameplayEventData ForeignPayload;
		ForeignPayload.EventTag = TagCancelBegin;
		ForeignPayload.Instigator = Player;
		ForeignPayload.Target = Player;
		ForeignPayload.OptionalObject = ForeignMontage;
		ForeignPayload.OptionalObject2 = FrontCancelNotify;
		ForeignPayload.TargetData = FManagedMontageTestHelpers::MakeCancelWindowTargetData(Anim, BigReactionAbility->GetTestRateWindowMontageInstanceID());
		BigReactionAbility->GetTestMontageTask()->TestInvokeCancelBegin(ForeignPayload);
		Test.TestFalse(TEXT("6.3: Foreign OptionalObject rejected in closed state"), BigReactionAbility->GetTestDodgeCancelable());
		Test.TestEqual(TEXT("6.4: Tag count remains 0 on foreign OptionalObject"), ASC->GetTagCount(TagCanCancelDodge), 0);

		ForeignPayload.OptionalObject = MontageFront;
		ForeignPayload.Target = Enemy;
		BigReactionAbility->GetTestMontageTask()->TestInvokeCancelBegin(ForeignPayload);
		Test.TestFalse(TEXT("6.5: Foreign Target rejected in closed state"), BigReactionAbility->GetTestDodgeCancelable());
		Test.TestEqual(TEXT("6.6: Tag count remains 0 on foreign Target"), ASC->GetTagCount(TagCanCancelDodge), 0);

		ForeignPayload.Target = Player;
		ForeignPayload.Instigator = Enemy;
		BigReactionAbility->GetTestMontageTask()->TestInvokeCancelBegin(ForeignPayload);
		Test.TestFalse(TEXT("6.7: Foreign Instigator rejected in closed state"), BigReactionAbility->GetTestDodgeCancelable());
		Test.TestEqual(TEXT("6.8: Tag count remains 0 on foreign Instigator"), ASC->GetTagCount(TagCanCancelDodge), 0);

		// 6B: Valid Begin proves the entry point is functional, followed by duplicate Begin for idempotence
		ForeignPayload.Instigator = Player;
		BigReactionAbility->GetTestMontageTask()->TestInvokeCancelBegin(ForeignPayload);
		Test.TestTrue(TEXT("6.9: Valid Begin opens cancel window"), BigReactionAbility->GetTestDodgeCancelable());
		Test.TestEqual(TEXT("6.10: Tag count is 1 after valid Begin"), ASC->GetTagCount(TagCanCancelDodge), 1);

		BigReactionAbility->GetTestMontageTask()->TestInvokeCancelBegin(ForeignPayload);
		Test.TestEqual(TEXT("6.11: Duplicate Begin preserves tag count at 1"), ASC->GetTagCount(TagCanCancelDodge), 1);

		// 6C: In OPEN state, verify illegal End payloads are rejected and window remains open
		FGameplayEventData ForeignEndPayload;
		ForeignEndPayload.EventTag = TagCancelEnd;
		ForeignEndPayload.Instigator = Player;
		ForeignEndPayload.Target = Player;
		ForeignEndPayload.OptionalObject = ForeignMontage;
		ForeignEndPayload.OptionalObject2 = FrontCancelNotify;
		ForeignEndPayload.TargetData = FManagedMontageTestHelpers::MakeCancelWindowTargetData(Anim, BigReactionAbility->GetTestRateWindowMontageInstanceID(), true);
		BigReactionAbility->GetTestMontageTask()->TestInvokeCancelEnd(ForeignEndPayload);
		Test.TestTrue(TEXT("6.12: Foreign End rejected, window remains open"), BigReactionAbility->GetTestDodgeCancelable());
		Test.TestEqual(TEXT("6.13: Tag count remains 1 on foreign End"), ASC->GetTagCount(TagCanCancelDodge), 1);

		ForeignEndPayload.OptionalObject = MontageFront;
		ForeignEndPayload.Target = Enemy;
		BigReactionAbility->GetTestMontageTask()->TestInvokeCancelEnd(ForeignEndPayload);
		Test.TestTrue(TEXT("6.14: Foreign Target End rejected, window remains open"), BigReactionAbility->GetTestDodgeCancelable());
		Test.TestEqual(TEXT("6.15: Tag count remains 1 on foreign Target End"), ASC->GetTagCount(TagCanCancelDodge), 1);

		// 6D: Valid End closes the window, followed by duplicate End for idempotence
		ForeignEndPayload.Target = Player;
		BigReactionAbility->GetTestMontageTask()->TestInvokeCancelEnd(ForeignEndPayload);
		Test.TestFalse(TEXT("6.16: Valid End closes cancel window"), BigReactionAbility->GetTestDodgeCancelable());
		Test.TestEqual(TEXT("6.17: Tag count is 0 after valid End"), ASC->GetTagCount(TagCanCancelDodge), 0);

		BigReactionAbility->GetTestMontageTask()->TestInvokeCancelEnd(ForeignEndPayload);
		Test.TestEqual(TEXT("6.18: Duplicate End preserves tag count at 0"), ASC->GetTagCount(TagCanCancelDodge), 0);

		// ---------------------------------------------------------------------
		// 7. RateWindow Access & Context Token Isolation
		// ---------------------------------------------------------------------
		UAnimNotifyState_MontageRateWindow* RateNotifyA = Cast<UAnimNotifyState_MontageRateWindow>(FindRateWindowEvent(MontageFront, 0)->NotifyStateClass.Get());
		FAnimNotifyEvent* RateEventA = FindRateWindowEvent(MontageFront, 0);
		FAnimNotifyEventReference RateRefA(RateEventA, MontageFront);
		RateRefA.AddContextData<UE::Anim::FAnimNotifyMontageInstanceContext>(BigReactionAbility->GetTestRateWindowMontageInstanceID());

		RateNotifyA->NotifyBegin(Player->GetMesh(), MontageFront, 1.0f, RateRefA);
		Test.TestEqual(TEXT("7.1: RateWindow applied 0.5f play rate"), GetMontagePlayRate(MontageFront), 0.5f);

		RateNotifyA->NotifyEnd(Player->GetMesh(), MontageFront, RateRefA);
		Test.TestEqual(TEXT("7.2: RateWindow restored 1.0f baseline"), GetMontagePlayRate(MontageFront), 1.0f);

		auto* FirstContext = BigReactionAbility->GetTestMontageTask();
		if (!Test.TestNotNull(TEXT("7.3: Active standard Task exists"), FirstContext)) return false;
		TStrongObjectPtr<UObject> KeepFirstContext(FirstContext);

		const auto FirstBegin = ASC->GenericGameplayEventCallbacks.FindChecked(TagRateBegin);
		const auto FirstEnd = ASC->GenericGameplayEventCallbacks.FindChecked(TagRateEnd);
		const int32 FirstID = BigReactionAbility->GetTestRateWindowMontageInstanceID();
		ASC->CancelAbilityHandle(BigReactionHandle);
		if (!Test.TestTrue(TEXT("7.4: Reactivation rebuilds the managed binding"), ActivateBigReaction(FVector(200.0f, 0.0f, 0.0f)))) return false;
		Test.TestTrue(TEXT("7.4a: Old Task terminated"), FirstContext->IsTerminated());
		Test.TestNotEqual(TEXT("7.4b: New montage instance"), BigReactionAbility->GetTestRateWindowMontageInstanceID(), FirstID);
		auto* SecondContext = BigReactionAbility->GetTestMontageTask();
		Test.TestTrue(TEXT("7.5: Rebind creates fresh context instance"), SecondContext != FirstContext);

		FGameplayEventData RatePayload;
		RatePayload.EventTag = TagRateBegin;
		RatePayload.Instigator = Player;
		RatePayload.Target = Player;
		RatePayload.OptionalObject = MontageFront;
		RatePayload.OptionalObject2 = RateNotifyA;
		RatePayload.EventMagnitude = 0.5f;
		RatePayload.TargetData = FManagedMontageTestHelpers::MakeRateWindowTargetData(Anim, BigReactionAbility->GetTestRateWindowMontageInstanceID());

		FirstBegin.Broadcast(&RatePayload);
		Test.TestEqual(TEXT("7.6: Stale context callback rejected (window count 0)"),
			BigReactionAbility->GetTestRateWindowLifecycle().GetActiveWindowCount(), 0);

		ASC->HandleGameplayEvent(TagRateBegin, &RatePayload);
		Test.TestEqual(TEXT("7.7: New context callback accepted (window count 1)"),
			BigReactionAbility->GetTestRateWindowLifecycle().GetActiveWindowCount(), 1);

		RatePayload.EventTag = TagRateEnd;
		FirstEnd.Broadcast(&RatePayload);
		Test.TestEqual(TEXT("7.7a: Old End cannot close new Task window"), BigReactionAbility->GetTestRateWindowLifecycle().GetActiveWindowCount(), 1);
		ASC->HandleGameplayEvent(TagRateEnd, &RatePayload);
		Test.TestEqual(TEXT("7.8: New context callback ended (window count 0)"),
			BigReactionAbility->GetTestRateWindowLifecycle().GetActiveWindowCount(), 0);
		// Ending a rate window does not end its owning ability. Close this case
		// explicitly before section 8 starts a fresh playback; do not flush events.
		ASC->CancelAbilityHandle(BigReactionHandle);
		if (!Test.TestFalse(TEXT("7.9: Context isolation case ends its reaction before the next activation"), BigReactionAbility->IsActive())) return false;

		// ---------------------------------------------------------------------
		// 8. Active Windows Teardown across All Exits (P2-1)
		// ---------------------------------------------------------------------
		// 8.1: Natural Montage End from Active Windows
		Test.TestTrue(TEXT("8.1a: Activate with open windows for natural end"), ActivateAndOpenWindows());
		Test.TestTrue(TEXT("8.1b: Cancel window is open before natural end"), BigReactionAbility->GetTestDodgeCancelable());
		Test.TestEqual(TEXT("8.1c: Play rate is modified (0.5f) before natural end"), GetMontagePlayRate(MontageFront), 0.5f);

		for (int32 Step = 0; Step < 40 && BigReactionAbility->IsActive(); ++Step)
		{
			AdvanceMontage(0.1f);
		}
		Test.TestFalse(TEXT("8.1d: Natural end ends big reaction ability"), BigReactionAbility->IsActive());
		Test.TestFalse(TEXT("8.1e: Natural end clears HitReacting tag"), ASC->HasMatchingGameplayTag(TagHitReacting));
		Test.TestEqual(TEXT("8.1f: Natural end clears CanCancel.Dodge tag"), ASC->GetTagCount(TagCanCancelDodge), 0);
		Test.TestFalse(TEXT("8.1g: Natural end clears RateWindow lifecycle"), BigReactionAbility->GetTestRateWindowLifecycle().IsBound());
		Test.TestFalse(TEXT("8.1h: Natural end removes cancel tasks"), BigReactionAbility->GetTestMontageTask() != nullptr);
		Test.TestFalse(TEXT("8.1i: Natural end removes rate tasks"), BigReactionAbility->HasTestRateWindowTasks());

		// 8.2: External Cancellation from Active Windows
		Test.TestTrue(TEXT("8.2a: Activate with open windows for external cancel"), ActivateAndOpenWindows());
		Test.TestTrue(TEXT("8.2b: Cancel window open"), BigReactionAbility->GetTestDodgeCancelable());
		Test.TestEqual(TEXT("8.2c: Play rate modified (0.5f)"), GetMontagePlayRate(MontageFront), 0.5f);

		ASC->CancelAbilityHandle(BigReactionHandle);
		Test.TestFalse(TEXT("8.2d: External cancel ends big reaction ability"), BigReactionAbility->IsActive());
		Test.TestFalse(TEXT("8.2e: External cancel clears HitReacting tag"), ASC->HasMatchingGameplayTag(TagHitReacting));
		Test.TestEqual(TEXT("8.2f: External cancel clears CanCancel.Dodge tag"), ASC->GetTagCount(TagCanCancelDodge), 0);
		Test.TestFalse(TEXT("8.2g: External cancel clears RateWindow lifecycle"), BigReactionAbility->GetTestRateWindowLifecycle().IsBound());
		Test.TestFalse(TEXT("8.2h: External cancel removes cancel tasks"), BigReactionAbility->GetTestMontageTask() != nullptr);
		Test.TestFalse(TEXT("8.2i: External cancel removes rate tasks"), BigReactionAbility->HasTestRateWindowTasks());

		// 8.3: Falling from Active Windows
		Test.TestTrue(TEXT("8.3a: Activate with open windows for falling teardown"), ActivateAndOpenWindows());
		Test.TestTrue(TEXT("8.3b: Cancel window open"), BigReactionAbility->GetTestDodgeCancelable());
		Test.TestEqual(TEXT("8.3c: Play rate modified (0.5f)"), GetMontagePlayRate(MontageFront), 0.5f);

		Movement->SetMovementMode(MOVE_Falling);
		Test.TestFalse(TEXT("8.3d: Falling ended big reaction ability"), BigReactionAbility->IsActive());
		Test.TestFalse(TEXT("8.3e: Falling cleared HitReacting tag"), ASC->HasMatchingGameplayTag(TagHitReacting));
		Test.TestEqual(TEXT("8.3f: Falling cleared CanCancel.Dodge tag"), ASC->GetTagCount(TagCanCancelDodge), 0);
		Test.TestFalse(TEXT("8.3g: Falling cleared RateWindow lifecycle"), BigReactionAbility->GetTestRateWindowLifecycle().IsBound());
		Test.TestFalse(TEXT("8.3h: Falling removed cancel tasks"), BigReactionAbility->GetTestMontageTask() != nullptr);
		Test.TestFalse(TEXT("8.3i: Falling removed rate tasks"), BigReactionAbility->HasTestRateWindowTasks());
		Movement->SetMovementMode(MOVE_Walking);

		// 8.4: Death from Active Windows
		Test.TestTrue(TEXT("8.4a: Activate with open windows for death teardown"), ActivateAndOpenWindows());
		Test.TestTrue(TEXT("8.4b: Cancel window open"), BigReactionAbility->GetTestDodgeCancelable());
		Test.TestEqual(TEXT("8.4c: Play rate modified (0.5f)"), GetMontagePlayRate(MontageFront), 0.5f);

		const FGameplayTag TagDead = FGameplayTag::RequestGameplayTag(TEXT("State.Status.Dead"));
		ASC->AddLooseGameplayTag(TagDead);
		ASC->CancelAbilityHandle(BigReactionHandle);
		Test.TestFalse(TEXT("8.4d: Death cancel ends big reaction ability"), BigReactionAbility->IsActive());
		Test.TestFalse(TEXT("8.4e: Death clears HitReacting tag"), ASC->HasMatchingGameplayTag(TagHitReacting));
		Test.TestEqual(TEXT("8.4f: Death clears CanCancel.Dodge tag"), ASC->GetTagCount(TagCanCancelDodge), 0);
		Test.TestFalse(TEXT("8.4g: Death clears RateWindow lifecycle"), BigReactionAbility->GetTestRateWindowLifecycle().IsBound());
		Test.TestFalse(TEXT("8.4h: Death removes cancel tasks"), BigReactionAbility->GetTestMontageTask() != nullptr);
		Test.TestFalse(TEXT("8.4i: Death removes rate tasks"), BigReactionAbility->HasTestRateWindowTasks());

		// Death blocks reactivation
		Test.TestFalse(TEXT("8.4j: Dead state blocks big reaction reactivation"), ActivateBigReaction(FVector(200.0f, 0.0f, 0.0f)));
		ASC->RemoveLooseGameplayTag(TagDead);

		// 8.5: Playback Startup Failure (Convergence)
		UAnimMontage* EmptyMontage = NewObject<UAnimMontage>(World);
		EmptyMontage->SetSkeleton(MontageFront->GetSkeleton());
		BigReactionAbility->SetTestMontages(EmptyMontage, EmptyMontage, EmptyMontage, EmptyMontage);
		const bool bStartupFailResult = ActivateBigReaction(FVector(200.0f, 0.0f, 0.0f));
		Test.TestFalse(TEXT("8.5a: Startup failure fails activation"), bStartupFailResult);
		Test.TestFalse(TEXT("8.5b: Startup failure leaves ability inactive"), BigReactionAbility->IsActive());
		Test.TestFalse(TEXT("8.5c: Startup failure leaves no HitReacting tag"), ASC->HasMatchingGameplayTag(TagHitReacting));
		Test.TestEqual(TEXT("8.5d: Startup failure leaves no CanCancel tag"), ASC->GetTagCount(TagCanCancelDodge), 0);
		Test.TestFalse(TEXT("8.5e: Startup failure leaves no rate binding"), BigReactionAbility->GetTestRateWindowLifecycle().IsBound());
		Test.TestFalse(TEXT("8.5f: Startup failure leaves no cancel tasks"), BigReactionAbility->GetTestMontageTask() != nullptr);
		Test.TestFalse(TEXT("8.5g: Startup failure leaves no rate tasks"), BigReactionAbility->HasTestRateWindowTasks());
		Test.TestNull(TEXT("8.5h: Startup failure leaves no rate context"), BigReactionAbility->GetTestMontageTask());
		BigReactionAbility->SetTestMontages(MontageFront, MontageBack, MontageLeft, MontageRight);

		// 8.6: Clean Reactivation after previous teardowns
		Test.TestTrue(TEXT("8.6a: Clean reactivation succeeds"), ActivateBigReaction(FVector(200.0f, 0.0f, 0.0f)));
		Test.TestFalse(TEXT("8.6b: Reactivation starts with cancel window closed"), BigReactionAbility->GetTestDodgeCancelable());
		Test.TestEqual(TEXT("8.6c: Reactivation starts with tag count 0"), ASC->GetTagCount(TagCanCancelDodge), 0);
		Test.TestEqual(TEXT("8.6d: Reactivation starts with baseline play rate 1.0f"), GetMontagePlayRate(MontageFront), 1.0f);
		ASC->CancelAbilityHandle(BigReactionHandle);

		// Cancel and replay the same asset BEFORE dispatching the old end event.
		if (!Test.TestTrue(TEXT("8.6e: Activate old playback for queued-end regression"), ActivateAndOpenWindows())) return false;
		const int32 OldInstanceID = BigReactionAbility->GetTestRateWindowMontageInstanceID();
		ASC->CancelAbilityHandle(BigReactionHandle);
		Anim->TickMontageOnly(0.001f); // Queue termination; deliberately do not dispatch yet.
		if (!Test.TestTrue(TEXT("8.6f: Immediate same-asset reactivation succeeds"), ActivateBigReaction(FVector(200.0f, 0.0f, 0.0f)))) return false;
		const int32 NewInstanceID = BigReactionAbility->GetTestRateWindowMontageInstanceID();
		Test.TestNotEqual(TEXT("8.6g: Reactivation owns a different playback instance"), NewInstanceID, OldInstanceID);
		AdvanceMontage(0.01f);
		Test.TestTrue(TEXT("8.6h: Queued old end event cannot end new reaction"), BigReactionAbility->IsActive());
		Test.TestEqual(TEXT("8.6i: New playback binding survives old end event"), BigReactionAbility->GetTestRateWindowMontageInstanceID(), NewInstanceID);
		Test.TestTrue(TEXT("8.6j: New reaction retains HitReacting"), ASC->HasMatchingGameplayTag(TagHitReacting));
		ASC->CancelAbilityHandle(BigReactionHandle);

		// 8.7: Player Destruction Teardown
		Test.TestTrue(TEXT("8.7a: Activate with open windows for player destruction"), ActivateAndOpenWindows());
		auto* DestroyedContext = BigReactionAbility->GetTestMontageTask();
		TStrongObjectPtr<UObject> KeepDestroyedContext(DestroyedContext);
		TStrongObjectPtr<UPlayerBigHitReactionAbility> KeepAbility(BigReactionAbility);

		Player->Destroy();
		Test.TestFalse(TEXT("8.7b: Destroy ends big reaction ability"), BigReactionAbility->IsActive());
		Test.TestFalse(TEXT("8.7c: Destroy clears RateWindow lifecycle"), BigReactionAbility->GetTestRateWindowLifecycle().IsBound());
		Test.TestFalse(TEXT("8.7d: Destroy removes cancel tasks"), BigReactionAbility->GetTestMontageTask() != nullptr);
		Test.TestFalse(TEXT("8.7e: Destroy removes rate tasks"), BigReactionAbility->HasTestRateWindowTasks());
		if (DestroyedContext)
		{
			Test.TestTrue(TEXT("8.7f: Destroy terminates callback Task"), DestroyedContext->IsTerminated());
			Test.TestNull(TEXT("8.7g: Destroy clears callback animation owner"), DestroyedContext->GetBoundAnimInstance());
		}

		return true;
	}

	bool RunSprintAttackConsumer(FAutomationTestBase& Test, const TCHAR* MontageProperty, const TCHAR* Name)
	{
		FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		WorldContext.SetCurrentWorld(World);
		FTestWorldScope Cleanup{ World };
		if (!Test.TestNotNull(TEXT("Consumer world exists"), World)) return false;
		FURL URL;
		World->InitializeActorsForPlay(URL);
		World->BeginPlay();
		APlayerCharacter* Player = FCombatAutomationFixture::SpawnPlayer(World);
		if (!Test.TestNotNull(TEXT("Player exists"), Player)) return false;
		UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
		if (!Test.TestNotNull(TEXT("ASC exists"), ASC)) return false;
		Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxStaminaAttribute(), 100.0f);
		ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 100.0f);

		UAnimMontage* Montage = CreatePlayableRateMontage(Test, World, Name);
		if (!Test.TestNotNull(TEXT("Playable consumer montage exists"), Montage)) return false;
		UAnimInstance* Anim = NewObject<UAnimInstance>(Player->GetMesh());
		Anim->InitializeMontageOnly();
		Anim->CurrentSkeleton = Montage->GetSkeleton();
		Player->GetMesh()->AnimScriptInstance = Anim;
		ASC->RefreshAbilityActorInfo();

		ASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Movement.Sprinting")));
		Player->SetTestCurrentMoveInput(FVector2D(0.0f, 1.0f));

		const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(FGameplayAbilitySpec(USprintAttackAbility::StaticClass(), 1, INDEX_NONE, Player));
		USprintAttackAbility* Ability = Cast<USprintAttackAbility>(ASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance());
		if (!Test.TestNotNull(TEXT("ASC instanced the SprintAttack consumer"), Ability)) return false;
		ON_SCOPE_EXIT { if (IsValid(ASC)) { ASC->CancelAbilityHandle(Handle); ASC->ClearAbility(Handle); } };

		if (!Test.TestTrue(TEXT("Fixture assigns authored montage"), SetFixtureObject(Ability, MontageProperty, Montage))) return false;
		for (const FName EffectProperty : { FName(TEXT("CostGameplayEffectClass")), FName(TEXT("CooldownGameplayEffectClass")),
			FName(TEXT("DamageGameplayEffectClass")), FName(TEXT("StaminaRegenDelayGameplayEffectClass")), FName(TEXT("InvulnerabilityGameplayEffectClass")) })
		{
			SetFixtureObject(Ability, EffectProperty, UGameplayEffect::StaticClass());
		}

		const bool bActivated = ASC->TryActivateAbility(Handle);
		if (!Test.TestTrue(TEXT("Real ASC activation keeps SprintAttack active"), bActivated && Ability->IsActive()
			&& ASC->FindAbilitySpecFromHandle(Handle)->IsActive()))
		{
			return false;
		}

		Test.TestTrue(TEXT("SprintAttack RateWindow lifecycle is bound"), Ability->GetTestRateWindowLifecycle().IsBound());
		Test.TestTrue(TEXT("SprintAttack has active montage task"), Ability->HasTestRateWindowTasks());

		const FAnimMontageInstance* Instance = Anim->GetActiveInstanceForMontage(Montage);
		if (!Test.TestNotNull(TEXT("Actual montage instance exists"), Instance)) return false;
		Test.TestEqual(TEXT("SprintAttack owns actual instance ID"), Ability->GetTestRateWindowMontageInstanceID(), Instance->GetInstanceID());

		FAnimNotifyEvent* EventA = FindRateWindowEvent(Montage, 0);
		FAnimNotifyEvent* EventB = FindRateWindowEvent(Montage, 1);
		if (!Test.TestNotNull(TEXT("Notify A exists"), EventA) || !Test.TestNotNull(TEXT("Notify B exists"), EventB)) return false;
		auto* NotifyA = CastChecked<UAnimNotifyState_MontageRateWindow>(EventA->NotifyStateClass);
		auto* NotifyB = CastChecked<UAnimNotifyState_MontageRateWindow>(EventB->NotifyStateClass);
		FAnimNotifyEventReference RefA(EventA, Montage);
		RefA.AddContextData<UE::Anim::FAnimNotifyMontageInstanceContext>(Instance->GetInstanceID());
		FAnimNotifyEventReference RefB(EventB, Montage);
		RefB.AddContextData<UE::Anim::FAnimNotifyMontageInstanceContext>(Instance->GetInstanceID());

		const auto BeginA = [&]() { NotifyA->NotifyBegin(Player->GetMesh(), Montage, 1.0f, RefA); };
		const auto BeginB = [&]() { NotifyB->NotifyBegin(Player->GetMesh(), Montage, 1.0f, RefB); };
		const auto EndA = [&]() { NotifyA->NotifyEnd(Player->GetMesh(), Montage, RefA); };
		const auto EndB = [&]() { NotifyB->NotifyEnd(Player->GetMesh(), Montage, RefB); };
		const auto Rate = [&](float Expected) { Test.TestEqual(TEXT("Actual montage rate"), Anim->Montage_GetPlayRate(Montage), Expected); };

		// Exercise RateWindow via standard AnimNotify transport
		BeginA(); Rate(0.5f);
		BeginB(); Rate(0.2f);
		EndA(); Rate(0.2f);
		EndB(); Rate(1.0f);

		// Rejection of invalid payloads (missing TargetData, mismatched ID, INDEX_NONE ID)
		const FGameplayTag BeginTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Action.RateWindow.Begin"));
		FGameplayEventData InvalidData;
		InvalidData.EventTag = BeginTag;
		InvalidData.EventMagnitude = 0.3f;
		ASC->HandleGameplayEvent(BeginTag, &InvalidData);
		Rate(1.0f); // Rejected (missing TargetData), rate unaffected

		FGameplayEventData WrongIDData = InvalidData;
		WrongIDData.TargetData = FManagedMontageTestHelpers::MakeRateWindowTargetData(Anim, Instance->GetInstanceID() + 9999);
		ASC->HandleGameplayEvent(BeginTag, &WrongIDData);
		Rate(1.0f); // Rejected (mismatched ID), rate unaffected

		FGameplayEventData IndexNoneData = InvalidData;
		IndexNoneData.TargetData = FManagedMontageTestHelpers::MakeRateWindowTargetData(Anim, INDEX_NONE);
		ASC->HandleGameplayEvent(BeginTag, &IndexNoneData);
		Rate(1.0f); // Rejected (INDEX_NONE ID), rate unaffected

		// End ability
		ASC->CancelAbilityHandle(Handle);
		Test.TestFalse(TEXT("SprintAttack ended"), Ability->IsActive());
		Test.TestFalse(TEXT("SprintAttack lifecycle unbound"), Ability->GetTestRateWindowLifecycle().IsBound());
		Test.TestFalse(TEXT("SprintAttack montage task cleaned up"), Ability->HasTestRateWindowTasks());

		return true;
	}

	bool RunChargedAttackConsumer(FAutomationTestBase& Test, const TCHAR* MontageProperty, const TCHAR* Name)
	{
		FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		WorldContext.SetCurrentWorld(World);
		FTestWorldScope Cleanup{ World };
		if (!Test.TestNotNull(TEXT("Consumer world exists"), World)) return false;
		FURL URL;
		World->InitializeActorsForPlay(URL);
		World->BeginPlay();
		APlayerCharacter* Player = FCombatAutomationFixture::SpawnPlayer(World);
		if (!Test.TestNotNull(TEXT("Player exists"), Player)) return false;
		UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
		if (!Test.TestNotNull(TEXT("ASC exists"), ASC)) return false;
		Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxStaminaAttribute(), 100.0f);
		ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 100.0f);

		UAnimMontage* Montage = CreatePlayableRateMontage(Test, World, Name);
		if (!Test.TestNotNull(TEXT("Playable consumer montage exists"), Montage)) return false;
		UAnimInstance* Anim = NewObject<UAnimInstance>(Player->GetMesh());
		Anim->InitializeMontageOnly();
		Anim->CurrentSkeleton = Montage->GetSkeleton();
		Player->GetMesh()->AnimScriptInstance = Anim;
		ASC->RefreshAbilityActorInfo();

		const FGameplayTag PrimaryInput = FGameplayTag::RequestGameplayTag(TEXT("Input.PrimaryAttack"));
		Player->TriggerTestHandleCombatInputStarted(PrimaryInput);
		ASC->CancelAllAbilities();

		const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(FGameplayAbilitySpec(UChargedAttackAbility::StaticClass(), 1, INDEX_NONE, Player));
		UChargedAttackAbility* Ability = Cast<UChargedAttackAbility>(ASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance());
		if (!Test.TestNotNull(TEXT("ASC instanced the ChargedAttack consumer"), Ability)) return false;
		ON_SCOPE_EXIT { if (IsValid(ASC)) { ASC->CancelAbilityHandle(Handle); ASC->ClearAbility(Handle); } };

		if (!Test.TestTrue(TEXT("Fixture assigns authored montage"), SetFixtureObject(Ability, MontageProperty, Montage))) return false;
		for (const FName EffectProperty : { FName(TEXT("CostGameplayEffectClass")), FName(TEXT("CooldownGameplayEffectClass")),
			FName(TEXT("DamageGameplayEffectClass")), FName(TEXT("StaminaRegenDelayGameplayEffectClass")), FName(TEXT("InvulnerabilityGameplayEffectClass")) })
		{
			SetFixtureObject(Ability, EffectProperty, UGameplayEffect::StaticClass());
		}

		const bool bActivated = ASC->TryActivateAbility(Handle);
		if (!Test.TestTrue(TEXT("Real ASC activation keeps ChargedAttack active"), bActivated && Ability->IsActive()
			&& ASC->FindAbilitySpecFromHandle(Handle)->IsActive()))
		{
			return false;
		}

		Test.TestTrue(TEXT("ChargedAttack RateWindow lifecycle is bound"), Ability->GetTestRateWindowLifecycle().IsBound());
		Test.TestTrue(TEXT("ChargedAttack has active montage task"), Ability->HasTestRateWindowTasks());
		Test.TestEqual(TEXT("ChargedAttack starts at montage time zero"), Anim->Montage_GetPosition(Montage), 0.0f);
		Test.TestEqual(TEXT("ChargedAttack preserves root motion scale"), Player->GetAnimRootMotionTranslationScale(), 1.0f);

		const FAnimMontageInstance* Instance = Anim->GetActiveInstanceForMontage(Montage);
		if (!Test.TestNotNull(TEXT("Actual montage instance exists"), Instance)) return false;
		Test.TestEqual(TEXT("ChargedAttack owns actual instance ID"), Ability->GetTestRateWindowMontageInstanceID(), Instance->GetInstanceID());

		FAnimNotifyEvent* EventA = FindRateWindowEvent(Montage, 0);
		FAnimNotifyEvent* EventB = FindRateWindowEvent(Montage, 1);
		if (!Test.TestNotNull(TEXT("Notify A exists"), EventA) || !Test.TestNotNull(TEXT("Notify B exists"), EventB)) return false;
		auto* NotifyA = CastChecked<UAnimNotifyState_MontageRateWindow>(EventA->NotifyStateClass);
		auto* NotifyB = CastChecked<UAnimNotifyState_MontageRateWindow>(EventB->NotifyStateClass);
		FAnimNotifyEventReference RefA(EventA, Montage);
		RefA.AddContextData<UE::Anim::FAnimNotifyMontageInstanceContext>(Instance->GetInstanceID());
		FAnimNotifyEventReference RefB(EventB, Montage);
		RefB.AddContextData<UE::Anim::FAnimNotifyMontageInstanceContext>(Instance->GetInstanceID());

		const auto BeginA = [&]() { NotifyA->NotifyBegin(Player->GetMesh(), Montage, 1.0f, RefA); };
		const auto BeginB = [&]() { NotifyB->NotifyBegin(Player->GetMesh(), Montage, 1.0f, RefB); };
		const auto EndA = [&]() { NotifyA->NotifyEnd(Player->GetMesh(), Montage, RefA); };
		const auto EndB = [&]() { NotifyB->NotifyEnd(Player->GetMesh(), Montage, RefB); };
		const auto Rate = [&](float Expected) { Test.TestEqual(TEXT("Actual montage rate"), Anim->Montage_GetPlayRate(Montage), Expected); };

		// Exercise RateWindow via standard AnimNotify transport
		BeginA(); Rate(0.5f);
		BeginB(); Rate(0.2f);
		EndA(); Rate(0.2f);
		EndB(); Rate(1.0f);

		// Charged Hold pause & resume
		BeginA(); Rate(0.5f);
		FGameplayEventData HoldPayload;
		HoldPayload.EventTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Attack.Charged.HoldReady"));
		HoldPayload.Instigator = Player;
		HoldPayload.Target = Player;
		HoldPayload.OptionalObject = Montage;
		ASC->HandleGameplayEvent(HoldPayload.EventTag, &HoldPayload);
		Test.TestFalse(TEXT("Hold pauses montage"), Anim->Montage_IsPlaying(Montage));

		// RateWindow End while paused does not resume
		EndA(); Rate(1.0f);
		Test.TestFalse(TEXT("Rate End cannot resume Hold"), Anim->Montage_IsPlaying(Montage));

		// Release resumes
		Player->TriggerTestHandleCombatInputEnded(PrimaryInput);
		Test.TestTrue(TEXT("Release resumes playing"), Anim->Montage_IsPlaying(Montage));

		// End ability
		ASC->CancelAbilityHandle(Handle);
		Test.TestFalse(TEXT("ChargedAttack ended"), Ability->IsActive());
		Test.TestFalse(TEXT("ChargedAttack RateWindow lifecycle unbound"), Ability->GetTestRateWindowLifecycle().IsBound());
		Test.TestFalse(TEXT("ChargedAttack montage task cleaned up"), Ability->HasTestRateWindowTasks());

		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMeleeSkillRateWindowTest, "PolyQuest.Combat.PlayerMontageRateWindow.MeleeSkill", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FMeleeSkillRateWindowTest::RunTest(const FString&) { return PlayerMontageRateWindowAutomation::RunConsumer<UPlayerMeleeSkillAbility>(*this, TEXT("SkillMontage"), TEXT("MeleeSkill")); }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSprintAttackRateWindowTest, "PolyQuest.Combat.PlayerMontageRateWindow.Sprint", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSprintAttackRateWindowTest::RunTest(const FString&) { return PlayerMontageRateWindowAutomation::RunSprintAttackConsumer(*this, TEXT("SprintAttackMontage"), TEXT("Sprint")); }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChargedAttackRateWindowTest, "PolyQuest.Combat.PlayerMontageRateWindow.Charged", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FChargedAttackRateWindowTest::RunTest(const FString&) { return PlayerMontageRateWindowAutomation::RunChargedAttackConsumer(*this, TEXT("ChargedAttackMontage"), TEXT("Charged")); }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBowRateWindowTest, "PolyQuest.Combat.PlayerMontageRateWindow.Bow", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FBowRateWindowTest::RunTest(const FString&) { return PlayerMontageRateWindowAutomation::RunConsumer<UBowDrawFireAbility>(*this, TEXT("BowMontage"), TEXT("Bow")); }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDodgeRateWindowTest, "PolyQuest.Combat.PlayerMontageRateWindow.Dodge", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDodgeRateWindowTest::RunTest(const FString&) { return PlayerMontageRateWindowAutomation::RunConsumer<UDodgeAbility>(*this, TEXT("DodgeMontage"), TEXT("Dodge")); }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerBigHitReactionWindowsTest, "PolyQuest.Combat.PlayerBigHitReactionWindows", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPlayerBigHitReactionWindowsTest::RunTest(const FString&) { return PlayerMontageRateWindowAutomation::RunPlayerBigHitReactionWindowsTest(*this); }
#endif // WITH_EDITOR

#endif // WITH_DEV_AUTOMATION_TESTS
