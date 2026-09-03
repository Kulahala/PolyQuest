#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/EnemyStanceBreakAbility.h"
#include "AbilitySystem/Abilities/EnemyVictimExecutionAbility.h"
#include "AbilitySystem/Abilities/PlayerBackstabExecutionAbility.h"
#include "AbilitySystem/Abilities/PlayerFrontExecutionAbility.h"
#include "AbilitySystem/CharacterAttributeSet.h"
#include "Animation/AnimMontage.h"
#include "Animation/Combat/AnimNotify_PlayerExecutionHit.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Character/Player/PlayerCharacter.h"
#include "Combat/Equipment/MeleeWeaponDefinition.h"
#include "Combat/Equipment/WeaponEquipmentComponent.h"
#include "Combat/Execution/ExecutionLockContext.h"
#include "Combat/Melee/MeleeHitResolver.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/PolyQuestPlayerController.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "Kismet/GameplayStatics.h"
#include "Tests/CombatAutomationFixture.h"
#include "Tests/TestProjectileDamageGE.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FExecutionHitNotifyAutomationTest,
	"PolyQuest.Combat.ExecutionHitNotify",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	struct FExecutionHitTestWorldScope
	{
		UWorld* World = nullptr;
		~FExecutionHitTestWorldScope()
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

bool FExecutionHitNotifyAutomationTest::RunTest(const FString& Parameters)
{
	// =========================================================================
	// 1. CDO, Display Name & Tag Registration Verification
	// =========================================================================
	const UAnimNotify_PlayerExecutionHit* NotifyCDO = UAnimNotify_PlayerExecutionHit::StaticClass()->GetDefaultObject<UAnimNotify_PlayerExecutionHit>();
	if (!TestNotNull(TEXT("UAnimNotify_PlayerExecutionHit CDO exists"), NotifyCDO))
	{
		return false;
	}

	TestEqual(TEXT("NotifyName is 'Player Execution Hit'"),
		NotifyCDO->GetNotifyName(),
		FString(TEXT("Player Execution Hit")));

	const FGameplayTag TagCanonicalHit = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Hit")), false);
	const FGameplayTag TagFrontLegacyHit = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Front.Hit")), false);
	const FGameplayTag TagBackstabLegacyHit = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Backstab.Hit")), false);
	const FGameplayTag TagExecutionParent = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution")), false);
	const FGameplayTag TagReleaseEvent = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Release")), false);
	const FGameplayTag TagVictimStartEvent = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.VictimStart")), false);

	TestTrue(TEXT("Tag Event.Action.Execution.Hit is registered"), TagCanonicalHit.IsValid());
	TestTrue(TEXT("Tag Event.Action.Execution.Front.Hit is registered"), TagFrontLegacyHit.IsValid());
	TestTrue(TEXT("Tag Event.Action.Execution.Backstab.Hit is registered"), TagBackstabLegacyHit.IsValid());

	// =========================================================================
	// 2. Notify Fail-Closed Static Checks (No MeshComp, No Owner, No Animation)
	// =========================================================================
	{
		UAnimNotify_PlayerExecutionHit* TestNotify = NewObject<UAnimNotify_PlayerExecutionHit>();
		FAnimNotifyEventReference DummyEventRef;

		// 2.1 Null MeshComp -> fail-closed
		TestNotify->Notify(nullptr, nullptr, DummyEventRef);

		// 2.2 Valid MeshComp with no owner/ASC -> fail-closed
		USkeletalMeshComponent* OrphanMesh = NewObject<USkeletalMeshComponent>();
		TestNotify->Notify(OrphanMesh, nullptr, DummyEventRef);
	}

	// =========================================================================
	// 3. Test World & Actor Fixture Setup
	// =========================================================================
	if (!TestNotNull(TEXT("GEngine is valid"), GEngine))
	{
		return false;
	}

	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("ExecutionHitNotifyTestWorld"));
	WorldContext.SetCurrentWorld(World);
	FExecutionHitTestWorldScope ScopeCleanup{ World };

	if (!TestNotNull(TEXT("Test World created"), World))
	{
		return false;
	}

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

	FGameplayAbilitySpec VictimSpec(UEnemyVictimExecutionAbility::StaticClass(), 1, INDEX_NONE, Enemy);
	const FGameplayAbilitySpecHandle VictimSpecHandle = EnemyASC->GiveAbility(VictimSpec);

	const FGameplayTag TagStunned = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);

	auto GrantEnemyStanceBreakAbility = [&](AEnemyCharacter* InEnemy) -> FGameplayAbilitySpecHandle
	{
		UAbilitySystemComponent* ASC = InEnemy->GetAbilitySystemComponent();
		FGameplayAbilitySpec Spec(UEnemyStanceBreakAbility::StaticClass(), 1, INDEX_NONE, InEnemy);
		const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);
		if (FGameplayAbilitySpec* FoundSpec = ASC->FindAbilitySpecFromHandle(Handle))
		{
			FoundSpec->ActivationInfo.SetActivationConfirmed();
			FoundSpec->ActiveCount = 1;
		}
		return Handle;
	};

	auto SetupFrontPrerequisites = [&](AEnemyCharacter* InEnemy) -> FGameplayAbilitySpecHandle
	{
		UAbilitySystemComponent* ASC = InEnemy->GetAbilitySystemComponent();
		if (!ASC->HasMatchingGameplayTag(TagStunned))
		{
			ASC->AddLooseGameplayTag(TagStunned);
		}
		if (UCharacterAttributeSet* EnemyAttribs = const_cast<UCharacterAttributeSet*>(ASC->GetSet<UCharacterAttributeSet>()))
		{
			EnemyAttribs->SetPoise(0.0f);
			EnemyAttribs->SetHealth(100.0f);
		}
		return GrantEnemyStanceBreakAbility(InEnemy);
	};

	auto CleanupFrontPrerequisites = [&](AEnemyCharacter* InEnemy, FGameplayAbilitySpecHandle StanceBreakHandle)
	{
		UAbilitySystemComponent* ASC = InEnemy->GetAbilitySystemComponent();
		if (ASC->HasMatchingGameplayTag(TagStunned))
		{
			ASC->RemoveLooseGameplayTag(TagStunned);
		}
		if (StanceBreakHandle.IsValid())
		{
			ASC->ClearAbility(StanceBreakHandle);
		}
	};

	auto GrantAndConfigureFrontAbility = [&](APlayerCharacter* InPlayer, UAnimMontage* Montage, TSubclassOf<UGameplayEffect> DamageClass) -> TPair<FGameplayAbilitySpecHandle, UPlayerFrontExecutionAbility*>
	{
		UAbilitySystemComponent* ASC = InPlayer->GetAbilitySystemComponent();
		FGameplayAbilitySpec Spec(UPlayerFrontExecutionAbility::StaticClass(), 1, INDEX_NONE, InPlayer);
		const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);
		FGameplayAbilitySpec* FoundSpec = ASC->FindAbilitySpecFromHandle(Handle);
		UPlayerFrontExecutionAbility* Instance = FoundSpec ? Cast<UPlayerFrontExecutionAbility>(FoundSpec->GetPrimaryInstance()) : nullptr;
		if (Instance)
		{
			Instance->SetTestSkipMontageTaskActivation(true);
			Instance->SetTestExecutionMontage(Montage);
			Instance->SetTestDamageGameplayEffectClass(DamageClass);
			Instance->SetTestExecutionDistances(0.0f, 250.0f);
			Instance->SetTestMaxFrontAngleDegrees(60.0f);
		}
		return { Handle, Instance };
	};

	auto GrantAndConfigureBackstabAbility = [&](APlayerCharacter* InPlayer, UAnimMontage* Montage, TSubclassOf<UGameplayEffect> DamageClass) -> TPair<FGameplayAbilitySpecHandle, UPlayerBackstabExecutionAbility*>
	{
		UAbilitySystemComponent* ASC = InPlayer->GetAbilitySystemComponent();
		FGameplayAbilitySpec Spec(UPlayerBackstabExecutionAbility::StaticClass(), 1, INDEX_NONE, InPlayer);
		const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);
		FGameplayAbilitySpec* FoundSpec = ASC->FindAbilitySpecFromHandle(Handle);
		UPlayerBackstabExecutionAbility* Instance = FoundSpec ? Cast<UPlayerBackstabExecutionAbility>(FoundSpec->GetPrimaryInstance()) : nullptr;
		if (Instance)
		{
			Instance->SetTestSkipMontageTaskActivation(true);
			Instance->SetTestExecutionMontage(Montage);
			Instance->SetTestDamageGameplayEffectClass(DamageClass);
			Instance->SetTestExecutionDistances(0.0f, 250.0f);
			Instance->SetTestMaxBackAngleDegrees(60.0f);
		}
		return { Handle, Instance };
	};

	UAnimMontage* SyntheticMontage = NewObject<UAnimMontage>();
	TSubclassOf<UGameplayEffect> DamageGEClass = UTestProjectileDamageGE::StaticClass();

	// 2.3 Notify with valid MeshComp and Owner, but null Animation -> fail-closed (no event sent)
	{
		UAnimNotify_PlayerExecutionHit* TestNotify = NewObject<UAnimNotify_PlayerExecutionHit>();
		FAnimNotifyEventReference DummyEventRef;
		TestNotify->Notify(Player->GetMesh(), nullptr, DummyEventRef);
	}

	// =========================================================================
	// 4. Front Execution: Canonical Hit vs Legacy Hit vs Exactly-Once
	// =========================================================================
	{
		const FGameplayAbilitySpecHandle StanceBreakHandle = SetupFrontPrerequisites(Enemy);

		// Align front geometry
		Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
		Player->SetActorRotation(FRotator::ZeroRotator);
		Enemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
		Enemy->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f));

		auto [FrontHandle, FrontAbility] = GrantAndConfigureFrontAbility(Player, SyntheticMontage, DamageGEClass);
		TestNotNull(TEXT("Front execution ability instance exists"), FrontAbility);

		Player->SetTestLockedTarget(Enemy);
		Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));
		TestTrue(TEXT("Front execution ability activated"), FrontAbility->IsActive());

		// 4.1 Canonical Hit Tag consumption
		const UCharacterAttributeSet* EnemyAttribSet = EnemyASC->GetSet<UCharacterAttributeSet>();
		const float HealthBeforeHit = EnemyAttribSet ? EnemyAttribSet->GetHealth() : 0.0f;
		TestEqual(TEXT("Initial health is 100"), HealthBeforeHit, 100.0f);

		FGameplayEventData CanonicalPayload;
		CanonicalPayload.EventTag = TagCanonicalHit;
		CanonicalPayload.Instigator = Player;
		CanonicalPayload.Target = Player;
		CanonicalPayload.OptionalObject = SyntheticMontage;
		FrontAbility->TestTriggerHitEvent(CanonicalPayload);

		TestTrue(TEXT("Front ability consumed canonical Hit event"), FrontAbility->IsTestDamageEventConsumed());
		const float HealthAfterHit = EnemyAttribSet ? EnemyAttribSet->GetHealth() : 0.0f;
		TestTrue(TEXT("Enemy health was reduced by canonical Hit"), HealthAfterHit < HealthBeforeHit);

		// 4.2 Legacy Hit sent afterwards must be idempotent (no double damage)
		FGameplayEventData LegacyPayload;
		LegacyPayload.EventTag = TagFrontLegacyHit;
		LegacyPayload.Instigator = Player;
		LegacyPayload.Target = Player;
		LegacyPayload.OptionalObject = SyntheticMontage;
		FrontAbility->TestTriggerHitEvent(LegacyPayload);

		TestEqual(TEXT("Subsequent Legacy Hit does not re-apply damage"), EnemyAttribSet->GetHealth(), HealthAfterHit);

		FrontAbility->TestEndAbility();
		PlayerASC->ClearAbility(FrontHandle);
		CleanupFrontPrerequisites(Enemy, StanceBreakHandle);
	}

	// =========================================================================
	// 5. Front Execution: Legacy Hit Path & Whitelist Rejections
	// =========================================================================
	{
		const FGameplayAbilitySpecHandle StanceBreakHandle = SetupFrontPrerequisites(Enemy);

		auto [FrontHandle, FrontAbility] = GrantAndConfigureFrontAbility(Player, SyntheticMontage, DamageGEClass);
		Player->SetTestLockedTarget(Enemy);
		Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));
		TestTrue(TEXT("Front execution ability re-activated"), FrontAbility->IsActive());

		// 5.1 Cross-direction: Backstab Legacy Tag sent to Front ability must be rejected
		FGameplayEventData BadPayload;
		BadPayload.EventTag = TagBackstabLegacyHit;
		BadPayload.Instigator = Player;
		BadPayload.Target = Player;
		BadPayload.OptionalObject = SyntheticMontage;
		FrontAbility->TestTriggerHitEvent(BadPayload);
		TestFalse(TEXT("Front rejects Backstab Legacy Tag"), FrontAbility->IsTestDamageEventConsumed());

		// 5.2 Parent Tag Event.Action.Execution must be rejected (exact match required)
		BadPayload.EventTag = TagExecutionParent;
		FrontAbility->TestTriggerHitEvent(BadPayload);
		TestFalse(TEXT("Front rejects broad parent Tag"), FrontAbility->IsTestDamageEventConsumed());

		// 5.3 Release & VictimStart tags must be rejected by Hit handler
		BadPayload.EventTag = TagReleaseEvent;
		FrontAbility->TestTriggerHitEvent(BadPayload);
		TestFalse(TEXT("Front Hit handler rejects Release Tag"), FrontAbility->IsTestDamageEventConsumed());

		BadPayload.EventTag = TagVictimStartEvent;
		FrontAbility->TestTriggerHitEvent(BadPayload);
		TestFalse(TEXT("Front Hit handler rejects VictimStart Tag"), FrontAbility->IsTestDamageEventConsumed());

		// 5.4 Unrelated Tag rejected
		BadPayload.EventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Attack.Light.Combo.BranchWindow.Begin")), false);
		FrontAbility->TestTriggerHitEvent(BadPayload);
		TestFalse(TEXT("Front Hit handler rejects unrelated Tag"), FrontAbility->IsTestDamageEventConsumed());

		// 5.5 Front Legacy Hit path works directly
		const UCharacterAttributeSet* EnemyAttribSet = EnemyASC->GetSet<UCharacterAttributeSet>();
		const float HealthBeforeHit = EnemyAttribSet ? EnemyAttribSet->GetHealth() : 0.0f;

		FGameplayEventData LegacyPayload;
		LegacyPayload.EventTag = TagFrontLegacyHit;
		LegacyPayload.Instigator = Player;
		LegacyPayload.Target = Player;
		LegacyPayload.OptionalObject = SyntheticMontage;
		FrontAbility->TestTriggerHitEvent(LegacyPayload);

		TestTrue(TEXT("Front ability consumed legacy Hit event"), FrontAbility->IsTestDamageEventConsumed());
		const float HealthAfterHit = EnemyAttribSet ? EnemyAttribSet->GetHealth() : 0.0f;
		TestTrue(TEXT("Enemy health was reduced by legacy Hit"), HealthAfterHit < HealthBeforeHit);

		// Subsequent Canonical Hit must be ignored
		FGameplayEventData CanonicalPayload;
		CanonicalPayload.EventTag = TagCanonicalHit;
		CanonicalPayload.Instigator = Player;
		CanonicalPayload.Target = Player;
		CanonicalPayload.OptionalObject = SyntheticMontage;
		FrontAbility->TestTriggerHitEvent(CanonicalPayload);
		TestEqual(TEXT("Subsequent Canonical Hit does not re-apply damage"), EnemyAttribSet->GetHealth(), HealthAfterHit);

		FrontAbility->TestEndAbility();
		PlayerASC->ClearAbility(FrontHandle);
		CleanupFrontPrerequisites(Enemy, StanceBreakHandle);
	}

	// =========================================================================
	// 6. Backstab Execution: Canonical Hit, Legacy Hit & Cross-Direction Rejection
	// =========================================================================
	{
		if (EnemyASC->HasMatchingGameplayTag(TagStunned))
		{
			EnemyASC->RemoveLooseGameplayTag(TagStunned);
		}

		// Align backstab geometry: Enemy facing +X (0 deg), Player at origin behind Enemy
		Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
		Player->SetActorRotation(FRotator::ZeroRotator);
		Enemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
		Enemy->SetActorRotation(FRotator(0.0f, 0.0f, 0.0f));

		if (UCharacterAttributeSet* EnemyAttribs = const_cast<UCharacterAttributeSet*>(EnemyASC->GetSet<UCharacterAttributeSet>()))
		{
			EnemyAttribs->SetHealth(100.0f);
		}

		auto [BackstabHandle, BackstabAbility] = GrantAndConfigureBackstabAbility(Player, SyntheticMontage, DamageGEClass);
		Player->SetTestLockedTarget(Enemy);
		Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));
		TestTrue(TEXT("Backstab ability activated"), BackstabAbility->IsActive());

		// 6.1 Cross-direction: Front Legacy Tag sent to Backstab ability must be rejected
		FGameplayEventData BadPayload;
		BadPayload.EventTag = TagFrontLegacyHit;
		BadPayload.Instigator = Player;
		BadPayload.Target = Player;
		BadPayload.OptionalObject = SyntheticMontage;
		BackstabAbility->TestTriggerHitEvent(BadPayload);
		TestFalse(TEXT("Backstab rejects Front Legacy Tag"), BackstabAbility->IsTestDamageEventConsumed());

		// 6.2 Canonical Hit works on Backstab
		const UCharacterAttributeSet* EnemyAttribSet = EnemyASC->GetSet<UCharacterAttributeSet>();
		const float HealthBeforeHit = EnemyAttribSet ? EnemyAttribSet->GetHealth() : 0.0f;

		FGameplayEventData CanonicalPayload;
		CanonicalPayload.EventTag = TagCanonicalHit;
		CanonicalPayload.Instigator = Player;
		CanonicalPayload.Target = Player;
		CanonicalPayload.OptionalObject = SyntheticMontage;
		BackstabAbility->TestTriggerHitEvent(CanonicalPayload);

		TestTrue(TEXT("Backstab consumed canonical Hit event"), BackstabAbility->IsTestDamageEventConsumed());
		const float HealthAfterHit = EnemyAttribSet ? EnemyAttribSet->GetHealth() : 0.0f;
		TestTrue(TEXT("Enemy health was reduced by canonical Hit on backstab"), HealthAfterHit < HealthBeforeHit);

		// Subsequent Backstab Legacy Hit must be ignored (exactly-once)
		FGameplayEventData LegacyPayload;
		LegacyPayload.EventTag = TagBackstabLegacyHit;
		LegacyPayload.Instigator = Player;
		LegacyPayload.Target = Player;
		LegacyPayload.OptionalObject = SyntheticMontage;
		BackstabAbility->TestTriggerHitEvent(LegacyPayload);
		TestEqual(TEXT("Subsequent Backstab Legacy Hit does not re-apply damage"), EnemyAttribSet->GetHealth(), HealthAfterHit);

		BackstabAbility->TestEndAbility();
		PlayerASC->ClearAbility(BackstabHandle);
	}

	// 6.3 Backstab Legacy Hit standalone
	{
		if (UCharacterAttributeSet* EnemyAttribs = const_cast<UCharacterAttributeSet*>(EnemyASC->GetSet<UCharacterAttributeSet>()))
		{
			EnemyAttribs->SetHealth(100.0f);
		}

		auto [BackstabHandle, BackstabAbility] = GrantAndConfigureBackstabAbility(Player, SyntheticMontage, DamageGEClass);
		Player->SetTestLockedTarget(Enemy);
		Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));
		TestTrue(TEXT("Backstab ability re-activated for legacy check"), BackstabAbility->IsActive());

		const UCharacterAttributeSet* EnemyAttribSet = EnemyASC->GetSet<UCharacterAttributeSet>();
		const float HealthBeforeHit = EnemyAttribSet ? EnemyAttribSet->GetHealth() : 0.0f;

		FGameplayEventData LegacyPayload;
		LegacyPayload.EventTag = TagBackstabLegacyHit;
		LegacyPayload.Instigator = Player;
		LegacyPayload.Target = Player;
		LegacyPayload.OptionalObject = SyntheticMontage;
		BackstabAbility->TestTriggerHitEvent(LegacyPayload);

		TestTrue(TEXT("Backstab consumed Backstab Legacy Hit event"), BackstabAbility->IsTestDamageEventConsumed());
		TestTrue(TEXT("Enemy health reduced by Backstab Legacy Hit"), EnemyAttribSet->GetHealth() < HealthBeforeHit);

		BackstabAbility->TestEndAbility();
		PlayerASC->ClearAbility(BackstabHandle);
	}

	// =========================================================================
	// 7. Malformed Payload & Target Integrity Checks
	// =========================================================================
	{
		const FGameplayAbilitySpecHandle StanceBreakHandle = SetupFrontPrerequisites(Enemy);
		Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
		Player->SetActorRotation(FRotator::ZeroRotator);
		Enemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
		Enemy->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f));

		auto [FrontHandle, FrontAbility] = GrantAndConfigureFrontAbility(Player, SyntheticMontage, DamageGEClass);
		Player->SetTestLockedTarget(Enemy);
		Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));

		// 7.1 Bad Instigator (Enemy instead of Player)
		FGameplayEventData BadPayload;
		BadPayload.EventTag = TagCanonicalHit;
		BadPayload.Instigator = Enemy;
		BadPayload.Target = Player;
		BadPayload.OptionalObject = SyntheticMontage;
		FrontAbility->TestTriggerHitEvent(BadPayload);
		TestFalse(TEXT("Bad Instigator rejected"), FrontAbility->IsTestDamageEventConsumed());

		// 7.2 Bad Target (Enemy instead of Player)
		BadPayload.Instigator = Player;
		BadPayload.Target = Enemy;
		FrontAbility->TestTriggerHitEvent(BadPayload);
		TestFalse(TEXT("Bad Target rejected"), FrontAbility->IsTestDamageEventConsumed());

		// 7.3 Bad Animation object
		UAnimMontage* WrongMontage = NewObject<UAnimMontage>();
		BadPayload.Target = Player;
		BadPayload.OptionalObject = WrongMontage;
		FrontAbility->TestTriggerHitEvent(BadPayload);
		TestFalse(TEXT("Wrong animation object rejected"), FrontAbility->IsTestDamageEventConsumed());

		FrontAbility->TestEndAbility();
		PlayerASC->ClearAbility(FrontHandle);
		CleanupFrontPrerequisites(Enemy, StanceBreakHandle);
	}

	// =========================================================================
	// 8. End-to-End Live Dispatch: Notify -> Player ASC -> Task -> Damage
	// =========================================================================
	{
		const FGameplayAbilitySpecHandle StanceBreakHandle = SetupFrontPrerequisites(Enemy);
		Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
		Player->SetActorRotation(FRotator::ZeroRotator);
		Enemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
		Enemy->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f));

		auto [FrontHandle, FrontAbility] = GrantAndConfigureFrontAbility(Player, SyntheticMontage, DamageGEClass);
		Player->SetTestLockedTarget(Enemy);
		Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));
		TestTrue(TEXT("Front ability active for live notify dispatch"), FrontAbility->IsActive());

		const UCharacterAttributeSet* EnemyAttribSet = EnemyASC->GetSet<UCharacterAttributeSet>();
		const float HealthBefore = EnemyAttribSet ? EnemyAttribSet->GetHealth() : 0.0f;

		// Dispatch via real UAnimNotify_PlayerExecutionHit instance
		UAnimNotify_PlayerExecutionHit* RealNotify = NewObject<UAnimNotify_PlayerExecutionHit>();
		FAnimNotifyEventReference EventRef;
		RealNotify->Notify(Player->GetMesh(), SyntheticMontage, EventRef);

		TestTrue(TEXT("Live Notify dispatched through ASC to active WaitHitEventTask"), FrontAbility->IsTestDamageEventConsumed());
		const float HealthAfter = EnemyAttribSet ? EnemyAttribSet->GetHealth() : 0.0f;
		TestTrue(TEXT("Live Notify reduced enemy health"), HealthAfter < HealthBefore);

		FrontAbility->TestEndAbility();
		PlayerASC->ClearAbility(FrontHandle);
		CleanupFrontPrerequisites(Enemy, StanceBreakHandle);
	}

	// =========================================================================
	// 9. Task Lifecycle, Invalidation Seams & Re-entry Checks
	// =========================================================================
	{
		// 9.1 Front: Legacy Hit task invalidation seam on ReadyForActivation
		{
			const FGameplayAbilitySpecHandle StanceBreakHandle = SetupFrontPrerequisites(Enemy);
			Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
			Player->SetActorRotation(FRotator::ZeroRotator);
			Enemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
			Enemy->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f));

			auto [FrontHandle, FrontAbility] = GrantAndConfigureFrontAbility(Player, SyntheticMontage, DamageGEClass);
			FrontAbility->SetTestInvalidateWaitLegacyHitEventTaskAfterReady(true);
			Player->SetTestLockedTarget(Enemy);
			Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));

			TestFalse(TEXT("Front ability fail-closed when Legacy Hit Task invalidated"), FrontAbility->IsActive());
			TestNull(TEXT("Front reserved target cleared on fail-closed"), FrontAbility->GetTestReservedTarget());
			PlayerASC->ClearAbility(FrontHandle);
			CleanupFrontPrerequisites(Enemy, StanceBreakHandle);
		}

		// 9.2 Backstab: Legacy Hit task invalidation seam on ReadyForActivation
		{
			if (EnemyASC->HasMatchingGameplayTag(TagStunned))
			{
				EnemyASC->RemoveLooseGameplayTag(TagStunned);
			}

			Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
			Player->SetActorRotation(FRotator::ZeroRotator);
			Enemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
			Enemy->SetActorRotation(FRotator(0.0f, 0.0f, 0.0f));

			auto [BackstabHandle, BackstabAbility] = GrantAndConfigureBackstabAbility(Player, SyntheticMontage, DamageGEClass);
			BackstabAbility->SetTestInvalidateWaitLegacyHitEventTaskAfterReady(true);
			Player->SetTestLockedTarget(Enemy);
			Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));

			TestFalse(TEXT("Backstab ability fail-closed when Legacy Hit Task invalidated"), BackstabAbility->IsActive());
			TestNull(TEXT("Backstab reserved target cleared on fail-closed"), BackstabAbility->GetTestReservedTarget());
			PlayerASC->ClearAbility(BackstabHandle);
		}

		// 9.3 Synchronous EndAbility during Task Ready
		{
			const FGameplayAbilitySpecHandle StanceBreakHandle = SetupFrontPrerequisites(Enemy);
			Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
			Player->SetActorRotation(FRotator::ZeroRotator);
			Enemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
			Enemy->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f));

			auto [FrontHandle, FrontAbility] = GrantAndConfigureFrontAbility(Player, SyntheticMontage, DamageGEClass);
			FrontAbility->SetTestEndAbilityDuringTaskReady(true);
			Player->SetTestLockedTarget(Enemy);
			Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));

			TestFalse(TEXT("Ability cleanly ended during Ready re-entry"), FrontAbility->IsActive());
			TestNull(TEXT("No context or target retained after synchronous EndAbility"), FrontAbility->GetTestActiveContext());
			PlayerASC->ClearAbility(FrontHandle);
			CleanupFrontPrerequisites(Enemy, StanceBreakHandle);
		}
	}

	EnemyASC->ClearAbility(VictimSpecHandle);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
