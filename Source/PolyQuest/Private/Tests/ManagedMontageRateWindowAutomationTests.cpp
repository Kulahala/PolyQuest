// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#if WITH_EDITOR
#include "Animation/AnimData/IAnimationDataController.h"
#endif

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Tasks/AbilityTask_PlayActionMontage.h"
#include "Animation/Combat/AnimNotifyState_ActionWindows.h"
#include "Animation/ActiveMontageInstanceScope.h"
#include "Animation/AnimComposite.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Character/Player/PlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayTagContainer.h"
#include "Misc/ScopeExit.h"
#include "Tests/CombatAutomationFixture.h"
#include "Tests/TestManagedMontageAbility.h"
#include "UObject/Package.h"

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

	return true;
#endif // WITH_EDITOR
}

#endif // WITH_DEV_AUTOMATION_TESTS
