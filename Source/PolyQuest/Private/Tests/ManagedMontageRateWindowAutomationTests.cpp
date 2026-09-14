// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#if WITH_EDITOR
#include "Animation/AnimData/IAnimationDataController.h"
#endif

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/EnemyHitReactionAbility.h"
#include "AbilitySystem/Abilities/EnemyLaunchReactionAbility.h"
#include "AbilitySystem/Abilities/EnemyMeleeAbility.h"
#include "AbilitySystem/Abilities/EnemyStanceBreakAbility.h"
#include "AbilitySystem/Abilities/EnemyVictimExecutionAbility.h"
#include "AbilitySystem/Abilities/PlayerFrontExecutionAbility.h"
#include "AbilitySystem/Abilities/PlayerBackstabExecutionAbility.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "AI/EnemyAIController.h"
#include "AI/EnemyAIProfile.h"
#include "Combat/Enemy/EnemyAttackProfile.h"
#include "Combat/Enemy/EnemyAttackSet.h"
#include "Tests/TestProjectileDamageGE.h"
#include "AbilitySystem/Abilities/PlayerGuardAbility.h"
#include "AbilitySystem/Abilities/PlayerGuardBreakAbility.h"
#include "AbilitySystem/Abilities/PlayerParryAbility.h"
#include "AbilitySystem/Abilities/PlayerSmallHitReactionAbility.h"
#include "AbilitySystem/CharacterAttributeSet.h"
#include "AbilitySystem/Tasks/AbilityTask_PlayActionMontage.h"
#include "Animation/Combat/AnimNotifyState_ActionWindows.h"
#include "Animation/ActiveMontageInstanceScope.h"
#include "Animation/AnimComposite.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Character/Player/PlayerCharacter.h"
#include "Combat/CombatImpactEffectContext.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Framework/PolyQuestPlayerController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayTagContainer.h"
#include "Misc/ScopeExit.h"
#include <limits>
#include "Tests/CombatAutomationFixture.h"
#include "Tests/TestManagedMontageAbility.h"
#include "Tests/TestMobileBowMoveSpeedGE.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FManagedMontageRateWindowAutomationTest,
	"PolyQuest.Combat.ManagedMontageRateWindow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace ManagedMontageAutomation
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
#endif

	struct FTestWorldScope
	{
		UWorld* World = nullptr;
		~FTestWorldScope()
		{
			if (World && GEngine)
			{
				World->DestroyWorld(false);
				GEngine->DestroyWorldContext(World);
			}
		}
	};

#if WITH_EDITOR
	struct FTestMontageSetup
	{
		UAnimMontage* Montage = nullptr;
		UAnimNotifyState_MontageRateWindow* NotifyA = nullptr;
		UAnimNotifyState_MontageRateWindow* NotifyB = nullptr;
	};

	FTestMontageSetup CreatePlayableTestMontage(FAutomationTestBase& Test, UObject* Outer)
	{
		USkeleton* Skeleton = NewObject<USkeleton>(Outer);
		const FName RootBoneName(TEXT("root"));
		{
			FReferenceSkeletonModifier Modifier(Skeleton);
			Modifier.Add(FMeshBoneInfo(RootBoneName, TEXT("root"), INDEX_NONE), FTransform::Identity);
		}

		UAnimSequence* Sequence = NewObject<UAnimSequence>(Outer, TEXT("Seq_ManagedTest"));
		Sequence->SetSkeleton(Skeleton);
		IAnimationDataController& Controller = Sequence->GetController();
		Controller.InitializeModel();
		bool bPopulated = false;
		{
			IAnimationDataController::FScopedBracket Populate(Controller,
				FText::FromString(TEXT("Populate ManagedMontage fixture")), false);
			Controller.SetFrameRate(FFrameRate(30, 1), false);
			Controller.SetNumberOfFrames(FFrameNumber(30), false); // 30 frames = 1.00s duration
			const bool bTrackAdded = Controller.AddBoneCurve(RootBoneName, false);
			TArray<FVector3f> Positions;
			TArray<FQuat4f> Rotations;
			TArray<FVector3f> Scales;
			Positions.Init(FVector3f::ZeroVector, 31);
			Rotations.Init(FQuat4f::Identity, 31);
			Scales.Init(FVector3f::OneVector, 31);
			bPopulated = bTrackAdded && Controller.SetBoneTrackKeys(RootBoneName, Positions, Rotations, Scales, false);
			Controller.NotifyPopulated();
		}
		Sequence->WaitOnExistingCompression();
		if (!Test.TestTrue(TEXT("Runtime: synthetic animation has a populated root track"), bPopulated))
		{
			return {};
		}

		UAnimMontage* Montage = NewObject<UAnimMontage>(Outer, TEXT("Montage_ManagedTest"));
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

#if WITH_EDITORONLY_DATA
		Montage->AnimNotifyTracks.Add(FAnimNotifyTrack(FName(TEXT("1")), FLinearColor::White));
#endif

		UAnimNotifyState_MontageRateWindow* NotifyA = NewObject<UAnimNotifyState_MontageRateWindow>(Montage, TEXT("RateWindowA"));
		NotifyA->RateMultiplier = 0.4f;
		{
			FAnimNotifyEvent& EventA = Montage->Notifies.AddDefaulted_GetRef();
			EventA.NotifyName = FName(TEXT("RateWindowA"));
			EventA.NotifyStateClass = NotifyA;
			EventA.TrackIndex = 0;
			EventA.MontageTickType = EMontageNotifyTickType::Queued;
			EventA.Link(Montage, 0.10f);
			EventA.SetTime(0.10f);
			EventA.SetDuration(0.10f);
			EventA.EndLink.Link(Montage, 0.20f);
			EventA.EndLink.SetTime(0.20f);
		}

		UAnimNotifyState_MontageRateWindow* NotifyB = NewObject<UAnimNotifyState_MontageRateWindow>(Montage, TEXT("RateWindowB"));
		NotifyB->RateMultiplier = 0.2f;
		{
			FAnimNotifyEvent& EventB = Montage->Notifies.AddDefaulted_GetRef();
			EventB.NotifyName = FName(TEXT("RateWindowB"));
			EventB.NotifyStateClass = NotifyB;
			EventB.TrackIndex = 0;
			EventB.MontageTickType = EMontageNotifyTickType::BranchingPoint;
			EventB.Link(Montage, 0.40f);
			EventB.SetTime(0.40f);
			EventB.SetDuration(0.10f);
			EventB.EndLink.Link(Montage, 0.50f);
			EventB.EndLink.SetTime(0.50f);
		}

		Montage->RefreshCacheData();

		return { Montage, NotifyA, NotifyB };
	}
#endif
}

bool FManagedMontageRateWindowAutomationTest::RunTest(const FString& Parameters)
{
	const FGameplayTag TagRateWindowBegin = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.RateWindow.Begin")), false);
	const FGameplayTag TagRateWindowEnd = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.RateWindow.End")), false);

	TestTrue(TEXT("Tag Event.Action.RateWindow.Begin is valid"), TagRateWindowBegin.IsValid());
	TestTrue(TEXT("Tag Event.Action.RateWindow.End is valid"), TagRateWindowEnd.IsValid());

#if !WITH_EDITOR
	// Non-Editor Development build: test pure C++ structs, validation contracts, and static guarantees
	// without invoking Editor-only animation sequence / controller construction.
	FGameplayAbilityTargetData_MontageRateWindowSource TestTargetData;
	TestTargetData.MontageInstanceID = 123;
	TestTrue(TEXT("TargetData GetScriptStruct matches StaticStruct"),
		TestTargetData.GetScriptStruct() == FGameplayAbilityTargetData_MontageRateWindowSource::StaticStruct());

	const FGameplayEventData EventData = FManagedMontageTestHelpers::MakeRateWindowEventData(
		TagRateWindowBegin, nullptr, 123, 0.5f);
	TestTrue(TEXT("Helper constructs valid TargetData"), EventData.TargetData.IsValid(0));

	return true;
#else
	// WITH_EDITOR: Full runtime integration tests with synthetic animation and real time advance

	if (!TestNotNull(TEXT("GEngine is valid"), GEngine))
	{
		return false;
	}

	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("ManagedMontageTestWorld"));
	WorldContext.SetCurrentWorld(World);
	ManagedMontageAutomation::FTestWorldScope ScopeCleanup{ World };

	if (!TestNotNull(TEXT("Test World created"), World))
	{
		return false;
	}

	FURL WorldURL;
	World->InitializeActorsForPlay(WorldURL);
	World->BeginPlay();

	APlayerCharacter* Player = FCombatAutomationFixture::SpawnPlayer(World, FTransform::Identity);
	if (!TestNotNull(TEXT("Player spawned"), Player))
	{
		return false;
	}

	UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
	if (!TestNotNull(TEXT("Player ASC valid"), ASC))
	{
		return false;
	}

	ManagedMontageAutomation::FTestMontageSetup Setup = ManagedMontageAutomation::CreatePlayableTestMontage(*this, World);
	UAnimMontage* TestMontage = Setup.Montage;
	UAnimNotifyState_MontageRateWindow* NotifyA = Setup.NotifyA;
	UAnimNotifyState_MontageRateWindow* NotifyB = Setup.NotifyB;
	if (!TestNotNull(TEXT("TestMontage created"), TestMontage) || !TestNotNull(TEXT("NotifyA created"), NotifyA))
	{
		return false;
	}

	UAnimInstance* MockAnimInstance = NewObject<UAnimInstance>(Player->GetMesh());
	MockAnimInstance->InitializeMontageOnly();
	MockAnimInstance->CurrentSkeleton = TestMontage->GetSkeleton();
	Player->GetMesh()->AnimScriptInstance = MockAnimInstance;
	ASC->RefreshAbilityActorInfo();

	// =========================================================================
	// 1. 默认支持（Default Support）与 速率语义（Rate Semantics）
	// =========================================================================
	{
		const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(
			FGameplayAbilitySpec(UTestManagedMontageAbility::StaticClass(), 1, INDEX_NONE, Player));
		ON_SCOPE_EXIT
		{
			if (IsValid(ASC))
			{
				ASC->CancelAbilityHandle(Handle);
				ASC->ClearAbility(Handle);
			}
		};

		FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle);
		UTestManagedMontageAbility* Ability = Spec ? Cast<UTestManagedMontageAbility>(Spec->GetPrimaryInstance()) : nullptr;
		TestNotNull(TEXT("Ability instanced"), Ability);

		if (Ability)
		{
			Ability->TestMontage = TestMontage;
			const bool bActivated = ASC->TryActivateAbility(Handle);
			TestTrue(TEXT("1. Ability activated"), bActivated);

			UAbilityTask_PlayActionMontage* Task = Ability->GetMontageTask();
			TestNotNull(TEXT("1. Task created"), Task);
			if (Task)
			{
				Task->SetTestBypassMontageActiveCheck(true);

				// 1.1 默认支持：无需配置即具备速率窗口监听，Lifecycle 已绑定
				const FAbilityMontageRateWindowLifecycle& Lifecycle = Task->GetRateWindowLifecycle();
				TestTrue(TEXT("1.1 Lifecycle bound by default"), Lifecycle.IsBound());
				TestEqual(TEXT("1.1 Initial active window count is 0"), Lifecycle.GetActiveWindowCount(), 0);

				const int32 BoundInstanceID = Task->GetBoundMontageInstanceID();

				// 2.1 速率语义：发送合法 Begin 事件
				const FGameplayEventData ValidBeginEvent = FManagedMontageTestHelpers::MakeRateWindowEventData(
					TagRateWindowBegin, MockAnimInstance, BoundInstanceID, 0.4f, Player, TestMontage, NotifyA);
				ASC->HandleGameplayEvent(TagRateWindowBegin, &ValidBeginEvent);

				TestEqual(TEXT("2.1 Valid Begin applied, count is 1"), Lifecycle.GetActiveWindowCount(), 1);
				TestEqual(TEXT("2.1 Active rate multiplier is 0.4"), Lifecycle.GetCurrentTargetRate(), 0.4f);

				// 2.2 速率语义：发送合法 End 事件
				const FGameplayEventData ValidEndEvent = FManagedMontageTestHelpers::MakeRateWindowEventData(
					TagRateWindowEnd, MockAnimInstance, BoundInstanceID, 0.4f, Player, TestMontage, NotifyA);
				ASC->HandleGameplayEvent(TagRateWindowEnd, &ValidEndEvent);

				TestEqual(TEXT("2.2 Valid End restored, count is 0"), Lifecycle.GetActiveWindowCount(), 0);
				TestEqual(TEXT("2.2 Active rate multiplier restored to baseline"), Lifecycle.GetCurrentTargetRate(), 1.0f);

				// =========================================================================
				// 2.3 身份防御（Identity Defense）：拒收无身份、非法类型、不匹配ID与缺失ID
				// =========================================================================
				// 4.1 拒收无 TargetData 事件
				FGameplayEventData NoTargetDataEvent;
				NoTargetDataEvent.EventTag = TagRateWindowBegin;
				NoTargetDataEvent.EventMagnitude = 0.5f;
				ASC->HandleGameplayEvent(TagRateWindowBegin, &NoTargetDataEvent);
				TestEqual(TEXT("4.1 Missing TargetData rejected"), Lifecycle.GetActiveWindowCount(), 0);

				// 4.2 拒收携带基类 TargetData（非派生类型）的事件
				FGameplayEventData BaseTargetDataEvent;
				BaseTargetDataEvent.EventTag = TagRateWindowBegin;
				BaseTargetDataEvent.EventMagnitude = 0.5f;
				BaseTargetDataEvent.TargetData = FGameplayAbilityTargetDataHandle(new FGameplayAbilityTargetData());
				ASC->HandleGameplayEvent(TagRateWindowBegin, &BaseTargetDataEvent);
				TestEqual(TEXT("4.2 Base FGameplayAbilityTargetData rejected"), Lifecycle.GetActiveWindowCount(), 0);

				// 4.3 拒收携带空 AnimInstance 的事件
				const FGameplayEventData NullAnimEvent = FManagedMontageTestHelpers::MakeRateWindowEventData(
					TagRateWindowBegin, nullptr, BoundInstanceID, 0.5f, Player, TestMontage, NotifyA);
				ASC->HandleGameplayEvent(TagRateWindowBegin, &NullAnimEvent);
				TestEqual(TEXT("4.3 Null AnimInstance rejected"), Lifecycle.GetActiveWindowCount(), 0);

				// 4.4 拒收不匹配的 MontageInstanceID
				const FGameplayEventData WrongIDEvent = FManagedMontageTestHelpers::MakeRateWindowEventData(
					TagRateWindowBegin, MockAnimInstance, BoundInstanceID + 9999, 0.5f, Player, TestMontage, NotifyA);
				ASC->HandleGameplayEvent(TagRateWindowBegin, &WrongIDEvent);
				TestEqual(TEXT("4.4 Mismatched MontageInstanceID rejected"), Lifecycle.GetActiveWindowCount(), 0);

				// 4.5 修复 1 门禁：拒收携带 INDEX_NONE 来源 ID 的事件（缺失身份绝不放行）
				const FGameplayEventData IndexNoneEvent = FManagedMontageTestHelpers::MakeRateWindowEventData(
					TagRateWindowBegin, MockAnimInstance, INDEX_NONE, 0.5f, Player, TestMontage, NotifyA);
				ASC->HandleGameplayEvent(TagRateWindowBegin, &IndexNoneEvent);
				TestEqual(TEXT("4.5 INDEX_NONE MontageInstanceID rejected"), Lifecycle.GetActiveWindowCount(), 0);

				// =========================================================================
				// 2.4 重入与退出清理（Reentrancy & Exit Cleanup）
				// =========================================================================
				// 开启未闭合的窗口
				ASC->HandleGameplayEvent(TagRateWindowBegin, &ValidBeginEvent);
				TestEqual(TEXT("6.1 Window opened before EndAbility"), Lifecycle.GetActiveWindowCount(), 1);

				// 结束 Ability
				Ability->EndTestAbility();

				TestFalse(TEXT("6.2 Ability ended"), Ability->IsActive());
				TestFalse(TEXT("6.3 Lifecycle unbound upon cleanup"), Lifecycle.IsBound());
				TestEqual(TEXT("6.4 Active window count cleared"), Lifecycle.GetActiveWindowCount(), 0);
			}
		}
	}

	// =========================================================================
	// 3. 真实时间推进互斥单发与速率验证（Queued vs Branching Point via AdvanceWorld）
	// =========================================================================
	{
		const FGameplayAbilitySpecHandle Handle3 = ASC->GiveAbility(
			FGameplayAbilitySpec(UTestManagedMontageAbility::StaticClass(), 1, INDEX_NONE, Player));
		ON_SCOPE_EXIT
		{
			if (IsValid(ASC))
			{
				ASC->CancelAbilityHandle(Handle3);
				ASC->ClearAbility(Handle3);
			}
		};

		FGameplayAbilitySpec* Spec3 = ASC->FindAbilitySpecFromHandle(Handle3);
		UTestManagedMontageAbility* Ability3 = Spec3 ? Cast<UTestManagedMontageAbility>(Spec3->GetPrimaryInstance()) : nullptr;
		TestNotNull(TEXT("3. Ability3 instanced"), Ability3);

		if (Ability3)
		{
			Ability3->TestMontage = TestMontage;

			int32 BeginEventCount = 0;
			int32 ReceivedBeginInstanceID = INDEX_NONE;
			UAnimInstance* ReceivedBeginAnimInstance = nullptr;
			FDelegateHandle BeginHandle = ASC->GenericGameplayEventCallbacks.FindOrAdd(TagRateWindowBegin).AddLambda(
				[&BeginEventCount, &ReceivedBeginInstanceID, &ReceivedBeginAnimInstance](const FGameplayEventData* Payload)
				{
					++BeginEventCount;
					if (Payload && Payload->TargetData.IsValid(0))
					{
						if (const FGameplayAbilityTargetData_MontageRateWindowSource* Source =
							static_cast<const FGameplayAbilityTargetData_MontageRateWindowSource*>(Payload->TargetData.Get(0)))
						{
							ReceivedBeginInstanceID = Source->MontageInstanceID;
							ReceivedBeginAnimInstance = Source->AnimInstance.Get();
						}
					}
				});

			int32 EndEventCount = 0;
			int32 ReceivedEndInstanceID = INDEX_NONE;
			UAnimInstance* ReceivedEndAnimInstance = nullptr;
			FDelegateHandle EndHandle = ASC->GenericGameplayEventCallbacks.FindOrAdd(TagRateWindowEnd).AddLambda(
				[&EndEventCount, &ReceivedEndInstanceID, &ReceivedEndAnimInstance](const FGameplayEventData* Payload)
				{
					++EndEventCount;
					if (Payload && Payload->TargetData.IsValid(0))
					{
						if (const FGameplayAbilityTargetData_MontageRateWindowSource* Source =
							static_cast<const FGameplayAbilityTargetData_MontageRateWindowSource*>(Payload->TargetData.Get(0)))
						{
							ReceivedEndInstanceID = Source->MontageInstanceID;
							ReceivedEndAnimInstance = Source->AnimInstance.Get();
						}
					}
				});

			ON_SCOPE_EXIT
			{
				if (IsValid(ASC))
				{
					ASC->GenericGameplayEventCallbacks.FindOrAdd(TagRateWindowBegin).Remove(BeginHandle);
					ASC->GenericGameplayEventCallbacks.FindOrAdd(TagRateWindowEnd).Remove(EndHandle);
				}
			};

			const bool bActivated = ASC->TryActivateAbility(Handle3);
			TestTrue(TEXT("3. Ability3 activated"), bActivated);

			UAbilityTask_PlayActionMontage* Task3 = Ability3->GetMontageTask();
			TestNotNull(TEXT("3. Task3 created"), Task3);
			if (Task3)
			{
				Task3->SetTestBypassMontageActiveCheck(false);
				const int32 ExpectedInstanceID = Task3->GetBoundMontageInstanceID();
				TestNotEqual(TEXT("3. ExpectedInstanceID is valid"), ExpectedInstanceID, (int32)INDEX_NONE);

				// 3.1 推进 0.12s，到达 Queued 窗口（0.10s~0.20s），断言 Begin 触发、ID 准确、改速 0.4f
				FCombatAutomationFixture::AdvanceWorld(World, 0.12f);
				TestEqual(TEXT("3.1 Queued Begin fires exactly once"), BeginEventCount, 1);
				TestEqual(TEXT("3.1 Queued Begin AnimInstance matches"), ReceivedBeginAnimInstance, MockAnimInstance);
				TestEqual(TEXT("3.1 Queued Begin carries accurate InstanceID"), ReceivedBeginInstanceID, ExpectedInstanceID);
				TestEqual(TEXT("3.1 Queued active window count is 1"), Task3->GetRateWindowLifecycle().GetActiveWindowCount(), 1);
				TestEqual(TEXT("3.1 Queued rate changed to 0.4f"), Task3->GetRateWindowLifecycle().GetCurrentTargetRate(), 0.4f);
				TestTrue(TEXT("3.1 Actual montage play rate is 0.4f"),
					FMath::IsNearlyEqual(MockAnimInstance->Montage_GetPlayRate(TestMontage), 0.4f, 0.001f));

				// 3.2 推进 0.35s，穿过 Queued 结束点（0.20s），断言 End 触发、速率恢复 1.0f
				FCombatAutomationFixture::AdvanceWorld(World, 0.35f);
				TestEqual(TEXT("3.2 Queued End fires exactly once"), EndEventCount, 1);
				TestEqual(TEXT("3.2 Queued End carries accurate InstanceID"), ReceivedEndInstanceID, ExpectedInstanceID);
				TestEqual(TEXT("3.2 Queued active window count is 0"), Task3->GetRateWindowLifecycle().GetActiveWindowCount(), 0);
				TestEqual(TEXT("3.2 Queued rate restored to 1.0f"), Task3->GetRateWindowLifecycle().GetCurrentTargetRate(), 1.0f);
				TestTrue(TEXT("3.2 Actual montage play rate restored to 1.0f"),
					FMath::IsNearlyEqual(MockAnimInstance->Montage_GetPlayRate(TestMontage), 1.0f, 0.001f));

				// 3.3 推进 0.16s，到达 Branching Point 窗口（0.40s~0.50s），断言 Begin 触发、改速 0.2f
				FCombatAutomationFixture::AdvanceWorld(World, 0.16f);
				TestEqual(TEXT("3.3 BranchingPoint Begin fires exactly once"), BeginEventCount, 2);
				TestEqual(TEXT("3.3 BranchingPoint Begin AnimInstance matches"), ReceivedBeginAnimInstance, MockAnimInstance);
				TestEqual(TEXT("3.3 BranchingPoint Begin carries accurate InstanceID"), ReceivedBeginInstanceID, ExpectedInstanceID);
				TestEqual(TEXT("3.3 BranchingPoint active window count is 1"), Task3->GetRateWindowLifecycle().GetActiveWindowCount(), 1);
				TestEqual(TEXT("3.3 BranchingPoint rate changed to 0.2f"), Task3->GetRateWindowLifecycle().GetCurrentTargetRate(), 0.2f);
				TestTrue(TEXT("3.3 Actual montage play rate is 0.2f"),
					FMath::IsNearlyEqual(MockAnimInstance->Montage_GetPlayRate(TestMontage), 0.2f, 0.001f));

				// 3.4 推进 0.55s，穿过 Branching Point 结束点（0.50s），断言 End 触发、速率恢复 1.0f
				FCombatAutomationFixture::AdvanceWorld(World, 0.55f);
				TestEqual(TEXT("3.4 BranchingPoint End fires exactly once"), EndEventCount, 2);
				TestEqual(TEXT("3.4 BranchingPoint End carries accurate InstanceID"), ReceivedEndInstanceID, ExpectedInstanceID);
				TestEqual(TEXT("3.4 BranchingPoint active window count is 0"), Task3->GetRateWindowLifecycle().GetActiveWindowCount(), 0);
				TestEqual(TEXT("3.4 BranchingPoint rate restored to 1.0f"), Task3->GetRateWindowLifecycle().GetCurrentTargetRate(), 1.0f);
				TestTrue(TEXT("3.4 Actual montage play rate restored to 1.0f"),
					FMath::IsNearlyEqual(MockAnimInstance->Montage_GetPlayRate(TestMontage), 1.0f, 0.001f));

				Ability3->EndTestAbility();
			}
		}
	}

	// =========================================================================
	// 4. ASC 取消通知与 Root Motion Scale 恢复（Cancellation & Root Motion Scale Recovery）
	// =========================================================================
	{
		// 4.1 默认配置（bAllowInterruptAfterBlendOut = false）下普通播放期间 ASC 取消恰好通知一次 OnInterrupted，并恢复 Root Motion Scale
		const FGameplayAbilitySpecHandle CancelHandle = ASC->GiveAbility(
			FGameplayAbilitySpec(UTestManagedMontageAbility::StaticClass(), 1, INDEX_NONE, Player));
		ON_SCOPE_EXIT
		{
			if (IsValid(ASC))
			{
				ASC->CancelAbilityHandle(CancelHandle);
				ASC->ClearAbility(CancelHandle);
			}
		};

		FGameplayAbilitySpec* CancelSpec = ASC->FindAbilitySpecFromHandle(CancelHandle);
		UTestManagedMontageAbility* CancelAbility = CancelSpec ? Cast<UTestManagedMontageAbility>(CancelSpec->GetPrimaryInstance()) : nullptr;
		TestNotNull(TEXT("4.1 CancelAbility instanced"), CancelAbility);

		if (CancelAbility)
		{
			CancelAbility->TestMontage = TestMontage;
			CancelAbility->InitialRootMotionScale = 2.0f;
			CancelAbility->bTestAllowInterruptAfterBlendOut = false;

			const bool bActivated = ASC->TryActivateAbility(CancelHandle);
			TestTrue(TEXT("4.1 CancelAbility activated"), bActivated);

			UAbilityTask_PlayActionMontage* TaskCancel = CancelAbility->GetMontageTask();
			TestNotNull(TEXT("4.1 TaskCancel created"), TaskCancel);
			if (TaskCancel)
			{
				TaskCancel->SetTestBypassMontageActiveCheck(false);

				TestEqual(TEXT("4.1 AnimRootMotionTranslationScale set to 2.0 upon play"),
					Player->GetAnimRootMotionTranslationScale(), 2.0f);

				// ASC 取消 Ability
				ASC->CancelAbilityHandle(CancelHandle);

				// 断言 P2-1：普通播放期间 ASC 取消恰好通知一次 OnInterrupted
				TestEqual(TEXT("4.1 InterruptedCallCount is exactly 1"), CancelAbility->InterruptedCallCount, 1);
				TestEqual(TEXT("4.1 CompletedCallCount is 0"), CancelAbility->CompletedCallCount, 0);
				TestFalse(TEXT("4.1 Ability ended cleanly"), CancelAbility->IsActive());

				// 断言 P2-2：Root Motion Scale 成功恢复为 1.0f
				TestEqual(TEXT("4.1 AnimRootMotionTranslationScale restored to 1.0"),
					Player->GetAnimRootMotionTranslationScale(), 1.0f);
			}
		}

		// 4.2 ExternalCancel 显式调用时恢复 Root Motion Scale 并通知 OnCancelled
		const FGameplayAbilitySpecHandle ExtCancelHandle = ASC->GiveAbility(
			FGameplayAbilitySpec(UTestManagedMontageAbility::StaticClass(), 1, INDEX_NONE, Player));
		ON_SCOPE_EXIT
		{
			if (IsValid(ASC))
			{
				ASC->CancelAbilityHandle(ExtCancelHandle);
				ASC->ClearAbility(ExtCancelHandle);
			}
		};

		FGameplayAbilitySpec* ExtCancelSpec = ASC->FindAbilitySpecFromHandle(ExtCancelHandle);
		UTestManagedMontageAbility* ExtCancelAbility = ExtCancelSpec ? Cast<UTestManagedMontageAbility>(ExtCancelSpec->GetPrimaryInstance()) : nullptr;
		TestNotNull(TEXT("4.2 ExtCancelAbility instanced"), ExtCancelAbility);

		if (ExtCancelAbility)
		{
			ExtCancelAbility->TestMontage = TestMontage;
			ExtCancelAbility->InitialRootMotionScale = 3.0f;

			const bool bActivated = ASC->TryActivateAbility(ExtCancelHandle);
			TestTrue(TEXT("4.2 ExtCancelAbility activated"), bActivated);

			UAbilityTask_PlayActionMontage* ExtTask = ExtCancelAbility->GetMontageTask();
			TestNotNull(TEXT("4.2 ExtTask created"), ExtTask);
			if (ExtTask)
			{
				ExtTask->SetTestBypassMontageActiveCheck(false);
				TestEqual(TEXT("4.2 AnimRootMotionTranslationScale set to 3.0"),
					Player->GetAnimRootMotionTranslationScale(), 3.0f);

				ExtTask->ExternalCancel();

				TestEqual(TEXT("4.2 CancelledCallCount is 1"), ExtCancelAbility->CancelledCallCount, 1);
				TestEqual(TEXT("4.2 AnimRootMotionTranslationScale restored to 1.0 after ExternalCancel"),
					Player->GetAnimRootMotionTranslationScale(), 1.0f);
			}
		}

		// 4.3 修复门禁（反例 1）：Task 启动失败时 CleanupTask(false) 绝不重置其他正常播放的 Root Motion Scale
		const FGameplayAbilitySpecHandle ActiveHandle = ASC->GiveAbility(
			FGameplayAbilitySpec(UTestManagedMontageAbility::StaticClass(), 1, INDEX_NONE, Player));
		ON_SCOPE_EXIT
		{
			if (IsValid(ASC))
			{
				ASC->CancelAbilityHandle(ActiveHandle);
				ASC->ClearAbility(ActiveHandle);
			}
		};

		FGameplayAbilitySpec* ActiveSpec = ASC->FindAbilitySpecFromHandle(ActiveHandle);
		UTestManagedMontageAbility* ActiveAbility = ActiveSpec ? Cast<UTestManagedMontageAbility>(ActiveSpec->GetPrimaryInstance()) : nullptr;
		TestNotNull(TEXT("4.3 ActiveAbility instanced"), ActiveAbility);

		if (ActiveAbility)
		{
			ActiveAbility->TestMontage = TestMontage;
			ActiveAbility->InitialRootMotionScale = 2.5f;

			const bool bActivatedA = ASC->TryActivateAbility(ActiveHandle);
			TestTrue(TEXT("4.3 ActiveAbility activated"), bActivatedA);

			UAbilityTask_PlayActionMontage* TaskActive = ActiveAbility->GetMontageTask();
			TestNotNull(TEXT("4.3 TaskActive created"), TaskActive);
			if (TaskActive)
			{
				TaskActive->SetTestBypassMontageActiveCheck(false);
				TestEqual(TEXT("4.3 Active Task sets AnimRootMotionTranslationScale to 2.5"),
					Player->GetAnimRootMotionTranslationScale(), 2.5f);

				// 尝试启动一个因非法参数（Rate = -1.0f）导致失败的 TaskFailed
				const FGameplayAbilitySpecHandle FailedHandle = ASC->GiveAbility(
					FGameplayAbilitySpec(UTestManagedMontageAbility::StaticClass(), 1, INDEX_NONE, Player));
				ON_SCOPE_EXIT
				{
					if (IsValid(ASC))
					{
						ASC->CancelAbilityHandle(FailedHandle);
						ASC->ClearAbility(FailedHandle);
					}
				};

				FGameplayAbilitySpec* FailedSpec = ASC->FindAbilitySpecFromHandle(FailedHandle);
				UTestManagedMontageAbility* FailedAbility = FailedSpec ? Cast<UTestManagedMontageAbility>(FailedSpec->GetPrimaryInstance()) : nullptr;
				TestNotNull(TEXT("4.3 FailedAbility instanced"), FailedAbility);
				if (FailedAbility)
				{
					FailedAbility->TestMontage = TestMontage;
					FailedAbility->TestRate = -1.0f; // 非法 Rate，导致 Activate() 立即失败触发 CleanupTask(false)

					const bool bActivatedF = ASC->TryActivateAbility(FailedHandle);
					TestTrue(TEXT("4.3 FailedAbility activation attempted"), bActivatedF);
					TestEqual(TEXT("4.3 FailedAbility OnMontageFailed called"), FailedAbility->FailedCallCount, 1);

					// 核心反例 1 断言：启动失败的 Task 绝对不得将活跃 Task 的 2.5 scale 重置为 1.0
					TestEqual(TEXT("4.3 Failed Task cleanup does not overwrite active scale of 2.5"),
						Player->GetAnimRootMotionTranslationScale(), 2.5f);
					TestTrue(TEXT("4.3 Active Task montage remains playing"),
						MockAnimInstance->Montage_IsActive(TestMontage));
				}

				// 取消活跃 Task，断言 Scale 正常恢复为 1.0
				ASC->CancelAbilityHandle(ActiveHandle);
				TestEqual(TEXT("4.3 AnimRootMotionTranslationScale restored to 1.0 after active cancel"),
					Player->GetAnimRootMotionTranslationScale(), 1.0f);
			}
		}
	}

	// =========================================================================
	// 5. 同资产重播误杀防御与旧身份迟到事件隔离（Same-Asset Replay Defense）
	// =========================================================================
	{
		// 5.1 基础校验：StopPlayingMontage 在实例 ID 不匹配时拒绝停止
		UTestManagedMontageAbility* MockAbility = NewObject<UTestManagedMontageAbility>(World);
		UAbilityTask_PlayActionMontage* TaskA = UAbilityTask_PlayActionMontage::PlayActionMontage(
			MockAbility, NAME_None, TestMontage);
		TestNotNull(TEXT("5.1 Task A created"), TaskA);

		if (TaskA)
		{
			TaskA->SetTestBoundMontageInstanceID(101);
			TaskA->SetTestBypassMontageActiveCheck(false);

			const bool bStopped = TaskA->TestStopPlayingMontage();
			TestFalse(TEXT("5.1 TestStopPlayingMontage denies stopping when not owning current instance"), bStopped);
			TaskA->EndTask();
		}

		// 5.2 真实同资产重播产生两个不同 InstanceID，旧身份迟到事件与旧 Task 清理均不干扰新实例
		const FGameplayAbilitySpecHandle ReplayHandle = ASC->GiveAbility(
			FGameplayAbilitySpec(UTestManagedMontageAbility::StaticClass(), 1, INDEX_NONE, Player));
		ON_SCOPE_EXIT
		{
			if (IsValid(ASC))
			{
				ASC->CancelAbilityHandle(ReplayHandle);
				ASC->ClearAbility(ReplayHandle);
			}
		};

		FGameplayAbilitySpec* ReplaySpec = ASC->FindAbilitySpecFromHandle(ReplayHandle);
		UTestManagedMontageAbility* ReplayAbility = ReplaySpec ? Cast<UTestManagedMontageAbility>(ReplaySpec->GetPrimaryInstance()) : nullptr;
		TestNotNull(TEXT("5.2 ReplayAbility instanced"), ReplayAbility);

		if (ReplayAbility)
		{
			ReplayAbility->TestMontage = TestMontage;
			ReplayAbility->InitialRootMotionScale = 1.0f;

			const bool bActivated1 = ASC->TryActivateAbility(ReplayHandle);
			TestTrue(TEXT("5.2 First playback activated"), bActivated1);

			UAbilityTask_PlayActionMontage* Task1 = ReplayAbility->GetMontageTask();
			TestNotNull(TEXT("5.2 Task 1 created"), Task1);
			if (Task1)
			{
				Task1->SetTestBypassMontageActiveCheck(false);
				const int32 ID1 = Task1->GetBoundMontageInstanceID();
				TestNotEqual(TEXT("5.2 Instance 1 has valid ID"), ID1, (int32)INDEX_NONE);

				// 解除 Task 1 对 ReplayAbility 的回调绑定，防止重播中断导致 Ability 提前结束
				Task1->OnCompleted.Clear();
				Task1->OnInterrupted.Clear();
				Task1->OnCancelled.Clear();

				// 重播同一个 Montage，传入新 Scale (2.5f) 产生实例 2
				UAbilityTask_PlayActionMontage* Task2 = ReplayAbility->PlayActionMontage(
					TestMontage, 1.0f, NAME_None, 2.5f);
				TestNotNull(TEXT("5.2 Task 2 created"), Task2);
				if (Task2)
				{
					Task2->SetTestBypassMontageActiveCheck(false);
					const int32 ID2 = Task2->GetBoundMontageInstanceID();
					TestNotEqual(TEXT("5.2 Instance 2 has valid ID"), ID2, (int32)INDEX_NONE);
					TestTrue(TEXT("5.2 Replay generates different instance ID"), ID1 != ID2);

					// 断言新播放的 Root Motion Scale 与 ASC 播放归属
					TestEqual(TEXT("5.2 Task 2 sets AnimRootMotionTranslationScale to 2.5"),
						Player->GetAnimRootMotionTranslationScale(), 2.5f);
					TestEqual(TEXT("5.2 ASC animating ability is ReplayAbility"),
						ASC->GetAnimatingAbility(), (UGameplayAbility*)ReplayAbility);
					TestEqual(TEXT("5.2 ASC current montage is TestMontage"),
						ASC->GetCurrentMontage(), TestMontage);

					// Task 2 接受合法 Begin 事件（携带 ID2，倍率 0.4f）
					const FGameplayEventData ValidBegin2 = FManagedMontageTestHelpers::MakeRateWindowEventData(
						TagRateWindowBegin, MockAnimInstance, ID2, 0.4f, Player, TestMontage, NotifyA);
					ASC->HandleGameplayEvent(TagRateWindowBegin, &ValidBegin2);
					TestEqual(TEXT("5.2 Task 2 has 1 active rate window"), Task2->GetRateWindowLifecycle().GetActiveWindowCount(), 1);
					TestEqual(TEXT("5.2 Target rate for instance 2 is 0.4f"), Task2->GetRateWindowLifecycle().GetCurrentTargetRate(), 0.4f);
					TestTrue(TEXT("5.2 Actual montage play rate is 0.4f"),
						FMath::IsNearlyEqual(MockAnimInstance->Montage_GetPlayRate(TestMontage), 0.4f, 0.001f));

					// 向 ASC 注入携带旧实例 ID1 的迟到 End 事件
					const FGameplayEventData StaleEnd1 = FManagedMontageTestHelpers::MakeRateWindowEventData(
						TagRateWindowEnd, MockAnimInstance, ID1, 0.4f, Player, TestMontage, NotifyA);
					ASC->HandleGameplayEvent(TagRateWindowEnd, &StaleEnd1);

					// 断言：Task 2 坚决拒绝旧 ID 事件，窗口数不减，速率不被篡改
					TestEqual(TEXT("5.2 Stale event with old ID1 cannot close instance 2 window"),
						Task2->GetRateWindowLifecycle().GetActiveWindowCount(), 1);
					TestEqual(TEXT("5.2 Instance 2 target rate remains 0.4f"),
						Task2->GetRateWindowLifecycle().GetCurrentTargetRate(), 0.4f);
					TestTrue(TEXT("5.2 Actual montage play rate remains 0.4f"),
						FMath::IsNearlyEqual(MockAnimInstance->Montage_GetPlayRate(TestMontage), 0.4f, 0.001f));

					// 清理旧 Task 1
					Task1->EndTask();

					// 断言：旧 Task 1 清理绝不停止新实例 2，不篡改速率，不重置 Root Motion Scale，不清除 ASC 所有权
					TestTrue(TEXT("5.2 Instance 2 remains actively playing after Task 1 cleanup"),
						MockAnimInstance->Montage_IsActive(TestMontage));
					TestTrue(TEXT("5.2 Instance 2 rate preserved after Task 1 cleanup"),
						FMath::IsNearlyEqual(MockAnimInstance->Montage_GetPlayRate(TestMontage), 0.4f, 0.001f));
					TestEqual(TEXT("5.2 AnimRootMotionTranslationScale remains 2.5 after Task 1 cleanup"),
						Player->GetAnimRootMotionTranslationScale(), 2.5f);
					TestEqual(TEXT("5.2 ASC animating ability remains ReplayAbility"),
						ASC->GetAnimatingAbility(), (UGameplayAbility*)ReplayAbility);
					TestEqual(TEXT("5.2 ASC current montage remains TestMontage"),
						ASC->GetCurrentMontage(), TestMontage);

					// 真实推进世界时间，验证 Task 2 自然完成及结果委托有效（慢速窗口下完整播放耗时约 2.75s，推进 3.5s）
					FCombatAutomationFixture::AdvanceWorld(World, 3.5f);
					TestEqual(TEXT("5.2 ReplayAbility CompletedCallCount is exactly 1"),
						ReplayAbility->CompletedCallCount, 1);
					TestFalse(TEXT("5.2 ReplayAbility ended cleanly upon natural completion"),
						ReplayAbility->IsActive());
					TestTrue(TEXT("5.2 Task 2 terminated cleanly"), Task2->IsTerminated());
					TestEqual(TEXT("5.2 AnimRootMotionTranslationScale restored to 1.0 upon natural completion"),
						Player->GetAnimRootMotionTranslationScale(), 1.0f);
				}
			}
		}

		// 5.3 修复门禁（反例 2）：不同 Montage B 接管并设置自己的 scale，旧 Montage A 清理绝不得重置 B 的 scale
		UAnimMontage* TestMontageB = NewObject<UAnimMontage>(World, TEXT("Montage_DifferentAssetTest"));
		TestMontageB->SetSkeleton(TestMontage->GetSkeleton());
		FSlotAnimationTrack TrackB;
		TrackB.SlotName = FName(TEXT("SecondarySlot"));
		if (TestMontage->SlotAnimTracks.Num() > 0 && TestMontage->SlotAnimTracks[0].AnimTrack.AnimSegments.Num() > 0)
		{
			FAnimSegment SegmentB;
			SegmentB.SetAnimReference(TestMontage->SlotAnimTracks[0].AnimTrack.AnimSegments[0].GetAnimReference());
			SegmentB.AnimEndTime = TestMontage->GetPlayLength();
			TrackB.AnimTrack.AnimSegments.Add(SegmentB);
		}
		TestMontageB->SlotAnimTracks.Add(TrackB);
		TestMontageB->CompositeSections = TestMontage->CompositeSections;
		ManagedMontageAutomation::UTestMontageLengthAccess::SetLength(TestMontageB, TestMontage->GetPlayLength());
		TestMontageB->BlendIn.SetBlendTime(0.0f);
		TestMontageB->BlendOut.SetBlendTime(0.0f);

		const FGameplayAbilitySpecHandle SpecHandleA = ASC->GiveAbility(
			FGameplayAbilitySpec(UTestManagedMontageAbility::StaticClass(), 1, INDEX_NONE, Player));
		const FGameplayAbilitySpecHandle SpecHandleB = ASC->GiveAbility(
			FGameplayAbilitySpec(UTestManagedMontageAbility::StaticClass(), 1, INDEX_NONE, Player));
		ON_SCOPE_EXIT
		{
			if (IsValid(ASC))
			{
				ASC->CancelAbilityHandle(SpecHandleA);
				ASC->ClearAbility(SpecHandleA);
				ASC->CancelAbilityHandle(SpecHandleB);
				ASC->ClearAbility(SpecHandleB);
			}
		};

		UTestManagedMontageAbility* AbilityA = Cast<UTestManagedMontageAbility>(ASC->FindAbilitySpecFromHandle(SpecHandleA)->GetPrimaryInstance());
		UTestManagedMontageAbility* AbilityB = Cast<UTestManagedMontageAbility>(ASC->FindAbilitySpecFromHandle(SpecHandleB)->GetPrimaryInstance());
		TestNotNull(TEXT("5.3 AbilityA instanced"), AbilityA);
		TestNotNull(TEXT("5.3 AbilityB instanced"), AbilityB);

		if (AbilityA && AbilityB)
		{
			// 1. AbilityA 播放 TestMontage，设置 scale = 2.5
			AbilityA->TestMontage = TestMontage;
			AbilityA->InitialRootMotionScale = 2.5f;
			const bool bActivatedA = ASC->TryActivateAbility(SpecHandleA);
			TestTrue(TEXT("5.3 AbilityA activated"), bActivatedA);

			UAbilityTask_PlayActionMontage* TaskAssetA = AbilityA->GetMontageTask();
			TestNotNull(TEXT("5.3 TaskAssetA created"), TaskAssetA);
			if (TaskAssetA)
			{
				TaskAssetA->SetTestBypassMontageActiveCheck(false);
				TestEqual(TEXT("5.3 TaskAssetA sets scale to 2.5"), Player->GetAnimRootMotionTranslationScale(), 2.5f);

				// 清空 TaskAssetA 的回调，并解绑 MockAnimInstance 蒙太奇委托，防止在 Montage_Play 时同步打断提前终结 TaskAssetA
				TaskAssetA->OnCompleted.Clear();
				TaskAssetA->OnInterrupted.Clear();
				TaskAssetA->OnCancelled.Clear();
				FOnMontageBlendingOutStarted DummyBlendingOutDelegate;
				FOnMontageEnded DummyEndDelegate;
				MockAnimInstance->Montage_SetBlendingOutDelegate(DummyBlendingOutDelegate, TestMontage);
				MockAnimInstance->Montage_SetEndDelegate(DummyEndDelegate, TestMontage);

				// 2. AbilityB 接管播放不同资产 TestMontageB，设置 scale = 3.5
				AbilityB->TestMontage = TestMontageB;
				AbilityB->InitialRootMotionScale = 3.5f;
				const bool bActivatedB = ASC->TryActivateAbility(SpecHandleB);
				TestTrue(TEXT("5.3 AbilityB activated with different Montage"), bActivatedB);

				UAbilityTask_PlayActionMontage* TaskAssetB = AbilityB->GetMontageTask();
				TestNotNull(TEXT("5.3 TaskAssetB created"), TaskAssetB);
				if (TaskAssetB)
				{
					TaskAssetB->SetTestBypassMontageActiveCheck(false);

					// 3. 旧 Task A 清理前明确断言：新播放及非 1.0 scale 已建立，且旧 Task 尚未 terminated
					TestEqual(TEXT("5.3 TaskAssetB sets scale to 3.5"), Player->GetAnimRootMotionTranslationScale(), 3.5f);
					TestEqual(TEXT("5.3 ASC current montage is TestMontageB"), ASC->GetCurrentMontage(), TestMontageB);
					TestTrue(TEXT("5.3 MontageB is active on MockAnimInstance"), MockAnimInstance->Montage_IsActive(TestMontageB));
					TestFalse(TEXT("5.3 Old TaskAssetA not yet terminated"), TaskAssetA->IsTerminated());

					// 4. 触发旧 Task A 清理（模拟迟到清理或显式 EndTask）
					TaskAssetA->EndTask();
					TestTrue(TEXT("5.3 Old TaskAssetA now terminated"), TaskAssetA->IsTerminated());

					// 5. 核心反例 2 断言：旧 Task A 清理绝对不得重置新播放 TestMontageB 的 scale 3.5
					TestEqual(TEXT("5.3 Old TaskAssetA cleanup does not reset TaskAssetB scale 3.5"),
						Player->GetAnimRootMotionTranslationScale(), 3.5f);
					TestTrue(TEXT("5.3 TaskAssetB montage remains actively playing"),
						MockAnimInstance->Montage_IsActive(TestMontageB));
					TestEqual(TEXT("5.3 ASC current montage remains TestMontageB"),
						ASC->GetCurrentMontage(), TestMontageB);

					// 6. 正常结束 AbilityB，断言 Scale 正确恢复为 1.0
					AbilityB->EndTestAbility();
					TestEqual(TEXT("5.3 AnimRootMotionTranslationScale restored to 1.0 after AbilityB ends"),
						Player->GetAnimRootMotionTranslationScale(), 1.0f);
				}
			}
		}
	}

	// =========================================================================
	// 6. 真实时间推进自然完成端到端验证（Natural Completion via Real World Advance）
	// =========================================================================
	{
		// 修复 4 最小真实集成门禁：
		// 1. 通过 ASC 赋予/激活测试 Ability，播放可运行 Montage，以真实时间推进（AdvanceWorld）完成播放；
		// 2. 不开 bypass，不手调 Ended；
		// 3. 断言 Completed 恰好一次、Ability 结束、状态收敛退出；
		// 4. 断言 ActivationOwnedTags 在激活期间持有、自然结束后清除。
		const FGameplayAbilitySpecHandle CompleteHandle = ASC->GiveAbility(
			FGameplayAbilitySpec(UTestManagedMontageAbility::StaticClass(), 1, INDEX_NONE, Player));
		ON_SCOPE_EXIT
		{
			if (IsValid(ASC))
			{
				ASC->CancelAbilityHandle(CompleteHandle);
				ASC->ClearAbility(CompleteHandle);
			}
		};

		FGameplayAbilitySpec* CompleteSpec = ASC->FindAbilitySpecFromHandle(CompleteHandle);
		UTestManagedMontageAbility* CompleteAbility = CompleteSpec ? Cast<UTestManagedMontageAbility>(CompleteSpec->GetPrimaryInstance()) : nullptr;
		TestNotNull(TEXT("6. CompleteAbility instanced"), CompleteAbility);

		if (CompleteAbility)
		{
			CompleteAbility->TestMontage = TestMontage;
			const bool bActivated = ASC->TryActivateAbility(CompleteHandle);
			TestTrue(TEXT("6. CompleteAbility activated"), bActivated);

			UAbilityTask_PlayActionMontage* TaskComplete = CompleteAbility->GetMontageTask();
			TestNotNull(TEXT("6. TaskComplete created"), TaskComplete);
			if (TaskComplete)
			{
				// 严格关闭 bypass：以真实活动实例和引擎生命周期推进
				TaskComplete->SetTestBypassMontageActiveCheck(false);
				TestFalse(TEXT("6. Bypass is strictly disabled"), TaskComplete->GetTestBypassMontageActiveCheck());
				TestTrue(TEXT("6. Ability is actively running before advance"), CompleteAbility->IsActive());
				TestTrue(TEXT("6. Task is active before advance"), TaskComplete->IsActive());

				// 断言激活期间存在 ActivationOwnedTags
				const FGameplayTag TagAttacking = FGameplayTag::RequestGameplayTag(TEXT("State.Action.Attacking"), false);
				const FGameplayTag TagBlockMovement = FGameplayTag::RequestGameplayTag(TEXT("State.Input.Block.Movement"), false);
				TestTrue(TEXT("6. State.Action.Attacking present during activation"), ASC->HasMatchingGameplayTag(TagAttacking));
				TestTrue(TEXT("6. State.Input.Block.Movement present during activation"), ASC->HasMatchingGameplayTag(TagBlockMovement));

				// 推进 2.5 秒触发自然播放结束（覆盖两次改速窗口后的完整播放，实际耗时约 1.55s）
				FCombatAutomationFixture::AdvanceWorld(World, 2.5f);

				// 推进后断言：Completed 委托恰好执行一次，Ability 成功调用 EndAbility 结束，Task 终止
				TestEqual(TEXT("6. CompletedCallCount is exactly 1"), CompleteAbility->CompletedCallCount, 1);
				TestTrue(TEXT("6. Real World advance naturally triggers OnCompleted via engine MontageEnded"), CompleteAbility->bCompletedCalled);
				TestFalse(TEXT("6. Ability ended cleanly upon montage completion"), CompleteAbility->IsActive());
				TestTrue(TEXT("6. Task terminated cleanly"), TaskComplete->IsTerminated());

				// 断言自然结束后标签清除
				TestFalse(TEXT("6. State.Action.Attacking cleared after completion"), ASC->HasMatchingGameplayTag(TagAttacking));
				TestFalse(TEXT("6. State.Input.Block.Movement cleared after completion"), ASC->HasMatchingGameplayTag(TagBlockMovement));
			}
		}
	}

	// Explicit pre-activation stop configuration: observe the actual instance blend,
	// separately from cancellation/failed-result delegates. Factory stays at nine parameters.
	{
		const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(FGameplayAbilitySpec(UTestManagedMontageAbility::StaticClass(), 1, INDEX_NONE, Player));
		ON_SCOPE_EXIT { ASC->CancelAbilityHandle(Handle); ASC->ClearAbility(Handle); };
		auto* Ability = Cast<UTestManagedMontageAbility>(ASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance());
		if (!TestNotNull(TEXT("Blend configuration ability"), Ability)) return false;
		TestMontage->BlendIn.SetBlendTime(0.0f);
		TestMontage->BlendOut.SetBlendTime(0.35f);
		for (int32 Mode = 0; Mode < 4; ++Mode)
		{
			for (bool bCancel : {false, true})
			{
				if (!TestTrue(TEXT("Blend test real ASC activation"), ASC->TryActivateAbility(Handle) && Ability->IsActive())) return false;
				auto* Task = UAbilityTask_PlayActionMontage::PlayActionMontage(Ability, NAME_None, TestMontage);
				if (!TestNotNull(TEXT("Blend test standard Task"), Task)) return false;
				const float Configured = Mode == 1 ? 0.1f : Mode == 2 ? 0.2f : -1.0f;
				const float Expected = Mode == 0 ? 0.0f : Mode == 3 ? 0.35f : Configured;
				if (Mode != 0) TestTrue(TEXT("Finite pre-activation blend accepted"), Task->SetOverrideBlendOutTime(Configured));
				TestFalse(TEXT("NaN rejected without replacing configured value"), Task->SetOverrideBlendOutTime(std::numeric_limits<float>::quiet_NaN()));
				TestFalse(TEXT("Infinity rejected without replacing configured value"), Task->SetOverrideBlendOutTime(std::numeric_limits<float>::infinity()));
				const int32 CancelledBefore = Ability->CancelledCallCount;
				const int32 InterruptedBefore = Ability->InterruptedCallCount;
				const int32 FailedBefore = Ability->FailedCallCount;
				Task->OnCancelled.AddDynamic(Ability, &UTestManagedMontageAbility::OnMontageCancelled);
				Task->OnInterrupted.AddDynamic(Ability, &UTestManagedMontageAbility::OnMontageInterrupted);
				Task->OnFailed.AddDynamic(Ability, &UTestManagedMontageAbility::OnMontageFailed);
				Task->ReadyForActivation();
				if (!TestTrue(TEXT("Configured Task actually playing"), Task->IsActive() && !Task->IsTerminated())) return false;
				TestFalse(TEXT("Setter rejects changes after activation"), Task->SetOverrideBlendOutTime(0.8f));
				const int32 InstanceID = Task->GetBoundMontageInstanceID();
				MockAnimInstance->TickMontageOnly(0.01f);
				MockAnimInstance->DispatchQueuedAnimEvents();
				const FGameplayEventData Begin = FManagedMontageTestHelpers::MakeRateWindowEventData(
					TagRateWindowBegin, MockAnimInstance, InstanceID, 0.5f, Player, TestMontage, NotifyA);
				ASC->HandleGameplayEvent(TagRateWindowBegin, &Begin);
				TestEqual(TEXT("Blend test starts with a live rate window"), Task->GetRateWindowLifecycle().GetActiveWindowCount(), 1);
				if (bCancel) ASC->CancelAbilityHandle(Handle);
				else Task->EndTask();
				FAnimMontageInstance* Stopped = MockAnimInstance->GetMontageInstanceForID(InstanceID);
				if (!TestNotNull(TEXT("Stopped instance remains available for blend inspection"), Stopped)) return false;
				TestTrue(TEXT("Task emitted a stop"), Stopped->IsStopped());
				TestEqual(TEXT("Stop uses configured blend duration"), Stopped->GetBlendTime(), Expected);
				TestTrue(TEXT("Stop terminates Task"), Task->IsTerminated());
				TestFalse(TEXT("Stop clears rate lifecycle"), Task->GetRateWindowLifecycle().IsBound());
				TestEqual(TEXT("GAS cancellation retains Interrupted result regardless of blend"), Ability->InterruptedCallCount - InterruptedBefore, bCancel ? 1 : 0);
				TestEqual(TEXT("GAS cancellation does not become ExternalCancel result"), Ability->CancelledCallCount, CancelledBefore);
				TestEqual(TEXT("Rejected setter does not report playback failure"), Ability->FailedCallCount, FailedBefore);
				TestFalse(TEXT("Finished Task cannot change blend"), Task->SetOverrideBlendOutTime(0.4f));
				ASC->CancelAbilityHandle(Handle);
				MockAnimInstance->TickMontageOnly(0.5f);
				MockAnimInstance->DispatchQueuedAnimEvents();
			}
		}
		auto* Unactivated = UAbilityTask_PlayActionMontage::PlayActionMontage(Ability, NAME_None, TestMontage);
		Unactivated->EndTask();
		TestFalse(TEXT("Task ended before activation also rejects setter"), Unactivated->SetOverrideBlendOutTime(0.2f));
		TestFalse(TEXT("Unactivated ended Task rejects stop ownership"), Unactivated->SetTaskOwnsMontageStop(false));

		// Stop ownership is independent of result delivery. Exercise the shared failure
		// cleanup entry as well as explicit end, external cancel and GAS cancel.
		for (int32 Exit = 0; Exit < 4; ++Exit)
		{
			if (!TestTrue(TEXT("Presentation owner real ASC activation"), ASC->TryActivateAbility(Handle) && Ability->IsActive())) return false;
			auto* Task = UAbilityTask_PlayActionMontage::PlayActionMontage(Ability, NAME_None, TestMontage,
				1.0f, NAME_None, /*AnimRootMotionTranslationScale=*/0.5f, /*StartTimeSeconds=*/0.0f,
				/*bAllowInterruptAfterBlendOut=*/false, /*CancelPolicy=*/EActionMontageCancelPolicy::None);
			TestTrue(TEXT("Stop ownership can be configured before activation"), Task->SetTaskOwnsMontageStop(false));
			const int32 InterruptedBefore = Ability->InterruptedCallCount;
			const int32 CancelledBefore = Ability->CancelledCallCount;
			const int32 FailedBefore = Ability->FailedCallCount;
			Task->OnInterrupted.AddDynamic(Ability, &UTestManagedMontageAbility::OnMontageInterrupted);
			Task->OnCancelled.AddDynamic(Ability, &UTestManagedMontageAbility::OnMontageCancelled);
			Task->OnFailed.AddDynamic(Ability, &UTestManagedMontageAbility::OnMontageFailed);
			Task->ReadyForActivation();
			if (!TestTrue(TEXT("Non-stopping Task actually playing"), Task->IsActive() && !Task->IsTerminated())) return false;
			TestFalse(TEXT("Activation freezes stop ownership"), Task->SetTaskOwnsMontageStop(true));
			const int32 InstanceID = Task->GetBoundMontageInstanceID();
			MockAnimInstance->TickMontageOnly(0.01f);
			MockAnimInstance->DispatchQueuedAnimEvents();
			const FGameplayEventData Begin = FManagedMontageTestHelpers::MakeRateWindowEventData(
				TagRateWindowBegin, MockAnimInstance, InstanceID, 0.5f, Player, TestMontage, NotifyA);
			ASC->HandleGameplayEvent(TagRateWindowBegin, &Begin);
			TestEqual(TEXT("Non-stopping Task owns its rate window"), Task->GetRateWindowLifecycle().GetActiveWindowCount(), 1);
			if (Exit == 0) Task->EndTask();
			else if (Exit == 1) ASC->CancelAbilityHandle(Handle);
			else if (Exit == 2) Task->ExternalCancel();
			else { Task->TestCleanupTask(true); Task->EndTask(); }
			FAnimMontageInstance* Instance = MockAnimInstance->GetMontageInstanceForID(InstanceID);
			if (!TestNotNull(TEXT("Presentation instance survives task cleanup"), Instance)) return false;
			TestFalse(TEXT("No Task exit sends a stop to presentation-owned playback"), Instance->IsStopped());
			TestEqual(TEXT("Rate restored despite retaining presentation"), Instance->GetPlayRate(), 1.0f);
			TestEqual(TEXT("Root Motion restored despite retaining presentation"), Player->GetAnimRootMotionTranslationScale(), 1.0f);
			TestTrue(TEXT("Non-stopping Task terminates"), Task->IsTerminated());
			TestFalse(TEXT("Rate binding cleared"), Task->GetRateWindowLifecycle().IsBound());
			TestFalse(TEXT("Rate subscription cleared"), ASC->GenericGameplayEventCallbacks.FindOrAdd(TagRateWindowBegin).IsBoundToObject(Task));
			TestFalse(TEXT("Instance result callback detached"), Instance->OnMontageEnded.IsBound());
			TestEqual(TEXT("GAS cancel still reports Interrupted with stop disabled"), Ability->InterruptedCallCount - InterruptedBefore, Exit == 1 ? 1 : 0);
			TestEqual(TEXT("External cancel still reports Cancelled with stop disabled"), Ability->CancelledCallCount - CancelledBefore, Exit == 2 ? 1 : 0);
			TestEqual(TEXT("Stop configuration does not fabricate failure"), Ability->FailedCallCount, FailedBefore);
			TestFalse(TEXT("Ended Task cannot regain stop ownership"), Task->SetTaskOwnsMontageStop(true));
			TestFalse(TEXT("Direct stop helper also respects ownership"), Task->TestStopPlayingMontage());
			ASC->CancelAbilityHandle(Handle);
			Instance->Stop(FAlphaBlend(0.0f), true);
			MockAnimInstance->TickMontageOnly(0.01f);
			MockAnimInstance->DispatchQueuedAnimEvents();
		}
	}

	return true;
#endif // WITH_EDITOR
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FManagedMontageAdoptionBatch1ATest,
	"PolyQuest.Combat.ManagedMontageAdoptionBatch1A",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FManagedMontageAdoptionBatch1ATest::RunTest(const FString& Parameters)
{
	class FProxyAccess : public UAnimInstance
	{
	public:
		static FAnimInstanceProxy& Get(UAnimInstance* Instance)
		{
			return *GetProxyOnGameThreadStatic<FAnimInstanceProxy>(Instance);
		}
	};
	const TArray<TSubclassOf<UGameplayAbility>> Classes = {
		UPlayerSmallHitReactionAbility::StaticClass(), UPlayerGuardAbility::StaticClass(),
		UPlayerGuardBreakAbility::StaticClass(), UPlayerParryAbility::StaticClass()
	};
	// Each case has its own ASC and effects; no CDO or production asset is modified.
	for (const TSubclassOf<UGameplayAbility>& AbilityClass : Classes)
	// Natural, ASC cancel, late interruption, early interruption, immediate reactivation.
	for (int32 ExitCase = 0; ExitCase < 5; ++ExitCase)
	{
		const FString Case = FString::Printf(TEXT("%s / exit %d"), *AbilityClass->GetName(), ExitCase);
		if (!TestNotNull(TEXT("GEngine exists"), GEngine)) return false;
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		if (!TestNotNull(*Case, World)) return false;
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		ManagedMontageAutomation::FTestWorldScope WorldScope{ World };
		FURL URL;
		World->InitializeActorsForPlay(URL);
		World->BeginPlay();
		APlayerCharacter* Player = FCombatAutomationFixture::SpawnPlayer(World);
		if (!TestNotNull(*(Case + TEXT(" player")), Player)) return false;
		UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
		USkeletalMeshComponent* Mesh = Player->GetMesh();
		UCharacterMovementComponent* Movement = Player->GetCharacterMovement();
		if (!TestNotNull(*(Case + TEXT(" ASC")), ASC) || !Mesh || !Movement) return false;
		const auto Setup = ManagedMontageAutomation::CreatePlayableTestMontage(*this, World);
		UAnimMontage* Montage = Setup.Montage;
		if (!TestNotNull(*(Case + TEXT(" montage")), Montage)) return false;
		Montage->BlendOut.SetBlendTime(0.2f);
		UAnimInstance* Anim = NewObject<UAnimInstance>(Mesh);
		Anim->InitializeMontageOnly();
		Anim->CurrentSkeleton = Montage->GetSkeleton();
		UAnimInstance* PreviousAnim = Mesh->AnimScriptInstance;
		Mesh->AnimScriptInstance = Anim;
		ASC->RefreshAbilityActorInfo();
		const bool bMeshTick = Mesh->IsComponentTickEnabled();
		const bool bMovementTick = Movement->IsComponentTickEnabled();
		const bool bAutonomousPose = Mesh->bIsAutonomousTickPose;
		Mesh->SetComponentTickEnabled(false);
		Movement->SetComponentTickEnabled(false);
		ON_SCOPE_EXIT
		{
			ASC->CancelAllAbilities();
			Mesh->AnimScriptInstance = PreviousAnim;
			Mesh->SetComponentTickEnabled(bMeshTick);
			Movement->SetComponentTickEnabled(bMovementTick);
			Mesh->bIsAutonomousTickPose = bAutonomousPose;
			ASC->RefreshAbilityActorInfo();
		};
		// Same single animation driver as ManagedMontageCancelWindow section 9.
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
		Movement->SetMovementMode(MOVE_Walking);
		const bool bSmallHitReaction = AbilityClass == UPlayerSmallHitReactionAbility::StaticClass();
		const bool bGuardBreak = AbilityClass == UPlayerGuardBreakAbility::StaticClass();
		const bool bGuard = AbilityClass == UPlayerGuardAbility::StaticClass();
		const bool bParry = AbilityClass == UPlayerParryAbility::StaticClass();
		ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxStaminaAttribute(), 100.0f);
		ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), bGuardBreak ? 0.0f : 100.0f);
		if (bGuard) Player->TriggerTestHandleCombatInputStarted(FGameplayTag::RequestGameplayTag(TEXT("Input.Guard")));
		const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, Player));
		FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle);
		UGameplayAbility* Ability = Spec ? Spec->GetPrimaryInstance() : nullptr;
		if (!TestNotNull(*(Case + TEXT(" primary instance")), Ability)) return false;
		const auto Configure = [&](const FName Field, UObject* Value)
		{
			FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Ability->GetClass(), Field);
			if (!TestNotNull(*(Case + TEXT(" property ") + Field.ToString()), Property)) return false;
			Property->SetObjectPropertyValue_InContainer(Ability, Value);
			return true;
		};
		if (AbilityClass == UPlayerSmallHitReactionAbility::StaticClass())
		{
			for (const FName Field : { FName(TEXT("FrontSmallHitReactionMontage")), FName(TEXT("BackSmallHitReactionMontage")),
				FName(TEXT("LeftSmallHitReactionMontage")), FName(TEXT("RightSmallHitReactionMontage")) })
				if (!Configure(Field, Montage)) return false;
		}
		else if (!Configure(bGuard ? TEXT("GuardMontage") : bGuardBreak ? TEXT("GuardBreakMontage") : TEXT("ParryMontage"), Montage)) return false;
		if (bGuard)
		{
			if (!Configure(TEXT("GuardMoveSpeedGameplayEffectClass"), UTestMobileBowMoveSpeedGE::StaticClass())
				|| !Configure(TEXT("GuardStaminaRegenMultiplierGameplayEffectClass"), UTestMobileBowMoveSpeedGE::StaticClass())
				|| !Configure(TEXT("GuardStaminaCostGameplayEffectClass"), UGameplayEffect::StaticClass())
				|| !Configure(TEXT("StaminaRegenDelayGameplayEffectClass"), UGameplayEffect::StaticClass())) return false;
		}
		if (bParry)
		{
			if (!Configure(TEXT("CostGameplayEffectClass"), UGameplayEffect::StaticClass())
				|| !Configure(TEXT("CooldownGameplayEffectClass"), UTestMobileBowMoveSpeedGE::StaticClass())
				|| !Configure(TEXT("ParryCounterPoiseGameplayEffectClass"), UGameplayEffect::StaticClass())
				|| !Configure(TEXT("StaminaRegenDelayGameplayEffectClass"), UGameplayEffect::StaticClass())) return false;
		}
		FGameplayEffectContextHandle SmallHitContext;
		if (bSmallHitReaction)
		{
			FCombatImpactEffectContext* ImpactContext = new FCombatImpactEffectContext();
			ImpactContext->SetWorldIncomingDirection(Player->GetActorForwardVector());
			SmallHitContext = FGameplayEffectContextHandle(ImpactContext);
			if (!TestTrue(*(Case + TEXT(" explicit hit direction context")), SmallHitContext.IsValid())) return false;
		}
		const auto ActivateRealAbility = [&]()
		{
			if (!bSmallHitReaction)
			{
				return ASC->TryActivateAbility(Handle);
			}

			const FGameplayTag SmallHitEventTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Reaction.Player.Small"));
			FGameplayEventData ReactionEventData;
			ReactionEventData.EventTag = SmallHitEventTag;
			ReactionEventData.Target = Player;
			ReactionEventData.EventMagnitude = 1.0f;
			ReactionEventData.ContextHandle = SmallHitContext;
			return ASC->HandleGameplayEvent(SmallHitEventTag, &ReactionEventData) > 0;
		};
		int32 CooldownApplications = 0;
		const FDelegateHandle EffectReceipt = ASC->OnGameplayEffectAppliedDelegateToSelf.AddLambda(
			[&](UAbilitySystemComponent*, const FGameplayEffectSpec& EffectSpec, FActiveGameplayEffectHandle)
			{
				if (bParry && EffectSpec.Def == GetDefault<UTestMobileBowMoveSpeedGE>()) ++CooldownApplications;
			});
		ON_SCOPE_EXIT { ASC->OnGameplayEffectAppliedDelegateToSelf.Remove(EffectReceipt); };
		if (!TestTrue(*(Case + TEXT(" real ASC activation")), ActivateRealAbility())
			|| !TestTrue(*(Case + TEXT(" remains active")), Ability->IsActive())) return false;
		const FObjectPropertyBase* TaskProperty = FindFProperty<FObjectPropertyBase>(Ability->GetClass(), TEXT("MontageTask"));
		UAbilityTask_PlayActionMontage* Task = TaskProperty
			? Cast<UAbilityTask_PlayActionMontage>(TaskProperty->GetObjectPropertyValue_InContainer(Ability)) : nullptr;
		if (!TestNotNull(*(Case + TEXT(" actual standard Task")), Task)) return false;
		TestEqual(*(Case + TEXT(" explicit None")), Task->GetCancelPolicy(), EActionMontageCancelPolicy::None);
		TestFalse(*(Case + TEXT(" no bypass")), Task->GetTestBypassMontageActiveCheck());
		TestEqual(*(Case + TEXT(" starts at zero")), Anim->Montage_GetPosition(Montage), 0.0f);
		TestEqual(*(Case + TEXT(" root motion scale")), Player->GetAnimRootMotionTranslationScale(), 1.0f);
		if (bGuard) TestTrue(*(Case + TEXT(" Guard effects confirmed")), CastChecked<UPlayerGuardAbility>(Ability)->IsGuardActive());
		const int32 InstanceID = Task->GetBoundMontageInstanceID();
		Advance(0.15f);
		TestEqual(*(Case + TEXT(" authored rate window opens")), Task->GetRateWindowLifecycle().GetActiveWindowCount(), 1);
		TestEqual(*(Case + TEXT(" authored rate applied")), Anim->Montage_GetPlayRate(Montage), 0.4f);
		TestFalse(*(Case + TEXT(" no Dodge permission")), ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Action.CanCancel.Dodge"))));
		TestFalse(*(Case + TEXT(" no Defense permission")), ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Action.CanCancel.Defense"))));
		TestEqual(*(Case + TEXT(" cooldown not paid at activation")), CooldownApplications, 0);
		if (ExitCase == 1)
		{
			ASC->CancelAbilityHandle(Handle);
			if (const FAnimMontageInstance* Instance = Anim->GetMontageInstanceForID(InstanceID))
				TestEqual(*(Case + TEXT(" cancel restores rate before stopping")), Instance->GetPlayRate(), 1.0f);
		}
		else if (ExitCase == 3)
		{
			Anim->Montage_Stop(0.2f, Montage);
			Advance(0.05f);
			const bool bWaitForFullBlend = AbilityClass != UPlayerSmallHitReactionAbility::StaticClass();
			TestEqual(*(Case + TEXT(" preserves interrupted blend-out lifetime")), Ability->IsActive(), bWaitForFullBlend);
			if (bGuard) TestTrue(*(Case + TEXT(" Guard effects remain through blend-out")), CastChecked<UPlayerGuardAbility>(Ability)->IsGuardActive());
			if (bGuardBreak || bParry) TestEqual(*(Case + TEXT(" movement stays locked through blend-out")), Movement->MovementMode.GetValue(), MOVE_None);
			TestEqual(*(Case + TEXT(" interrupted blend-out never commits cooldown")), CooldownApplications, 0);
			Advance(0.3f);
		}
		else if (ExitCase == 4)
		{
			ASC->CancelAbilityHandle(Handle);
			if (!TestTrue(*(Case + TEXT(" immediate real reactivation")), ActivateRealAbility())) return false;
			UAbilityTask_PlayActionMontage* NewTask = Cast<UAbilityTask_PlayActionMontage>(TaskProperty->GetObjectPropertyValue_InContainer(Ability));
			if (!TestNotNull(*(Case + TEXT(" new Task")), NewTask)) return false;
			TestTrue(*(Case + TEXT(" new playback identity")), NewTask->GetBoundMontageInstanceID() != InstanceID);
			Advance(0.05f);
			TestTrue(*(Case + TEXT(" queued old End cannot end new activation")), Ability->IsActive());
			TestFalse(*(Case + TEXT(" new Task survives old receipt")), NewTask->IsTerminated());
			ASC->CancelAbilityHandle(Handle);
			TestTrue(*(Case + TEXT(" new Task also cleans up")), NewTask->IsTerminated());
		}
		else
		{
			Advance(0.35f);
			TestEqual(*(Case + TEXT(" authored End restores baseline")), Anim->Montage_GetPlayRate(Montage), 1.0f);
			for (int32 Step = 0; Step < 60; ++Step)
			{
				const FAnimMontageInstance* Instance = Anim->GetMontageInstanceForID(InstanceID);
				if (!Instance || Instance->IsStopped()) break;
				Advance(0.025f);
			}
			FAnimMontageInstance* BlendingInstance = Anim->GetMontageInstanceForID(InstanceID);
			if (!TestNotNull(*(Case + TEXT(" natural blend-out instance exists")), BlendingInstance)
				|| !TestTrue(*(Case + TEXT(" reached natural blend-out")), BlendingInstance->IsStopped())) return false;
			TestTrue(*(Case + TEXT(" natural blend-out is not completion")), Ability->IsActive());
			TestEqual(*(Case + TEXT(" no cooldown at natural blend-out")), CooldownApplications, 0);
			// Natural blend-out has left ActiveMontagesMap; stop the exact surviving instance.
			if (ExitCase == 2) BlendingInstance->Stop(FAlphaBlend(0.0f), true);
			Advance(0.4f);
		}
		TestFalse(*(Case + TEXT(" ability ended")), Ability->IsActive());
		TestTrue(*(Case + TEXT(" task terminated")), Task->IsTerminated());
		TestFalse(*(Case + TEXT(" rate lifecycle cleared")), Task->GetRateWindowLifecycle().IsBound());
		TestEqual(*(Case + TEXT(" no window residue")), Task->GetRateWindowLifecycle().GetActiveWindowCount(), 0);
		TestEqual(*(Case + TEXT(" only natural Parry completion commits cooldown")), CooldownApplications, bParry && ExitCase == 0 ? 1 : 0);
		const FGameplayTag StateTag = FGameplayTag::RequestGameplayTag(bGuard ? TEXT("State.Action.Guarding")
			: bParry ? TEXT("State.Action.Parrying") : bGuardBreak ? TEXT("State.Status.Stunned") : TEXT("State.Action.SmallHitReacting"));
		TestFalse(*(Case + TEXT(" owned state cleared")), ASC->HasMatchingGameplayTag(StateTag));
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FManagedMontageAdoptionBatch1BTest,
	"PolyQuest.Combat.ManagedMontageAdoptionBatch1B",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FManagedMontageAdoptionBatch1BTest::RunTest(const FString& Parameters)
{
	class FProxyAccess : public UAnimInstance
	{
	public:
		static FAnimInstanceProxy& Get(UAnimInstance* Instance)
		{
			return *GetProxyOnGameThreadStatic<FAnimInstanceProxy>(Instance);
		}
	};
	const TArray<TSubclassOf<UGameplayAbility>> Classes = {
		UEnemyHitReactionAbility::StaticClass(), UEnemyMeleeAbility::StaticClass(),
		UEnemyLaunchReactionAbility::StaticClass(), UEnemyStanceBreakAbility::StaticClass()
	};
	// Each case has its own ASC and effects; no CDO or production asset is modified.
	for (const TSubclassOf<UGameplayAbility>& AbilityClass : Classes)
	// Natural, ASC cancel, late interruption, early interruption, immediate reactivation.
	for (int32 ExitCase = 0; ExitCase < 5; ++ExitCase)
	{
		const FString Case = FString::Printf(TEXT("%s / exit %d"), *AbilityClass->GetName(), ExitCase);
		if (!TestNotNull(TEXT("GEngine exists"), GEngine)) return false;
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		if (!TestNotNull(*Case, World)) return false;
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		ManagedMontageAutomation::FTestWorldScope WorldScope{ World };
		FURL URL;
		World->InitializeActorsForPlay(URL);
		World->BeginPlay();
		const bool bMelee = AbilityClass == UEnemyMeleeAbility::StaticClass();
		const bool bLaunch = AbilityClass == UEnemyLaunchReactionAbility::StaticClass();
		const bool bStance = AbilityClass == UEnemyStanceBreakAbility::StaticClass();
		const auto Setup = ManagedMontageAutomation::CreatePlayableTestMontage(*this, World);
		UAnimMontage* Montage = Setup.Montage;
		if (!TestNotNull(*(Case + TEXT(" montage")), Montage)) return false;
		if (bLaunch)
		{
			CastChecked<UAnimSequence>(Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[0].GetAnimReference())->bEnableRootMotion = true;
			TestTrue(*(Case + TEXT(" authored root motion")), Montage->HasRootMotion());
		}
		UEnemyAttackProfile* Attack = NewObject<UEnemyAttackProfile>(World);
		Attack->SetTestMontage(Montage);
		Attack->SetTestDamageEffectClass(UTestProjectileDamageGE::StaticClass());
		Attack->SetTestAttackRange(200.0f);
		Attack->SetTestCooldown(0.0f);
		Attack->SetTestGuardStaminaDamage(10.0f);
		UEnemyAttackSet* AttackSet = NewObject<UEnemyAttackSet>(World);
		AttackSet->SetTestEngagementRange(250.0f);
		AttackSet->AddTestEntry(Attack, 1.0f);
		UEnemyAIProfile* AIProfile = NewObject<UEnemyAIProfile>(World);
		AIProfile->SetTestPreferredCombatDistance(180.0f);
		AIProfile->SetTestLateralRepositionDistance(100.0f);
		AIProfile->SetTestRepositionAcceptanceRadius(50.0f);
		AIProfile->SetTestRepositionRetryDelay(1.0f);
		AIProfile->SetTestLeashRadius(1000.0f);
		AIProfile->SetTestApproachTimeout(5.0f);
		AEnemyCharacter* Enemy = FCombatAutomationFixture::SpawnPassiveEnemy(World, FTransform::Identity,
			[&](AEnemyCharacter& Spawned) { Spawned.SetTestAttackSet(AttackSet); Spawned.SetTestAIProfile(AIProfile); });
		if (!TestNotNull(*(Case + TEXT(" enemy")), Enemy)) return false;
		UAbilitySystemComponent* ASC = Enemy->GetAbilitySystemComponent();
		USkeletalMeshComponent* Mesh = Enemy->GetMesh();
		UCharacterMovementComponent* Movement = Enemy->GetCharacterMovement();
		if (!TestNotNull(*(Case + TEXT(" ASC")), ASC) || !Mesh || !Movement) return false;
		AEnemyAIController* AI = nullptr;
		if (bMelee)
		{
			APlayerCharacter* Target = FCombatAutomationFixture::SpawnPlayer(World, FTransform(FVector(100.0f, 0.0f, 0.0f)));
			AI = World->SpawnActor<AEnemyAIController>();
			if (!TestNotNull(*(Case + TEXT(" target")), Target) || !TestNotNull(*(Case + TEXT(" AI")), AI)) return false;
			AI->Possess(Enemy);
			AI->SetTestTargetForAutomation(Target);
		}
		Montage->BlendOut.SetBlendTime(0.2f);
		UAnimInstance* Anim = NewObject<UAnimInstance>(Mesh);
		Anim->InitializeMontageOnly();
		Anim->CurrentSkeleton = Montage->GetSkeleton();
		UAnimInstance* PreviousAnim = Mesh->AnimScriptInstance;
		Mesh->AnimScriptInstance = Anim;
		ASC->RefreshAbilityActorInfo();
		const bool bMeshTick = Mesh->IsComponentTickEnabled();
		const bool bMovementTick = Movement->IsComponentTickEnabled();
		const bool bAutonomousPose = Mesh->bIsAutonomousTickPose;
		Mesh->SetComponentTickEnabled(false);
		Movement->SetComponentTickEnabled(false);
		ON_SCOPE_EXIT
		{
			ASC->CancelAllAbilities();
			Mesh->AnimScriptInstance = PreviousAnim;
			Mesh->SetComponentTickEnabled(bMeshTick);
			Movement->SetComponentTickEnabled(bMovementTick);
			Mesh->bIsAutonomousTickPose = bAutonomousPose;
			ASC->RefreshAbilityActorInfo();
		};
		// Same single animation driver as ManagedMontageCancelWindow section 9.
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
		Movement->SetMovementMode(MOVE_Walking);
		const bool bOriginalLedges = Movement->bCanWalkOffLedges;
		const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, Enemy));
		FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle);
		UGameplayAbility* Ability = Spec ? Spec->GetPrimaryInstance() : nullptr;
		if (!TestNotNull(*(Case + TEXT(" granted instance")), Ability)) return false;
		if (UEnemyHitReactionAbility* Hit = Cast<UEnemyHitReactionAbility>(Ability))
		{
			for (const FName Field : { FName(TEXT("FrontHitReactionMontage")), FName(TEXT("BackHitReactionMontage")),
				FName(TEXT("LeftHitReactionMontage")), FName(TEXT("RightHitReactionMontage")) })
			{
				FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(Hit->GetClass(), Field);
				if (!TestNotNull(*(Case + TEXT(" direction property ") + Field.ToString()), Property)) return false;
				Property->SetObjectPropertyValue_InContainer(Hit, Montage);
			}
		}
		if (bLaunch) CastChecked<UEnemyLaunchReactionAbility>(Ability)->SetTestRootMotionKnockdownMontage(Montage);
		if (bStance) CastChecked<UEnemyStanceBreakAbility>(Ability)->SetTestStanceBreakMontage(Montage);
		const auto Activate = [&]()
		{
			if (bMelee) return AI->PreparePendingAttackProfile() && AI->TryRequestMeleeAttack();
			if (bStance)
			{
				ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetPoiseAttribute(), 0.0f);
				return ASC->TryActivateAbility(Handle);
			}
			FCombatImpactEffectContext* Impact = new FCombatImpactEffectContext();
			Impact->SetWorldIncomingDirection(Enemy->GetActorForwardVector());
			FGameplayEventData Payload;
			Payload.EventTag = FGameplayTag::RequestGameplayTag(bLaunch ? TEXT("Event.Reaction.Enemy.Launch") : TEXT("Event.Reaction.Enemy.Big"));
			Payload.Target = Enemy;
			Payload.EventMagnitude = 1.0f;
			Payload.ContextHandle = FGameplayEffectContextHandle(Impact);
			return ASC->HandleGameplayEvent(Payload.EventTag, &Payload) > 0;
		};
		if (!TestTrue(*(Case + TEXT(" real ASC activation")), Activate())
			|| !TestTrue(*(Case + TEXT(" remains active")), Ability->IsActive())) return false;
		const FObjectPropertyBase* TaskProperty = FindFProperty<FObjectPropertyBase>(Ability->GetClass(), TEXT("MontageTask"));
		const auto GetTask = [&]() { return TaskProperty ? Cast<UAbilityTask_PlayActionMontage>(TaskProperty->GetObjectPropertyValue_InContainer(Ability)) : nullptr; };
		UAbilityTask_PlayActionMontage* Task = GetTask();
		if (!TestNotNull(*(Case + TEXT(" standard Task")), Task)) return false;
		TestEqual(*(Case + TEXT(" None policy")), Task->GetCancelPolicy(), EActionMontageCancelPolicy::None);
		TestFalse(*(Case + TEXT(" no bypass")), Task->GetTestBypassMontageActiveCheck());
		TestEqual(*(Case + TEXT(" starts at zero")), Anim->Montage_GetPosition(Montage), 0.0f);
		TestEqual(*(Case + TEXT(" root motion scale")), Enemy->GetAnimRootMotionTranslationScale(), 1.0f);
		const int32 InstanceID = Task->GetBoundMontageInstanceID();
		Advance(0.15f);
		TestEqual(*(Case + TEXT(" native Rate Notify opens")), Task->GetRateWindowLifecycle().GetActiveWindowCount(), 1);
		TestEqual(*(Case + TEXT(" native Rate Notify applies")), Anim->Montage_GetPlayRate(Montage), 0.4f);
		TestFalse(*(Case + TEXT(" no Dodge permission")), ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Action.CanCancel.Dodge"))));
		TestFalse(*(Case + TEXT(" no Defense permission")), ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Action.CanCancel.Defense"))));
		if (ExitCase == 1 || ExitCase == 4)
		{
			UEnemyStanceBreakExecutionContext* OldContext = bStance ? CastChecked<UEnemyStanceBreakAbility>(Ability)->GetTestActiveContext() : nullptr;
			ASC->CancelAbilityHandle(Handle);
			if (const FAnimMontageInstance* OldInstance = Anim->GetMontageInstanceForID(InstanceID))
				TestEqual(*(Case + TEXT(" cancellation restores rate before stop")), OldInstance->GetPlayRate(), 1.0f);
			if (ExitCase == 4)
			{
				if (bMelee)
				{
					// The fractional world time must not round a zero cooldown into the future.
					TestFalse(*(Case + TEXT(" zero cooldown permits same-frame reactivation")), AI->IsMeleeAttackOnCooldown());
					TestFalse(*(Case + TEXT(" cancellation clears attacking state")), AI->IsEnemyMeleeAttackActive());
					TestTrue(*(Case + TEXT(" reactivation target remains in range")), AI->IsCombatTargetInMeleeRange());
				}
				if (!TestTrue(*(Case + TEXT(" immediate reactivation")), Activate())) return false;
				UAbilityTask_PlayActionMontage* NewTask = GetTask();
				if (!TestNotNull(*(Case + TEXT(" new Task")), NewTask)) return false;
				TestNotEqual(*(Case + TEXT(" new instance ID")), NewTask->GetBoundMontageInstanceID(), InstanceID);
				if (OldContext) OldContext->OnMontageEnded(Montage, true);
				Advance(0.05f);
				TestTrue(*(Case + TEXT(" old completion cannot end new activation")), Ability->IsActive());
				TestFalse(*(Case + TEXT(" new Task survives old end")), NewTask->IsTerminated());
				ASC->CancelAbilityHandle(Handle);
				TestTrue(*(Case + TEXT(" new Task cleans up")), NewTask->IsTerminated());
			}
		}
		else if (ExitCase == 3)
		{
			Anim->Montage_Stop(0.2f, Montage);
			Advance(0.05f);
			TestTrue(*(Case + TEXT(" business remains active during interrupted blend")), Ability->IsActive());
			if (bStance) TestEqual(*(Case + TEXT(" stance movement stays locked during blend")), Movement->MovementMode.GetValue(), MOVE_None);
			if (!bMelee && !bStance) TestFalse(*(Case + TEXT(" reaction ledge protection stays during blend")), Movement->bCanWalkOffLedges);
			Advance(0.3f);
		}
		else
		{
			Advance(0.35f);
			TestEqual(*(Case + TEXT(" native Rate End restores baseline")), Anim->Montage_GetPlayRate(Montage), 1.0f);
			for (int32 Step = 0; Step < 60; ++Step)
			{
				const FAnimMontageInstance* Instance = Anim->GetMontageInstanceForID(InstanceID);
				if (!Instance || Instance->IsStopped()) break;
				Advance(0.025f);
			}
			FAnimMontageInstance* Instance = Anim->GetMontageInstanceForID(InstanceID);
			if (!TestNotNull(*(Case + TEXT(" natural blend instance exists")), Instance)
				|| !TestTrue(*(Case + TEXT(" natural blend started")), Instance->IsStopped())) return false;
			TestTrue(*(Case + TEXT(" natural blend is not business completion")), Ability->IsActive());
			if (ExitCase == 2) Instance->Stop(FAlphaBlend(0.0f), true);
			Advance(0.4f);
		}
		TestFalse(*(Case + TEXT(" ability ended")), Ability->IsActive());
		TestTrue(*(Case + TEXT(" task terminated")), Task->IsTerminated());
		TestFalse(*(Case + TEXT(" rate lifecycle cleared")), Task->GetRateWindowLifecycle().IsBound());
		TestEqual(*(Case + TEXT(" no window residue")), Task->GetRateWindowLifecycle().GetActiveWindowCount(), 0);
		TestEqual(*(Case + TEXT(" ledge state restored")), Movement->bCanWalkOffLedges, bOriginalLedges);
		const FGameplayTag State = FGameplayTag::RequestGameplayTag(bMelee ? TEXT("State.Action.Attacking") : bStance ? TEXT("State.Status.Stunned") : TEXT("State.Action.HitReacting"));
		TestFalse(*(Case + TEXT(" owned state cleared")), ASC->HasMatchingGameplayTag(State));
		if (bStance)
		{
			TestEqual(*(Case + TEXT(" stance restores walking")), Movement->MovementMode.GetValue(), MOVE_Walking);
			TestEqual(*(Case + TEXT(" stance restores Poise")), ASC->GetNumericAttribute(UCharacterAttributeSet::GetPoiseAttribute()), ASC->GetNumericAttribute(UCharacterAttributeSet::GetMaxPoiseAttribute()));
		}
	}
	return true;
}
#endif // WITH_EDITOR

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FManagedMontageAdoptionBatch3Test,
	"PolyQuest.Combat.ManagedMontageAdoptionBatch3", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FManagedMontageAdoptionBatch3Test::RunTest(const FString& Parameters)
{
	class FProxyAccess : public UAnimInstance
	{
	public:
		static FAnimInstanceProxy& Get(UAnimInstance* Anim) { return *GetProxyOnGameThreadStatic<FAnimInstanceProxy>(Anim); }
	};
	for (bool bFront : {true, false})
	for (int32 Exit = 0; Exit < 3; ++Exit)
	{
		const FString Case = FString::Printf(TEXT("%s / exit %d"), bFront ? TEXT("Front") : TEXT("Backstab"), Exit);
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		if (!TestNotNull(*Case, World)) return false;
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		ManagedMontageAutomation::FTestWorldScope WorldScope{World};
		FURL URL; World->InitializeActorsForPlay(URL); World->BeginPlay();
		APlayerCharacter* Player = FCombatAutomationFixture::SpawnPlayer(World);
		AEnemyCharacter* Enemy = FCombatAutomationFixture::SpawnPassiveEnemy(World,
			FTransform(FRotator(0.0f, bFront ? 180.0f : 0.0f, 0.0f), FVector(150.0f, 0.0f, 0.0f)));
		auto* Controller = World->SpawnActor<APolyQuestPlayerController>();
		if (!Player || !Enemy || !Controller) return false;
		Controller->Possess(Player);
		AActor* Floor = World->SpawnActor<AActor>();
		UBoxComponent* Box = NewObject<UBoxComponent>(Floor);
		Box->InitBoxExtent(FVector(5000.0f, 5000.0f, 50.0f));
		Box->SetCollisionProfileName(TEXT("BlockAll"));
		Floor->SetRootComponent(Box); Box->RegisterComponent();
		Floor->SetActorLocation(FVector(0.0f, 0.0f, -Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 50.0f));
		Player->GetCapsuleComponent()->IgnoreActorWhenMoving(Floor, true);
		Player->SetTestCombatTeamTag(FGameplayTag::RequestGameplayTag(TEXT("Team.Player")));
		Enemy->SetTestCombatTeamTag(FGameplayTag::RequestGameplayTag(TEXT("Team.Enemy")));
		Player->SetTestLockedTarget(Enemy);
		UAbilitySystemComponent* PlayerASC = Player->GetAbilitySystemComponent();
		UAbilitySystemComponent* EnemyASC = Enemy->GetAbilitySystemComponent();
		UAnimMontage* Montage = ManagedMontageAutomation::CreatePlayableTestMontage(*this, World).Montage;
		if (!TestNotNull(TEXT("Paired playable montage"), Montage)) return false;
		Montage->BlendOut.SetBlendTime(0.2f);
		TArray<ACharacter*> Characters = {Player, Enemy};
		TArray<UAnimInstance*> Anims;
		TArray<UAnimInstance*> PreviousAnims;
		TArray<bool> MeshTicks, MovementTicks, Poses;
		for (ACharacter* Character : Characters)
		{
			auto* Mesh = Character->GetMesh();
			PreviousAnims.Add(Mesh->AnimScriptInstance);
			MeshTicks.Add(Mesh->IsComponentTickEnabled());
			MovementTicks.Add(Character->GetCharacterMovement()->IsComponentTickEnabled());
			Poses.Add(Mesh->bIsAutonomousTickPose);
			auto* Anim = NewObject<UAnimInstance>(Mesh);
			Anim->InitializeMontageOnly(); Anim->CurrentSkeleton = Montage->GetSkeleton();
			Mesh->AnimScriptInstance = Anim; Anims.Add(Anim);
			Mesh->SetComponentTickEnabled(false);
			Character->GetCharacterMovement()->SetComponentTickEnabled(false);
			Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			FProxyAccess::Get(Anim).RegisterSlotNodeWithAnimInstance(TEXT("DefaultSlot"));
		}
		PlayerASC->RefreshAbilityActorInfo(); EnemyASC->RefreshAbilityActorInfo();
		ON_SCOPE_EXIT
		{
			PlayerASC->CancelAllAbilities(); EnemyASC->CancelAllAbilities();
			for (int32 I = 0; I < Characters.Num(); ++I)
			{
				auto* Mesh = Characters[I]->GetMesh();
				Mesh->AnimScriptInstance = PreviousAnims[I];
				Mesh->SetComponentTickEnabled(MeshTicks[I]); Mesh->bIsAutonomousTickPose = Poses[I];
				Characters[I]->GetCharacterMovement()->SetComponentTickEnabled(MovementTicks[I]);
			}
			PlayerASC->RefreshAbilityActorInfo(); EnemyASC->RefreshAbilityActorInfo();
		};
		const auto Advance = [&](float Seconds)
		{
			while (Seconds > KINDA_SMALL_NUMBER)
			{
				const float Step = FMath::Min(Seconds, 0.05f);
				FCombatAutomationFixture::TickWorld(World, Step);
				for (int32 I = 0; I < Anims.Num(); ++I)
				{
					auto& Proxy = FProxyAccess::Get(Anims[I]);
					Proxy.UpdateSlotNodeWeight(TEXT("DefaultSlot"), 1.0f, 1.0f); Proxy.FlipBufferWriteIndex();
					Proxy.UpdateSlotNodeWeight(TEXT("DefaultSlot"), 1.0f, 1.0f); Proxy.FlipBufferWriteIndex();
					Characters[I]->GetMesh()->bIsAutonomousTickPose = true;
					Anims[I]->TickMontageOnly(Step); Anims[I]->DispatchQueuedAnimEvents();
					Characters[I]->GetMesh()->bIsAutonomousTickPose = Poses[I];
				}
				Seconds -= Step;
			}
		};
		EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 100.0f);
		if (bFront)
		{
			const auto Stance = FManagedMontageTestHelpers::ActivateExecutionStanceBreak(Enemy);
			if (!TestTrue(TEXT("Front prerequisite is real active StanceBreak"), Stance.IsValid() && EnemyASC->FindAbilitySpecFromHandle(Stance)->IsActive())) return false;
		}
		const auto VictimHandle = EnemyASC->GiveAbility(FGameplayAbilitySpec(UEnemyVictimExecutionAbility::StaticClass(), 1, INDEX_NONE, Enemy));
		auto* Victim = CastChecked<UEnemyVictimExecutionAbility>(EnemyASC->FindAbilitySpecFromHandle(VictimHandle)->GetPrimaryInstance());
		Victim->SetTestVictimMontages(Montage, Montage);
		const auto SourceHandle = PlayerASC->GiveAbility(FGameplayAbilitySpec(
			bFront ? UPlayerFrontExecutionAbility::StaticClass() : UPlayerBackstabExecutionAbility::StaticClass(), 1, INDEX_NONE, Player));
		UGameplayAbility* Source = PlayerASC->FindAbilitySpecFromHandle(SourceHandle)->GetPrimaryInstance();
		auto* Front = Cast<UPlayerFrontExecutionAbility>(Source);
		auto* Backstab = Cast<UPlayerBackstabExecutionAbility>(Source);
		if (Front) { Front->SetTestExecutionMontage(Montage); Front->SetTestDamageGameplayEffectClass(UTestProjectileDamageGE::StaticClass()); Front->SetTestExecutionDistances(0.0f, 250.0f); }
		else { Backstab->SetTestExecutionMontage(Montage); Backstab->SetTestDamageGameplayEffectClass(UTestProjectileDamageGE::StaticClass()); Backstab->SetTestExecutionDistances(0.0f, 250.0f); }
		if (!TestTrue(*(Case + TEXT(" actual paired ASC activation")), PlayerASC->TryActivateAbility(SourceHandle) && Source->IsActive() && Victim->IsActive())) return false;
		auto* SourceTask = Front ? Front->GetTestMontageTask() : Backstab->GetTestMontageTask();
		if (!TestNotNull(TEXT("Player execution standard Task"), SourceTask)) return false;
		TestEqual(TEXT("Player execution None"), SourceTask->GetCancelPolicy(), EActionMontageCancelPolicy::None);
		TestEqual(TEXT("Player execution starts at zero"), Anims[0]->Montage_GetPosition(Montage), 0.0f);
		TestEqual(TEXT("Player execution root motion scale"), Player->GetAnimRootMotionTranslationScale(), 1.0f);
		TestNull(TEXT("Victim has no rate listener before recovery"), Victim->GetTestVictimMontageTask());
		Advance(0.15f);
		TestEqual(TEXT("Player execution native rate window"), SourceTask->GetRateWindowLifecycle().GetActiveWindowCount(), 1);
		TestEqual(TEXT("Player execution native rate applied"), Anims[0]->Montage_GetPlayRate(Montage), 0.4f);
		if (Exit == 1)
		{
			const int32 ID = SourceTask->GetBoundMontageInstanceID();
			PlayerASC->CancelAbilityHandle(SourceHandle);
			auto* Stopped = Anims[0]->GetMontageInstanceForID(ID);
			if (!TestNotNull(TEXT("Player stop instance"), Stopped)) return false;
			TestTrue(TEXT("Player cancelled playback"), Stopped->IsStopped());
			TestEqual(TEXT("Player preserves 0.2 second blend"), Stopped->GetBlendTime(), 0.2f);
			TestEqual(TEXT("Player restores rate before stop"), Stopped->GetPlayRate(), 1.0f);
			TestFalse(TEXT("Cancellation releases paired victim"), Victim->IsActive());
		}
		else
		{
			// Business events use the real GAS handshake; rate windows are native animation events.
			FGameplayEventData Event;
			Event.Instigator = Player; Event.Target = Player; Event.OptionalObject = Montage;
			Event.EventTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Action.Execution.Hit"));
			PlayerASC->HandleGameplayEvent(Event.EventTag, &Event);
			Event.EventTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Action.Execution.Request.VictimStart"));
			PlayerASC->HandleGameplayEvent(Event.EventTag, &Event);
			auto* VictimTask = Victim->GetTestVictimMontageTask();
			if (!TestNotNull(TEXT("Victim standard Task"), VictimTask) || !TestTrue(TEXT("Victim entered recovery"), Victim->IsTestNonLethalRecoveryActive())) return false;
			TestFalse(TEXT("Victim uses actual playback"), VictimTask->GetTestBypassMontageActiveCheck());
			TestEqual(TEXT("Victim None"), VictimTask->GetCancelPolicy(), EActionMontageCancelPolicy::None);
			TestEqual(TEXT("Victim starts at zero"), Anims[1]->Montage_GetPosition(Montage), 0.0f);
			TestEqual(TEXT("Victim root motion scale"), Enemy->GetAnimRootMotionTranslationScale(), 1.0f);
			Advance(0.15f);
			TestEqual(TEXT("Victim native rate window"), VictimTask->GetRateWindowLifecycle().GetActiveWindowCount(), 1);
			TestEqual(TEXT("Victim native rate applied"), Anims[1]->Montage_GetPlayRate(Montage), 0.4f);
			if (Exit == 2) EnemyASC->CancelAbilityHandle(VictimHandle);
			else Advance(3.0f);
			TestFalse(TEXT("Victim ends after recovery or cancel"), Victim->IsActive());
			TestTrue(TEXT("Victim Task cleaned"), VictimTask->IsTerminated());
			TestFalse(TEXT("Victim rate lifecycle cleared"), VictimTask->GetRateWindowLifecycle().IsBound());
			PlayerASC->CancelAbilityHandle(SourceHandle);
		}
		TestTrue(TEXT("Source Task cleaned"), SourceTask->IsTerminated());
		TestFalse(TEXT("Source rate lifecycle cleared"), SourceTask->GetRateWindowLifecycle().IsBound());
		for (auto* ASC : {PlayerASC, EnemyASC})
		{
			TestFalse(TEXT("Execution never grants Dodge"), ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Action.CanCancel.Dodge"))));
			TestFalse(TEXT("Execution never grants Defense"), ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Action.CanCancel.Defense"))));
		}
	}
	return true;
}
#endif

#endif // WITH_DEV_AUTOMATION_TESTS
