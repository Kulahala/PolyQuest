#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/EnemyStanceBreakAbility.h"
#include "AbilitySystem/Abilities/EnemyVictimExecutionAbility.h"
#include "AbilitySystem/Abilities/PlayerBackstabExecutionAbility.h"
#include "AbilitySystem/Abilities/PlayerFrontExecutionAbility.h"
#include "AbilitySystem/CharacterAttributeSet.h"
#include "Camera/CameraShakeBase.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Character/Player/PlayerCharacter.h"
#include "Combat/Execution/ExecutionLockContext.h"
#include "Combat/Feedback/CombatFeedbackDataAsset.h"
#include "Combat/Melee/MeleeHitResolver.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/PolyQuestPlayerController.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "Kismet/GameplayStatics.h"
#include "Tests/CombatAutomationFixture.h"
#include "Tests/TestHitFeedbackCameraShake.h"
#include "Tests/TestProjectileDamageGE.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExecutionImpactFeedbackAutomationTest,
	"PolyQuest.Combat.ExecutionImpactFeedback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	struct FExecutionImpactFeedbackWorldScope
	{
		UWorld* World = nullptr;
		~FExecutionImpactFeedbackWorldScope()
		{
			if (World)
			{
				UGameplayStatics::SetGlobalTimeDilation(World, 1.0f);
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
		}
	};

	void AdvanceFeedbackWorldTimer(UWorld* World, float DeltaSeconds)
	{
		constexpr float MaxTickStepSeconds = 0.05f;
		while (DeltaSeconds > KINDA_SMALL_NUMBER)
		{
			const float TickStep = FMath::Min(DeltaSeconds, MaxTickStepSeconds);
			if (World)
			{
				World->Tick(ELevelTick::LEVELTICK_All, TickStep);
				++GFrameCounter;
			}
			DeltaSeconds -= TickStep;
		}
	}

	FGameplayAbilitySpecHandle ActivateEnemyStanceBreakHelper(AEnemyCharacter* InEnemy)
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

bool FExecutionImpactFeedbackAutomationTest::RunTest(const FString& Parameters)
{
	// =========================================================================
	// 1. DataAsset Defaults & Contract Verification
	// =========================================================================
	{
		const UPlayerCombatFeedbackDataAsset* PlayerCDO = UPlayerCombatFeedbackDataAsset::StaticClass()->GetDefaultObject<UPlayerCombatFeedbackDataAsset>();
		TestNotNull(TEXT("PlayerCombatFeedbackDataAsset CDO exists"), PlayerCDO);
		if (PlayerCDO)
		{
			TestNull(TEXT("Player Execution AttackerImpactCameraShakeClass defaults to null"), PlayerCDO->Execution.AttackerImpactCameraShakeClass);
		}

		const UEnemyCombatFeedbackDataAsset* EnemyCDO = UEnemyCombatFeedbackDataAsset::StaticClass()->GetDefaultObject<UEnemyCombatFeedbackDataAsset>();
		TestNotNull(TEXT("EnemyCombatFeedbackDataAsset CDO exists"), EnemyCDO);
		if (EnemyCDO)
		{
			TestEqual(TEXT("Enemy Execution ImpactHitStopDuration defaults to 0.05s"), EnemyCDO->Execution.ImpactHitStopDurationSeconds, 0.05f);
			TestEqual(TEXT("Enemy Execution ImpactHitStopTimeDilation defaults to 0.03"), EnemyCDO->Execution.ImpactHitStopTimeDilation, 0.03f);
		}
	}

	// =========================================================================
	// 2. World & Actor Fixture Setup
	// =========================================================================
	FExecutionImpactFeedbackWorldScope WorldScope;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("ExecutionImpactFeedbackTestWorld"));
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

	if (!TestNotNull(TEXT("Player spawned"), Player)
		|| !TestNotNull(TEXT("Enemy spawned"), Enemy)
		|| !TestNotNull(TEXT("Controller spawned"), Controller))
	{
		return false;
	}

	Controller->DispatchBeginPlay();
	Controller->SetAsLocalPlayerController();
	Controller->Possess(Player);
	TestTrue(TEXT("Controller is local for Player camera feedback"), Controller->IsLocalController());
	TestNotNull(TEXT("Controller has a PlayerCameraManager for shake feedback"), Controller->PlayerCameraManager.Get());

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

	UPlayerCombatFeedbackDataAsset* PlayerFeedback = NewObject<UPlayerCombatFeedbackDataAsset>(Player, NAME_None, RF_Transient);
	PlayerFeedback->Execution.AttackerImpactCameraShakeClass = UTestBigHitFeedbackCameraShake::StaticClass();
	Player->SetTestCombatFeedbackData(PlayerFeedback);

	UEnemyCombatFeedbackDataAsset* EnemyFeedback = NewObject<UEnemyCombatFeedbackDataAsset>(Enemy, NAME_None, RF_Transient);
	EnemyFeedback->Execution.ImpactHitStopDurationSeconds = 0.05f;
	EnemyFeedback->Execution.ImpactHitStopTimeDilation = 0.03f;
	Enemy->SetTestCombatFeedbackData(EnemyFeedback);

	const FGameplayTag TagCanonicalHit = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Hit")), false);
	const FGameplayTag TagVictimStart = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.VictimStart")), false);
	UAnimMontage* SyntheticMontage = NewObject<UAnimMontage>(GetTransientPackage());

	auto GrantAndConfigureVictimAbility = [&](AEnemyCharacter* InEnemy) -> TPair<FGameplayAbilitySpecHandle, UEnemyVictimExecutionAbility*>
	{
		UAbilitySystemComponent* TargetASC = InEnemy->GetAbilitySystemComponent();
		FGameplayAbilitySpec VictimSpec(UEnemyVictimExecutionAbility::StaticClass(), 1, INDEX_NONE, InEnemy);
		const FGameplayAbilitySpecHandle VictimHandle = TargetASC->GiveAbility(VictimSpec);
		FGameplayAbilitySpec* FoundSpec = TargetASC->FindAbilitySpecFromHandle(VictimHandle);
		UEnemyVictimExecutionAbility* Instance = FoundSpec ? Cast<UEnemyVictimExecutionAbility>(FoundSpec->GetPrimaryInstance()) : nullptr;
		return { VictimHandle, Instance };
	};

	auto SetupFrontExec = [&](float InEnemyHealth = 100.0f) -> TTuple<FGameplayAbilitySpecHandle, UPlayerFrontExecutionAbility*, FGameplayAbilitySpecHandle, UEnemyVictimExecutionAbility*>
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

		ActivateEnemyStanceBreakHelper(Enemy);
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
			FrontAbility->SetTestExecutionMontage(SyntheticMontage);
			FrontAbility->SetTestDamageGameplayEffectClass(UTestProjectileDamageGE::StaticClass());
			FrontAbility->SetTestExecutionDistances(0.0f, 250.0f);
			FrontAbility->SetTestMaxFrontAngleDegrees(60.0f);
			FrontAbility->SetTestSkipMontageTaskActivation(true);
		}

		PlayerASC->TryActivateAbility(FrontHandle);
		return MakeTuple(FrontHandle, FrontAbility, VictimHandle, VictimAbility);
	};

	auto SetupBackstabExec = [&](AEnemyCharacter* TargetEnemy, float InEnemyHealth = 100.0f) -> TTuple<FGameplayAbilitySpecHandle, UPlayerBackstabExecutionAbility*, FGameplayAbilitySpecHandle, UEnemyVictimExecutionAbility*>
	{
		UAbilitySystemComponent* TargetASC = TargetEnemy ? TargetEnemy->GetAbilitySystemComponent() : nullptr;
		Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
		Player->SetActorRotation(FRotator::ZeroRotator);
		if (TargetEnemy)
		{
			TargetEnemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
			TargetEnemy->SetActorRotation(FRotator::ZeroRotator);
		}

		if (TargetASC)
		{
			if (UCharacterAttributeSet* EnemyAttribs = const_cast<UCharacterAttributeSet*>(TargetASC->GetSet<UCharacterAttributeSet>()))
			{
				EnemyAttribs->SetHealth(InEnemyHealth);
				EnemyAttribs->SetPoise(EnemyAttribs->GetMaxPoise());
			}

			const FGameplayTag StunnedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
			if (TargetASC->HasMatchingGameplayTag(StunnedTag))
			{
				TargetASC->RemoveLooseGameplayTag(StunnedTag);
			}
		}

		Player->SetTestLockedTarget(TargetEnemy);

		auto [VictimHandle, VictimAbility] = GrantAndConfigureVictimAbility(TargetEnemy);

		FGameplayAbilitySpec BackstabSpec(UPlayerBackstabExecutionAbility::StaticClass(), 1, INDEX_NONE, Player);
		const FGameplayAbilitySpecHandle BackstabHandle = PlayerASC->GiveAbility(BackstabSpec);
		FGameplayAbilitySpec* FoundSpec = PlayerASC->FindAbilitySpecFromHandle(BackstabHandle);
		UPlayerBackstabExecutionAbility* BackstabAbility = FoundSpec ? Cast<UPlayerBackstabExecutionAbility>(FoundSpec->GetPrimaryInstance()) : nullptr;
		if (BackstabAbility)
		{
			BackstabAbility->SetTestExecutionMontage(SyntheticMontage);
			BackstabAbility->SetTestDamageGameplayEffectClass(UTestProjectileDamageGE::StaticClass());
			BackstabAbility->SetTestExecutionDistances(0.0f, 250.0f);
			BackstabAbility->SetTestMaxBackAngleDegrees(60.0f);
			BackstabAbility->SetTestSkipMontageTaskActivation(true);
		}

		PlayerASC->TryActivateAbility(BackstabHandle);
		return MakeTuple(BackstabHandle, BackstabAbility, VictimHandle, VictimAbility);
	};

	// =========================================================================
	// 3. Front Execution: NonLethal Impact Feedback & Exactly-Once
	// =========================================================================
	{
		const int32 InitialShakeCount = Player->GetTestHitFeedbackCameraShakeStartCount();
		const int32 InitialHitStopCount = Enemy->GetTestCombatImpactHitStopRequestCount();
		const int32 InitialSoundCount = Enemy->GetTestImpactSoundDispatchCount();
		const int32 InitialBloodCount = Enemy->GetTestImpactBloodDispatchCount();

		auto [FrontHandle, FrontAbility, VictimHandle, VictimAbility] = SetupFrontExec(100.0f);
		TestTrue(TEXT("Front ability activated"), FrontAbility && FrontAbility->IsActive());
		TestTrue(TEXT("Victim ability active"), VictimAbility && VictimAbility->IsActive());

		UExecutionLockContext* Context = FrontAbility->GetTestExecutionContext();
		TestNotNull(TEXT("Execution context exists"), Context);

		FGameplayEventData HitPayload;
		HitPayload.EventTag = TagCanonicalHit;
		HitPayload.Instigator = Player;
		HitPayload.Target = Player;
		HitPayload.OptionalObject = SyntheticMontage;
		FrontAbility->TestTriggerHitEvent(HitPayload);

		TestTrue(TEXT("Damage consumed"), FrontAbility->IsTestDamageEventConsumed());
		if (Context)
		{
			TestEqual(TEXT("Context transitioned to NonLethal"), Context->GetHitState(), EExecutionSessionHitState::NonLethal);
		}

		TestEqual(TEXT("Player shake triggered exactly once"), Player->GetTestHitFeedbackCameraShakeStartCount(), InitialShakeCount + 1);
		TestEqual(TEXT("Enemy hit-stop triggered exactly once"), Enemy->GetTestCombatImpactHitStopRequestCount(), InitialHitStopCount + 1);
		TestEqual(TEXT("Enemy hit-stop duration is 0.05s"), Enemy->GetTestLastImpactHitStopDuration(), 0.05f);
		TestEqual(TEXT("Enemy hit-stop dilation is 0.03"), Enemy->GetTestLastImpactHitStopTimeDilation(), 0.03f);
		TestEqual(TEXT("Sound dispatched exactly once"), Enemy->GetTestImpactSoundDispatchCount(), InitialSoundCount + 1);
		TestEqual(TEXT("Blood dispatched exactly once"), Enemy->GetTestImpactBloodDispatchCount(), InitialBloodCount + 1);

		// Blood normal must point from Target to Attacker: Player at (0,0,0), Target at (150,0,0) -> (-1,0,0)
		const FVector ExpectedBloodNormal(-1.0f, 0.0f, 0.0f);
		TestTrue(TEXT("Blood normal is Target-to-Attacker direction"), Enemy->GetTestLastImpactBloodNormal().Equals(ExpectedBloodNormal, 1e-3f));

		// Second canonical hit must be ignored
		FrontAbility->TestTriggerHitEvent(HitPayload);
		TestEqual(TEXT("Repeated Hit does not increase shake count"), Player->GetTestHitFeedbackCameraShakeStartCount(), InitialShakeCount + 1);
		TestEqual(TEXT("Repeated Hit does not increase hit-stop count"), Enemy->GetTestCombatImpactHitStopRequestCount(), InitialHitStopCount + 1);
		TestEqual(TEXT("Repeated Hit does not increase sound count"), Enemy->GetTestImpactSoundDispatchCount(), InitialSoundCount + 1);
		TestEqual(TEXT("Repeated Hit does not increase blood count"), Enemy->GetTestImpactBloodDispatchCount(), InitialBloodCount + 1);

		// VictimStart handoff must not trigger feedback
		FGameplayEventData VictimStartPayload;
		VictimStartPayload.EventTag = TagVictimStart;
		VictimStartPayload.Instigator = Player;
		VictimStartPayload.Target = Player;
		VictimStartPayload.OptionalObject = SyntheticMontage;
		FrontAbility->TestTriggerVictimStartEvent(VictimStartPayload);

		TestEqual(TEXT("VictimStart does not trigger shake"), Player->GetTestHitFeedbackCameraShakeStartCount(), InitialShakeCount + 1);
		TestEqual(TEXT("VictimStart does not trigger hit-stop"), Enemy->GetTestCombatImpactHitStopRequestCount(), InitialHitStopCount + 1);
		TestEqual(TEXT("VictimStart does not trigger sound"), Enemy->GetTestImpactSoundDispatchCount(), InitialSoundCount + 1);
		TestEqual(TEXT("VictimStart does not trigger blood"), Enemy->GetTestImpactBloodDispatchCount(), InitialBloodCount + 1);

		FrontAbility->TestEndAbility();
		if (VictimAbility && VictimAbility->IsActive())
		{
			VictimAbility->TestEndAbility();
		}
		PlayerASC->ClearAbility(FrontHandle);
		EnemyASC->ClearAbility(VictimHandle);
		AdvanceFeedbackWorldTimer(World, 0.1f);
	}

	// =========================================================================
	// 4. Backstab Execution: DeathPending Impact Feedback & Exactly-Once
	// =========================================================================
	{
		AEnemyCharacter* LethalEnemy = FCombatAutomationFixture::SpawnPassiveEnemy(
			World,
			FTransform(FRotator(0.0f, 180.0f, 0.0f), FVector(150.0f, 0.0f, 0.0f)));
		if (!TestNotNull(TEXT("LethalEnemy spawned"), LethalEnemy))
		{
			return false;
		}
		LethalEnemy->SetTestCombatTeamTag(TagTeamEnemy);
		LethalEnemy->SetTestCombatFeedbackData(EnemyFeedback);
		UAbilitySystemComponent* LethalEnemyASC = LethalEnemy->GetAbilitySystemComponent();

		const int32 ShakeCountBefore = Player->GetTestHitFeedbackCameraShakeStartCount();
		const int32 HitStopCountBefore = LethalEnemy->GetTestCombatImpactHitStopRequestCount();
		const int32 SoundCountBefore = LethalEnemy->GetTestImpactSoundDispatchCount();
		const int32 BloodCountBefore = LethalEnemy->GetTestImpactBloodDispatchCount();

		// LethalEnemy with 10 HP will receive lethal damage from UTestProjectileDamageGE (25 damage)
		auto [BackstabHandle, BackstabAbility, VictimHandle, VictimAbility] = SetupBackstabExec(LethalEnemy, 10.0f);
		TestTrue(TEXT("Backstab ability activated"), BackstabAbility && BackstabAbility->IsActive());
		TestTrue(TEXT("Victim ability active"), VictimAbility && VictimAbility->IsActive());

		UExecutionLockContext* Context = BackstabAbility->GetTestExecutionContext();
		TestNotNull(TEXT("Backstab Execution context exists"), Context);

		FGameplayEventData HitPayload;
		HitPayload.EventTag = TagCanonicalHit;
		HitPayload.Instigator = Player;
		HitPayload.Target = Player;
		HitPayload.OptionalObject = SyntheticMontage;
		BackstabAbility->TestTriggerHitEvent(HitPayload);

		TestTrue(TEXT("Damage consumed for backstab"), BackstabAbility->IsTestDamageEventConsumed());
		if (Context)
		{
			TestEqual(TEXT("Context transitioned to DeathPending"), Context->GetHitState(), EExecutionSessionHitState::DeathPending);
		}
		TestTrue(TEXT("Enemy is in DeathPending"), LethalEnemy->IsDeathPending());
		TestFalse(TEXT("Enemy is not yet dead before release"), LethalEnemy->IsDead());

		TestEqual(TEXT("Player shake triggered on lethal execution"), Player->GetTestHitFeedbackCameraShakeStartCount(), ShakeCountBefore + 1);
		TestEqual(TEXT("Enemy hit-stop triggered on lethal execution"), LethalEnemy->GetTestCombatImpactHitStopRequestCount(), HitStopCountBefore + 1);
		TestEqual(TEXT("Sound dispatched on lethal execution"), LethalEnemy->GetTestImpactSoundDispatchCount(), SoundCountBefore + 1);
		TestEqual(TEXT("Blood dispatched on lethal execution"), LethalEnemy->GetTestImpactBloodDispatchCount(), BloodCountBefore + 1);

		// Backstab TargetToAttacker: Player at (0,0,0), Target at (150,0,0) -> (-1,0,0)
		const FVector ExpectedBloodNormal(-1.0f, 0.0f, 0.0f);
		TestTrue(TEXT("Backstab blood normal is Target-to-Attacker"), LethalEnemy->GetTestLastImpactBloodNormal().Equals(ExpectedBloodNormal, 1e-3f));

		// Repeated hit is ignored
		BackstabAbility->TestTriggerHitEvent(HitPayload);
		TestEqual(TEXT("Repeated Backstab Hit does not increment shake count"), Player->GetTestHitFeedbackCameraShakeStartCount(), ShakeCountBefore + 1);
		TestEqual(TEXT("Repeated Backstab Hit does not increment hit-stop count"), LethalEnemy->GetTestCombatImpactHitStopRequestCount(), HitStopCountBefore + 1);

		// VictimStart must finalize without triggering second feedback
		FGameplayEventData VictimStartPayload;
		VictimStartPayload.EventTag = TagVictimStart;
		VictimStartPayload.Instigator = Player;
		VictimStartPayload.Target = Player;
		VictimStartPayload.OptionalObject = SyntheticMontage;
		BackstabAbility->TestTriggerVictimStartEvent(VictimStartPayload);

		TestEqual(TEXT("VictimStart does not re-trigger feedback"), Player->GetTestHitFeedbackCameraShakeStartCount(), ShakeCountBefore + 1);
		TestEqual(TEXT("VictimStart does not re-trigger hit-stop"), LethalEnemy->GetTestCombatImpactHitStopRequestCount(), HitStopCountBefore + 1);

		BackstabAbility->TestEndAbility();
		if (VictimAbility && VictimAbility->IsActive())
		{
			VictimAbility->TestEndAbility();
		}
		PlayerASC->ClearAbility(BackstabHandle);
		if (LethalEnemyASC)
		{
			LethalEnemyASC->ClearAbility(VictimHandle);
		}
		AdvanceFeedbackWorldTimer(World, 0.1f);
	}

	// =========================================================================
	// 5. Credential Rejection & Failure Cases (No Feedback Dispatched)
	// =========================================================================
	{
		// Restore health for credential testing
		if (UCharacterAttributeSet* EnemyAttribs = const_cast<UCharacterAttributeSet*>(EnemyASC->GetSet<UCharacterAttributeSet>()))
		{
			EnemyAttribs->SetHealth(100.0f);
			EnemyAttribs->SetPoise(EnemyAttribs->GetMaxPoise());
		}
		const FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
		const FGameplayTag DeathPendingTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.DeathPending")), false);
		EnemyASC->RemoveLooseGameplayTag(DeadTag);
		EnemyASC->RemoveLooseGameplayTag(DeathPendingTag);

		const int32 ShakeCountBefore = Player->GetTestHitFeedbackCameraShakeStartCount();
		const int32 HitStopCountBefore = Enemy->GetTestCombatImpactHitStopRequestCount();
		const int32 SoundCountBefore = Enemy->GetTestImpactSoundDispatchCount();
		const int32 BloodCountBefore = Enemy->GetTestImpactBloodDispatchCount();

		// 5.1 Friendly Fire rejection (Same team)
		FMeleeHitRequest SameTeamRequest;
		SameTeamRequest.SourceActor = Player;
		SameTeamRequest.SourceAbilitySystemComponent = PlayerASC;
		SameTeamRequest.DamageGameplayEffectClass = UTestProjectileDamageGE::StaticClass();
		SameTeamRequest.HitResult.HitObjectHandle = FActorInstanceHandle(Enemy);
		SameTeamRequest.HitResult.Location = Enemy->GetActorLocation();
		SameTeamRequest.HitResult.ImpactPoint = Enemy->GetActorLocation();
		SameTeamRequest.HitResult.ImpactNormal = FVector(-1.0f, 0.0f, 0.0f);

		Enemy->SetTestCombatTeamTag(TagTeamPlayer); // same team
		TestFalse(TEXT("TryResolveHit rejects same team hit"), FMeleeHitResolver::TryResolveHit(SameTeamRequest));
		TestEqual(TEXT("No shake on friendly fire"), Player->GetTestHitFeedbackCameraShakeStartCount(), ShakeCountBefore);
		TestEqual(TEXT("No hit-stop on friendly fire"), Enemy->GetTestCombatImpactHitStopRequestCount(), HitStopCountBefore);
		TestEqual(TEXT("No sound on friendly fire"), Enemy->GetTestImpactSoundDispatchCount(), SoundCountBefore);
		TestEqual(TEXT("No blood on friendly fire"), Enemy->GetTestImpactBloodDispatchCount(), BloodCountBefore);
		Enemy->SetTestCombatTeamTag(TagTeamEnemy); // restore

		// 5.2 Foreign / Unaccepted Context rejection
		UExecutionLockContext* ForeignContext = NewObject<UExecutionLockContext>(GetTransientPackage());
		FMeleeHitRequest ForeignContextRequest = SameTeamRequest;
		ForeignContextRequest.ExecutionContext = ForeignContext;
		TestFalse(TEXT("TryResolveHit rejects uninitialized Context"), FMeleeHitResolver::TryResolveHit(ForeignContextRequest));
		TestEqual(TEXT("No shake on foreign context"), Player->GetTestHitFeedbackCameraShakeStartCount(), ShakeCountBefore);
		TestEqual(TEXT("No hit-stop on foreign context"), Enemy->GetTestCombatImpactHitStopRequestCount(), HitStopCountBefore);

		// 5.3 Dead Source Actor rejection
		PlayerASC->AddLooseGameplayTag(DeadTag);
		TestFalse(TEXT("TryResolveHit rejects hit from dead source"), FMeleeHitResolver::TryResolveHit(SameTeamRequest));
		PlayerASC->RemoveLooseGameplayTag(DeadTag);
		TestEqual(TEXT("No shake when source dead"), Player->GetTestHitFeedbackCameraShakeStartCount(), ShakeCountBefore);

		// 5.4 Dead Target Actor rejection
		EnemyASC->AddLooseGameplayTag(DeadTag);
		TestFalse(TEXT("TryResolveHit rejects hit on dead target"), FMeleeHitResolver::TryResolveHit(SameTeamRequest));
		EnemyASC->RemoveLooseGameplayTag(DeadTag);
		TestEqual(TEXT("No hit-stop when target dead"), Enemy->GetTestCombatImpactHitStopRequestCount(), HitStopCountBefore);

		// 5.5 HandleExecutionImpactFeedback direct validation: State Mismatch (DeathPending expected but not set)
		UExecutionLockContext* DirectContext = NewObject<UExecutionLockContext>(GetTransientPackage());
		UGameplayAbility* DummySourceAbility5 = NewObject<UGameplayAbility>(GetTransientPackage());
		UGameplayAbility* DummyVictimAbility5 = NewObject<UGameplayAbility>(GetTransientPackage());
		const FGameplayTag TagFrontReq5 = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.Front")), false);
		DirectContext->InitializeSession(DummySourceAbility5, Player, PlayerASC, Enemy, TagFrontReq5, 105);
		DirectContext->AcceptVictim(DummyVictimAbility5, Enemy, EnemyASC, TagFrontReq5, 105);
		DirectContext->SetTestHitState(EExecutionSessionHitState::DeathPending);

		FHitResult TestHit;
		TestHit.HitObjectHandle = FActorInstanceHandle(Enemy);
		TestHit.ImpactPoint = Enemy->GetActorLocation();
		TestHit.ImpactNormal = FVector(-1.0f, 0.0f, 0.0f);

		// Enemy is currently alive with 100 HP, NOT in DeathPending -> Must reject DeathPending state
		Enemy->HandleExecutionImpactFeedback(DirectContext, Player, TestHit);
		TestEqual(TEXT("Direct call rejected when enemy not in DeathPending"), Enemy->GetTestCombatImpactHitStopRequestCount(), HitStopCountBefore);

		// 5.6 HandleExecutionImpactFeedback direct validation: NonLethal expected but Enemy in DeathPending
		DirectContext->SetTestHitState(EExecutionSessionHitState::NonLethal);
		EnemyASC->AddLooseGameplayTag(DeathPendingTag);
		Enemy->HandleExecutionImpactFeedback(DirectContext, Player, TestHit);
		TestEqual(TEXT("Direct call rejected when enemy unexpectedly in DeathPending"), Enemy->GetTestCombatImpactHitStopRequestCount(), HitStopCountBefore);
		EnemyASC->RemoveLooseGameplayTag(DeathPendingTag);
	}

	// =========================================================================
	// 6. Config Fallbacks & Edge Cases (No Big Fallback, Invalid HitStop Params)
	// =========================================================================
	{
		AEnemyCharacter* FreshEnemy = FCombatAutomationFixture::SpawnPassiveEnemy(
			World,
			FTransform(FRotator(0.0f, 180.0f, 0.0f), FVector(150.0f, 0.0f, 0.0f)));
		if (!TestNotNull(TEXT("FreshEnemy spawned"), FreshEnemy))
		{
			return false;
		}
		FreshEnemy->SetTestCombatTeamTag(TagTeamEnemy);
		FreshEnemy->SetTestCombatFeedbackData(EnemyFeedback);
		UAbilitySystemComponent* FreshEnemyASC = FreshEnemy->GetAbilitySystemComponent();

		const int32 ShakeCountBefore = Player->GetTestHitFeedbackCameraShakeStartCount();
		const int32 HitStopCountBefore = FreshEnemy->GetTestCombatImpactHitStopRequestCount();
		const int32 SoundCountBefore = FreshEnemy->GetTestImpactSoundDispatchCount();
		const int32 BloodCountBefore = FreshEnemy->GetTestImpactBloodDispatchCount();

		// 6.1 Unconfigured Player Execution Class -> No shake dispatched, NO fallback to Big
		PlayerFeedback->Execution.AttackerImpactCameraShakeClass = nullptr;
		PlayerFeedback->BigTier.AttackerImpactCameraShakeClass = UTestBigHitFeedbackCameraShake::StaticClass();

		Player->TriggerExecutionImpactCameraShake();
		TestEqual(TEXT("No shake dispatched when Execution class is null"), Player->GetTestHitFeedbackCameraShakeStartCount(), ShakeCountBefore);
		PlayerFeedback->Execution.AttackerImpactCameraShakeClass = UTestBigHitFeedbackCameraShake::StaticClass(); // restore

		// 6.2 Invalid HitStop values: duration <= 0, dilation <= 0, dilation > 1, NaN, Inf
		UExecutionLockContext* ValidContext = NewObject<UExecutionLockContext>(GetTransientPackage());
		UGameplayAbility* DummySourceAbility = NewObject<UGameplayAbility>(GetTransientPackage());
		UGameplayAbility* DummyVictimAbility = NewObject<UGameplayAbility>(GetTransientPackage());
		const FGameplayTag FrontReqTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.Front")), false);
		const uint32 TestToken = 201;

		ValidContext->InitializeSession(DummySourceAbility, Player, PlayerASC, FreshEnemy, FrontReqTag, TestToken);
		ValidContext->AcceptVictim(DummyVictimAbility, FreshEnemy, FreshEnemyASC, FrontReqTag, TestToken);
		ValidContext->SetTestHitState(EExecutionSessionHitState::NonLethal);

		FHitResult ValidHit;
		ValidHit.HitObjectHandle = FActorInstanceHandle(FreshEnemy);
		ValidHit.ImpactPoint = FVector(150.0f, 0.0f, 50.0f);
		ValidHit.ImpactNormal = FVector(-1.0f, 0.0f, 0.0f);

		// Zero duration -> Hit-Stop skipped, Sound & Blood still dispatch
		EnemyFeedback->Execution.ImpactHitStopDurationSeconds = 0.0f;
		EnemyFeedback->Execution.ImpactHitStopTimeDilation = 0.03f;
		FreshEnemy->HandleExecutionImpactFeedback(ValidContext, Player, ValidHit);
		TestEqual(TEXT("Hit-stop skipped when duration is 0"), FreshEnemy->GetTestCombatImpactHitStopRequestCount(), HitStopCountBefore);
		TestEqual(TEXT("Sound dispatches despite 0 hit-stop duration"), FreshEnemy->GetTestImpactSoundDispatchCount(), SoundCountBefore + 1);
		TestEqual(TEXT("Blood dispatches despite 0 hit-stop duration"), FreshEnemy->GetTestImpactBloodDispatchCount(), BloodCountBefore + 1);

		// Dilation > 1.0 -> Hit-Stop skipped
		EnemyFeedback->Execution.ImpactHitStopDurationSeconds = 0.05f;
		EnemyFeedback->Execution.ImpactHitStopTimeDilation = 1.5f;
		FreshEnemy->HandleExecutionImpactFeedback(ValidContext, Player, ValidHit);
		TestEqual(TEXT("Hit-stop skipped when dilation > 1.0"), FreshEnemy->GetTestCombatImpactHitStopRequestCount(), HitStopCountBefore);

		// Negative duration -> Hit-Stop skipped
		EnemyFeedback->Execution.ImpactHitStopDurationSeconds = -0.05f;
		EnemyFeedback->Execution.ImpactHitStopTimeDilation = 0.03f;
		FreshEnemy->HandleExecutionImpactFeedback(ValidContext, Player, ValidHit);
		TestEqual(TEXT("Hit-stop skipped when duration is negative"), FreshEnemy->GetTestCombatImpactHitStopRequestCount(), HitStopCountBefore);

		// Restore valid feedback settings
		EnemyFeedback->Execution.ImpactHitStopDurationSeconds = 0.05f;
		EnemyFeedback->Execution.ImpactHitStopTimeDilation = 0.03f;

		// 6.3 Missing / Null profile safe no-op
		FreshEnemy->SetTestCombatFeedbackData(nullptr);
		FreshEnemy->HandleExecutionImpactFeedback(ValidContext, Player, ValidHit);
		TestEqual(TEXT("Null profile does not increment hit-stop count"), FreshEnemy->GetTestCombatImpactHitStopRequestCount(), HitStopCountBefore);
		FreshEnemy->SetTestCombatFeedbackData(EnemyFeedback); // restore

		// =========================================================================
		// 7. HitResult Geometry Validation (Sound fallback & Blood filters)
		// =========================================================================
		const int32 BloodCountBefore7 = FreshEnemy->GetTestImpactBloodDispatchCount();
		const int32 SoundCountBefore7 = FreshEnemy->GetTestImpactSoundDispatchCount();

		// 7.1 Non-finite ImpactPoint -> Sound falls back to Enemy Location, Blood skipped
		FHitResult NonFinitePointHit;
		NonFinitePointHit.HitObjectHandle = FActorInstanceHandle(FreshEnemy);
		NonFinitePointHit.ImpactPoint = FVector(NAN, 0.0f, 0.0f);
		NonFinitePointHit.ImpactNormal = FVector(-1.0f, 0.0f, 0.0f);

		FreshEnemy->HandleExecutionImpactFeedback(ValidContext, Player, NonFinitePointHit);
		TestEqual(TEXT("Sound dispatched on non-finite point"), FreshEnemy->GetTestImpactSoundDispatchCount(), SoundCountBefore7 + 1);
		TestEqual(TEXT("Sound fell back to ActorLocation"), FreshEnemy->GetTestLastImpactSoundLocation(), FreshEnemy->GetActorLocation());
		TestEqual(TEXT("Blood skipped on non-finite point"), FreshEnemy->GetTestImpactBloodDispatchCount(), BloodCountBefore7);

		// 7.2 Zero normal -> Blood skipped
		FHitResult ZeroNormalHit;
		ZeroNormalHit.HitObjectHandle = FActorInstanceHandle(FreshEnemy);
		ZeroNormalHit.ImpactPoint = FVector(150.0f, 0.0f, 50.0f);
		ZeroNormalHit.ImpactNormal = FVector::ZeroVector;

		FreshEnemy->HandleExecutionImpactFeedback(ValidContext, Player, ZeroNormalHit);
		TestEqual(TEXT("Blood skipped on zero normal"), FreshEnemy->GetTestImpactBloodDispatchCount(), BloodCountBefore7);

		// 7.3 Mismatched Actor (targeting Player instead of Enemy) -> Blood skipped
		FHitResult MismatchedActorHit;
		MismatchedActorHit.HitObjectHandle = FActorInstanceHandle(Player);
		MismatchedActorHit.ImpactPoint = FVector(0.0f, 0.0f, 50.0f);
		MismatchedActorHit.ImpactNormal = FVector(1.0f, 0.0f, 0.0f);

		FreshEnemy->HandleExecutionImpactFeedback(ValidContext, Player, MismatchedActorHit);
		TestEqual(TEXT("Blood skipped when HitResult targets different actor"), FreshEnemy->GetTestImpactBloodDispatchCount(), BloodCountBefore7);

		// =========================================================================
		// 8. Normal Combat Feedback Non-Regression
		// =========================================================================
		// Verify normal melee feedback triggers outside execution scope
		const int32 HitStopBeforeNormal = FreshEnemy->GetTestCombatImpactHitStopRequestCount();
		EnemyFeedback->SmallTier.ImpactHitStopDurationSeconds = 0.03f;
		EnemyFeedback->SmallTier.ImpactHitStopTimeDilation = 0.1f;

		FGameplayEffectContextHandle ContextHandle = PlayerASC->MakeEffectContext();
		ContextHandle.AddInstigator(Player, Player);
		FHitResult NormalHit;
		NormalHit.HitObjectHandle = FActorInstanceHandle(FreshEnemy);
		NormalHit.ImpactPoint = FVector(150.0f, 0.0f, 50.0f);
		NormalHit.ImpactNormal = FVector(-1.0f, 0.0f, 0.0f);
		ContextHandle.AddHitResult(NormalHit, true);

		FGameplayEffectSpecHandle SpecHandle = PlayerASC->MakeOutgoingSpec(UTestProjectileDamageGE::StaticClass(), 1.0f, ContextHandle);
		if (SpecHandle.IsValid() && SpecHandle.Data.IsValid())
		{
			FreshEnemyASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
			TestEqual(TEXT("Normal damage outside execution triggers normal hit-stop"), FreshEnemy->GetTestCombatImpactHitStopRequestCount(), HitStopBeforeNormal + 1);
			TestEqual(TEXT("Normal hit-stop uses Small tier duration"), FreshEnemy->GetTestLastImpactHitStopDuration(), 0.03f);
			TestEqual(TEXT("Normal hit-stop uses Small tier dilation"), FreshEnemy->GetTestLastImpactHitStopTimeDilation(), 0.1f);
		}
	}

	return true;
}

#endif
