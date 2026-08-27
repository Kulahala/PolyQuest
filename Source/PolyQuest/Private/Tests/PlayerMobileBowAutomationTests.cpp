#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/BowDrawFireAbility.h"
#include "AbilitySystem/CharacterAttributeSet.h"
#include "Character/Player/PlayerCharacter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayAbilitySpec.h"
#include "GameplayTagContainer.h"
#include "Tests/CombatAutomationFixture.h"
#include "Tests/TestExhaustionMoveSpeedGE.h"
#include "Tests/TestMobileBowMoveSpeedGE.h"
#include "Tests/TestMobileBowSprintAbility.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerMobileBowAutomationTest, "PolyQuest.Player.MobileBow", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlayerMobileBowAutomationTest::RunTest(const FString&)
{
	constexpr float BaseMoveSpeed = 500.0f;
	constexpr float MobileBowMoveSpeed = 300.0f;
	constexpr float ExhaustedMoveSpeed = 350.0f;

	if (!TestNotNull(TEXT("Engine is available for the mobile bow fixture"), GEngine))
	{
		return false;
	}

	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("PlayerMobileBowTestWorld"));
	WorldContext.SetCurrentWorld(World);
	if (!TestNotNull(TEXT("Transient mobile bow test world created"), World))
	{
		return false;
	}

	const FURL WorldURL;
	World->InitializeActorsForPlay(WorldURL);
	World->BeginPlay();

	struct FTestScopeCleanup
	{
		UWorld* WorldToDestroy = nullptr;

		~FTestScopeCleanup()
		{
			if (WorldToDestroy)
			{
				GEngine->DestroyWorldContext(WorldToDestroy);
				WorldToDestroy->DestroyWorld(false);
			}
		}
	} ScopeCleanup{ World };

	// -------------------------------------------------------------------------
	// SECTION 1: Gameplay Tags Resolution & CDO Tag Contract
	// -------------------------------------------------------------------------
	const FGameplayTag TagAbilityPrimaryAttack = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Primary")), false);
	const FGameplayTag TagAttacking = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Attacking")), false);
	const FGameplayTag TagCharging = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Charging")), false);
	const FGameplayTag TagMovementBlock = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Movement")), false);
	const FGameplayTag TagJumpBlock = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Jump")), false);
	const FGameplayTag TagAbilitySprint = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Movement.Sprint")), false);
	const FGameplayTag TagSprinting = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Movement.Sprinting")), false);
	const FGameplayTag TagTeamPlayer = FGameplayTag::RequestGameplayTag(FName(TEXT("Team.Player")), false);

	TestTrue(TEXT("Tag Ability.Attack.Primary is valid"), TagAbilityPrimaryAttack.IsValid());
	TestTrue(TEXT("Tag State.Action.Attacking is valid"), TagAttacking.IsValid());
	TestTrue(TEXT("Tag State.Action.Charging is valid"), TagCharging.IsValid());
	TestTrue(TEXT("Tag State.Input.Block.Movement is valid"), TagMovementBlock.IsValid());
	TestTrue(TEXT("Tag State.Input.Block.Jump is valid"), TagJumpBlock.IsValid());
	TestTrue(TEXT("Tag Ability.Movement.Sprint is valid"), TagAbilitySprint.IsValid());
	TestTrue(TEXT("Tag State.Movement.Sprinting is valid"), TagSprinting.IsValid());
	TestTrue(TEXT("Tag Team.Player is valid"), TagTeamPlayer.IsValid());

	// 1.1 Bow CDO Tag Contract: owns Attacking and Jump Block, but NOT Movement Block or Charging
	const UBowDrawFireAbility* BowCDO = UBowDrawFireAbility::StaticClass()->GetDefaultObject<UBowDrawFireAbility>();
	TestNotNull(TEXT("Bow CDO exists"), BowCDO);
	if (BowCDO)
	{
		TestTrue(TEXT("Bow CDO carries Ability.Attack.Primary in AbilityTags"), BowCDO->AbilityTags.HasTagExact(TagAbilityPrimaryAttack));
		TestTrue(TEXT("Bow CDO carries State.Action.Attacking in AbilityTags"), BowCDO->AbilityTags.HasTagExact(TagAttacking));
		TestFalse(TEXT("Bow CDO does NOT carry State.Action.Charging in AbilityTags"), BowCDO->AbilityTags.HasTagExact(TagCharging));
		TestTrue(TEXT("Bow CDO carries State.Action.Attacking in ActivationOwnedTags"), BowCDO->GetTestActivationOwnedTags().HasTagExact(TagAttacking));
		TestFalse(TEXT("Bow CDO does NOT carry State.Action.Charging in ActivationOwnedTags"), BowCDO->GetTestActivationOwnedTags().HasTagExact(TagCharging));
		TestTrue(TEXT("Bow CDO carries State.Input.Block.Jump in ActivationOwnedTags"), BowCDO->GetTestActivationOwnedTags().HasTagExact(TagJumpBlock));
		TestFalse(TEXT("Bow CDO does NOT carry State.Input.Block.Movement in ActivationOwnedTags"), BowCDO->GetTestActivationOwnedTags().HasTagExact(TagMovementBlock));
	}

	// -------------------------------------------------------------------------
	// SECTION 2: Player Fixture Spawn & Initial Attribute Baseline
	// -------------------------------------------------------------------------
	APlayerCharacter* Player = FCombatAutomationFixture::SpawnPlayer(
		World,
		FTransform::Identity,
		[](APlayerCharacter& InPlayer)
		{
			InPlayer.SetTestCombatTeamTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Team.Player")), false));
		});
	TestNotNull(TEXT("Player mobile bow fixture spawned"), Player);
	if (!Player)
	{
		return false;
	}

	UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
	UCharacterMovementComponent* MovementComponent = Player->GetCharacterMovement();
	TestNotNull(TEXT("Player mobile bow fixture has an ASC"), ASC);
	TestNotNull(TEXT("Player mobile bow fixture has CharacterMovement"), MovementComponent);
	if (!ASC || !MovementComponent)
	{
		return false;
	}

	MovementComponent->SetMovementMode(MOVE_Walking);
	ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMoveSpeedAttribute(), BaseMoveSpeed);
	TestTrue(TEXT("Initial MoveSpeed attribute is 500"), FMath::IsNearlyEqual(ASC->GetNumericAttribute(UCharacterAttributeSet::GetMoveSpeedAttribute()), BaseMoveSpeed));
	TestTrue(TEXT("Initial CharacterMovement MaxWalkSpeed is 500"), FMath::IsNearlyEqual(MovementComponent->MaxWalkSpeed, BaseMoveSpeed));

	// -------------------------------------------------------------------------
	// SECTION 3: Bow Instance Construction & MoveSpeed Effect Start
	// -------------------------------------------------------------------------
	const FGameplayAbilitySpecHandle BowSpecHandle = ASC->GiveAbility(FGameplayAbilitySpec(UBowDrawFireAbility::StaticClass(), 1, INDEX_NONE, Player));
	TestTrue(TEXT("Bow spec handle is valid"), BowSpecHandle.IsValid());

	const FGameplayAbilitySpec* BowSpec = ASC->FindAbilitySpecFromHandle(BowSpecHandle);
	TestNotNull(TEXT("Bow ability spec found on ASC"), BowSpec);
	UBowDrawFireAbility* BowAbility = BowSpec ? Cast<UBowDrawFireAbility>(BowSpec->GetPrimaryInstance()) : nullptr;
	TestNotNull(TEXT("Bow ability primary instance created by ASC"), BowAbility);
	if (!BowAbility)
	{
		return false;
	}

	BowAbility->SetTestCurrentActorInfo(ASC->AbilityActorInfo.Get());
	BowAbility->SetTestCurrentSpecHandle(BowSpecHandle);

	// 3.1 Fail-closed when MobileBowMoveSpeedGameplayEffectClass is null
	TestFalse(TEXT("StartMobileBowMoveSpeedEffect fails when GE class is null"), BowAbility->TestStartMobileBowMoveSpeedEffect());
	TestFalse(TEXT("Bow has no active move speed handle on failed start"), BowAbility->HasTestMobileBowMoveSpeedEffectHandle());
	TestTrue(TEXT("MoveSpeed attribute remains 500 after failed start"), FMath::IsNearlyEqual(ASC->GetNumericAttribute(UCharacterAttributeSet::GetMoveSpeedAttribute()), BaseMoveSpeed));
	TestTrue(TEXT("MaxWalkSpeed remains 500 after failed start"), FMath::IsNearlyEqual(MovementComponent->MaxWalkSpeed, BaseMoveSpeed));

	// 3.2 Successful start with UTestMobileBowMoveSpeedGE
	BowAbility->SetTestMobileBowMoveSpeedGameplayEffectClass(UTestMobileBowMoveSpeedGE::StaticClass());
	const bool bStarted = BowAbility->TestStartMobileBowMoveSpeedEffect();
	TestTrue(TEXT("StartMobileBowMoveSpeedEffect succeeds with valid GE class"), bStarted);
	TestTrue(TEXT("Bow has valid move speed effect handle"), BowAbility->HasTestMobileBowMoveSpeedEffectHandle());

	const FActiveGameplayEffectHandle BowHandle = BowAbility->GetTestMobileBowMoveSpeedEffectHandle();
	TestTrue(TEXT("Bow active GE handle is valid"), BowHandle.IsValid());
	TestTrue(TEXT("MoveSpeed attribute resolves to 300 (0.6x)"), FMath::IsNearlyEqual(ASC->GetNumericAttribute(UCharacterAttributeSet::GetMoveSpeedAttribute()), MobileBowMoveSpeed));
	TestTrue(TEXT("CharacterMovement MaxWalkSpeed synchronizes to 300"), FMath::IsNearlyEqual(MovementComponent->MaxWalkSpeed, MobileBowMoveSpeed));

	// -------------------------------------------------------------------------
	// SECTION 4: Active Sprint Cancellation on Bow Start
	// -------------------------------------------------------------------------
	// 4.1 Clear Bow effect for a clean pre-sprint baseline
	BowAbility->TestClearMobileBowMoveSpeedEffect();
	TestFalse(TEXT("Bow handle cleared"), BowAbility->HasTestMobileBowMoveSpeedEffectHandle());
	TestTrue(TEXT("MoveSpeed restored to 500"), FMath::IsNearlyEqual(ASC->GetNumericAttribute(UCharacterAttributeSet::GetMoveSpeedAttribute()), BaseMoveSpeed));

	// 4.2 Grant and activate TestMobileBowSprintAbility
	const FGameplayAbilitySpecHandle SprintSpecHandle = ASC->GiveAbility(FGameplayAbilitySpec(UTestMobileBowSprintAbility::StaticClass(), 1, INDEX_NONE, Player));
	TestTrue(TEXT("Sprint spec handle is valid"), SprintSpecHandle.IsValid());

	const bool bSprintActivated = ASC->TryActivateAbility(SprintSpecHandle);
	TestTrue(TEXT("Test Sprint ability activated"), bSprintActivated);
	TestTrue(TEXT("ASC owns State.Movement.Sprinting tag"), ASC->HasMatchingGameplayTag(TagSprinting));
	TestTrue(TEXT("Player reports active Sprint"), Player->HasActiveSprint());

	// 4.3 Successful Bow start cancels active Sprint
	const bool bBowStartedWithSprint = BowAbility->TestStartMobileBowMoveSpeedEffect();
	TestTrue(TEXT("Bow start helper succeeds while Sprint is active"), bBowStartedWithSprint);
	TestFalse(TEXT("Bow start cancels active Sprint on Player"), Player->HasActiveSprint());
	TestFalse(TEXT("State.Movement.Sprinting tag is removed"), ASC->HasMatchingGameplayTag(TagSprinting));
	TestTrue(TEXT("Bow owns valid move speed effect handle"), BowAbility->HasTestMobileBowMoveSpeedEffectHandle());
	TestTrue(TEXT("MoveSpeed attribute is 300"), FMath::IsNearlyEqual(ASC->GetNumericAttribute(UCharacterAttributeSet::GetMoveSpeedAttribute()), MobileBowMoveSpeed));
	TestTrue(TEXT("MaxWalkSpeed is 300"), FMath::IsNearlyEqual(MovementComponent->MaxWalkSpeed, MobileBowMoveSpeed));

	// -------------------------------------------------------------------------
	// SECTION 5: Handle Persistence Across Draw, Hold, Release Representation
	// -------------------------------------------------------------------------
	const FActiveGameplayEffectHandle RetainedHandle = BowAbility->GetTestMobileBowMoveSpeedEffectHandle();

	BowAbility->SetTestBowStateDrawing();
	TestEqual(TEXT("Retained handle unchanged in Drawing state"), BowAbility->GetTestMobileBowMoveSpeedEffectHandle(), RetainedHandle);

	BowAbility->SetTestBowStateHolding();
	TestEqual(TEXT("Retained handle unchanged in Holding state"), BowAbility->GetTestMobileBowMoveSpeedEffectHandle(), RetainedHandle);

	BowAbility->SetTestBowStateReleasing();
	TestEqual(TEXT("Retained handle unchanged in Releasing state"), BowAbility->GetTestMobileBowMoveSpeedEffectHandle(), RetainedHandle);

	// Re-calling start while already active is idempotent and does not create duplicate handles
	TestTrue(TEXT("Re-calling start helper is idempotent"), BowAbility->TestStartMobileBowMoveSpeedEffect());
	TestEqual(TEXT("Handle remains identical after repeat start"), BowAbility->GetTestMobileBowMoveSpeedEffectHandle(), RetainedHandle);
	TestTrue(TEXT("MoveSpeed remains 300"), FMath::IsNearlyEqual(ASC->GetNumericAttribute(UCharacterAttributeSet::GetMoveSpeedAttribute()), MobileBowMoveSpeed));

	// -------------------------------------------------------------------------
	// SECTION 6: EndAbility Exact-Handle Cleanup & Base Speed Restoration
	// -------------------------------------------------------------------------
	// 6.1 Normal Completion Path (bWasCancelled = false)
	BowAbility->TestSetCharging(true);
	TestTrue(TEXT("Normal completion test establishes Charging on Bow instance"), BowAbility->GetTestChargingApplied());
	TestTrue(TEXT("ASC owns State.Action.Charging before normal EndAbility"), ASC->HasMatchingGameplayTag(TagCharging));

	BowAbility->EndAbility(BowSpecHandle, ASC->AbilityActorInfo.Get(), BowAbility->GetCurrentActivationInfo(), true, false);
	TestFalse(TEXT("Normal EndAbility(bWasCancelled=false) clears Bow move speed handle"), BowAbility->HasTestMobileBowMoveSpeedEffectHandle());
	TestFalse(TEXT("Normal EndAbility(bWasCancelled=false) clears Charging applied state"), BowAbility->GetTestChargingApplied());
	TestFalse(TEXT("Normal EndAbility(bWasCancelled=false) removes State.Action.Charging from ASC"), ASC->HasMatchingGameplayTag(TagCharging));
	TestTrue(TEXT("Base MoveSpeed 500 restored after normal EndAbility"), FMath::IsNearlyEqual(ASC->GetNumericAttribute(UCharacterAttributeSet::GetMoveSpeedAttribute()), BaseMoveSpeed));
	TestTrue(TEXT("CharacterMovement MaxWalkSpeed 500 restored after normal EndAbility"), FMath::IsNearlyEqual(MovementComponent->MaxWalkSpeed, BaseMoveSpeed));

	// 6.2 Interrupted / Cancelled Path (bWasCancelled = true) on an independent Bow ability instance
	const FGameplayAbilitySpecHandle CancelBowSpecHandle = ASC->GiveAbility(FGameplayAbilitySpec(UBowDrawFireAbility::StaticClass(), 1, INDEX_NONE, Player));
	TestTrue(TEXT("Cancel test Bow spec handle is valid"), CancelBowSpecHandle.IsValid());
	const FGameplayAbilitySpec* CancelBowSpec = ASC->FindAbilitySpecFromHandle(CancelBowSpecHandle);
	TestNotNull(TEXT("Cancel test Bow spec found on ASC"), CancelBowSpec);
	UBowDrawFireAbility* CancelBowAbility = CancelBowSpec ? Cast<UBowDrawFireAbility>(CancelBowSpec->GetPrimaryInstance()) : nullptr;
	TestNotNull(TEXT("Cancel test Bow primary instance created by ASC"), CancelBowAbility);
	if (!CancelBowAbility)
	{
		return false;
	}

	CancelBowAbility->SetTestCurrentActorInfo(ASC->AbilityActorInfo.Get());
	CancelBowAbility->SetTestCurrentSpecHandle(CancelBowSpecHandle);
	CancelBowAbility->SetTestMobileBowMoveSpeedGameplayEffectClass(UTestMobileBowMoveSpeedGE::StaticClass());

	const bool bCancelBowStarted = CancelBowAbility->TestStartMobileBowMoveSpeedEffect();
	TestTrue(TEXT("Cancel test Bow start helper succeeds"), bCancelBowStarted);
	TestTrue(TEXT("Cancel test Bow owns active move speed handle"), CancelBowAbility->HasTestMobileBowMoveSpeedEffectHandle());
	TestTrue(TEXT("MoveSpeed reduced to 300 for cancel test"), FMath::IsNearlyEqual(ASC->GetNumericAttribute(UCharacterAttributeSet::GetMoveSpeedAttribute()), MobileBowMoveSpeed));
	TestTrue(TEXT("MaxWalkSpeed reduced to 300 for cancel test"), FMath::IsNearlyEqual(MovementComponent->MaxWalkSpeed, MobileBowMoveSpeed));

	CancelBowAbility->TestSetCharging(true);
	TestTrue(TEXT("Cancel test establishes Charging on independent Bow instance"), CancelBowAbility->GetTestChargingApplied());
	TestTrue(TEXT("ASC owns State.Action.Charging before cancelled EndAbility"), ASC->HasMatchingGameplayTag(TagCharging));

	CancelBowAbility->EndAbility(CancelBowSpecHandle, ASC->AbilityActorInfo.Get(), CancelBowAbility->GetCurrentActivationInfo(), true, true);
	TestFalse(TEXT("Cancelled EndAbility(bWasCancelled=true) clears Bow move speed handle"), CancelBowAbility->HasTestMobileBowMoveSpeedEffectHandle());
	TestFalse(TEXT("Cancelled EndAbility(bWasCancelled=true) clears Charging applied state"), CancelBowAbility->GetTestChargingApplied());
	TestFalse(TEXT("Cancelled EndAbility(bWasCancelled=true) removes State.Action.Charging from ASC"), ASC->HasMatchingGameplayTag(TagCharging));
	TestTrue(TEXT("Base MoveSpeed 500 restored after cancelled EndAbility"), FMath::IsNearlyEqual(ASC->GetNumericAttribute(UCharacterAttributeSet::GetMoveSpeedAttribute()), BaseMoveSpeed));
	TestTrue(TEXT("CharacterMovement MaxWalkSpeed 500 restored after cancelled EndAbility"), FMath::IsNearlyEqual(MovementComponent->MaxWalkSpeed, BaseMoveSpeed));

	// -------------------------------------------------------------------------
	// SECTION 7: External MoveSpeed GE Survival Across Real Bow EndAbility Lifecycle
	// -------------------------------------------------------------------------
	// 7.1 Apply an independent external MoveSpeed GE (UTestExhaustionMoveSpeedGE, 0.7x -> 350)
	const UGameplayEffect* ExternalExhaustionGE = UTestExhaustionMoveSpeedGE::StaticClass()->GetDefaultObject<UGameplayEffect>();
	const FActiveGameplayEffectHandle ExternalHandle = ASC->ApplyGameplayEffectToSelf(
		ExternalExhaustionGE,
		1.0f,
		ASC->MakeEffectContext());
	TestTrue(TEXT("External MoveSpeed GE applied successfully"), ExternalHandle.IsValid());
	TestTrue(TEXT("External GE resolves MoveSpeed to 350 (0.7x)"), FMath::IsNearlyEqual(ASC->GetNumericAttribute(UCharacterAttributeSet::GetMoveSpeedAttribute()), ExhaustedMoveSpeed));
	TestTrue(TEXT("MaxWalkSpeed follows external GE to 350"), FMath::IsNearlyEqual(MovementComponent->MaxWalkSpeed, ExhaustedMoveSpeed));

	// 7.2 Apply Bow MoveSpeed GE on top of external GE via independent Bow instance
	const FGameplayAbilitySpecHandle ExternalBowSpecHandle = ASC->GiveAbility(FGameplayAbilitySpec(UBowDrawFireAbility::StaticClass(), 1, INDEX_NONE, Player));
	TestTrue(TEXT("External survival test Bow spec handle is valid"), ExternalBowSpecHandle.IsValid());
	const FGameplayAbilitySpec* ExternalBowSpec = ASC->FindAbilitySpecFromHandle(ExternalBowSpecHandle);
	TestNotNull(TEXT("External survival test Bow spec found on ASC"), ExternalBowSpec);
	UBowDrawFireAbility* ExternalBowAbility = ExternalBowSpec ? Cast<UBowDrawFireAbility>(ExternalBowSpec->GetPrimaryInstance()) : nullptr;
	TestNotNull(TEXT("External survival test Bow primary instance created by ASC"), ExternalBowAbility);
	if (!ExternalBowAbility)
	{
		return false;
	}

	ExternalBowAbility->SetTestCurrentActorInfo(ASC->AbilityActorInfo.Get());
	ExternalBowAbility->SetTestCurrentSpecHandle(ExternalBowSpecHandle);
	ExternalBowAbility->SetTestMobileBowMoveSpeedGameplayEffectClass(UTestMobileBowMoveSpeedGE::StaticClass());

	TestTrue(TEXT("Bow start succeeds with pre-existing external GE"), ExternalBowAbility->TestStartMobileBowMoveSpeedEffect());
	TestTrue(TEXT("Bow has valid move speed handle"), ExternalBowAbility->HasTestMobileBowMoveSpeedEffectHandle());
	TestTrue(TEXT("External handle remains valid while Bow is active"), ExternalHandle.IsValid());

	// 7.3 Real Bow EndAbility teardown: external GE and its speed must survive intact
	ExternalBowAbility->EndAbility(ExternalBowSpecHandle, ASC->AbilityActorInfo.Get(), ExternalBowAbility->GetCurrentActivationInfo(), true, false);
	TestFalse(TEXT("Bow handle cleared after real EndAbility"), ExternalBowAbility->HasTestMobileBowMoveSpeedEffectHandle());
	TestTrue(TEXT("External GE handle survives real Bow EndAbility teardown"), ExternalHandle.IsValid());
	TestTrue(TEXT("MoveSpeed attribute restores to pre-Bow external value (350)"), FMath::IsNearlyEqual(ASC->GetNumericAttribute(UCharacterAttributeSet::GetMoveSpeedAttribute()), ExhaustedMoveSpeed));
	TestTrue(TEXT("MaxWalkSpeed restores to pre-Bow external value (350)"), FMath::IsNearlyEqual(MovementComponent->MaxWalkSpeed, ExhaustedMoveSpeed));

	// 7.4 Clean up external GE
	ASC->RemoveActiveGameplayEffect(ExternalHandle);
	TestTrue(TEXT("Full cleanup restores base MoveSpeed 500"), FMath::IsNearlyEqual(ASC->GetNumericAttribute(UCharacterAttributeSet::GetMoveSpeedAttribute()), BaseMoveSpeed));
	TestTrue(TEXT("Full cleanup restores base MaxWalkSpeed 500"), FMath::IsNearlyEqual(MovementComponent->MaxWalkSpeed, BaseMoveSpeed));

	return true;
}

#endif
