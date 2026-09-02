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
	FExecutionLockInAutomationTest,
	"PolyQuest.Combat.ExecutionLockIn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace ExecutionLockInAutomation
{
	struct FExecutionLockInWorldScope
	{
		UWorld* World = nullptr;
		~FExecutionLockInWorldScope()
		{
			if (World)
			{
				UGameplayStatics::SetGlobalTimeDilation(World, 1.0f);
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
		}
	};
}

bool FExecutionLockInAutomationTest::RunTest(const FString& Parameters)
{
	// =========================================================================
	// 1. CDO & Tag Contract Verification
	// =========================================================================
	{
		// 1.1 Victim Ability CDO
		const UEnemyVictimExecutionAbility* VictimCDO = UEnemyVictimExecutionAbility::StaticClass()->GetDefaultObject<UEnemyVictimExecutionAbility>();
		if (!TestNotNull(TEXT("UEnemyVictimExecutionAbility CDO exists"), VictimCDO))
		{
			return false;
		}

		TestEqual(TEXT("Victim InstancingPolicy is InstancedPerActor"),
			VictimCDO->GetInstancingPolicy(),
			EGameplayAbilityInstancingPolicy::InstancedPerActor);

		TestEqual(TEXT("Victim NetExecutionPolicy is ServerOnly"),
			VictimCDO->GetNetExecutionPolicy(),
			EGameplayAbilityNetExecutionPolicy::ServerOnly);

		const FGameplayTag TagVictimAbility = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.Execution.Victim")), false);
		const FGameplayTag TagTeardownOnUnpossess = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.Teardown.OnUnpossess")), false);
		const FGameplayTag TagVictimLocked = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Execution.VictimLocked")), false);
		const FGameplayTag TagPlayerLocked = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Execution.PlayerLocked")), false);
		const FGameplayTag TagInvulnerable = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Invulnerable")), false);
		const FGameplayTag TagStunned = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
		const FGameplayTag TagDead = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
		const FGameplayTag TagBlockMovement = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Movement")), false);
		const FGameplayTag TagBlockJump = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Jump")), false);
		const FGameplayTag TagFrontReq = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.Front")), false);
		const FGameplayTag TagBackstabReq = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.Backstab")), false);
		const FGameplayTag TagRelease = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Release")), false);

		TestTrue(TEXT("Tag Ability.Action.Execution.Victim is valid"), TagVictimAbility.IsValid());
		TestTrue(TEXT("Tag State.Action.Execution.VictimLocked is valid"), TagVictimLocked.IsValid());
		TestTrue(TEXT("Tag State.Action.Execution.PlayerLocked is valid"), TagPlayerLocked.IsValid());
		TestTrue(TEXT("Tag Event.Action.Execution.Request.Front is valid"), TagFrontReq.IsValid());
		TestTrue(TEXT("Tag Event.Action.Execution.Request.Backstab is valid"), TagBackstabReq.IsValid());
		TestTrue(TEXT("Tag Event.Action.Execution.Release is valid"), TagRelease.IsValid());

		TestTrue(TEXT("Victim AbilityTags has Ability.Action.Execution.Victim"), VictimCDO->GetTestAbilityTags().HasTagExact(TagVictimAbility));
		TestTrue(TEXT("Victim AbilityTags has Teardown.OnUnpossess"), VictimCDO->GetTestAbilityTags().HasTagExact(TagTeardownOnUnpossess));

		TestTrue(TEXT("Victim OwnedTags has VictimLocked"), VictimCDO->GetTestActivationOwnedTags().HasTagExact(TagVictimLocked));
		TestTrue(TEXT("Victim OwnedTags has Invulnerable"), VictimCDO->GetTestActivationOwnedTags().HasTagExact(TagInvulnerable));
		TestTrue(TEXT("Victim OwnedTags has Stunned"), VictimCDO->GetTestActivationOwnedTags().HasTagExact(TagStunned));
		TestTrue(TEXT("Victim OwnedTags has Block.Movement"), VictimCDO->GetTestActivationOwnedTags().HasTagExact(TagBlockMovement));
		TestTrue(TEXT("Victim OwnedTags has Block.Jump"), VictimCDO->GetTestActivationOwnedTags().HasTagExact(TagBlockJump));

		TestTrue(TEXT("Victim BlockedTags has Dead"), VictimCDO->GetTestActivationBlockedTags().HasTagExact(TagDead));
		TestTrue(TEXT("Victim BlockedTags has VictimLocked"), VictimCDO->GetTestActivationBlockedTags().HasTagExact(TagVictimLocked));
		TestFalse(TEXT("Victim BlockedTags does NOT have Stunned"), VictimCDO->GetTestActivationBlockedTags().HasTagExact(TagStunned));

		// 1.2 Front Execution CDO
		const UPlayerFrontExecutionAbility* FrontCDO = UPlayerFrontExecutionAbility::StaticClass()->GetDefaultObject<UPlayerFrontExecutionAbility>();
		TestNotNull(TEXT("FrontCDO exists"), FrontCDO);
		TestTrue(TEXT("Front OwnedTags has PlayerLocked"), FrontCDO->GetTestActivationOwnedTags().HasTagExact(TagPlayerLocked));
		TestTrue(TEXT("Front OwnedTags has Invulnerable"), FrontCDO->GetTestActivationOwnedTags().HasTagExact(TagInvulnerable));
		TestTrue(TEXT("Front BlockedTags has PlayerLocked"), FrontCDO->GetTestActivationBlockedTags().HasTagExact(TagPlayerLocked));
		TestTrue(TEXT("Front BlockedTags has VictimLocked"), FrontCDO->GetTestActivationBlockedTags().HasTagExact(TagVictimLocked));

		// 1.3 Backstab Execution CDO
		const UPlayerBackstabExecutionAbility* BackstabCDO = UPlayerBackstabExecutionAbility::StaticClass()->GetDefaultObject<UPlayerBackstabExecutionAbility>();
		TestNotNull(TEXT("BackstabCDO exists"), BackstabCDO);
		TestTrue(TEXT("Backstab OwnedTags has PlayerLocked"), BackstabCDO->GetTestActivationOwnedTags().HasTagExact(TagPlayerLocked));
		TestTrue(TEXT("Backstab OwnedTags has Invulnerable"), BackstabCDO->GetTestActivationOwnedTags().HasTagExact(TagInvulnerable));
		TestTrue(TEXT("Backstab BlockedTags has PlayerLocked"), BackstabCDO->GetTestActivationBlockedTags().HasTagExact(TagPlayerLocked));
		TestTrue(TEXT("Backstab BlockedTags has VictimLocked"), BackstabCDO->GetTestActivationBlockedTags().HasTagExact(TagVictimLocked));

		// 1.4 Enemy Stance Break CDO
		const UEnemyStanceBreakAbility* StanceBreakCDO = UEnemyStanceBreakAbility::StaticClass()->GetDefaultObject<UEnemyStanceBreakAbility>();
		TestNotNull(TEXT("StanceBreakCDO exists"), StanceBreakCDO);
		TestTrue(TEXT("StanceBreak BlockedTags has VictimLocked"), StanceBreakCDO->GetTestActivationBlockedTags().HasTagExact(TagVictimLocked));
	}

	// =========================================================================
	// 2. ExecutionLockContext Unit Tests
	// =========================================================================
	{
		UExecutionLockContext* Context = NewObject<UExecutionLockContext>();
		TestNotNull(TEXT("UExecutionLockContext created"), Context);
		TestFalse(TEXT("Context initially inactive"), Context->IsActive());
		TestFalse(TEXT("Context initially not victim accepted"), Context->IsVictimAccepted());

		UPlayerFrontExecutionAbility* DummySourceAbility = NewObject<UPlayerFrontExecutionAbility>();
		UEnemyVictimExecutionAbility* DummyVictimAbility = NewObject<UEnemyVictimExecutionAbility>();
		AActor* DummySourceActor = NewObject<AActor>();
		AActor* DummyTargetActor = NewObject<AActor>();
		UAbilitySystemComponent* DummySourceASC = NewObject<UAbilitySystemComponent>();
		UAbilitySystemComponent* DummyVictimASC = NewObject<UAbilitySystemComponent>();

		const FGameplayTag FrontReqTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.Front")), false);
		const uint32 TestToken = 42;

		Context->InitializeSession(DummySourceAbility, DummySourceActor, DummySourceASC, DummyTargetActor, FrontReqTag, TestToken);
		TestTrue(TEXT("Context active after initialize"), Context->IsActive());
		TestFalse(TEXT("Context not yet victim accepted"), Context->IsVictimAccepted());
		TestEqual(TEXT("Activation token matches"), Context->GetSourceActivationToken(), TestToken);
		TestTrue(TEXT("IsCurrent for source ability with matching token"), Context->IsCurrent(DummySourceAbility, TestToken));
		TestFalse(TEXT("IsCurrent for source ability with wrong token"), Context->IsCurrent(DummySourceAbility, 999));

		// Accept with mismatched token or tag should fail
		TestFalse(TEXT("Accept with mismatched tag fails"),
			Context->AcceptVictim(DummyVictimAbility, DummyTargetActor, DummyVictimASC, FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.Backstab")), false), TestToken));
		TestFalse(TEXT("Accept with mismatched token fails"),
			Context->AcceptVictim(DummyVictimAbility, DummyTargetActor, DummyVictimASC, FrontReqTag, 999));

		// Correct accept succeeds
		TestTrue(TEXT("Accept with correct credentials succeeds"),
			Context->AcceptVictim(DummyVictimAbility, DummyTargetActor, DummyVictimASC, FrontReqTag, TestToken));
		TestTrue(TEXT("Context is victim accepted"), Context->IsVictimAccepted());
		TestTrue(TEXT("IsCurrent for victim ability"), Context->IsCurrent(DummyVictimAbility, 0));

		// Double accept fails
		TestFalse(TEXT("Double accept fails"),
			Context->AcceptVictim(DummyVictimAbility, DummyTargetActor, DummyVictimASC, FrontReqTag, TestToken));

		// Invalidation
		Context->InvalidateSession();
		TestFalse(TEXT("Context inactive after invalidate"), Context->IsActive());
		TestFalse(TEXT("IsCurrent false after invalidate"), Context->IsCurrent(DummySourceAbility, TestToken));
		TestFalse(TEXT("IsHitAuthorized false after invalidate"),
			Context->IsHitAuthorized(DummySourceAbility, DummySourceActor, DummySourceASC, DummyTargetActor, DummyVictimASC));
	}

	// =========================================================================
	// 3. Test World Integration Setup
	// =========================================================================
	ExecutionLockInAutomation::FExecutionLockInWorldScope WorldScope;
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("ExecutionLockInTestWorld"));
	WorldScope.World = World;
	if (!TestNotNull(TEXT("World created successfully"), World))
	{
		return false;
	}

	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(World);

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

	// =========================================================================
	// 4. MeleeHitResolver Authorization Tests
	// =========================================================================
	{
		const FGameplayTag TagInvulnerable = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Invulnerable")), false);
		EnemyASC->AddLooseGameplayTag(TagInvulnerable);

		FMeleeHitRequest Request;
		Request.SourceActor = Player;
		Request.SourceAbilitySystemComponent = PlayerASC;
		Request.DamageGameplayEffectClass = UTestProjectileDamageGE::StaticClass();
		Request.AbilityLevel = 1.0f;
		Request.HitResult.HitObjectHandle = FActorInstanceHandle(Enemy);
		Request.HitResult.Location = Enemy->GetActorLocation();

		// Case 4.1: Normal attack against Invulnerable target without ExecutionContext -> rejected
		Request.ExecutionContext = nullptr;
		TestFalse(TEXT("Normal attack rejected on Invulnerable target"), FMeleeHitResolver::TryResolveHit(Request));

		// Case 4.2: Unauthorized ExecutionContext -> rejected
		UExecutionLockContext* AuthContext = NewObject<UExecutionLockContext>(Player);
		Request.ExecutionContext = AuthContext;
		TestFalse(TEXT("Attack with uninitialized ExecutionContext rejected on Invulnerable target"), FMeleeHitResolver::TryResolveHit(Request));

		EnemyASC->RemoveLooseGameplayTag(TagInvulnerable);
	}

	// =========================================================================
	// 5. Front Execution -> Victim Lock Synchronous Handshake
	// =========================================================================
	{
		// Grant abilities
		FGameplayAbilitySpec VictimSpec(UEnemyVictimExecutionAbility::StaticClass(), 1, INDEX_NONE, Enemy);
		EnemyASC->GiveAbility(VictimSpec);

		FGameplayAbilitySpec FrontSpec(UPlayerFrontExecutionAbility::StaticClass(), 1, INDEX_NONE, Player);
		const FGameplayAbilitySpecHandle FrontHandle = PlayerASC->GiveAbility(FrontSpec);
		FGameplayAbilitySpec* FoundFrontSpec = PlayerASC->FindAbilitySpecFromHandle(FrontHandle);
		UPlayerFrontExecutionAbility* FrontInstance = FoundFrontSpec ? Cast<UPlayerFrontExecutionAbility>(FoundFrontSpec->GetPrimaryInstance()) : nullptr;

		if (TestNotNull(TEXT("FrontInstance valid"), FrontInstance))
		{
			FrontInstance->SetTestSkipMontageTaskActivation(true);
			FrontInstance->SetTestExecutionMontage(NewObject<UAnimMontage>());
			FrontInstance->SetTestDamageGameplayEffectClass(UTestProjectileDamageGE::StaticClass());
			FrontInstance->SetTestExecutionDistances(0.0f, 250.0f);
			FrontInstance->SetTestMaxFrontAngleDegrees(60.0f);

			// Position Player & Enemy facing each other
			Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
			Enemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
			Enemy->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f));
			Player->SetTestLockedTarget(Enemy);

			// Set Enemy Poise to 0 and activate EnemyStanceBreakAbility
			if (UCharacterAttributeSet* EnemyAttribs = const_cast<UCharacterAttributeSet*>(EnemyASC->GetSet<UCharacterAttributeSet>()))
			{
				EnemyAttribs->SetPoise(0.0f);
			}

			FGameplayAbilitySpec StanceBreakSpec(UEnemyStanceBreakAbility::StaticClass(), 1, INDEX_NONE, Enemy);
			const FGameplayAbilitySpecHandle StanceBreakHandle = EnemyASC->GiveAbility(StanceBreakSpec);
			if (FGameplayAbilitySpec* FoundSBSpec = EnemyASC->FindAbilitySpecFromHandle(StanceBreakHandle))
			{
				FoundSBSpec->ActivationInfo.SetActivationConfirmed();
				FoundSBSpec->ActiveCount = 1;
			}
			const FGameplayTag TagStunned = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
			EnemyASC->AddLooseGameplayTag(TagStunned);
			TestTrue(TEXT("Enemy has Stunned from StanceBreak"), EnemyASC->HasMatchingGameplayTag(TagStunned));

			// Activate Front Execution (commits ability and requests victim execution)
			const bool bActivated = PlayerASC->TryActivateAbility(FrontHandle);
			TestTrue(TEXT("Front Execution activated"), bActivated);

			// Remove the pre-execution simulated StanceBreak tag so only the Victim Ability's owned tag remains
			EnemyASC->RemoveLooseGameplayTag(TagStunned);

			const FGameplayTag TagPlayerLocked = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Execution.PlayerLocked")), false);
			const FGameplayTag TagVictimLocked = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Execution.VictimLocked")), false);
			const FGameplayTag TagInvulnerable = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Invulnerable")), false);

			TestTrue(TEXT("Player has PlayerLocked tag"), PlayerASC->HasMatchingGameplayTag(TagPlayerLocked));
			TestTrue(TEXT("Player has Invulnerable tag"), PlayerASC->HasMatchingGameplayTag(TagInvulnerable));
			TestTrue(TEXT("Enemy has VictimLocked tag"), EnemyASC->HasMatchingGameplayTag(TagVictimLocked));
			TestTrue(TEXT("Enemy has Invulnerable tag"), EnemyASC->HasMatchingGameplayTag(TagInvulnerable));
			TestTrue(TEXT("Enemy continues to have Stunned during Front Execution (owned by Victim Ability)"), EnemyASC->HasMatchingGameplayTag(TagStunned));

			// End Front Execution (sends Release event to Enemy)
			FrontInstance->TestEndAbility(false);

			TestFalse(TEXT("Player released PlayerLocked tag"), PlayerASC->HasMatchingGameplayTag(TagPlayerLocked));
			TestFalse(TEXT("Player released Invulnerable tag"), PlayerASC->HasMatchingGameplayTag(TagInvulnerable));
			TestFalse(TEXT("Enemy released VictimLocked tag"), EnemyASC->HasMatchingGameplayTag(TagVictimLocked));
			TestFalse(TEXT("Enemy released Invulnerable tag"), EnemyASC->HasMatchingGameplayTag(TagInvulnerable));
			TestFalse(TEXT("Enemy released Stunned tag automatically"), EnemyASC->HasMatchingGameplayTag(TagStunned));

			EnemyASC->ClearAbility(StanceBreakHandle);
			PlayerASC->ClearAbility(FrontHandle);
		}
	}

	// =========================================================================
	// 6. Backstab Execution -> Victim Lock Synchronous Handshake
	// =========================================================================
	{
		FGameplayAbilitySpec BackstabSpec(UPlayerBackstabExecutionAbility::StaticClass(), 1, INDEX_NONE, Player);
		const FGameplayAbilitySpecHandle BackstabHandle = PlayerASC->GiveAbility(BackstabSpec);
		FGameplayAbilitySpec* FoundBackstabSpec = PlayerASC->FindAbilitySpecFromHandle(BackstabHandle);
		UPlayerBackstabExecutionAbility* BackstabInstance = FoundBackstabSpec ? Cast<UPlayerBackstabExecutionAbility>(FoundBackstabSpec->GetPrimaryInstance()) : nullptr;

		if (TestNotNull(TEXT("BackstabInstance valid"), BackstabInstance))
		{
			BackstabInstance->SetTestSkipMontageTaskActivation(true);
			BackstabInstance->SetTestExecutionMontage(NewObject<UAnimMontage>());
			BackstabInstance->SetTestDamageGameplayEffectClass(UTestProjectileDamageGE::StaticClass());
			BackstabInstance->SetTestExecutionDistances(0.0f, 250.0f);
			BackstabInstance->SetTestMaxBackAngleDegrees(60.0f);

			// Position Player behind Enemy (Enemy facing forward (1,0,0), Player at (-150,0,0))
			Player->SetActorLocation(FVector(-150.0f, 0.0f, 0.0f));
			Enemy->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
			Enemy->SetActorRotation(FRotator(0.0f, 0.0f, 0.0f));
			Player->SetTestLockedTarget(Enemy);

			const bool bActivated = PlayerASC->TryActivateAbility(BackstabHandle);
			TestTrue(TEXT("Backstab Execution activated"), bActivated);

			const FGameplayTag TagPlayerLocked = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Execution.PlayerLocked")), false);
			const FGameplayTag TagVictimLocked = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Execution.VictimLocked")), false);

			TestTrue(TEXT("Player has PlayerLocked during Backstab"), PlayerASC->HasMatchingGameplayTag(TagPlayerLocked));
			TestTrue(TEXT("Enemy has VictimLocked during Backstab"), EnemyASC->HasMatchingGameplayTag(TagVictimLocked));

			BackstabInstance->TestEndAbility(false);

			TestFalse(TEXT("Player released PlayerLocked after Backstab"), PlayerASC->HasMatchingGameplayTag(TagPlayerLocked));
			TestFalse(TEXT("Enemy released VictimLocked after Backstab"), EnemyASC->HasMatchingGameplayTag(TagVictimLocked));

			PlayerASC->ClearAbility(BackstabHandle);
		}
	}

	// =========================================================================
	// 7. AI Controller Lock & StateTree Gate Verification
	// =========================================================================
	{
		AEnemyAIController* AIController = World->SpawnActor<AEnemyAIController>();
		if (TestNotNull(TEXT("AIController spawned"), AIController))
		{
			TestFalse(TEXT("AIController initially not execution locked"), AIController->IsExecutionLocked());

			AIController->BeginExecutionLock();
			TestTrue(TEXT("AIController is execution locked after BeginExecutionLock"), AIController->IsExecutionLocked());
			TestFalse(TEXT("CanRequestCooldownReposition blocked while locked"), AIController->CanRequestCooldownReposition());
			TestFalse(TEXT("CanRequestApproach blocked while locked"), AIController->CanRequestApproach());
			TestFalse(TEXT("PreparePendingAttackProfile blocked while locked"), AIController->PreparePendingAttackProfile());
			TestFalse(TEXT("TryRequestMeleeAttack blocked while locked"), AIController->TryRequestMeleeAttack());

			AIController->EndExecutionLock();
			TestFalse(TEXT("AIController unlocked after EndExecutionLock"), AIController->IsExecutionLocked());
		}
	}

	// =========================================================================
	// 8. Hit Task & Release Task Fail-Closed Verification
	// =========================================================================
	{
		// Case 8.1: Front Execution Hit Task Invalidation -> Ends fail-closed
		{
			FGameplayAbilitySpec FrontSpec(UPlayerFrontExecutionAbility::StaticClass(), 1, INDEX_NONE, Player);
			const FGameplayAbilitySpecHandle FrontHandle = PlayerASC->GiveAbility(FrontSpec);
			FGameplayAbilitySpec* FoundFrontSpec = PlayerASC->FindAbilitySpecFromHandle(FrontHandle);
			UPlayerFrontExecutionAbility* FrontInstance = FoundFrontSpec ? Cast<UPlayerFrontExecutionAbility>(FoundFrontSpec->GetPrimaryInstance()) : nullptr;

			if (TestNotNull(TEXT("FrontInstance valid for task failure test"), FrontInstance))
			{
				FrontInstance->SetTestSkipMontageTaskActivation(true);
				FrontInstance->SetTestExecutionMontage(NewObject<UAnimMontage>());
				FrontInstance->SetTestDamageGameplayEffectClass(UTestProjectileDamageGE::StaticClass());
				FrontInstance->SetTestExecutionDistances(0.0f, 250.0f);
				FrontInstance->SetTestMaxFrontAngleDegrees(60.0f);
				FrontInstance->SetTestInvalidateWaitHitEventTaskAfterReady(true);

				Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
				Enemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
				Enemy->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f));
				Player->SetTestLockedTarget(Enemy);

				if (UCharacterAttributeSet* EnemyAttribs = const_cast<UCharacterAttributeSet*>(EnemyASC->GetSet<UCharacterAttributeSet>()))
				{
					EnemyAttribs->SetPoise(0.0f);
				}
				FGameplayAbilitySpec StanceBreakSpec(UEnemyStanceBreakAbility::StaticClass(), 1, INDEX_NONE, Enemy);
				const FGameplayAbilitySpecHandle StanceBreakHandle = EnemyASC->GiveAbility(StanceBreakSpec);
				EnemyASC->TryActivateAbility(StanceBreakHandle);

				PlayerASC->TryActivateAbility(FrontHandle);

				TestFalse(TEXT("Front execution ended fail-closed on invalid hit task"), FrontInstance->IsActive());
				TestNull(TEXT("Front execution cleared reserved target on invalid hit task"), FrontInstance->GetTestReservedTarget());
				const FGameplayTag TagPlayerLocked = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Execution.PlayerLocked")), false);
				TestFalse(TEXT("Player does not leak PlayerLocked on hit task failure"), PlayerASC->HasMatchingGameplayTag(TagPlayerLocked));

				EnemyASC->ClearAbility(StanceBreakHandle);
				PlayerASC->ClearAbility(FrontHandle);
			}
		}

		// Case 8.2: Victim Ability Release Task Invalidation -> Ends fail-closed
		{
			FGameplayAbilitySpec VictimSpec(UEnemyVictimExecutionAbility::StaticClass(), 1, INDEX_NONE, Enemy);
			const FGameplayAbilitySpecHandle VictimHandle = EnemyASC->GiveAbility(VictimSpec);
			FGameplayAbilitySpec* FoundVictimSpec = EnemyASC->FindAbilitySpecFromHandle(VictimHandle);
			UEnemyVictimExecutionAbility* VictimInstance = FoundVictimSpec ? Cast<UEnemyVictimExecutionAbility>(FoundVictimSpec->GetPrimaryInstance()) : nullptr;

			if (TestNotNull(TEXT("VictimInstance valid for release failure test"), VictimInstance))
			{
				VictimInstance->SetTestInvalidateWaitReleaseTaskAfterReady(true);

				UPlayerFrontExecutionAbility* DummySourceAbility = NewObject<UPlayerFrontExecutionAbility>();
				UExecutionLockContext* Context = NewObject<UExecutionLockContext>(Player);
				const FGameplayTag TagFrontReq = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.Front")), false);
				Context->InitializeSession(DummySourceAbility, Player, PlayerASC, Enemy, TagFrontReq, 101);

				if (UCharacterAttributeSet* EnemyAttribs = const_cast<UCharacterAttributeSet*>(EnemyASC->GetSet<UCharacterAttributeSet>()))
				{
					EnemyAttribs->SetPoise(0.0f);
				}
				FGameplayAbilitySpec StanceBreakSpec(UEnemyStanceBreakAbility::StaticClass(), 1, INDEX_NONE, Enemy);
				const FGameplayAbilitySpecHandle StanceBreakHandle = EnemyASC->GiveAbility(StanceBreakSpec);
				EnemyASC->TryActivateAbility(StanceBreakHandle);

				FGameplayEventData Payload;
				Payload.EventTag = TagFrontReq;
				Payload.Instigator = Player;
				Payload.Target = Enemy;
				Payload.OptionalObject = Context;

				EnemyASC->HandleGameplayEvent(TagFrontReq, &Payload);

				TestFalse(TEXT("Victim ability ended fail-closed on invalid release task"), VictimInstance->IsActive());
				const FGameplayTag TagVictimLocked = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Execution.VictimLocked")), false);
				TestFalse(TEXT("Enemy does not leak VictimLocked on release task failure"), EnemyASC->HasMatchingGameplayTag(TagVictimLocked));
				TestFalse(TEXT("AI execution lock is released"), VictimInstance->IsTestAIExecutionLocked());

				EnemyASC->ClearAbility(StanceBreakHandle);
				EnemyASC->ClearAbility(VictimHandle);
			}
		}
	}

	// =========================================================================
	// 9. ValidateExecutionRequest Malformed & Prerequisite Gate Tests
	// =========================================================================
	{
		FGameplayAbilitySpec VictimSpec(UEnemyVictimExecutionAbility::StaticClass(), 1, INDEX_NONE, Enemy);
		const FGameplayAbilitySpecHandle VictimHandle = EnemyASC->GiveAbility(VictimSpec);
		FGameplayAbilitySpec* FoundVictimSpec = EnemyASC->FindAbilitySpecFromHandle(VictimHandle);
		UEnemyVictimExecutionAbility* VictimInstance = FoundVictimSpec ? Cast<UEnemyVictimExecutionAbility>(FoundVictimSpec->GetPrimaryInstance()) : nullptr;

		if (TestNotNull(TEXT("VictimInstance valid for validation tests"), VictimInstance))
		{
			const FGameplayTag TagFrontReq = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.Front")), false);
			const FGameplayTag TagBackstabReq = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.Backstab")), false);

			// Case 9.1: Malformed payload with null OptionalObject -> rejected
			{
				FGameplayEventData NullPayload;
				NullPayload.EventTag = TagFrontReq;
				NullPayload.Instigator = Player;
				NullPayload.Target = Enemy;
				NullPayload.OptionalObject = nullptr;
				EnemyASC->HandleGameplayEvent(TagFrontReq, &NullPayload);
				TestFalse(TEXT("Victim ability rejected null context payload"), VictimInstance->IsActive());
			}

			// Case 9.2: Front request when Poise is NOT zero -> rejected
			{
				if (UCharacterAttributeSet* EnemyAttribs = const_cast<UCharacterAttributeSet*>(EnemyASC->GetSet<UCharacterAttributeSet>()))
				{
					EnemyAttribs->SetPoise(50.0f);
				}
				UPlayerFrontExecutionAbility* DummySourceAbility = NewObject<UPlayerFrontExecutionAbility>();
				UExecutionLockContext* Context = NewObject<UExecutionLockContext>(Player);
				Context->InitializeSession(DummySourceAbility, Player, PlayerASC, Enemy, TagFrontReq, 102);

				FGameplayEventData PoisePayload;
				PoisePayload.EventTag = TagFrontReq;
				PoisePayload.Instigator = Player;
				PoisePayload.Target = Enemy;
				PoisePayload.OptionalObject = Context;

				EnemyASC->HandleGameplayEvent(TagFrontReq, &PoisePayload);
				TestFalse(TEXT("Victim ability rejected Front request when Poise > 0"), VictimInstance->IsActive());
			}

			// Case 9.3: Backstab request when Enemy has active StanceBreak / Stunned -> rejected
			{
				if (UCharacterAttributeSet* EnemyAttribs = const_cast<UCharacterAttributeSet*>(EnemyASC->GetSet<UCharacterAttributeSet>()))
				{
					EnemyAttribs->SetPoise(0.0f);
				}
				FGameplayAbilitySpec StanceBreakSpec(UEnemyStanceBreakAbility::StaticClass(), 1, INDEX_NONE, Enemy);
				const FGameplayAbilitySpecHandle StanceBreakHandle = EnemyASC->GiveAbility(StanceBreakSpec);
				EnemyASC->TryActivateAbility(StanceBreakHandle);

				UPlayerBackstabExecutionAbility* DummyBackstabAbility = NewObject<UPlayerBackstabExecutionAbility>();
				UExecutionLockContext* Context = NewObject<UExecutionLockContext>(Player);
				Context->InitializeSession(DummyBackstabAbility, Player, PlayerASC, Enemy, TagBackstabReq, 103);

				FGameplayEventData BackstabPayload;
				BackstabPayload.EventTag = TagBackstabReq;
				BackstabPayload.Instigator = Player;
				BackstabPayload.Target = Enemy;
				BackstabPayload.OptionalObject = Context;

				EnemyASC->HandleGameplayEvent(TagBackstabReq, &BackstabPayload);
				TestFalse(TEXT("Victim ability rejected Backstab request on Stunned/StanceBreak target"), VictimInstance->IsActive());

				EnemyASC->ClearAbility(StanceBreakHandle);
			}

			EnemyASC->ClearAbility(VictimHandle);
		}
	}

	// =========================================================================
	// 10. VictimLock Loss Awareness Tests
	// =========================================================================
	{
		FGameplayAbilitySpec VictimSpec(UEnemyVictimExecutionAbility::StaticClass(), 1, INDEX_NONE, Enemy);
		const FGameplayAbilitySpecHandle VictimHandle = EnemyASC->GiveAbility(VictimSpec);

		FGameplayAbilitySpec FrontSpec(UPlayerFrontExecutionAbility::StaticClass(), 1, INDEX_NONE, Player);
		const FGameplayAbilitySpecHandle FrontHandle = PlayerASC->GiveAbility(FrontSpec);
		FGameplayAbilitySpec* FoundFrontSpec = PlayerASC->FindAbilitySpecFromHandle(FrontHandle);
		UPlayerFrontExecutionAbility* FrontInstance = FoundFrontSpec ? Cast<UPlayerFrontExecutionAbility>(FoundFrontSpec->GetPrimaryInstance()) : nullptr;

		if (TestNotNull(TEXT("FrontInstance valid for VictimLock loss test"), FrontInstance))
		{
			FrontInstance->SetTestSkipMontageTaskActivation(true);
			FrontInstance->SetTestExecutionMontage(NewObject<UAnimMontage>());
			FrontInstance->SetTestDamageGameplayEffectClass(UTestProjectileDamageGE::StaticClass());
			FrontInstance->SetTestExecutionDistances(0.0f, 250.0f);
			FrontInstance->SetTestMaxFrontAngleDegrees(60.0f);

			Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
			Enemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
			Enemy->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f));
			Player->SetTestLockedTarget(Enemy);

			if (UCharacterAttributeSet* EnemyAttribs = const_cast<UCharacterAttributeSet*>(EnemyASC->GetSet<UCharacterAttributeSet>()))
			{
				EnemyAttribs->SetPoise(0.0f);
			}
			FGameplayAbilitySpec StanceBreakSpec(UEnemyStanceBreakAbility::StaticClass(), 1, INDEX_NONE, Enemy);
			const FGameplayAbilitySpecHandle StanceBreakHandle = EnemyASC->GiveAbility(StanceBreakSpec);
			if (FGameplayAbilitySpec* FoundSBSpec = EnemyASC->FindAbilitySpecFromHandle(StanceBreakHandle))
			{
				FoundSBSpec->ActivationInfo.SetActivationConfirmed();
				FoundSBSpec->ActiveCount = 1;
			}
			const FGameplayTag TagStunned = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
			EnemyASC->AddLooseGameplayTag(TagStunned);

			const bool bFrontActivated = PlayerASC->TryActivateAbility(FrontHandle);
			TestTrue(TEXT("Front Execution activated"), bFrontActivated);

			// Prematurely end victim ability / remove VictimLocked
			const FGameplayTag TagVictimLocked = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Execution.VictimLocked")), false);
			EnemyASC->RemoveLooseGameplayTag(TagVictimLocked);
			FGameplayAbilitySpec* FoundVictimSpec = EnemyASC->FindAbilitySpecFromHandle(VictimHandle);
			if (UEnemyVictimExecutionAbility* VictimInstance = FoundVictimSpec ? Cast<UEnemyVictimExecutionAbility>(FoundVictimSpec->GetPrimaryInstance()) : nullptr)
			{
				VictimInstance->TestEndAbility(true);
			}

			TestFalse(TEXT("Player Front execution ended fail-closed when VictimLocked was removed"), FrontInstance->IsActive());
			TestNull(TEXT("Reserved target cleared on VictimLock loss"), FrontInstance->GetTestReservedTarget());

			EnemyASC->ClearAbility(StanceBreakHandle);
			PlayerASC->ClearAbility(FrontHandle);
		}

		EnemyASC->ClearAbility(VictimHandle);
	}

	// =========================================================================
	// 11. MeleeHitResolver Strict SourceObject Authorization Tests
	// =========================================================================
	{
		FGameplayAbilitySpec VictimSpec(UEnemyVictimExecutionAbility::StaticClass(), 1, INDEX_NONE, Enemy);
		const FGameplayAbilitySpecHandle VictimHandle = EnemyASC->GiveAbility(VictimSpec);

		FGameplayAbilitySpec FrontSpec(UPlayerFrontExecutionAbility::StaticClass(), 1, INDEX_NONE, Player);
		const FGameplayAbilitySpecHandle FrontHandle = PlayerASC->GiveAbility(FrontSpec);
		FGameplayAbilitySpec* FoundFrontSpec = PlayerASC->FindAbilitySpecFromHandle(FrontHandle);
		UPlayerFrontExecutionAbility* FrontInstance = FoundFrontSpec ? Cast<UPlayerFrontExecutionAbility>(FoundFrontSpec->GetPrimaryInstance()) : nullptr;

		if (TestNotNull(TEXT("FrontInstance valid for Section 11"), FrontInstance))
		{
			FrontInstance->SetTestSkipMontageTaskActivation(true);
			FrontInstance->SetTestExecutionMontage(NewObject<UAnimMontage>());
			FrontInstance->SetTestDamageGameplayEffectClass(UTestProjectileDamageGE::StaticClass());
			FrontInstance->SetTestExecutionDistances(0.0f, 250.0f);
			FrontInstance->SetTestMaxFrontAngleDegrees(60.0f);

			Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
			Enemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
			Enemy->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f));
			Player->SetTestLockedTarget(Enemy);

			if (UCharacterAttributeSet* EnemyAttribs = const_cast<UCharacterAttributeSet*>(EnemyASC->GetSet<UCharacterAttributeSet>()))
			{
				EnemyAttribs->SetPoise(0.0f);
			}
			FGameplayAbilitySpec StanceBreakSpec(UEnemyStanceBreakAbility::StaticClass(), 1, INDEX_NONE, Enemy);
			const FGameplayAbilitySpecHandle StanceBreakHandle = EnemyASC->GiveAbility(StanceBreakSpec);
			if (FGameplayAbilitySpec* FoundSBSpec = EnemyASC->FindAbilitySpecFromHandle(StanceBreakHandle))
			{
				FoundSBSpec->ActivationInfo.SetActivationConfirmed();
				FoundSBSpec->ActiveCount = 1;
			}
			const FGameplayTag TagStunned = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
			EnemyASC->AddLooseGameplayTag(TagStunned);

			const bool bFrontActivated = PlayerASC->TryActivateAbility(FrontHandle);
			TestTrue(TEXT("Front Execution activated for Section 11"), bFrontActivated);

			UExecutionLockContext* AuthContext = FrontInstance->GetTestExecutionContext();
			TestNotNull(TEXT("Active execution context exists"), AuthContext);

			FMeleeHitRequest Request;
			Request.SourceActor = Player;
			Request.SourceAbilitySystemComponent = PlayerASC;
			Request.DamageGameplayEffectClass = UTestProjectileDamageGE::StaticClass();
			Request.AbilityLevel = 1.0f;
			Request.HitResult.HitObjectHandle = FActorInstanceHandle(Enemy);
			Request.HitResult.Location = Enemy->GetActorLocation();
			Request.ExecutionContext = AuthContext;

			// Case 11.1: SourceObject is nullptr -> rejected on Invulnerable target
			Request.SourceObject = nullptr;
			TestFalse(TEXT("Hit rejected when SourceObject is nullptr"), FMeleeHitResolver::TryResolveHit(Request));

			// Case 11.2: SourceObject is wrong ability -> rejected on Invulnerable target
			UPlayerBackstabExecutionAbility* WrongAbility = NewObject<UPlayerBackstabExecutionAbility>();
			Request.SourceObject = WrongAbility;
			TestFalse(TEXT("Hit rejected when SourceObject does not match SourceAbility"), FMeleeHitResolver::TryResolveHit(Request));

			// Case 11.3: SourceObject matches SourceAbility -> authorized
			Request.SourceObject = FrontInstance;
			TestTrue(TEXT("Hit authorized when SourceObject strictly matches SourceAbility"),
				AuthContext && AuthContext->IsHitAuthorized(FrontInstance, Player, PlayerASC, Enemy, EnemyASC));

			FrontInstance->TestEndAbility(false);
			EnemyASC->ClearAbility(StanceBreakHandle);
			PlayerASC->ClearAbility(FrontHandle);
		}

		EnemyASC->ClearAbility(VictimHandle);
	}

	// =========================================================================
	// 12. Main Hand Melee Weapon Gate Tests
	// =========================================================================
	{
		const UPlayerFrontExecutionAbility* FrontCDO = UPlayerFrontExecutionAbility::StaticClass()->GetDefaultObject<UPlayerFrontExecutionAbility>();
		const UPlayerBackstabExecutionAbility* BackstabCDO = UPlayerBackstabExecutionAbility::StaticClass()->GetDefaultObject<UPlayerBackstabExecutionAbility>();

		// Player without weapon equipment or with null weapon
		APlayerCharacter* UnarmedPlayer = World->SpawnActor<APlayerCharacter>();
		if (TestNotNull(TEXT("UnarmedPlayer spawned"), UnarmedPlayer))
		{
			UAbilitySystemComponent* UnarmedASC = UnarmedPlayer->GetAbilitySystemComponent();
			if (TestNotNull(TEXT("Unarmed ASC valid"), UnarmedASC))
			{
				FGameplayAbilityActorInfo ActorInfo;
				ActorInfo.InitFromActor(UnarmedPlayer, UnarmedPlayer, UnarmedASC);

				TestFalse(TEXT("Front CanActivateAbility rejected for UnarmedPlayer"),
					FrontCDO->CanActivateAbility(FGameplayAbilitySpecHandle(), &ActorInfo));
				TestFalse(TEXT("Backstab CanActivateAbility rejected for UnarmedPlayer"),
					BackstabCDO->CanActivateAbility(FGameplayAbilitySpecHandle(), &ActorInfo));
			}

			UnarmedPlayer->Destroy();
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
