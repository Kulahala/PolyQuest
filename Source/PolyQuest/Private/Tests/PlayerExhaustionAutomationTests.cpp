#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/JumpAbility.h"
#include "AbilitySystem/Abilities/PlayerParryAbility.h"
#include "AbilitySystem/CharacterAttributeSet.h"
#include "Character/Player/PlayerCharacter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayAbilitySpec.h"
#include "GameplayTagContainer.h"
#include "Tests/CombatAutomationFixture.h"
#include "Tests/TestJumpGameplayEffects.h"

namespace
{
	void TickExhaustionTestWorld(UWorld* World, const float DeltaSeconds)
	{
		if (World)
		{
			World->Tick(ELevelTick::LEVELTICK_All, DeltaSeconds);
			++GFrameCounter;
		}
	}

	void PrimeExhaustionTimer(UWorld* World)
	{
		// TimerManager ignores repeat ticks in one GFrameCounter; mirror Engine's timer test frame pump.
		TickExhaustionTestWorld(World, 0.0f);
		TickExhaustionTestWorld(World, 0.0f);
	}

	void AdvanceExhaustionTimer(UWorld* World, float DeltaSeconds)
	{
		constexpr float MaxTickStepSeconds = 0.1f;
		while (DeltaSeconds > KINDA_SMALL_NUMBER)
		{
			const float TickStep = FMath::Min(DeltaSeconds, MaxTickStepSeconds);
			TickExhaustionTestWorld(World, TickStep);
			DeltaSeconds -= TickStep;
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerExhaustionAutomationTest, "PolyQuest.Player.Exhaustion", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlayerExhaustionAutomationTest::RunTest(const FString&)
{
	constexpr float BaseMoveSpeed = 500.0f;
	constexpr float ExhaustedMoveSpeed = 350.0f;

	if (!TestNotNull(TEXT("Engine is available for the exhaustion fixture"), GEngine))
	{
		return false;
	}

	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("PlayerExhaustionTestWorld"));
	WorldContext.SetCurrentWorld(World);
	if (!TestNotNull(TEXT("Transient exhaustion test world created"), World))
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

	const FGameplayTag ExhaustedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Exhausted")), false);
	const FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
	TestTrue(TEXT("State.Status.Exhausted resolves"), ExhaustedTag.IsValid());
	TestTrue(TEXT("State.Status.Dead resolves"), DeadTag.IsValid());

	const UPlayerParryAbility* ParryCDO = UPlayerParryAbility::StaticClass()->GetDefaultObject<UPlayerParryAbility>();
	const UTestJumpExhaustionAbility* JumpCDO = UTestJumpExhaustionAbility::StaticClass()->GetDefaultObject<UTestJumpExhaustionAbility>();
	TestNotNull(TEXT("Parry CDO exists"), ParryCDO);
	TestNotNull(TEXT("Jump CDO exists"), JumpCDO);
	if (ParryCDO && JumpCDO)
	{
		TestTrue(TEXT("Parry is blocked while Exhausted"), ParryCDO->GetTestActivationBlockedTags().HasTagExact(ExhaustedTag));
		TestFalse(TEXT("Jump remains unblocked while Exhausted"), JumpCDO->GetTestActivationBlockedTags().HasTagExact(ExhaustedTag));
		TestFalse(TEXT("Jump does not apply a Stamina regeneration delay after its zero-cost activation"), JumpCDO->DoesTestApplyStaminaRegenDelayOnEnd());
	}

	APlayerCharacter* Player = FCombatAutomationFixture::SpawnPlayer(
		World,
		FTransform::Identity,
		[](APlayerCharacter& InPlayer)
		{
			InPlayer.SetTestCombatTeamTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Team.Player")), false));
		});
	TestNotNull(TEXT("Player exhaustion fixture spawned"), Player);
	if (!Player)
	{
		return false;
	}

	UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
	UCharacterMovementComponent* MovementComponent = Player->GetCharacterMovement();
	TestNotNull(TEXT("Player exhaustion fixture has an ASC"), ASC);
	TestNotNull(TEXT("Player exhaustion fixture has CharacterMovement"), MovementComponent);
	if (!ASC || !MovementComponent)
	{
		return false;
	}

	MovementComponent->SetMovementMode(MOVE_Walking);
	ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxStaminaAttribute(), 100.0f);
	ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 100.0f);
	ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMoveSpeedAttribute(), BaseMoveSpeed);
	TestTrue(TEXT("Player fixture applied the persistent Stamina regen setup"), Player->HasTestStaminaRegenEffectApplied());

	const FGameplayAbilitySpecHandle JumpSpecHandle = ASC->GiveAbility(FGameplayAbilitySpec(UTestJumpExhaustionAbility::StaticClass(), 1, INDEX_NONE, Player));
	TestTrue(TEXT("Jump test spec is valid"), JumpSpecHandle.IsValid());

	// First depletion starts the sole recovery window and applies the test-only 0.7 speed effect.
	ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 0.0f);
	TestTrue(TEXT("Zero Stamina starts Exhaustion"), Player->IsTestExhaustionActive());
	TestTrue(TEXT("Zero Stamina writes the Exhausted tag"), ASC->HasMatchingGameplayTag(ExhaustedTag));
	TestTrue(TEXT("Zero Stamina starts exactly one recovery timer"), Player->HasTestExhaustionRecoveryTimer());
	TestTrue(TEXT("Zero Stamina applies the Exhaustion move-speed effect"), Player->HasTestExhaustionMoveSpeedEffect());
	TestTrue(TEXT("Exhaustion resolves MoveSpeed to Guard's 70 percent"), FMath::IsNearlyEqual(ASC->GetNumericAttribute(UCharacterAttributeSet::GetMoveSpeedAttribute()), ExhaustedMoveSpeed));
	TestTrue(TEXT("CharacterMovement follows the exhausted MoveSpeed attribute"), FMath::IsNearlyEqual(MovementComponent->MaxWalkSpeed, ExhaustedMoveSpeed));
	TestTrue(TEXT("Jump can activate at zero Stamina during Exhaustion"),
		JumpCDO && JumpCDO->CanActivateAbility(JumpSpecHandle, ASC->AbilityActorInfo.Get()));
	ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaRegenRateMultiplierAttribute(), 1.0f);
	TestTrue(TEXT("A real zero-cost Jump activates during Exhaustion"), ASC->TryActivateAbility(JumpSpecHandle));
	TestTrue(TEXT("A real Jump retains Exhaustion before the timer expires"), Player->IsTestExhaustionActive());
	TestTrue(TEXT("A real Jump retains the Exhausted tag before the timer expires"), ASC->HasMatchingGameplayTag(ExhaustedTag));
	TestTrue(TEXT("A real Jump retains the original Exhaustion timer"), Player->HasTestExhaustionRecoveryTimer());
	TestTrue(TEXT("A real Jump does not apply its regeneration delay effect"),
		FMath::IsNearlyEqual(ASC->GetNumericAttribute(UCharacterAttributeSet::GetStaminaRegenRateMultiplierAttribute()), 1.0f));
	Player->StopJumping();
	MovementComponent->SetMovementMode(MOVE_Walking);
	PrimeExhaustionTimer(World);

	ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 20.0f);
	TestTrue(TEXT("Positive Stamina before three seconds retains Exhaustion"), ASC->HasMatchingGameplayTag(ExhaustedTag));
	TestTrue(TEXT("Jump can activate during Exhaustion once Stamina is positive"),
		JumpCDO && JumpCDO->CanActivateAbility(JumpSpecHandle, ASC->AbilityActorInfo.Get()));

	AdvanceExhaustionTimer(World, 1.0f);
	ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 0.0f);
	AdvanceExhaustionTimer(World, 2.1f);
	TestTrue(TEXT("A second depletion does not extend the original timer"), Player->IsTestExhaustionActive());
	TestFalse(TEXT("Expired timer is not retained while waiting for positive Stamina"), Player->HasTestExhaustionRecoveryTimer());
	TestTrue(TEXT("Zero Stamina at timer expiry retains Exhaustion"), ASC->HasMatchingGameplayTag(ExhaustedTag));

	ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 20.0f);
	TestFalse(TEXT("Positive Stamina clears Exhaustion after the original timer expired"), Player->IsTestExhaustionActive());
	TestFalse(TEXT("Recovery removes the Exhausted tag"), ASC->HasMatchingGameplayTag(ExhaustedTag));
	TestFalse(TEXT("Recovery removes the exact Exhaustion move-speed handle"), Player->HasTestExhaustionMoveSpeedEffect());
	TestTrue(TEXT("Recovery restores the base MoveSpeed"), FMath::IsNearlyEqual(ASC->GetNumericAttribute(UCharacterAttributeSet::GetMoveSpeedAttribute()), BaseMoveSpeed));
	TestTrue(TEXT("CharacterMovement restores the base MoveSpeed"), FMath::IsNearlyEqual(MovementComponent->MaxWalkSpeed, BaseMoveSpeed));

	// The Player owns one loose-tag contribution and must not clear another system's contribution.
	ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 0.0f);
	ASC->AddLooseGameplayTag(ExhaustedTag);
	AdvanceExhaustionTimer(World, 3.1f);
	ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 20.0f);
	TestFalse(TEXT("Recovery clears the Player-owned Exhaustion lifecycle with an external tag contributor"), Player->IsTestExhaustionActive());
	TestTrue(TEXT("Recovery preserves an external Exhausted loose-tag contribution"), ASC->HasMatchingGameplayTag(ExhaustedTag));
	ASC->RemoveLooseGameplayTag(ExhaustedTag);
	TestFalse(TEXT("Removing the external contributor clears the remaining Exhausted tag"), ASC->HasMatchingGameplayTag(ExhaustedTag));

	// A post-recovery depletion starts a fresh three-second window.
	ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 0.0f);
	TestTrue(TEXT("A post-recovery depletion starts a new Exhaustion cycle"), Player->HasTestExhaustionRecoveryTimer());
	AdvanceExhaustionTimer(World, 2.9f);
	ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 20.0f);
	TestTrue(TEXT("A fresh cycle still holds before its own three-second expiry"), ASC->HasMatchingGameplayTag(ExhaustedTag));
	AdvanceExhaustionTimer(World, 0.2f);
	TestFalse(TEXT("A fresh cycle clears after its own expiry and positive Stamina"), ASC->HasMatchingGameplayTag(ExhaustedTag));

	// A future Player death tag must clear the Player-owned state immediately without adding a death system here.
	ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 0.0f);
	TestTrue(TEXT("Exhaustion is active before the death-tag cleanup check"), Player->IsTestExhaustionActive());
	ASC->AddLooseGameplayTag(DeadTag);
	TestFalse(TEXT("Death-tag receipt clears Exhaustion"), Player->IsTestExhaustionActive());
	TestFalse(TEXT("Death-tag receipt removes the Exhaustion tag"), ASC->HasMatchingGameplayTag(ExhaustedTag));
	TestFalse(TEXT("Death-tag receipt clears the Exhaustion timer"), Player->HasTestExhaustionRecoveryTimer());
	TestTrue(TEXT("Death-tag receipt restores the non-Exhaustion MoveSpeed"), FMath::IsNearlyEqual(ASC->GetNumericAttribute(UCharacterAttributeSet::GetMoveSpeedAttribute()), BaseMoveSpeed));
	ASC->RemoveLooseGameplayTag(DeadTag);

	ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 100.0f);
	ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 0.0f);
	TestTrue(TEXT("Exhaustion is active before EndPlay cleanup"), Player->IsTestExhaustionActive());
	TestTrue(TEXT("Destroying the Player succeeds"), Player->Destroy());
	TestFalse(TEXT("EndPlay removes the Exhausted tag"), ASC->HasMatchingGameplayTag(ExhaustedTag));
	TestTrue(TEXT("EndPlay restores the non-Exhaustion MoveSpeed"), FMath::IsNearlyEqual(ASC->GetNumericAttribute(UCharacterAttributeSet::GetMoveSpeedAttribute()), BaseMoveSpeed));
	AdvanceExhaustionTimer(World, 3.1f);

	return true;
}

#endif
