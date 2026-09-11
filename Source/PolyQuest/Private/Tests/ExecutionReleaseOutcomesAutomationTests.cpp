#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/EnemyLaunchReactionAbility.h"
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
#include "Combat/Equipment/MeleeWeaponDefinition.h"
#include "Combat/Equipment/WeaponEquipmentComponent.h"
#include "Combat/Execution/ExecutionLockContext.h"
#include "Combat/Melee/MeleeHitResolver.h"
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
	FExecutionReleaseOutcomesAutomationTest,
	"PolyQuest.Combat.ExecutionReleaseOutcomes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace ExecutionReleaseOutcomesAutomation
{
	struct FReleaseOutcomesWorldScope
	{
		UWorld* World = nullptr;
		~FReleaseOutcomesWorldScope()
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

bool FExecutionReleaseOutcomesAutomationTest::RunTest(const FString& Parameters)
{
	AddExpectedErrorPlain(TEXT("SequencerDataModel"), EAutomationExpectedErrorFlags::Contains, -1);

	// =========================================================================
	// 1. CDO & Tag Registration Verification
	// =========================================================================
	{
		const FGameplayTag ReleaseFormalTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Release")), false);
		TestTrue(TEXT("Tag Event.Action.Execution.Release is registered and valid"), ReleaseFormalTag.IsValid());

		const FGameplayTag LaunchReactionTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Enemy.Launch")), false);
		TestTrue(TEXT("Tag Event.Reaction.Enemy.Launch is registered and valid"), LaunchReactionTag.IsValid());
	}

	// =========================================================================
	// 2. ExecutionLockContext Release & Outcome State Machine Unit Tests
	// =========================================================================
	{
		UExecutionLockContext* Context = NewObject<UExecutionLockContext>();
		TestNotNull(TEXT("Context created"), Context);
		TestEqual(TEXT("Initial ReleaseState is NotRequested"), Context->GetReleaseState(), EExecutionSessionReleaseState::NotRequested);
		TestFalse(TEXT("Context initially not release sent"), Context->IsReleaseSent());
		TestFalse(TEXT("Context initially not victim released"), Context->IsVictimReleased());

		UPlayerFrontExecutionAbility* DummySourceAbility = NewObject<UPlayerFrontExecutionAbility>();
		UEnemyVictimExecutionAbility* DummyVictimAbility = NewObject<UEnemyVictimExecutionAbility>();
		AActor* DummySourceActor = NewObject<AActor>();
		AActor* DummyTargetActor = NewObject<AActor>();
		UAbilitySystemComponent* DummySourceASC = NewObject<UAbilitySystemComponent>();
		UAbilitySystemComponent* DummyVictimASC = NewObject<UAbilitySystemComponent>();

		const FGameplayTag FrontReqTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.Front")), false);
		const uint32 TestToken = 201;

		Context->InitializeSession(DummySourceAbility, DummySourceActor, DummySourceASC, DummyTargetActor, FrontReqTag, TestToken);
		Context->AcceptVictim(DummyVictimAbility, DummyTargetActor, DummyVictimASC, FrontReqTag, TestToken);

		// 2.1 TryBeginRelease requires resolved hit when bRequireResolvedHit is true
		Context->SetTestHitState(EExecutionSessionHitState::Ready);
		TestFalse(TEXT("TryBeginRelease rejected when HitState is Ready and bRequireResolvedHit=true"),
			Context->TryBeginRelease(DummySourceAbility, TestToken, false, true));
		TestEqual(TEXT("ReleaseState remains NotRequested"), Context->GetReleaseState(), EExecutionSessionReleaseState::NotRequested);

		// TryBeginRelease with bRequireResolvedHit=false (fallback) succeeds even if hit not resolved
		TestTrue(TEXT("TryBeginRelease succeeds with bRequireResolvedHit=false"),
			Context->TryBeginRelease(DummySourceAbility, TestToken, false, false));
		TestTrue(TEXT("Release is sent"), Context->IsReleaseSent());
		TestEqual(TEXT("ReleaseState is Requested"), Context->GetReleaseState(), EExecutionSessionReleaseState::Requested);

		// 2.2 MarkVictimReleased transitions state
		Context->MarkVictimReleased(DummyVictimAbility);
		TestTrue(TEXT("IsVictimReleased true"), Context->IsVictimReleased());
		TestEqual(TEXT("ReleaseState is VictimReleased"), Context->GetReleaseState(), EExecutionSessionReleaseState::VictimReleased);

		// Late hit must be rejected once victim released
		TestFalse(TEXT("IsHitAuthorized rejects late hit when victim released"),
			Context->IsHitAuthorized(DummySourceAbility, DummySourceActor, DummySourceASC, DummyTargetActor, DummyVictimASC));

		// 2.3 Outcome Finalization
		Context->SetTestHitState(EExecutionSessionHitState::DeathPending);
		TestTrue(TEXT("BeginOutcomeFinalization succeeds from DeathPending and VictimReleased"),
			Context->BeginOutcomeFinalization(DummyVictimAbility));
		TestTrue(TEXT("IsFinalizing true"), Context->IsFinalizing());

		Context->AbortOutcomeFinalization(DummyVictimAbility);
		TestTrue(TEXT("AbortOutcomeFinalization rolls back to DeathPending"), Context->IsDeathPending());
		TestFalse(TEXT("IsFinalizing false after abort"), Context->IsFinalizing());

		TestTrue(TEXT("BeginOutcomeFinalization succeeds second time"),
			Context->BeginOutcomeFinalization(DummyVictimAbility));
		Context->CompleteOutcomeFinalization(DummyVictimAbility);
		TestTrue(TEXT("IsFinalized true"), Context->IsFinalized());
		TestEqual(TEXT("ReleaseState is Finalized"), Context->GetReleaseState(), EExecutionSessionReleaseState::Finalized);
	}

	// =========================================================================
	// 3. World Setup for Gameplay Tests
	// =========================================================================
	ExecutionReleaseOutcomesAutomation::FReleaseOutcomesWorldScope WorldScope;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("ExecutionReleaseOutcomesTestWorld"));
	WorldScope.World = World;
	if (!TestNotNull(TEXT("World created successfully"), World))
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
	const FGameplayTag VictimStartTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.VictimStart")), false);
	const FGameplayTag FrontHitTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Hit")), false);
	const FGameplayTag VictimLockedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Execution.VictimLocked")), false);
	const FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
	const FGameplayTag InvulnerableTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Invulnerable")), false);

	// Setup mock animations
	UAnimMontage* PlayerExecutionMontage = NewObject<UAnimMontage>(GetTransientPackage());
	UAnimMontage* EnemyVictimMontage = ExecutionReleaseOutcomesAutomation::CreateValidRecoveryMontage(GetTransientPackage(), 2.5f, true);
	UAnimInstance* MockAnimInstance = NewObject<UAnimInstance>(Enemy->GetMesh());

	auto GrantAndConfigureVictimAbility = [&](AEnemyCharacter* InEnemy, bool bConfigureVictimMontage = false) -> TPair<FGameplayAbilitySpecHandle, UEnemyVictimExecutionAbility*>
	{
		UAbilitySystemComponent* TargetASC = InEnemy->GetAbilitySystemComponent();
		FGameplayAbilitySpec VictimSpec(UEnemyVictimExecutionAbility::StaticClass(), 1, INDEX_NONE, InEnemy);
		const FGameplayAbilitySpecHandle VictimHandle = TargetASC->GiveAbility(VictimSpec);
		FGameplayAbilitySpec* FoundSpec = TargetASC->FindAbilitySpecFromHandle(VictimHandle);
		UEnemyVictimExecutionAbility* Instance = FoundSpec ? Cast<UEnemyVictimExecutionAbility>(FoundSpec->GetPrimaryInstance()) : nullptr;
		if (Instance)
		{
			Instance->SetTestBoundAnimInstance(MockAnimInstance);
			Instance->SetTestBypassMontageActiveCheck(true);
			if (bConfigureVictimMontage)
			{
				Instance->SetTestVictimMontages(EnemyVictimMontage, EnemyVictimMontage);
			}
			else
			{
				Instance->SetTestVictimMontages(nullptr, nullptr);
			}
		}
		return { VictimHandle, Instance };
	};

	auto SetupFrontAndVictimExec = [&](float InEnemyHealth = 100.0f, bool bConfigureVictimMontage = false) -> TTuple<FGameplayAbilitySpecHandle, UPlayerFrontExecutionAbility*, FGameplayAbilitySpecHandle, UEnemyVictimExecutionAbility*>
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

		ExecutionReleaseOutcomesAutomation::ActivateEnemyStanceBreak(Enemy);
		const FGameplayTag StunnedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
		if (!EnemyASC->HasMatchingGameplayTag(StunnedTag))
		{
			EnemyASC->AddLooseGameplayTag(StunnedTag);
		}

		Player->SetTestLockedTarget(Enemy);

		// 1. Grant Victim Ability FIRST
		auto [VictimHandle, VictimAbility] = GrantAndConfigureVictimAbility(Enemy, bConfigureVictimMontage);

		// 2. Grant Player Ability
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

		// 3. Activate Player Ability
		PlayerASC->TryActivateAbility(FrontHandle);

		return MakeTuple(FrontHandle, FrontAbility, VictimHandle, VictimAbility);
	};

	auto CleanupFrontExec = [&](FGameplayAbilitySpecHandle FrontHandle, FGameplayAbilitySpecHandle VictimHandle)
	{
		Player->TestClearLockedTarget();
		const FGameplayTag StunnedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
		EnemyASC->RemoveLooseGameplayTag(StunnedTag);
		PlayerASC->ClearAbility(FrontHandle);
		EnemyASC->ClearAbility(VictimHandle);

		if (UWeaponEquipmentComponent* EquipComp = Player->FindComponentByClass<UWeaponEquipmentComponent>())
		{
			if (UMeleeWeaponDefinition* WeaponDef = EquipComp->GetEquippedMainHandMelee())
			{
				WeaponDef->MinExecutionDistance = 0.0f;
				WeaponDef->MaxExecutionDistance = 250.0f;
				WeaponDef->ExecutionSnapDistance = 190.0f;
				WeaponDef->FrontExecutionMontage = nullptr;
				WeaponDef->BackstabExecutionMontage = nullptr;
			}
		}
	};

	// =========================================================================
	// 5. Orderly Hit -> VictimStart Sequence (NonLethal, Formal Release Handshake, Unconfigured Victim Clean Cleanup Without Launch)
	// =========================================================================
	{
		auto [FrontHandle, FrontAbility, VictimHandle, VictimAbility] = SetupFrontAndVictimExec(100.0f, false);
		if (!TestNotNull(TEXT("FrontAbility instance exists"), FrontAbility) ||
			!TestTrue(TEXT("Front execution active"), FrontAbility->IsActive()))
		{
			return false;
		}

		// Track Launch Reaction event on Enemy
		bool bLaunchReactionReceived = false;
		const FGameplayTag LaunchEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Enemy.Launch")), false);
		FDelegateHandle LaunchHandle = EnemyASC->GenericGameplayEventCallbacks.FindOrAdd(LaunchEventTag)
			.AddLambda([&bLaunchReactionReceived, Player, Enemy](const FGameplayEventData* InPayload)
			{
				if (InPayload && InPayload->Instigator.Get() == Player && InPayload->Target.Get() == Enemy)
				{
					bLaunchReactionReceived = true;
				}
			});

		TestTrue(TEXT("Enemy locked in execution"), EnemyASC->HasMatchingGameplayTag(VictimLockedTag));

		UExecutionLockContext* ExecContext = FrontAbility->GetTestExecutionContext();
		if (!TestNotNull(TEXT("ExecContext exists"), ExecContext))
		{
			return false;
		}

		// Step A: Hit arrives first (stab)
		FGameplayEventData HitPayload;
		HitPayload.EventTag = FrontHitTag;
		HitPayload.Instigator = Player;
		HitPayload.Target = Player;
		HitPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerHitEvent(HitPayload);

		TestTrue(TEXT("Hit consumed"), FrontAbility->IsTestDamageEventConsumed());
		TestEqual(TEXT("HitState is NonLethal"), ExecContext->GetHitState(), EExecutionSessionHitState::NonLethal);
		TestFalse(TEXT("Release not yet sent"), ExecContext->IsReleaseSent());
		TestTrue(TEXT("Enemy remains locked after hit"), EnemyASC->HasMatchingGameplayTag(VictimLockedTag));

		// Step B: VictimStart arrives (draw blade: formal release handoff)
		FGameplayEventData StartPayload;
		StartPayload.EventTag = VictimStartTag;
		StartPayload.Instigator = Player;
		StartPayload.Target = Player;
		StartPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerVictimStartEvent(StartPayload);

		TestTrue(TEXT("Release sent to victim on VictimStart"), ExecContext->IsReleaseSent());
		TestTrue(TEXT("Victim released on VictimStart"), ExecContext->IsVictimReleased());
		TestFalse(TEXT("Victim locked tag removed on release"), EnemyASC->HasMatchingGameplayTag(VictimLockedTag));
		TestFalse(TEXT("Launch reaction event NEVER dispatched to unconfigured victim"), bLaunchReactionReceived);
		TestTrue(TEXT("Player execution ability remains active for montage tail"), FrontAbility->IsActive());

		// Step D: Player montage finishes naturally
		FrontAbility->TestEndAbility(false);
		TestFalse(TEXT("Player execution ended naturally"), FrontAbility->IsActive());
		TestFalse(TEXT("Context invalidated after player end"), ExecContext->IsActive());

		EnemyASC->GenericGameplayEventCallbacks.FindOrAdd(LaunchEventTag).Remove(LaunchHandle);
		CleanupFrontExec(FrontHandle, VictimHandle);
	}

	// =========================================================================
	// 6. VictimStart Arrives Before Hit: Rejected Without Latching, Safe Normal Hit -> VictimStart
	// =========================================================================
	{
		Enemy->RestorePoiseToMax();
		Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

		auto [FrontHandle, FrontAbility, VictimHandle, VictimAbility] = SetupFrontAndVictimExec(100.0f);
		if (!TestNotNull(TEXT("FrontAbility instance exists"), FrontAbility) ||
			!TestTrue(TEXT("Front ability activated"), FrontAbility->IsActive()))
		{
			return false;
		}

		UExecutionLockContext* ExecContext = FrontAbility->GetTestExecutionContext();
		if (!TestNotNull(TEXT("ExecContext exists"), ExecContext))
		{
			return false;
		}

		// Step A: VictimStart arrives early before hit (rejected without latching)
		FGameplayEventData EarlyStartPayload;
		EarlyStartPayload.EventTag = VictimStartTag;
		EarlyStartPayload.Instigator = Player;
		EarlyStartPayload.Target = Player;
		EarlyStartPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerVictimStartEvent(EarlyStartPayload);

		TestFalse(TEXT("Early VictimStart not forwarded"), FrontAbility->IsTestVictimStartForwarded());
		TestFalse(TEXT("Release NOT sent prematurely"), ExecContext->IsReleaseSent());
		TestTrue(TEXT("Victim remains locked"), EnemyASC->HasMatchingGameplayTag(VictimLockedTag));

		// Step B: Hit arrives and resolves damage (does NOT flush out-of-order start)
		FGameplayEventData HitPayload;
		HitPayload.EventTag = FrontHitTag;
		HitPayload.Instigator = Player;
		HitPayload.Target = Player;
		HitPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerHitEvent(HitPayload);

		TestTrue(TEXT("Hit consumed"), FrontAbility->IsTestDamageEventConsumed());
		TestFalse(TEXT("Release NOT flushed on Hit alone"), ExecContext->IsReleaseSent());
		TestFalse(TEXT("Victim NOT marked released on Hit alone"), ExecContext->IsVictimReleased());
		TestTrue(TEXT("Victim still locked after Hit"), EnemyASC->HasMatchingGameplayTag(VictimLockedTag));

		// Step C: Legitimate VictimStart arrives and performs the single authoritative release handoff
		FGameplayEventData StartPayload;
		StartPayload.EventTag = VictimStartTag;
		StartPayload.Instigator = Player;
		StartPayload.Target = Player;
		StartPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerVictimStartEvent(StartPayload);

		TestTrue(TEXT("Release sent on VictimStart"), ExecContext->IsReleaseSent());
		TestTrue(TEXT("Victim marked released on VictimStart"), ExecContext->IsVictimReleased());
		TestFalse(TEXT("Victim lock removed"), EnemyASC->HasMatchingGameplayTag(VictimLockedTag));

		FrontAbility->TestEndAbility(false);
		CleanupFrontExec(FrontHandle, VictimHandle);
	}

	// =========================================================================
	// 7. Non-Lethal Release With Unconfigured Montage Cleans Up With Zero Enemy Launch Dispatch
	// =========================================================================
	{
		Enemy->RestorePoiseToMax();
		Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

		auto [FrontHandle, FrontAbility, VictimHandle, VictimAbility] = SetupFrontAndVictimExec(100.0f);
		if (!TestNotNull(TEXT("FrontAbility instance exists"), FrontAbility) ||
			!TestTrue(TEXT("Front ability activated"), FrontAbility->IsActive()))
		{
			return false;
		}

		bool bLaunchReactionReceived = false;
		const FGameplayTag LaunchEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Enemy.Launch")), false);
		FDelegateHandle LaunchHandle = EnemyASC->GenericGameplayEventCallbacks.FindOrAdd(LaunchEventTag)
			.AddLambda([&bLaunchReactionReceived](const FGameplayEventData* InPayload) { bLaunchReactionReceived = true; });

		// Step A: Hit
		FGameplayEventData HitPayload;
		HitPayload.EventTag = FrontHitTag;
		HitPayload.Instigator = Player;
		HitPayload.Target = Player;
		HitPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerHitEvent(HitPayload);

		// Step B: VictimStart (triggers release handoff; zero fallback launch)
		FGameplayEventData StartPayload;
		StartPayload.EventTag = VictimStartTag;
		StartPayload.Instigator = Player;
		StartPayload.Target = Player;
		StartPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerVictimStartEvent(StartPayload);

		TestFalse(TEXT("Launch reaction NEVER dispatched on victim release"), bLaunchReactionReceived);
		TestFalse(TEXT("Enemy dead tag NOT applied"), EnemyASC->HasMatchingGameplayTag(DeadTag));
		TestEqual(TEXT("Enemy movement mode restored to walking"), Enemy->GetCharacterMovement()->MovementMode.GetValue(), MOVE_Walking);

		FrontAbility->TestEndAbility(false);
		EnemyASC->GenericGameplayEventCallbacks.FindOrAdd(LaunchEventTag).Remove(LaunchHandle);
		CleanupFrontExec(FrontHandle, VictimHandle);
	}

	// =========================================================================
	// 8. Lethal Execution Delayed Death & Ragdoll Commit On VictimStart
	// =========================================================================
	{
		Enemy->RestorePoiseToMax();
		Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

		auto [FrontHandle, FrontAbility, VictimHandle, VictimAbility] = SetupFrontAndVictimExec(1.0f);
		if (!TestNotNull(TEXT("FrontAbility instance exists"), FrontAbility) ||
			!TestTrue(TEXT("Front ability activated"), FrontAbility->IsActive()))
		{
			return false;
		}

		UExecutionLockContext* ExecContext = FrontAbility->GetTestExecutionContext();
		if (!TestNotNull(TEXT("ExecContext exists"), ExecContext))
		{
			return false;
		}

		// Step A: Hit causes lethal drop -> death pending (remains living pending blade pull)
		FGameplayEventData HitPayload;
		HitPayload.EventTag = FrontHitTag;
		HitPayload.Instigator = Player;
		HitPayload.Target = Player;
		HitPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerHitEvent(HitPayload);

		TestTrue(TEXT("Enemy is DeathPending"), ExecContext->IsDeathPending());
		TestFalse(TEXT("Enemy NOT yet dead before VictimStart"), Enemy->IsDead());
		TestFalse(TEXT("Enemy NOT dead tag before VictimStart"), EnemyASC->HasMatchingGameplayTag(DeadTag));

		// Step B: VictimStart arrives at blade extraction -> commits death and ragdoll
		FGameplayEventData StartPayload;
		StartPayload.EventTag = VictimStartTag;
		StartPayload.Instigator = Player;
		StartPayload.Target = Player;
		StartPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerVictimStartEvent(StartPayload);

		TestTrue(TEXT("Enemy is now dead after VictimStart commit"), Enemy->IsDead());
		TestTrue(TEXT("Enemy has State.Status.Dead tag"), EnemyASC->HasMatchingGameplayTag(DeadTag));
		TestTrue(TEXT("Context finalized successfully"), ExecContext->IsFinalized());

		// Step C: Player montage tail finishes without being interrupted by enemy death
		TestTrue(TEXT("Player execution ability still active for montage tail after enemy death"), FrontAbility->IsActive());
		FrontAbility->TestEndAbility(false);
		TestFalse(TEXT("Player execution finished cleanly"), FrontAbility->IsActive());

		CleanupFrontExec(FrontHandle, VictimHandle);
		if (Enemy)
		{
			Enemy->Destroy();
			Enemy = nullptr;
		}
	}

	// =========================================================================
	// 9. Backstab Execution Release Parity Test
	// =========================================================================
	{
		// Spawn a second fresh enemy for backstab test
		AEnemyCharacter* BackstabEnemy = FCombatAutomationFixture::SpawnPassiveEnemy(
			World,
			FTransform(FRotator::ZeroRotator, FVector(100.0f, 0.0f, 0.0f)));
		Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
		Player->SetActorRotation(FRotator::ZeroRotator);
		Player->SetTestCombatTeamTag(TagTeamPlayer);
		BackstabEnemy->SetTestCombatTeamTag(TagTeamEnemy);
		BackstabEnemy->SetActorLocation(FVector(100.0f, 0.0f, 0.0f));
		BackstabEnemy->SetActorRotation(FRotator::ZeroRotator); // Facing away from player (+X)

		Player->SetTestLockedTarget(BackstabEnemy);

		UAbilitySystemComponent* BackstabEnemyASC = BackstabEnemy ? BackstabEnemy->GetAbilitySystemComponent() : nullptr;
		if (!TestNotNull(TEXT("BackstabEnemy ASC valid"), BackstabEnemyASC))
		{
			return false;
		}

		FGameplayAbilitySpec BackstabSpec(UPlayerBackstabExecutionAbility::StaticClass(), 1, INDEX_NONE, Player);
		const FGameplayAbilitySpecHandle BackstabHandle = PlayerASC->GiveAbility(BackstabSpec);
		FGameplayAbilitySpec* FoundBackstabSpec = PlayerASC->FindAbilitySpecFromHandle(BackstabHandle);
		UPlayerBackstabExecutionAbility* BackstabAbility = FoundBackstabSpec ? Cast<UPlayerBackstabExecutionAbility>(FoundBackstabSpec->GetPrimaryInstance()) : nullptr;
		if (!TestNotNull(TEXT("BackstabAbility instance valid"), BackstabAbility))
		{
			return false;
		}

		BackstabAbility->SetTestExecutionMontage(PlayerExecutionMontage);
		BackstabAbility->SetTestDamageGameplayEffectClass(UTestProjectileDamageGE::StaticClass());
		BackstabAbility->SetTestExecutionDistances(50.0f, 250.0f);
		BackstabAbility->SetTestMaxBackAngleDegrees(90.0f);
		BackstabAbility->SetTestSkipMontageTaskActivation(true);

		auto [VictimHandle, VictimAbility] = GrantAndConfigureVictimAbility(BackstabEnemy);

		PlayerASC->TryActivateAbility(BackstabHandle);
		if (!TestTrue(TEXT("Backstab ability activated"), BackstabAbility->IsActive()))
		{
			return false;
		}

		UExecutionLockContext* ExecContext = BackstabAbility->GetTestExecutionContext();
		if (!TestNotNull(TEXT("Backstab ExecContext exists"), ExecContext))
		{
			return false;
		}

		// Step A: Hit
		const FGameplayTag BackstabHitTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Hit")), false);
		FGameplayEventData HitPayload;
		HitPayload.EventTag = BackstabHitTag;
		HitPayload.Instigator = Player;
		HitPayload.Target = Player;
		HitPayload.OptionalObject = PlayerExecutionMontage;
		BackstabAbility->TestTriggerHitEvent(HitPayload);

		TestTrue(TEXT("Backstab hit consumed"), BackstabAbility->IsTestDamageEventConsumed());
		TestEqual(TEXT("Backstab hit state is NonLethal"), ExecContext->GetHitState(), EExecutionSessionHitState::NonLethal);

		// Step B: VictimStart
		FGameplayEventData StartPayload;
		StartPayload.EventTag = VictimStartTag;
		StartPayload.Instigator = Player;
		StartPayload.Target = Player;
		StartPayload.OptionalObject = PlayerExecutionMontage;
		BackstabAbility->TestTriggerVictimStartEvent(StartPayload);

		TestTrue(TEXT("Backstab release sent to victim"), ExecContext->IsReleaseSent());
		TestTrue(TEXT("Backstab victim marked released"), ExecContext->IsVictimReleased());
		TestTrue(TEXT("Backstab player ability alive for tail"), BackstabAbility->IsActive());

		BackstabAbility->TestEndAbility(false);
		TestFalse(TEXT("Backstab ability ended"), BackstabAbility->IsActive());

		Player->TestClearLockedTarget();
		PlayerASC->ClearAbility(BackstabHandle);
		BackstabEnemyASC->ClearAbility(VictimHandle);
		if (BackstabEnemy)
		{
			BackstabEnemy->Destroy();
			BackstabEnemy = nullptr;
		}

		if (UWeaponEquipmentComponent* EquipComp = Player->FindComponentByClass<UWeaponEquipmentComponent>())
		{
			if (UMeleeWeaponDefinition* WeaponDef = EquipComp->GetEquippedMainHandMelee())
			{
				WeaponDef->MinExecutionDistance = 0.0f;
				WeaponDef->MaxExecutionDistance = 250.0f;
				WeaponDef->ExecutionSnapDistance = 190.0f;
				WeaponDef->FrontExecutionMontage = nullptr;
				WeaponDef->BackstabExecutionMontage = nullptr;
			}
		}
	}

	// =========================================================================
	// 10. Commit Failure Does Not Taint Victim Or Establish Lock
	// =========================================================================
	{
		// Spawn a fresh living enemy since Section 8 committed lethal execution death on the prior enemy
		Enemy = FCombatAutomationFixture::SpawnPassiveEnemy(
			World,
			FTransform(FRotator(0.0f, 180.0f, 0.0f), FVector(150.0f, 0.0f, 0.0f)));
		Enemy->SetTestCombatTeamTag(TagTeamEnemy);
		EnemyASC = Enemy->GetAbilitySystemComponent();
		Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

		Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
		Player->SetActorRotation(FRotator::ZeroRotator);

		if (UCharacterAttributeSet* EnemyAttribs = const_cast<UCharacterAttributeSet*>(EnemyASC->GetSet<UCharacterAttributeSet>()))
		{
			EnemyAttribs->SetHealth(100.0f);
			EnemyAttribs->SetPoise(0.0f);
		}

		ExecutionReleaseOutcomesAutomation::ActivateEnemyStanceBreak(Enemy);
		const FGameplayTag StunnedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
		if (!EnemyASC->HasMatchingGameplayTag(StunnedTag))
		{
			EnemyASC->AddLooseGameplayTag(StunnedTag);
		}

		Player->SetTestLockedTarget(Enemy);

		auto [VictimHandle, VictimAbility] = GrantAndConfigureVictimAbility(Enemy);

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
			FrontAbility->SetTestForceCommitAbilityFailure(true);
		}

		const EMovementMode ModeBeforeExec = Enemy->GetCharacterMovement()->MovementMode;

		PlayerASC->TryActivateAbility(FrontHandle);
		TestFalse(TEXT("Front ability is not active due to CommitAbility failure"), FrontAbility && FrontAbility->IsActive());
		TestFalse(TEXT("Victim has no VictimLocked tag"), EnemyASC->HasMatchingGameplayTag(VictimLockedTag));
		TestFalse(TEXT("Victim has no Invulnerable tag"), EnemyASC->HasMatchingGameplayTag(InvulnerableTag));
		TestFalse(TEXT("Victim ability was never accepted/activated"), VictimAbility && VictimAbility->IsActive());
		TestEqual(TEXT("Enemy movement mode unchanged after commit failure"), Enemy->GetCharacterMovement()->MovementMode.GetValue(), ModeBeforeExec);

		CleanupFrontExec(FrontHandle, VictimHandle);
	}

	// =========================================================================
	// 11. Release Dispatch Failure Fallback Cleans Victim State
	// =========================================================================
	{
		auto [FrontHandle, FrontAbility, VictimHandle, VictimAbility] = SetupFrontAndVictimExec(100.0f);
		if (!TestNotNull(TEXT("FrontAbility valid"), FrontAbility) ||
			!TestTrue(TEXT("Front ability active in Section 11"), FrontAbility->IsActive()) ||
			!TestNotNull(TEXT("VictimAbility valid"), VictimAbility))
		{
			return false;
		}

		// Step A: Hit first
		FGameplayEventData HitPayload;
		HitPayload.EventTag = FrontHitTag;
		HitPayload.Instigator = Player;
		HitPayload.Target = Player;
		HitPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerHitEvent(HitPayload);

		TestTrue(TEXT("Hit resolved"), FrontAbility->IsTestDamageEventConsumed());

		// Invalidate victim's wait victim start task right before dispatch so victim cannot process and mark released
		VictimAbility->SetTestInvalidateWaitVictimStartTaskAfterReady(true);

		// Step B: Send VictimStart: Player dispatches, detects unconfirmed release, and invokes fail-closed fallback
		FGameplayEventData StartPayload;
		StartPayload.EventTag = VictimStartTag;
		StartPayload.Instigator = Player;
		StartPayload.Target = Player;
		StartPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerVictimStartEvent(StartPayload);

		// Victim ability must be safely ended by fallback and not left hanging locked
		TestFalse(TEXT("Victim ability is ended by fallback"), VictimAbility->IsActive());
		TestFalse(TEXT("Victim locked tag cleared by fallback"), EnemyASC->HasMatchingGameplayTag(VictimLockedTag));
		TestFalse(TEXT("Victim invulnerable tag cleared by fallback"), EnemyASC->HasMatchingGameplayTag(InvulnerableTag));
		TestFalse(TEXT("Player ability also ended on release dispatch failure"), FrontAbility->IsActive());

		CleanupFrontExec(FrontHandle, VictimHandle);
	}

	// =========================================================================
	// 12. Execution Cancellation / Interruption Does Not Trigger Launch
	// =========================================================================
	{
		Enemy->RestorePoiseToMax();
		Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

		auto [FrontHandle, FrontAbility, VictimHandle, VictimAbility] = SetupFrontAndVictimExec(100.0f);
		if (!TestNotNull(TEXT("FrontAbility valid"), FrontAbility) ||
			!TestTrue(TEXT("Front ability active in Section 12"), FrontAbility->IsActive()) ||
			!TestNotNull(TEXT("VictimAbility valid"), VictimAbility))
		{
			return false;
		}

		bool bLaunchReceivedOnCancel = false;
		const FGameplayTag LaunchEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Enemy.Launch")), false);
		FDelegateHandle LaunchHandle = EnemyASC->GenericGameplayEventCallbacks.FindOrAdd(LaunchEventTag)
			.AddLambda([&bLaunchReceivedOnCancel](const FGameplayEventData* InPayload) { bLaunchReceivedOnCancel = true; });

		// Cancel execution prematurely before hit/release
		FrontAbility->TestEndAbility(true);

		TestFalse(TEXT("Player execution ended on cancel"), FrontAbility->IsActive());
		TestFalse(TEXT("Victim ability ended on cancel"), VictimAbility->IsActive());
		TestFalse(TEXT("Launch reaction NEVER dispatched on cancel"), bLaunchReceivedOnCancel);
		TestFalse(TEXT("Victim lock tag removed"), EnemyASC->HasMatchingGameplayTag(VictimLockedTag));
		TestEqual(TEXT("Enemy movement restored to walking"), Enemy->GetCharacterMovement()->MovementMode.GetValue(), MOVE_Walking);

		EnemyASC->GenericGameplayEventCallbacks.FindOrAdd(LaunchEventTag).Remove(LaunchHandle);
		CleanupFrontExec(FrontHandle, VictimHandle);
	}

	// =========================================================================
	// 13. Natural BlendOut Retains Session, Only Completed Finishes Victim Recovery
	// =========================================================================
	{
		Enemy->RestorePoiseToMax();
		Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

		auto [FrontHandle, FrontAbility, VictimHandle, VictimAbility] = SetupFrontAndVictimExec(100.0f, true);
		if (!TestNotNull(TEXT("FrontAbility valid"), FrontAbility) ||
			!TestTrue(TEXT("Front ability active in Section 13"), FrontAbility->IsActive()) ||
			!TestNotNull(TEXT("VictimAbility valid"), VictimAbility))
		{
			return false;
		}

		// Step A: Hit arrives first (stab)
		FGameplayEventData HitPayload;
		HitPayload.EventTag = FrontHitTag;
		HitPayload.Instigator = Player;
		HitPayload.Target = Player;
		HitPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerHitEvent(HitPayload);

		TestTrue(TEXT("Damage consumed on Hit in Sec 13"), FrontAbility->IsTestDamageEventConsumed());

		// Step B: VictimStart arrives second (draw blade: starts recovery)
		FGameplayEventData VictimStartPayload;
		VictimStartPayload.EventTag = VictimStartTag;
		VictimStartPayload.Instigator = Player;
		VictimStartPayload.Target = Player;
		VictimStartPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerVictimStartEvent(VictimStartPayload);

		// Step C: Trigger montage BlendOut: locks must be retained
		VictimAbility->TestTriggerVictimMontageBlendOut();
		TestTrue(TEXT("Victim ability remains active during BlendOut"), VictimAbility->IsActive());
		TestTrue(TEXT("Victim remains locked during BlendOut"), EnemyASC->HasMatchingGameplayTag(VictimLockedTag));

		// Step D: Trigger montage completion: ability ends and locks cleared
		VictimAbility->TestTriggerVictimMontageCompleted();
		TestFalse(TEXT("Victim released upon montage completion"), VictimAbility->IsActive());
		TestFalse(TEXT("Victim lock removed upon montage completion"), EnemyASC->HasMatchingGameplayTag(VictimLockedTag));

		FrontAbility->TestEndAbility(false);
		CleanupFrontExec(FrontHandle, VictimHandle);
	}

	// =========================================================================
	// 14. Malformed / Stale / Duplicate Release Requests Fail-Closed
	// =========================================================================
	{
		Enemy->RestorePoiseToMax();
		Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

		auto [FrontHandle, FrontAbility, VictimHandle, VictimAbility] = SetupFrontAndVictimExec(100.0f);
		if (!TestNotNull(TEXT("FrontAbility valid"), FrontAbility) ||
			!TestTrue(TEXT("Front ability active in Section 14"), FrontAbility->IsActive()) ||
			!TestNotNull(TEXT("VictimAbility valid"), VictimAbility))
		{
			return false;
		}

		UExecutionLockContext* ExecContext = FrontAbility->GetTestExecutionContext();
		if (!TestNotNull(TEXT("ExecContext valid"), ExecContext))
		{
			return false;
		}

		// 1. Malformed VictimStart payload with wrong Instigator/Target
		FGameplayEventData MalformedPayload;
		MalformedPayload.EventTag = VictimStartTag;
		MalformedPayload.Instigator = Enemy; // Wrong!
		MalformedPayload.Target = Enemy;     // Wrong!
		MalformedPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerVictimStartEvent(MalformedPayload);

		TestFalse(TEXT("Release not sent on malformed VictimStart payload"), ExecContext->IsReleaseSent());
		TestFalse(TEXT("VictimStart not forwarded on malformed payload"), FrontAbility->IsTestVictimStartForwarded());

		// 2. Normal hit and VictimStart to perform release handoff
		FGameplayEventData HitPayload;
		HitPayload.EventTag = FrontHitTag;
		HitPayload.Instigator = Player;
		HitPayload.Target = Player;
		HitPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerHitEvent(HitPayload);

		FGameplayEventData ValidStartPayload;
		ValidStartPayload.EventTag = VictimStartTag;
		ValidStartPayload.Instigator = Player;
		ValidStartPayload.Target = Player;
		ValidStartPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerVictimStartEvent(ValidStartPayload);

		TestTrue(TEXT("Release sent on VictimStart"), ExecContext->IsReleaseSent());
		TestTrue(TEXT("Victim released on VictimStart"), ExecContext->IsVictimReleased());

		// 3. Duplicate release request attempt
		const bool bDuplicateSent = FrontAbility->SendFormalReleaseToVictim(false, true);
		TestFalse(TEXT("Duplicate formal release rejected"), bDuplicateSent);

		FrontAbility->TestEndAbility(false);
		CleanupFrontExec(FrontHandle, VictimHandle);
	}

	// =========================================================================
	// 15. Failed Hit Resolution Cleans Victim Ability And State Without Launch
	// =========================================================================
	{
		Enemy->RestorePoiseToMax();
		Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

		auto [FrontHandle, FrontAbility, VictimHandle, VictimAbility] = SetupFrontAndVictimExec(100.0f);
		if (!TestNotNull(TEXT("FrontAbility valid in Section 15"), FrontAbility) ||
			!TestTrue(TEXT("Front ability active in Section 15"), FrontAbility->IsActive()) ||
			!TestNotNull(TEXT("VictimAbility valid in Section 15"), VictimAbility))
		{
			return false;
		}

		UExecutionLockContext* ExecContext = FrontAbility->GetTestExecutionContext();
		if (!TestNotNull(TEXT("ExecContext valid in Section 15"), ExecContext))
		{
			return false;
		}

		TestTrue(TEXT("Victim ability active"), VictimAbility->IsActive());
		TestTrue(TEXT("Victim has VictimLocked tag"), EnemyASC->HasMatchingGameplayTag(VictimLockedTag));
		TestTrue(TEXT("Victim has Invulnerable tag"), EnemyASC->HasMatchingGameplayTag(InvulnerableTag));
		const FGameplayTag StunnedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
		TestTrue(TEXT("Victim has Stunned tag"), EnemyASC->HasMatchingGameplayTag(StunnedTag));

		// Track Launch Reaction event on Enemy (must NEVER be fired on failed hit)
		bool bLaunchReactionFired = false;
		const FGameplayTag LaunchEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Enemy.Launch")), false);
		FDelegateHandle LaunchHandle = EnemyASC->GenericGameplayEventCallbacks.FindOrAdd(LaunchEventTag)
			.AddLambda([&bLaunchReactionFired](const FGameplayEventData* InPayload)
			{
				bLaunchReactionFired = true;
			});

		// Temporarily invalidate modifier attribute on Damage GE CDO to fail ApplyGameplayEffectSpecToSelf
		UTestProjectileDamageGE* DamageCDO = UTestProjectileDamageGE::StaticClass()->GetDefaultObject<UTestProjectileDamageGE>();
		const FGameplayAttribute OriginalAttribute = DamageCDO->Modifiers.Num() > 0 ? DamageCDO->Modifiers[0].Attribute : FGameplayAttribute();
		if (DamageCDO->Modifiers.Num() > 0)
		{
			DamageCDO->Modifiers[0].Attribute = FGameplayAttribute();
		}

		// Trigger Hit event -> FMeleeHitResolver fails GE application -> HitState becomes Failed -> FrontAbility calls EndAbility
		FGameplayEventData HitPayload;
		HitPayload.EventTag = FrontHitTag;
		HitPayload.Instigator = Player;
		HitPayload.Target = Player;
		HitPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerHitEvent(HitPayload);

		// Immediately restore modifier attribute on Damage GE CDO
		if (DamageCDO->Modifiers.Num() > 0)
		{
			DamageCDO->Modifiers[0].Attribute = OriginalAttribute;
		}

		// Assertions:
		// 1. Player ability ended
		TestFalse(TEXT("Player execution ability ended on hit failure"), FrontAbility->IsActive());
		// 2. Victim ability ended
		TestFalse(TEXT("Victim ability ended on hit failure"), VictimAbility->IsActive());
		// 3. Victim tags cleaned up (no leak of VictimLocked, Invulnerable, Stunned)
		TestFalse(TEXT("VictimLocked tag removed on hit failure"), EnemyASC->HasMatchingGameplayTag(VictimLockedTag));
		TestFalse(TEXT("Invulnerable tag removed on hit failure"), EnemyASC->HasMatchingGameplayTag(InvulnerableTag));
		TestFalse(TEXT("Stunned tag removed on hit failure"), EnemyASC->HasMatchingGameplayTag(StunnedTag));
		// 4. Context no longer active, HitState is Failed
		TestFalse(TEXT("Context is no longer active"), ExecContext->IsActive());
		TestEqual(TEXT("HitState is Failed"), ExecContext->GetHitState(), EExecutionSessionHitState::Failed);
		// 5. No Launch event dispatched
		TestFalse(TEXT("Launch reaction never dispatched on hit failure"), bLaunchReactionFired);

		EnemyASC->GenericGameplayEventCallbacks.FindOrAdd(LaunchEventTag).Remove(LaunchHandle);
		CleanupFrontExec(FrontHandle, VictimHandle);
	}

	// =========================================================================
	// 12. Ground Check Rejection And Movement Mode Ownership Preservation
	// =========================================================================
	{
		Enemy->RestorePoiseToMax();
		Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

		// Case A: Enemy in mid-air (MOVE_None with no floor underneath)
		{
			auto [FrontHandle, FrontAbility, VictimHandle, VictimAbility] = SetupFrontAndVictimExec(100.0f, true);
			if (TestNotNull(TEXT("FrontAbility valid in Sec 12A"), FrontAbility) && TestNotNull(TEXT("VictimAbility valid in Sec 12A"), VictimAbility))
			{
				VictimAbility->SetTestBoundAnimInstance(MockAnimInstance);
				VictimAbility->SetTestBypassMontageActiveCheck(true);

				// Move enemy high into the air so FindFloor finds no floor
				Enemy->SetActorLocation(FVector(150.0f, 0.0f, 2000.0f));

				bool bLaunchDispatched = false;
				const FGameplayTag LaunchEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Enemy.Launch")), false);
				FDelegateHandle LaunchHandle = EnemyASC->GenericGameplayEventCallbacks.FindOrAdd(LaunchEventTag)
					.AddLambda([&bLaunchDispatched](const FGameplayEventData*) { bLaunchDispatched = true; });

				FGameplayEventData HitPayload;
				HitPayload.EventTag = FrontHitTag;
				HitPayload.Instigator = Player;
				HitPayload.Target = Player;
				HitPayload.OptionalObject = PlayerExecutionMontage;
				FrontAbility->TestTriggerHitEvent(HitPayload);

				FGameplayEventData StartPayload;
				StartPayload.EventTag = VictimStartTag;
				StartPayload.Instigator = Player;
				StartPayload.Target = Player;
				StartPayload.OptionalObject = PlayerExecutionMontage;
				FrontAbility->TestTriggerVictimStartEvent(StartPayload);

				TestFalse(TEXT("Victim presentation failed handoff when enemy is in mid-air"), VictimAbility->IsTestVictimPresentationStarted());
				TestFalse(TEXT("Victim ability ended safely after failed floor check"), VictimAbility->IsActive());
				TestFalse(TEXT("Launch reaction NEVER dispatched on mid-air failure"), bLaunchDispatched);
				// In mid-air, EndAbility restores movement mode to Falling because FindFloor finds no floor
				TestEqual(TEXT("Enemy movement mode restored to Falling in mid-air"), Enemy->GetCharacterMovement()->MovementMode, MOVE_Falling);

				// Reset enemy location back to ground
				Enemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
				Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

				FrontAbility->TestEndAbility(false);
				EnemyASC->GenericGameplayEventCallbacks.FindOrAdd(LaunchEventTag).Remove(LaunchHandle);
				CleanupFrontExec(FrontHandle, VictimHandle);
			}
		}

		// Case B: Enemy externally in MOVE_Falling mode preserves mode and rejects handoff
		{
			Enemy->RestorePoiseToMax();
			Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

			auto [FrontHandle, FrontAbility, VictimHandle, VictimAbility] = SetupFrontAndVictimExec(100.0f, true);
			if (TestNotNull(TEXT("FrontAbility valid in Sec 12B"), FrontAbility) && TestNotNull(TEXT("VictimAbility valid in Sec 12B"), VictimAbility))
			{
				VictimAbility->SetTestBoundAnimInstance(MockAnimInstance);
				VictimAbility->SetTestBypassMontageActiveCheck(true);

				// Simulate external state changing movement mode to Falling
				Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Falling);

				bool bLaunchDispatched = false;
				const FGameplayTag LaunchEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Enemy.Launch")), false);
				FDelegateHandle LaunchHandle = EnemyASC->GenericGameplayEventCallbacks.FindOrAdd(LaunchEventTag)
					.AddLambda([&bLaunchDispatched](const FGameplayEventData*) { bLaunchDispatched = true; });

				FGameplayEventData HitPayload;
				HitPayload.EventTag = FrontHitTag;
				HitPayload.Instigator = Player;
				HitPayload.Target = Player;
				HitPayload.OptionalObject = PlayerExecutionMontage;
				FrontAbility->TestTriggerHitEvent(HitPayload);

				FGameplayEventData StartPayload;
				StartPayload.EventTag = VictimStartTag;
				StartPayload.Instigator = Player;
				StartPayload.Target = Player;
				StartPayload.OptionalObject = PlayerExecutionMontage;
				FrontAbility->TestTriggerVictimStartEvent(StartPayload);

				TestFalse(TEXT("Victim presentation rejected when enemy is in MOVE_Falling"), VictimAbility->IsTestVictimPresentationStarted());
				TestFalse(TEXT("Victim ability ended safely"), VictimAbility->IsActive());
				TestFalse(TEXT("Launch reaction NEVER dispatched on Falling rejection"), bLaunchDispatched);
				// External Falling mode preserved, NOT overwritten to Walking
				TestEqual(TEXT("Enemy movement mode remains Falling (external mode preserved)"), Enemy->GetCharacterMovement()->MovementMode, MOVE_Falling);

				Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

				FrontAbility->TestEndAbility(false);
				EnemyASC->GenericGameplayEventCallbacks.FindOrAdd(LaunchEventTag).Remove(LaunchHandle);
				CleanupFrontExec(FrontHandle, VictimHandle);
			}
		}

		// Case C: Enemy externally in MOVE_Flying mode on valid ground rejects handoff and preserves mode and velocity
		{
			Enemy->RestorePoiseToMax();
			Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

			auto [FrontHandle, FrontAbility, VictimHandle, VictimAbility] = SetupFrontAndVictimExec(100.0f, true);
			if (TestNotNull(TEXT("FrontAbility valid in Sec 12C"), FrontAbility) && TestNotNull(TEXT("VictimAbility valid in Sec 12C"), VictimAbility))
			{
				VictimAbility->SetTestBoundAnimInstance(MockAnimInstance);
				VictimAbility->SetTestBypassMontageActiveCheck(true);

				// Enemy is on ground, but externally in MOVE_Flying with non-zero velocity
				Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
				const FVector PreservedFlyingVelocity(150.0f, 0.0f, 0.0f);
				Enemy->GetCharacterMovement()->Velocity = PreservedFlyingVelocity;

				bool bLaunchDispatched = false;
				const FGameplayTag LaunchEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Enemy.Launch")), false);
				FDelegateHandle LaunchHandle = EnemyASC->GenericGameplayEventCallbacks.FindOrAdd(LaunchEventTag)
					.AddLambda([&bLaunchDispatched](const FGameplayEventData*) { bLaunchDispatched = true; });

				FGameplayEventData HitPayload;
				HitPayload.EventTag = FrontHitTag;
				HitPayload.Instigator = Player;
				HitPayload.Target = Player;
				HitPayload.OptionalObject = PlayerExecutionMontage;
				FrontAbility->TestTriggerHitEvent(HitPayload);

				FGameplayEventData StartPayload;
				StartPayload.EventTag = VictimStartTag;
				StartPayload.Instigator = Player;
				StartPayload.Target = Player;
				StartPayload.OptionalObject = PlayerExecutionMontage;
				FrontAbility->TestTriggerVictimStartEvent(StartPayload);

				TestFalse(TEXT("Victim presentation rejected when enemy is in MOVE_Flying"), VictimAbility->IsTestVictimPresentationStarted());
				TestFalse(TEXT("Victim ability ended safely on Flying rejection"), VictimAbility->IsActive());
				TestFalse(TEXT("Launch reaction NEVER dispatched on Flying rejection"), bLaunchDispatched);
				TestEqual(TEXT("Enemy movement mode remains Flying (not forced to Walking)"), Enemy->GetCharacterMovement()->MovementMode, MOVE_Flying);
				TestEqual(TEXT("Enemy velocity preserved under Flying rejection"), Enemy->GetCharacterMovement()->Velocity, PreservedFlyingVelocity);

				Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
				Enemy->GetCharacterMovement()->Velocity = FVector::ZeroVector;

				FrontAbility->TestEndAbility(false);
				EnemyASC->GenericGameplayEventCallbacks.FindOrAdd(LaunchEventTag).Remove(LaunchHandle);
				CleanupFrontExec(FrontHandle, VictimHandle);
			}
		}

		// Case D: Enemy externally in MOVE_Custom mode on valid ground rejects handoff and preserves mode and velocity
		{
			Enemy->RestorePoiseToMax();
			Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

			auto [FrontHandle, FrontAbility, VictimHandle, VictimAbility] = SetupFrontAndVictimExec(100.0f, true);
			if (TestNotNull(TEXT("FrontAbility valid in Sec 12D"), FrontAbility) && TestNotNull(TEXT("VictimAbility valid in Sec 12D"), VictimAbility))
			{
				VictimAbility->SetTestBoundAnimInstance(MockAnimInstance);
				VictimAbility->SetTestBypassMontageActiveCheck(true);

				// Enemy is on ground, but externally in MOVE_Custom with custom submode and non-zero velocity
				Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Custom, 3);
				const FVector PreservedCustomVelocity(0.0f, 200.0f, 0.0f);
				Enemy->GetCharacterMovement()->Velocity = PreservedCustomVelocity;

				bool bLaunchDispatched = false;
				const FGameplayTag LaunchEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Enemy.Launch")), false);
				FDelegateHandle LaunchHandle = EnemyASC->GenericGameplayEventCallbacks.FindOrAdd(LaunchEventTag)
					.AddLambda([&bLaunchDispatched](const FGameplayEventData*) { bLaunchDispatched = true; });

				FGameplayEventData HitPayload;
				HitPayload.EventTag = FrontHitTag;
				HitPayload.Instigator = Player;
				HitPayload.Target = Player;
				HitPayload.OptionalObject = PlayerExecutionMontage;
				FrontAbility->TestTriggerHitEvent(HitPayload);

				FGameplayEventData StartPayload;
				StartPayload.EventTag = VictimStartTag;
				StartPayload.Instigator = Player;
				StartPayload.Target = Player;
				StartPayload.OptionalObject = PlayerExecutionMontage;
				FrontAbility->TestTriggerVictimStartEvent(StartPayload);

				TestFalse(TEXT("Victim presentation rejected when enemy is in MOVE_Custom"), VictimAbility->IsTestVictimPresentationStarted());
				TestFalse(TEXT("Victim ability ended safely on Custom rejection"), VictimAbility->IsActive());
				TestFalse(TEXT("Launch reaction NEVER dispatched on Custom rejection"), bLaunchDispatched);
				TestEqual(TEXT("Enemy movement mode remains Custom (not forced to Walking)"), Enemy->GetCharacterMovement()->MovementMode, MOVE_Custom);
				TestEqual(TEXT("Custom submode preserved under Custom rejection"), Enemy->GetCharacterMovement()->CustomMovementMode, (uint8)3);
				TestEqual(TEXT("Enemy velocity preserved under Custom rejection"), Enemy->GetCharacterMovement()->Velocity, PreservedCustomVelocity);

				Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
				Enemy->GetCharacterMovement()->Velocity = FVector::ZeroVector;

				FrontAbility->TestEndAbility(false);
				EnemyASC->GenericGameplayEventCallbacks.FindOrAdd(LaunchEventTag).Remove(LaunchHandle);
				CleanupFrontExec(FrontHandle, VictimHandle);
			}
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
