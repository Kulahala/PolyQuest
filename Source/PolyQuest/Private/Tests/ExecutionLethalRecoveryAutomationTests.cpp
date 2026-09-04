#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/EnemyStanceBreakAbility.h"
#include "AbilitySystem/Abilities/EnemyVictimExecutionAbility.h"
#include "AbilitySystem/Abilities/PlayerBackstabExecutionAbility.h"
#include "AbilitySystem/Abilities/PlayerFrontExecutionAbility.h"
#include "AI/EnemyAIController.h"
#include "Animation/AnimMontage.h"
#include "AbilitySystem/CharacterAttributeSet.h"
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
	FExecutionLethalRecoveryAutomationTest,
	"PolyQuest.Combat.ExecutionLethalRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace ExecutionLethalRecoveryAutomation
{
	struct FExecutionLethalRecoveryWorldScope
	{
		UWorld* World = nullptr;
		~FExecutionLethalRecoveryWorldScope()
		{
			if (World)
			{
				UGameplayStatics::SetGlobalTimeDilation(World, 1.0f);
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
		}
	};

	FGameplayAbilitySpecHandle ActivateEnemyStanceBreakProduction(AEnemyCharacter* InEnemy)
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

bool FExecutionLethalRecoveryAutomationTest::RunTest(const FString& Parameters)
{
	// =========================================================================
	// 1. CDO & Tag Contract Verification
	// =========================================================================
	{
		const FGameplayTag TagDeathPending = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.DeathPending")), false);
		TestTrue(TEXT("Tag State.Status.DeathPending is registered and valid"), TagDeathPending.IsValid());

		const FGameplayTag TagDead = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
		TestTrue(TEXT("Tag State.Status.Dead is registered and valid"), TagDead.IsValid());

		const UEnemyVictimExecutionAbility* VictimCDO = UEnemyVictimExecutionAbility::StaticClass()->GetDefaultObject<UEnemyVictimExecutionAbility>();
		if (!TestNotNull(TEXT("UEnemyVictimExecutionAbility CDO exists"), VictimCDO))
		{
			return false;
		}

		TestTrue(TEXT("Victim CDO requires valid DeathPending tag"), TagDeathPending.IsValid());
	}

	// =========================================================================
	// 2. ExecutionLockContext State Machine & Finalization Rollback Unit Tests
	// =========================================================================
	{
		UExecutionLockContext* Context = NewObject<UExecutionLockContext>();
		TestNotNull(TEXT("Context created"), Context);
		TestEqual(TEXT("Initial HitState is Ready"), Context->GetHitState(), EExecutionSessionHitState::Ready);
		TestFalse(TEXT("Context initially not resolving"), Context->IsResolving());
		TestFalse(TEXT("Context initially not death pending"), Context->IsDeathPending());
		TestFalse(TEXT("Context initially not finalizing"), Context->IsFinalizing());
		TestFalse(TEXT("Context initially not finalized"), Context->IsFinalized());

		UPlayerFrontExecutionAbility* DummySourceAbility = NewObject<UPlayerFrontExecutionAbility>();
		UEnemyVictimExecutionAbility* DummyVictimAbility = NewObject<UEnemyVictimExecutionAbility>();
		AActor* DummySourceActor = NewObject<AActor>();
		AActor* DummyTargetActor = NewObject<AActor>();
		UAbilitySystemComponent* DummySourceASC = NewObject<UAbilitySystemComponent>();
		UAbilitySystemComponent* DummyVictimASC = NewObject<UAbilitySystemComponent>();

		const FGameplayTag FrontReqTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.Front")), false);
		const uint32 TestToken = 101;

		Context->InitializeSession(DummySourceAbility, DummySourceActor, DummySourceASC, DummyTargetActor, FrontReqTag, TestToken);
		Context->AcceptVictim(DummyVictimAbility, DummyTargetActor, DummyVictimASC, FrontReqTag, TestToken);

		// 2.1 State transitions and queries
		Context->SetTestHitState(EExecutionSessionHitState::Ready);
		TestEqual(TEXT("HitState is Ready"), Context->GetHitState(), EExecutionSessionHitState::Ready);

		Context->SetTestHitState(EExecutionSessionHitState::Resolving);
		TestTrue(TEXT("Context is resolving"), Context->IsResolving());

		Context->CompleteHitScope(true, true);
		TestTrue(TEXT("Context is death pending"), Context->IsDeathPending());

		Context->SetTestHitState(EExecutionSessionHitState::Resolving);
		Context->CompleteHitScope(false, false);
		TestEqual(TEXT("Context is non-lethal"), Context->GetHitState(), EExecutionSessionHitState::NonLethal);

		Context->SetTestHitState(EExecutionSessionHitState::Resolving);
		Context->AbortHitScope();
		TestEqual(TEXT("Context is failed"), Context->GetHitState(), EExecutionSessionHitState::Failed);

		// Inactive dummy abilities are strictly rejected by IsHitAuthorized
		TestFalse(TEXT("IsHitAuthorized rejected when abilities inactive"),
			Context->IsHitAuthorized(DummySourceAbility, DummySourceActor, DummySourceASC, DummyTargetActor, DummyVictimASC));

		// 2.2 Release Cancellation Tracking
		Context->MarkReleaseSent(true);
		TestTrue(TEXT("Release sent marked"), Context->IsReleaseSent());
		TestTrue(TEXT("Release cancellation recorded"), Context->WasReleaseCancelled());

		// 2.3 Finalization rollback on failed commit
		Context->SetTestHitState(EExecutionSessionHitState::DeathPending);
		TestTrue(TEXT("BeginFinalization succeeds from DeathPending"), Context->BeginFinalization(DummyVictimAbility));
		TestTrue(TEXT("IsFinalizing true"), Context->IsFinalizing());
		Context->AbortFinalization(DummyVictimAbility);
		TestTrue(TEXT("AbortFinalization rolled back to DeathPending"), Context->IsDeathPending());

		// 2.4 Finalization completion
		TestTrue(TEXT("BeginFinalization re-enters from DeathPending"), Context->BeginFinalization(DummyVictimAbility));
		Context->CompleteFinalization(DummyVictimAbility);
		TestTrue(TEXT("IsFinalized true"), Context->IsFinalized());
	}

	// =========================================================================
	// 3. Test World Setup
	// =========================================================================
	ExecutionLethalRecoveryAutomation::FExecutionLethalRecoveryWorldScope WorldScope;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("ExecutionLethalRecoveryTestWorld"));
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

	if (!TestNotNull(TEXT("Player spawned"), Player)
		|| !TestNotNull(TEXT("Enemy spawned"), Enemy)
		|| !TestNotNull(TEXT("Controller spawned"), Controller))
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

	const FGameplayTag TagDeathPending = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.DeathPending")), false);
	const FGameplayTag TagDead = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
	const FGameplayTag TagPlayerLocked = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Execution.PlayerLocked")), false);
	const FGameplayTag TagVictimLocked = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Execution.VictimLocked")), false);
	const FGameplayTag TagInvulnerable = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Invulnerable")), false);
	const FGameplayTag TagStunned = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);

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

	// =========================================================================
	// 4. Production Front Execution Lethal Chain: Health=0 -> DeathPending -> Release -> Dead
	// =========================================================================
	{
		// 1. Activate production StanceBreak on Enemy via TryActivateAbility
		const FGameplayAbilitySpecHandle StanceBreakHandle = ExecutionLethalRecoveryAutomation::ActivateEnemyStanceBreakProduction(Enemy);
		TestTrue(TEXT("Production StanceBreak activated on Enemy"), StanceBreakHandle.IsValid());
		TestTrue(TEXT("Enemy has Stunned tag from StanceBreak"), EnemyASC->HasMatchingGameplayTag(TagStunned));

		// 2. Grant Victim Ability and Front Ability
		FGameplayAbilitySpec VictimSpec(UEnemyVictimExecutionAbility::StaticClass(), 1, INDEX_NONE, Enemy);
		const FGameplayAbilitySpecHandle VictimHandle = EnemyASC->GiveAbility(VictimSpec);
		FGameplayAbilitySpec* FoundVictimSpec = EnemyASC->FindAbilitySpecFromHandle(VictimHandle);
		UEnemyVictimExecutionAbility* VictimInstance = FoundVictimSpec ? Cast<UEnemyVictimExecutionAbility>(FoundVictimSpec->GetPrimaryInstance()) : nullptr;

		FGameplayAbilitySpec FrontSpec(UPlayerFrontExecutionAbility::StaticClass(), 1, INDEX_NONE, Player);
		const FGameplayAbilitySpecHandle FrontHandle = PlayerASC->GiveAbility(FrontSpec);
		FGameplayAbilitySpec* FoundFrontSpec = PlayerASC->FindAbilitySpecFromHandle(FrontHandle);
		UPlayerFrontExecutionAbility* FrontInstance = FoundFrontSpec ? Cast<UPlayerFrontExecutionAbility>(FoundFrontSpec->GetPrimaryInstance()) : nullptr;

		if (TestNotNull(TEXT("VictimInstance valid"), VictimInstance) && TestNotNull(TEXT("FrontInstance valid"), FrontInstance))
		{
			FrontInstance->SetTestSkipMontageTaskActivation(true);
			UAnimMontage* DummyMontage = NewObject<UAnimMontage>();
			FrontInstance->SetTestExecutionMontage(DummyMontage);
			FrontInstance->SetTestDamageGameplayEffectClass(UTestProjectileDamageGE::StaticClass());
			FrontInstance->SetTestExecutionDistances(0.0f, 250.0f);
			FrontInstance->SetTestMaxFrontAngleDegrees(60.0f);

			Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
			Enemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
			Enemy->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f));
			Player->SetTestLockedTarget(Enemy);

			// Set Enemy Health to 20.0f (so 25 damage GE is lethal)
			if (UCharacterAttributeSet* EnemyAttribs = const_cast<UCharacterAttributeSet*>(EnemyASC->GetSet<UCharacterAttributeSet>()))
			{
				EnemyAttribs->SetHealth(20.0f);
			}

			// Activate Front Execution via production GAS TryActivateAbility
			const bool bActivated = PlayerASC->TryActivateAbility(FrontHandle);
			TestTrue(TEXT("Front Execution activated via production TryActivateAbility"), bActivated);
			TestTrue(TEXT("Victim Ability activated"), VictimInstance->IsActive());
			TestTrue(TEXT("Enemy has VictimLocked"), EnemyASC->HasMatchingGameplayTag(TagVictimLocked));
			TestTrue(TEXT("Enemy has Invulnerable"), EnemyASC->HasMatchingGameplayTag(TagInvulnerable));
			TestTrue(TEXT("Player has PlayerLocked"), PlayerASC->HasMatchingGameplayTag(TagPlayerLocked));

			UExecutionLockContext* ExecContext = FrontInstance->GetTestExecutionContext();
			TestNotNull(TEXT("Active ExecutionLockContext valid"), ExecContext);

			// Trigger lethal execution hit via production GAS HandleGameplayEvent
			const FGameplayTag HitEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Hit")), false);
			FGameplayEventData HitPayload;
			HitPayload.EventTag = HitEventTag;
			HitPayload.Instigator = Player;
			HitPayload.Target = Player;
			HitPayload.OptionalObject = DummyMontage;

			PlayerASC->HandleGameplayEvent(HitEventTag, &HitPayload);

			// Verify lethal state after hit:
			const float CurrentHealth = EnemyASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute());
			TestEqual(TEXT("Enemy Health reduced to 0"), CurrentHealth, 0.0f);
			TestTrue(TEXT("Enemy IsDeathPending() is true"), Enemy->IsDeathPending());
			TestTrue(TEXT("VictimInstance IsDeathPending() is true"), VictimInstance->IsDeathPending());
			TestTrue(TEXT("EnemyASC has State.Status.DeathPending tag"), EnemyASC->HasMatchingGameplayTag(TagDeathPending));
			TestEqual(TEXT("Context HitState is DeathPending"), ExecContext->GetHitState(), EExecutionSessionHitState::DeathPending);

			// Enemy is NOT dead yet!
			TestFalse(TEXT("Enemy IsDead() is FALSE during DeathPending"), Enemy->IsDead());
			TestFalse(TEXT("EnemyASC does NOT have State.Status.Dead yet"), EnemyASC->HasMatchingGameplayTag(TagDead));

			// Paired lock-in, invulnerability, and Lock-On retention remain active
			TestTrue(TEXT("Player still has PlayerLocked during DeathPending"), PlayerASC->HasMatchingGameplayTag(TagPlayerLocked));
			TestTrue(TEXT("Enemy still has VictimLocked during DeathPending"), EnemyASC->HasMatchingGameplayTag(TagVictimLocked));
			TestTrue(TEXT("Enemy still has Invulnerable during DeathPending"), EnemyASC->HasMatchingGameplayTag(TagInvulnerable));
			TestTrue(TEXT("Player retains execution target during DeathPending"),
				Player->TriggerTestCanRetainExecutionLockedTarget(Enemy));

			// Abnormal healing clamped back to 0 during DeathPending
			EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 50.0f);
			const float ClampedHealth = EnemyASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute());
			TestEqual(TEXT("Abnormal healing clamped back to 0 during DeathPending"), ClampedHealth, 0.0f);
			TestTrue(TEXT("Enemy remains DeathPending after healing attempt"), Enemy->IsDeathPending());
			TestFalse(TEXT("Enemy remains not Dead after healing attempt"), Enemy->IsDead());

			// End Player Execution -> Dispatches Release event to Victim
			FrontInstance->TestEndAbility(false);

			// Verify synchronous post-release lethal finalization:
			TestTrue(TEXT("Enemy IsDead() is TRUE after Release"), Enemy->IsDead());
			TestTrue(TEXT("EnemyASC has State.Status.Dead"), EnemyASC->HasMatchingGameplayTag(TagDead));
			TestFalse(TEXT("Enemy IsDeathPending() is FALSE after Release"), Enemy->IsDeathPending());
			TestFalse(TEXT("EnemyASC no longer has State.Status.DeathPending"), EnemyASC->HasMatchingGameplayTag(TagDeathPending));
			TestFalse(TEXT("Enemy released VictimLocked"), EnemyASC->HasMatchingGameplayTag(TagVictimLocked));
			TestFalse(TEXT("Player released PlayerLocked"), PlayerASC->HasMatchingGameplayTag(TagPlayerLocked));
			TestFalse(TEXT("Victim ability ended"), VictimInstance->IsActive());
			TestFalse(TEXT("Front ability ended"), FrontInstance->IsActive());

			// Clean up abilities
			PlayerASC->ClearAbility(FrontHandle);
			EnemyASC->ClearAbility(VictimHandle);
			EnemyASC->ClearAbility(StanceBreakHandle);
			ResetWeaponExecutionMontages(Player);
		}
	}

	// =========================================================================
	// 5. Production Backstab Lethal Chain: Health=0 -> DeathPending -> Release -> Dead
	// =========================================================================
	{
		AEnemyCharacter* Enemy2 = FCombatAutomationFixture::SpawnPassiveEnemy(
			World,
			FTransform(FRotator(0.0f, 0.0f, 0.0f), FVector(150.0f, 0.0f, 0.0f)));
		TestNotNull(TEXT("Enemy2 spawned"), Enemy2);
		Enemy2->SetTestCombatTeamTag(TagTeamEnemy);
		UAbilitySystemComponent* Enemy2ASC = Enemy2->GetAbilitySystemComponent();
		TestNotNull(TEXT("Enemy2 ASC valid"), Enemy2ASC);

		FGameplayAbilitySpec VictimSpec(UEnemyVictimExecutionAbility::StaticClass(), 1, INDEX_NONE, Enemy2);
		const FGameplayAbilitySpecHandle VictimHandle = Enemy2ASC->GiveAbility(VictimSpec);
		FGameplayAbilitySpec* FoundVictimSpec = Enemy2ASC->FindAbilitySpecFromHandle(VictimHandle);
		UEnemyVictimExecutionAbility* VictimInstance = FoundVictimSpec ? Cast<UEnemyVictimExecutionAbility>(FoundVictimSpec->GetPrimaryInstance()) : nullptr;

		FGameplayAbilitySpec BackstabSpec(UPlayerBackstabExecutionAbility::StaticClass(), 1, INDEX_NONE, Player);
		const FGameplayAbilitySpecHandle BackstabHandle = PlayerASC->GiveAbility(BackstabSpec);
		FGameplayAbilitySpec* FoundBackstabSpec = PlayerASC->FindAbilitySpecFromHandle(BackstabHandle);
		UPlayerBackstabExecutionAbility* BackstabInstance = FoundBackstabSpec ? Cast<UPlayerBackstabExecutionAbility>(FoundBackstabSpec->GetPrimaryInstance()) : nullptr;

		if (TestNotNull(TEXT("VictimInstance 2 valid"), VictimInstance) && TestNotNull(TEXT("BackstabInstance valid"), BackstabInstance))
		{
			BackstabInstance->SetTestSkipMontageTaskActivation(true);
			UAnimMontage* DummyMontage = NewObject<UAnimMontage>();
			BackstabInstance->SetTestExecutionMontage(DummyMontage);
			BackstabInstance->SetTestDamageGameplayEffectClass(UTestProjectileDamageGE::StaticClass());
			BackstabInstance->SetTestExecutionDistances(0.0f, 250.0f);
			BackstabInstance->SetTestMaxBackAngleDegrees(60.0f);

			// Position Player behind Enemy2: Enemy facing (1,0,0) at (150,0,0), Player at (0,0,0)
			Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
			Enemy2->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
			Enemy2->SetActorRotation(FRotator(0.0f, 0.0f, 0.0f));
			Player->SetTestLockedTarget(Enemy2);

			// Set Enemy2 Health to 20.0f (so 25 damage GE is lethal)
			if (UCharacterAttributeSet* EnemyAttribs = const_cast<UCharacterAttributeSet*>(Enemy2ASC->GetSet<UCharacterAttributeSet>()))
			{
				EnemyAttribs->SetHealth(20.0f);
			}

			const bool bActivated = PlayerASC->TryActivateAbility(BackstabHandle);
			TestTrue(TEXT("Backstab Execution activated via production TryActivateAbility"), bActivated);
			TestTrue(TEXT("Victim Ability activated on Enemy2"), VictimInstance->IsActive());

			UExecutionLockContext* ExecContext = BackstabInstance->GetTestExecutionContext();
			TestNotNull(TEXT("Backstab ExecutionLockContext valid"), ExecContext);

			// Trigger lethal Backstab hit via production GAS HandleGameplayEvent
			const FGameplayTag HitEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Hit")), false);
			FGameplayEventData HitPayload;
			HitPayload.EventTag = HitEventTag;
			HitPayload.Instigator = Player;
			HitPayload.Target = Player;
			HitPayload.OptionalObject = DummyMontage;

			PlayerASC->HandleGameplayEvent(HitEventTag, &HitPayload);

			// Verify lethal state after Backstab hit:
			const float RemainingHealth = Enemy2ASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute());
			TestEqual(TEXT("Enemy2 Health reduced to 0 by Backstab"), RemainingHealth, 0.0f);
			TestTrue(TEXT("Enemy2 IsDeathPending() is true"), Enemy2->IsDeathPending());
			TestFalse(TEXT("Enemy2 is NOT dead before Release"), Enemy2->IsDead());
			TestEqual(TEXT("Backstab Context HitState is DeathPending"), ExecContext->GetHitState(), EExecutionSessionHitState::DeathPending);

			// End Backstab Execution -> Dispatches Release
			BackstabInstance->TestEndAbility(false);

			TestTrue(TEXT("Enemy2 is Dead after Backstab Release"), Enemy2->IsDead());
			TestTrue(TEXT("Enemy2ASC has State.Status.Dead after Backstab Release"), Enemy2ASC->HasMatchingGameplayTag(TagDead));
			TestFalse(TEXT("Enemy2 cleared DeathPending after Backstab Release"), Enemy2->IsDeathPending());
			TestFalse(TEXT("Enemy2 has no VictimLocked after Release"), Enemy2ASC->HasMatchingGameplayTag(TagVictimLocked));

			PlayerASC->ClearAbility(BackstabHandle);
			Enemy2ASC->ClearAbility(VictimHandle);
			Enemy2->Destroy();
			ResetWeaponExecutionMontages(Player);
		}
	}

	// =========================================================================
	// 6. Foreign / Mismatched Execution Context Rejected Even Without Invulnerable
	// =========================================================================
	{
		AEnemyCharacter* NormalEnemy = FCombatAutomationFixture::SpawnPassiveEnemy(
			World,
			FTransform(FRotator::ZeroRotator, FVector(200.0f, 0.0f, 0.0f)));
		NormalEnemy->SetTestCombatTeamTag(TagTeamEnemy);
		UAbilitySystemComponent* NormalEnemyASC = NormalEnemy->GetAbilitySystemComponent();

		if (UCharacterAttributeSet* Attribs = const_cast<UCharacterAttributeSet*>(NormalEnemyASC->GetSet<UCharacterAttributeSet>()))
		{
			Attribs->SetHealth(100.0f);
		}

		// Ensure NormalEnemy does NOT have Invulnerable tag
		TestFalse(TEXT("NormalEnemy has no Invulnerable tag"), NormalEnemyASC->HasMatchingGameplayTag(TagInvulnerable));

		// Construct foreign / unauthorized execution context
		UExecutionLockContext* ForeignContext = NewObject<UExecutionLockContext>();

		FMeleeHitRequest ForeignRequest;
		ForeignRequest.SourceActor = Player;
		ForeignRequest.SourceAbilitySystemComponent = PlayerASC;
		ForeignRequest.DamageGameplayEffectClass = UTestProjectileDamageGE::StaticClass();
		ForeignRequest.AbilityLevel = 1.0f;
		ForeignRequest.SourceObject = Player;
		ForeignRequest.ExecutionContext = ForeignContext;
		ForeignRequest.HitResult.HitObjectHandle = FActorInstanceHandle(NormalEnemy);
		ForeignRequest.HitResult.Location = NormalEnemy->GetActorLocation();
		ForeignRequest.HitResult.ImpactPoint = NormalEnemy->GetActorLocation();

		// FMeleeHitResolver must strictly reject foreign context even without Invulnerable on target
		const bool bResolved = FMeleeHitResolver::TryResolveHit(ForeignRequest);
		TestFalse(TEXT("TryResolveHit strictly rejects foreign ExecutionContext on non-invulnerable target"), bResolved);
		TestEqual(TEXT("NormalEnemy Health untouched"),
			NormalEnemyASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute()), 100.0f);
		TestEqual(TEXT("ForeignContext HitState remained Ready (not altered)"),
			ForeignContext->GetHitState(), EExecutionSessionHitState::Ready);

		NormalEnemy->Destroy();
	}

	// =========================================================================
	// 7. External DeathPending Tag Count Incremental Preservation
	// =========================================================================
	{
		AEnemyCharacter* EnemyTagTest = FCombatAutomationFixture::SpawnPassiveEnemy(
			World,
			FTransform(FRotator(0.0f, 180.0f, 0.0f), FVector(150.0f, 0.0f, 0.0f)));
		EnemyTagTest->SetTestCombatTeamTag(TagTeamEnemy);
		UAbilitySystemComponent* TagTestASC = EnemyTagTest->GetAbilitySystemComponent();

		// External caller adds 2 loose DeathPending tags
		TagTestASC->AddLooseGameplayTag(TagDeathPending);
		TagTestASC->AddLooseGameplayTag(TagDeathPending);
		TestEqual(TEXT("Initial external DeathPending tag count is 2"), TagTestASC->GetTagCount(TagDeathPending), 2);

		FGameplayAbilitySpec VictimSpec(UEnemyVictimExecutionAbility::StaticClass(), 1, INDEX_NONE, EnemyTagTest);
		const FGameplayAbilitySpecHandle VictimHandle = TagTestASC->GiveAbility(VictimSpec);
		FGameplayAbilitySpec* FoundVictimSpec = TagTestASC->FindAbilitySpecFromHandle(VictimHandle);
		UEnemyVictimExecutionAbility* VictimInstance = FoundVictimSpec ? Cast<UEnemyVictimExecutionAbility>(FoundVictimSpec->GetPrimaryInstance()) : nullptr;

		if (TestNotNull(TEXT("VictimInstance valid for tag test"), VictimInstance))
		{
			// Notify lethal damage adds 1 tag (increment)
			VictimInstance->NotifyLethalDamageReceived();
			TestEqual(TEXT("Tag count becomes 3 after Victim ability notifies lethal"), TagTestASC->GetTagCount(TagDeathPending), 3);

			// Idempotent: repeated notify does not increment again
			VictimInstance->NotifyLethalDamageReceived();
			TestEqual(TEXT("Repeated NotifyLethalDamageReceived is idempotent (count remains 3)"), TagTestASC->GetTagCount(TagDeathPending), 3);

			// Mark as dead so EndAbility clears its loose tag increment
			TagTestASC->AddLooseGameplayTag(TagDead);
			VictimInstance->TestEndAbility(false);

			// Victim ability removed only its own increment (1 count); external 2 counts remain!
			TestEqual(TEXT("Victim ability removed only its own increment; external 2 counts remain intact"),
				TagTestASC->GetTagCount(TagDeathPending), 2);

			// Clean up external tags
			TagTestASC->RemoveLooseGameplayTag(TagDeathPending);
			TagTestASC->RemoveLooseGameplayTag(TagDeathPending);
			TestEqual(TEXT("External tags cleaned up to 0"), TagTestASC->GetTagCount(TagDeathPending), 0);

			TagTestASC->ClearAbility(VictimHandle);
			EnemyTagTest->Destroy();
		}
	}

	// =========================================================================
	// 8. Commit Failure / Dead Tag Failure Fallback (No Zero-Health Non-Dead Zombie)
	// =========================================================================
	{
		AEnemyCharacter* EnemyFallback = FCombatAutomationFixture::SpawnPassiveEnemy(
			World,
			FTransform(FRotator(0.0f, 180.0f, 0.0f), FVector(150.0f, 0.0f, 0.0f)));
		EnemyFallback->SetTestCombatTeamTag(TagTeamEnemy);
		UAbilitySystemComponent* FallbackASC = EnemyFallback->GetAbilitySystemComponent();

		// First add loose DeathPending tag so setting health to 0 doesn't immediately kill the enemy
		FallbackASC->AddLooseGameplayTag(TagDeathPending);
		if (UCharacterAttributeSet* Attribs = const_cast<UCharacterAttributeSet*>(FallbackASC->GetSet<UCharacterAttributeSet>()))
		{
			Attribs->SetHealth(0.0f);
		}
		TestTrue(TEXT("EnemyFallback is in DeathPending"), EnemyFallback->IsDeathPending());
		TestFalse(TEXT("EnemyFallback not dead yet"), EnemyFallback->IsDead());

		// Direct call to CommitExecutionDeath with invalid/null context fails-closed
		const bool bCommitFailed = EnemyFallback->CommitExecutionDeath(nullptr);
		TestFalse(TEXT("CommitExecutionDeath fails-closed on null context"), bCommitFailed);

		// With invalid context, calling CommitExecutionDeath with mismatched target fails
		UExecutionLockContext* MismatchedContext = NewObject<UExecutionLockContext>();
		const bool bMismatchedCommit = EnemyFallback->CommitExecutionDeath(MismatchedContext);
		TestFalse(TEXT("CommitExecutionDeath fails-closed on mismatched context"), bMismatchedCommit);

		// Inactive context cannot CommitExecutionDeath
		UExecutionLockContext* InactiveContext = NewObject<UExecutionLockContext>();
		const bool bInactiveCommit = EnemyFallback->CommitExecutionDeath(InactiveContext);
		TestFalse(TEXT("CommitExecutionDeath fails-closed on inactive context"), bInactiveCommit);

		// Remove loose tag before testing EndAbility fallback
		FallbackASC->RemoveLooseGameplayTag(TagDeathPending);

		// Give victim ability, notify lethal, and end without context:
		// Fallback in EndAbility must detect Health <= 0, call SetDeadState, and ensure enemy is Dead!
		FGameplayAbilitySpec VictimSpec(UEnemyVictimExecutionAbility::StaticClass(), 1, INDEX_NONE, EnemyFallback);
		const FGameplayAbilitySpecHandle VictimHandle = FallbackASC->GiveAbility(VictimSpec);
		FGameplayAbilitySpec* FoundVictimSpec = FallbackASC->FindAbilitySpecFromHandle(VictimHandle);
		UEnemyVictimExecutionAbility* VictimInstance = FoundVictimSpec ? Cast<UEnemyVictimExecutionAbility>(FoundVictimSpec->GetPrimaryInstance()) : nullptr;

		if (TestNotNull(TEXT("VictimInstance valid for fallback test"), VictimInstance))
		{
			VictimInstance->NotifyLethalDamageReceived();
			VictimInstance->TestEndAbility(false);

			// Fallback canonical death ensured enemy is Dead!
			TestTrue(TEXT("Fallback canonical death ensured Enemy is Dead"), EnemyFallback->IsDead());
			TestTrue(TEXT("Fallback canonical death ensured Dead tag exists"), FallbackASC->HasMatchingGameplayTag(TagDead));
			TestFalse(TEXT("DeathPending tag cleared on confirmed death"), EnemyFallback->IsDeathPending());

			FallbackASC->ClearAbility(VictimHandle);
			EnemyFallback->Destroy();
		}
	}

	// =========================================================================
	// 9. UnPossessed While In DeathPending Commits Canonical Death
	// =========================================================================
	{
		AEnemyCharacter* EnemyUnpossess = FCombatAutomationFixture::SpawnPassiveEnemy(
			World,
			FTransform(FRotator(0.0f, 180.0f, 0.0f), FVector(150.0f, 0.0f, 0.0f)));
		EnemyUnpossess->SetTestCombatTeamTag(TagTeamEnemy);
		UAbilitySystemComponent* UnpossessASC = EnemyUnpossess->GetAbilitySystemComponent();

		FGameplayAbilitySpec VictimSpec(UEnemyVictimExecutionAbility::StaticClass(), 1, INDEX_NONE, EnemyUnpossess);
		const FGameplayAbilitySpecHandle VictimHandle = UnpossessASC->GiveAbility(VictimSpec);
		FGameplayAbilitySpec* FoundVictimSpec = UnpossessASC->FindAbilitySpecFromHandle(VictimHandle);
		UEnemyVictimExecutionAbility* VictimInstance = FoundVictimSpec ? Cast<UEnemyVictimExecutionAbility>(FoundVictimSpec->GetPrimaryInstance()) : nullptr;

		AEnemyAIController* AIController = World->SpawnActor<AEnemyAIController>();
		if (TestNotNull(TEXT("AIController spawned for UnPossessed test"), AIController)
			&& TestNotNull(TEXT("VictimInstance valid for UnPossessed test"), VictimInstance))
		{
			AIController->Possess(EnemyUnpossess);

			// Put enemy in DeathPending state
			VictimInstance->NotifyLethalDamageReceived();
			TestTrue(TEXT("Enemy is in DeathPending before UnPossessed"), EnemyUnpossess->IsDeathPending());
			TestFalse(TEXT("Enemy is not dead yet"), EnemyUnpossess->IsDead());

			// Calling UnPossess while in DeathPending must commit canonical death directly
			AIController->UnPossess();

			TestTrue(TEXT("Enemy entered Dead state on UnPossessed during DeathPending"), EnemyUnpossess->IsDead());
			TestTrue(TEXT("Enemy has Dead tag"), UnpossessASC->HasMatchingGameplayTag(TagDead));
			TestFalse(TEXT("DeathPending tag cleared on confirmed death"), EnemyUnpossess->IsDeathPending());

			VictimInstance->TestEndAbility(false);
			UnpossessASC->ClearAbility(VictimHandle);
			AIController->Destroy();
			EnemyUnpossess->Destroy();
		}
	}

	// =========================================================================
	// 10. Inactive / Foreign Context Release Rejected By Victim (Ability Remains Active)
	// =========================================================================
	{
		AEnemyCharacter* EnemyReleaseTest = FCombatAutomationFixture::SpawnPassiveEnemy(
			World,
			FTransform(FRotator(0.0f, 180.0f, 0.0f), FVector(150.0f, 0.0f, 0.0f)));
		EnemyReleaseTest->SetTestCombatTeamTag(TagTeamEnemy);
		UAbilitySystemComponent* ReleaseTestASC = EnemyReleaseTest->GetAbilitySystemComponent();

		const FGameplayAbilitySpecHandle StanceBreakHandle = ExecutionLethalRecoveryAutomation::ActivateEnemyStanceBreakProduction(EnemyReleaseTest);

		FGameplayAbilitySpec VictimSpec(UEnemyVictimExecutionAbility::StaticClass(), 1, INDEX_NONE, EnemyReleaseTest);
		const FGameplayAbilitySpecHandle VictimHandle = ReleaseTestASC->GiveAbility(VictimSpec);
		FGameplayAbilitySpec* FoundVictimSpec = ReleaseTestASC->FindAbilitySpecFromHandle(VictimHandle);
		UEnemyVictimExecutionAbility* VictimInstance = FoundVictimSpec ? Cast<UEnemyVictimExecutionAbility>(FoundVictimSpec->GetPrimaryInstance()) : nullptr;

		FGameplayAbilitySpec FrontSpec(UPlayerFrontExecutionAbility::StaticClass(), 1, INDEX_NONE, Player);
		const FGameplayAbilitySpecHandle FrontHandle = PlayerASC->GiveAbility(FrontSpec);
		FGameplayAbilitySpec* FoundFrontSpec = PlayerASC->FindAbilitySpecFromHandle(FrontHandle);
		UPlayerFrontExecutionAbility* FrontInstance = FoundFrontSpec ? Cast<UPlayerFrontExecutionAbility>(FoundFrontSpec->GetPrimaryInstance()) : nullptr;

		if (TestNotNull(TEXT("VictimInstance valid for Release rejection test"), VictimInstance)
			&& TestNotNull(TEXT("FrontInstance valid for Release rejection test"), FrontInstance))
		{
			FrontInstance->SetTestSkipMontageTaskActivation(true);
			FrontInstance->SetTestExecutionMontage(NewObject<UAnimMontage>());
			FrontInstance->SetTestDamageGameplayEffectClass(UTestProjectileDamageGE::StaticClass());
			FrontInstance->SetTestExecutionDistances(0.0f, 250.0f);
			FrontInstance->SetTestMaxFrontAngleDegrees(60.0f);

			Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
			EnemyReleaseTest->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
			EnemyReleaseTest->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f));
			Player->SetTestLockedTarget(EnemyReleaseTest);

			const bool bActivated = PlayerASC->TryActivateAbility(FrontHandle);
			TestTrue(TEXT("Front Execution activated for Release test"), bActivated);
			TestTrue(TEXT("Victim Ability activated for Release test"), VictimInstance->IsActive());

			UExecutionLockContext* ExecContext = FrontInstance->GetTestExecutionContext();
			TestNotNull(TEXT("Active ExecContext valid"), ExecContext);

			const FGameplayTag ReleaseTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Release")), false);
			const FGameplayTag FrontReqTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.Front")), false);

			// Premature Release (before IsReleaseSent) is rejected
			FGameplayEventData PrematurePayload;
			PrematurePayload.EventTag = ReleaseTag;
			PrematurePayload.Instigator = Player;
			PrematurePayload.Target = EnemyReleaseTest;
			PrematurePayload.OptionalObject = ExecContext;

			VictimInstance->TestTriggerReleaseEvent(PrematurePayload);
			TestTrue(TEXT("Victim ability remains active after premature Release"), VictimInstance->IsActive());
			TestTrue(TEXT("VictimLocked remains intact after premature Release"), ReleaseTestASC->HasMatchingGameplayTag(TagVictimLocked));
			TestFalse(TEXT("DeathPending not prematurely set after premature Release"), EnemyReleaseTest->IsDeathPending());
			TestTrue(TEXT("Context remains active after premature Release"), ExecContext->IsActive());

			// Mark release sent so we can test payload validation
			ExecContext->MarkReleaseSent(false);

			// Case A: null Instigator Release is rejected
			FGameplayEventData NullInstigatorPayload;
			NullInstigatorPayload.EventTag = ReleaseTag;
			NullInstigatorPayload.Instigator = nullptr;
			NullInstigatorPayload.Target = EnemyReleaseTest;
			NullInstigatorPayload.OptionalObject = ExecContext;

			VictimInstance->TestTriggerReleaseEvent(NullInstigatorPayload);
			TestTrue(TEXT("Victim ability remains active after null Instigator Release"), VictimInstance->IsActive());
			TestTrue(TEXT("VictimLocked remains intact after null Instigator Release"), ReleaseTestASC->HasMatchingGameplayTag(TagVictimLocked));
			TestFalse(TEXT("DeathPending not prematurely set after null Instigator Release"), EnemyReleaseTest->IsDeathPending());
			TestTrue(TEXT("Context remains active after null Instigator Release"), ExecContext->IsActive());

			// Case B: null Target Release is rejected
			FGameplayEventData NullTargetPayload;
			NullTargetPayload.EventTag = ReleaseTag;
			NullTargetPayload.Instigator = Player;
			NullTargetPayload.Target = nullptr;
			NullTargetPayload.OptionalObject = ExecContext;

			VictimInstance->TestTriggerReleaseEvent(NullTargetPayload);
			TestTrue(TEXT("Victim ability remains active after null Target Release"), VictimInstance->IsActive());
			TestTrue(TEXT("VictimLocked remains intact after null Target Release"), ReleaseTestASC->HasMatchingGameplayTag(TagVictimLocked));
			TestFalse(TEXT("DeathPending not prematurely set after null Target Release"), EnemyReleaseTest->IsDeathPending());
			TestTrue(TEXT("Context remains active after null Target Release"), ExecContext->IsActive());

			// Case C: Wrong EventTag Release is rejected
			FGameplayEventData WrongTagPayload;
			WrongTagPayload.EventTag = FrontReqTag;
			WrongTagPayload.Instigator = Player;
			WrongTagPayload.Target = EnemyReleaseTest;
			WrongTagPayload.OptionalObject = ExecContext;

			VictimInstance->TestTriggerReleaseEvent(WrongTagPayload);
			TestTrue(TEXT("Victim ability remains active after wrong tag Release"), VictimInstance->IsActive());
			TestTrue(TEXT("VictimLocked remains intact after wrong tag Release"), ReleaseTestASC->HasMatchingGameplayTag(TagVictimLocked));
			TestFalse(TEXT("DeathPending not prematurely set after wrong tag Release"), EnemyReleaseTest->IsDeathPending());
			TestTrue(TEXT("Context remains active after wrong tag Release"), ExecContext->IsActive());

			// Case D: Foreign context Release is rejected
			UExecutionLockContext* ForeignContext = NewObject<UExecutionLockContext>();
			FGameplayEventData ForeignPayload;
			ForeignPayload.EventTag = ReleaseTag;
			ForeignPayload.Instigator = Player;
			ForeignPayload.Target = EnemyReleaseTest;
			ForeignPayload.OptionalObject = ForeignContext;

			VictimInstance->TestTriggerReleaseEvent(ForeignPayload);
			TestTrue(TEXT("Victim ability remains active after foreign context Release"), VictimInstance->IsActive());
			TestTrue(TEXT("VictimLocked remains intact after foreign context Release"), ReleaseTestASC->HasMatchingGameplayTag(TagVictimLocked));
			TestFalse(TEXT("DeathPending not prematurely set after foreign context Release"), EnemyReleaseTest->IsDeathPending());
			TestTrue(TEXT("Context remains active after foreign context Release"), ExecContext->IsActive());

			// Case E: Finally send complete valid Release, verifying standard cleanup succeeds
			FGameplayEventData ValidReleasePayload;
			ValidReleasePayload.EventTag = ReleaseTag;
			ValidReleasePayload.Instigator = Player;
			ValidReleasePayload.Target = EnemyReleaseTest;
			ValidReleasePayload.OptionalObject = ExecContext;

			VictimInstance->TestTriggerReleaseEvent(ValidReleasePayload);
			TestFalse(TEXT("Victim ability ended successfully after valid Release"), VictimInstance->IsActive());
			TestFalse(TEXT("VictimLocked cleared after valid Release"), ReleaseTestASC->HasMatchingGameplayTag(TagVictimLocked));

			PlayerASC->ClearAbility(FrontHandle);
			ReleaseTestASC->ClearAbility(VictimHandle);
			ReleaseTestASC->ClearAbility(StanceBreakHandle);
			EnemyReleaseTest->Destroy();
			ResetWeaponExecutionMontages(Player);
		}
	}

	// =========================================================================
	// 11. Lethal GE Succeeded but Pending Unconfirmed -> Canonical Death Fallback, Resolver Returns False
	// =========================================================================
	{
		AEnemyCharacter* EnemyUnconfirmed = FCombatAutomationFixture::SpawnPassiveEnemy(
			World,
			FTransform(FRotator(0.0f, 180.0f, 0.0f), FVector(150.0f, 0.0f, 0.0f)));
		EnemyUnconfirmed->SetTestCombatTeamTag(TagTeamEnemy);
		UAbilitySystemComponent* UnconfirmedASC = EnemyUnconfirmed->GetAbilitySystemComponent();

		const FGameplayAbilitySpecHandle StanceBreakHandle = ExecutionLethalRecoveryAutomation::ActivateEnemyStanceBreakProduction(EnemyUnconfirmed);

		if (UCharacterAttributeSet* Attribs = const_cast<UCharacterAttributeSet*>(UnconfirmedASC->GetSet<UCharacterAttributeSet>()))
		{
			Attribs->SetHealth(20.0f);
		}

		FGameplayAbilitySpec VictimSpec(UEnemyVictimExecutionAbility::StaticClass(), 1, INDEX_NONE, EnemyUnconfirmed);
		const FGameplayAbilitySpecHandle VictimHandle = UnconfirmedASC->GiveAbility(VictimSpec);
		FGameplayAbilitySpec* FoundVictimSpec = UnconfirmedASC->FindAbilitySpecFromHandle(VictimHandle);
		UEnemyVictimExecutionAbility* VictimInstance = FoundVictimSpec ? Cast<UEnemyVictimExecutionAbility>(FoundVictimSpec->GetPrimaryInstance()) : nullptr;

		FGameplayAbilitySpec FrontSpec(UPlayerFrontExecutionAbility::StaticClass(), 1, INDEX_NONE, Player);
		const FGameplayAbilitySpecHandle FrontHandle = PlayerASC->GiveAbility(FrontSpec);
		FGameplayAbilitySpec* FoundFrontSpec = PlayerASC->FindAbilitySpecFromHandle(FrontHandle);
		UPlayerFrontExecutionAbility* FrontInstance = FoundFrontSpec ? Cast<UPlayerFrontExecutionAbility>(FoundFrontSpec->GetPrimaryInstance()) : nullptr;

		if (TestNotNull(TEXT("VictimInstance valid for fallback test"), VictimInstance)
			&& TestNotNull(TEXT("FrontInstance valid for fallback test"), FrontInstance))
		{
			FrontInstance->SetTestSkipMontageTaskActivation(true);
			FrontInstance->SetTestExecutionMontage(NewObject<UAnimMontage>());
			FrontInstance->SetTestDamageGameplayEffectClass(UTestProjectileDamageGE::StaticClass());
			FrontInstance->SetTestExecutionDistances(0.0f, 250.0f);
			FrontInstance->SetTestMaxFrontAngleDegrees(60.0f);

			Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
			EnemyUnconfirmed->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
			EnemyUnconfirmed->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f));
			Player->SetTestLockedTarget(EnemyUnconfirmed);

			const bool bActivated = PlayerASC->TryActivateAbility(FrontHandle);
			TestTrue(TEXT("Front execution activated for unconfirmed pending test"), bActivated);

			UExecutionLockContext* ExecContext = FrontInstance->GetTestExecutionContext();
			TestNotNull(TEXT("ExecContext valid"), ExecContext);

			// Clear ActiveVictimExecutionAbility on enemy so OnHealthAttributeChanged cannot notify lethal damage!
			EnemyUnconfirmed->ClearExecutionVictimAbility(VictimInstance);

			FMeleeHitRequest Request;
			Request.SourceActor = Player;
			Request.SourceAbilitySystemComponent = PlayerASC;
			Request.DamageGameplayEffectClass = UTestProjectileDamageGE::StaticClass();
			Request.AbilityLevel = 1.0f;
			Request.SourceObject = FrontInstance;
			Request.ExecutionContext = ExecContext;
			Request.HitResult.HitObjectHandle = FActorInstanceHandle(EnemyUnconfirmed);
			Request.HitResult.Location = EnemyUnconfirmed->GetActorLocation();
			Request.HitResult.ImpactPoint = EnemyUnconfirmed->GetActorLocation();

			// Resolver must return false, and trigger canonical death fallback immediately
			const bool bResolved = FMeleeHitResolver::TryResolveHit(Request);
			TestFalse(TEXT("TryResolveHit returns false when pending cannot be confirmed"), bResolved);
			TestTrue(TEXT("Canonical death fallback executed: Enemy is Dead"), EnemyUnconfirmed->IsDead());
			TestTrue(TEXT("Enemy has Dead tag"), UnconfirmedASC->HasMatchingGameplayTag(TagDead));
			TestEqual(TEXT("Context HitState aborted to Failed"), ExecContext->GetHitState(), EExecutionSessionHitState::Failed);

			PlayerASC->ClearAbility(FrontHandle);
			UnconfirmedASC->ClearAbility(VictimHandle);
			UnconfirmedASC->ClearAbility(StanceBreakHandle);
			EnemyUnconfirmed->Destroy();
			ResetWeaponExecutionMontages(Player);
		}
	}

	// =========================================================================
	// 12. Failed Finalization -> Target Re-entry Blocked, Old Pending Ownership Retained
	// =========================================================================
	{
		AEnemyCharacter* EnemyPending = FCombatAutomationFixture::SpawnPassiveEnemy(
			World,
			FTransform(FRotator(0.0f, 0.0f, 0.0f), FVector(150.0f, 0.0f, 0.0f)));
		EnemyPending->SetTestCombatTeamTag(TagTeamEnemy);
		UAbilitySystemComponent* PendingASC = EnemyPending->GetAbilitySystemComponent();

		// Add DeathPending tag BEFORE setting health to 0.0f so OnHealthAttributeChanged doesn't call SetDeadState()
		PendingASC->AddLooseGameplayTag(TagDeathPending);
		if (UCharacterAttributeSet* Attribs = const_cast<UCharacterAttributeSet*>(PendingASC->GetSet<UCharacterAttributeSet>()))
		{
			Attribs->SetHealth(0.0f);
		}
		TestTrue(TEXT("Enemy is in DeathPending"), EnemyPending->IsDeathPending());
		TestFalse(TEXT("Enemy is not Dead yet"), EnemyPending->IsDead());

		// 1. Try to activate Player Backstab Execution on this enemy
		Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
		Player->SetTestLockedTarget(EnemyPending);

		FGameplayAbilitySpec BackstabSpec(UPlayerBackstabExecutionAbility::StaticClass(), 1, INDEX_NONE, Player);
		const FGameplayAbilitySpecHandle BackstabHandle = PlayerASC->GiveAbility(BackstabSpec);
		const bool bBackstabActivated = PlayerASC->TryActivateAbility(BackstabHandle);
		TestFalse(TEXT("Player Backstab Execution rejected against target in DeathPending / Health<=0"), bBackstabActivated);

		// 2. Try to activate Enemy Victim Execution on this enemy
		FGameplayAbilitySpec VictimSpec(UEnemyVictimExecutionAbility::StaticClass(), 1, INDEX_NONE, EnemyPending);
		const FGameplayAbilitySpecHandle VictimHandle = PendingASC->GiveAbility(VictimSpec);
		const bool bVictimActivated = PendingASC->TryActivateAbility(VictimHandle);
		TestFalse(TEXT("Victim Execution Ability rejected when target in DeathPending / Health<=0"), bVictimActivated);

		// 3. Verify old pending ownership was NOT swallowed
		TestTrue(TEXT("Old DeathPending tag remains intact on enemy"), PendingASC->HasMatchingGameplayTag(TagDeathPending));
		TestTrue(TEXT("Enemy still IsDeathPending()"), EnemyPending->IsDeathPending());
		TestFalse(TEXT("Enemy still not dead"), EnemyPending->IsDead());

		// Clean up
		PendingASC->RemoveLooseGameplayTag(TagDeathPending);
		PlayerASC->ClearAbility(BackstabHandle);
		PendingASC->ClearAbility(VictimHandle);
		EnemyPending->Destroy();
		ResetWeaponExecutionMontages(Player);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
