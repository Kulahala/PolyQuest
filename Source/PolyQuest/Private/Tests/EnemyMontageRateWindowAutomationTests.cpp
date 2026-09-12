#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystem/Abilities/EnemyHitReactionAbility.h"
#include "AbilitySystem/Abilities/EnemyLaunchReactionAbility.h"
#include "AbilitySystem/Abilities/EnemyMeleeAbility.h"
#include "AbilitySystem/Abilities/EnemySmallHitReactionAbility.h"
#include "AbilitySystem/Abilities/EnemyVictimExecutionAbility.h"
#include "AbilitySystem/Abilities/MontageRateWindowLifecycle.h"
#include "Animation/AnimComposite.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifyQueue.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Animation/Combat/AnimNotifyState_ActionWindows.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Character/Player/PlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameplayTagContainer.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ScopeExit.h"
#include "ReferenceSkeleton.h"
#include "Tests/CombatAutomationFixture.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include <limits>

#if WITH_EDITOR
#include "Animation/AnimData/IAnimationDataController.h"
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FEnemyMontageRateWindowAutomationTest,
	"PolyQuest.Combat.EnemyMontageRateWindow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace EnemyMontageRateWindowAutomation
{
#if WITH_EDITOR
	// Same transient montage-only fixture approach as PlayerLockOnAutomationTests.
	class UTestMontageLengthAccess : public UAnimMontage
	{
	public:
		static void SetLength(UAnimMontage* Montage, float Length)
		{
			static_cast<UTestMontageLengthAccess*>(Montage)->SequenceLength = Length;
		}
	};

	UAnimMontage* CreatePlayableRateMontage(FAutomationTestBase& Test, UObject* Outer)
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
		// Notify arrays may be reordered when the duplicated animation refreshes its cache.
		return Montage->Notifies.FindByPredicate([WindowName](const FAnimNotifyEvent& Event)
		{
			return Event.NotifyStateClass && Event.NotifyStateClass->GetFName() == WindowName;
		});
	}
#endif

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

		UAnimNotifyState_MontageRateWindow* NotifyA = NewObject<UAnimNotifyState_MontageRateWindow>(EffectiveOuter, TEXT("Test_NotifyA"));
		NotifyA->RateMultiplier = 0.5f;
		FAnimNotifyEvent EventA;
		EventA.NotifyStateClass = NotifyA;
		Montage->Notifies.Add(EventA);

		UAnimNotifyState_MontageRateWindow* NotifyB = NewObject<UAnimNotifyState_MontageRateWindow>(EffectiveOuter, TEXT("Test_NotifyB"));
		NotifyB->RateMultiplier = 0.2f;
		FAnimNotifyEvent EventB;
		EventB.NotifyStateClass = NotifyB;
		Montage->Notifies.Add(EventB);

		UAnimNotifyState_MontageRateWindow* NotifyC = NewObject<UAnimNotifyState_MontageRateWindow>(EffectiveOuter, TEXT("Test_NotifyC"));
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
}

bool FEnemyMontageRateWindowAutomationTest::RunTest(const FString& Parameters)
{
	// =========================================================================
	// 1. CDO & Tag Contract Verification across all 5 Enemy Abilities
	// =========================================================================
	const FGameplayTag TagRateWindowBegin = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.RateWindow.Begin")), false);
	const FGameplayTag TagRateWindowEnd = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.RateWindow.End")), false);

	TestTrue(TEXT("Tag Event.Action.RateWindow.Begin is valid"), TagRateWindowBegin.IsValid());
	TestTrue(TEXT("Tag Event.Action.RateWindow.End is valid"), TagRateWindowEnd.IsValid());

	// 1.1 UEnemyMeleeAbility CDO
	const UEnemyMeleeAbility* MeleeCDO = UEnemyMeleeAbility::StaticClass()->GetDefaultObject<UEnemyMeleeAbility>();
	TestNotNull(TEXT("UEnemyMeleeAbility CDO exists"), MeleeCDO);
	if (MeleeCDO)
	{
		TestEqual(TEXT("EnemyMelee instancing is InstancedPerActor"),
			MeleeCDO->GetInstancingPolicy(), EGameplayAbilityInstancingPolicy::InstancedPerActor);
		TestEqual(TEXT("EnemyMelee net execution is ServerOnly"),
			MeleeCDO->GetNetExecutionPolicy(), EGameplayAbilityNetExecutionPolicy::ServerOnly);
		TestTrue(TEXT("EnemyMelee RateWindowBegin tag matches Event.Action.RateWindow.Begin"),
			MeleeCDO->GetTestRateWindowBeginEventTag() == TagRateWindowBegin);
		TestTrue(TEXT("EnemyMelee RateWindowEnd tag matches Event.Action.RateWindow.End"),
			MeleeCDO->GetTestRateWindowEndEventTag() == TagRateWindowEnd);
		TestNull(TEXT("EnemyMelee CDO has null RateWindowContext"), MeleeCDO->GetTestActiveRateWindowContext());
	}

	// 1.2 UEnemyHitReactionAbility CDO
	const UEnemyHitReactionAbility* BigHitCDO = UEnemyHitReactionAbility::StaticClass()->GetDefaultObject<UEnemyHitReactionAbility>();
	TestNotNull(TEXT("UEnemyHitReactionAbility CDO exists"), BigHitCDO);
	if (BigHitCDO)
	{
		TestEqual(TEXT("EnemyHitReaction instancing is InstancedPerActor"),
			BigHitCDO->GetInstancingPolicy(), EGameplayAbilityInstancingPolicy::InstancedPerActor);
		TestEqual(TEXT("EnemyHitReaction net execution is ServerOnly"),
			BigHitCDO->GetNetExecutionPolicy(), EGameplayAbilityNetExecutionPolicy::ServerOnly);
		TestTrue(TEXT("EnemyHitReaction RateWindowBegin tag matches Event.Action.RateWindow.Begin"),
			BigHitCDO->GetTestRateWindowBeginEventTag() == TagRateWindowBegin);
		TestTrue(TEXT("EnemyHitReaction RateWindowEnd tag matches Event.Action.RateWindow.End"),
			BigHitCDO->GetTestRateWindowEndEventTag() == TagRateWindowEnd);
		TestNull(TEXT("EnemyHitReaction CDO has null RateWindowContext"), BigHitCDO->GetTestActiveRateWindowContext());
	}

	// 1.3 UEnemySmallHitReactionAbility CDO
	const UEnemySmallHitReactionAbility* SmallHitCDO = UEnemySmallHitReactionAbility::StaticClass()->GetDefaultObject<UEnemySmallHitReactionAbility>();
	TestNotNull(TEXT("UEnemySmallHitReactionAbility CDO exists"), SmallHitCDO);
	if (SmallHitCDO)
	{
		TestEqual(TEXT("EnemySmallHitReaction instancing is InstancedPerActor"),
			SmallHitCDO->GetInstancingPolicy(), EGameplayAbilityInstancingPolicy::InstancedPerActor);
		TestEqual(TEXT("EnemySmallHitReaction net execution is ServerOnly"),
			SmallHitCDO->GetNetExecutionPolicy(), EGameplayAbilityNetExecutionPolicy::ServerOnly);
		TestTrue(TEXT("EnemySmallHitReaction RateWindowBegin tag matches Event.Action.RateWindow.Begin"),
			SmallHitCDO->GetTestRateWindowBeginEventTag() == TagRateWindowBegin);
		TestTrue(TEXT("EnemySmallHitReaction RateWindowEnd tag matches Event.Action.RateWindow.End"),
			SmallHitCDO->GetTestRateWindowEndEventTag() == TagRateWindowEnd);
		TestNull(TEXT("EnemySmallHitReaction CDO has null RateWindowContext"), SmallHitCDO->GetTestActiveRateWindowContext());
	}

	// 1.4 UEnemyLaunchReactionAbility CDO
	const UEnemyLaunchReactionAbility* LaunchCDO = UEnemyLaunchReactionAbility::StaticClass()->GetDefaultObject<UEnemyLaunchReactionAbility>();
	TestNotNull(TEXT("UEnemyLaunchReactionAbility CDO exists"), LaunchCDO);
	if (LaunchCDO)
	{
		TestEqual(TEXT("EnemyLaunchReaction instancing is InstancedPerActor"),
			LaunchCDO->GetInstancingPolicy(), EGameplayAbilityInstancingPolicy::InstancedPerActor);
		TestEqual(TEXT("EnemyLaunchReaction net execution is ServerOnly"),
			LaunchCDO->GetNetExecutionPolicy(), EGameplayAbilityNetExecutionPolicy::ServerOnly);
		TestTrue(TEXT("EnemyLaunchReaction RateWindowBegin tag matches Event.Action.RateWindow.Begin"),
			LaunchCDO->GetTestRateWindowBeginEventTag() == TagRateWindowBegin);
		TestTrue(TEXT("EnemyLaunchReaction RateWindowEnd tag matches Event.Action.RateWindow.End"),
			LaunchCDO->GetTestRateWindowEndEventTag() == TagRateWindowEnd);
		TestNull(TEXT("EnemyLaunchReaction CDO has null RateWindowContext"), LaunchCDO->GetTestActiveRateWindowContext());
	}

	// 1.5 UEnemyVictimExecutionAbility CDO
	const UEnemyVictimExecutionAbility* VictimCDO = UEnemyVictimExecutionAbility::StaticClass()->GetDefaultObject<UEnemyVictimExecutionAbility>();
	TestNotNull(TEXT("UEnemyVictimExecutionAbility CDO exists"), VictimCDO);
	if (VictimCDO)
	{
		TestEqual(TEXT("EnemyVictimExecution instancing is InstancedPerActor"),
			VictimCDO->GetInstancingPolicy(), EGameplayAbilityInstancingPolicy::InstancedPerActor);
		TestEqual(TEXT("EnemyVictimExecution net execution is ServerOnly"),
			VictimCDO->GetNetExecutionPolicy(), EGameplayAbilityNetExecutionPolicy::ServerOnly);
		TestTrue(TEXT("EnemyVictimExecution RateWindowBegin tag matches Event.Action.RateWindow.Begin"),
			VictimCDO->GetTestRateWindowBeginEventTag() == TagRateWindowBegin);
		TestTrue(TEXT("EnemyVictimExecution RateWindowEnd tag matches Event.Action.RateWindow.End"),
			VictimCDO->GetTestRateWindowEndEventTag() == TagRateWindowEnd);
		TestNull(TEXT("EnemyVictimExecution CDO has null RateWindowContext"), VictimCDO->GetTestActiveRateWindowContext());
		TestFalse(TEXT("EnemyVictimExecution CDO is not in non-lethal recovery"), VictimCDO->IsTestNonLethalRecoveryActive());
	}

	// =========================================================================
	// 2. World & Actor Setup
	// =========================================================================
	if (!TestNotNull(TEXT("GEngine is valid"), GEngine))
	{
		return false;
	}

	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("EnemyMontageRateWindowTestWorld"));
	WorldContext.SetCurrentWorld(World);
	EnemyMontageRateWindowAutomation::FTestWorldScope ScopeCleanup{ World };

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

	if (!TestNotNull(TEXT("Enemy spawned"), Enemy) || !TestNotNull(TEXT("Player spawned"), Player))
	{
		return false;
	}

	UAbilitySystemComponent* EnemyASC = Enemy->GetAbilitySystemComponent();
	if (!TestNotNull(TEXT("Enemy ASC valid"), EnemyASC))
	{
		return false;
	}

	EnemyMontageRateWindowAutomation::FTestMontageSetup Setup = EnemyMontageRateWindowAutomation::CreateTestMontage(World);
	UAnimMontage* ActiveMontage = Setup.Montage;
	UAnimNotifyState_MontageRateWindow* NotifyA = Setup.NotifyA;
	UAnimNotifyState_MontageRateWindow* NotifyB = Setup.NotifyB;
	UAnimNotifyState_MontageRateWindow* NotifyC = Setup.NotifyC;

	UAnimMontage* ForeignMontage = EnemyMontageRateWindowAutomation::CreateDummyMontage(World);
	UAnimInstance* MockAnimInstance = NewObject<UAnimInstance>(Enemy->GetMesh());

	// =========================================================================
	// 3. Near-Combat Melee (UEnemyMeleeAbility) RateWindow Integration
	// =========================================================================
	{
		UEnemyMeleeAbility* MeleeAbility = NewObject<UEnemyMeleeAbility>(Enemy);
		MeleeAbility->SetTestActorInfo(FGameplayAbilitySpecHandle(), EnemyASC->AbilityActorInfo.Get());
		MeleeAbility->SetTestAbilityActive(true);
		MeleeAbility->SetTestBypassMontageActiveCheck(true);

		FAbilityMontageRateWindowLifecycle& Lifecycle = MeleeAbility->GetTestRateWindowLifecycle_Mutable();
		Lifecycle.SetTestActiveContext(MeleeAbility, MockAnimInstance, ActiveMontage, TagRateWindowBegin, TagRateWindowEnd, 1.25f);
		Lifecycle.SetTestBypassMontageActiveCheck(true);

		TestTrue(TEXT("Melee: RateWindowLifecycle is bound"), Lifecycle.IsBound());
		TestEqual(TEXT("Melee: Initial active window count is 0"), Lifecycle.GetActiveWindowCount(), 0);
		TestEqual(TEXT("Melee: Baseline play rate captured as 1.25"), Lifecycle.GetBaselinePlayRate(), 1.25f);

		// Valid Begin event carrying NotifyA identity
		FGameplayEventData BeginPayload;
		BeginPayload.EventTag = TagRateWindowBegin;
		BeginPayload.Instigator = Enemy;
		BeginPayload.Target = Enemy;
		BeginPayload.OptionalObject = ActiveMontage;
		BeginPayload.OptionalObject2 = NotifyA;
		BeginPayload.EventMagnitude = 0.5f;

		Lifecycle.HandleBegin(BeginPayload);
		TestEqual(TEXT("Melee: Active window count is 1 after Begin"), Lifecycle.GetActiveWindowCount(), 1);
		TestEqual(TEXT("Melee: Target play rate is 0.5"), Lifecycle.GetCurrentTargetRate(), 0.5f);

		// Matching End event carrying NotifyA identity
		FGameplayEventData EndPayload;
		EndPayload.EventTag = TagRateWindowEnd;
		EndPayload.Instigator = Enemy;
		EndPayload.Target = Enemy;
		EndPayload.OptionalObject = ActiveMontage;
		EndPayload.OptionalObject2 = NotifyA;

		Lifecycle.HandleEnd(EndPayload);
		TestEqual(TEXT("Melee: Active window count returns to 0 after End"), Lifecycle.GetActiveWindowCount(), 0);
		TestEqual(TEXT("Melee: Target play rate restored to 1.25 baseline"), Lifecycle.GetCurrentTargetRate(), 1.25f);

		// Cleanup verification
		Lifecycle.RestoreAndClear();
		TestFalse(TEXT("Melee: Lifecycle unbound after RestoreAndClear"), Lifecycle.IsBound());
	}

	// =========================================================================
	// 4. Section Boundary 交接与活跃集合矩阵 (Section 8 Identity Matrix)
	// =========================================================================
	{
		UEnemyHitReactionAbility* BigHitAbility = NewObject<UEnemyHitReactionAbility>(Enemy);
		BigHitAbility->SetTestActorInfo(FGameplayAbilitySpecHandle(), EnemyASC->AbilityActorInfo.Get());
		BigHitAbility->SetTestAbilityActive(true);
		BigHitAbility->SetTestBypassMontageActiveCheck(true);

		FAbilityMontageRateWindowLifecycle& Lifecycle = BigHitAbility->GetTestRateWindowLifecycle_Mutable();
		Lifecycle.SetTestActiveContext(BigHitAbility, MockAnimInstance, ActiveMontage, TagRateWindowBegin, TagRateWindowEnd, 1.0f);
		Lifecycle.SetTestBypassMontageActiveCheck(true);

		// Payloads for Window A (0.5f) and Window B (0.2f)
		FGameplayEventData BeginA;
		BeginA.EventTag = TagRateWindowBegin;
		BeginA.Instigator = Enemy;
		BeginA.Target = Enemy;
		BeginA.OptionalObject = ActiveMontage;
		BeginA.OptionalObject2 = NotifyA;
		BeginA.EventMagnitude = 0.5f;

		FGameplayEventData EndA;
		EndA.EventTag = TagRateWindowEnd;
		EndA.Instigator = Enemy;
		EndA.Target = Enemy;
		EndA.OptionalObject = ActiveMontage;
		EndA.OptionalObject2 = NotifyA;

		FGameplayEventData BeginB;
		BeginB.EventTag = TagRateWindowBegin;
		BeginB.Instigator = Enemy;
		BeginB.Target = Enemy;
		BeginB.OptionalObject = ActiveMontage;
		BeginB.OptionalObject2 = NotifyB;
		BeginB.EventMagnitude = 0.2f;

		FGameplayEventData EndB;
		EndB.EventTag = TagRateWindowEnd;
		EndB.Instigator = Enemy;
		EndB.Target = Enemy;
		EndB.OptionalObject = ActiveMontage;
		EndB.OptionalObject2 = NotifyB;

		// 4.1 Section Boundary 交接吸附: A Begin(0.5) -> B Begin(0.2) -> A End -> B End
		// Core bug fix: When A ends, rate MUST remain B's 0.2f (Last-Active-Wins), NOT pop back to 0.5f.
		Lifecycle.HandleBegin(BeginA);
		TestEqual(TEXT("Boundary: Depth 1 after BeginA"), Lifecycle.GetActiveWindowCount(), 1);
		TestEqual(TEXT("Boundary: Rate is 0.5 after BeginA"), Lifecycle.GetCurrentTargetRate(), 0.5f);

		Lifecycle.HandleBegin(BeginB);
		TestEqual(TEXT("Boundary: Depth 2 after BeginB"), Lifecycle.GetActiveWindowCount(), 2);
		TestEqual(TEXT("Boundary: Rate is 0.2 after BeginB"), Lifecycle.GetCurrentTargetRate(), 0.2f);

		Lifecycle.HandleEnd(EndA);
		TestEqual(TEXT("Boundary: Depth 1 after EndA"), Lifecycle.GetActiveWindowCount(), 1);
		TestEqual(TEXT("Boundary: Rate STAYS 0.2 after EndA (Last-Active-Wins)"), Lifecycle.GetCurrentTargetRate(), 0.2f);

		Lifecycle.HandleEnd(EndB);
		TestEqual(TEXT("Boundary: Depth 0 after EndB"), Lifecycle.GetActiveWindowCount(), 0);
		TestEqual(TEXT("Boundary: Rate restored to baseline 1.0"), Lifecycle.GetCurrentTargetRate(), 1.0f);

		// 4.2 Strict Nesting: A Begin(0.5) -> B Begin(0.2) -> B End -> A End
		// Inner B ends first: must restore still-active outer A's rate (0.5f).
		Lifecycle.HandleBegin(BeginA);
		Lifecycle.HandleBegin(BeginB);
		TestEqual(TEXT("Nesting: Rate is 0.2 while both active"), Lifecycle.GetCurrentTargetRate(), 0.2f);

		Lifecycle.HandleEnd(EndB);
		TestEqual(TEXT("Nesting: Depth 1 after EndB"), Lifecycle.GetActiveWindowCount(), 1);
		TestEqual(TEXT("Nesting: Rate RESTORES 0.5 after inner EndB"), Lifecycle.GetCurrentTargetRate(), 0.5f);

		Lifecycle.HandleEnd(EndA);
		TestEqual(TEXT("Nesting: Depth 0 after EndA"), Lifecycle.GetActiveWindowCount(), 0);
		TestEqual(TEXT("Nesting: Rate restored to baseline 1.0"), Lifecycle.GetCurrentTargetRate(), 1.0f);

		// 4.3 Sequential Windows: A Begin -> A End -> B Begin -> B End
		Lifecycle.HandleBegin(BeginA);
		TestEqual(TEXT("Sequential: Rate 0.5 during A"), Lifecycle.GetCurrentTargetRate(), 0.5f);
		Lifecycle.HandleEnd(EndA);
		TestEqual(TEXT("Sequential: Rate 1.0 after A"), Lifecycle.GetCurrentTargetRate(), 1.0f);
		Lifecycle.HandleBegin(BeginB);
		TestEqual(TEXT("Sequential: Rate 0.2 during B"), Lifecycle.GetCurrentTargetRate(), 0.2f);
		Lifecycle.HandleEnd(EndB);
		TestEqual(TEXT("Sequential: Rate 1.0 after B"), Lifecycle.GetCurrentTargetRate(), 1.0f);

		// 4.4 Three-Window Cross Overlap & Non-tail End: A(0.5) -> B(0.2) -> C(0.8)
		FGameplayEventData BeginC;
		BeginC.EventTag = TagRateWindowBegin;
		BeginC.Instigator = Enemy;
		BeginC.Target = Enemy;
		BeginC.OptionalObject = ActiveMontage;
		BeginC.OptionalObject2 = NotifyC;
		BeginC.EventMagnitude = 0.8f;

		FGameplayEventData EndC;
		EndC.EventTag = TagRateWindowEnd;
		EndC.Instigator = Enemy;
		EndC.Target = Enemy;
		EndC.OptionalObject = ActiveMontage;
		EndC.OptionalObject2 = NotifyC;

		Lifecycle.HandleBegin(BeginA);
		Lifecycle.HandleBegin(BeginB);
		Lifecycle.HandleBegin(BeginC);
		TestEqual(TEXT("Three-Window: Depth 3 after all begins"), Lifecycle.GetActiveWindowCount(), 3);
		TestEqual(TEXT("Three-Window: Rate is 0.8 (latest C)"), Lifecycle.GetCurrentTargetRate(), 0.8f);

		// Remove middle window B: active winner C remains 0.8f
		Lifecycle.HandleEnd(EndB);
		TestEqual(TEXT("Three-Window: Depth 2 after removing middle window B"), Lifecycle.GetActiveWindowCount(), 2);
		TestEqual(TEXT("Three-Window: Rate STAYS 0.8 after removing B"), Lifecycle.GetCurrentTargetRate(), 0.8f);

		// Remove tail window C: active winner reverts to A (0.5f)
		Lifecycle.HandleEnd(EndC);
		TestEqual(TEXT("Three-Window: Depth 1 after removing tail C"), Lifecycle.GetActiveWindowCount(), 1);
		TestEqual(TEXT("Three-Window: Rate restores to 0.5 (A)"), Lifecycle.GetCurrentTargetRate(), 0.5f);

		Lifecycle.HandleEnd(EndA);
		TestEqual(TEXT("Three-Window: Depth 0 after removing A"), Lifecycle.GetActiveWindowCount(), 0);
		TestEqual(TEXT("Three-Window: Rate restores to baseline 1.0"), Lifecycle.GetCurrentTargetRate(), 1.0f);

		// 4.5 Idempotency & Extra Ends
		Lifecycle.HandleBegin(BeginA);
		TestEqual(TEXT("Idempotency: Depth 1 on first Begin"), Lifecycle.GetActiveWindowCount(), 1);
		Lifecycle.HandleBegin(BeginA);
		TestEqual(TEXT("Idempotency: Duplicate Begin ignored (depth remains 1)"), Lifecycle.GetActiveWindowCount(), 1);
		TestEqual(TEXT("Idempotency: Target rate unchanged at 0.5"), Lifecycle.GetCurrentTargetRate(), 0.5f);

		Lifecycle.HandleEnd(EndA);
		TestEqual(TEXT("Idempotency: Depth 0 after End"), Lifecycle.GetActiveWindowCount(), 0);
		Lifecycle.HandleEnd(EndA);
		TestEqual(TEXT("Idempotency: Extra End ignored (depth remains 0)"), Lifecycle.GetActiveWindowCount(), 0);

		// Unknown End (unregistered notify)
		UAnimNotifyState_MontageRateWindow* UnrelatedNotify = NewObject<UAnimNotifyState_MontageRateWindow>(World, TEXT("Test_UnrelatedNotify"));
		FGameplayEventData UnknownEnd = EndA;
		UnknownEnd.OptionalObject2 = UnrelatedNotify;
		Lifecycle.HandleEnd(UnknownEnd);
		TestEqual(TEXT("Idempotency: Unknown notify End ignored"), Lifecycle.GetActiveWindowCount(), 0);

		// 4.6 Fail-Closed Rejections
		// a) Bad Instigator / Target
		FGameplayEventData BadActorPayload = BeginA;
		BadActorPayload.Instigator = Player;
		Lifecycle.HandleBegin(BadActorPayload);
		TestEqual(TEXT("BigHit: Bad Instigator rejected (depth 0)"), Lifecycle.GetActiveWindowCount(), 0);

		BadActorPayload = BeginA;
		BadActorPayload.Target = Player;
		Lifecycle.HandleBegin(BadActorPayload);
		TestEqual(TEXT("BigHit: Bad Target rejected (depth 0)"), Lifecycle.GetActiveWindowCount(), 0);

		// b) Unrelated Montage
		FGameplayEventData BadMontagePayload = BeginA;
		BadMontagePayload.OptionalObject = ForeignMontage;
		Lifecycle.HandleBegin(BadMontagePayload);
		TestEqual(TEXT("BigHit: Foreign Montage rejected (depth 0)"), Lifecycle.GetActiveWindowCount(), 0);

		// c) Missing or Unregistered Notify Identity
		FGameplayEventData MissingIdentityPayload = BeginA;
		MissingIdentityPayload.OptionalObject2 = nullptr;
		Lifecycle.HandleBegin(MissingIdentityPayload);
		TestEqual(TEXT("BigHit: Missing OptionalObject2 rejected (depth 0)"), Lifecycle.GetActiveWindowCount(), 0);

		FGameplayEventData UnregisteredIdentityPayload = BeginA;
		UnregisteredIdentityPayload.OptionalObject2 = UnrelatedNotify;
		Lifecycle.HandleBegin(UnregisteredIdentityPayload);
		TestEqual(TEXT("BigHit: Unregistered OptionalObject2 rejected (depth 0)"), Lifecycle.GetActiveWindowCount(), 0);

		// d) Negative / Zero / NaN magnitude
		FGameplayEventData BadMagPayload = BeginA;
		BadMagPayload.EventMagnitude = -0.5f;
		Lifecycle.HandleBegin(BadMagPayload);
		TestEqual(TEXT("BigHit: Negative magnitude rejected (depth 0)"), Lifecycle.GetActiveWindowCount(), 0);

		BadMagPayload.EventMagnitude = 0.0f;
		Lifecycle.HandleBegin(BadMagPayload);
		TestEqual(TEXT("BigHit: Zero magnitude rejected (depth 0)"), Lifecycle.GetActiveWindowCount(), 0);

		BadMagPayload.EventMagnitude = std::numeric_limits<float>::quiet_NaN();
		Lifecycle.HandleBegin(BadMagPayload);
		TestEqual(TEXT("BigHit: NaN magnitude rejected (depth 0)"), Lifecycle.GetActiveWindowCount(), 0);

		Lifecycle.RestoreAndClear();
	}

	// =========================================================================
	// 5. Small Hit Reaction token-filter unit test (does not exercise ASC retrigger)
	// =========================================================================
	{
		UEnemySmallHitReactionAbility* SmallHitAbility = NewObject<UEnemySmallHitReactionAbility>(Enemy);
		SmallHitAbility->SetTestActorInfo(FGameplayAbilitySpecHandle(), EnemyASC->AbilityActorInfo.Get());
		SmallHitAbility->SetTestAbilityActive(true);
		SmallHitAbility->SetTestBypassMontageActiveCheck(true);
		SmallHitAbility->SetTestBoundAnimInstance(MockAnimInstance);
		SmallHitAbility->SetTestActiveMontage(ActiveMontage);
		SmallHitAbility->SetTestActiveMontageInstanceID(1);

		FAbilityMontageRateWindowLifecycle& Lifecycle = SmallHitAbility->GetTestRateWindowLifecycle_Mutable();
		Lifecycle.SetTestActiveContext(SmallHitAbility, MockAnimInstance, ActiveMontage, TagRateWindowBegin, TagRateWindowEnd, 1.0f);
		Lifecycle.SetTestBypassMontageActiveCheck(true);

		// Context 1 simulates token 1 activation
		UEnemySmallHitReactionRateWindowContext* Context1 = NewObject<UEnemySmallHitReactionRateWindowContext>(SmallHitAbility);
		Context1->OwningAbility = SmallHitAbility;
		Context1->Token = 1;

		FGameplayEventData ValidBeginA;
		ValidBeginA.EventTag = TagRateWindowBegin;
		ValidBeginA.Instigator = Enemy;
		ValidBeginA.Target = Enemy;
		ValidBeginA.OptionalObject = ActiveMontage;
		ValidBeginA.OptionalObject2 = NotifyA;
		ValidBeginA.EventMagnitude = 0.4f;

		// Token mismatch when ability token is 0 -> dropped
		TestEqual(TEXT("SmallHit: Initial Ability Token is 0"), SmallHitAbility->GetTestCurrentActivationToken(), 0u);
		Context1->OnRateWindowBegin(ValidBeginA);
		TestEqual(TEXT("SmallHit: Mismatched token drops event (depth 0)"), Lifecycle.GetActiveWindowCount(), 0);

		// Matched token 1 -> routes to ability and pushes rate
		SmallHitAbility->SetTestCurrentActivationToken(1);
		Context1->OnRateWindowBegin(ValidBeginA);
		TestEqual(TEXT("SmallHit: Matched token 1 applies event (depth 1)"), Lifecycle.GetActiveWindowCount(), 1);

		// Change only the token to isolate filtering; real retrigger cleanup is covered below.
		SmallHitAbility->SetTestCurrentActivationToken(2);
		UEnemySmallHitReactionRateWindowContext* Context2 = NewObject<UEnemySmallHitReactionRateWindowContext>(SmallHitAbility);
		Context2->OwningAbility = SmallHitAbility;
		Context2->Token = 2;

		// Old Context1 with Token 1 is now stale -> dropped by Ability
		Context1->OnRateWindowBegin(ValidBeginA);
		TestEqual(TEXT("SmallHit: Stale Token 1 event dropped by Token 2 ability (depth unchanged at 1)"), Lifecycle.GetActiveWindowCount(), 1);

		// Active Context2 with Token 2 can push rate for window B
		FGameplayEventData ValidBeginB;
		ValidBeginB.EventTag = TagRateWindowBegin;
		ValidBeginB.Instigator = Enemy;
		ValidBeginB.Target = Enemy;
		ValidBeginB.OptionalObject = ActiveMontage;
		ValidBeginB.OptionalObject2 = NotifyB;
		ValidBeginB.EventMagnitude = 0.2f;

		Context2->OnRateWindowBegin(ValidBeginB);
		TestEqual(TEXT("SmallHit: Active Token 2 event pushed (depth 2)"), Lifecycle.GetActiveWindowCount(), 2);

		Lifecycle.RestoreAndClear();
	}

	// =========================================================================
	// 6. Launch Reaction (UEnemyLaunchReactionAbility) RateWindow Integration
	// =========================================================================
	{
		UEnemyLaunchReactionAbility* LaunchAbility = NewObject<UEnemyLaunchReactionAbility>(Enemy);
		LaunchAbility->SetTestActorInfo(FGameplayAbilitySpecHandle(), EnemyASC->AbilityActorInfo.Get());
		LaunchAbility->SetTestAbilityActive(true);
		LaunchAbility->SetTestBypassMontageActiveCheck(true);

		FAbilityMontageRateWindowLifecycle& Lifecycle = LaunchAbility->GetTestRateWindowLifecycle_Mutable();
		Lifecycle.SetTestActiveContext(LaunchAbility, MockAnimInstance, ActiveMontage, TagRateWindowBegin, TagRateWindowEnd, 1.0f);
		Lifecycle.SetTestBypassMontageActiveCheck(true);

		FGameplayEventData LaunchBegin;
		LaunchBegin.EventTag = TagRateWindowBegin;
		LaunchBegin.Instigator = Enemy;
		LaunchBegin.Target = Enemy;
		LaunchBegin.OptionalObject = ActiveMontage;
		LaunchBegin.OptionalObject2 = NotifyA;
		LaunchBegin.EventMagnitude = 0.2f;

		Lifecycle.HandleBegin(LaunchBegin);
		TestEqual(TEXT("Launch: Depth is 1 after RateWindow Begin"), Lifecycle.GetActiveWindowCount(), 1);

		// Recovery transition / cleanup restores rate
		Lifecycle.RestoreAndClear();
		TestFalse(TEXT("Launch: Lifecycle unbound after cleanup"), Lifecycle.IsBound());
		TestEqual(TEXT("Launch: Active window count 0 after cleanup"), Lifecycle.GetActiveWindowCount(), 0);
	}

	// =========================================================================
	// 7. Victim Execution (UEnemyVictimExecutionAbility) Delayed Binding & Lethal Guard
	// =========================================================================
	{
		UEnemyVictimExecutionAbility* VictimAbility = NewObject<UEnemyVictimExecutionAbility>(Enemy);
		VictimAbility->SetTestActorInfo(FGameplayAbilitySpecHandle(), EnemyASC->AbilityActorInfo.Get());
		VictimAbility->SetTestAbilityActive(true);
		VictimAbility->SetTestBypassMontageActiveCheck(true);

		// 7.1 Stage 1: Victim locked but NOT in non-lethal recovery
		TestFalse(TEXT("Victim: Not in non-lethal recovery initially"), VictimAbility->IsTestNonLethalRecoveryActive());
		TestNull(TEXT("Victim: RateWindowContext is null before recovery"), VictimAbility->GetTestActiveRateWindowContext());

		// 7.2 Stage 2: Non-lethal recovery entered
		VictimAbility->SetTestNonLethalRecovery(true, Player);
		TestTrue(TEXT("Victim: Non-lethal recovery active"), VictimAbility->IsTestNonLethalRecoveryActive());

		FAbilityMontageRateWindowLifecycle& Lifecycle = VictimAbility->GetTestRateWindowLifecycle_Mutable();
		Lifecycle.SetTestActiveContext(VictimAbility, MockAnimInstance, ActiveMontage, TagRateWindowBegin, TagRateWindowEnd, 1.0f);
		Lifecycle.SetTestBypassMontageActiveCheck(true);

		FGameplayEventData VictimBegin;
		VictimBegin.EventTag = TagRateWindowBegin;
		VictimBegin.Instigator = Enemy;
		VictimBegin.Target = Enemy;
		VictimBegin.OptionalObject = ActiveMontage;
		VictimBegin.OptionalObject2 = NotifyA;
		VictimBegin.EventMagnitude = 0.5f;

		Lifecycle.HandleBegin(VictimBegin);
		TestEqual(TEXT("Victim: RateWindow Begin accepted during non-lethal recovery (depth 1)"), Lifecycle.GetActiveWindowCount(), 1);

		FGameplayEventData VictimEnd;
		VictimEnd.EventTag = TagRateWindowEnd;
		VictimEnd.Instigator = Enemy;
		VictimEnd.Target = Enemy;
		VictimEnd.OptionalObject = ActiveMontage;
		VictimEnd.OptionalObject2 = NotifyA;

		Lifecycle.HandleEnd(VictimEnd);
		TestEqual(TEXT("Victim: RateWindow End restores baseline (depth 0)"), Lifecycle.GetActiveWindowCount(), 0);

		// 7.3 Stage 3: Lethal guard (DeathPending)
		VictimAbility->SetTestDeathPending(true);
		VictimAbility->SetTestNonLethalRecovery(false, nullptr);
		TestFalse(TEXT("Victim: Non-lethal recovery is false when DeathPending"), VictimAbility->IsTestNonLethalRecoveryActive());

		Lifecycle.RestoreAndClear();
		TestFalse(TEXT("Victim: Lifecycle unbound after cleanup"), Lifecycle.IsBound());
	}

	// =========================================================================
	// 8. Notify-to-ASC payload transport unit test (no active Ability consumer)
	// =========================================================================
	{
		// Test that calling NotifyBegin and NotifyEnd on UAnimNotifyState_MontageRateWindow
		// genuinely populates OptionalObject2 and transmits the gameplay event to the ASC.
		FGameplayEventData ReceivedBeginPayload;
		bool bBeginReceived = false;
		FGameplayEventData ReceivedEndPayload;
		bool bEndReceived = false;

		FDelegateHandle BeginHandle = EnemyASC->GenericGameplayEventCallbacks.FindOrAdd(TagRateWindowBegin).AddLambda(
			[&ReceivedBeginPayload, &bBeginReceived](const FGameplayEventData* Payload)
			{
				if (Payload)
				{
					ReceivedBeginPayload = *Payload;
					bBeginReceived = true;
				}
			});

		FDelegateHandle EndHandle = EnemyASC->GenericGameplayEventCallbacks.FindOrAdd(TagRateWindowEnd).AddLambda(
			[&ReceivedEndPayload, &bEndReceived](const FGameplayEventData* Payload)
			{
				if (Payload)
				{
					ReceivedEndPayload = *Payload;
					bEndReceived = true;
				}
			});

		// Trigger NotifyBegin directly on authored NotifyA
		const FAnimNotifyEventReference EventRef;
		NotifyA->NotifyBegin(Enemy->GetMesh(), ActiveMontage, 1.0f, EventRef);

		TestTrue(TEXT("Transmission: ASC received RateWindowBegin event"), bBeginReceived);
		TestTrue(TEXT("Transmission: Begin event has valid OptionalObject (ActiveMontage)"), ReceivedBeginPayload.OptionalObject.Get() == ActiveMontage);
		TestTrue(TEXT("Transmission: Begin event carries NotifyA in OptionalObject2"), ReceivedBeginPayload.OptionalObject2.Get() == NotifyA);
		TestEqual(TEXT("Transmission: Begin event magnitude equals NotifyA RateMultiplier"), ReceivedBeginPayload.EventMagnitude, 0.5f);

		// Trigger NotifyEnd directly on authored NotifyA
		NotifyA->NotifyEnd(Enemy->GetMesh(), ActiveMontage, EventRef);

		TestTrue(TEXT("Transmission: ASC received RateWindowEnd event"), bEndReceived);
		TestTrue(TEXT("Transmission: End event has valid OptionalObject (ActiveMontage)"), ReceivedEndPayload.OptionalObject.Get() == ActiveMontage);
		TestTrue(TEXT("Transmission: End event carries NotifyA in OptionalObject2"), ReceivedEndPayload.OptionalObject2.Get() == NotifyA);

		// Clean up delegate handles
		EnemyASC->GenericGameplayEventCallbacks.FindOrAdd(TagRateWindowBegin).Remove(BeginHandle);
		EnemyASC->GenericGameplayEventCallbacks.FindOrAdd(TagRateWindowEnd).Remove(EndHandle);
	}

#if WITH_EDITOR
	// =========================================================================
	// 9. Real ASC activation/retrigger and Notify -> task -> Ability -> montage rate
	// =========================================================================
	{
		UAnimMontage* FrontMontage = EnemyMontageRateWindowAutomation::CreatePlayableRateMontage(*this, World);
		if (!TestNotNull(TEXT("Runtime: playable front montage created"), FrontMontage))
		{
			return false;
		}
		const float FixtureMontageLength = FrontMontage->GetPlayLength();
		if (!TestTrue(TEXT("Runtime fixture: front montage has positive duration"), FixtureMontageLength > 0.0f))
		{
			return false;
		}
		UAnimMontage* RightMontage = DuplicateObject<UAnimMontage>(FrontMontage, World);
		AEnemyCharacter* RuntimeEnemy = FCombatAutomationFixture::SpawnPassiveEnemy(
			World, FTransform(FRotator::ZeroRotator, FVector(500.0f, 0.0f, 0.0f)));
		if (!TestNotNull(TEXT("Runtime: right montage created"), RightMontage)
			|| !TestNotNull(TEXT("Runtime: isolated enemy spawned"), RuntimeEnemy))
		{
			return false;
		}
		// Duplicating the empty montage data model emits Populated and can overwrite
		// the manually authored SequenceLength. Restore it after duplication completes.
		EnemyMontageRateWindowAutomation::UTestMontageLengthAccess::SetLength(RightMontage, FixtureMontageLength);
		if (!TestEqual(TEXT("Runtime fixture: right montage preserves playback duration"), RightMontage->GetPlayLength(), FixtureMontageLength)
			|| !TestEqual(TEXT("Runtime fixture: right montage has exactly one slot"), RightMontage->SlotAnimTracks.Num(), 1)
			|| !TestEqual(TEXT("Runtime fixture: right montage slot has one animation segment"),
				RightMontage->SlotAnimTracks[0].AnimTrack.AnimSegments.Num(), 1))
		{
			return false;
		}

		USkeletalMeshComponent* Mesh = RuntimeEnemy->GetMesh();
		UAbilitySystemComponent* RuntimeASC = RuntimeEnemy->GetAbilitySystemComponent();
		if (!TestNotNull(TEXT("Runtime: mesh exists"), Mesh)
			|| !TestNotNull(TEXT("Runtime: ASC exists"), RuntimeASC))
		{
			return false;
		}
		Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered;
		UAnimInstance* Anim = NewObject<UAnimInstance>(Mesh);
		Anim->InitializeMontageOnly();
		Anim->CurrentSkeleton = FrontMontage->GetSkeleton();
		Mesh->AnimScriptInstance = Anim;
		RuntimeASC->RefreshAbilityActorInfo();
		if (!TestTrue(TEXT("Runtime fixture: ASC resolves the montage-only AnimInstance"),
			RuntimeASC->AbilityActorInfo.IsValid() && RuntimeASC->AbilityActorInfo->GetAnimInstance() == Anim))
		{
			return false;
		}

		const FGameplayTag SmallEventTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Reaction.Enemy.Small"));
		const FGameplayTag SmallStateTag = FGameplayTag::RequestGameplayTag(TEXT("State.Action.SmallHitReacting"));
		const FGameplayAbilitySpecHandle Handle = RuntimeASC->GiveAbility(
			FGameplayAbilitySpec(UEnemySmallHitReactionAbility::StaticClass(), 1, INDEX_NONE, RuntimeEnemy));
		const FTransform OriginalPlayerTransform = Player->GetActorTransform();
		TArray<FString> Transitions;
		FDelegateHandle ActivatedHandle;
		FDelegateHandle EndedHandle;
		ON_SCOPE_EXIT
		{
			RuntimeASC->AbilityActivatedCallbacks.Remove(ActivatedHandle);
			RuntimeASC->AbilityEndedCallbacks.Remove(EndedHandle);
			RuntimeASC->CancelAbilityHandle(Handle);
			RuntimeASC->ClearAbility(Handle);
			Anim->Montage_Stop(0.0f);
			Player->SetActorTransform(OriginalPlayerTransform);
		};

		FGameplayAbilitySpec* Spec = RuntimeASC->FindAbilitySpecFromHandle(Handle);
		UEnemySmallHitReactionAbility* Ability = Spec
			? Cast<UEnemySmallHitReactionAbility>(Spec->GetPrimaryInstance()) : nullptr;
		if (!TestNotNull(TEXT("Runtime: ASC created the instanced ability"), Ability))
		{
			return false;
		}
		Ability->SetTestFrontSmallHitReactionMontage(FrontMontage);
		Ability->SetTestBackSmallHitReactionMontage(FrontMontage);
		Ability->SetTestLeftSmallHitReactionMontage(RightMontage);
		Ability->SetTestRightSmallHitReactionMontage(RightMontage);
		FAbilityMontageRateWindowLifecycle& Lifecycle = Ability->GetTestRateWindowLifecycle_Mutable();
		int32 EndingInstanceID = INDEX_NONE;
		float ExpectedRestoredRate = 1.0f;
		bool bExpectRestoreBeforeStop = true;
		ActivatedHandle = RuntimeASC->AbilityActivatedCallbacks.AddLambda([&](UGameplayAbility* Activated)
		{
			if (Activated == Ability)
			{
				Transitions.Add(TEXT("Started"));
			}
		});
		EndedHandle = RuntimeASC->AbilityEndedCallbacks.AddLambda([&](UGameplayAbility* Ended)
		{
			if (Ended != Ability)
			{
				return;
			}
			Transitions.Add(TEXT("Ended"));
			TestFalse(TEXT("Runtime End: lifecycle unbound before next activation"), Lifecycle.IsBound());
			TestEqual(TEXT("Runtime End: no active windows"), Lifecycle.GetActiveWindowCount(), 0);
			TestNull(TEXT("Runtime End: context released"), Ability->GetTestActiveRateWindowContext());
			TestNull(TEXT("Runtime End: montage task released"), Ability->GetTestMontageTask());
			TestFalse(TEXT("Runtime End: activation-owned tag removed"), RuntimeASC->HasMatchingGameplayTag(SmallStateTag));
			for (const FGameplayTag Tag : { TagRateWindowBegin, TagRateWindowEnd })
			{
				const auto* EventDelegate = RuntimeASC->GenericGameplayEventCallbacks.Find(Tag);
				TestTrue(TEXT("Runtime End: RateWindow listener unregistered"), !EventDelegate || !EventDelegate->IsBound());
			}
			// No animation advance occurs here, so the stopped instance remains inspectable.
			const FAnimMontageInstance* EndedInstance = Anim->GetMontageInstanceForID(EndingInstanceID);
			if (TestNotNull(TEXT("Runtime End: previous montage instance still exists"), EndedInstance))
			{
				if (bExpectRestoreBeforeStop)
				{
					TestEqual(TEXT("Runtime retrigger End: previous instance restored before stop"), EndedInstance->GetPlayRate(), ExpectedRestoredRate);
				}
				TestTrue(TEXT("Runtime End: previous instance stopped"), EndedInstance->IsStopped());
			}
		});

		FGameplayEventData Hit;
		Hit.EventTag = SmallEventTag;
		Hit.Instigator = Player;
		Hit.Target = RuntimeEnemy;
		auto TriggerHit = [&](const FVector& AttackerDirection)
		{
			Player->SetActorLocation(RuntimeEnemy->GetActorLocation() + AttackerDirection * 100.0f);
			return TestEqual(TEXT("Runtime: hit event activates exactly one ability"),
				RuntimeASC->HandleGameplayEvent(SmallEventTag, &Hit), 1);
		};
		auto CheckActiveMontage = [&](UAnimMontage* ExpectedMontage)
		{
			if (!TestTrue(TEXT("Runtime: ability is active"), Ability->IsActive())
				|| !TestNotNull(TEXT("Runtime: live montage task exists"), Ability->GetTestMontageTask())
				|| !TestNotNull(TEXT("Runtime: live event context exists"), Ability->GetTestActiveRateWindowContext()))
			{
				return false;
			}
			TestTrue(TEXT("Runtime: montage task is active"), Ability->GetTestMontageTask()->IsActive());
			TestTrue(TEXT("Runtime: selected directional montage matches"), Ability->GetTestActiveMontage() == ExpectedMontage);
			TestTrue(TEXT("Runtime: montage is really active"), Anim->Montage_IsActive(ExpectedMontage));
			TestFalse(TEXT("Runtime: montage checks are not bypassed"), Ability->GetTestBypassMontageActiveCheck());
			TestTrue(TEXT("Runtime: activation-owned tag present"), RuntimeASC->HasMatchingGameplayTag(SmallStateTag));
			TestTrue(TEXT("Runtime: helper bound by activation"), Lifecycle.IsBound());
			TestEqual(TEXT("Runtime: activation starts with no windows"), Lifecycle.GetActiveWindowCount(), 0);
			TestEqual(TEXT("Runtime: activation captures a fresh baseline"), Lifecycle.GetBaselinePlayRate(), 1.0f);
			TestEqual(TEXT("Runtime: actual activation rate is 1.0"), Anim->Montage_GetPlayRate(ExpectedMontage), 1.0f);
			const FAnimMontageInstance* Instance = Anim->GetActiveInstanceForMontage(ExpectedMontage);
			if (!TestNotNull(TEXT("Runtime: real montage instance exists"), Instance))
			{
				return false;
			}
			TestEqual(TEXT("Runtime: ability captured the real instance ID"), Ability->GetTestActiveMontageInstanceID(), Instance->GetInstanceID());
			return true;
		};
		auto SendNotify = [&](UAnimMontage* Montage, int32 NotifyIndex, bool bBegin, int32 ExpectedCount, float ExpectedRate)
		{
			FAnimNotifyEvent* NotifyEvent = EnemyMontageRateWindowAutomation::FindRateWindowEvent(Montage, NotifyIndex);
			if (!TestNotNull(TEXT("Runtime fixture: named rate window exists"), NotifyEvent))
			{
				return;
			}
			UAnimNotifyState_MontageRateWindow* Notify = Cast<UAnimNotifyState_MontageRateWindow>(NotifyEvent->NotifyStateClass);
			if (!TestNotNull(TEXT("Runtime: montage contains the rate notify"), Notify))
			{
				return;
			}
			if (!TestEqual(TEXT("Runtime fixture: named window retains its authored rate"),
				Notify->RateMultiplier, NotifyIndex == 0 ? 0.5f : 0.2f))
			{
				return;
			}
			const FAnimNotifyEventReference EventRef(NotifyEvent, Montage);
			// Drive Notify callbacks deliberately; this tests delivery/consumption, not timeline scheduling.
			if (bBegin)
			{
				Notify->NotifyBegin(Mesh, Montage, 1.0f, EventRef);
			}
			else
			{
				Notify->NotifyEnd(Mesh, Montage, EventRef);
			}
			const FString Step = FString::Printf(TEXT("Runtime %s Notify %s %s"), *Montage->GetName(),
				*Notify->GetName(), bBegin ? TEXT("Begin") : TEXT("End"));
			TestEqual(Step + TEXT(": active windows"), Lifecycle.GetActiveWindowCount(), ExpectedCount);
			TestEqual(Step + TEXT(": actual montage play rate"), Anim->Montage_GetPlayRate(Montage), ExpectedRate);
		};

		AddInfo(FString::Printf(TEXT("Runtime activation: initial front montage %s"), *FrontMontage->GetName()));
		if (!TriggerHit(RuntimeEnemy->GetActorForwardVector()) || !CheckActiveMontage(FrontMontage))
		{
			return false;
		}
		// Exercise capture of a non-unit rate on a real instance, with production listeners still active.
		Lifecycle.RestoreAndClear();
		Anim->Montage_SetPlayRate(FrontMontage, 1.25f);
		Lifecycle.BindAndCapture(Ability, Anim, FrontMontage, TagRateWindowBegin, TagRateWindowEnd);
		TestEqual(TEXT("Runtime: BindAndCapture reads actual 1.25 baseline"), Lifecycle.GetBaselinePlayRate(), 1.25f);
		ExpectedRestoredRate = 1.25f;
		SendNotify(FrontMontage, 0, true, 1, 0.5f);
		SendNotify(FrontMontage, 1, true, 2, 0.2f);
		SendNotify(FrontMontage, 0, false, 1, 0.2f); // B Begin before A End must keep B.
		SendNotify(FrontMontage, 1, false, 0, 1.25f);
		SendNotify(FrontMontage, 0, true, 1, 0.5f);
		SendNotify(FrontMontage, 1, true, 2, 0.2f);
		SendNotify(FrontMontage, 1, false, 1, 0.5f); // True nesting restores the surviving A.
		SendNotify(FrontMontage, 0, false, 0, 1.25f);
		SendNotify(FrontMontage, 0, true, 1, 0.5f); // Retrigger while a window is open.

		for (int32 RetriggerIndex = 0; RetriggerIndex < 2; ++RetriggerIndex)
		{
			const uint32 OldToken = Ability->GetTestCurrentActivationToken();
			EndingInstanceID = Ability->GetTestActiveMontageInstanceID();
			TStrongObjectPtr<UEnemySmallHitReactionRateWindowContext> OldContext(Ability->GetTestActiveRateWindowContext());
			TStrongObjectPtr<UAbilityTask_PlayMontageAndWait> OldTask(Ability->GetTestMontageTask());
			UAnimMontage* NextMontage = RetriggerIndex == 0 ? FrontMontage : RightMontage;
			const FVector Direction = RetriggerIndex == 0 ? RuntimeEnemy->GetActorForwardVector() : RuntimeEnemy->GetActorRightVector();
			AddInfo(FString::Printf(TEXT("Runtime retrigger: %s montage %s"),
				RetriggerIndex == 0 ? TEXT("same front") : TEXT("switch right"), *NextMontage->GetName()));
			if (!TriggerHit(Direction) || !CheckActiveMontage(NextMontage))
			{
				return false;
			}
			TestEqual(TEXT("Runtime retrigger: End precedes the next activation"), FString::Join(Transitions, TEXT(",")),
				RetriggerIndex == 0 ? FString(TEXT("Started,Ended,Started")) : FString(TEXT("Started,Ended,Started,Ended,Started")));
			TestTrue(TEXT("Runtime retrigger: same ability object reused"), RuntimeASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance() == Ability);
			TestTrue(TEXT("Runtime retrigger: new montage instance ID"), Ability->GetTestActiveMontageInstanceID() != EndingInstanceID);
			TestTrue(TEXT("Runtime retrigger: activation token advanced"), Ability->GetTestCurrentActivationToken() > OldToken);
			TestFalse(TEXT("Runtime retrigger: old context invalidated"), OldContext->OwningAbility.IsValid());
			TestTrue(TEXT("Runtime retrigger: old montage task finished"), OldTask->IsFinished());
			ExpectedRestoredRate = 1.0f;
			SendNotify(NextMontage, 1, true, 1, 0.2f);
			FGameplayEventData LateEnd;
			LateEnd.EventTag = TagRateWindowEnd;
			LateEnd.Instigator = RuntimeEnemy;
			LateEnd.Target = RuntimeEnemy;
			LateEnd.OptionalObject = NextMontage;
			const FAnimNotifyEvent* CurrentWindowB = EnemyMontageRateWindowAutomation::FindRateWindowEvent(NextMontage, 1);
			if (!TestNotNull(TEXT("Runtime retrigger: current window B exists"), CurrentWindowB))
			{
				return false;
			}
			LateEnd.OptionalObject2 = CurrentWindowB->NotifyStateClass;
			OldContext->OnRateWindowEnd(LateEnd);
			TestEqual(TEXT("Runtime retrigger: stale context cannot remove current window"), Lifecycle.GetActiveWindowCount(), 1);
			TestEqual(TEXT("Runtime retrigger: stale context cannot change current rate"), Anim->Montage_GetPlayRate(NextMontage), 0.2f);
		}

		EndingInstanceID = Ability->GetTestActiveMontageInstanceID();
		// GAS Cancel broadcasts to MontageTask first, which stops playback before EndAbility.
		// This path must clear state/listeners and leave subsequent playback untouched; an
		// already stopped instance is not eligible for the active-instance rate restoration.
		bExpectRestoreBeforeStop = false;
		RuntimeASC->CancelAbilityHandle(Handle);
		TestFalse(TEXT("Runtime cancel: ability inactive"), Ability->IsActive());
		TestFalse(TEXT("Runtime cancel: montage inactive"), Anim->Montage_IsActive(RightMontage));
		TestEqual(TEXT("Runtime cancel: one End for each activation"), FString::Join(Transitions, TEXT(",")),
			FString(TEXT("Started,Ended,Started,Ended,Started,Ended")));
		if (TestTrue(TEXT("Runtime cancel: unrelated playback starts"), Anim->Montage_Play(RightMontage, 1.5f) > 0.0f))
		{
			SendNotify(RightMontage, 0, true, 0, 1.5f);
			SendNotify(RightMontage, 0, false, 0, 1.5f);
		}
	}
#endif

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
