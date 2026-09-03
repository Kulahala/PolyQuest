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
#include "Animation/Combat/AnimNotify_PlayerExecutionRelease.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Character/Player/PlayerCharacter.h"
#include "Combat/Equipment/MeleeWeaponDefinition.h"
#include "Combat/Equipment/WeaponEquipmentComponent.h"
#include "Combat/Execution/ExecutionLockContext.h"
#include "Combat/Melee/MeleeHitResolver.h"
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
	// =========================================================================
	// 1. CDO & Tag Registration Verification
	// =========================================================================
	{
		const FGameplayTag ReleaseRequestTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.Release")), false);
		TestTrue(TEXT("Tag Event.Action.Execution.Request.Release is registered and valid"), ReleaseRequestTag.IsValid());

		const FGameplayTag ReleaseFormalTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Release")), false);
		TestTrue(TEXT("Tag Event.Action.Execution.Release is registered and valid"), ReleaseFormalTag.IsValid());

		const FGameplayTag LaunchReactionTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Enemy.Launch")), false);
		TestTrue(TEXT("Tag Event.Reaction.Enemy.Launch is registered and valid"), LaunchReactionTag.IsValid());

		const UEnemyVictimExecutionAbility* VictimCDO = UEnemyVictimExecutionAbility::StaticClass()->GetDefaultObject<UEnemyVictimExecutionAbility>();
		if (TestNotNull(TEXT("UEnemyVictimExecutionAbility CDO exists"), VictimCDO))
		{
			TestTrue(TEXT("Victim CDO default bLaunchNonLethalOnRelease is true"), VictimCDO->GetTestLaunchNonLethalOnRelease());
		}
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

	const FGameplayTag ReleaseRequestTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.Release")), false);
	const FGameplayTag FrontHitTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Front.Hit")), false);
	const FGameplayTag VictimLockedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Execution.VictimLocked")), false);
	const FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
	const FGameplayTag InvulnerableTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Invulnerable")), false);

	// Setup mock animations
	UAnimMontage* PlayerExecutionMontage = NewObject<UAnimMontage>(GetTransientPackage());
	UAnimMontage* EnemyVictimMontage = NewObject<UAnimMontage>(GetTransientPackage());

	auto GrantAndConfigureVictimAbility = [&](AEnemyCharacter* InEnemy, bool bConfigureVictimMontage = false) -> TPair<FGameplayAbilitySpecHandle, UEnemyVictimExecutionAbility*>
	{
		UAbilitySystemComponent* TargetASC = InEnemy->GetAbilitySystemComponent();
		FGameplayAbilitySpec VictimSpec(UEnemyVictimExecutionAbility::StaticClass(), 1, INDEX_NONE, InEnemy);
		const FGameplayAbilitySpecHandle VictimHandle = TargetASC->GiveAbility(VictimSpec);
		FGameplayAbilitySpec* FoundSpec = TargetASC->FindAbilitySpecFromHandle(VictimHandle);
		UEnemyVictimExecutionAbility* Instance = FoundSpec ? Cast<UEnemyVictimExecutionAbility>(FoundSpec->GetPrimaryInstance()) : nullptr;
		if (Instance)
		{
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

	auto SetupFrontAndVictimExec = [&](float InEnemyHealth = 100.0f, bool bLaunchOnRelease = true, bool bConfigureVictimMontage = false) -> TTuple<FGameplayAbilitySpecHandle, UPlayerFrontExecutionAbility*, FGameplayAbilitySpecHandle, UEnemyVictimExecutionAbility*>
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
		if (VictimAbility)
		{
			VictimAbility->SetTestLaunchNonLethalOnRelease(bLaunchOnRelease);
		}

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
	};

	// =========================================================================
	// 4. AnimNotify_PlayerExecutionRelease Payload Verification
	// =========================================================================
	{
		UAnimNotify_PlayerExecutionRelease* ReleaseNotify = NewObject<UAnimNotify_PlayerExecutionRelease>();
		TestNotNull(TEXT("ReleaseNotify created"), ReleaseNotify);

		FGameplayTag CapturedEventTag;
		FGameplayEventData CapturedPayload;
		FDelegateHandle Handle = PlayerASC->GenericGameplayEventCallbacks.FindOrAdd(ReleaseRequestTag)
			.AddLambda([&CapturedEventTag, &CapturedPayload](const FGameplayEventData* InPayload)
			{
				if (InPayload)
				{
					CapturedEventTag = InPayload->EventTag;
					CapturedPayload = *InPayload;
				}
			});

		ReleaseNotify->Notify(Player->GetMesh(), PlayerExecutionMontage, FAnimNotifyEventReference());

		TestEqual(TEXT("Notify sent ReleaseRequest tag"), CapturedEventTag, ReleaseRequestTag);
		TestTrue(TEXT("Notify payload Instigator is Player"), CapturedPayload.Instigator.Get() == Player);
		TestTrue(TEXT("Notify payload Target is Player"), CapturedPayload.Target.Get() == Player);
		TestTrue(TEXT("Notify payload OptionalObject is Animation"), CapturedPayload.OptionalObject.Get() == PlayerExecutionMontage);

		PlayerASC->GenericGameplayEventCallbacks.FindOrAdd(ReleaseRequestTag).Remove(Handle);
	}

	// =========================================================================
	// 5. Orderly Hit -> Release Sequence (NonLethal, Launch Reaction Fired)
	// =========================================================================
	{
		auto [FrontHandle, FrontAbility, VictimHandle, VictimAbility] = SetupFrontAndVictimExec(100.0f, true);
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

		// Step A: Hit arrives first
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

		// Step B: Release request arrives second
		FGameplayEventData ReleaseReqPayload;
		ReleaseReqPayload.EventTag = ReleaseRequestTag;
		ReleaseReqPayload.Instigator = Player;
		ReleaseReqPayload.Target = Player;
		ReleaseReqPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerReleaseRequestEvent(ReleaseReqPayload);

		TestTrue(TEXT("Release sent to victim"), ExecContext->IsReleaseSent());
		TestTrue(TEXT("Victim released"), ExecContext->IsVictimReleased());
		TestFalse(TEXT("Victim locked tag removed on release"), EnemyASC->HasMatchingGameplayTag(VictimLockedTag));
		TestTrue(TEXT("Launch reaction event dispatched to enemy with correct instigator and target"), bLaunchReactionReceived);
		TestTrue(TEXT("Player execution ability remains active for montage tail"), FrontAbility->IsActive());

		// Step C: Player montage finishes naturally
		FrontAbility->TestEndAbility(false);
		TestFalse(TEXT("Player execution ended naturally"), FrontAbility->IsActive());
		TestFalse(TEXT("Context invalidated after player end"), ExecContext->IsActive());

		EnemyASC->GenericGameplayEventCallbacks.FindOrAdd(LaunchEventTag).Remove(LaunchHandle);
		CleanupFrontExec(FrontHandle, VictimHandle);
	}

	// =========================================================================
	// 6. Reverse Sequence: Release Request Arrives Before Hit (Latch & Flush)
	// =========================================================================
	{
		Enemy->RestorePoiseToMax();
		Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

		auto [FrontHandle, FrontAbility, VictimHandle, VictimAbility] = SetupFrontAndVictimExec(100.0f, true);
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

		// Step A: Release request arrives early before hit
		FGameplayEventData ReleaseReqPayload;
		ReleaseReqPayload.EventTag = ReleaseRequestTag;
		ReleaseReqPayload.Instigator = Player;
		ReleaseReqPayload.Target = Player;
		ReleaseReqPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerReleaseRequestEvent(ReleaseReqPayload);

		TestTrue(TEXT("Release request latched on player ability"), FrontAbility->IsTestReleaseRequestLatched());
		TestFalse(TEXT("Release NOT sent yet to victim"), ExecContext->IsReleaseSent());
		TestTrue(TEXT("Victim remains locked"), EnemyASC->HasMatchingGameplayTag(VictimLockedTag));

		// Step B: Hit arrives and flushes latched release
		FGameplayEventData HitPayload;
		HitPayload.EventTag = FrontHitTag;
		HitPayload.Instigator = Player;
		HitPayload.Target = Player;
		HitPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerHitEvent(HitPayload);

		TestTrue(TEXT("Hit consumed"), FrontAbility->IsTestDamageEventConsumed());
		TestTrue(TEXT("Release flushed to victim upon hit resolution"), ExecContext->IsReleaseSent());
		TestTrue(TEXT("Victim marked released"), ExecContext->IsVictimReleased());
		TestFalse(TEXT("Victim lock removed"), EnemyASC->HasMatchingGameplayTag(VictimLockedTag));

		FrontAbility->TestEndAbility(false);
		CleanupFrontExec(FrontHandle, VictimHandle);
	}

	// =========================================================================
	// 7. Non-Lethal Release Without Launch Reaction (Toggle Off)
	// =========================================================================
	{
		Enemy->RestorePoiseToMax();
		Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

		auto [FrontHandle, FrontAbility, VictimHandle, VictimAbility] = SetupFrontAndVictimExec(100.0f, false);
		if (!TestNotNull(TEXT("FrontAbility instance exists"), FrontAbility) ||
			!TestTrue(TEXT("Front ability activated"), FrontAbility->IsActive()))
		{
			return false;
		}

		bool bLaunchReactionReceived = false;
		const FGameplayTag LaunchEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Enemy.Launch")), false);
		FDelegateHandle LaunchHandle = EnemyASC->GenericGameplayEventCallbacks.FindOrAdd(LaunchEventTag)
			.AddLambda([&bLaunchReactionReceived](const FGameplayEventData* InPayload) { bLaunchReactionReceived = true; });

		// Hit
		FGameplayEventData HitPayload;
		HitPayload.EventTag = FrontHitTag;
		HitPayload.Instigator = Player;
		HitPayload.Target = Player;
		HitPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerHitEvent(HitPayload);

		// Release
		FGameplayEventData ReleaseReqPayload;
		ReleaseReqPayload.EventTag = ReleaseRequestTag;
		ReleaseReqPayload.Instigator = Player;
		ReleaseReqPayload.Target = Player;
		ReleaseReqPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerReleaseRequestEvent(ReleaseReqPayload);

		TestFalse(TEXT("Launch reaction NOT dispatched when bLaunchNonLethalOnRelease is false"), bLaunchReactionReceived);
		TestFalse(TEXT("Enemy dead tag NOT applied"), EnemyASC->HasMatchingGameplayTag(DeadTag));
		TestEqual(TEXT("Enemy movement mode restored to walking"), Enemy->GetCharacterMovement()->MovementMode.GetValue(), MOVE_Walking);

		FrontAbility->TestEndAbility(false);
		EnemyASC->GenericGameplayEventCallbacks.FindOrAdd(LaunchEventTag).Remove(LaunchHandle);
		CleanupFrontExec(FrontHandle, VictimHandle);
	}

	// =========================================================================
	// 8. Lethal Execution Delayed Death & Ragdoll Commit On Release
	// =========================================================================
	{
		Enemy->RestorePoiseToMax();
		Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

		auto [FrontHandle, FrontAbility, VictimHandle, VictimAbility] = SetupFrontAndVictimExec(1.0f, true);
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

		// Step A: Hit causes lethal drop -> death pending
		FGameplayEventData HitPayload;
		HitPayload.EventTag = FrontHitTag;
		HitPayload.Instigator = Player;
		HitPayload.Target = Player;
		HitPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerHitEvent(HitPayload);

		TestTrue(TEXT("Enemy is DeathPending"), ExecContext->IsDeathPending());
		TestFalse(TEXT("Enemy NOT yet dead before Release"), Enemy->IsDead());
		TestFalse(TEXT("Enemy NOT dead tag before Release"), EnemyASC->HasMatchingGameplayTag(DeadTag));

		// Step B: Release triggers CommitExecutionDeath
		FGameplayEventData ReleaseReqPayload;
		ReleaseReqPayload.EventTag = ReleaseRequestTag;
		ReleaseReqPayload.Instigator = Player;
		ReleaseReqPayload.Target = Player;
		ReleaseReqPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerReleaseRequestEvent(ReleaseReqPayload);

		TestTrue(TEXT("Enemy is now dead after Release commit"), Enemy->IsDead());
		TestTrue(TEXT("Enemy has State.Status.Dead tag"), EnemyASC->HasMatchingGameplayTag(DeadTag));
		TestTrue(TEXT("Context finalized successfully"), ExecContext->IsFinalized());

		// Step C: Player montage tail finishes without being interrupted by enemy death
		TestTrue(TEXT("Player execution ability still active for montage tail after enemy death"), FrontAbility->IsActive());
		FrontAbility->TestEndAbility(false);
		TestFalse(TEXT("Player execution finished cleanly"), FrontAbility->IsActive());

		CleanupFrontExec(FrontHandle, VictimHandle);
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
		BackstabAbility->SetTestExecutionDistances(50.0f, 300.0f);
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

		// Hit
		const FGameplayTag BackstabHitTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Backstab.Hit")), false);
		FGameplayEventData HitPayload;
		HitPayload.EventTag = BackstabHitTag;
		HitPayload.Instigator = Player;
		HitPayload.Target = Player;
		HitPayload.OptionalObject = PlayerExecutionMontage;
		BackstabAbility->TestTriggerHitEvent(HitPayload);

		TestTrue(TEXT("Backstab hit consumed"), BackstabAbility->IsTestDamageEventConsumed());
		TestEqual(TEXT("Backstab hit state is NonLethal"), ExecContext->GetHitState(), EExecutionSessionHitState::NonLethal);

		// Release
		FGameplayEventData ReleaseReqPayload;
		ReleaseReqPayload.EventTag = ReleaseRequestTag;
		ReleaseReqPayload.Instigator = Player;
		ReleaseReqPayload.Target = Player;
		ReleaseReqPayload.OptionalObject = PlayerExecutionMontage;
		BackstabAbility->TestTriggerReleaseRequestEvent(ReleaseReqPayload);

		TestTrue(TEXT("Backstab release sent to victim"), ExecContext->IsReleaseSent());
		TestTrue(TEXT("Backstab victim marked released"), ExecContext->IsVictimReleased());
		TestTrue(TEXT("Backstab player ability alive for tail"), BackstabAbility->IsActive());

		BackstabAbility->TestEndAbility(false);
		TestFalse(TEXT("Backstab ability ended"), BackstabAbility->IsActive());

		Player->TestClearLockedTarget();
		PlayerASC->ClearAbility(BackstabHandle);
		BackstabEnemyASC->ClearAbility(VictimHandle);
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
		auto [FrontHandle, FrontAbility, VictimHandle, VictimAbility] = SetupFrontAndVictimExec(100.0f, true);
		if (!TestNotNull(TEXT("FrontAbility valid"), FrontAbility) ||
			!TestTrue(TEXT("Front ability active in Section 11"), FrontAbility->IsActive()) ||
			!TestNotNull(TEXT("VictimAbility valid"), VictimAbility))
		{
			return false;
		}

		// Hit first
		FGameplayEventData HitPayload;
		HitPayload.EventTag = FrontHitTag;
		HitPayload.Instigator = Player;
		HitPayload.Target = Player;
		HitPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerHitEvent(HitPayload);

		TestTrue(TEXT("Hit resolved"), FrontAbility->IsTestDamageEventConsumed());

		// Invalidate victim's wait release task right before release dispatch so it cannot process formal Release
		VictimAbility->SetTestInvalidateWaitReleaseTaskAfterReady(true);

		// Send release request: SendFormalReleaseToVictim dispatches ReleaseEventTag, but if victim fails to mark released, fallback fires
		FGameplayEventData ReleaseReqPayload;
		ReleaseReqPayload.EventTag = ReleaseRequestTag;
		ReleaseReqPayload.Instigator = Player;
		ReleaseReqPayload.Target = Player;
		ReleaseReqPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerReleaseRequestEvent(ReleaseReqPayload);

		// Victim ability must be safely ended by fallback and not left hanging locked
		TestFalse(TEXT("Victim ability is ended by fallback"), VictimAbility->IsActive());
		TestFalse(TEXT("Victim locked tag cleared by fallback"), EnemyASC->HasMatchingGameplayTag(VictimLockedTag));
		TestFalse(TEXT("Victim invulnerable tag cleared by fallback"), EnemyASC->HasMatchingGameplayTag(InvulnerableTag));

		FrontAbility->TestEndAbility(false);
		CleanupFrontExec(FrontHandle, VictimHandle);
	}

	// =========================================================================
	// 12. Execution Cancellation / Interruption Does Not Trigger Launch
	// =========================================================================
	{
		Enemy->RestorePoiseToMax();
		Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

		auto [FrontHandle, FrontAbility, VictimHandle, VictimAbility] = SetupFrontAndVictimExec(100.0f, true);
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
	// 13. Victim Montage Early Completion Does Not End Execution Session
	// =========================================================================
	{
		Enemy->RestorePoiseToMax();
		Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

		auto [FrontHandle, FrontAbility, VictimHandle, VictimAbility] = SetupFrontAndVictimExec(100.0f, true, true);
		if (!TestNotNull(TEXT("FrontAbility valid"), FrontAbility) ||
			!TestTrue(TEXT("Front ability active in Section 13"), FrontAbility->IsActive()) ||
			!TestNotNull(TEXT("VictimAbility valid"), VictimAbility))
		{
			return false;
		}

		// Trigger VictimStart to initiate presentation montage
		const FGameplayTag VictimStartTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.VictimStart")), false);
		FGameplayEventData VictimStartPayload;
		VictimStartPayload.EventTag = VictimStartTag;
		VictimStartPayload.Instigator = Player;
		VictimStartPayload.Target = Player;
		VictimStartPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerVictimStartEvent(VictimStartPayload);

		// Trigger early victim montage completion
		VictimAbility->TestTriggerVictimMontageCompleted();

		// Victim ability must remain active waiting for gameplay Release clock
		TestTrue(TEXT("Victim ability remains active despite montage completion"), VictimAbility->IsActive());
		TestTrue(TEXT("Victim remains locked"), EnemyASC->HasMatchingGameplayTag(VictimLockedTag));

		// Now execute Hit and Release naturally
		FGameplayEventData HitPayload;
		HitPayload.EventTag = FrontHitTag;
		HitPayload.Instigator = Player;
		HitPayload.Target = Player;
		HitPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerHitEvent(HitPayload);

		FGameplayEventData ReleaseReqPayload;
		ReleaseReqPayload.EventTag = ReleaseRequestTag;
		ReleaseReqPayload.Instigator = Player;
		ReleaseReqPayload.Target = Player;
		ReleaseReqPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerReleaseRequestEvent(ReleaseReqPayload);

		TestFalse(TEXT("Victim released upon formal release event"), VictimAbility->IsActive());
		TestFalse(TEXT("Victim lock removed on formal release"), EnemyASC->HasMatchingGameplayTag(VictimLockedTag));

		FrontAbility->TestEndAbility(false);
		CleanupFrontExec(FrontHandle, VictimHandle);
	}

	// =========================================================================
	// 14. Malformed / Stale / Duplicate Release Requests Fail-Closed
	// =========================================================================
	{
		Enemy->RestorePoiseToMax();
		Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

		auto [FrontHandle, FrontAbility, VictimHandle, VictimAbility] = SetupFrontAndVictimExec(100.0f, true);
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

		// 1. Malformed payload with wrong Instigator/Target
		FGameplayEventData MalformedPayload;
		MalformedPayload.EventTag = ReleaseRequestTag;
		MalformedPayload.Instigator = Enemy; // Wrong!
		MalformedPayload.Target = Enemy;     // Wrong!
		MalformedPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerReleaseRequestEvent(MalformedPayload);

		TestFalse(TEXT("Malformed release request ignored"), FrontAbility->IsTestReleaseRequestLatched());
		TestFalse(TEXT("Release not sent"), ExecContext->IsReleaseSent());

		// 2. Normal hit and release to finalize release state
		FGameplayEventData HitPayload;
		HitPayload.EventTag = FrontHitTag;
		HitPayload.Instigator = Player;
		HitPayload.Target = Player;
		HitPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerHitEvent(HitPayload);

		FGameplayEventData ValidReleasePayload;
		ValidReleasePayload.EventTag = ReleaseRequestTag;
		ValidReleasePayload.Instigator = Player;
		ValidReleasePayload.Target = Player;
		ValidReleasePayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerReleaseRequestEvent(ValidReleasePayload);

		TestTrue(TEXT("Release sent"), ExecContext->IsReleaseSent());
		TestTrue(TEXT("Victim released"), ExecContext->IsVictimReleased());

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

		auto [FrontHandle, FrontAbility, VictimHandle, VictimAbility] = SetupFrontAndVictimExec(100.0f, true);
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

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
