#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystem/Abilities/EnemyHitReactionAbility.h"
#include "AbilitySystem/Abilities/EnemyLaunchReactionAbility.h"
#include "AbilitySystem/Abilities/EnemyMeleeAbility.h"
#include "AbilitySystem/Abilities/EnemySmallHitReactionAbility.h"
#include "AbilitySystem/Tasks/AbilityTask_PlayActionMontage.h"
#include "Tests/TestManagedMontageAbility.h"
#include "AbilitySystem/Abilities/EnemyVictimExecutionAbility.h"
#include "AbilitySystem/Abilities/MontageRateWindowLifecycle.h"
#include "AI/EnemyAIController.h"
#include "AI/EnemyAIProfile.h"
#include "Combat/Enemy/EnemyAttackProfile.h"
#include "Combat/Enemy/EnemyAttackSet.h"
#include "Tests/TestProjectileDamageGE.h"
#include "Animation/AnimComposite.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifyQueue.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Animation/Combat/AnimNotifyState_ActionWindows.h"
#include "Animation/ActiveMontageInstanceScope.h"
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
		TestNull(TEXT("EnemyMelee CDO has no runtime window Task"), MeleeCDO->GetTestMontageTask());
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
		TestNull(TEXT("EnemyHitReaction CDO has no runtime window Task"), BigHitCDO->GetTestMontageTask());
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
		TestNull(TEXT("EnemyLaunchReaction CDO has no runtime window Task"), LaunchCDO->GetTestMontageTask());
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
		// Begin/End configuration assertions now inspect actual Task subscriptions in section 7.
		TestNull(TEXT("EnemyVictimExecution CDO has no standard montage Task"), VictimCDO->GetTestVictimMontageTask());
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
	// 2.1 Helper Seam Verification: Mock Environment Bypass vs Non-Bypass & Invalid Object Tests
	// =========================================================================
	{
		UEnemyMeleeAbility* SeamAbility = NewObject<UEnemyMeleeAbility>(Enemy);
		SeamAbility->SetTestActorInfo(FGameplayAbilitySpecHandle(), EnemyASC->AbilityActorInfo.Get());
		SeamAbility->SetTestAbilityActive(true);

		// Finding 1 Negative: Non-null but invalid (MarkAsGarbage) UObject inputs must be rejected
		{
			UAnimInstance* GarbageAnimInstance = NewObject<UAnimInstance>(Enemy->GetMesh());
			GarbageAnimInstance->MarkAsGarbage();
			TestFalse(TEXT("Helper: IsCurrentMontageInstance rejects garbage AnimInstance"),
				FAbilityMontageRateWindowLifecycle::IsCurrentMontageInstance(GarbageAnimInstance, ActiveMontage, 1));

			UAnimMontage* GarbageMontage = NewObject<UAnimMontage>(GetTransientPackage());
			GarbageMontage->MarkAsGarbage();
			TestFalse(TEXT("Helper: IsCurrentMontageInstance rejects garbage Montage"),
				FAbilityMontageRateWindowLifecycle::IsCurrentMontageInstance(MockAnimInstance, GarbageMontage, 1));

			FAbilityMontageRateWindowLifecycle GarbageLifecycle;
			GarbageLifecycle.BindAndCapture(SeamAbility, GarbageAnimInstance, ActiveMontage, TagRateWindowBegin, TagRateWindowEnd);
			TestFalse(TEXT("Helper: BindAndCapture rejects garbage AnimInstance"), GarbageLifecycle.IsBound());

			GarbageLifecycle.BindAndCapture(SeamAbility, MockAnimInstance, GarbageMontage, TagRateWindowBegin, TagRateWindowEnd);
			TestFalse(TEXT("Helper: BindAndCapture rejects garbage Montage"), GarbageLifecycle.IsBound());
		}

		// Finding 2 Negative: Bypass false vs true without real playing instance
		{
			FAbilityMontageRateWindowLifecycle NoBypassLifecycle;
			NoBypassLifecycle.SetTestBypassMontageActiveCheck(false);
			NoBypassLifecycle.BindAndCapture(SeamAbility, MockAnimInstance, ActiveMontage, TagRateWindowBegin, TagRateWindowEnd);
			TestFalse(TEXT("Helper: BindAndCapture rejects when bypass is false and no real instance"), NoBypassLifecycle.IsBound());
			TestEqual(TEXT("Helper: Bound ID is INDEX_NONE when bypass is false"), NoBypassLifecycle.GetBoundMontageInstanceID(), static_cast<int32>(INDEX_NONE));

			FAbilityMontageRateWindowLifecycle BypassLifecycle;
			BypassLifecycle.SetTestBypassMontageActiveCheck(true);
			BypassLifecycle.BindAndCapture(SeamAbility, MockAnimInstance, ActiveMontage, TagRateWindowBegin, TagRateWindowEnd);
			TestTrue(TEXT("Helper: BindAndCapture succeeds when bypass is true"), BypassLifecycle.IsBound());
			TestEqual(TEXT("Helper: Bound ID remains INDEX_NONE under bypass without real instance"), BypassLifecycle.GetBoundMontageInstanceID(), static_cast<int32>(INDEX_NONE));
			BypassLifecycle.RestoreAndClear();
			TestFalse(TEXT("Helper: Lifecycle is not bound after RestoreAndClear"), BypassLifecycle.IsBound());
		}

		FAbilityMontageRateWindowLifecycle SeamLifecycle;
		SeamLifecycle.SetTestActiveContext(SeamAbility, MockAnimInstance, ActiveMontage, TagRateWindowBegin, TagRateWindowEnd, 1.0f);
		TestEqual(TEXT("Seam: Bound ID is INDEX_NONE with mock instance"), SeamLifecycle.GetBoundMontageInstanceID(), static_cast<int32>(INDEX_NONE));

		// When bypass is false, Begin should be rejected without altering collection
		FGameplayEventData BeginPayload;
		BeginPayload.EventTag = TagRateWindowBegin;
		BeginPayload.Instigator = Enemy;
		BeginPayload.Target = Enemy;
		BeginPayload.OptionalObject = ActiveMontage;
		BeginPayload.OptionalObject2 = NotifyA;
		BeginPayload.EventMagnitude = 0.5f;

		SeamLifecycle.HandleBegin(BeginPayload);
		TestEqual(TEXT("Seam: active window count remains 0 when bypass is false and no real instance"), SeamLifecycle.GetActiveWindowCount(), 0);

		// When bypass is enabled, Begin succeeds
		SeamLifecycle.SetTestBypassMontageActiveCheck(true);
		SeamLifecycle.HandleBegin(BeginPayload);
		TestEqual(TEXT("Seam: active window count is 1 when bypass is true"), SeamLifecycle.GetActiveWindowCount(), 1);

		// Clear resets state idempotently
		SeamLifecycle.RestoreAndClear();
		TestEqual(TEXT("Seam: active window count is 0 after Clear"), SeamLifecycle.GetActiveWindowCount(), 0);
		TestFalse(TEXT("Seam: lifecycle is not bound after Clear"), SeamLifecycle.IsBound());
	}

	// =========================================================================
	// 2.2 Helper Direct Multi-Instance Authorization & Play Rate Protection
	// =========================================================================
#if WITH_EDITOR // The playable fixture authors animation data through the Editor controller.
	{
		UAnimMontage* DirectPlayableMontage = EnemyMontageRateWindowAutomation::CreatePlayableRateMontage(*this, World);
		if (TestNotNull(TEXT("Helper Direct: Playable montage created"), DirectPlayableMontage))
		{
			USkeletalMeshComponent* Mesh = Enemy->GetMesh();
			Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered;
			UAnimInstance* RealAnimInstance = NewObject<UAnimInstance>(Mesh);
			RealAnimInstance->InitializeMontageOnly();
			RealAnimInstance->CurrentSkeleton = DirectPlayableMontage->GetSkeleton();
			Mesh->AnimScriptInstance = RealAnimInstance;

			UEnemyMeleeAbility* DirectAbility = NewObject<UEnemyMeleeAbility>(Enemy);
			DirectAbility->SetTestActorInfo(FGameplayAbilitySpecHandle(), EnemyASC->AbilityActorInfo.Get());
			DirectAbility->SetTestAbilityActive(true);

			FAbilityMontageRateWindowLifecycle DirectHelper;
			DirectHelper.SetTestBypassMontageActiveCheck(false); // Strictly NO bypass!

			// Step 1: Play instance 1 at 1.0f
			const float PlayResult1 = RealAnimInstance->Montage_Play(DirectPlayableMontage, 1.0f, EMontagePlayReturnType::MontageLength, 0.0f, false);
			TestTrue(TEXT("Helper Direct: Instance 1 started successfully"), PlayResult1 > 0.0f);
			const FAnimMontageInstance* Instance1 = RealAnimInstance->GetActiveInstanceForMontage(DirectPlayableMontage);
			if (TestNotNull(TEXT("Helper Direct: Instance 1 exists"), Instance1))
			{
				const int32 InstanceID1 = Instance1->GetInstanceID();

				// Finding 1 & 2: Positive control with valid real playing instance
				TestTrue(TEXT("Helper Direct (Positive Control): IsCurrentMontageInstance returns true for valid playing instance"),
					FAbilityMontageRateWindowLifecycle::IsCurrentMontageInstance(RealAnimInstance, DirectPlayableMontage, InstanceID1));

				// Negative controls against the positive setup:
				// If IsValid(AnimInstance) were omitted, an invalid pointer would be queried.
				UAnimInstance* GarbageAnim = NewObject<UAnimInstance>(Mesh);
				GarbageAnim->MarkAsGarbage();
				TestFalse(TEXT("Helper Direct (Negative Control): IsCurrentMontageInstance rejects garbage AnimInstance"),
					FAbilityMontageRateWindowLifecycle::IsCurrentMontageInstance(GarbageAnim, DirectPlayableMontage, InstanceID1));

				// If IsValid(Montage) were omitted, an invalid pointer would be queried.
				UAnimMontage* GarbageMont = NewObject<UAnimMontage>(GetTransientPackage());
				GarbageMont->MarkAsGarbage();
				TestFalse(TEXT("Helper Direct (Negative Control): IsCurrentMontageInstance rejects garbage Montage"),
					FAbilityMontageRateWindowLifecycle::IsCurrentMontageInstance(RealAnimInstance, GarbageMont, InstanceID1));

				// Negative control for BindAndCapture with garbage:
				FAbilityMontageRateWindowLifecycle GarbageBindHelper;
				GarbageBindHelper.BindAndCapture(DirectAbility, GarbageAnim, DirectPlayableMontage, TagRateWindowBegin, TagRateWindowEnd);
				TestFalse(TEXT("Helper Direct: BindAndCapture rejects garbage AnimInstance"), GarbageBindHelper.IsBound());
				GarbageBindHelper.BindAndCapture(DirectAbility, RealAnimInstance, GarbageMont, TagRateWindowBegin, TagRateWindowEnd);
				TestFalse(TEXT("Helper Direct: BindAndCapture rejects garbage Montage"), GarbageBindHelper.IsBound());

				// Step 2: Bind helper to Instance 1
				DirectHelper.BindAndCapture(DirectAbility, RealAnimInstance, DirectPlayableMontage, TagRateWindowBegin, TagRateWindowEnd);
				TestTrue(TEXT("Helper Direct: Helper is bound to Instance 1"), DirectHelper.IsBound());
				TestEqual(TEXT("Helper Direct: Bound ID matches Instance 1"), DirectHelper.GetBoundMontageInstanceID(), InstanceID1);
				TestEqual(TEXT("Helper Direct: Captured baseline is 1.0"), DirectHelper.GetBaselinePlayRate(), 1.0f);
				TestEqual(TEXT("Helper Direct: Initial window count is 0"), DirectHelper.GetActiveWindowCount(), 0);

				// Step 2b: Open Window A on Instance 1 BEFORE replay so window collection is non-empty!
				FAnimNotifyEvent* EventA = EnemyMontageRateWindowAutomation::FindRateWindowEvent(DirectPlayableMontage, 0);
				FAnimNotifyEvent* EventB = EnemyMontageRateWindowAutomation::FindRateWindowEvent(DirectPlayableMontage, 1);
				UAnimNotifyState_MontageRateWindow* RateNotifyA = EventA ? Cast<UAnimNotifyState_MontageRateWindow>(EventA->NotifyStateClass) : nullptr;
				UAnimNotifyState_MontageRateWindow* RateNotifyB = EventB ? Cast<UAnimNotifyState_MontageRateWindow>(EventB->NotifyStateClass) : nullptr;

				FGameplayEventData WindowAPayload;
				WindowAPayload.EventTag = TagRateWindowBegin;
				WindowAPayload.Instigator = Enemy;
				WindowAPayload.Target = Enemy;
				WindowAPayload.OptionalObject = DirectPlayableMontage;
				WindowAPayload.OptionalObject2 = RateNotifyA;
				WindowAPayload.EventMagnitude = 0.5f;

				DirectHelper.HandleBegin(WindowAPayload);
				TestEqual(TEXT("Helper Direct: Window A pushes active window count to 1"), DirectHelper.GetActiveWindowCount(), 1);
				TestEqual(TEXT("Helper Direct: Instance 1 play rate scaled to 0.5 by Window A"),
					RealAnimInstance->Montage_GetPlayRate(DirectPlayableMontage), 0.5f);

				// Step 3: Replay same Montage at rate 2.0f without stopping existing instances (bStopAllMontages = false)
				const float PlayResult2 = RealAnimInstance->Montage_Play(DirectPlayableMontage, 2.0f, EMontagePlayReturnType::MontageLength, 0.0f, false);
				TestTrue(TEXT("Helper Direct: Instance 2 started successfully"), PlayResult2 > 0.0f);
				const FAnimMontageInstance* Instance2 = RealAnimInstance->GetActiveInstanceForMontage(DirectPlayableMontage);
				if (TestNotNull(TEXT("Helper Direct: Instance 2 exists"), Instance2))
				{
					const int32 InstanceID2 = Instance2->GetInstanceID();
					TestNotEqual(TEXT("Helper Direct: New instance ID differs from old instance ID"), InstanceID2, InstanceID1);

					const FAnimMontageInstance* OldInstanceCheck = RealAnimInstance->GetMontageInstanceForID(InstanceID1);
					TestTrue(TEXT("Helper Direct: Old instance 1 still alive"), OldInstanceCheck != nullptr);
					TestFalse(TEXT("Helper Direct: Old instance 1 is not stopped"), OldInstanceCheck && OldInstanceCheck->IsStopped());
					TestEqual(TEXT("Helper Direct: Active montage play rate is 2.0"), RealAnimInstance->Montage_GetPlayRate(DirectPlayableMontage), 2.0f);
					TestEqual(TEXT("Helper Direct: Helper maintains Window A in active collection prior to test"), DirectHelper.GetActiveWindowCount(), 1);

					// Step 4: HandleBegin for old bound instance (Window B) directly on helper - must be rejected
					FGameplayEventData OldBeginPayload;
					OldBeginPayload.EventTag = TagRateWindowBegin;
					OldBeginPayload.Instigator = Enemy;
					OldBeginPayload.Target = Enemy;
					OldBeginPayload.OptionalObject = DirectPlayableMontage;
					OldBeginPayload.OptionalObject2 = RateNotifyB;
					OldBeginPayload.EventMagnitude = 0.2f;

					DirectHelper.HandleBegin(OldBeginPayload);
					TestEqual(TEXT("Helper Direct: Old Begin rejected - new instance play rate remains 2.0"), RealAnimInstance->Montage_GetPlayRate(DirectPlayableMontage), 2.0f);
					TestEqual(TEXT("Helper Direct: Old Begin rejected - active window count remains 1 (Window B not pushed)"), DirectHelper.GetActiveWindowCount(), 1);

					// Step 5: HandleEnd for old bound instance (Window A) directly on helper - must be rejected by instance authorization
					// (Even though Window A exists in collection, it must NOT pop or alter new instance rate)
					FGameplayEventData OldEndPayload;
					OldEndPayload.EventTag = TagRateWindowEnd;
					OldEndPayload.Instigator = Enemy;
					OldEndPayload.Target = Enemy;
					OldEndPayload.OptionalObject = DirectPlayableMontage;
					OldEndPayload.OptionalObject2 = RateNotifyA;

					DirectHelper.HandleEnd(OldEndPayload);
					TestEqual(TEXT("Helper Direct: Old End rejected - new instance play rate remains 2.0 (not overwritten)"), RealAnimInstance->Montage_GetPlayRate(DirectPlayableMontage), 2.0f);
					TestEqual(TEXT("Helper Direct: Old End rejected - active window count remains 1 (Window A not popped)"), DirectHelper.GetActiveWindowCount(), 1);

					// Step 6: RestoreAndClear on stale helper - must NOT overwrite new instance play rate with old baseline 1.0
					DirectHelper.RestoreAndClear();
					TestEqual(TEXT("Helper Direct: RestoreAndClear with stale ID does NOT overwrite new instance rate (remains 2.0)"),
						RealAnimInstance->Montage_GetPlayRate(DirectPlayableMontage), 2.0f);
					TestFalse(TEXT("Helper Direct: Helper is cleared (not bound)"), DirectHelper.IsBound());
					TestEqual(TEXT("Helper Direct: Helper bound ID reset to INDEX_NONE"), DirectHelper.GetBoundMontageInstanceID(), static_cast<int32>(INDEX_NONE));
					TestEqual(TEXT("Helper Direct: Helper active window count is 0"), DirectHelper.GetActiveWindowCount(), 0);

					// Step 7: Rebind helper to new instance 2 (current active instance)
					DirectHelper.BindAndCapture(DirectAbility, RealAnimInstance, DirectPlayableMontage, TagRateWindowBegin, TagRateWindowEnd);
					TestTrue(TEXT("Helper Direct: Rebind successfully bound to Instance 2"), DirectHelper.IsBound());
					TestEqual(TEXT("Helper Direct: Rebound ID matches Instance 2"), DirectHelper.GetBoundMontageInstanceID(), InstanceID2);
					TestEqual(TEXT("Helper Direct: Rebound captured new baseline rate 2.0"), DirectHelper.GetBaselinePlayRate(), 2.0f);

					// Step 8: Valid Begin on authorized Instance 2
					DirectHelper.HandleBegin(WindowAPayload);
					TestEqual(TEXT("Helper Direct: Valid Begin on authorized Instance 2 adds window (count 1)"), DirectHelper.GetActiveWindowCount(), 1);
					TestEqual(TEXT("Helper Direct: Play rate updated to Window A target rate (0.5) on authorized Instance 2"),
						RealAnimInstance->Montage_GetPlayRate(DirectPlayableMontage), 0.5f);

					// Step 9: RestoreAndClear on authorized helper restores new baseline 2.0
					DirectHelper.RestoreAndClear();
					TestEqual(TEXT("Helper Direct: RestoreAndClear restores baseline rate 2.0 on authorized Instance 2"), RealAnimInstance->Montage_GetPlayRate(DirectPlayableMontage), 2.0f);
					TestFalse(TEXT("Helper Direct: Helper is cleared after authorized restore"), DirectHelper.IsBound());
				}
			}

			RealAnimInstance->Montage_Stop(0.0f, DirectPlayableMontage);
			Mesh->AnimScriptInstance = MockAnimInstance;
		}
	}

	// =========================================================================
#endif
	// 3. Near-Combat Melee (UEnemyMeleeAbility) RateWindow Integration
	// =========================================================================
	{
		UEnemyMeleeAbility* MeleeAbility = NewObject<UEnemyMeleeAbility>(Enemy);
		MeleeAbility->SetTestActorInfo(FGameplayAbilitySpecHandle(), EnemyASC->AbilityActorInfo.Get());
		MeleeAbility->SetTestAbilityActive(true);
		MeleeAbility->SetTestBypassMontageActiveCheck(true);

		UAbilityTask_PlayActionMontage* WindowTask = UAbilityTask_PlayActionMontage::PlayActionMontage(MeleeAbility, NAME_None, ActiveMontage);
		if (!TestNotNull(TEXT("Standard window Task created"), WindowTask))
		{
			return false;
		}
		ON_SCOPE_EXIT { WindowTask->EndTask(); };
		WindowTask->TestBindRateWindowEvents(EnemyASC);
		// Former per-Ability tag getters map to real subscriptions on the standard Task.
		TestTrue(TEXT("Standard Task subscribes to RateWindow Begin tag"), EnemyASC->GenericGameplayEventCallbacks.FindChecked(TagRateWindowBegin).IsBoundToObject(WindowTask));
		TestTrue(TEXT("Standard Task subscribes to RateWindow End tag"), EnemyASC->GenericGameplayEventCallbacks.FindChecked(TagRateWindowEnd).IsBoundToObject(WindowTask));
		FAbilityMontageRateWindowLifecycle& Lifecycle = WindowTask->GetRateWindowLifecycle_Mutable();
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

		UAbilityTask_PlayActionMontage* WindowTask = UAbilityTask_PlayActionMontage::PlayActionMontage(BigHitAbility, NAME_None, ActiveMontage);
		if (!TestNotNull(TEXT("Standard window Task created"), WindowTask))
		{
			return false;
		}
		ON_SCOPE_EXIT { WindowTask->EndTask(); };
		WindowTask->TestBindRateWindowEvents(EnemyASC);
		// Former per-Ability tag getters map to real subscriptions on the standard Task.
		TestTrue(TEXT("Standard Task subscribes to RateWindow Begin tag"), EnemyASC->GenericGameplayEventCallbacks.FindChecked(TagRateWindowBegin).IsBoundToObject(WindowTask));
		TestTrue(TEXT("Standard Task subscribes to RateWindow End tag"), EnemyASC->GenericGameplayEventCallbacks.FindChecked(TagRateWindowEnd).IsBoundToObject(WindowTask));
		FAbilityMontageRateWindowLifecycle& Lifecycle = WindowTask->GetRateWindowLifecycle_Mutable();
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
	// 5. Small Hit Reaction task & instance ID isolation test
	// =========================================================================
	{
		UEnemySmallHitReactionAbility* SmallHitAbility = NewObject<UEnemySmallHitReactionAbility>(Enemy);
		SmallHitAbility->SetTestActorInfo(FGameplayAbilitySpecHandle(), EnemyASC->AbilityActorInfo.Get());
		SmallHitAbility->SetTestAbilityActive(true);
		SmallHitAbility->SetTestBoundAnimInstance(MockAnimInstance);
		SmallHitAbility->SetTestActiveMontage(ActiveMontage);

		// Task 1 simulates instance ID 1 activation
		UAbilityTask_PlayActionMontage* Task1 = UAbilityTask_PlayActionMontage::PlayActionMontage(
			SmallHitAbility, NAME_None, ActiveMontage);
		TestNotNull(TEXT("SmallHit: Task 1 created"), Task1);
		if (Task1)
		{
			Task1->SetTestTaskActive(true);
			Task1->SetTestBoundAnimInstance(MockAnimInstance);
			Task1->SetTestBoundMontageInstanceID(1);
			Task1->SetTestBypassMontageActiveCheck(true);
			Task1->GetRateWindowLifecycle_Mutable().SetTestActiveContext(SmallHitAbility, MockAnimInstance, ActiveMontage, TagRateWindowBegin, TagRateWindowEnd, 1.0f);
			Task1->GetRateWindowLifecycle_Mutable().SetTestBypassMontageActiveCheck(true);
			Task1->TestBindRateWindowEvents(EnemyASC);
			SmallHitAbility->SetTestMontageTask(Task1);

			FGameplayEventData ValidBeginA = FManagedMontageTestHelpers::MakeRateWindowEventData(
				TagRateWindowBegin, MockAnimInstance, 1, 0.4f, Enemy, ActiveMontage, NotifyA);

			// Mismatched instance ID (e.g. 99) is rejected
			FGameplayEventData MismatchedEvent = FManagedMontageTestHelpers::MakeRateWindowEventData(
				TagRateWindowBegin, MockAnimInstance, 99, 0.4f, Enemy, ActiveMontage, NotifyA);
			EnemyASC->HandleGameplayEvent(TagRateWindowBegin, &MismatchedEvent);
			TestEqual(TEXT("SmallHit: Mismatched instance ID drops event (depth 0)"), Task1->GetRateWindowLifecycle().GetActiveWindowCount(), 0);

			// Matched instance ID 1 applies event
			EnemyASC->HandleGameplayEvent(TagRateWindowBegin, &ValidBeginA);
			TestEqual(TEXT("SmallHit: Matched instance ID 1 applies event (depth 1)"), Task1->GetRateWindowLifecycle().GetActiveWindowCount(), 1);

			// Task 2 simulates retrigger with instance ID 2
			UAbilityTask_PlayActionMontage* Task2 = UAbilityTask_PlayActionMontage::PlayActionMontage(
				SmallHitAbility, NAME_None, ActiveMontage);
			TestNotNull(TEXT("SmallHit: Task 2 created"), Task2);
			if (Task2)
			{
				Task2->SetTestTaskActive(true);
				Task2->SetTestBoundAnimInstance(MockAnimInstance);
				Task2->SetTestBoundMontageInstanceID(2);
				Task2->SetTestBypassMontageActiveCheck(true);
				Task2->GetRateWindowLifecycle_Mutable().SetTestActiveContext(SmallHitAbility, MockAnimInstance, ActiveMontage, TagRateWindowBegin, TagRateWindowEnd, 1.0f);
				Task2->GetRateWindowLifecycle_Mutable().SetTestBypassMontageActiveCheck(true);
				Task2->TestBindRateWindowEvents(EnemyASC);
				SmallHitAbility->SetTestMontageTask(Task2);

				// Old event for instance 1 is rejected by Task 2
				EnemyASC->HandleGameplayEvent(TagRateWindowBegin, &ValidBeginA);
				TestEqual(TEXT("SmallHit: Old instance 1 event rejected by Task 2 (depth 0)"), Task2->GetRateWindowLifecycle().GetActiveWindowCount(), 0);

				// New event for instance 2 is applied
				FGameplayEventData ValidBeginB = FManagedMontageTestHelpers::MakeRateWindowEventData(
					TagRateWindowBegin, MockAnimInstance, 2, 0.2f, Enemy, ActiveMontage, NotifyB);
				EnemyASC->HandleGameplayEvent(TagRateWindowBegin, &ValidBeginB);
				TestEqual(TEXT("SmallHit: Active instance 2 event applied (depth 1)"), Task2->GetRateWindowLifecycle().GetActiveWindowCount(), 1);

				Task2->EndTask();
			}

			Task1->EndTask();
		}
	}

	// =========================================================================
	// 6. Launch Reaction (UEnemyLaunchReactionAbility) RateWindow Integration
	// =========================================================================
	{
		UEnemyLaunchReactionAbility* LaunchAbility = NewObject<UEnemyLaunchReactionAbility>(Enemy);
		LaunchAbility->SetTestActorInfo(FGameplayAbilitySpecHandle(), EnemyASC->AbilityActorInfo.Get());
		LaunchAbility->SetTestAbilityActive(true);
		LaunchAbility->SetTestBypassMontageActiveCheck(true);

		UAbilityTask_PlayActionMontage* WindowTask = UAbilityTask_PlayActionMontage::PlayActionMontage(LaunchAbility, NAME_None, ActiveMontage);
		if (!TestNotNull(TEXT("Standard window Task created"), WindowTask))
		{
			return false;
		}
		ON_SCOPE_EXIT { WindowTask->EndTask(); };
		WindowTask->TestBindRateWindowEvents(EnemyASC);
		// Former per-Ability tag getters map to real subscriptions on the standard Task.
		TestTrue(TEXT("Standard Task subscribes to RateWindow Begin tag"), EnemyASC->GenericGameplayEventCallbacks.FindChecked(TagRateWindowBegin).IsBoundToObject(WindowTask));
		TestTrue(TEXT("Standard Task subscribes to RateWindow End tag"), EnemyASC->GenericGameplayEventCallbacks.FindChecked(TagRateWindowEnd).IsBoundToObject(WindowTask));
		FAbilityMontageRateWindowLifecycle& Lifecycle = WindowTask->GetRateWindowLifecycle_Mutable();
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
		TestNull(TEXT("Victim: standard Task is null before recovery (former window Context)"), VictimAbility->GetTestVictimMontageTask());

		// 7.2 Stage 2: Non-lethal recovery entered
		VictimAbility->SetTestNonLethalRecovery(true, Player);
		TestTrue(TEXT("Victim: Non-lethal recovery active"), VictimAbility->IsTestNonLethalRecoveryActive());

		UAbilityTask_PlayActionMontage* WindowTask = UAbilityTask_PlayActionMontage::PlayActionMontage(VictimAbility, NAME_None, ActiveMontage);
		if (!TestNotNull(TEXT("Victim: standard window Task created"), WindowTask)) return false;
		ON_SCOPE_EXIT { WindowTask->EndTask(); };
		WindowTask->SetTaskOwnsMontageStop(false);
		WindowTask->TestBindRateWindowEvents(EnemyASC);
		// Former per-Ability configured tags are now actual standard Task subscriptions.
		TestTrue(TEXT("Victim: standard Begin subscription"), EnemyASC->GenericGameplayEventCallbacks.FindChecked(TagRateWindowBegin).IsBoundToObject(WindowTask));
		TestTrue(TEXT("Victim: standard End subscription"), EnemyASC->GenericGameplayEventCallbacks.FindChecked(TagRateWindowEnd).IsBoundToObject(WindowTask));
		FAbilityMontageRateWindowLifecycle& Lifecycle = WindowTask->GetRateWindowLifecycle_Mutable();
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
		auto GetLifecycle = [&]() -> const FAbilityMontageRateWindowLifecycle&
		{
			return Ability->GetTestRateWindowLifecycle();
		};
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
			TestFalse(TEXT("Runtime End: lifecycle unbound before next activation"), GetLifecycle().IsBound());
			TestEqual(TEXT("Runtime End: no active windows"), GetLifecycle().GetActiveWindowCount(), 0);
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
				|| !TestNotNull(TEXT("Runtime: live montage task exists"), Ability->GetTestMontageTask()))
			{
				return false;
			}
			TestTrue(TEXT("Runtime: montage task is active"), Ability->GetTestMontageTask()->IsActive());
			TestTrue(TEXT("Runtime: selected directional montage matches"), Ability->GetTestActiveMontage() == ExpectedMontage);
			TestTrue(TEXT("Runtime: montage is really active"), Anim->Montage_IsActive(ExpectedMontage));
			TestFalse(TEXT("Runtime: montage checks are not bypassed"), Ability->GetTestBypassMontageActiveCheck());
			TestTrue(TEXT("Runtime: activation-owned tag present"), RuntimeASC->HasMatchingGameplayTag(SmallStateTag));
			TestTrue(TEXT("Runtime: helper bound by activation"), GetLifecycle().IsBound());
			TestEqual(TEXT("Runtime: activation starts with no windows"), GetLifecycle().GetActiveWindowCount(), 0);
			TestEqual(TEXT("Runtime: activation captures a fresh baseline"), GetLifecycle().GetBaselinePlayRate(), 1.0f);
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
			FAnimNotifyEventReference EventRef(NotifyEvent, Montage);
			const int32 CurrentInstID = Ability->GetTestActiveMontageInstanceID();
			if (CurrentInstID != INDEX_NONE)
			{
				EventRef.AddContextData<UE::Anim::FAnimNotifyMontageInstanceContext>(CurrentInstID);
			}
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
			TestEqual(Step + TEXT(": active windows"), GetLifecycle().GetActiveWindowCount(), ExpectedCount);
			TestEqual(Step + TEXT(": actual montage play rate"), Anim->Montage_GetPlayRate(Montage), ExpectedRate);
		};

		AddInfo(FString::Printf(TEXT("Runtime activation: initial front montage %s"), *FrontMontage->GetName()));
		if (!TriggerHit(RuntimeEnemy->GetActorForwardVector()) || !CheckActiveMontage(FrontMontage))
		{
			return false;
		}
		ExpectedRestoredRate = 1.0f;
		SendNotify(FrontMontage, 0, true, 1, 0.5f);
		SendNotify(FrontMontage, 1, true, 2, 0.2f);
		SendNotify(FrontMontage, 0, false, 1, 0.2f); // B Begin before A End must keep B.
		SendNotify(FrontMontage, 1, false, 0, 1.0f);
		SendNotify(FrontMontage, 0, true, 1, 0.5f);
		SendNotify(FrontMontage, 1, true, 2, 0.2f);
		SendNotify(FrontMontage, 1, false, 1, 0.5f); // True nesting restores the surviving A.
		SendNotify(FrontMontage, 0, false, 0, 1.0f);
		SendNotify(FrontMontage, 0, true, 1, 0.5f); // Retrigger while a window is open.

		for (int32 RetriggerIndex = 0; RetriggerIndex < 2; ++RetriggerIndex)
		{
			EndingInstanceID = Ability->GetTestActiveMontageInstanceID();
			TStrongObjectPtr<UAbilityTask_PlayActionMontage> OldTask(Ability->GetTestMontageTask());
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
			TestTrue(TEXT("Runtime retrigger: old montage task finished"), OldTask->IsTerminated());
			ExpectedRestoredRate = 1.0f;
			SendNotify(NextMontage, 1, true, 1, 0.2f);

			// Stale event carrying old EndingInstanceID must be rejected
			FGameplayEventData LateEnd = FManagedMontageTestHelpers::MakeRateWindowEventData(
				TagRateWindowEnd, Mesh->GetAnimInstance(), EndingInstanceID, 0.2f);
			RuntimeASC->HandleGameplayEvent(TagRateWindowEnd, &LateEnd);
			TestEqual(TEXT("Runtime retrigger: stale event cannot remove current window"), GetLifecycle().GetActiveWindowCount(), 1);
			TestEqual(TEXT("Runtime retrigger: stale event cannot change current rate"), Anim->Montage_GetPlayRate(NextMontage), 0.2f);
		}

		EndingInstanceID = Ability->GetTestActiveMontageInstanceID();
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

		// Direct EndTask uses OnDestroy(false), but must still restore and stop its playback.
		if (!TriggerHit(RuntimeEnemy->GetActorForwardVector()) || !CheckActiveMontage(FrontMontage))
		{
			return false;
		}
		EndingInstanceID = Ability->GetTestActiveMontageInstanceID();
		bExpectRestoreBeforeStop = true;
		SendNotify(FrontMontage, 0, true, 1, 0.5f);
		TStrongObjectPtr<UAbilityTask_PlayActionMontage> ExplicitlyEndedTask(Ability->GetTestMontageTask());
		ExplicitlyEndedTask->EndTask();
		TestTrue(TEXT("Runtime EndTask: task finished"), ExplicitlyEndedTask->IsFinished());
		TestFalse(TEXT("Runtime EndTask: lifecycle unbound"), ExplicitlyEndedTask->GetRateWindowLifecycle().IsBound());
		TestFalse(TEXT("Runtime EndTask: montage inactive"), Anim->Montage_IsActive(FrontMontage));
		const FAnimMontageInstance* ExplicitlyEndedInstance = Anim->GetMontageInstanceForID(EndingInstanceID);
		if (TestNotNull(TEXT("Runtime EndTask: original instance remains inspectable"), ExplicitlyEndedInstance))
		{
			TestEqual(TEXT("Runtime EndTask: original rate restored"), ExplicitlyEndedInstance->GetPlayRate(), 1.0f);
			TestTrue(TEXT("Runtime EndTask: original instance stopped"), ExplicitlyEndedInstance->IsStopped());
		}
		// EndTask releases playback resources; the Ability still owns its gameplay exit.
		TestTrue(TEXT("Runtime EndTask: ability remains active until explicitly ended"), Ability->IsActive());
		RuntimeASC->CancelAbilityHandle(Handle);
		if (TestTrue(TEXT("Runtime EndTask: replacement playback starts"), Anim->Montage_Play(FrontMontage, 1.5f) > 0.0f))
		{
			ExplicitlyEndedTask->EndTask();
			TestTrue(TEXT("Runtime EndTask: repeated cleanup leaves replacement active"), Anim->Montage_IsActive(FrontMontage));
			TestEqual(TEXT("Runtime EndTask: repeated cleanup leaves replacement rate unchanged"), Anim->Montage_GetPlayRate(FrontMontage), 1.5f);
		}
	}

	// =========================================================================
	// 10. Real Enemy Melee same-asset replacement negative proof (H6-F01 reproduction)
	// =========================================================================
	{
		enum class EMeleeNegativeStep : uint8
		{
			OldBegin,
			OldEnd,
			OldClear
		};

		auto RunMeleeReplacementNegativeProof = [&](EMeleeNegativeStep StepToTest, const TCHAR* StepName, float EnemyYOffset) -> bool
		{
			// 1. Create a playable rate montage without Root Motion.
			UAnimMontage* PlayableMontage = EnemyMontageRateWindowAutomation::CreatePlayableRateMontage(*this, World);
			if (!TestNotNull(FString::Printf(TEXT("H6-F01 (%s): playable montage created"), StepName), PlayableMontage))
			{
				return false;
			}
			PlayableMontage->bEnableRootMotionTranslation = false;
			PlayableMontage->bEnableRootMotionRotation = false;

			// 2. Author AttackProfile and AttackSet.
			UEnemyAttackProfile* AttackProfile = NewObject<UEnemyAttackProfile>(World);
			AttackProfile->SetTestMontage(PlayableMontage);
			AttackProfile->SetTestDamageEffectClass(UTestProjectileDamageGE::StaticClass());
			AttackProfile->SetTestAttackRange(200.0f);
			AttackProfile->SetTestCooldown(0.0f);
			AttackProfile->SetTestGuardStaminaDamage(10.0f);

			UEnemyAttackSet* AttackSet = NewObject<UEnemyAttackSet>(World);
			AttackSet->SetTestEngagementRange(250.0f);
			AttackSet->AddTestEntry(AttackProfile, 1.0f);

			FString SetReason;
			if (!TestTrue(FString::Printf(TEXT("H6-F01 (%s): attack set is valid"), StepName), AttackSet->IsAttackSetValid(SetReason)))
			{
				return false;
			}

			// 3. Author AIProfile.
			UEnemyAIProfile* AIProfile = NewObject<UEnemyAIProfile>(World);
			AIProfile->SetTestPreferredCombatDistance(180.0f);
			AIProfile->SetTestLateralRepositionDistance(100.0f);
			AIProfile->SetTestRepositionAcceptanceRadius(50.0f);
			AIProfile->SetTestRepositionRetryDelay(1.0f);
			AIProfile->SetTestLeashRadius(1000.0f);
			AIProfile->SetTestApproachTimeout(5.0f);

			FString ProfileReason;
			if (!TestTrue(FString::Printf(TEXT("H6-F01 (%s): AI profile is valid"), StepName), AIProfile->IsValidAIProfile(ProfileReason)))
			{
				return false;
			}

			// 4. Spawn Passive Enemy with pre-BeginPlay setup so OnPossess caches valid AttackSet & AIProfile.
			const FVector EnemyLocation(500.0f, EnemyYOffset, 0.0f);
			AEnemyCharacter* MeleeEnemy = FCombatAutomationFixture::SpawnPassiveEnemy(
				World,
				FTransform(FRotator::ZeroRotator, EnemyLocation),
				[&](AEnemyCharacter& SpawnedEnemy)
				{
					SpawnedEnemy.SetTestAttackSet(AttackSet);
					SpawnedEnemy.SetTestAIProfile(AIProfile);
				});

			if (!TestNotNull(FString::Printf(TEXT("H6-F01 (%s): enemy spawned"), StepName), MeleeEnemy))
			{
				return false;
			}

			USkeletalMeshComponent* Mesh = MeleeEnemy->GetMesh();
			UAbilitySystemComponent* EnemyASC = MeleeEnemy->GetAbilitySystemComponent();
			if (!TestNotNull(FString::Printf(TEXT("H6-F01 (%s): mesh exists"), StepName), Mesh)
				|| !TestNotNull(FString::Printf(TEXT("H6-F01 (%s): ASC exists"), StepName), EnemyASC))
			{
				return false;
			}

			// 5. Install Montage-only AnimInstance and refresh ActorInfo.
			Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered;
			UAnimInstance* Anim = NewObject<UAnimInstance>(Mesh);
			Anim->InitializeMontageOnly();
			Anim->CurrentSkeleton = PlayableMontage->GetSkeleton();
			Mesh->AnimScriptInstance = Anim;
			EnemyASC->RefreshAbilityActorInfo();

			if (!TestTrue(FString::Printf(TEXT("H6-F01 (%s): ASC resolves montage AnimInstance"), StepName),
				EnemyASC->AbilityActorInfo.IsValid() && EnemyASC->AbilityActorInfo->GetAnimInstance() == Anim))
			{
				return false;
			}

			// 6. Verify Controller state and set Combat Target.
			AEnemyAIController* AIController = World->SpawnActor<AEnemyAIController>(AEnemyAIController::StaticClass());
			if (!TestNotNull(FString::Printf(TEXT("H6-F01 (%s): AIController exists"), StepName), AIController))
			{
				return false;
			}
			AIController->Possess(MeleeEnemy);

			TestTrue(FString::Printf(TEXT("H6-F01 (%s): controller has valid AttackSet"), StepName), AIController->HasValidAttackSet());
			TestTrue(FString::Printf(TEXT("H6-F01 (%s): controller has valid AIProfile"), StepName), AIController->HasValidAIProfile());

			// Position Player in front of Enemy (distance 100cm <= AttackRange 200cm <= MeleeRange 250cm).
			Player->SetActorLocation(EnemyLocation + FVector(100.0f, 0.0f, 0.0f));
			Player->SetActorRotation(FRotator::ZeroRotator);
			AIController->SetTestTargetForAutomation(Player);

			TestTrue(FString::Printf(TEXT("H6-F01 (%s): combat target is valid"), StepName), AIController->HasValidCombatTarget());
			TestTrue(FString::Printf(TEXT("H6-F01 (%s): target in melee range"), StepName), AIController->IsCombatTargetInMeleeRange());

			// 7. Grant EnemyMeleeAbility and prepare pending attack profile.
			const FGameplayAbilitySpecHandle MeleeHandle = EnemyASC->GiveAbility(
				FGameplayAbilitySpec(UEnemyMeleeAbility::StaticClass(), 1, INDEX_NONE, MeleeEnemy));

			if (!TestTrue(FString::Printf(TEXT("H6-F01 (%s): PreparePendingAttackProfile succeeds"), StepName),
				AIController->PreparePendingAttackProfile()))
			{
				EnemyASC->ClearAbility(MeleeHandle);
				return false;
			}

			TestTrue(FString::Printf(TEXT("H6-F01 (%s): pending attack in range"), StepName), AIController->IsPendingAttackInRange());

			// 8. Real ASC route activation.
			const bool bRequestSucceeded = AIController->TryRequestMeleeAttack();
			if (!TestTrue(FString::Printf(TEXT("H6-F01 (%s): TryRequestMeleeAttack succeeds"), StepName), bRequestSucceeded))
			{
				EnemyASC->ClearAbility(MeleeHandle);
				return false;
			}

			// 9. Inspect active Ability and Task.
			FGameplayAbilitySpec* Spec = EnemyASC->FindAbilitySpecFromHandle(MeleeHandle);
			UEnemyMeleeAbility* Ability = Spec ? Cast<UEnemyMeleeAbility>(Spec->GetPrimaryInstance()) : nullptr;
			if (!TestNotNull(FString::Printf(TEXT("H6-F01 (%s): live ability instance exists"), StepName), Ability))
			{
				EnemyASC->ClearAbility(MeleeHandle);
				return false;
			}

			TestTrue(FString::Printf(TEXT("H6-F01 (%s): ability is active"), StepName), Ability->IsActive());
			TestFalse(FString::Printf(TEXT("H6-F01 (%s): ability bypass is false"), StepName), Ability->GetTestBypassMontageActiveCheck());

			UAbilityTask_PlayActionMontage* OldTask = Ability->GetTestMontageTask();
			if (!TestNotNull(FString::Printf(TEXT("H6-F01 (%s): live rate Task exists"), StepName), OldTask))
			{
				EnemyASC->CancelAbilityHandle(MeleeHandle);
				EnemyASC->ClearAbility(MeleeHandle);
				return false;
			}

			TestFalse(FString::Printf(TEXT("H6-F01 (%s): helper bypass is false"), StepName), OldTask->GetRateWindowLifecycle().GetTestBypassMontageActiveCheck());

			const int32 OldInstanceID = Ability->GetTestActiveMontageInstanceID();
			TestNotEqual(FString::Printf(TEXT("H6-F01 (%s): captured instance ID is valid"), StepName), OldInstanceID, static_cast<int32>(INDEX_NONE));
			TestTrue(FString::Printf(TEXT("H6-F01 (%s): helper is bound"), StepName), OldTask->GetRateWindowLifecycle().IsBound());

			// 10. Enter Window A on old instance.
			FAnimNotifyEvent* EventA = EnemyMontageRateWindowAutomation::FindRateWindowEvent(PlayableMontage, 0);
			FAnimNotifyEvent* EventB = EnemyMontageRateWindowAutomation::FindRateWindowEvent(PlayableMontage, 1);
			if (!TestNotNull(FString::Printf(TEXT("H6-F01 (%s): RateWindowA exists"), StepName), EventA)
				|| !TestNotNull(FString::Printf(TEXT("H6-F01 (%s): RateWindowB exists"), StepName), EventB))
			{
				EnemyASC->CancelAbilityHandle(MeleeHandle);
				EnemyASC->ClearAbility(MeleeHandle);
				return false;
			}

			FAnimNotifyEventReference EventRef(EventA, PlayableMontage);
			EventRef.AddContextData<UE::Anim::FAnimNotifyMontageInstanceContext>(OldInstanceID);
			CastChecked<UAnimNotifyState_MontageRateWindow>(EventA->NotifyStateClass)->NotifyBegin(
				Mesh, PlayableMontage, 1.0f, EventRef);
			TestEqual(FString::Printf(TEXT("H6-F01 (%s): Window A is accepted on old instance"), StepName),
				OldTask->GetRateWindowLifecycle().GetActiveWindowCount(), 1);
			TestEqual(FString::Printf(TEXT("H6-F01 (%s): Window A sets rate to 0.5"), StepName), Anim->Montage_GetPlayRate(PlayableMontage), 0.5f);

			// 11. Same-asset replay with rate 2.0 and bStopAllMontages = false.
			const float ReplayLength = Anim->Montage_Play(PlayableMontage, 2.0f, EMontagePlayReturnType::MontageLength, 0.0f, false);
			if (!TestTrue(FString::Printf(TEXT("H6-F01 (%s): replay without stopping old instance succeeds"), StepName), ReplayLength > 0.0f))
			{
				EnemyASC->CancelAbilityHandle(MeleeHandle);
				EnemyASC->ClearAbility(MeleeHandle);
				return false;
			}

			// 12. Assert dual-instance precondition strictly.
			const FAnimMontageInstance* OldLiveInstance = Anim->GetMontageInstanceForID(OldInstanceID);
			const FAnimMontageInstance* NewLiveInstance = Anim->GetActiveInstanceForMontage(PlayableMontage);

			if (!TestNotNull(FString::Printf(TEXT("H6-F01 (%s): old instance still exists"), StepName), OldLiveInstance)
				|| !TestNotNull(FString::Printf(TEXT("H6-F01 (%s): new active instance exists"), StepName), NewLiveInstance))
			{
				EnemyASC->CancelAbilityHandle(MeleeHandle);
				EnemyASC->ClearAbility(MeleeHandle);
				return false;
			}

			TestFalse(FString::Printf(TEXT("H6-F01 (%s): old instance is not stopped"), StepName), OldLiveInstance->IsStopped());
			TestNotEqual(FString::Printf(TEXT("H6-F01 (%s): new instance ID differs from old ID"), StepName),
				NewLiveInstance->GetInstanceID(), OldInstanceID);
			TestEqual(FString::Printf(TEXT("H6-F01 (%s): new instance play rate is 2.0"), StepName),
				Anim->Montage_GetPlayRate(PlayableMontage), 2.0f);
			TestTrue(FString::Printf(TEXT("H6-F01 (%s): ability remains active"), StepName), Ability->IsActive());
			TestTrue(FString::Printf(TEXT("H6-F01 (%s): old Task remains bound"), StepName),
				Ability->GetTestMontageTask() == OldTask);
			TestFalse(FString::Printf(TEXT("H6-F01 (%s): ability bypass remains false"), StepName),
				Ability->GetTestBypassMontageActiveCheck());
			TestFalse(FString::Printf(TEXT("H6-F01 (%s): helper bypass remains false"), StepName),
				OldTask->GetRateWindowLifecycle().GetTestBypassMontageActiveCheck());

			// 13. Execute the specific negative step under test.
			if (StepToTest == EMeleeNegativeStep::OldBegin)
			{
				FGameplayEventData Payload;
				Payload.EventTag = TagRateWindowBegin;
				Payload.Instigator = MeleeEnemy;
				Payload.Target = MeleeEnemy;
				Payload.OptionalObject = PlayableMontage;
				Payload.OptionalObject2 = EventB->NotifyStateClass;
				Payload.EventMagnitude = 0.2f;

				Payload.TargetData = FManagedMontageTestHelpers::MakeRateWindowEventData(
					TagRateWindowBegin, Anim, OldInstanceID, Payload.EventMagnitude, MeleeEnemy, PlayableMontage, Payload.OptionalObject2.Get()).TargetData;
				EnemyASC->HandleGameplayEvent(TagRateWindowBegin, &Payload);
				// Unfixed production code will overwrite new instance rate to 0.2f (EXPECTED FAILURE BEFORE FIX).
				TestEqual(FString::Printf(TEXT("H6-F01 Reproduction (%s): old Begin cannot alter new rate"), StepName),
					Anim->Montage_GetPlayRate(PlayableMontage), 2.0f);
			}
			else if (StepToTest == EMeleeNegativeStep::OldEnd)
			{
				FGameplayEventData Payload;
				Payload.EventTag = TagRateWindowEnd;
				Payload.Instigator = MeleeEnemy;
				Payload.Target = MeleeEnemy;
				Payload.OptionalObject = PlayableMontage;
				Payload.OptionalObject2 = EventA->NotifyStateClass;

				Payload.TargetData = FManagedMontageTestHelpers::MakeRateWindowEventData(
					TagRateWindowEnd, Anim, OldInstanceID, Payload.EventMagnitude, MeleeEnemy, PlayableMontage, Payload.OptionalObject2.Get()).TargetData;
				EnemyASC->HandleGameplayEvent(TagRateWindowEnd, &Payload);
				// Unfixed production code will restore new instance to old baseline 1.0f (EXPECTED FAILURE BEFORE FIX).
				TestEqual(FString::Printf(TEXT("H6-F01 Reproduction (%s): old End cannot alter new rate"), StepName),
					Anim->Montage_GetPlayRate(PlayableMontage), 2.0f);
			}
			else if (StepToTest == EMeleeNegativeStep::OldClear)
			{
				OldTask->TestCleanupTask(false);
				// Unfixed production code will restore new instance to old baseline 1.0f (EXPECTED FAILURE BEFORE FIX).
				TestEqual(FString::Printf(TEXT("H6-F01 Reproduction (%s): old Clear cannot alter new rate"), StepName),
					Anim->Montage_GetPlayRate(PlayableMontage), 2.0f);
			}

			// 14. Cleanup.
			EnemyASC->CancelAbilityHandle(MeleeHandle);
			EnemyASC->ClearAbility(MeleeHandle);
			if (Anim->Montage_IsActive(PlayableMontage))
			{
				Anim->Montage_Stop(0.0f, PlayableMontage);
			}
			if (AIController)
			{
				AIController->UnPossess();
				AIController->Destroy();
			}
			MeleeEnemy->Destroy();

			return true;
		};

		// Run all three negative proof steps on independent preconditions.
		RunMeleeReplacementNegativeProof(EMeleeNegativeStep::OldBegin, TEXT("OldBegin"), 1000.0f);
		RunMeleeReplacementNegativeProof(EMeleeNegativeStep::OldEnd, TEXT("OldEnd"), 2000.0f);
		RunMeleeReplacementNegativeProof(EMeleeNegativeStep::OldClear, TEXT("OldClear"), 3000.0f);
	}
#endif

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
