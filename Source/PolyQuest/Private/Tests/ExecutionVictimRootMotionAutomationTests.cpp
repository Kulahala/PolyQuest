#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/EnemyStanceBreakAbility.h"
#include "AbilitySystem/Abilities/EnemyVictimExecutionAbility.h"
#include "AbilitySystem/Abilities/PlayerBackstabExecutionAbility.h"
#include "AbilitySystem/Abilities/PlayerFrontExecutionAbility.h"
#include "AbilitySystem/CharacterAttributeSet.h"
#include "AI/EnemyAIController.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Character/Player/PlayerCharacter.h"
#include "Combat/Execution/ExecutionLockContext.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/PolyQuestPlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "Kismet/GameplayStatics.h"
#include "Tests/CombatAutomationFixture.h"
#include "Tests/TestProjectileDamageGE.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExecutionVictimRootMotionAutomationTest,
	"PolyQuest.Combat.ExecutionVictimRootMotion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace ExecutionVictimRootMotionAutomation
{
	struct FRootMotionTestWorldScope
	{
		UWorld* World = nullptr;
		~FRootMotionTestWorldScope()
		{
			if (World)
			{
				UGameplayStatics::SetGlobalTimeDilation(World, 1.0f);
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
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

	UAnimMontage* CreateValidRecoveryMontage(
		UObject* Outer,
		float Duration = 2.5f,
		bool bEnableRootMotion = true)
	{
		UObject* EffectiveOuter = Outer ? Outer : GetTransientPackage();
		UAnimMontage* Montage = NewObject<UAnimMontage>(EffectiveOuter);
		UAnimSequence* Seq = NewObject<UAnimSequence>(EffectiveOuter);
		Seq->bEnableRootMotion = bEnableRootMotion;

		FSlotAnimationTrack Track;
		Track.SlotName = FName(TEXT("DefaultGroup.DefaultSlot"));
		FAnimSegment Segment;
		Segment.SetAnimReference(Seq);
		Segment.StartPos = 0.0f;
		Segment.AnimStartTime = 0.0f;
		Segment.AnimEndTime = Duration;
		Segment.AnimPlayRate = 1.0f;
		Track.AnimTrack.AnimSegments.Add(Segment);
		Montage->SlotAnimTracks.Add(Track);

		FCompositeSection DefaultSec;
		DefaultSec.SectionName = FName(TEXT("Default"));
		DefaultSec.NextSectionName = NAME_None;
		DefaultSec.SetTime(0.0f);
		Montage->CompositeSections.Add(DefaultSec);

		UTestMontageAccessHelper::SetMontageLength(Montage, Duration);

		return Montage;
	}

	FGameplayAbilitySpecHandle ActivateEnemyStanceBreak(AEnemyCharacter* InEnemy)
	{
		UAbilitySystemComponent* ASC = InEnemy ? InEnemy->GetAbilitySystemComponent() : nullptr;
		if (!ASC)
		{
			return FGameplayAbilitySpecHandle();
		}

		ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetPoiseAttribute(), 0.0f);

		UAnimMontage* MockMontage = NewObject<UAnimMontage>(GetTransientPackage());
		UAnimInstance* MockAnimInstance = NewObject<UAnimInstance>(InEnemy->GetMesh());

		FGameplayAbilitySpec StanceBreakSpec(UEnemyStanceBreakAbility::StaticClass(), 1, INDEX_NONE, InEnemy);
		const FGameplayAbilitySpecHandle StanceBreakHandle = ASC->GiveAbility(StanceBreakSpec);
		if (FGameplayAbilitySpec* FoundSpec = ASC->FindAbilitySpecFromHandle(StanceBreakHandle))
		{
			if (UEnemyStanceBreakAbility* CDO = Cast<UEnemyStanceBreakAbility>(FoundSpec->Ability))
			{
				UAnimMontage* OldMontage = CDO->GetTestStanceBreakMontage();
				UAnimInstance* OldAnim = CDO->GetTestBoundAnimInstance();
				const bool bOldBypass = CDO->GetTestBypassMontageActiveCheck();

				CDO->SetTestStanceBreakMontage(MockMontage);
				CDO->SetTestBoundAnimInstance(MockAnimInstance);
				CDO->SetTestBypassMontageActiveCheck(true);

				ASC->TryActivateAbility(StanceBreakHandle);

				CDO->SetTestStanceBreakMontage(OldMontage);
				CDO->SetTestBoundAnimInstance(OldAnim);
				CDO->SetTestBypassMontageActiveCheck(bOldBypass);
			}
		}
		return StanceBreakHandle;
	}
}

bool FExecutionVictimRootMotionAutomationTest::RunTest(const FString& Parameters)
{
	AddExpectedErrorPlain(TEXT("SequencerDataModel"), EAutomationExpectedErrorFlags::Contains, -1);

	// =========================================================================
	// 1. CDO Defaults & Contract Verification
	// =========================================================================
	{
		const UEnemyVictimExecutionAbility* VictimCDO = UEnemyVictimExecutionAbility::StaticClass()->GetDefaultObject<UEnemyVictimExecutionAbility>();
		TestNotNull(TEXT("UEnemyVictimExecutionAbility CDO exists"), VictimCDO);
	}

	// =========================================================================
	// World Fixture Setup
	// =========================================================================
	ExecutionVictimRootMotionAutomation::FRootMotionTestWorldScope WorldScope;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("ExecutionVictimRootMotionTestWorld"));
	WorldScope.World = World;
	if (!TestNotNull(TEXT("Combat World created"), World))
	{
		return false;
	}

	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(World);

	FURL WorldURL;
	World->InitializeActorsForPlay(WorldURL);
	World->BeginPlay();

	APlayerCharacter* Player = FCombatAutomationFixture::SpawnPlayer(
		World,
		FTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, 0.0f)));
	AEnemyCharacter* Enemy = FCombatAutomationFixture::SpawnPassiveEnemy(
		World,
		FTransform(FRotator(0.0f, 180.0f, 0.0f), FVector(150.0f, 0.0f, 0.0f)));
	APolyQuestPlayerController* Controller = World->SpawnActor<APolyQuestPlayerController>();

	if (!TestNotNull(TEXT("Player spawned"), Player) ||
		!TestNotNull(TEXT("Enemy spawned"), Enemy) ||
		!TestNotNull(TEXT("Controller spawned"), Controller))
	{
		return false;
	}

	Controller->Possess(Player);

	AActor* FloorActor = World->SpawnActor<AActor>();
	if (FloorActor)
	{
		UBoxComponent* FloorBox = NewObject<UBoxComponent>(FloorActor);
		FloorBox->InitBoxExtent(FVector(5000.0f, 5000.0f, 50.0f));
		FloorBox->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		FloorActor->SetRootComponent(FloorBox);
		FloorBox->RegisterComponent();
		const float CapsuleHalfHeight = Enemy->GetCapsuleComponent() ? Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.0f;
		const float CapsuleBottomZ = Enemy->GetActorLocation().Z - CapsuleHalfHeight;
		FloorActor->SetActorLocation(FVector(0.0f, 0.0f, CapsuleBottomZ - 50.0f));
		if (Player->GetCapsuleComponent())
		{
			Player->GetCapsuleComponent()->IgnoreActorWhenMoving(FloorActor, true);
		}
	}

	UAbilitySystemComponent* PlayerASC = Player->GetAbilitySystemComponent();
	UAbilitySystemComponent* EnemyASC = Enemy->GetAbilitySystemComponent();
	if (!TestNotNull(TEXT("Player ASC valid"), PlayerASC) || !TestNotNull(TEXT("Enemy ASC valid"), EnemyASC))
	{
		return false;
	}

	const FGameplayTag TagTeamPlayer = FGameplayTag::RequestGameplayTag(FName(TEXT("Team.Player")), false);
	const FGameplayTag TagTeamEnemy = FGameplayTag::RequestGameplayTag(FName(TEXT("Team.Enemy")), false);
	Player->SetTestCombatTeamTag(TagTeamPlayer);
	Enemy->SetTestCombatTeamTag(TagTeamEnemy);

	const FGameplayTag VictimStartTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.VictimStart")), false);
	const FGameplayTag FrontHitTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Hit")), false);
	const FGameplayTag VictimLockedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Execution.VictimLocked")), false);
	const FGameplayTag InvulnerableTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Invulnerable")), false);
	const FGameplayTag StunnedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
	const FGameplayTag LaunchReactionEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Enemy.Launch")), false);

	UAnimMontage* PlayerExecutionMontage = NewObject<UAnimMontage>(GetTransientPackage());

	UAnimInstance* MockAnimInstance = NewObject<UAnimInstance>(Enemy->GetMesh());

	auto GrantAndConfigureVictimAbility = [&](AEnemyCharacter* InEnemy, UAnimMontage* FrontMontage, UAnimMontage* BackstabMontage) -> TPair<FGameplayAbilitySpecHandle, UEnemyVictimExecutionAbility*>
	{
		UAbilitySystemComponent* TargetASC = InEnemy->GetAbilitySystemComponent();
		FGameplayAbilitySpec VictimSpec(UEnemyVictimExecutionAbility::StaticClass(), 1, INDEX_NONE, InEnemy);
		const FGameplayAbilitySpecHandle VictimHandle = TargetASC->GiveAbility(VictimSpec);
		FGameplayAbilitySpec* FoundSpec = TargetASC->FindAbilitySpecFromHandle(VictimHandle);
		UEnemyVictimExecutionAbility* Instance = FoundSpec ? Cast<UEnemyVictimExecutionAbility>(FoundSpec->GetPrimaryInstance()) : nullptr;
		if (Instance)
		{
			Instance->SetTestVictimMontages(FrontMontage, BackstabMontage);
			Instance->SetTestBoundAnimInstance(MockAnimInstance);
			Instance->SetTestBypassMontageActiveCheck(true);
		}
		return { VictimHandle, Instance };
	};

	auto SetupFrontExec = [&](UAnimMontage* VictimMontage, float InEnemyHealth = 100.0f) -> TTuple<FGameplayAbilitySpecHandle, UPlayerFrontExecutionAbility*, FGameplayAbilitySpecHandle, UEnemyVictimExecutionAbility*>
	{
		Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
		Player->SetActorRotation(FRotator::ZeroRotator);
		Enemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
		Enemy->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f));

		if (UCharacterAttributeSet* EnemyAttribs = const_cast<UCharacterAttributeSet*>(EnemyASC->GetSet<UCharacterAttributeSet>()))
		{
			EnemyAttribs->SetHealth(InEnemyHealth);
			EnemyAttribs->SetPoise(0.0f);
		}

		ExecutionVictimRootMotionAutomation::ActivateEnemyStanceBreak(Enemy);
		if (!EnemyASC->HasMatchingGameplayTag(StunnedTag))
		{
			EnemyASC->AddLooseGameplayTag(StunnedTag);
		}

		Player->SetTestLockedTarget(Enemy);

		auto [VictimHandle, VictimAbility] = GrantAndConfigureVictimAbility(Enemy, VictimMontage, nullptr);

		FGameplayAbilitySpec FrontSpec(UPlayerFrontExecutionAbility::StaticClass(), 1, INDEX_NONE, Player);
		const FGameplayAbilitySpecHandle FrontHandle = PlayerASC->GiveAbility(FrontSpec);
		FGameplayAbilitySpec* FoundSpec = PlayerASC->FindAbilitySpecFromHandle(FrontHandle);
		UPlayerFrontExecutionAbility* FrontAbility = FoundSpec ? Cast<UPlayerFrontExecutionAbility>(FoundSpec->GetPrimaryInstance()) : nullptr;
		if (FrontAbility)
		{
			FrontAbility->SetTestExecutionMontage(PlayerExecutionMontage);
			FrontAbility->SetTestDamageGameplayEffectClass(UTestProjectileDamageGE::StaticClass());
			FrontAbility->SetTestExecutionDistances(0.0f, 250.0f);
			FrontAbility->SetTestMaxFrontAngleDegrees(60.0f);
			FrontAbility->SetTestSkipMontageTaskActivation(true);
		}

		PlayerASC->TryActivateAbility(FrontHandle);
		return MakeTuple(FrontHandle, FrontAbility, VictimHandle, VictimAbility);
	};

	auto CleanupExec = [&](FGameplayAbilitySpecHandle FrontHandle, FGameplayAbilitySpecHandle VictimHandle)
	{
		if (PlayerASC && FrontHandle.IsValid())
		{
			PlayerASC->ClearAbility(FrontHandle);
		}
		if (EnemyASC && VictimHandle.IsValid())
		{
			EnemyASC->ClearAbility(VictimHandle);
		}
	};

	int32 LaunchEventCount = 0;
	FDelegateHandle LaunchHandle = EnemyASC->GenericGameplayEventCallbacks.FindOrAdd(LaunchReactionEventTag).AddLambda(
		[&LaunchEventCount](const FGameplayEventData* EventData)
		{
			++LaunchEventCount;
		});

	// =========================================================================
	// 2. Recovery Candidate Gating & Legitimate In-Place Montage (Zero Launch)
	// =========================================================================
	{
		Enemy->RestorePoiseToMax();
		Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		LaunchEventCount = 0;

		// 2A: Missing victim montage (nullptr) -> Cleans up with zero Enemy Launch
		auto [FrontHandle, FrontAbility, VictimHandle, VictimAbility] = SetupFrontExec(nullptr, 100.0f);
		if (TestNotNull(TEXT("FrontAbility valid in Sec 2A"), FrontAbility) && TestNotNull(TEXT("VictimAbility valid in Sec 2A"), VictimAbility))
		{
			// Step A: Hit arrives first (stab)
			FGameplayEventData HitPayload;
			HitPayload.EventTag = FrontHitTag;
			HitPayload.Instigator = Player;
			HitPayload.Target = Player;
			HitPayload.OptionalObject = PlayerExecutionMontage;
			FrontAbility->TestTriggerHitEvent(HitPayload);

			// Step B: VictimStart arrives second (draw blade)
			FGameplayEventData StartPayload;
			StartPayload.EventTag = VictimStartTag;
			StartPayload.Instigator = Player;
			StartPayload.Target = Player;
			StartPayload.OptionalObject = PlayerExecutionMontage;
			FrontAbility->TestTriggerVictimStartEvent(StartPayload);

			TestFalse(TEXT("Victim ability ended cleanly on missing montage"), VictimAbility->IsActive());
			TestFalse(TEXT("Non-lethal recovery was NOT active"), VictimAbility->IsTestNonLethalRecoveryActive());
			TestEqual(TEXT("Launch reaction NEVER dispatched on missing montage"), LaunchEventCount, 0);

			FrontAbility->TestEndAbility(false);
			CleanupExec(FrontHandle, VictimHandle);
		}

		// 2B: Legitimate In-Place Montage (no root motion) -> Successful handoff, plays directly with zero launch
		LaunchEventCount = 0;
		UAnimMontage* NoRootMotionMontage = ExecutionVictimRootMotionAutomation::CreateValidRecoveryMontage(
			GetTransientPackage(), 2.5f, false);
		TestFalse(TEXT("NoRootMotionMontage has no root motion"), NoRootMotionMontage->HasRootMotion());

		auto [FrontHandle2, FrontAbility2, VictimHandle2, VictimAbility2] = SetupFrontExec(NoRootMotionMontage, 100.0f);
		if (TestNotNull(TEXT("FrontAbility valid in Sec 2B"), FrontAbility2) && TestNotNull(TEXT("VictimAbility valid in Sec 2B"), VictimAbility2))
		{
			// Step A: Hit arrives first
			FGameplayEventData HitPayload;
			HitPayload.EventTag = FrontHitTag;
			HitPayload.Instigator = Player;
			HitPayload.Target = Player;
			HitPayload.OptionalObject = PlayerExecutionMontage;
			FrontAbility2->TestTriggerHitEvent(HitPayload);

			// Step B: VictimStart arrives second
			FGameplayEventData StartPayload;
			StartPayload.EventTag = VictimStartTag;
			StartPayload.Instigator = Player;
			StartPayload.Target = Player;
			StartPayload.OptionalObject = PlayerExecutionMontage;
			FrontAbility2->TestTriggerVictimStartEvent(StartPayload);

			TestTrue(TEXT("Victim ability active on legitimate in-place montage"), VictimAbility2->IsActive());
			TestTrue(TEXT("Recovery activated for legitimate in-place montage"), VictimAbility2->IsTestNonLethalRecoveryActive());
			TestEqual(TEXT("Launch reaction NEVER dispatched for in-place montage"), LaunchEventCount, 0);

			// Step C: Natural completion finishes recovery
			VictimAbility2->TestTriggerVictimMontageCompleted();
			TestFalse(TEXT("Victim ability ended on completion"), VictimAbility2->IsActive());
			TestEqual(TEXT("Launch reaction STILL zero after completion"), LaunchEventCount, 0);

			FrontAbility2->TestEndAbility(false);
			CleanupExec(FrontHandle2, VictimHandle2);
		}
	}

	// =========================================================================
	// 3. Full Non-Lethal Recovery Handoff & Independent Lifecycle (Zero Launch)
	// =========================================================================
	{
		Enemy->RestorePoiseToMax();
		Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		Enemy->GetCharacterMovement()->bCanWalkOffLedges = true;
		LaunchEventCount = 0;

		UAnimMontage* ValidMontage = ExecutionVictimRootMotionAutomation::CreateValidRecoveryMontage(
			GetTransientPackage(), 2.5f, true);

		auto [FrontHandle, FrontAbility, VictimHandle, VictimAbility] = SetupFrontExec(ValidMontage, 100.0f);
		if (TestNotNull(TEXT("FrontAbility valid in Sec 3"), FrontAbility) && TestNotNull(TEXT("VictimAbility valid in Sec 3"), VictimAbility))
		{
			TestTrue(TEXT("Enemy initial bCanWalkOffLedges is true"), Enemy->GetCharacterMovement()->bCanWalkOffLedges);

			// Step A: Hit arrives first (stab)
			FGameplayEventData HitPayload;
			HitPayload.EventTag = FrontHitTag;
			HitPayload.Instigator = Player;
			HitPayload.Target = Player;
			HitPayload.OptionalObject = PlayerExecutionMontage;
			FrontAbility->TestTriggerHitEvent(HitPayload);

			// Step B: VictimStart arrives second (draw blade: starts recovery from time 0)
			FGameplayEventData StartPayload;
			StartPayload.EventTag = VictimStartTag;
			StartPayload.Instigator = Player;
			StartPayload.Target = Player;
			StartPayload.OptionalObject = PlayerExecutionMontage;
			FrontAbility->TestTriggerVictimStartEvent(StartPayload);

			TestTrue(TEXT("Victim ability remains Active during recovery"), VictimAbility->IsActive());
			TestTrue(TEXT("bNonLethalRecoveryActive is true"), VictimAbility->IsTestNonLethalRecoveryActive());
			TestEqual(TEXT("MovementMode switched to MOVE_Walking"), Enemy->GetCharacterMovement()->MovementMode, MOVE_Walking);
			TestFalse(TEXT("bCanWalkOffLedges turned off during recovery"), Enemy->GetCharacterMovement()->bCanWalkOffLedges);
			TestTrue(TEXT("Saved bCanWalkOffLedges is true"), VictimAbility->GetTestSavedCanWalkOffLedges());

			TestTrue(TEXT("VictimLocked tag retained"), EnemyASC->HasMatchingGameplayTag(VictimLockedTag));
			TestTrue(TEXT("Invulnerable tag retained"), EnemyASC->HasMatchingGameplayTag(InvulnerableTag));
			TestTrue(TEXT("Stunned tag retained"), EnemyASC->HasMatchingGameplayTag(StunnedTag));

			// Player ability ends naturally on its own montage blendout
			FrontAbility->TestEndAbility(false);
			TestFalse(TEXT("Player ability is ended"), FrontAbility->IsActive());

			// Contract: Victim recovery is completely decoupled and continues unaffected
			TestTrue(TEXT("Victim ability survives player termination"), VictimAbility->IsActive());
			TestTrue(TEXT("Victim recovery remains active"), VictimAbility->IsTestNonLethalRecoveryActive());

			// BlendOut arrives: locks retained
			VictimAbility->TestTriggerVictimMontageBlendOut();
			TestTrue(TEXT("Victim still active after BlendOut"), VictimAbility->IsActive());
			TestTrue(TEXT("Locks retained after BlendOut"), EnemyASC->HasMatchingGameplayTag(VictimLockedTag));

			// Completed arrives: natural cleanup
			VictimAbility->TestTriggerVictimMontageCompleted();
			TestFalse(TEXT("Victim completed naturally"), VictimAbility->IsActive());
			TestFalse(TEXT("VictimLocked tag cleared"), EnemyASC->HasMatchingGameplayTag(VictimLockedTag));
			TestFalse(TEXT("Invulnerable tag cleared"), EnemyASC->HasMatchingGameplayTag(InvulnerableTag));
			TestTrue(TEXT("bCanWalkOffLedges restored to true"), Enemy->GetCharacterMovement()->bCanWalkOffLedges);

			TestEqual(TEXT("Zero launch reaction dispatched for recovered victim"), LaunchEventCount, 0);

			CleanupExec(FrontHandle, VictimHandle);
		}
	}

	// =========================================================================
	// 4. Movement Mode Transition Interruption (Falling triggers cancellation)
	// =========================================================================
	{
		Enemy->RestorePoiseToMax();
		Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		Enemy->GetCharacterMovement()->bCanWalkOffLedges = false;
		LaunchEventCount = 0;

		UAnimMontage* ValidMontage = ExecutionVictimRootMotionAutomation::CreateValidRecoveryMontage(
			GetTransientPackage(), 2.5f, true);

		auto [FrontHandle, FrontAbility, VictimHandle, VictimAbility] = SetupFrontExec(ValidMontage, 100.0f);
		if (TestNotNull(TEXT("FrontAbility valid in Sec 4"), FrontAbility) && TestNotNull(TEXT("VictimAbility valid in Sec 4"), VictimAbility))
		{
			// Step A: Hit arrives first
			FGameplayEventData HitPayload;
			HitPayload.EventTag = FrontHitTag;
			HitPayload.Instigator = Player;
			HitPayload.Target = Player;
			HitPayload.OptionalObject = PlayerExecutionMontage;
			FrontAbility->TestTriggerHitEvent(HitPayload);

			// Step B: VictimStart arrives second
			FGameplayEventData StartPayload;
			StartPayload.EventTag = VictimStartTag;
			StartPayload.Instigator = Player;
			StartPayload.Target = Player;
			StartPayload.OptionalObject = PlayerExecutionMontage;
			FrontAbility->TestTriggerVictimStartEvent(StartPayload);

			TestTrue(TEXT("Recovery active in Sec 4"), VictimAbility->IsTestNonLethalRecoveryActive());

			Enemy->GetCharacterMovement()->MovementMode = MOVE_Falling;
			VictimAbility->TestTriggerMovementModeChanged(MOVE_Walking, 0);

			TestFalse(TEXT("Victim cancelled on leaving Walking"), VictimAbility->IsActive());
			TestEqual(TEXT("External Falling mode not overridden"), Enemy->GetCharacterMovement()->MovementMode, MOVE_Falling);
			TestFalse(TEXT("bCanWalkOffLedges restored to original false"), Enemy->GetCharacterMovement()->bCanWalkOffLedges);

			TestEqual(TEXT("Zero launch dispatched on falling cancellation"), LaunchEventCount, 0);

			FrontAbility->TestEndAbility(false);
			CleanupExec(FrontHandle, VictimHandle);
		}
	}

	// =========================================================================
	// 5. Lethal Damage Preempts Non-Lethal Recovery
	// =========================================================================
	{
		Enemy->RestorePoiseToMax();
		Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		LaunchEventCount = 0;

		UAnimMontage* ValidMontage = ExecutionVictimRootMotionAutomation::CreateValidRecoveryMontage(
			GetTransientPackage(), 2.5f, true);

		auto [FrontHandle, FrontAbility, VictimHandle, VictimAbility] = SetupFrontExec(ValidMontage, 100.0f);
		if (TestNotNull(TEXT("FrontAbility valid in Sec 5"), FrontAbility) && TestNotNull(TEXT("VictimAbility valid in Sec 5"), VictimAbility))
		{
			VictimAbility->NotifyLethalDamageReceived();
			TestTrue(TEXT("Death pending marked"), VictimAbility->IsDeathPending());

			// Step A: Hit arrives first (lethal)
			FGameplayEventData HitPayload;
			HitPayload.EventTag = FrontHitTag;
			HitPayload.Instigator = Player;
			HitPayload.Target = Player;
			HitPayload.OptionalObject = PlayerExecutionMontage;
			FrontAbility->TestTriggerHitEvent(HitPayload);

			// Step B: VictimStart arrives second (commits death immediately, does NOT play recovery)
			FGameplayEventData StartPayload;
			StartPayload.EventTag = VictimStartTag;
			StartPayload.Instigator = Player;
			StartPayload.Target = Player;
			StartPayload.OptionalObject = PlayerExecutionMontage;
			FrontAbility->TestTriggerVictimStartEvent(StartPayload);

			TestFalse(TEXT("Victim does NOT enter non-lethal recovery on lethal execution"), VictimAbility->IsTestNonLethalRecoveryActive());
			TestFalse(TEXT("Victim ability ended on death commit"), VictimAbility->IsActive());
			TestEqual(TEXT("Zero launch on lethal completion"), LaunchEventCount, 0);

			FrontAbility->TestEndAbility(false);
			CleanupExec(FrontHandle, VictimHandle);
		}
	}

	EnemyASC->GenericGameplayEventCallbacks.FindOrAdd(LaunchReactionEventTag).Remove(LaunchHandle);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
