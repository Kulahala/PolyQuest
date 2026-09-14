// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#if WITH_EDITOR
#include "Animation/AnimData/IAnimationDataController.h"
#endif

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/ChargedAttackAbility.h"
#include "AbilitySystem/Abilities/DodgeAbility.h"
#include "AbilitySystem/Abilities/PlayerGuardAbility.h"
#include "AbilitySystem/Abilities/PlayerParryAbility.h"
#include "AbilitySystem/Abilities/SprintAttackAbility.h"
#include "AbilitySystem/Tasks/AbilityTask_PlayActionMontage.h"
#include "AbilitySystem/CharacterAttributeSet.h"
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
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayTagContainer.h"
#include "Misc/ScopeExit.h"
#include "Tests/CombatAutomationFixture.h"
#include "Tests/TestManagedMontageAbility.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FManagedMontageCancelWindowAutomationTest,
	"PolyQuest.Combat.ManagedMontageCancelWindow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace ManagedMontageCancelAutomation
{
#if WITH_EDITOR
	class FAnimInstanceProxyAccess : public UAnimInstance
	{
	public:
		static FAnimInstanceProxy& GetProxy(UAnimInstance* AnimInstance)
		{
			return *GetProxyOnGameThreadStatic<FAnimInstanceProxy>(AnimInstance);
		}
	};

	class UTestMontageLengthAccess : public UAnimMontage
	{
	public:
		static void SetLength(UAnimMontage* Montage, float Length)
		{
			static_cast<UTestMontageLengthAccess*>(Montage)->SequenceLength = Length;
		}
	};

	// Shared only by section 9 and the configuration-only acceptance below.
	struct FControlledMontageAdvance
	{
		APlayerCharacter* Player;
		UAnimInstance* Anim;
		bool bMeshTick, bMovementTick, bPose;
		FControlledMontageAdvance(APlayerCharacter* InPlayer, UAnimInstance* InAnim)
			: Player(InPlayer), Anim(InAnim),
			bMeshTick(Player->GetMesh()->IsComponentTickEnabled()),
			bMovementTick(Player->GetCharacterMovement()->IsComponentTickEnabled()),
			bPose(Player->GetMesh()->bIsAutonomousTickPose)
		{
			Player->GetMesh()->SetComponentTickEnabled(false);
			Player->GetCharacterMovement()->SetComponentTickEnabled(false);
			FAnimInstanceProxyAccess::GetProxy(Anim).RegisterSlotNodeWithAnimInstance(TEXT("DefaultSlot"));
		}
		~FControlledMontageAdvance()
		{
			Player->GetMesh()->SetComponentTickEnabled(bMeshTick);
			Player->GetCharacterMovement()->SetComponentTickEnabled(bMovementTick);
			Player->GetMesh()->bIsAutonomousTickPose = bPose;
		}
		void Advance(float Seconds)
		{
			while (Seconds > KINDA_SMALL_NUMBER)
			{
				const float Step = FMath::Min(Seconds, 0.05f);
				FCombatAutomationFixture::TickWorld(Player->GetWorld(), Step);
				auto& Proxy = FAnimInstanceProxyAccess::GetProxy(Anim);
				Proxy.UpdateSlotNodeWeight(TEXT("DefaultSlot"), 1.0f, 1.0f); Proxy.FlipBufferWriteIndex();
				Proxy.UpdateSlotNodeWeight(TEXT("DefaultSlot"), 1.0f, 1.0f); Proxy.FlipBufferWriteIndex();
				Player->GetMesh()->bIsAutonomousTickPose = true;
				Anim->TickMontageOnly(Step);
				Anim->DispatchQueuedAnimEvents();
				Player->GetMesh()->bIsAutonomousTickPose = bPose;
				Seconds -= Step;
			}
		}
	};

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

	static bool SetFixtureObject(UObject* Owner, const FName PropertyName, UObject* Value)
	{
		FObjectPropertyBase* Property = Owner ? FindFProperty<FObjectPropertyBase>(Owner->GetClass(), PropertyName) : nullptr;
		if (!Property)
		{
			return false;
		}
		Property->SetObjectPropertyValue_InContainer(Owner, Value);
		return true;
	}

	struct FTestCancelMontageSetup
	{
		UAnimMontage* Montage = nullptr;
		UAnimNotifyState_ActionDodgeCancelWindow* NotifyA = nullptr; // Queued: 0.10s ~ 0.20s
		UAnimNotifyState_ActionDodgeCancelWindow* NotifyB = nullptr; // BranchingPoint: 0.15s ~ 0.25s (overlapping)
	};

	FTestCancelMontageSetup CreatePlayableCancelTestMontage(FAutomationTestBase& Test, UObject* Outer)
	{
		USkeleton* Skeleton = NewObject<USkeleton>(Outer);
		const FName RootBoneName(TEXT("root"));
		{
			FReferenceSkeletonModifier Modifier(Skeleton);
			Modifier.Add(FMeshBoneInfo(RootBoneName, TEXT("root"), INDEX_NONE), FTransform::Identity);
		}

		UAnimSequence* Sequence = NewObject<UAnimSequence>(Outer, TEXT("Seq_ManagedCancelTest"));
		Sequence->SetSkeleton(Skeleton);
		IAnimationDataController& Controller = Sequence->GetController();
		Controller.InitializeModel();
		bool bPopulated = false;
		{
			IAnimationDataController::FScopedBracket Populate(Controller,
				FText::FromString(TEXT("Populate ManagedMontageCancel fixture")), false);
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

		UAnimMontage* Montage = NewObject<UAnimMontage>(Outer, TEXT("Montage_ManagedCancelTest"));
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
		Montage->AnimNotifyTracks.Add(FAnimNotifyTrack(FName(TEXT("2")), FLinearColor::Green));
#endif

		// NotifyA: Queued (0.10s ~ 0.20s)
		UAnimNotifyState_ActionDodgeCancelWindow* NotifyA = NewObject<UAnimNotifyState_ActionDodgeCancelWindow>(Montage, TEXT("CancelNotifyA"));
		{
			FAnimNotifyEvent& EventA = Montage->Notifies.AddDefaulted_GetRef();
			EventA.NotifyName = FName(TEXT("CancelNotifyA"));
			EventA.NotifyStateClass = NotifyA;
			EventA.TrackIndex = 0;
			EventA.MontageTickType = EMontageNotifyTickType::Queued;
			EventA.Link(Montage, 0.10f);
			EventA.SetTime(0.10f);
			EventA.SetDuration(0.10f);
			EventA.EndLink.Link(Montage, 0.20f);
			EventA.EndLink.SetTime(0.20f);
		}

		// NotifyB: BranchingPoint (0.15s ~ 0.30s) - Overlaps with NotifyA from 0.15s to 0.20s!
		UAnimNotifyState_ActionDodgeCancelWindow* NotifyB = NewObject<UAnimNotifyState_ActionDodgeCancelWindow>(Montage, TEXT("CancelNotifyB"));
		{
			FAnimNotifyEvent& EventB = Montage->Notifies.AddDefaulted_GetRef();
			EventB.NotifyName = FName(TEXT("CancelNotifyB"));
			EventB.NotifyStateClass = NotifyB;
			EventB.TrackIndex = 1;
			EventB.MontageTickType = EMontageNotifyTickType::BranchingPoint;
			EventB.Link(Montage, 0.15f);
			EventB.SetTime(0.15f);
			EventB.SetDuration(0.15f);
			EventB.EndLink.Link(Montage, 0.30f);
			EventB.EndLink.SetTime(0.30f);
		}

		Montage->RefreshCacheData();

		return { Montage, NotifyA, NotifyB };
	}
#endif
}

bool FManagedMontageCancelWindowAutomationTest::RunTest(const FString& Parameters)
{
	const FGameplayTag TagCancelBegin = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.CancelWindow.Dodge.Begin")), false);
	const FGameplayTag TagCancelEnd = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.CancelWindow.Dodge.End")), false);
	const FGameplayTag TagCanCancelDodge = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.CanCancel.Dodge")), false);
	const FGameplayTag TagCanCancelDefense = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.CanCancel.Defense")), false);
	const FGameplayTag TagCancelableDodge = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.CancelableBy.Dodge")), false);
	const FGameplayTag TagCancelableDefense = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.CancelableBy.Defense")), false);

	TestTrue(TEXT("Tag Event.Action.CancelWindow.Dodge.Begin is valid"), TagCancelBegin.IsValid());
	TestTrue(TEXT("Tag Event.Action.CancelWindow.Dodge.End is valid"), TagCancelEnd.IsValid());
	TestTrue(TEXT("Tag State.Action.CanCancel.Dodge is valid"), TagCanCancelDodge.IsValid());
	TestTrue(TEXT("Tag State.Action.CanCancel.Defense is valid"), TagCanCancelDefense.IsValid());

#if !WITH_EDITOR
	// Non-Editor Development build: test pure C++ structs, validation contracts, and static guarantees
	FGameplayAbilityTargetData_MontageRateWindowSource TestTargetData;
	TestTargetData.MontageInstanceID = 123;
	TestTargetData.bReachedEnd = true;
	TestTrue(TEXT("TargetData bReachedEnd is true"), TestTargetData.bReachedEnd);
	TestTrue(TEXT("TargetData GetScriptStruct matches StaticStruct"),
		TestTargetData.GetScriptStruct() == FGameplayAbilityTargetData_MontageRateWindowSource::StaticStruct());

	const FGameplayEventData EventData = FManagedMontageTestHelpers::MakeCancelWindowEventData(
		TagCancelBegin, nullptr, 123, nullptr, nullptr, nullptr, false);
	TestTrue(TEXT("Helper constructs valid TargetData"), EventData.TargetData.IsValid(0));

	FString Reason;
	const bool bValid = FManagedMontageTestHelpers::ValidateCancelWindowConfiguration(
		GetDefault<UTestManagedMontageAbility>(), nullptr, nullptr,
		EActionMontageCancelPolicy::DodgeAndDefense, {}, Reason);
	TestFalse(TEXT("Enabled policy requires an actual Task"), bValid);

	return true;
#else
	// WITH_EDITOR: Full runtime integration tests with synthetic animation and real time advance

	if (!TestNotNull(TEXT("GEngine is valid"), GEngine))
	{
		return false;
	}

	using namespace ManagedMontageCancelAutomation;
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("ManagedMontageCancelTestWorld"));
	WorldContext.SetCurrentWorld(World);
	ManagedMontageCancelAutomation::FTestWorldScope ScopeCleanup{ World };

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

	ManagedMontageCancelAutomation::FTestCancelMontageSetup Setup = ManagedMontageCancelAutomation::CreatePlayableCancelTestMontage(*this, World);
	UAnimMontage* TestMontage = Setup.Montage;
	UAnimNotifyState_ActionDodgeCancelWindow* NotifyA = Setup.NotifyA;
	UAnimNotifyState_ActionDodgeCancelWindow* NotifyB = Setup.NotifyB;
	if (!TestNotNull(TEXT("TestMontage created"), TestMontage) || !TestNotNull(TEXT("NotifyA created"), NotifyA) || !TestNotNull(TEXT("NotifyB created"), NotifyB))
	{
		return false;
	}

	UAnimInstance* MockAnimInstance = NewObject<UAnimInstance>(Player->GetMesh());
	MockAnimInstance->InitializeMontageOnly();
	MockAnimInstance->CurrentSkeleton = TestMontage->GetSkeleton();
	USkeletalMeshComponent* PlayerMesh = Player->GetMesh();
	PlayerMesh->AnimScriptInstance = MockAnimInstance;
	ASC->RefreshAbilityActorInfo();

	// =========================================================================
	// 1. 策略支持与单窗生命周期 (Cancel Policy & Single Window Lifecycle)
	// =========================================================================
	{
		// 1.1 Policy::None: 默认策略不绑定 CancelWindow 事件，不授予标签
		{
			const FGameplayAbilitySpecHandle NoneHandle = ASC->GiveAbility(
				FGameplayAbilitySpec(UTestManagedMontageAbility::StaticClass(), 1, INDEX_NONE, Player));
			ON_SCOPE_EXIT
			{
				if (IsValid(ASC))
				{
					ASC->CancelAbilityHandle(NoneHandle);
					ASC->ClearAbility(NoneHandle);
				}
			};

			UTestManagedMontageAbility* NoneAbility = Cast<UTestManagedMontageAbility>(ASC->FindAbilitySpecFromHandle(NoneHandle)->GetPrimaryInstance());
			TestNotNull(TEXT("1.1 NoneAbility instanced"), NoneAbility);
			if (NoneAbility)
			{
				NoneAbility->TestMontage = TestMontage;
				NoneAbility->TestCancelPolicy = EActionMontageCancelPolicy::None;
				const bool bActivated = ASC->TryActivateAbility(NoneHandle);
				TestTrue(TEXT("1.1 NoneAbility activated"), bActivated);

				UAbilityTask_PlayActionMontage* Task = NoneAbility->GetMontageTask();
				TestNotNull(TEXT("1.1 Task created"), Task);
				if (Task)
				{
					Task->SetTestBypassMontageActiveCheck(true);
					const int32 BoundID = Task->GetBoundMontageInstanceID();

					// 注入合法 Begin 事件
					const FGameplayEventData BeginEvent = FManagedMontageTestHelpers::MakeCancelWindowEventData(
						TagCancelBegin, MockAnimInstance, BoundID, Player, TestMontage, NotifyA, false);
					ASC->HandleGameplayEvent(TagCancelBegin, &BeginEvent);

					TestEqual(TEXT("1.1 Policy None: ActiveCancelWindowCount remains 0"), Task->GetActiveCancelWindowCount(), 0);
					TestFalse(TEXT("1.1 Policy None: CanCancel.Dodge is NOT granted"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
					TestFalse(TEXT("1.1 Policy None: CanCancel.Defense is NOT granted"), ASC->HasMatchingGameplayTag(TagCanCancelDefense));
				}
				NoneAbility->EndTestAbility();
			}
		}

		// 1.2 Policy::DodgeOnly: 仅授予 Dodge 标签
		{
			const FGameplayAbilitySpecHandle DodgeOnlyHandle = ASC->GiveAbility(
				FGameplayAbilitySpec(UTestManagedMontageAbility::StaticClass(), 1, INDEX_NONE, Player));
			ON_SCOPE_EXIT
			{
				if (IsValid(ASC))
				{
					ASC->CancelAbilityHandle(DodgeOnlyHandle);
					ASC->ClearAbility(DodgeOnlyHandle);
				}
			};

			UTestManagedMontageAbility* DodgeOnlyAbility = Cast<UTestManagedMontageAbility>(ASC->FindAbilitySpecFromHandle(DodgeOnlyHandle)->GetPrimaryInstance());
			TestNotNull(TEXT("1.2 DodgeOnlyAbility instanced"), DodgeOnlyAbility);
			if (DodgeOnlyAbility)
			{
				DodgeOnlyAbility->TestMontage = TestMontage;
				DodgeOnlyAbility->TestCancelPolicy = EActionMontageCancelPolicy::DodgeOnly;
				const bool bActivated = ASC->TryActivateAbility(DodgeOnlyHandle);
				TestTrue(TEXT("1.2 DodgeOnlyAbility activated"), bActivated);

				UAbilityTask_PlayActionMontage* Task = DodgeOnlyAbility->GetMontageTask();
				TestNotNull(TEXT("1.2 Task created"), Task);
				if (Task)
				{
					Task->SetTestBypassMontageActiveCheck(true);
					const int32 BoundID = Task->GetBoundMontageInstanceID();

					// 发送合法 Begin
					const FGameplayEventData BeginEvent = FManagedMontageTestHelpers::MakeCancelWindowEventData(
						TagCancelBegin, MockAnimInstance, BoundID, Player, TestMontage, NotifyA, false);
					ASC->HandleGameplayEvent(TagCancelBegin, &BeginEvent);

					TestEqual(TEXT("1.2 Policy DodgeOnly: ActiveCancelWindowCount is 1"), Task->GetActiveCancelWindowCount(), 1);
					TestTrue(TEXT("1.2 Policy DodgeOnly: CanCancel.Dodge is granted"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
					TestFalse(TEXT("1.2 Policy DodgeOnly: CanCancel.Defense is NOT granted"), ASC->HasMatchingGameplayTag(TagCanCancelDefense));

					// 重复 Begin 幂等
					ASC->HandleGameplayEvent(TagCancelBegin, &BeginEvent);
					TestEqual(TEXT("1.2 Duplicate Begin: count remains 1"), Task->GetActiveCancelWindowCount(), 1);

					// 发送合法 End
					const FGameplayEventData EndEvent = FManagedMontageTestHelpers::MakeCancelWindowEventData(
						TagCancelEnd, MockAnimInstance, BoundID, Player, TestMontage, NotifyA, true);
					ASC->HandleGameplayEvent(TagCancelEnd, &EndEvent);

					TestEqual(TEXT("1.2 Policy DodgeOnly: ActiveCancelWindowCount is 0"), Task->GetActiveCancelWindowCount(), 0);
					TestFalse(TEXT("1.2 Policy DodgeOnly: CanCancel.Dodge is removed"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));

					// 重复 End 幂等
					ASC->HandleGameplayEvent(TagCancelEnd, &EndEvent);
					TestEqual(TEXT("1.2 Duplicate End: count remains 0"), Task->GetActiveCancelWindowCount(), 0);
				}
				DodgeOnlyAbility->EndTestAbility();
			}
		}

		// 1.3 Policy::DodgeAndDefense: 同时授予 Dodge 与 Defense 标签，及 End-before-Begin 防御
		{
			const FGameplayAbilitySpecHandle BothHandle = ASC->GiveAbility(
				FGameplayAbilitySpec(UTestManagedMontageAbility::StaticClass(), 1, INDEX_NONE, Player));
			ON_SCOPE_EXIT
			{
				if (IsValid(ASC))
				{
					ASC->CancelAbilityHandle(BothHandle);
					ASC->ClearAbility(BothHandle);
				}
			};

			UTestManagedMontageAbility* BothAbility = Cast<UTestManagedMontageAbility>(ASC->FindAbilitySpecFromHandle(BothHandle)->GetPrimaryInstance());
			TestNotNull(TEXT("1.3 BothAbility instanced"), BothAbility);
			if (BothAbility)
			{
				BothAbility->TestMontage = TestMontage;
				BothAbility->TestCancelPolicy = EActionMontageCancelPolicy::DodgeAndDefense;
				const bool bActivated = ASC->TryActivateAbility(BothHandle);
				TestTrue(TEXT("1.3 BothAbility activated"), bActivated);

				UAbilityTask_PlayActionMontage* Task = BothAbility->GetMontageTask();
				TestNotNull(TEXT("1.3 Task created"), Task);
				if (Task)
				{
					Task->SetTestBypassMontageActiveCheck(true);
					const int32 BoundID = Task->GetBoundMontageInstanceID();

					// 1.4 End-before-Begin: 未经 Begin 的 End 事件绝无副作用
					const FGameplayEventData UnsolicitedEnd = FManagedMontageTestHelpers::MakeCancelWindowEventData(
						TagCancelEnd, MockAnimInstance, BoundID, Player, TestMontage, NotifyA, true);
					ASC->HandleGameplayEvent(TagCancelEnd, &UnsolicitedEnd);
					TestEqual(TEXT("1.4 End-before-Begin: count remains 0"), Task->GetActiveCancelWindowCount(), 0);
					TestFalse(TEXT("1.4 End-before-Begin: CanCancel.Dodge absent"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
					TestFalse(TEXT("1.4 End-before-Begin: CanCancel.Defense absent"), ASC->HasMatchingGameplayTag(TagCanCancelDefense));

					// 发送合法 Begin
					const FGameplayEventData BeginEvent = FManagedMontageTestHelpers::MakeCancelWindowEventData(
						TagCancelBegin, MockAnimInstance, BoundID, Player, TestMontage, NotifyA, false);
					ASC->HandleGameplayEvent(TagCancelBegin, &BeginEvent);

					TestEqual(TEXT("1.3 Policy DodgeAndDefense: count is 1"), Task->GetActiveCancelWindowCount(), 1);
					TestTrue(TEXT("1.3 Policy DodgeAndDefense: CanCancel.Dodge granted"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
					TestTrue(TEXT("1.3 Policy DodgeAndDefense: CanCancel.Defense granted"), ASC->HasMatchingGameplayTag(TagCanCancelDefense));

					// 发送合法 End
					const FGameplayEventData EndEvent = FManagedMontageTestHelpers::MakeCancelWindowEventData(
						TagCancelEnd, MockAnimInstance, BoundID, Player, TestMontage, NotifyA, true);
					ASC->HandleGameplayEvent(TagCancelEnd, &EndEvent);

					TestEqual(TEXT("1.3 Policy DodgeAndDefense: count is 0"), Task->GetActiveCancelWindowCount(), 0);
					TestFalse(TEXT("1.3 Policy DodgeAndDefense: CanCancel.Dodge removed"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
					TestFalse(TEXT("1.3 Policy DodgeAndDefense: CanCancel.Defense removed"), ASC->HasMatchingGameplayTag(TagCanCancelDefense));
				}
				BothAbility->EndTestAbility();
			}
		}
	}

	// =========================================================================
	// 2. 重叠窗口并集与相邻窗口 (Overlapping Union & Adjacent Windows)
	// =========================================================================
	{
		const FGameplayAbilitySpecHandle UnionHandle = ASC->GiveAbility(
			FGameplayAbilitySpec(UTestManagedMontageAbility::StaticClass(), 1, INDEX_NONE, Player));
		ON_SCOPE_EXIT
		{
			if (IsValid(ASC))
			{
				ASC->CancelAbilityHandle(UnionHandle);
				ASC->ClearAbility(UnionHandle);
			}
		};

		UTestManagedMontageAbility* UnionAbility = Cast<UTestManagedMontageAbility>(ASC->FindAbilitySpecFromHandle(UnionHandle)->GetPrimaryInstance());
		TestNotNull(TEXT("2. UnionAbility instanced"), UnionAbility);
		if (UnionAbility)
		{
			UnionAbility->TestMontage = TestMontage;
			UnionAbility->TestCancelPolicy = EActionMontageCancelPolicy::DodgeAndDefense;
			const bool bActivated = ASC->TryActivateAbility(UnionHandle);
			TestTrue(TEXT("2. UnionAbility activated"), bActivated);

			UAbilityTask_PlayActionMontage* Task = UnionAbility->GetMontageTask();
			TestNotNull(TEXT("2. Task created"), Task);
			if (Task)
			{
				Task->SetTestBypassMontageActiveCheck(true);
				const int32 BoundID = Task->GetBoundMontageInstanceID();

				const FGameplayEventData BeginA = FManagedMontageTestHelpers::MakeCancelWindowEventData(
					TagCancelBegin, MockAnimInstance, BoundID, Player, TestMontage, NotifyA, false);
				const FGameplayEventData EndA = FManagedMontageTestHelpers::MakeCancelWindowEventData(
					TagCancelEnd, MockAnimInstance, BoundID, Player, TestMontage, NotifyA, true);
				const FGameplayEventData BeginB = FManagedMontageTestHelpers::MakeCancelWindowEventData(
					TagCancelBegin, MockAnimInstance, BoundID, Player, TestMontage, NotifyB, false);
				const FGameplayEventData EndB = FManagedMontageTestHelpers::MakeCancelWindowEventData(
					TagCancelEnd, MockAnimInstance, BoundID, Player, TestMontage, NotifyB, true);

				// 2.1 重叠并集断言：Begin A -> Begin B -> End A -> tags 必须仍然保持！
				ASC->HandleGameplayEvent(TagCancelBegin, &BeginA);
				TestEqual(TEXT("2.1 Begin A: count is 1"), Task->GetActiveCancelWindowCount(), 1);
				TestTrue(TEXT("2.1 Begin A: CanCancel.Dodge granted"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));

				ASC->HandleGameplayEvent(TagCancelBegin, &BeginB);
				TestEqual(TEXT("2.1 Begin B: count is 2"), Task->GetActiveCancelWindowCount(), 2);
				TestTrue(TEXT("2.1 Begin B: CanCancel.Dodge remains granted"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));

				ASC->HandleGameplayEvent(TagCancelEnd, &EndA);
				TestEqual(TEXT("2.1 End A: count is 1 (B still active)"), Task->GetActiveCancelWindowCount(), 1);
				TestTrue(TEXT("2.1 End A does NOT close window B: CanCancel.Dodge remains active!"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
				TestTrue(TEXT("2.1 End A does NOT close window B: CanCancel.Defense remains active!"), ASC->HasMatchingGameplayTag(TagCanCancelDefense));

				ASC->HandleGameplayEvent(TagCancelEnd, &EndB);
				TestEqual(TEXT("2.1 End B: count is 0"), Task->GetActiveCancelWindowCount(), 0);
				TestFalse(TEXT("2.1 End B: CanCancel.Dodge now removed"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
				TestFalse(TEXT("2.1 End B: CanCancel.Defense now removed"), ASC->HasMatchingGameplayTag(TagCanCancelDefense));

				// 2.2 相邻窗口序列 A：Begin A -> End A -> Begin B
				ASC->HandleGameplayEvent(TagCancelBegin, &BeginA);
				ASC->HandleGameplayEvent(TagCancelEnd, &EndA);
				TestFalse(TEXT("2.2 Seq A: End A removes tags"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
				ASC->HandleGameplayEvent(TagCancelBegin, &BeginB);
				TestTrue(TEXT("2.2 Seq A: Begin B restores tags"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
				ASC->HandleGameplayEvent(TagCancelEnd, &EndB);
				TestFalse(TEXT("2.2 Seq A: End B removes tags"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));

				// 2.3 相邻窗口序列 B（无缝衔接）：Begin A -> Begin B -> End A
				ASC->HandleGameplayEvent(TagCancelBegin, &BeginA);
				ASC->HandleGameplayEvent(TagCancelBegin, &BeginB);
				ASC->HandleGameplayEvent(TagCancelEnd, &EndA);
				TestTrue(TEXT("2.3 Seamless handoff: tags remain continuous throughout"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
				ASC->HandleGameplayEvent(TagCancelEnd, &EndB);
				TestFalse(TEXT("2.3 Seamless handoff: tags cleared after B"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
			}
			UnionAbility->EndTestAbility();
		}
	}

	// =========================================================================
	// 3. Charged 暂停锁存与自然结束区分 (Charged Pause Latch & bReachedEnd)
	// =========================================================================
	{
		const FGameplayAbilitySpecHandle PauseHandle = ASC->GiveAbility(
			FGameplayAbilitySpec(UTestManagedMontageAbility::StaticClass(), 1, INDEX_NONE, Player));
		ON_SCOPE_EXIT
		{
			if (IsValid(ASC))
			{
				ASC->CancelAbilityHandle(PauseHandle);
				ASC->ClearAbility(PauseHandle);
			}
		};

		UTestManagedMontageAbility* PauseAbility = Cast<UTestManagedMontageAbility>(ASC->FindAbilitySpecFromHandle(PauseHandle)->GetPrimaryInstance());
		TestNotNull(TEXT("3. PauseAbility instanced"), PauseAbility);
		if (PauseAbility)
		{
			PauseAbility->TestMontage = TestMontage;
			PauseAbility->TestCancelPolicy = EActionMontageCancelPolicy::DodgeAndDefense;
			const bool bActivated = ASC->TryActivateAbility(PauseHandle);
			TestTrue(TEXT("3. PauseAbility activated"), bActivated);

			UAbilityTask_PlayActionMontage* Task = PauseAbility->GetMontageTask();
			TestNotNull(TEXT("3. Task created"), Task);
			if (Task)
			{
				Task->SetTestBypassMontageActiveCheck(true);
				const int32 BoundID = Task->GetBoundMontageInstanceID();

				const FGameplayEventData BeginA = FManagedMontageTestHelpers::MakeCancelWindowEventData(
					TagCancelBegin, MockAnimInstance, BoundID, Player, TestMontage, NotifyA, false);
				const FGameplayEventData PseudoEndA = FManagedMontageTestHelpers::MakeCancelWindowEventData(
					TagCancelEnd, MockAnimInstance, BoundID, Player, TestMontage, NotifyA, false); // bReachedEnd = false!
				const FGameplayEventData NaturalEndA = FManagedMontageTestHelpers::MakeCancelWindowEventData(
					TagCancelEnd, MockAnimInstance, BoundID, Player, TestMontage, NotifyA, true); // bReachedEnd = true!

				// 3.1 窗口内暂停：Begin -> Latch -> Pseudo End (bReachedEnd=false) -> 暂停期间保留 -> Unlatch 后依然有效
				ASC->HandleGameplayEvent(TagCancelBegin, &BeginA);
				TestTrue(TEXT("3.1 Window active before pause"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));

				Task->LatchCancelWindowsAcrossPause();
				TestTrue(TEXT("3.1 Task is latched across pause"), Task->IsCancelWindowLatchedAcrossPause());

				// 动画暂停截断触发伪 End (bReachedEnd = false)
				ASC->HandleGameplayEvent(TagCancelEnd, &PseudoEndA);
				TestTrue(TEXT("3.1 Pseudo End during pause does NOT strip CanCancel.Dodge"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
				TestTrue(TEXT("3.1 Pseudo End during pause does NOT strip CanCancel.Defense"), ASC->HasMatchingGameplayTag(TagCanCancelDefense));

				// 恢复播放 (Unlatch)
				Task->UnlatchCancelWindowsAfterPause();
				TestFalse(TEXT("3.1 Task unlatched after pause"), Task->IsCancelWindowLatchedAcrossPause());
				TestTrue(TEXT("3.1 Window still active after unlatch (waiting for natural end)"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));

				// 恢复后真实到达窗口终点 (bReachedEnd = true)
				ASC->HandleGameplayEvent(TagCancelEnd, &NaturalEndA);
				TestFalse(TEXT("3.1 Natural End after resume removes CanCancel.Dodge"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
				TestFalse(TEXT("3.1 Natural End after resume removes CanCancel.Defense"), ASC->HasMatchingGameplayTag(TagCanCancelDefense));

				// 3.2 窗口终点暂停：Begin -> Latch -> Natural End (bReachedEnd=true) -> 暂停保留 -> Unlatch 时结算关闭
				ASC->HandleGameplayEvent(TagCancelBegin, &BeginA);
				TestTrue(TEXT("3.2 Window active before pause boundary"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));

				Task->LatchCancelWindowsAcrossPause();
				TestTrue(TEXT("3.2 Latched before boundary end"), Task->IsCancelWindowLatchedAcrossPause());

				// 动画刚好走到终点暂停，原生派发 bReachedEnd = true
				ASC->HandleGameplayEvent(TagCancelEnd, &NaturalEndA);
				TestTrue(TEXT("3.2 Natural End deferred: tags STILL ACTIVE during pause"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));

				// 恢复播放 (Unlatch) -> 待结算自然结束窗口立即结算关闭
				Task->UnlatchCancelWindowsAfterPause();
				TestFalse(TEXT("3.2 Unlatch settles natural end: tags removed immediately!"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
				TestEqual(TEXT("3.2 Unlatch settles natural end: count is 0"), Task->GetActiveCancelWindowCount(), 0);

				// 3.3 无窗暂停：无活跃窗口时 Latch 不产生权限
				Task->LatchCancelWindowsAcrossPause();
				TestFalse(TEXT("3.3 Latched without window does NOT grant Dodge tag"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
				ASC->HandleGameplayEvent(TagCancelEnd, &PseudoEndA);
				TestFalse(TEXT("3.3 Pseudo End during unwindowed latch has no effect"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
				Task->UnlatchCancelWindowsAfterPause();
				TestFalse(TEXT("3.3 Unlatch without window leaves tags absent"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));

				// 3.4 两个窗口部分自然结束：Window A 自然结束 (bReachedEnd=true)，Window B 截断 (bReachedEnd=false)
				const FGameplayEventData BeginB = FManagedMontageTestHelpers::MakeCancelWindowEventData(
					TagCancelBegin, MockAnimInstance, BoundID, Player, TestMontage, NotifyB, false);
				const FGameplayEventData NaturalEndB = FManagedMontageTestHelpers::MakeCancelWindowEventData(
					TagCancelEnd, MockAnimInstance, BoundID, Player, TestMontage, NotifyB, true);
				const FGameplayEventData PseudoEndB = FManagedMontageTestHelpers::MakeCancelWindowEventData(
					TagCancelEnd, MockAnimInstance, BoundID, Player, TestMontage, NotifyB, false);

				ASC->HandleGameplayEvent(TagCancelBegin, &BeginA);
				ASC->HandleGameplayEvent(TagCancelBegin, &BeginB);
				TestEqual(TEXT("3.4 Both windows active"), Task->GetActiveCancelWindowCount(), 2);

				Task->LatchCancelWindowsAcrossPause();
				ASC->HandleGameplayEvent(TagCancelEnd, &NaturalEndA); // A reached end
				ASC->HandleGameplayEvent(TagCancelEnd, &PseudoEndB);  // B truncated

				TestTrue(TEXT("3.4 Tags active during pause with partial natural end"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));

				Task->UnlatchCancelWindowsAfterPause();
				// A was naturally ended -> settled and removed. B was truncated -> still active!
				TestEqual(TEXT("3.4 Unlatch settles A only, B remains active"), Task->GetActiveCancelWindowCount(), 1);
				TestTrue(TEXT("3.4 Tags remain active because B is still open"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));

				// B later naturally ends
				ASC->HandleGameplayEvent(TagCancelEnd, &NaturalEndB);
				TestEqual(TEXT("3.4 B naturally ends, count is 0"), Task->GetActiveCancelWindowCount(), 0);
				TestFalse(TEXT("3.4 Tags removed after B ends"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));

				// 3.5 退出清理断言：在 Latch 状态下直接结束 Ability，必须完全清理
				ASC->HandleGameplayEvent(TagCancelBegin, &BeginA);
				Task->LatchCancelWindowsAcrossPause();
				TestTrue(TEXT("3.5 Window active and latched before exit"), Task->IsCancelWindowLatchedAcrossPause());

				PauseAbility->EndTestAbility();
				TestFalse(TEXT("3.5 EndAbility clears latch"), Task->IsCancelWindowLatchedAcrossPause());
				TestEqual(TEXT("3.5 EndAbility clears active windows"), Task->GetActiveCancelWindowCount(), 0);
				TestFalse(TEXT("3.5 EndAbility removes CanCancel.Dodge"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
				TestFalse(TEXT("3.5 EndAbility removes CanCancel.Defense"), ASC->HasMatchingGameplayTag(TagCanCancelDefense));
			}
		}
	}

	// =========================================================================
	// 4. 身份与来源防御 (Identity & Source Defense)
	// =========================================================================
	{
		const FGameplayAbilitySpecHandle SourceHandle = ASC->GiveAbility(
			FGameplayAbilitySpec(UTestManagedMontageAbility::StaticClass(), 1, INDEX_NONE, Player));
		ON_SCOPE_EXIT
		{
			if (IsValid(ASC))
			{
				ASC->CancelAbilityHandle(SourceHandle);
				ASC->ClearAbility(SourceHandle);
			}
		};

		UTestManagedMontageAbility* SourceAbility = Cast<UTestManagedMontageAbility>(ASC->FindAbilitySpecFromHandle(SourceHandle)->GetPrimaryInstance());
		TestNotNull(TEXT("4. SourceAbility instanced"), SourceAbility);
		if (SourceAbility)
		{
			SourceAbility->TestMontage = TestMontage;
			SourceAbility->TestCancelPolicy = EActionMontageCancelPolicy::DodgeAndDefense;
			const bool bActivated = ASC->TryActivateAbility(SourceHandle);
			TestTrue(TEXT("4. SourceAbility activated"), bActivated);

			UAbilityTask_PlayActionMontage* Task = SourceAbility->GetMontageTask();
			TestNotNull(TEXT("4. Task created"), Task);
			if (Task)
			{
				Task->SetTestBypassMontageActiveCheck(true);
				const int32 BoundID = Task->GetBoundMontageInstanceID();

				// 4.1 缺失 TargetData 拒收
				FGameplayEventData NoTargetDataEvent;
				NoTargetDataEvent.EventTag = TagCancelBegin;
				ASC->HandleGameplayEvent(TagCancelBegin, &NoTargetDataEvent);
				TestEqual(TEXT("4.1 Missing TargetData rejected"), Task->GetActiveCancelWindowCount(), 0);

				// 4.2 基类 TargetData 拒收
				FGameplayEventData BaseTargetDataEvent;
				BaseTargetDataEvent.EventTag = TagCancelBegin;
				BaseTargetDataEvent.TargetData = FGameplayAbilityTargetDataHandle(new FGameplayAbilityTargetData());
				ASC->HandleGameplayEvent(TagCancelBegin, &BaseTargetDataEvent);
				TestEqual(TEXT("4.2 Base FGameplayAbilityTargetData rejected"), Task->GetActiveCancelWindowCount(), 0);

				// 4.3 空 AnimInstance 拒收
				const FGameplayEventData NullAnimEvent = FManagedMontageTestHelpers::MakeCancelWindowEventData(
					TagCancelBegin, nullptr, BoundID, Player, TestMontage, NotifyA, false);
				ASC->HandleGameplayEvent(TagCancelBegin, &NullAnimEvent);
				TestEqual(TEXT("4.3 Null AnimInstance rejected"), Task->GetActiveCancelWindowCount(), 0);

				// 4.4 不匹配的 AnimInstance 拒收
				UAnimInstance* DummyAnimInstance = NewObject<UAnimInstance>(Player->GetMesh());
				const FGameplayEventData WrongAnimEvent = FManagedMontageTestHelpers::MakeCancelWindowEventData(
					TagCancelBegin, DummyAnimInstance, BoundID, Player, TestMontage, NotifyA, false);
				ASC->HandleGameplayEvent(TagCancelBegin, &WrongAnimEvent);
				TestEqual(TEXT("4.4 Wrong AnimInstance rejected"), Task->GetActiveCancelWindowCount(), 0);

				// 4.5 INDEX_NONE MontageInstanceID 拒收
				const FGameplayEventData IndexNoneEvent = FManagedMontageTestHelpers::MakeCancelWindowEventData(
					TagCancelBegin, MockAnimInstance, INDEX_NONE, Player, TestMontage, NotifyA, false);
				ASC->HandleGameplayEvent(TagCancelBegin, &IndexNoneEvent);
				TestEqual(TEXT("4.5 INDEX_NONE MontageInstanceID rejected"), Task->GetActiveCancelWindowCount(), 0);

				// 4.6 不匹配的 MontageInstanceID 拒收
				const FGameplayEventData WrongIDEvent = FManagedMontageTestHelpers::MakeCancelWindowEventData(
					TagCancelBegin, MockAnimInstance, BoundID + 7777, Player, TestMontage, NotifyA, false);
				ASC->HandleGameplayEvent(TagCancelBegin, &WrongIDEvent);
				TestEqual(TEXT("4.6 Mismatched MontageInstanceID rejected"), Task->GetActiveCancelWindowCount(), 0);

				// 4.7 错误 Avatar 拒收
				APlayerCharacter* OtherPlayer = FCombatAutomationFixture::SpawnPlayer(World, FTransform::Identity);
				const FGameplayEventData WrongAvatarEvent = FManagedMontageTestHelpers::MakeCancelWindowEventData(
					TagCancelBegin, MockAnimInstance, BoundID, OtherPlayer, TestMontage, NotifyA, false);
				ASC->HandleGameplayEvent(TagCancelBegin, &WrongAvatarEvent);
				TestEqual(TEXT("4.7 Wrong Avatar rejected"), Task->GetActiveCancelWindowCount(), 0);

				// 4.8 错误 Montage 拒收
				UAnimMontage* WrongMontage = NewObject<UAnimMontage>(GetTransientPackage(), TEXT("WrongMontage"));
				const FGameplayEventData WrongMontageEvent = FManagedMontageTestHelpers::MakeCancelWindowEventData(
					TagCancelBegin, MockAnimInstance, BoundID, Player, WrongMontage, NotifyA, false);
				ASC->HandleGameplayEvent(TagCancelBegin, &WrongMontageEvent);
				TestEqual(TEXT("4.8 Wrong Montage rejected"), Task->GetActiveCancelWindowCount(), 0);

				// Pair each rejected payload with a valid event on the same live binding.
				Task->SetTestBypassMontageActiveCheck(false);
				FGameplayEventData ValidBegin = FManagedMontageTestHelpers::MakeCancelWindowEventData(
					TagCancelBegin, MockAnimInstance, BoundID, Player, TestMontage, NotifyA);
				FGameplayEventData WrongTag = ValidBegin;
				WrongTag.EventTag = TagCancelEnd;
				Task->TestInvokeCancelBegin(WrongTag);
				TestEqual(TEXT("4.10 Wrong Begin tag rejected"), Task->GetActiveCancelWindowCount(), 0);
				UAnimNotifyState* ForeignNotify = NewObject<UAnimNotifyState_DodgeInvulnerability>(TestMontage);
				TestMontage->Notifies.AddDefaulted_GetRef().NotifyStateClass = ForeignNotify;
				FGameplayEventData WrongType = ValidBegin;
				WrongType.OptionalObject2 = ForeignNotify;
				ASC->HandleGameplayEvent(TagCancelBegin, &WrongType);
				TestEqual(TEXT("4.10 Authored non-cancel Notify rejected"), Task->GetActiveCancelWindowCount(), 0);
				TestMontage->Notifies.Pop();
				ASC->HandleGameplayEvent(TagCancelBegin, &ValidBegin);
				TestEqual(TEXT("4.10 Valid identity accepted on the same binding"), Task->GetActiveCancelWindowCount(), 1);
				Task->TestInvokeCancelEnd(ValidBegin);
				TestEqual(TEXT("4.10 Wrong End tag cannot close a valid window"), Task->GetActiveCancelWindowCount(), 1);
				ValidBegin.EventTag = TagCancelEnd;
				ASC->HandleGameplayEvent(TagCancelEnd, &ValidBegin);
				TestEqual(TEXT("4.10 Correct End closes the window"), Task->GetActiveCancelWindowCount(), 0);
			}
			SourceAbility->EndTestAbility();
		}

		// 4.9 同资产重播防御：旧 ID 迟到事件与旧 Task 清理不干扰新实例
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

		UTestManagedMontageAbility* ReplayAbility = Cast<UTestManagedMontageAbility>(ASC->FindAbilitySpecFromHandle(ReplayHandle)->GetPrimaryInstance());
		TestNotNull(TEXT("4.9 ReplayAbility instanced"), ReplayAbility);
		if (ReplayAbility)
		{
			ReplayAbility->TestMontage = TestMontage;
			ReplayAbility->TestCancelPolicy = EActionMontageCancelPolicy::DodgeAndDefense;
			const bool bActivated1 = ASC->TryActivateAbility(ReplayHandle);
			TestTrue(TEXT("4.9 Playback 1 activated"), bActivated1);

			UAbilityTask_PlayActionMontage* Task1 = ReplayAbility->GetMontageTask();
			TestNotNull(TEXT("4.9 Task 1 created"), Task1);
			if (Task1)
			{
				Task1->SetTestBypassMontageActiveCheck(false);
				const int32 ID1 = Task1->GetBoundMontageInstanceID();
				TestNotEqual(TEXT("4.9 Instance 1 has valid ID"), ID1, (int32)INDEX_NONE);

				// 解除 Task 1 的外部委托以防提前结束
				Task1->OnCompleted.Clear();
				Task1->OnInterrupted.Clear();
				Task1->OnCancelled.Clear();

				// 重播产生 Task 2 (ID2)
				UAbilityTask_PlayActionMontage* Task2 = ReplayAbility->PlayActionMontage(
					TestMontage, 1.0f, NAME_None, 1.0f, 0.0f, false, EActionMontageCancelPolicy::DodgeAndDefense);
				TestNotNull(TEXT("4.9 Task 2 created"), Task2);
				if (Task2)
				{
					Task2->SetTestBypassMontageActiveCheck(false);
					const int32 ID2 = Task2->GetBoundMontageInstanceID();
					TestNotEqual(TEXT("4.9 Instance 2 has valid ID"), ID2, (int32)INDEX_NONE);
					TestTrue(TEXT("4.9 Replay generates different instance ID"), ID1 != ID2);

					// Task 2 接收合法 Begin 事件 (ID2)
					const FGameplayEventData ValidBegin2 = FManagedMontageTestHelpers::MakeCancelWindowEventData(
						TagCancelBegin, MockAnimInstance, ID2, Player, TestMontage, NotifyA, false);
					ASC->HandleGameplayEvent(TagCancelBegin, &ValidBegin2);
					TestEqual(TEXT("4.9 Task 2 has 1 active cancel window"), Task2->GetActiveCancelWindowCount(), 1);
					TestTrue(TEXT("4.9 CanCancel.Dodge active on instance 2"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));

					// 注入携带旧 ID1 的迟到 End 事件
					const FGameplayEventData StaleEnd1 = FManagedMontageTestHelpers::MakeCancelWindowEventData(
						TagCancelEnd, MockAnimInstance, ID1, Player, TestMontage, NotifyA, true);
					ASC->HandleGameplayEvent(TagCancelEnd, &StaleEnd1);

					// 断言：Task 2 坚决拒绝旧 ID1 事件
					TestEqual(TEXT("4.9 Stale ID1 cannot close instance 2 window"), Task2->GetActiveCancelWindowCount(), 1);
					TestTrue(TEXT("4.9 CanCancel.Dodge remains active on instance 2"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));

					// 清理旧 Task 1
					Task1->EndTask();

					// 断言：旧 Task 1 清理绝不关闭新实例 2 的窗口与标签
					TestEqual(TEXT("4.9 Task 1 cleanup does NOT close Task 2 window"), Task2->GetActiveCancelWindowCount(), 1);
					TestTrue(TEXT("4.9 CanCancel.Dodge preserved after Task 1 cleanup"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));

					// 清理 Task 2
					Task2->EndTask();
					TestEqual(TEXT("4.9 Task 2 cleanup removes tags"), Task2->GetActiveCancelWindowCount(), 0);
					TestFalse(TEXT("4.9 CanCancel.Dodge removed after Task 2 cleanup"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
				}
			}
			ReplayAbility->EndTestAbility();
		}
	}

	// =========================================================================
	// 5. 重入安全与外部标签所有权 (Reentrancy & External Tag Ownership)
	// =========================================================================
	{
		// 5.1 外部标签保护：Task 退出或清理绝不得删除外部拥有的 Tag
		ASC->AddLooseGameplayTag(TagCanCancelDodge);
		TestTrue(TEXT("5.1 External CanCancel.Dodge added loosely to ASC"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));

		const FGameplayAbilitySpecHandle ExtHandle = ASC->GiveAbility(
			FGameplayAbilitySpec(UTestManagedMontageAbility::StaticClass(), 1, INDEX_NONE, Player));
		ON_SCOPE_EXIT
		{
			if (IsValid(ASC))
			{
				ASC->CancelAbilityHandle(ExtHandle);
				ASC->ClearAbility(ExtHandle);
			}
		};

		UTestManagedMontageAbility* ExtAbility = Cast<UTestManagedMontageAbility>(ASC->FindAbilitySpecFromHandle(ExtHandle)->GetPrimaryInstance());
		TestNotNull(TEXT("5.1 ExtAbility instanced"), ExtAbility);
		if (ExtAbility)
		{
			ExtAbility->TestMontage = TestMontage;
			ExtAbility->TestCancelPolicy = EActionMontageCancelPolicy::DodgeAndDefense;
			const bool bActivated = ASC->TryActivateAbility(ExtHandle);
			TestTrue(TEXT("5.1 ExtAbility activated"), bActivated);

			UAbilityTask_PlayActionMontage* Task = ExtAbility->GetMontageTask();
			TestNotNull(TEXT("5.1 Task created"), Task);
			if (Task)
			{
				Task->SetTestBypassMontageActiveCheck(true);
				const int32 BoundID = Task->GetBoundMontageInstanceID();

				const FGameplayEventData BeginA = FManagedMontageTestHelpers::MakeCancelWindowEventData(
					TagCancelBegin, MockAnimInstance, BoundID, Player, TestMontage, NotifyA, false);
				const FGameplayEventData EndA = FManagedMontageTestHelpers::MakeCancelWindowEventData(
					TagCancelEnd, MockAnimInstance, BoundID, Player, TestMontage, NotifyA, true);

				// 窗口开启：Task 贡献 1 次 Dodge 与 1 次 Defense
				ASC->HandleGameplayEvent(TagCancelBegin, &BeginA);
				TestTrue(TEXT("5.1 CanCancel.Defense granted by Task"), ASC->HasMatchingGameplayTag(TagCanCancelDefense));

				// 窗口关闭：Task 仅撤销自己的 1 次贡献
				ASC->HandleGameplayEvent(TagCancelEnd, &EndA);
				TestFalse(TEXT("5.1 CanCancel.Defense removed by Task"), ASC->HasMatchingGameplayTag(TagCanCancelDefense));
				// 外部拥有的 Dodge 标签必须仍然存在！
				TestTrue(TEXT("5.1 External CanCancel.Dodge PRESERVED after Task window ends!"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));

				// 再次开启窗口，并在开启状态下调用 EndTask (模拟打断/清理)
				ASC->HandleGameplayEvent(TagCancelBegin, &BeginA);
				Task->EndTask();

				// Task 清理后，外部标签依然存在！
				TestTrue(TEXT("5.1 External CanCancel.Dodge PRESERVED after Task->EndTask!"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
				TestFalse(TEXT("5.1 CanCancel.Defense cleared after Task->EndTask"), ASC->HasMatchingGameplayTag(TagCanCancelDefense));
			}
			ExtAbility->EndTestAbility();
		}

		ASC->RemoveLooseGameplayTag(TagCanCancelDodge);
		TestFalse(TEXT("5.1 External CanCancel.Dodge removed after test"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
	}

	// 5.2 Synchronous tag callbacks: destroy on add/remove, or close while the task stays active.
	for (int32 Scenario = 0; Scenario < 3; ++Scenario)
	{
		const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(
			FGameplayAbilitySpec(UTestManagedMontageAbility::StaticClass(), 1, INDEX_NONE, Player));
		ON_SCOPE_EXIT { ASC->CancelAbilityHandle(Handle); ASC->ClearAbility(Handle); };
		auto* Ability = Cast<UTestManagedMontageAbility>(ASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance());
		if (!TestNotNull(TEXT("5.2 Reentrant source exists"), Ability)) return false;
		Ability->TestMontage = TestMontage;
		Ability->TestCancelPolicy = EActionMontageCancelPolicy::DodgeAndDefense;
		ASC->TryActivateAbility(Handle);
		auto* Task = Ability->GetMontageTask();
		if (!TestNotNull(TEXT("5.2 Live task exists"), Task)) return false;
		const FGameplayEventData Begin = FManagedMontageTestHelpers::MakeCancelWindowEventData(
			TagCancelBegin, MockAnimInstance, Task->GetBoundMontageInstanceID(), Player, TestMontage, NotifyA);
		const FGameplayEventData End = FManagedMontageTestHelpers::MakeCancelWindowEventData(
			TagCancelEnd, MockAnimInstance, Task->GetBoundMontageInstanceID(), Player, TestMontage, NotifyA, true);
		bool bReentered = false;
		const FDelegateHandle Callback = ASC->RegisterGameplayTagEvent(TagCanCancelDodge).AddLambda(
			[&](const FGameplayTag, int32 Count)
			{
				if (bReentered || (Scenario == 2 ? Count != 0 : Count == 0)) return;
				bReentered = true;
				if (Scenario == 1) ASC->HandleGameplayEvent(TagCancelEnd, &End);
				else ASC->CancelAbilityHandle(Handle);
			});
		ON_SCOPE_EXIT { ASC->RegisterGameplayTagEvent(TagCanCancelDodge).Remove(Callback); };
		ASC->HandleGameplayEvent(TagCancelBegin, &Begin);
		if (Scenario == 2) ASC->HandleGameplayEvent(TagCancelEnd, &End);
		TestTrue(TEXT("5.2 Tag callback actually reentered"), bReentered);
		TestEqual(TEXT("5.2 No active window remains"), Task->GetActiveCancelWindowCount(), 0);
		TestFalse(TEXT("5.2 No Dodge permission leak"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
		TestFalse(TEXT("5.2 No Defense permission leak"), ASC->HasMatchingGameplayTag(TagCanCancelDefense));
		TestEqual(TEXT("5.2 Only the close-window scenario keeps the source active"), Ability->IsActive(), Scenario == 1);
		Task->EndTask();
		Task->EndTask();
		TestFalse(TEXT("5.2 Repeated cleanup stays clean"), ASC->HasMatchingGameplayTag(TagCanCancelDefense));
	}

	// =========================================================================
	// 6. 配置诊断 (Configuration Diagnostics)
	// =========================================================================
	{
		auto* Source = NewObject<UChargedAttackAbility>(Player);
		// UE only assigns an AbilityTask's owner when the ability has ActorInfo.
		Source->SetTestCurrentActorInfo(ASC->AbilityActorInfo.Get());
		auto* Target = NewObject<UDodgeAbility>(Player);
		const TArray<const UGameplayAbility*> Targets = { Target,
			GetDefault<UPlayerGuardAbility>(), GetDefault<UPlayerParryAbility>() };
		auto* Task = UAbilityTask_PlayActionMontage::PlayActionMontage(Source, NAME_None, TestMontage,
			1.0f, NAME_None, 1.0f, 0.0f, false, EActionMontageCancelPolicy::DodgeAndDefense);
		FString Reason;
		const auto Validate = [&]()
		{
			return FManagedMontageTestHelpers::ValidateCancelWindowConfiguration(
				Source, TestMontage, Task, EActionMontageCancelPolicy::DodgeAndDefense, Targets, Reason);
		};
		TestTrue(TEXT("6.1 Complete source/Task/montage/targets pass"), Validate());
		TestFalse(TEXT("6.2 Missing task fails even with a correct ability CDO"),
			FManagedMontageTestHelpers::ValidateCancelWindowConfiguration(Source, TestMontage, nullptr,
				EActionMontageCancelPolicy::DodgeAndDefense, Targets, Reason));
		TestTrue(TEXT("6.2 Reason identifies missing Task"), Reason.Contains(TEXT("missing standard")));
		Task->SetTestCancelPolicy(EActionMontageCancelPolicy::None);
		TestFalse(TEXT("6.3 Actual task policy mismatch fails"), Validate());
		TestTrue(TEXT("6.3 Reason identifies mismatch"), Reason.Contains(TEXT("mismatch")));
		Task->SetTestCancelPolicy(EActionMontageCancelPolicy::DodgeAndDefense);

		Source->AbilityTags.RemoveTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Action.CancelableBy.Defense")));
		TestFalse(TEXT("6.4 Missing source marker fails"), Validate());
		TestTrue(TEXT("6.4 Reason identifies missing marker"), Reason.Contains(TEXT("missing Ability.Action.CancelableBy")));
		Source->AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Action.CancelableBy.Defense")));

		const TArray<FAnimNotifyEvent> SavedNotifies = TestMontage->Notifies;
		TestMontage->Notifies.Reset();
		TestFalse(TEXT("6.5 Missing authored window fails"), Validate());
		auto* SegmentAnimation = Cast<UAnimSequenceBase>(TestMontage->SlotAnimTracks[0].AnimTrack.AnimSegments[0].GetAnimReference());
		if (!TestNotNull(TEXT("6.5 Direct animation exists"), SegmentAnimation)) return false;
		const TArray<FAnimNotifyEvent> SavedSegmentNotifies = SegmentAnimation->Notifies;
		SegmentAnimation->Notifies.AddDefaulted_GetRef().NotifyStateClass = NewObject<UAnimNotifyState_ActionDodgeCancelWindow>(SegmentAnimation);
		TestTrue(TEXT("6.5 Window on a direct animation segment passes"), Validate());
		SegmentAnimation->Notifies = SavedSegmentNotifies;
		TestMontage->Notifies = SavedNotifies;
		TestFalse(TEXT("6.6 Explicit None still requires the standard entry"),
			FManagedMontageTestHelpers::ValidateCancelWindowConfiguration(Source, nullptr, nullptr,
				EActionMontageCancelPolicy::None, {}, Reason));

		// Mutate only transient fixture instances, never global CDOs.
		for (const FName Field : { FName(TEXT("BlockAbilitiesWithTag")), FName(TEXT("ActivationBlockedTags")), FName(TEXT("CancelAbilitiesWithTag")) })
		{
			const bool bSourceField = Field == TEXT("BlockAbilitiesWithTag");
			UGameplayAbility* Owner = bSourceField ? static_cast<UGameplayAbility*>(Source) : Target;
			FStructProperty* Property = FindFProperty<FStructProperty>(UGameplayAbility::StaticClass(), Field);
			if (!TestNotNull(TEXT("6.7 Reflected tag property exists"), Property)) return false;
			FGameplayTagContainer* Tags = Property->ContainerPtrToValuePtr<FGameplayTagContainer>(Owner);
			const FGameplayTagContainer SavedTags = *Tags;
			const FGameplayTag Conflict = FGameplayTag::RequestGameplayTag(bSourceField ? TEXT("Ability.Dodge")
				: Field == TEXT("ActivationBlockedTags") ? TEXT("State.Action.Attacking") : TEXT("Ability.Attack.Charged"));
			Tags->AddTag(Conflict);
			TestFalse(*FString::Printf(TEXT("6.7 %s conflict rejected"), *Field.ToString()), Validate());
			TestTrue(TEXT("6.7 Diagnostic names target and conflict"), Reason.Contains(TEXT("target")) && Reason.Contains(Field.ToString()));
			*Tags = SavedTags;
			TestTrue(TEXT("6.7 Corrected configuration passes again"), Validate());
		}
		Task->EndTask();
	}

	// =========================================================================
	// 7. GAS 正向与负向取消集成 (GAS Dodge Cancellation Integration)
	// =========================================================================
	{
		// 7.1 正向取消：窗口内 Dodge 成功激活并实际取消源动作
		const FGameplayAbilitySpecHandle SprintHandle = ASC->GiveAbility(
			FGameplayAbilitySpec(USprintAttackAbility::StaticClass(), 1, INDEX_NONE, Player));
		const FGameplayAbilitySpecHandle DodgeHandle = ASC->GiveAbility(
			FGameplayAbilitySpec(UDodgeAbility::StaticClass(), 1, INDEX_NONE, Player));
		ON_SCOPE_EXIT
		{
			if (IsValid(ASC))
			{
				ASC->CancelAbilityHandle(SprintHandle);
				ASC->ClearAbility(SprintHandle);
				ASC->CancelAbilityHandle(DodgeHandle);
				ASC->ClearAbility(DodgeHandle);
			}
		};

		USprintAttackAbility* SprintAbility = Cast<USprintAttackAbility>(ASC->FindAbilitySpecFromHandle(SprintHandle)->GetPrimaryInstance());
		UDodgeAbility* DodgeAbility = Cast<UDodgeAbility>(ASC->FindAbilitySpecFromHandle(DodgeHandle)->GetPrimaryInstance());
		TestNotNull(TEXT("7.1 SprintAbility instanced"), SprintAbility);
		TestNotNull(TEXT("7.1 DodgeAbility instanced"), DodgeAbility);

		if (SprintAbility && DodgeAbility)
		{
			Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 100.0f);
			ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxStaminaAttribute(), 100.0f);

			// 配置 SprintAbility 运行必需的前置属性与标签
			SetFixtureObject(SprintAbility, TEXT("SprintAttackMontage"), TestMontage);
			SetFixtureObject(SprintAbility, TEXT("CostGameplayEffectClass"), UGameplayEffect::StaticClass());
			SetFixtureObject(SprintAbility, TEXT("DamageGameplayEffectClass"), UGameplayEffect::StaticClass());
			SetFixtureObject(SprintAbility, TEXT("StaminaRegenDelayGameplayEffectClass"), UGameplayEffect::StaticClass());
			const FGameplayTag TagSprinting = FGameplayTag::RequestGameplayTag(TEXT("State.Movement.Sprinting"));
			ASC->AddLooseGameplayTag(TagSprinting);
			Player->SetTestCurrentMoveInput(FVector2D(0.0f, 1.0f));

			// 配置 DodgeAbility 运行必需的前置属性
			SetFixtureObject(DodgeAbility, TEXT("DodgeMontage"), TestMontage);
			SetFixtureObject(DodgeAbility, TEXT("CostGameplayEffectClass"), UGameplayEffect::StaticClass());
			SetFixtureObject(DodgeAbility, TEXT("StaminaRegenDelayGameplayEffectClass"), UGameplayEffect::StaticClass());
			SetFixtureObject(DodgeAbility, TEXT("InvulnerabilityGameplayEffectClass"), UGameplayEffect::StaticClass());

			// 激活 SprintAttack
			const bool bSprintActivated = ASC->TryActivateAbility(SprintHandle);
			TestTrue(TEXT("7.1 SprintAttack activated"), bSprintActivated);
			TestTrue(TEXT("7.1 SprintAttack is active"), SprintAbility->IsActive());
			TestEqual(TEXT("7.1 Sprint starts at time zero"), MockAnimInstance->Montage_GetPosition(TestMontage), 0.0f);
			TestEqual(TEXT("7.1 Sprint root motion scale is preserved"), Player->GetAnimRootMotionTranslationScale(), 1.0f);
			FString Diagnostic;
			TestTrue(TEXT("7.1 Production Sprint Task passes configuration validation"),
				FManagedMontageTestHelpers::ValidateCancelWindowConfiguration(SprintAbility, TestMontage,
					SprintAbility->GetTestMontageTask(), EActionMontageCancelPolicy::DodgeAndDefense,
					{ DodgeAbility, GetDefault<UPlayerGuardAbility>(), GetDefault<UPlayerParryAbility>() }, Diagnostic));

			// 窗口外：Dodge CanActivate 必须被拒绝（因为缺乏 State.Action.CanCancel.Dodge）
			TestFalse(TEXT("7.1 Dodge CanActivate fails outside cancel window"),
				DodgeAbility->CanActivateAbility(DodgeHandle, ASC->AbilityActorInfo.Get()));

			// Real animation dispatch must open the production task's window.
			FCombatAutomationFixture::AdvanceWorld(World, 0.12f);
			TestTrue(TEXT("7.1 Authored Notify grants Dodge permission"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
			TestTrue(TEXT("7.1 Dodge CanActivate succeeds inside cancel window"),
				DodgeAbility->CanActivateAbility(DodgeHandle, ASC->AbilityActorInfo.Get()));

			// 激活 Dodge：Dodge 提交成功并取消带有 Ability.Action.CancelableBy.Dodge 的 SprintAttack
			const bool bDodgeActivated = ASC->TryActivateAbility(DodgeHandle);
			TestTrue(TEXT("7.1 Dodge activated successfully"), bDodgeActivated);

			// 断言：SprintAttack 被实际取消！
			TestFalse(TEXT("7.1 SprintAttack was CANCELLED by Dodge!"), SprintAbility->IsActive());
			TestTrue(TEXT("7.1 Dodge is now active"), DodgeAbility->IsActive());

			Player->SetTestCurrentMoveInput(FVector2D::ZeroVector);
			TestFalse(TEXT("7.1 Source task removes its permission after cancellation"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
			ASC->RemoveLooseGameplayTag(TagSprinting);
			DodgeAbility->EndAbility(DodgeHandle, ASC->AbilityActorInfo.Get(), DodgeAbility->GetCurrentActivationInfo(), true, false);
		}

		// 7.2 负向取消：CommitCheck 失败时 Dodge 激活失败，源动作与取消窗口保持运行
		const FGameplayAbilitySpecHandle SprintHandle2 = ASC->GiveAbility(
			FGameplayAbilitySpec(USprintAttackAbility::StaticClass(), 1, INDEX_NONE, Player));
		const FGameplayAbilitySpecHandle FailingDodgeHandle = ASC->GiveAbility(
			FGameplayAbilitySpec(UTestCommitFailingDodgeAbility::StaticClass(), 1, INDEX_NONE, Player));
		ON_SCOPE_EXIT
		{
			if (IsValid(ASC))
			{
				ASC->CancelAbilityHandle(SprintHandle2);
				ASC->ClearAbility(SprintHandle2);
				ASC->CancelAbilityHandle(FailingDodgeHandle);
				ASC->ClearAbility(FailingDodgeHandle);
			}
		};

		USprintAttackAbility* SprintAbility2 = Cast<USprintAttackAbility>(ASC->FindAbilitySpecFromHandle(SprintHandle2)->GetPrimaryInstance());
		UTestCommitFailingDodgeAbility* FailingDodge = Cast<UTestCommitFailingDodgeAbility>(ASC->FindAbilitySpecFromHandle(FailingDodgeHandle)->GetPrimaryInstance());
		TestNotNull(TEXT("7.2 SprintAbility2 instanced"), SprintAbility2);
		TestNotNull(TEXT("7.2 FailingDodge instanced"), FailingDodge);

		if (SprintAbility2 && FailingDodge)
		{
			Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 100.0f);
			ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxStaminaAttribute(), 100.0f);

			// 配置 SprintAbility2 运行必需的前置属性与标签
			SetFixtureObject(SprintAbility2, TEXT("SprintAttackMontage"), TestMontage);
			SetFixtureObject(SprintAbility2, TEXT("CostGameplayEffectClass"), UGameplayEffect::StaticClass());
			SetFixtureObject(SprintAbility2, TEXT("DamageGameplayEffectClass"), UGameplayEffect::StaticClass());
			SetFixtureObject(SprintAbility2, TEXT("StaminaRegenDelayGameplayEffectClass"), UGameplayEffect::StaticClass());
			const FGameplayTag TagSprinting = FGameplayTag::RequestGameplayTag(TEXT("State.Movement.Sprinting"));
			ASC->AddLooseGameplayTag(TagSprinting);
			Player->SetTestCurrentMoveInput(FVector2D(0.0f, 1.0f));

			// 配置 FailingDodge 运行必需的前置属性
			SetFixtureObject(FailingDodge, TEXT("DodgeMontage"), TestMontage);
			SetFixtureObject(FailingDodge, TEXT("CostGameplayEffectClass"), UGameplayEffect::StaticClass());
			SetFixtureObject(FailingDodge, TEXT("StaminaRegenDelayGameplayEffectClass"), UGameplayEffect::StaticClass());
			SetFixtureObject(FailingDodge, TEXT("InvulnerabilityGameplayEffectClass"), UGameplayEffect::StaticClass());

			const bool bSprintActivated2 = ASC->TryActivateAbility(SprintHandle2);
			TestTrue(TEXT("7.2 SprintAttack2 activated"), bSprintActivated2);

			FCombatAutomationFixture::AdvanceWorld(World, 0.12f);
			TestTrue(TEXT("7.2 Authored Notify grants Dodge permission"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
			TestTrue(TEXT("7.2 FailingDodge CanActivate passes (window is open)"),
				FailingDodge->CanActivateAbility(FailingDodgeHandle, ASC->AbilityActorInfo.Get()));

			// 尝试激活 FailingDodge：CanActivate 通过，但 CommitCheck 返回 false，导致其无法真正保持激活
			ASC->TryActivateAbility(FailingDodgeHandle);
			TestEqual(TEXT("7.2 Production Dodge reached CommitCheck"), FailingDodge->CommitCheckCallCount, 1);
			TestFalse(TEXT("7.2 FailingDodge ends immediately due to CommitCheck failure"), FailingDodge->IsActive());

			// 核心断言：源动作绝对没有被取消，依然活跃！取消窗口依然有效！
			TestTrue(TEXT("7.2 Source SprintAttack2 REMAINS ACTIVE after CommitCheck failure!"), SprintAbility2->IsActive());
			TestTrue(TEXT("7.2 Cancel tag remains active"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));

			Player->SetTestCurrentMoveInput(FVector2D::ZeroVector);
			ASC->RemoveLooseGameplayTag(TagSprinting);
			SprintAbility2->EndAbility(SprintHandle2, ASC->AbilityActorInfo.Get(), SprintAbility2->GetCurrentActivationInfo(), true, false);
		}
	}

	// =========================================================================
	// 8. 真实世界推进与双派发模式端到端验证 (AdvanceWorld Queued & BranchingPoint)
	// =========================================================================
	{
		// TestMontage 配置：
		// - CancelNotifyA: Queued (0.10s ~ 0.20s)
		// - CancelNotifyB: BranchingPoint (0.15s ~ 0.30s) [0.15s~0.20s 重叠并集，0.20s~0.30s 单独持续]
		const FGameplayAbilitySpecHandle AdvanceHandle = ASC->GiveAbility(
			FGameplayAbilitySpec(UTestManagedMontageAbility::StaticClass(), 1, INDEX_NONE, Player));
		ON_SCOPE_EXIT
		{
			if (IsValid(ASC))
			{
				ASC->CancelAbilityHandle(AdvanceHandle);
				ASC->ClearAbility(AdvanceHandle);
			}
		};

		UTestManagedMontageAbility* AdvanceAbility = Cast<UTestManagedMontageAbility>(ASC->FindAbilitySpecFromHandle(AdvanceHandle)->GetPrimaryInstance());
		TestNotNull(TEXT("8. AdvanceAbility instanced"), AdvanceAbility);
		if (AdvanceAbility)
		{
			AdvanceAbility->TestMontage = TestMontage;
			AdvanceAbility->TestCancelPolicy = EActionMontageCancelPolicy::DodgeAndDefense;

			const bool bActivated = ASC->TryActivateAbility(AdvanceHandle);
			TestTrue(TEXT("8. AdvanceAbility activated"), bActivated);

			UAbilityTask_PlayActionMontage* Task = AdvanceAbility->GetMontageTask();
			TestNotNull(TEXT("8. Task created"), Task);
			if (Task)
			{
				Task->SetTestBypassMontageActiveCheck(false);

				// 初始 (0.0s)：不在窗口内
				TestEqual(TEXT("8.0 Initial active window count is 0"), Task->GetActiveCancelWindowCount(), 0);
				TestFalse(TEXT("8.0 Initial CanCancel.Dodge absent"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
				TestFalse(TEXT("8.0 Initial CanCancel.Defense absent"), ASC->HasMatchingGameplayTag(TagCanCancelDefense));

				// 推进 0.12s：到达 Queued NotifyA 内部 (0.10s~0.20s)
				FCombatAutomationFixture::AdvanceWorld(World, 0.12f);
				TestEqual(TEXT("8.1 Queued NotifyA active, count is 1"), Task->GetActiveCancelWindowCount(), 1);
				TestTrue(TEXT("8.1 Queued NotifyA grants CanCancel.Dodge"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
				TestTrue(TEXT("8.1 Queued NotifyA grants CanCancel.Defense"), ASC->HasMatchingGameplayTag(TagCanCancelDefense));

				// 推进 0.05s (累积 0.17s)：进入 BranchingPoint NotifyB (0.15s~0.30s)，处于 A & B 重叠并集区间！
				FCombatAutomationFixture::AdvanceWorld(World, 0.05f);
				TestEqual(TEXT("8.2 Overlapping union active (A & B), count is 2"), Task->GetActiveCancelWindowCount(), 2);
				TestTrue(TEXT("8.2 CanCancel.Dodge remains active during overlap"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
				TestTrue(TEXT("8.2 CanCancel.Defense remains active during overlap"), ASC->HasMatchingGameplayTag(TagCanCancelDefense));

				// 推进 0.07s (累积 0.24s)：穿过 Queued NotifyA 结束点 (0.20s)，仅剩下 BranchingPoint NotifyB (持续至 0.30s)！
				FCombatAutomationFixture::AdvanceWorld(World, 0.07f);
				TestEqual(TEXT("8.3 NotifyA ended, NotifyB still active, count is 1"), Task->GetActiveCancelWindowCount(), 1);
				TestTrue(TEXT("8.3 Overlapping union preserved: CanCancel.Dodge STILL ACTIVE after NotifyA ends!"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
				TestTrue(TEXT("8.3 Overlapping union preserved: CanCancel.Defense STILL ACTIVE after NotifyA ends!"), ASC->HasMatchingGameplayTag(TagCanCancelDefense));

				// 推进 0.12s (累积 0.36s)：穿过 BranchingPoint NotifyB 结束点 (0.30s)，所有窗口全部自然关闭！
				FCombatAutomationFixture::AdvanceWorld(World, 0.12f);
				TestEqual(TEXT("8.4 Both windows ended naturally, count is 0"), Task->GetActiveCancelWindowCount(), 0);
				TestFalse(TEXT("8.4 CanCancel.Dodge removed after all windows closed"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
				TestFalse(TEXT("8.4 CanCancel.Defense removed after all windows closed"), ASC->HasMatchingGameplayTag(TagCanCancelDefense));

				// 推进到自然播放完成 (总长 1.0s，累积推进 1.06s)
				FCombatAutomationFixture::AdvanceWorld(World, 0.70f);
				TestEqual(TEXT("8.5 CompletedCallCount is 1"), AdvanceAbility->CompletedCallCount, 1);
				TestFalse(TEXT("8.5 Ability ended cleanly"), AdvanceAbility->IsActive());
				TestTrue(TEXT("8.5 Task terminated cleanly"), Task->IsTerminated());
			}
		}
	}

	// 9. Production Charged activation and authored HoldReady, with real queued/branching dispatch.
	const FAnimNotifyEvent WindowTemplate = TestMontage->Notifies[0];
	const TArray<FSlotAnimationTrack> OriginalTracks = TestMontage->SlotAnimTracks;
	UAnimSequenceBase* SegmentAnimation = OriginalTracks[0].AnimTrack.AnimSegments[0].GetAnimReference();
	if (!TestNotNull(TEXT("9. Source sequence exists"), SegmentAnimation)) return false;
	const TArray<FAnimNotifyEvent> OriginalSegmentNotifies = SegmentAnimation->Notifies;
	const float OriginalLength = TestMontage->GetPlayLength();
	FControlledMontageAdvance ControlledAdvance(Player, MockAnimInstance);
	FAnimInstanceProxy& MockAnimProxy = FAnimInstanceProxyAccess::GetProxy(MockAnimInstance);
	const FName DefaultSlotName(TEXT("DefaultSlot"));
	const auto AdvanceMontage = [&](float Seconds) { ControlledAdvance.Advance(Seconds); };
	for (int32 SourceCase = 0; SourceCase < 5; ++SourceCase)
	for (int32 Scenario = 0; Scenario < 3; ++Scenario)
	{
		const bool bOnSegment = SourceCase == 2 || SourceCase == 3;
		const bool bReverseSegment = SourceCase == 3;
		const bool bShortSibling = SourceCase == 4;
		const EMontageNotifyTickType::Type Mode = SourceCase == 1 ? EMontageNotifyTickType::BranchingPoint : EMontageNotifyTickType::Queued;
		const bool bAtEnd = Scenario == 1;
		const bool bCancelOnRelease = Scenario == 2;
		const TCHAR* SourceName = bOnSegment ? (bReverseSegment ? TEXT("Queued reverse segment") : TEXT("Queued forward segment"))
			: bShortSibling ? TEXT("Queued sibling window") : Mode == EMontageNotifyTickType::Queued ? TEXT("Queued") : TEXT("Branching");
		const FString Case = FString::Printf(TEXT("Charged %s %s"), SourceName,
			bAtEnd ? TEXT("endpoint") : bCancelOnRelease ? TEXT("release reentry") : TEXT("inside"));
		TestMontage->SlotAnimTracks = OriginalTracks;
		SegmentAnimation->Notifies = OriginalSegmentNotifies;
		UTestMontageLengthAccess::SetLength(TestMontage, bOnSegment ? 0.80f : OriginalLength);
		TestMontage->Notifies = { WindowTemplate };
		UAnimSequenceBase* CancelAnimation = TestMontage;
		UAnimNotifyState_ActionDodgeCancelWindow* ActiveWindowNotify = NotifyA;
		if (bOnSegment)
		{
			// Track [0.40, 0.70] maps to clip [0.20, 0.80] at +/-2x. Window [0.30, 0.70] maps to [0.45, 0.65].
			FAnimSegment& Segment = TestMontage->SlotAnimTracks[0].AnimTrack.AnimSegments[0];
			Segment.StartPos = 0.40f;
			Segment.AnimStartTime = 0.20f;
			Segment.AnimEndTime = 0.80f;
			Segment.AnimPlayRate = bReverseSegment ? -2.0f : 2.0f;
			CancelAnimation = SegmentAnimation;
			TestMontage->Notifies.Reset();
			SegmentAnimation->Notifies.AddDefaulted();
			ActiveWindowNotify = NewObject<UAnimNotifyState_ActionDodgeCancelWindow>(SegmentAnimation);
#if WITH_EDITORONLY_DATA
			if (SegmentAnimation->AnimNotifyTracks.IsEmpty())
			{
				SegmentAnimation->AnimNotifyTracks.Add(FAnimNotifyTrack(FName(TEXT("CancelWindow")), FLinearColor::White));
			}
#endif
		}
		FAnimNotifyEvent& Window = CancelAnimation->Notifies.Last();
		Window.NotifyStateClass = ActiveWindowNotify;
		Window.MontageTickType = Mode;
		Window.Link(CancelAnimation, bOnSegment ? 0.30f : 0.10f);
		Window.SetTime(bOnSegment ? 0.30f : 0.10f);
		Window.SetDuration(bOnSegment ? 0.40f : 0.20f);
		Window.EndLink.Link(CancelAnimation, bOnSegment ? 0.70f : 0.30f);
		Window.EndLink.SetTime(bOnSegment ? 0.70f : 0.30f);
		if (bShortSibling)
		{
			FAnimNotifyEvent& ShortWindow = TestMontage->Notifies.AddDefaulted_GetRef();
			ShortWindow.NotifyStateClass = NewObject<UAnimNotifyState_ActionDodgeCancelWindow>(TestMontage);
			ShortWindow.MontageTickType = EMontageNotifyTickType::Queued;
			ShortWindow.Link(TestMontage, 0.10f);
			ShortWindow.SetTime(0.10f);
			ShortWindow.SetDuration(0.07f);
			ShortWindow.EndLink.Link(TestMontage, 0.17f);
			ShortWindow.EndLink.SetTime(0.17f);
		}
		const float WindowTrackEnd = bOnSegment ? 0.65f : 0.30f;
		const float HoldTime = bAtEnd ? WindowTrackEnd : bOnSegment ? 0.53f : 0.17f;
		FAnimNotifyEvent& Hold = TestMontage->Notifies.AddDefaulted_GetRef();
		Hold.Notify = NewObject<UAnimNotify_PlayerChargedAttackHoldReady>(TestMontage);
		Hold.NotifyName = TEXT("HoldReady");
		Hold.MontageTickType = EMontageNotifyTickType::Queued;
		Hold.Link(TestMontage, HoldTime);
		Hold.SetTime(HoldTime);
		// Clip notification times differ from the sampled montage time; cache both assets after authoring.
		SegmentAnimation->RefreshCacheData();
		TestMontage->RefreshCacheData();

		Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxStaminaAttribute(), 100.0f);
		ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 100.0f);
		const FGameplayTag PrimaryInput = FGameplayTag::RequestGameplayTag(TEXT("Input.PrimaryAttack"));
		Player->TriggerTestHandleCombatInputStarted(PrimaryInput);
		ASC->CancelAllAbilities();
		const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(FGameplayAbilitySpec(UChargedAttackAbility::StaticClass(), 1, INDEX_NONE, Player));
		ON_SCOPE_EXIT
		{
			ASC->CancelAbilityHandle(Handle);
			ASC->ClearAbility(Handle);
			Player->TriggerTestHandleCombatInputEnded(PrimaryInput);
		};
		auto* Charged = Cast<UChargedAttackAbility>(ASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance());
		if (!TestNotNull(*(Case + TEXT(" instance exists")), Charged)) return false;
		SetFixtureObject(Charged, TEXT("ChargedAttackMontage"), TestMontage);
		for (const FName Field : { FName(TEXT("CostGameplayEffectClass")), FName(TEXT("DamageGameplayEffectClass")), FName(TEXT("StaminaRegenDelayGameplayEffectClass")) })
			SetFixtureObject(Charged, Field, UGameplayEffect::StaticClass());
		ASC->TryActivateAbility(Handle);
		if (!TestTrue(*(Case + TEXT(" production ability stays active")), Charged->IsActive())) return false;
		auto* Task = Charged->GetTestMontageTask();
		if (!TestNotNull(*(Case + TEXT(" managed Task exists")), Task)) return false;
		TestEqual(*(Case + TEXT(" starts at time zero")), MockAnimInstance->Montage_GetPosition(TestMontage), 0.0f);
		TestEqual(*(Case + TEXT(" root motion scale")), Player->GetAnimRootMotionTranslationScale(), 1.0f);
		FString Diagnostic;
		TestTrue(*(Case + TEXT(" actual Task configuration")), FManagedMontageTestHelpers::ValidateCancelWindowConfiguration(
			Charged, TestMontage, Task, EActionMontageCancelPolicy::DodgeAndDefense,
			{ GetDefault<UDodgeAbility>(), GetDefault<UPlayerGuardAbility>(), GetDefault<UPlayerParryAbility>() }, Diagnostic));
		const int32 InstanceID = Task->GetBoundMontageInstanceID();
		int32 NaturalEnds = 0;
		int32 EarlyEnds = 0;
		int32 NativeEndFlags = 0;
		int32 BeginReceipts = 0;
		int32 ObservedBegins = 0;
		int32 SourceMatchedBegins = 0;
		int32 LastBeginInstanceID = INDEX_NONE;
		const FDelegateHandle BeginReceipt = ASC->GenericGameplayEventCallbacks.FindOrAdd(TagCancelBegin).AddLambda(
			[&](const FGameplayEventData* Payload)
			{
				if (!Payload || Payload->EventTag != TagCancelBegin) return;
				++ObservedBegins;
				if (Payload->OptionalObject != CancelAnimation || Payload->OptionalObject2 != ActiveWindowNotify) return;
				++SourceMatchedBegins;
				if (!Payload->TargetData.IsValid(0)) return;
				const FGameplayAbilityTargetData* Base = Payload->TargetData.Get(0);
				if (Base->GetScriptStruct() != FGameplayAbilityTargetData_MontageRateWindowSource::StaticStruct()) return;
				const auto* Source = static_cast<const FGameplayAbilityTargetData_MontageRateWindowSource*>(Base);
				LastBeginInstanceID = Source->MontageInstanceID;
				if (Source->MontageInstanceID == InstanceID && Source->AnimInstance.Get() == MockAnimInstance) ++BeginReceipts;
			});
		const FDelegateHandle EndReceipt = ASC->GenericGameplayEventCallbacks.FindOrAdd(TagCancelEnd).AddLambda(
			[&](const FGameplayEventData* Payload)
			{
				if (!Payload || Payload->OptionalObject != CancelAnimation || Payload->OptionalObject2 != ActiveWindowNotify || !Payload->TargetData.IsValid(0)) return;
				const FGameplayAbilityTargetData* Base = Payload->TargetData.Get(0);
				if (Base->GetScriptStruct() != FGameplayAbilityTargetData_MontageRateWindowSource::StaticStruct()) return;
				const auto* Source = static_cast<const FGameplayAbilityTargetData_MontageRateWindowSource*>(Base);
				if (Source->MontageInstanceID != InstanceID) return;
				if (Source->bNativeReachedEnd) ++NativeEndFlags;
				if (Source->bReachedEnd) ++NaturalEnds;
				else ++EarlyEnds;
			});
		ON_SCOPE_EXIT
		{
			ASC->GenericGameplayEventCallbacks.FindOrAdd(TagCancelBegin).Remove(BeginReceipt);
			ASC->GenericGameplayEventCallbacks.FindOrAdd(TagCancelEnd).Remove(EndReceipt);
		};

		AdvanceMontage(HoldTime + (bAtEnd ? 0.02f : 0.05f));
		TestTrue(*(Case + TEXT(" HoldReady keeps ability active")), Charged->IsActive());
		TestFalse(*(Case + TEXT(" authored HoldReady pauses playback")), MockAnimInstance->Montage_IsPlaying(TestMontage));
		const float PausePosition = MockAnimInstance->Montage_GetPosition(TestMontage);
		AdvanceMontage(0.20f);
		TestEqual(*(Case + TEXT(" remains paused while held")), MockAnimInstance->Montage_GetPosition(TestMontage), PausePosition);
		// Branching End is synchronous, before queued HoldReady at the endpoint. Queued End arrives after HoldReady.
		const bool bWindowHeld = !bAtEnd || Mode == EMontageNotifyTickType::Queued;
		TestEqual(*(Case + TEXT(" one source-matched Begin dispatched before pause")), BeginReceipts, 1);
		TestEqual(*(Case + TEXT(" active window set follows pause dispatch order")), Task->GetActiveCancelWindowCount() > 0, bWindowHeld);
		TestEqual(*(Case + TEXT(" paused authorization follows native dispatch order")), ASC->HasMatchingGameplayTag(TagCanCancelDodge), bWindowHeld);
		const int32 ExpectedPending = bShortSibling && !bAtEnd ? 1 : bAtEnd && bWindowHeld ? 1 : 0;
		TestEqual(*(Case + TEXT(" only a natural End is pending")), Task->GetPendingNaturalEndWindowsNum(), ExpectedPending);
		if (bAtEnd) TestTrue(*(Case + TEXT(" verified this window reached its end")), NaturalEnds > 0);
		else if (Mode == EMontageNotifyTickType::Queued) TestTrue(*(Case + TEXT(" verified premature End while paused")), EarlyEnds > 0);
		if (SourceCase == 0 && !bAtEnd)
		{
			TestTrue(*(Case + TEXT(" exercises the shared native end flag")), NativeEndFlags > 0);
			TestEqual(*(Case + TEXT(" shared flag cannot end this interior window")), NaturalEnds, 0);
		}

		bool bReentered = false;
		FDelegateHandle ReleaseCallback;
		const int32 NativeEarlyEndsBeforeRelease = EarlyEnds;
		const int32 NativeNaturalEndsBeforeRelease = NaturalEnds;
		const int32 RawEndFlagsBeforeRelease = NativeEndFlags;
		if (bCancelOnRelease)
		{
			// Fault injection for the release callback boundary; not native end-timing evidence.
			const FGameplayEventData End = FManagedMontageTestHelpers::MakeCancelWindowEventData(
				TagCancelEnd, MockAnimInstance, InstanceID, Player, CancelAnimation, ActiveWindowNotify, true);
			ASC->HandleGameplayEvent(TagCancelEnd, &End);
			ReleaseCallback = ASC->RegisterGameplayTagEvent(TagCanCancelDodge).AddLambda([&](const FGameplayTag, int32 Count)
			{
				if (Count != 0 || bReentered) return;
				bReentered = true;
				ASC->CancelAbilityHandle(Handle);
			});
		}
		ON_SCOPE_EXIT { ASC->RegisterGameplayTagEvent(TagCanCancelDodge).Remove(ReleaseCallback); };
		Player->TriggerTestHandleCombatInputEnded(PrimaryInput);
		if (bCancelOnRelease)
		{
			TestTrue(*(Case + TEXT(" tag removal actually cancelled source")), bReentered);
			TestFalse(*(Case + TEXT(" cancelled ability is not resurrected")), Charged->IsActive());
			TestTrue(*(Case + TEXT(" task terminated")), Task->IsTerminated());
		}
		else
		{
			TestTrue(*(Case + TEXT(" release resumes the same montage")), MockAnimInstance->Montage_IsPlaying(TestMontage));
			TestEqual(*(Case + TEXT(" release retains instance ID")), Charged->GetTestRateWindowMontageInstanceID(), InstanceID);
			if (bAtEnd) TestFalse(*(Case + TEXT(" ended window closes on release")), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
			else TestTrue(*(Case + TEXT(" interior window survives release")), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
			AdvanceMontage(0.25f);
			TestFalse(*(Case + TEXT(" no Dodge permission during later swing")), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
		}
		TestFalse(*(Case + TEXT(" no Defense permission leak")), ASC->HasMatchingGameplayTag(TagCanCancelDefense));
		AddInfo(FString::Printf(TEXT("%s: pause=%.3f, observed/source/accepted-source Begins=%d/%d/%d, instance=%d expected=%d, slot relevant=%d, raw native end flags=%d, verified early Ends=%d, verified natural Ends=%d (before release injection)"),
			*Case, PausePosition, ObservedBegins, SourceMatchedBegins, BeginReceipts, LastBeginInstanceID, InstanceID,
			MockAnimProxy.IsSlotNodeRelevantForNotifies(DefaultSlotName), RawEndFlagsBeforeRelease, NativeEarlyEndsBeforeRelease, NativeNaturalEndsBeforeRelease));
	}
	return true;
#endif // WITH_EDITOR
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FManagedMontageConfigurationOnlyTest,
	"PolyQuest.Combat.ManagedMontageConfigurationOnly", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FManagedMontageConfigurationOnlyTest::RunTest(const FString& Parameters)
{
	using namespace ManagedMontageCancelAutomation;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Configuration world"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	FTestWorldScope WorldScope{World};
	FURL URL; World->InitializeActorsForPlay(URL); World->BeginPlay();
	APlayerCharacter* Player = FCombatAutomationFixture::SpawnPlayer(World);
	if (!TestNotNull(TEXT("Configuration player"), Player)) return false;
	auto* ASC = Player->GetAbilitySystemComponent();
	auto* Mesh = Player->GetMesh();
	auto Setup = CreatePlayableCancelTestMontage(*this, World);
	UAnimMontage* First = Setup.Montage;
	if (!TestNotNull(TEXT("First configured montage"), First)) return false;
	// Author the second montage independently: duplicating an unsaved montage also
	// copies notify links/caches and is not evidence of an independent configuration.
	UAnimMontage* Second = CreatePlayableCancelTestMontage(*this, Player).Montage;
	if (!TestNotNull(TEXT("Second independently configured montage"), Second)) return false;
	UAnimInstance* Anim = NewObject<UAnimInstance>(Mesh);
	Anim->InitializeMontageOnly(); Anim->CurrentSkeleton = First->GetSkeleton();
	UAnimInstance* PreviousAnim = Mesh->AnimScriptInstance;
	Mesh->AnimScriptInstance = Anim; ASC->RefreshAbilityActorInfo();
	FControlledMontageAdvance Advance(Player, Anim);
	ON_SCOPE_EXIT { ASC->CancelAllAbilities(); Mesh->AnimScriptInstance = PreviousAnim; ASC->RefreshAbilityActorInfo(); };
	Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxStaminaAttribute(), 100.0f);
	ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 100.0f);
	const auto SourceHandle = ASC->GiveAbility(FGameplayAbilitySpec(UTestConfigurationOnlyActionAbility::StaticClass(), 1, INDEX_NONE, Player));
	auto* Source = CastChecked<UTestConfigurationOnlyActionAbility>(ASC->FindAbilitySpecFromHandle(SourceHandle)->GetPrimaryInstance());
	const auto DodgeHandle = ASC->GiveAbility(FGameplayAbilitySpec(UDodgeAbility::StaticClass(), 1, INDEX_NONE, Player));
	const auto FailHandle = ASC->GiveAbility(FGameplayAbilitySpec(UTestCommitFailingDodgeAbility::StaticClass(), 1, INDEX_NONE, Player));
	auto* Dodge = CastChecked<UDodgeAbility>(ASC->FindAbilitySpecFromHandle(DodgeHandle)->GetPrimaryInstance());
	auto* FailingDodge = CastChecked<UTestCommitFailingDodgeAbility>(ASC->FindAbilitySpecFromHandle(FailHandle)->GetPrimaryInstance());
	const auto DodgeTag = FGameplayTag::RequestGameplayTag(TEXT("State.Action.CanCancel.Dodge"));
	const auto DefenseTag = FGameplayTag::RequestGameplayTag(TEXT("State.Action.CanCancel.Defense"));
	const auto RateBegin = FGameplayTag::RequestGameplayTag(TEXT("Event.Action.RateWindow.Begin"));
	const auto RateEnd = FGameplayTag::RequestGameplayTag(TEXT("Event.Action.RateWindow.End"));
	const auto CancelBegin = FGameplayTag::RequestGameplayTag(TEXT("Event.Action.CancelWindow.Dodge.Begin"));
	const auto CancelEnd = FGameplayTag::RequestGameplayTag(TEXT("Event.Action.CancelWindow.Dodge.End"));
	FString Reason;
	for (int32 Variant = 0; Variant < 2; ++Variant)
	{
		UAnimMontage* Montage = Variant == 0 ? First : Second;
		Anim->CurrentSkeleton = Montage->GetSkeleton();
		Source->Montage = Montage; // The same action instance changes only its configuration.
		auto* RateNotify = NewObject<UAnimNotifyState_MontageRateWindow>(Montage);
		RateNotify->RateMultiplier = Variant == 0 ? 0.5f : 0.75f;
		FAnimNotifyEvent& RateEvent = Montage->Notifies.AddDefaulted_GetRef();
		RateEvent.NotifyName = TEXT("ConfigurationRate"); RateEvent.NotifyStateClass = RateNotify;
		RateEvent.MontageTickType = Variant == 0 ? EMontageNotifyTickType::Queued : EMontageNotifyTickType::BranchingPoint;
		RateEvent.Link(Montage, 0.05f); RateEvent.SetTime(0.05f); RateEvent.SetDuration(0.30f);
		RateEvent.EndLink.Link(Montage, 0.35f); RateEvent.EndLink.SetTime(0.35f); Montage->RefreshCacheData();
		for (UDodgeAbility* Target : {Dodge, static_cast<UDodgeAbility*>(FailingDodge)})
		{
			if (!SetFixtureObject(Target, TEXT("DodgeMontage"), Montage)
				|| !SetFixtureObject(Target, TEXT("CostGameplayEffectClass"), UGameplayEffect::StaticClass())
				|| !SetFixtureObject(Target, TEXT("StaminaRegenDelayGameplayEffectClass"), UGameplayEffect::StaticClass())
				|| !SetFixtureObject(Target, TEXT("InvulnerabilityGameplayEffectClass"), UGameplayEffect::StaticClass())) return false;
		}
		const auto Activate = [&]() -> UAbilityTask_PlayActionMontage*
		{
			ASC->CancelAbilityHandle(DodgeHandle); ASC->CancelAbilityHandle(FailHandle);
			if (!TestTrue(TEXT("Configuration-only real source activation"), ASC->TryActivateAbility(SourceHandle) && Source->IsActive())) return nullptr;
			auto* Task = Source->GetMontageTask();
			if (!TestNotNull(TEXT("Configuration-only standard Task"), Task)) return nullptr;
			TestTrue(TEXT("Actual Task/configuration passes offline check"),
				FManagedMontageTestHelpers::ValidateCancelWindowConfiguration(Source, Montage, Task, Source->CancelPolicy,
					{Dodge, GetDefault<UPlayerGuardAbility>(), GetDefault<UPlayerParryAbility>()}, Reason));
			TestFalse(TEXT("Positive path does not bypass playback"), Task->GetTestBypassMontageActiveCheck());
			TestEqual(TEXT("Configuration-only starts at zero"), Anim->Montage_GetPosition(Montage), 0.0f);
			TestEqual(TEXT("Configuration-only root motion scale"), Player->GetAnimRootMotionTranslationScale(), 1.0f);
			return Task;
		};
		const auto AdvanceTo = [&](float Position)
		{
			for (int32 Step = 0; Step < 60 && Source->IsActive() && Anim->Montage_GetPosition(Montage) < Position; ++Step) Advance.Advance(0.025f);
			return Source->IsActive() && Anim->Montage_GetPosition(Montage) >= Position;
		};
		const auto CheckClean = [&](UAbilityTask_PlayActionMontage* Task)
		{
			TestTrue(TEXT("Source Task terminated"), Task->IsTerminated());
			TestNull(TEXT("Source releases Task"), Source->GetMontageTask());
			TestFalse(TEXT("Rate binding cleared"), Task->GetRateWindowLifecycle().IsBound());
			TestEqual(TEXT("Cancel windows cleared"), Task->GetActiveCancelWindowCount(), 0);
			TestFalse(TEXT("Dodge contribution cleared"), ASC->HasMatchingGameplayTag(DodgeTag));
			TestFalse(TEXT("Defense contribution cleared"), ASC->HasMatchingGameplayTag(DefenseTag));
		};
		UAbilityTask_PlayActionMontage* Task = Activate();
		if (!Task) return false;
		const int32 InstanceID = Task->GetBoundMontageInstanceID();
		int32 RateBegins = 0, RateEnds = 0, CancelBegins = 0, CancelEnds = 0;
		TArray<TPair<FGameplayTag, FDelegateHandle>> Receipts;
		for (const auto& Pair : {TPair<FGameplayTag, int32*>{RateBegin, &RateBegins}, {RateEnd, &RateEnds}, {CancelBegin, &CancelBegins}, {CancelEnd, &CancelEnds}})
		{
			int32* Count = Pair.Value;
			Receipts.Add({Pair.Key, ASC->GenericGameplayEventCallbacks.FindOrAdd(Pair.Key).AddLambda(
				[&, Count](const FGameplayEventData* Payload)
				{
					if (!Payload || Payload->OptionalObject != Montage || !Payload->OptionalObject2 || Payload->Target != Player
						|| !Payload->TargetData.IsValid(0)) return;
					const auto* Data = Payload->TargetData.Get(0);
					if (Data->GetScriptStruct() != FGameplayAbilityTargetData_MontageRateWindowSource::StaticStruct()) return;
					const auto* Receipt = static_cast<const FGameplayAbilityTargetData_MontageRateWindowSource*>(Data);
					if (Receipt->AnimInstance == Anim && Receipt->MontageInstanceID == InstanceID) ++*Count;
				})});
		}
		ON_SCOPE_EXIT { for (const auto& Receipt : Receipts) ASC->GenericGameplayEventCallbacks.FindOrAdd(Receipt.Key).Remove(Receipt.Value); };
		TestFalse(TEXT("Dodge activation denied outside authored window"), ASC->TryActivateAbility(DodgeHandle));
		TestTrue(TEXT("Outside rejection preserves source"), Source->IsActive() && Source->GetMontageTask() == Task);
		if (!TestTrue(TEXT("Native advance reaches window interior"), AdvanceTo(0.17f))) return false;
		TestEqual(TEXT("Native RateWindow applies configured rate"), Anim->Montage_GetPlayRate(Montage), RateNotify->RateMultiplier);
		TestTrue(*FString::Printf(TEXT("Native CancelWindow grants Dodge and Defense [%s position=%.3f windows=%d policy=%d]"),
			*Montage->GetPathName(), Anim->Montage_GetPosition(Montage), Task->GetActiveCancelWindowCount(), static_cast<int32>(Task->GetCancelPolicy())),
			ASC->HasMatchingGameplayTag(DodgeTag) && ASC->HasMatchingGameplayTag(DefenseTag));
		TestTrue(TEXT("Native Begin receipts include animation/notify/instance"), RateBegins > 0 && CancelBegins > 0);
		FStructProperty* BlockProperty = FindFProperty<FStructProperty>(UGameplayAbility::StaticClass(), TEXT("ActivationBlockedTags"));
		if (!TestNotNull(TEXT("Target block configuration property"), BlockProperty)) return false;
		auto* Blocks = BlockProperty->ContainerPtrToValuePtr<FGameplayTagContainer>(Dodge);
		const auto SavedBlocks = *Blocks;
		Blocks->AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Action.Attacking")));
		TestFalse(TEXT("CanActivate failure inside window"), ASC->TryActivateAbility(DodgeHandle));
		*Blocks = SavedBlocks;
		TestTrue(TEXT("CanActivate failure retains same source Task"), Source->IsActive() && Source->GetMontageTask() == Task);
		const int32 FailedChecksBefore = FailingDodge->CommitCheckCallCount;
		ASC->TryActivateAbility(FailHandle);
		TestTrue(TEXT("Real target CommitCheck executed and failed"), FailingDodge->CommitCheckCallCount > FailedChecksBefore && !FailingDodge->IsActive());
		TestTrue(TEXT("Commit failure retains source and window"), Source->IsActive() && Source->GetMontageTask() == Task && ASC->HasMatchingGameplayTag(DodgeTag));
		if (!TestTrue(TEXT("Native advance passes all window ends while source active"), AdvanceTo(0.45f))) return false;
		TestEqual(TEXT("Rate restores baseline on actual Notify End"), Anim->Montage_GetPlayRate(Montage), 1.0f);
		TestFalse(TEXT("Actual Cancel End removes Dodge"), ASC->HasMatchingGameplayTag(DodgeTag));
		TestFalse(TEXT("Actual Cancel End removes Defense"), ASC->HasMatchingGameplayTag(DefenseTag));
		TestTrue(TEXT("Native End receipts include source and instance"), RateEnds > 0 && CancelEnds >= CancelBegins);
		Advance.Advance(1.0f);
		TestFalse(TEXT("Natural completion ends source"), Source->IsActive()); CheckClean(Task);
		for (const auto& Receipt : Receipts) ASC->GenericGameplayEventCallbacks.FindOrAdd(Receipt.Key).Remove(Receipt.Value);
		Receipts.Reset();
		Task = Activate(); if (!Task || !AdvanceTo(0.17f)) return false;
		TestTrue(TEXT("Real Dodge activation succeeds in window"), ASC->TryActivateAbility(DodgeHandle) && Dodge->IsActive());
		TestFalse(TEXT("Real Dodge cancels configuration-only source"), Source->IsActive()); CheckClean(Task);
		ASC->CancelAbilityHandle(DodgeHandle);
		Task = Activate(); if (!Task || !AdvanceTo(0.17f)) return false;
		ASC->CancelAbilityHandle(SourceHandle); CheckClean(Task);

		// Negative diagnostics are configuration inspection, never a runtime class registry.
		auto* ConfigTask = UAbilityTask_PlayActionMontage::PlayActionMontage(Source, NAME_None, Montage,
			1.0f, NAME_None, 1.0f, 0.0f, false, Source->CancelPolicy);
		ON_SCOPE_EXIT { ConfigTask->EndTask(); };
		const auto Validate = [&](const UAbilityTask_PlayActionMontage* Actual, EActionMontageCancelPolicy Policy)
		{
			return FManagedMontageTestHelpers::ValidateCancelWindowConfiguration(Source, Montage, Actual, Policy, {Dodge}, Reason);
		};
		TestFalse(TEXT("None cannot bypass missing standard entry"), Validate(nullptr, EActionMontageCancelPolicy::None));
		TestTrue(TEXT("Missing entry diagnostic identifies source and montage"), Reason.Contains(Source->GetName()) && Reason.Contains(Montage->GetName()) && Reason.Contains(TEXT("missing standard")));
		TestFalse(TEXT("Mismatched policy rejected"), Validate(ConfigTask, EActionMontageCancelPolicy::DodgeOnly));
		TestTrue(TEXT("Policy mismatch localized"), Reason.Contains(TEXT("policy mismatch")));
		UAnimMontage* OtherMontage = Variant == 0 ? Second : First;
		TestFalse(TEXT("Task must actually reference the declared montage"),
			FManagedMontageTestHelpers::ValidateCancelWindowConfiguration(Source, OtherMontage, ConfigTask, Source->CancelPolicy, {Dodge}, Reason));
		const auto SavedTags = Source->AbilityTags;
		Source->AbilityTags.RemoveTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Action.CancelableBy.Dodge")));
		TestFalse(TEXT("Missing source marker rejected"), Validate(ConfigTask, Source->CancelPolicy));
		TestTrue(TEXT("Missing marker localized"), Reason.Contains(TEXT("CancelableBy.Dodge")));
		Source->AbilityTags = SavedTags;
		const auto SavedNotifies = Montage->Notifies;
		Montage->Notifies.Reset();
		TestFalse(TEXT("Missing cancellation window rejected"), Validate(ConfigTask, Source->CancelPolicy));
		TestTrue(TEXT("Missing window localized"), Reason.Contains(TEXT("lack CancelWindow")));
		auto* NoneTask = UAbilityTask_PlayActionMontage::PlayActionMontage(Source, NAME_None, Montage);
		TestTrue(TEXT("Explicit None with no Cancel Notify is legal"), Validate(NoneTask, EActionMontageCancelPolicy::None));
		NoneTask->EndTask(); Montage->Notifies = SavedNotifies;
		for (const FName Field : {FName(TEXT("ActivationBlockedTags")), FName(TEXT("CancelAbilitiesWithTag"))})
		{
			auto* Property = FindFProperty<FStructProperty>(UGameplayAbility::StaticClass(), Field);
			auto* Tags = Property->ContainerPtrToValuePtr<FGameplayTagContainer>(Dodge);
			const auto Saved = *Tags;
			Tags->AddTag(FGameplayTag::RequestGameplayTag(Field == TEXT("ActivationBlockedTags") ? TEXT("State.Action.Attacking") : TEXT("Ability.Action.CancelableBy.Dodge")));
			TestFalse(TEXT("Target block or premature cancellation conflict rejected"), Validate(ConfigTask, Source->CancelPolicy));
			TestTrue(TEXT("Conflict diagnostic identifies actual target and reason"), Reason.Contains(Dodge->GetName()) && Reason.Contains(Field.ToString()));
			*Tags = Saved;
		}
		// A declared legacy source tag is permitted only with its own real GAS proof.
		Source->AbilityTags.Reset();
		const auto LegacyTag = FGameplayTag::RequestGameplayTag(TEXT("Ability.Attack.Light"));
		Source->AbilityTags.AddTag(LegacyTag);
		Source->CancelPolicy = EActionMontageCancelPolicy::DodgeOnly;
		FGameplayTagContainer RequiredTags; RequiredTags.AddTag(LegacyTag);
		if (!TestTrue(TEXT("Explicit-route source activates through standard entry"), ASC->TryActivateAbility(SourceHandle) && Source->IsActive())) return false;
		Task = Source->GetMontageTask();
		TestTrue(TEXT("Declared existing cancellation route passes diagnosis"), FManagedMontageTestHelpers::ValidateCancelWindowConfiguration(
			Source, Montage, Task, Source->CancelPolicy, {Dodge}, Reason, RequiredTags));
		Source->AbilityTags.RemoveTag(LegacyTag);
		TestFalse(TEXT("Missing declared source tag fails diagnosis"), FManagedMontageTestHelpers::ValidateCancelWindowConfiguration(
			Source, Montage, Task, Source->CancelPolicy, {Dodge}, Reason, RequiredTags));
		Source->AbilityTags.AddTag(LegacyTag);
		if (!TestTrue(TEXT("Explicit route reaches native cancel window"), AdvanceTo(0.17f))) return false;
		TestTrue(TEXT("Existing Dodge route really cancels declared source"), ASC->TryActivateAbility(DodgeHandle) && Dodge->IsActive() && !Source->IsActive());
		CheckClean(Task); ASC->CancelAbilityHandle(DodgeHandle);
		Source->AbilityTags = SavedTags; Source->CancelPolicy = EActionMontageCancelPolicy::DodgeAndDefense;

		// Destruction uses a separate actor because Destroy is terminal. The action class
		// and both animation configurations are the same as the reusable source above.
		APlayerCharacter* DestroyedPlayer = FCombatAutomationFixture::SpawnPlayer(World,
			FTransform(FVector(1000.0f + Variant * 300.0f, 0.0f, 0.0f)));
		if (!TestNotNull(TEXT("Destruction fixture"), DestroyedPlayer)) return false;
		auto* DestroyASC = DestroyedPlayer->GetAbilitySystemComponent();
		auto* DestroyAnim = NewObject<UAnimInstance>(DestroyedPlayer->GetMesh());
		DestroyAnim->InitializeMontageOnly(); DestroyAnim->CurrentSkeleton = Montage->GetSkeleton();
		DestroyedPlayer->GetMesh()->AnimScriptInstance = DestroyAnim; DestroyASC->RefreshAbilityActorInfo();
		FControlledMontageAdvance DestroyAdvance(DestroyedPlayer, DestroyAnim);
		const auto DestroyHandle = DestroyASC->GiveAbility(FGameplayAbilitySpec(UTestConfigurationOnlyActionAbility::StaticClass(), 1, INDEX_NONE, DestroyedPlayer));
		auto* DestroyAbility = CastChecked<UTestConfigurationOnlyActionAbility>(DestroyASC->FindAbilitySpecFromHandle(DestroyHandle)->GetPrimaryInstance());
		DestroyAbility->Montage = Montage;
		if (!TestTrue(TEXT("Destruction source real activation"), DestroyASC->TryActivateAbility(DestroyHandle) && DestroyAbility->IsActive())) return false;
		auto* DestroyTask = DestroyAbility->GetMontageTask();
		DestroyAdvance.Advance(0.20f);
		TestTrue(TEXT("Destruction starts with a native rate window"), DestroyTask->GetRateWindowLifecycle().GetActiveWindowCount() > 0);
		TestTrue(TEXT("Destroy actor succeeds"), DestroyedPlayer->Destroy());
		TestTrue(TEXT("Destruction terminates Task"), DestroyTask->IsTerminated());
		TestFalse(TEXT("Destruction clears rate binding"), DestroyTask->GetRateWindowLifecycle().IsBound());
		TestEqual(TEXT("Destruction clears cancel windows"), DestroyTask->GetActiveCancelWindowCount(), 0);
		TestFalse(TEXT("Destruction clears contributed Dodge"), DestroyTask->HasContributedDodgeTag());
		TestFalse(TEXT("Destruction clears contributed Defense"), DestroyTask->HasContributedDefenseTag());
		AddInfo(FString::Printf(TEXT("Configuration montage %s: rate Begin/End=%d/%d, cancel Begin/End=%d/%d, instance=%d"),
			*Montage->GetName(), RateBegins, RateEnds, CancelBegins, CancelEnds, InstanceID));
	}
	return true;
}
#endif

#endif // WITH_DEV_AUTOMATION_TESTS
