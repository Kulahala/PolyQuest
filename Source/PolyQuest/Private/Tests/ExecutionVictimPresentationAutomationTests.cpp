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
#include "Animation/Combat/AnimNotify_PlayerExecutionVictimStart.h"
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
	FExecutionVictimPresentationAutomationTest,
	"PolyQuest.Combat.ExecutionVictimPresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace ExecutionVictimPresentationAutomation
{
	struct FVictimPresentationWorldScope
	{
		UWorld* World = nullptr;
		~FVictimPresentationWorldScope()
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

bool FExecutionVictimPresentationAutomationTest::RunTest(const FString& Parameters)
{
	// =========================================================================
	// 1. CDO & Tag Contract Verification
	// =========================================================================
	{
		const FGameplayTag VictimStartTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.VictimStart")), false);
		TestTrue(TEXT("Tag Event.Action.Execution.Request.VictimStart is registered and valid"), VictimStartTag.IsValid());

		const UAnimNotify_PlayerExecutionVictimStart* NotifyCDO = UAnimNotify_PlayerExecutionVictimStart::StaticClass()->GetDefaultObject<UAnimNotify_PlayerExecutionVictimStart>();
		if (TestNotNull(TEXT("UAnimNotify_PlayerExecutionVictimStart CDO exists"), NotifyCDO))
		{
			TestEqual(TEXT("Notify name is Player Execution VictimStart"), NotifyCDO->GetNotifyName_Implementation(), TEXT("Player Execution VictimStart"));
		}

		const UEnemyVictimExecutionAbility* VictimCDO = UEnemyVictimExecutionAbility::StaticClass()->GetDefaultObject<UEnemyVictimExecutionAbility>();
		if (TestNotNull(TEXT("UEnemyVictimExecutionAbility CDO exists"), VictimCDO))
		{
			// Verify VictimStart is strictly NOT in AbilityTriggers
			bool bVictimStartInTriggers = false;
			for (const FAbilityTriggerData& Trigger : VictimCDO->GetTestAbilityTriggers())
			{
				if (Trigger.TriggerTag == VictimStartTag)
				{
					bVictimStartInTriggers = true;
					break;
				}
			}
			TestFalse(TEXT("VictimStart is NOT in UEnemyVictimExecutionAbility AbilityTriggers"), bVictimStartInTriggers);

			// Verify Front & Backstab requests ARE in AbilityTriggers
			const FGameplayTag FrontReqTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.Front")), false);
			const FGameplayTag BackstabReqTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.Backstab")), false);
			bool bHasFrontTrigger = false;
			bool bHasBackstabTrigger = false;
			for (const FAbilityTriggerData& Trigger : VictimCDO->GetTestAbilityTriggers())
			{
				if (Trigger.TriggerTag == FrontReqTag) { bHasFrontTrigger = true; }
				if (Trigger.TriggerTag == BackstabReqTag) { bHasBackstabTrigger = true; }
			}
			TestTrue(TEXT("Victim CDO has Front Request trigger"), bHasFrontTrigger);
			TestTrue(TEXT("Victim CDO has Backstab Request trigger"), bHasBackstabTrigger);
		}
	}

	// =========================================================================
	// World Fixture Setup
	// =========================================================================
	ExecutionVictimPresentationAutomation::FVictimPresentationWorldScope WorldScope;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("ExecutionVictimPresentationTestWorld"));
	WorldScope.World = World;
	if (!TestNotNull(TEXT("Combat World created successfully"), World))
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

	const FGameplayTag VictimStartTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.VictimStart")), false);
	const FGameplayTag FrontHitTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Hit")), false);
	const FGameplayTag BackstabHitTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Hit")), false);
	const FGameplayTag ReleaseRequestTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.Release")), false);
	const FGameplayTag VictimLockedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Execution.VictimLocked")), false);
	const FGameplayTag PlayerLockedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Execution.PlayerLocked")), false);

	UAnimMontage* PlayerExecutionMontage = NewObject<UAnimMontage>(GetTransientPackage());
	UAnimMontage* EnemyFrontVictimMontage = NewObject<UAnimMontage>(GetTransientPackage());
	UAnimMontage* EnemyBackstabVictimMontage = NewObject<UAnimMontage>(GetTransientPackage());

	// Test Notify execution on Player
	{
		bool bReceivedNotifyEvent = false;
		FDelegateHandle Handle = PlayerASC->GenericGameplayEventCallbacks.FindOrAdd(VictimStartTag).AddLambda(
			[&bReceivedNotifyEvent, Player, PlayerExecutionMontage](const FGameplayEventData* EventData)
			{
				if (EventData && EventData->Instigator == Player && EventData->Target == Player && EventData->OptionalObject == PlayerExecutionMontage)
				{
					bReceivedNotifyEvent = true;
				}
			});

		UAnimNotify_PlayerExecutionVictimStart* NotifyInstance = NewObject<UAnimNotify_PlayerExecutionVictimStart>(GetTransientPackage());
		FAnimNotifyEventReference EventRef;
		NotifyInstance->Notify(Player->GetMesh(), PlayerExecutionMontage, EventRef);

		TestTrue(TEXT("AnimNotify_PlayerExecutionVictimStart dispatches valid payload to Player ASC"), bReceivedNotifyEvent);
		PlayerASC->GenericGameplayEventCallbacks.FindOrAdd(VictimStartTag).Remove(Handle);
	}

	auto GrantAndConfigureVictimAbility = [&](AEnemyCharacter* InEnemy, bool bConfigMontages = true) -> TPair<FGameplayAbilitySpecHandle, UEnemyVictimExecutionAbility*>
	{
		UAbilitySystemComponent* TargetASC = InEnemy->GetAbilitySystemComponent();
		FGameplayAbilitySpec VictimSpec(UEnemyVictimExecutionAbility::StaticClass(), 1, INDEX_NONE, InEnemy);
		const FGameplayAbilitySpecHandle VictimHandle = TargetASC->GiveAbility(VictimSpec);
		FGameplayAbilitySpec* FoundSpec = TargetASC->FindAbilitySpecFromHandle(VictimHandle);
		UEnemyVictimExecutionAbility* Instance = FoundSpec ? Cast<UEnemyVictimExecutionAbility>(FoundSpec->GetPrimaryInstance()) : nullptr;
		if (Instance && bConfigMontages)
		{
			Instance->SetTestVictimMontages(EnemyFrontVictimMontage, EnemyBackstabVictimMontage);
		}
		return { VictimHandle, Instance };
	};

	auto SetupFrontExec = [&](float InEnemyHealth = 100.0f, bool bConfigVictimMontage = true) -> TTuple<FGameplayAbilitySpecHandle, UPlayerFrontExecutionAbility*, FGameplayAbilitySpecHandle, UEnemyVictimExecutionAbility*>
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

		ExecutionVictimPresentationAutomation::ActivateEnemyStanceBreak(Enemy);
		const FGameplayTag StunnedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
		if (!EnemyASC->HasMatchingGameplayTag(StunnedTag))
		{
			EnemyASC->AddLooseGameplayTag(StunnedTag);
		}

		Player->SetTestLockedTarget(Enemy);

		auto [VictimHandle, VictimAbility] = GrantAndConfigureVictimAbility(Enemy, bConfigVictimMontage);

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

	auto SetupBackstabExec = [&](float InEnemyHealth = 100.0f, bool bConfigVictimMontage = true) -> TTuple<FGameplayAbilitySpecHandle, UPlayerBackstabExecutionAbility*, FGameplayAbilitySpecHandle, UEnemyVictimExecutionAbility*>
	{
		Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
		Player->SetActorRotation(FRotator::ZeroRotator);
		Enemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
		Enemy->SetActorRotation(FRotator::ZeroRotator);

		if (UCharacterAttributeSet* EnemyAttribs = const_cast<UCharacterAttributeSet*>(EnemyASC->GetSet<UCharacterAttributeSet>()))
		{
			EnemyAttribs->SetHealth(InEnemyHealth);
			EnemyAttribs->SetPoise(EnemyAttribs->GetMaxPoise());
		}

		const FGameplayTag StunnedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
		if (EnemyASC->HasMatchingGameplayTag(StunnedTag))
		{
			EnemyASC->RemoveLooseGameplayTag(StunnedTag);
		}

		Player->SetTestLockedTarget(Enemy);

		auto [VictimHandle, VictimAbility] = GrantAndConfigureVictimAbility(Enemy, bConfigVictimMontage);

		FGameplayAbilitySpec BackstabSpec(UPlayerBackstabExecutionAbility::StaticClass(), 1, INDEX_NONE, Player);
		const FGameplayAbilitySpecHandle BackstabHandle = PlayerASC->GiveAbility(BackstabSpec);
		FGameplayAbilitySpec* FoundSpec = PlayerASC->FindAbilitySpecFromHandle(BackstabHandle);
		UPlayerBackstabExecutionAbility* BackstabAbility = FoundSpec ? Cast<UPlayerBackstabExecutionAbility>(FoundSpec->GetPrimaryInstance()) : nullptr;
		if (BackstabAbility)
		{
			BackstabAbility->SetTestExecutionMontage(PlayerExecutionMontage);
			BackstabAbility->SetTestDamageGameplayEffectClass(UTestProjectileDamageGE::StaticClass());
			BackstabAbility->SetTestExecutionDistances(0.0f, 250.0f);
			BackstabAbility->SetTestMaxBackAngleDegrees(60.0f);
			BackstabAbility->SetTestSkipMontageTaskActivation(true);
		}

		PlayerASC->TryActivateAbility(BackstabHandle);
		return MakeTuple(BackstabHandle, BackstabAbility, VictimHandle, VictimAbility);
	};

	auto SetupStanceBreakBackstabExec = [&](float InEnemyHealth = 100.0f, bool bConfigVictimMontage = true) -> TTuple<FGameplayAbilitySpecHandle, UPlayerBackstabExecutionAbility*, FGameplayAbilitySpecHandle, UEnemyVictimExecutionAbility*>
	{
		Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
		Player->SetActorRotation(FRotator::ZeroRotator);
		Enemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
		Enemy->SetActorRotation(FRotator::ZeroRotator);

		if (UCharacterAttributeSet* EnemyAttribs = const_cast<UCharacterAttributeSet*>(EnemyASC->GetSet<UCharacterAttributeSet>()))
		{
			EnemyAttribs->SetHealth(InEnemyHealth);
			EnemyAttribs->SetPoise(0.0f);
		}

		ExecutionVictimPresentationAutomation::ActivateEnemyStanceBreak(Enemy);
		const FGameplayTag StunnedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
		if (!EnemyASC->HasMatchingGameplayTag(StunnedTag))
		{
			EnemyASC->AddLooseGameplayTag(StunnedTag);
		}

		Player->SetTestLockedTarget(Enemy);

		auto [VictimHandle, VictimAbility] = GrantAndConfigureVictimAbility(Enemy, bConfigVictimMontage);

		FGameplayAbilitySpec BackstabSpec(UPlayerBackstabExecutionAbility::StaticClass(), 1, INDEX_NONE, Player);
		const FGameplayAbilitySpecHandle BackstabHandle = PlayerASC->GiveAbility(BackstabSpec);
		FGameplayAbilitySpec* FoundSpec = PlayerASC->FindAbilitySpecFromHandle(BackstabHandle);
		UPlayerBackstabExecutionAbility* BackstabAbility = FoundSpec ? Cast<UPlayerBackstabExecutionAbility>(FoundSpec->GetPrimaryInstance()) : nullptr;
		if (BackstabAbility)
		{
			BackstabAbility->SetTestExecutionMontage(PlayerExecutionMontage);
			BackstabAbility->SetTestDamageGameplayEffectClass(UTestProjectileDamageGE::StaticClass());
			BackstabAbility->SetTestExecutionDistances(0.0f, 250.0f);
			BackstabAbility->SetTestMaxBackAngleDegrees(60.0f);
			BackstabAbility->SetTestSkipMontageTaskActivation(true);
		}

		PlayerASC->TryActivateAbility(BackstabHandle);
		return MakeTuple(BackstabHandle, BackstabAbility, VictimHandle, VictimAbility);
	};

	auto ResetWeaponExecutionMontages = [&](APlayerCharacter* InPlayer)
	{
		if (UWeaponEquipmentComponent* EquipComp = InPlayer ? InPlayer->FindComponentByClass<UWeaponEquipmentComponent>() : nullptr)
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

	auto CleanupExec = [&](FGameplayAbilitySpecHandle PlayerHandle, FGameplayAbilitySpecHandle VictimHandle)
	{
		PlayerASC->ClearAbility(PlayerHandle);
		EnemyASC->ClearAbility(VictimHandle);
		Player->SetTestLockedTarget(nullptr);
		ResetWeaponExecutionMontages(Player);
	};

	// =========================================================================
	// 2. Front Handshake: Delayed Victim Presentation & Montage Caching
	// =========================================================================
	{
		auto [FrontHandle, FrontAbility, VictimHandle, VictimAbility] = SetupFrontExec(100.0f, true);
		if (!TestNotNull(TEXT("FrontAbility valid in Sec 2"), FrontAbility) ||
			!TestTrue(TEXT("Front ability active"), FrontAbility->IsActive()) ||
			!TestNotNull(TEXT("VictimAbility valid in Sec 2"), VictimAbility) ||
			!TestTrue(TEXT("Victim ability active"), VictimAbility->IsActive()))
		{
			return false;
		}

		TestTrue(TEXT("Victim is locked on handshake"), EnemyASC->HasMatchingGameplayTag(VictimLockedTag));
		TestTrue(TEXT("Player is locked on handshake"), PlayerASC->HasMatchingGameplayTag(PlayerLockedTag));

		// Crucial contract: Victim presentation is NOT started on handshake!
		TestFalse(TEXT("Victim presentation is NOT started on handshake"), VictimAbility->IsTestVictimPresentationStarted());
		TestNull(TEXT("ActiveVictimMontage is null before VictimStart"), VictimAbility->GetTestActiveVictimMontage());
		TestEqual(TEXT("PendingVictimMontage cached Front montage"), VictimAbility->GetTestPendingVictimMontage(), EnemyFrontVictimMontage);
		TestTrue(TEXT("Front execution has handoff from StanceBreak"), VictimAbility->HasTestHandoffFromStanceBreak());

		// Now trigger VictimStart
		FGameplayEventData StartPayload;
		StartPayload.EventTag = VictimStartTag;
		StartPayload.Instigator = Player;
		StartPayload.Target = Player;
		StartPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerVictimStartEvent(StartPayload);

		TestTrue(TEXT("FrontAbility forwarded VictimStart"), FrontAbility->IsTestVictimStartForwarded());
		TestTrue(TEXT("VictimAbility started presentation on VictimStart"), VictimAbility->IsTestVictimPresentationStarted());

		// Idempotency: duplicate VictimStart is ignored
		FrontAbility->TestTriggerVictimStartEvent(StartPayload);
		TestTrue(TEXT("VictimAbility still in valid presentation state"), VictimAbility->IsTestVictimPresentationStarted());

		// Release cleanly
		FGameplayEventData HitPayload;
		HitPayload.EventTag = FrontHitTag;
		HitPayload.Instigator = Player;
		HitPayload.Target = Player;
		HitPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerHitEvent(HitPayload);

		FGameplayEventData ReleasePayload;
		ReleasePayload.EventTag = ReleaseRequestTag;
		ReleasePayload.Instigator = Player;
		ReleasePayload.Target = Player;
		ReleasePayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerReleaseRequestEvent(ReleasePayload);

		FrontAbility->TestEndAbility(false);
		CleanupExec(FrontHandle, VictimHandle);
	}

	// =========================================================================
	// 3. Backstab Handshake & VictimStart Verification
	// =========================================================================
	{
		Enemy->RestorePoiseToMax();
		Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

		auto [BackstabHandle, BackstabAbility, VictimHandle, VictimAbility] = SetupBackstabExec(100.0f, true);
		if (!TestNotNull(TEXT("BackstabAbility valid in Sec 3"), BackstabAbility) ||
			!TestTrue(TEXT("Backstab ability active"), BackstabAbility->IsActive()) ||
			!TestNotNull(TEXT("VictimAbility valid in Sec 3"), VictimAbility) ||
			!TestTrue(TEXT("Victim ability active"), VictimAbility->IsActive()))
		{
			return false;
		}

		TestFalse(TEXT("Backstab handshake: victim presentation not started"), VictimAbility->IsTestVictimPresentationStarted());
		TestEqual(TEXT("PendingVictimMontage cached Backstab montage"), VictimAbility->GetTestPendingVictimMontage(), EnemyBackstabVictimMontage);
		TestFalse(TEXT("Ordinary Backstab has NO handoff from StanceBreak"), VictimAbility->HasTestHandoffFromStanceBreak());

		FGameplayEventData StartPayload;
		StartPayload.EventTag = VictimStartTag;
		StartPayload.Instigator = Player;
		StartPayload.Target = Player;
		StartPayload.OptionalObject = PlayerExecutionMontage;
		BackstabAbility->TestTriggerVictimStartEvent(StartPayload);

		TestTrue(TEXT("Backstab forwarded VictimStart"), BackstabAbility->IsTestVictimStartForwarded());
		TestTrue(TEXT("Victim presentation started"), VictimAbility->IsTestVictimPresentationStarted());

		// Complete Hit and Release
		FGameplayEventData HitPayload;
		HitPayload.EventTag = BackstabHitTag;
		HitPayload.Instigator = Player;
		HitPayload.Target = Player;
		HitPayload.OptionalObject = PlayerExecutionMontage;
		BackstabAbility->TestTriggerHitEvent(HitPayload);

		FGameplayEventData ReleasePayload;
		ReleasePayload.EventTag = ReleaseRequestTag;
		ReleasePayload.Instigator = Player;
		ReleasePayload.Target = Player;
		ReleasePayload.OptionalObject = PlayerExecutionMontage;
		BackstabAbility->TestTriggerReleaseRequestEvent(ReleasePayload);

		BackstabAbility->TestEndAbility(false);
		CleanupExec(BackstabHandle, VictimHandle);
	}

	// =========================================================================
	// 3B. StanceBreak Backstab Compatibility: Handoff, Backstab Montage, & Full Poise Recovery
	// =========================================================================
	{
		Enemy->RestorePoiseToMax();
		Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

		auto [BackstabHandle, BackstabAbility, VictimHandle, VictimAbility] = SetupStanceBreakBackstabExec(100.0f, true);
		if (!TestNotNull(TEXT("BackstabAbility valid in Sec 3B"), BackstabAbility) ||
			!TestTrue(TEXT("Backstab ability active on StanceBreak target"), BackstabAbility->IsActive()) ||
			!TestNotNull(TEXT("VictimAbility valid in Sec 3B"), VictimAbility) ||
			!TestTrue(TEXT("Victim ability active on StanceBreak target"), VictimAbility->IsActive()))
		{
			return false;
		}

		// Contract: Handoff from StanceBreak MUST be true
		TestTrue(TEXT("StanceBreak Backstab has handoff from StanceBreak"), VictimAbility->HasTestHandoffFromStanceBreak());

		// Contract: PendingVictimMontage MUST be Backstab Montage (NOT Front Montage despite handoff!)
		TestEqual(TEXT("PendingVictimMontage cached Backstab montage for StanceBreak backstab"),
			VictimAbility->GetTestPendingVictimMontage(), EnemyBackstabVictimMontage);

		// Contract: StanceBreak was canceled by victim activation, but because VictimLocked was active,
		// StanceBreak skipped restoring Poise/movement. The enemy Poise is still 0 during execution!
		const UCharacterAttributeSet* EnemyAttribs = EnemyASC->GetSet<UCharacterAttributeSet>();
		TestNotNull(TEXT("Enemy CharacterAttributeSet valid"), EnemyAttribs);
		if (EnemyAttribs)
		{
			TestTrue(TEXT("Poise is 0 during execution lock"), FMath::IsNearlyZero(EnemyAttribs->GetPoise(), KINDA_SMALL_NUMBER));
		}

		// Trigger VictimStart
		FGameplayEventData StartPayload;
		StartPayload.EventTag = VictimStartTag;
		StartPayload.Instigator = Player;
		StartPayload.Target = Player;
		StartPayload.OptionalObject = PlayerExecutionMontage;
		BackstabAbility->TestTriggerVictimStartEvent(StartPayload);

		TestTrue(TEXT("Backstab forwarded VictimStart"), BackstabAbility->IsTestVictimStartForwarded());
		TestTrue(TEXT("Victim presentation started"), VictimAbility->IsTestVictimPresentationStarted());

		// Complete Hit and Release
		FGameplayEventData HitPayload;
		HitPayload.EventTag = BackstabHitTag;
		HitPayload.Instigator = Player;
		HitPayload.Target = Player;
		HitPayload.OptionalObject = PlayerExecutionMontage;
		BackstabAbility->TestTriggerHitEvent(HitPayload);

		FGameplayEventData ReleasePayload;
		ReleasePayload.EventTag = ReleaseRequestTag;
		ReleasePayload.Instigator = Player;
		ReleasePayload.Target = Player;
		ReleasePayload.OptionalObject = PlayerExecutionMontage;
		BackstabAbility->TestTriggerReleaseRequestEvent(ReleasePayload);

		BackstabAbility->TestEndAbility(false);

		// Contract: Upon Release & EndAbility for living enemy, Victim restores Poise to MaxPoise,
		// restores MovementMode to MOVE_Walking, and releases AI lock!
		if (EnemyAttribs)
		{
			TestEqual(TEXT("Enemy Poise fully restored to MaxPoise after StanceBreak Backstab release"),
				EnemyAttribs->GetPoise(), EnemyAttribs->GetMaxPoise());
		}
		TestEqual(TEXT("Enemy movement restored to MOVE_Walking"),
			Enemy->GetCharacterMovement()->MovementMode.GetValue(), MOVE_Walking);
		TestFalse(TEXT("Victim AI lock released"), VictimAbility->IsTestAIExecutionLocked());

		CleanupExec(BackstabHandle, VictimHandle);
	}

	// =========================================================================
	// 3C. StanceBreak Backstab Interruption: Restores Poise & Movement on Cancel
	// =========================================================================
	{
		Enemy->RestorePoiseToMax();
		Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

		auto [BackstabHandle, BackstabAbility, VictimHandle, VictimAbility] = SetupStanceBreakBackstabExec(100.0f, true);
		if (!TestNotNull(TEXT("BackstabAbility valid in Sec 3C"), BackstabAbility) ||
			!TestNotNull(TEXT("VictimAbility valid in Sec 3C"), VictimAbility))
		{
			return false;
		}

		// Cancel VictimAbility directly mid-execution
		VictimAbility->TestEndAbility(true);

		const UCharacterAttributeSet* EnemyAttribs = EnemyASC->GetSet<UCharacterAttributeSet>();
		if (EnemyAttribs)
		{
			TestEqual(TEXT("Enemy Poise restored to MaxPoise on cancellation of StanceBreak Backstab"),
				EnemyAttribs->GetPoise(), EnemyAttribs->GetMaxPoise());
		}
		TestEqual(TEXT("Enemy movement restored to MOVE_Walking on cancellation"),
			Enemy->GetCharacterMovement()->MovementMode.GetValue(), MOVE_Walking);
		TestFalse(TEXT("Victim AI lock released on cancellation"), VictimAbility->IsTestAIExecutionLocked());

		if (BackstabAbility)
		{
			BackstabAbility->TestEndAbility(true);
		}
		CleanupExec(BackstabHandle, VictimHandle);
	}

	// =========================================================================
	// 4. Out-of-Order / Same Frame: Hit before VictimStart
	// =========================================================================
	{
		Enemy->RestorePoiseToMax();
		Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

		auto [FrontHandle, FrontAbility, VictimHandle, VictimAbility] = SetupFrontExec(100.0f, true);
		if (!TestNotNull(TEXT("FrontAbility valid in Sec 4"), FrontAbility))
		{
			return false;
		}

		// Hit arrives FIRST
		FGameplayEventData HitPayload;
		HitPayload.EventTag = FrontHitTag;
		HitPayload.Instigator = Player;
		HitPayload.Target = Player;
		HitPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerHitEvent(HitPayload);

		TestTrue(TEXT("Damage consumed on early Hit"), FrontAbility->IsTestDamageEventConsumed());
		TestTrue(TEXT("Victim remains locked after Hit"), EnemyASC->HasMatchingGameplayTag(VictimLockedTag));
		TestFalse(TEXT("Victim presentation not yet started"), VictimAbility->IsTestVictimPresentationStarted());

		// VictimStart arrives SECOND
		FGameplayEventData StartPayload;
		StartPayload.EventTag = VictimStartTag;
		StartPayload.Instigator = Player;
		StartPayload.Target = Player;
		StartPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerVictimStartEvent(StartPayload);

		TestTrue(TEXT("Victim presentation successfully started after early Hit"), VictimAbility->IsTestVictimPresentationStarted());

		// Release arrives LAST
		FGameplayEventData ReleasePayload;
		ReleasePayload.EventTag = ReleaseRequestTag;
		ReleasePayload.Instigator = Player;
		ReleasePayload.Target = Player;
		ReleasePayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerReleaseRequestEvent(ReleasePayload);

		TestFalse(TEXT("Victim released on Release"), VictimAbility->IsActive());
		FrontAbility->TestEndAbility(false);
		CleanupExec(FrontHandle, VictimHandle);
	}

	// =========================================================================
	// 5. Fail-Closed Verification: Malformed Context / Wrong Animation / Late Arrival
	// =========================================================================
	{
		Enemy->RestorePoiseToMax();
		Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

		auto [FrontHandle, FrontAbility, VictimHandle, VictimAbility] = SetupFrontExec(100.0f, true);
		if (!TestNotNull(TEXT("FrontAbility valid in Sec 5"), FrontAbility))
		{
			return false;
		}

		// 5A: Wrong animation object on VictimStart
		UAnimMontage* WrongMontage = NewObject<UAnimMontage>(GetTransientPackage());
		FGameplayEventData WrongAnimPayload;
		WrongAnimPayload.EventTag = VictimStartTag;
		WrongAnimPayload.Instigator = Player;
		WrongAnimPayload.Target = Player;
		WrongAnimPayload.OptionalObject = WrongMontage;
		FrontAbility->TestTriggerVictimStartEvent(WrongAnimPayload);

		TestFalse(TEXT("Wrong animation rejected on Player side"), FrontAbility->IsTestVictimStartForwarded());
		TestFalse(TEXT("Victim presentation not started on wrong anim"), VictimAbility->IsTestVictimPresentationStarted());

		// 5B: Direct malformed payload to Victim ASC (wrong context)
		FGameplayEventData MalformedPayload;
		MalformedPayload.EventTag = VictimStartTag;
		MalformedPayload.Instigator = Player;
		MalformedPayload.Target = Enemy;
		MalformedPayload.OptionalObject = nullptr; // invalid context
		MalformedPayload.OptionalObject2 = PlayerExecutionMontage;
		VictimAbility->TestTriggerVictimStartEvent(MalformedPayload);

		TestFalse(TEXT("Null context rejected on Victim side"), VictimAbility->IsTestVictimPresentationStarted());

		// Now hit and release
		FGameplayEventData HitPayload;
		HitPayload.EventTag = FrontHitTag;
		HitPayload.Instigator = Player;
		HitPayload.Target = Player;
		HitPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerHitEvent(HitPayload);

		FGameplayEventData ReleasePayload;
		ReleasePayload.EventTag = ReleaseRequestTag;
		ReleasePayload.Instigator = Player;
		ReleasePayload.Target = Player;
		ReleasePayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerReleaseRequestEvent(ReleasePayload);

		// 5C: Late VictimStart arrival AFTER Release
		FGameplayEventData ValidStartPayload;
		ValidStartPayload.EventTag = VictimStartTag;
		ValidStartPayload.Instigator = Player;
		ValidStartPayload.Target = Player;
		ValidStartPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerVictimStartEvent(ValidStartPayload);

		TestFalse(TEXT("Late VictimStart rejected after Release"), FrontAbility->IsTestVictimStartForwarded());

		FrontAbility->TestEndAbility(false);
		CleanupExec(FrontHandle, VictimHandle);
	}

	// =========================================================================
	// 6. Graceful Fallback: Missing Victim Montage & Missing VictimStart
	// =========================================================================
	{
		Enemy->RestorePoiseToMax();
		Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

		// 6A: No Victim Montage configured
		auto [FrontHandle, FrontAbility, VictimHandle, VictimAbility] = SetupFrontExec(100.0f, false);
		if (!TestNotNull(TEXT("FrontAbility valid in Sec 6A"), FrontAbility))
		{
			return false;
		}

		TestNull(TEXT("PendingVictimMontage is null when unconfigured"), VictimAbility->GetTestPendingVictimMontage());

		// Send valid VictimStart: should succeed forwarding, but safe no-op on victim side
		FGameplayEventData StartPayload;
		StartPayload.EventTag = VictimStartTag;
		StartPayload.Instigator = Player;
		StartPayload.Target = Player;
		StartPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerVictimStartEvent(StartPayload);

		TestTrue(TEXT("VictimStart forwarded even when victim has no montage"), FrontAbility->IsTestVictimStartForwarded());
		TestNull(TEXT("No active montage started when unconfigured"), VictimAbility->GetTestActiveVictimMontage());

		// Hit & Release proceed naturally
		FGameplayEventData HitPayload;
		HitPayload.EventTag = FrontHitTag;
		HitPayload.Instigator = Player;
		HitPayload.Target = Player;
		HitPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerHitEvent(HitPayload);

		FGameplayEventData ReleasePayload;
		ReleasePayload.EventTag = ReleaseRequestTag;
		ReleasePayload.Instigator = Player;
		ReleasePayload.Target = Player;
		ReleasePayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerReleaseRequestEvent(ReleasePayload);

		TestFalse(TEXT("Session ended smoothly without victim montage"), VictimAbility->IsActive());
		FrontAbility->TestEndAbility(false);
		CleanupExec(FrontHandle, VictimHandle);
	}

	// =========================================================================
	// 7. Natural Montage BlendOut / Completion Does Not Terminate Session
	// =========================================================================
	{
		Enemy->RestorePoiseToMax();
		Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

		auto [FrontHandle, FrontAbility, VictimHandle, VictimAbility] = SetupFrontExec(100.0f, true);
		if (!TestNotNull(TEXT("FrontAbility valid in Sec 7"), FrontAbility))
		{
			return false;
		}

		FGameplayEventData StartPayload;
		StartPayload.EventTag = VictimStartTag;
		StartPayload.Instigator = Player;
		StartPayload.Target = Player;
		StartPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerVictimStartEvent(StartPayload);

		// Trigger natural completion
		VictimAbility->TestTriggerVictimMontageCompleted();

		TestTrue(TEXT("VictimAbility remains active after montage completes"), VictimAbility->IsActive());
		TestTrue(TEXT("Victim locked tag remains after montage completes"), EnemyASC->HasMatchingGameplayTag(VictimLockedTag));

		// Hit and Release naturally
		FGameplayEventData HitPayload;
		HitPayload.EventTag = FrontHitTag;
		HitPayload.Instigator = Player;
		HitPayload.Target = Player;
		HitPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerHitEvent(HitPayload);

		FGameplayEventData ReleasePayload;
		ReleasePayload.EventTag = ReleaseRequestTag;
		ReleasePayload.Instigator = Player;
		ReleasePayload.Target = Player;
		ReleasePayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerReleaseRequestEvent(ReleasePayload);

		TestFalse(TEXT("Victim released on formal Release"), VictimAbility->IsActive());
		FrontAbility->TestEndAbility(false);
		CleanupExec(FrontHandle, VictimHandle);
	}

	// =========================================================================
	// 8. ReadyForActivation Synchronous Invalidation
	// =========================================================================
	{
		Enemy->RestorePoiseToMax();
		Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

		// Invalidate Player VictimStart task after ready
		{
			auto [VictimHandle, VictimAbility] = GrantAndConfigureVictimAbility(Enemy, true);

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
				FrontAbility->SetTestInvalidateWaitVictimStartEventTaskAfterReady(true);
			}

			PlayerASC->TryActivateAbility(FrontHandle);
			TestFalse(TEXT("Player ability fails-closed when VictimStart task invalidated"), FrontAbility->IsActive());
			CleanupExec(FrontHandle, VictimHandle);
		}

		// Invalidate Victim VictimStart task after ready
		{
			UAbilitySystemComponent* TargetASC = Enemy->GetAbilitySystemComponent();
			FGameplayAbilitySpec VictimSpec(UEnemyVictimExecutionAbility::StaticClass(), 1, INDEX_NONE, Enemy);
			const FGameplayAbilitySpecHandle VictimHandle = TargetASC->GiveAbility(VictimSpec);
			FGameplayAbilitySpec* FoundSpec = TargetASC->FindAbilitySpecFromHandle(VictimHandle);
			UEnemyVictimExecutionAbility* VictimAbility = FoundSpec ? Cast<UEnemyVictimExecutionAbility>(FoundSpec->GetPrimaryInstance()) : nullptr;
			if (VictimAbility)
			{
				VictimAbility->SetTestInvalidateWaitVictimStartTaskAfterReady(true);
			}

			FGameplayAbilitySpec FrontSpec(UPlayerFrontExecutionAbility::StaticClass(), 1, INDEX_NONE, Player);
			const FGameplayAbilitySpecHandle FrontHandle = PlayerASC->GiveAbility(FrontSpec);
			FGameplayAbilitySpec* FoundFrontSpec = PlayerASC->FindAbilitySpecFromHandle(FrontHandle);
			UPlayerFrontExecutionAbility* FrontAbility = FoundFrontSpec ? Cast<UPlayerFrontExecutionAbility>(FoundFrontSpec->GetPrimaryInstance()) : nullptr;
			if (FrontAbility)
			{
				FrontAbility->SetTestExecutionMontage(PlayerExecutionMontage);
				FrontAbility->SetTestDamageGameplayEffectClass(UTestProjectileDamageGE::StaticClass());
				FrontAbility->SetTestExecutionDistances(0.0f, 250.0f);
				FrontAbility->SetTestMaxFrontAngleDegrees(60.0f);
				FrontAbility->SetTestSkipMontageTaskActivation(true);
			}

			PlayerASC->TryActivateAbility(FrontHandle);
			TestFalse(TEXT("Victim ability fails-closed when WaitVictimStartTask invalidated"), VictimAbility->IsActive());
			CleanupExec(FrontHandle, VictimHandle);
		}
	}

	// =========================================================================
	// 9. Launch Fallback & Disabled Launch Verification
	// =========================================================================
	{
		Enemy->RestorePoiseToMax();
		Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

		// 9A: Disabled Launch (bLaunchNonLethalOnRelease = false)
		auto [FrontHandle, FrontAbility, VictimHandle, VictimAbility] = SetupFrontExec(100.0f, true);
		if (!TestNotNull(TEXT("FrontAbility valid in Sec 9A"), FrontAbility))
		{
			return false;
		}

		VictimAbility->SetTestLaunchNonLethalOnRelease(false);

		FGameplayEventData StartPayload;
		StartPayload.EventTag = VictimStartTag;
		StartPayload.Instigator = Player;
		StartPayload.Target = Player;
		StartPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerVictimStartEvent(StartPayload);

		FGameplayEventData HitPayload;
		HitPayload.EventTag = FrontHitTag;
		HitPayload.Instigator = Player;
		HitPayload.Target = Player;
		HitPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerHitEvent(HitPayload);

		FGameplayEventData ReleasePayload;
		ReleasePayload.EventTag = ReleaseRequestTag;
		ReleasePayload.Instigator = Player;
		ReleasePayload.Target = Player;
		ReleasePayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerReleaseRequestEvent(ReleasePayload);

		TestFalse(TEXT("Victim released with disabled launch"), VictimAbility->IsActive());
		TestEqual(TEXT("Enemy movement mode restored to Walking"), Enemy->GetCharacterMovement()->MovementMode, MOVE_Walking);

		FrontAbility->TestEndAbility(false);
		CleanupExec(FrontHandle, VictimHandle);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
