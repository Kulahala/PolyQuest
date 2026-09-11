#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/Abilities/BowDrawFireAbility.h"
#include "AbilitySystem/Abilities/ChargedAttackAbility.h"
#include "AbilitySystem/Abilities/DodgeAbility.h"
#include "AbilitySystem/Abilities/PlayerGuardAbility.h"
#include "AbilitySystem/Abilities/PlayerLaunchReactionAbility.h"
#include "AbilitySystem/Abilities/PlayerMeleeSkillAbility.h"
#include "AbilitySystem/Abilities/PlayerParryAbility.h"
#include "AbilitySystem/CharacterAttributeSet.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimComposite.h"
#include "Character/Player/PlayerCharacter.h"
#include "Combat/Equipment/BowWeaponDefinition.h"
#include "Combat/Equipment/ProjectileDefinition.h"
#include "Combat/Equipment/WeaponEquipmentComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "Tests/TestProjectileDamageGE.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerActionWindowAutomationTest, "PolyQuest.Player.ActionWindows", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlayerActionWindowAutomationTest::RunTest(const FString& Parameters)
{
	// 1. Setup Test World with valid WorldContext
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("PlayerActionWindowTestWorld"));
	WorldContext.SetCurrentWorld(World);
	if (!TestNotNull(TEXT("Test World created successfully"), World))
	{
		return false;
	}

	FURL WorldURL;
	World->InitializeActorsForPlay(WorldURL);

	struct FTestScopeCleanup
	{
		UWorld* WorldToDestroy;
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
	// SECTION 1: Gameplay Tags Resolution & CDO Static Tag Contract
	// -------------------------------------------------------------------------
	const FGameplayTag TagCancelWindowBegin = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.CancelWindow.Dodge.Begin")), false);
	const FGameplayTag TagCancelWindowEnd = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.CancelWindow.Dodge.End")), false);
	const FGameplayTag TagRateWindowBegin = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.RateWindow.Begin")), false);
	const FGameplayTag TagRateWindowEnd = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.RateWindow.End")), false);
	const FGameplayTag TagCanCancelDodge = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.CanCancel.Dodge")), false);
	const FGameplayTag TagCanCancelDefense = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.CanCancel.Defense")), false);
	const FGameplayTag TagCharging = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Charging")), false);
	const FGameplayTag TagAttacking = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Attacking")), false);
	const FGameplayTag TagDodging = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Dodging")), false);
	const FGameplayTag TagHitReacting = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.HitReacting")), false);
	const FGameplayTag TagAbilityPrimaryAttack = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Primary")), false);
	const FGameplayTag TagAbilityChargedAttack = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Charged")), false);
	const FGameplayTag TagAbilityDodge = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Dodge")), false);
	const FGameplayTag TagAbilityPlayerLaunch = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Reaction.Player.Launch")), false);

	TestTrue(TEXT("Tag Event.Action.CancelWindow.Dodge.Begin is valid"), TagCancelWindowBegin.IsValid());
	TestTrue(TEXT("Tag Event.Action.CancelWindow.Dodge.End is valid"), TagCancelWindowEnd.IsValid());
	TestTrue(TEXT("Tag Event.Action.RateWindow.Begin is valid"), TagRateWindowBegin.IsValid());
	TestTrue(TEXT("Tag Event.Action.RateWindow.End is valid"), TagRateWindowEnd.IsValid());
	TestTrue(TEXT("Tag State.Action.CanCancel.Dodge is valid"), TagCanCancelDodge.IsValid());
	TestTrue(TEXT("Tag State.Action.CanCancel.Defense is valid"), TagCanCancelDefense.IsValid());
	TestTrue(TEXT("Tag State.Action.Charging is valid"), TagCharging.IsValid());
	TestTrue(TEXT("Tag State.Action.Attacking is valid"), TagAttacking.IsValid());
	TestTrue(TEXT("Tag State.Action.Dodging is valid"), TagDodging.IsValid());
	TestTrue(TEXT("Tag State.Action.HitReacting is valid"), TagHitReacting.IsValid());
	TestTrue(TEXT("Tag Ability.Attack.Primary is valid"), TagAbilityPrimaryAttack.IsValid());
	TestTrue(TEXT("Tag Ability.Attack.Charged is valid"), TagAbilityChargedAttack.IsValid());
	TestTrue(TEXT("Tag Ability.Dodge is valid"), TagAbilityDodge.IsValid());
	TestTrue(TEXT("Tag Ability.Reaction.Player.Launch is valid"), TagAbilityPlayerLaunch.IsValid());

	// 1.1 Bow CDO Tag Contract: has Ability.Attack.Primary & Attacking, but NOT static Charging
	const UBowDrawFireAbility* BowCDO = UBowDrawFireAbility::StaticClass()->GetDefaultObject<UBowDrawFireAbility>();
	TestNotNull(TEXT("Bow CDO exists"), BowCDO);
	if (BowCDO)
	{
		TestTrue(TEXT("Bow CDO carries Ability.Attack.Primary tag"), BowCDO->AbilityTags.HasTagExact(TagAbilityPrimaryAttack));
		TestTrue(TEXT("Bow CDO carries State.Action.Attacking tag in AbilityTags"), BowCDO->AbilityTags.HasTagExact(TagAttacking));
		TestTrue(TEXT("Bow CDO carries State.Action.Attacking tag in ActivationOwnedTags"), BowCDO->GetTestActivationOwnedTags().HasTagExact(TagAttacking));
		TestFalse(TEXT("Bow CDO does NOT statically carry State.Action.Charging in AbilityTags"), BowCDO->AbilityTags.HasTagExact(TagCharging));
		TestFalse(TEXT("Bow CDO does NOT statically carry State.Action.Charging in ActivationOwnedTags"), BowCDO->GetTestActivationOwnedTags().HasTagExact(TagCharging));
	}

	// 1.2 Dodge CDO Tag Contract (Slice B): has Ability.Dodge, retrigger enabled, NO Dodging self-blocker, Dodging in OwnedTags
	const UDodgeAbility* DodgeCDO = UDodgeAbility::StaticClass()->GetDefaultObject<UDodgeAbility>();
	TestNotNull(TEXT("Dodge CDO exists"), DodgeCDO);
	if (DodgeCDO)
	{
		TestTrue(TEXT("Dodge CDO carries Ability.Dodge tag"), DodgeCDO->AbilityTags.HasTagExact(TagAbilityDodge));
		TestTrue(TEXT("Dodge CDO has bRetriggerInstancedAbility enabled"), DodgeCDO->GetTestRetriggerInstancedAbility());
		TestFalse(TEXT("Dodge CDO does NOT have State.Action.Dodging in ActivationBlockedTags in Slice B"), DodgeCDO->GetTestActivationBlockedTags().HasTagExact(TagDodging));
		TestTrue(TEXT("Dodge CDO carries State.Action.Dodging in ActivationOwnedTags"), DodgeCDO->GetTestActivationOwnedTags().HasTagExact(TagDodging));
		TestFalse(TEXT("Dodge CDO does NOT carry State.Action.Charging in ActivationBlockedTags"), DodgeCDO->GetTestActivationBlockedTags().HasTagExact(TagCharging));
	}

	// -------------------------------------------------------------------------
	// SECTION 2: Player & ASC Test Setup
	// -------------------------------------------------------------------------
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	APlayerCharacter* Player = World->SpawnActor<APlayerCharacter>(APlayerCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	TestNotNull(TEXT("Spawned PlayerCharacter for action window tests"), Player);
	if (!Player)
	{
		return false;
	}

	if (UCharacterMovementComponent* MoveComp = Player->GetCharacterMovement())
	{
		MoveComp->SetMovementMode(MOVE_Walking);
	}

	UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
	TestNotNull(TEXT("PlayerCharacter has valid ASC"), ASC);
	if (!ASC)
	{
		return false;
	}

	// Initialize Stamina and Health attributes so Super::CanActivateAbility / CheckCost succeeds
	ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 100.0f);
	ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxStaminaAttribute(), 100.0f);
	ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 100.0f);
	ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxHealthAttribute(), 100.0f);

	// -------------------------------------------------------------------------
	// SECTION 3: Dodge Activation & Retrigger Preflight Gates (Attacking & Dodging)
	// -------------------------------------------------------------------------
	FGameplayAbilitySpec DodgeSpec(UDodgeAbility::StaticClass(), 1, INDEX_NONE, Player);
	const FGameplayAbilitySpecHandle DodgeSpecHandle = ASC->GiveAbility(DodgeSpec);
	const UDodgeAbility* DodgeInstance = DodgeCDO;

	// 3.1 Attacking with NO CanCancel.Dodge and NO Charging -> Dodge rejected
	ASC->AddLooseGameplayTag(TagAttacking);
	TestFalse(TEXT("Dodge CanActivateAbility rejected while Attacking without CanCancel.Dodge"),
		DodgeInstance->CanActivateAbility(DodgeSpecHandle, ASC->AbilityActorInfo.Get()));

	// 3.2 Attacking with Charging but NO CanCancel.Dodge -> Dodge MUST STILL BE REJECTED (exemption revoked)
	ASC->AddLooseGameplayTag(TagCharging);
	TestFalse(TEXT("Dodge CanActivateAbility is REJECTED while Attacking+Charging without CanCancel.Dodge"),
		DodgeInstance->CanActivateAbility(DodgeSpecHandle, ASC->AbilityActorInfo.Get()));

	// 3.3 Attacking with CanCancel.Dodge -> Dodge allowed
	ASC->AddLooseGameplayTag(TagCanCancelDodge);
	TestTrue(TEXT("Dodge CanActivateAbility is ACCEPTED when CanCancel.Dodge is present while Attacking"),
		DodgeInstance->CanActivateAbility(DodgeSpecHandle, ASC->AbilityActorInfo.Get()));

	ASC->RemoveLooseGameplayTag(TagAttacking);
	ASC->RemoveLooseGameplayTag(TagCharging);
	ASC->RemoveLooseGameplayTag(TagCanCancelDodge);

	// 3.4 Dodging with NO CanCancel.Dodge -> Dodge rejected (cannot restart dodge without recovery window)
	ASC->AddLooseGameplayTag(TagDodging);
	TestFalse(TEXT("Dodge CanActivateAbility rejected while Dodging without CanCancel.Dodge"),
		DodgeInstance->CanActivateAbility(DodgeSpecHandle, ASC->AbilityActorInfo.Get()));

	// 3.5 Dodging with CanCancel.Dodge -> Dodge allowed (retrigger allowed during recovery cancel window)
	ASC->AddLooseGameplayTag(TagCanCancelDodge);
	TestTrue(TEXT("Dodge CanActivateAbility is ACCEPTED when CanCancel.Dodge is present while Dodging"),
		DodgeInstance->CanActivateAbility(DodgeSpecHandle, ASC->AbilityActorInfo.Get()));

	ASC->RemoveLooseGameplayTag(TagDodging);
	ASC->RemoveLooseGameplayTag(TagCanCancelDodge);

	// 3.6 HitReacting with NO CanCancel.Dodge -> Dodge rejected
	ASC->AddLooseGameplayTag(TagHitReacting);
	TestFalse(TEXT("Dodge CanActivateAbility rejected while HitReacting without CanCancel.Dodge"),
		DodgeInstance->CanActivateAbility(DodgeSpecHandle, ASC->AbilityActorInfo.Get()));

	// 3.7 HitReacting with CanCancel.Dodge and full Stamina -> Dodge allowed
	ASC->AddLooseGameplayTag(TagCanCancelDodge);
	TestTrue(TEXT("Dodge CanActivateAbility is ACCEPTED when CanCancel.Dodge is present while HitReacting"),
		DodgeInstance->CanActivateAbility(DodgeSpecHandle, ASC->AbilityActorInfo.Get()));

	// 3.8 HitReacting with CanCancel.Dodge and low positive Stamina (soft overspend) -> Dodge allowed
	ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 1.0f);
	TestTrue(TEXT("Dodge CanActivateAbility is ACCEPTED with low positive Stamina (1.0) while HitReacting with CanCancel.Dodge"),
		DodgeInstance->CanActivateAbility(DodgeSpecHandle, ASC->AbilityActorInfo.Get()));

	// 3.9 HitReacting with CanCancel.Dodge but exact zero Stamina -> Dodge rejected
	ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 0.0f);
	TestFalse(TEXT("Dodge CanActivateAbility is REJECTED with exact zero Stamina while HitReacting with CanCancel.Dodge"),
		DodgeInstance->CanActivateAbility(DodgeSpecHandle, ASC->AbilityActorInfo.Get()));

	// Restore Stamina to 100
	ASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 100.0f);

	// 3.10 HitReacting with CanCancel.Dodge but Falling -> Dodge rejected
	if (UCharacterMovementComponent* MoveComp = Player->GetCharacterMovement())
	{
		MoveComp->SetMovementMode(MOVE_Falling);
	}
	TestFalse(TEXT("Dodge CanActivateAbility is REJECTED while Falling even if HitReacting and CanCancel.Dodge are present"),
		DodgeInstance->CanActivateAbility(DodgeSpecHandle, ASC->AbilityActorInfo.Get()));

	if (UCharacterMovementComponent* MoveComp = Player->GetCharacterMovement())
	{
		MoveComp->SetMovementMode(MOVE_Walking);
	}

	ASC->RemoveLooseGameplayTag(TagHitReacting);
	ASC->RemoveLooseGameplayTag(TagCanCancelDodge);

	// 3.11 Neither Attacking, Dodging, nor HitReacting -> Normal Dodge allowed
	TestTrue(TEXT("Dodge CanActivateAbility is ACCEPTED when neither Attacking, Dodging, nor HitReacting"),
		DodgeInstance->CanActivateAbility(DodgeSpecHandle, ASC->AbilityActorInfo.Get()));

	// -------------------------------------------------------------------------
	// SECTION 4: Bow Dynamic Charging Lifecycle
	// -------------------------------------------------------------------------
	UBowDrawFireAbility* BowAbility = NewObject<UBowDrawFireAbility>(Player, TEXT("Test_BowActionWindowAbilityInstance"));
	TestNotNull(TEXT("Created BowAbility instance for action window testing"), BowAbility);
	if (BowAbility)
	{
		BowAbility->SetTestCurrentActorInfo(ASC->AbilityActorInfo.Get());

		// Initially not charging
		TestFalse(TEXT("Bow initially does not have charging applied"), BowAbility->GetTestChargingApplied());
		TestFalse(TEXT("ASC initially does not have Charging tag"), ASC->HasMatchingGameplayTag(TagCharging));

		// Dynamic start charging (Draw/Hold phase)
		BowAbility->TestSetCharging(true);
		TestTrue(TEXT("Bow has charging applied after TestSetCharging(true)"), BowAbility->GetTestChargingApplied());
		TestTrue(TEXT("ASC has State.Action.Charging tag after TestSetCharging(true)"), ASC->HasMatchingGameplayTag(TagCharging));

		// Dynamic release / cleanup (TriggerRelease / EndAbility)
		BowAbility->TestSetCharging(false);
		TestFalse(TEXT("Bow charging applied is false after TestSetCharging(false)"), BowAbility->GetTestChargingApplied());
		TestFalse(TEXT("ASC State.Action.Charging tag removed after TestSetCharging(false)"), ASC->HasMatchingGameplayTag(TagCharging));
	}

	// -------------------------------------------------------------------------
	// SECTION 5: Bow Cancel Window Event Validation (Avatar, Montage, Sequence Fallback, Loose Tags)
	// -------------------------------------------------------------------------
	if (BowAbility)
	{
		UAnimMontage* ValidBowMontage = NewObject<UAnimMontage>(GetTransientPackage(), TEXT("Test_ValidBowMontage"));
		UAnimMontage* WrongMontage = NewObject<UAnimMontage>(GetTransientPackage(), TEXT("Test_WrongMontage"));
		UAnimComposite* InnerSequence = NewObject<UAnimComposite>(GetTransientPackage(), TEXT("Test_BowCancelInnerSequence"));
		UAnimComposite* ForeignSequence = NewObject<UAnimComposite>(GetTransientPackage(), TEXT("Test_BowCancelForeignSequence"));

		FSlotAnimationTrack SlotTrack;
		SlotTrack.SlotName = FName(TEXT("DefaultSlot"));
		FAnimSegment Segment;
		Segment.SetAnimReference(InnerSequence);
		Segment.StartPos = 0.0f;
		Segment.AnimStartTime = 0.0f;
		Segment.AnimEndTime = 1.0f;
		Segment.AnimPlayRate = 1.0f;
		SlotTrack.AnimTrack.AnimSegments.Add(Segment);
		ValidBowMontage->SlotAnimTracks.Add(SlotTrack);

		BowAbility->SetTestBowMontage(ValidBowMontage);

		// 5.1 Invalid Avatar Payload -> Rejected, no tags added
		{
			FGameplayEventData BadAvatarPayload;
			BadAvatarPayload.EventTag = TagCancelWindowBegin;
			BadAvatarPayload.Instigator = nullptr;
			BadAvatarPayload.Target = nullptr;
			BadAvatarPayload.OptionalObject = ValidBowMontage;

			BowAbility->TestOnDodgeCancelWindowBegin(BadAvatarPayload);
			TestFalse(TEXT("Bad Avatar payload does not activate DodgeCancelable"), BowAbility->GetTestDodgeCancelable());
			TestFalse(TEXT("Bad Avatar payload does not add CanCancel.Dodge tag to ASC"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
			TestFalse(TEXT("Bad Avatar payload does not add CanCancel.Defense tag to ASC"), ASC->HasMatchingGameplayTag(TagCanCancelDefense));
		}

		// 5.2 Wrong Montage Payload -> Rejected, no tags added
		{
			FGameplayEventData WrongMontagePayload;
			WrongMontagePayload.EventTag = TagCancelWindowBegin;
			WrongMontagePayload.Instigator = Player;
			WrongMontagePayload.Target = Player;
			WrongMontagePayload.OptionalObject = WrongMontage;

			BowAbility->TestOnDodgeCancelWindowBegin(WrongMontagePayload);
			TestFalse(TEXT("Wrong Montage payload does not activate DodgeCancelable"), BowAbility->GetTestDodgeCancelable());
			TestFalse(TEXT("Wrong Montage payload does not add CanCancel.Dodge tag to ASC"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
		}

		// 5.2b Foreign Sequence Payload -> Rejected, no tags added
		{
			FGameplayEventData ForeignSeqPayload;
			ForeignSeqPayload.EventTag = TagCancelWindowBegin;
			ForeignSeqPayload.Instigator = Player;
			ForeignSeqPayload.Target = Player;
			ForeignSeqPayload.OptionalObject = ForeignSequence;

			BowAbility->TestOnDodgeCancelWindowBegin(ForeignSeqPayload);
			TestFalse(TEXT("Foreign Sequence payload does not activate DodgeCancelable"), BowAbility->GetTestDodgeCancelable());
			TestFalse(TEXT("Foreign Sequence payload does not add CanCancel.Dodge tag to ASC"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
		}

		// 5.3 Valid Montage Inner Sequence Payload -> Simultaneously grants CanCancel.Dodge and CanCancel.Defense
		{
			FGameplayEventData InnerSeqBeginPayload;
			InnerSeqBeginPayload.EventTag = TagCancelWindowBegin;
			InnerSeqBeginPayload.Instigator = Player;
			InnerSeqBeginPayload.Target = Player;
			InnerSeqBeginPayload.OptionalObject = InnerSequence;

			BowAbility->TestOnDodgeCancelWindowBegin(InnerSeqBeginPayload);
			TestTrue(TEXT("Inner Sequence payload activates DodgeCancelable"), BowAbility->GetTestDodgeCancelable());
			TestTrue(TEXT("Inner Sequence payload adds CanCancel.Dodge tag to ASC"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
			TestTrue(TEXT("Inner Sequence payload adds CanCancel.Defense tag to ASC"), ASC->HasMatchingGameplayTag(TagCanCancelDefense));

			// Duplicate Begin -> No duplicate tag accumulation / idempotency
			BowAbility->TestOnDodgeCancelWindowBegin(InnerSeqBeginPayload);
			TestTrue(TEXT("Duplicate Begin maintains DodgeCancelable"), BowAbility->GetTestDodgeCancelable());
		}

		// 5.4 Foreign Sequence End Payload -> Does not close valid window
		{
			FGameplayEventData ForeignSeqEndPayload;
			ForeignSeqEndPayload.EventTag = TagCancelWindowEnd;
			ForeignSeqEndPayload.Instigator = Player;
			ForeignSeqEndPayload.Target = Player;
			ForeignSeqEndPayload.OptionalObject = ForeignSequence;

			BowAbility->TestOnDodgeCancelWindowEnd(ForeignSeqEndPayload);
			TestTrue(TEXT("Foreign Sequence End payload does not close active cancel window"), BowAbility->GetTestDodgeCancelable());
			TestTrue(TEXT("CanCancel.Dodge remains active on ASC"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
		}

		// 5.5 Valid Inner Sequence End Payload -> Simultaneously removes CanCancel.Dodge and CanCancel.Defense
		{
			FGameplayEventData InnerSeqEndPayload;
			InnerSeqEndPayload.EventTag = TagCancelWindowEnd;
			InnerSeqEndPayload.Instigator = Player;
			InnerSeqEndPayload.Target = Player;
			InnerSeqEndPayload.OptionalObject = InnerSequence;

			BowAbility->TestOnDodgeCancelWindowEnd(InnerSeqEndPayload);
			TestFalse(TEXT("Inner Sequence End payload deactivates DodgeCancelable"), BowAbility->GetTestDodgeCancelable());
			TestFalse(TEXT("Inner Sequence End payload removes CanCancel.Dodge tag from ASC"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
			TestFalse(TEXT("Inner Sequence End payload removes CanCancel.Defense tag from ASC"), ASC->HasMatchingGameplayTag(TagCanCancelDefense));
		}

		// 5.6 Direct Montage Begin & End Payload
		{
			FGameplayEventData ValidBeginPayload;
			ValidBeginPayload.EventTag = TagCancelWindowBegin;
			ValidBeginPayload.Instigator = Player;
			ValidBeginPayload.Target = Player;
			ValidBeginPayload.OptionalObject = ValidBowMontage;

			BowAbility->TestOnDodgeCancelWindowBegin(ValidBeginPayload);
			TestTrue(TEXT("Valid Direct Montage Begin payload activates DodgeCancelable"), BowAbility->GetTestDodgeCancelable());

			FGameplayEventData ValidEndPayload;
			ValidEndPayload.EventTag = TagCancelWindowEnd;
			ValidEndPayload.Instigator = Player;
			ValidEndPayload.Target = Player;
			ValidEndPayload.OptionalObject = ValidBowMontage;

			BowAbility->TestOnDodgeCancelWindowEnd(ValidEndPayload);
			TestFalse(TEXT("Valid Direct Montage End payload deactivates DodgeCancelable"), BowAbility->GetTestDodgeCancelable());
		}
	}

	// -------------------------------------------------------------------------
	// SECTION 6: Bow Rate Window Validation
	// -------------------------------------------------------------------------
	if (BowAbility)
	{
		UAnimMontage* ValidBowMontage = NewObject<UAnimMontage>(GetTransientPackage(), TEXT("Test_RateBowMontage"));
		UAnimMontage* WrongMontage = NewObject<UAnimMontage>(GetTransientPackage(), TEXT("Test_WrongRateMontage"));
		BowAbility->SetTestBowMontage(ValidBowMontage);

		// 6.1 Non-positive EventMagnitude -> Rejected fail-closed
		{
			FGameplayEventData ZeroMagnitudePayload;
			ZeroMagnitudePayload.EventTag = TagRateWindowBegin;
			ZeroMagnitudePayload.Instigator = Player;
			ZeroMagnitudePayload.Target = Player;
			ZeroMagnitudePayload.OptionalObject = ValidBowMontage;
			ZeroMagnitudePayload.EventMagnitude = 0.0f;

			BowAbility->TestOnRateWindowBegin(ZeroMagnitudePayload);
			TestFalse(TEXT("Zero magnitude rate window is rejected"), BowAbility->GetTestRateWindowApplied());
		}

		// 6.2 Wrong Montage -> Rejected fail-closed
		{
			FGameplayEventData WrongMontageRatePayload;
			WrongMontageRatePayload.EventTag = TagRateWindowBegin;
			WrongMontageRatePayload.Instigator = Player;
			WrongMontageRatePayload.Target = Player;
			WrongMontageRatePayload.OptionalObject = WrongMontage;
			WrongMontageRatePayload.EventMagnitude = 1.5f;

			BowAbility->TestOnRateWindowBegin(WrongMontageRatePayload);
			TestFalse(TEXT("Wrong montage rate window is rejected"), BowAbility->GetTestRateWindowApplied());
		}

		// 6.3 Teardown / baseline restore reset
		BowAbility->TestRestoreBaselineMontageRate();
		TestFalse(TEXT("Baseline restore keeps RateWindowApplied false"), BowAbility->GetTestRateWindowApplied());
	}

	// -------------------------------------------------------------------------
	// SECTION 7: Multi-Action Cancel Conformance (Guard & Parry cancel Bow & Skills)
	// -------------------------------------------------------------------------
	{
		const UPlayerGuardAbility* GuardCDO = UPlayerGuardAbility::StaticClass()->GetDefaultObject<UPlayerGuardAbility>();
		const UPlayerParryAbility* ParryCDO = UPlayerParryAbility::StaticClass()->GetDefaultObject<UPlayerParryAbility>();
		const UPlayerMeleeSkillAbility* SkillCDO = UPlayerMeleeSkillAbility::StaticClass()->GetDefaultObject<UPlayerMeleeSkillAbility>();
		TestNotNull(TEXT("Guard CDO exists"), GuardCDO);
		TestNotNull(TEXT("Parry CDO exists"), ParryCDO);
		TestNotNull(TEXT("Skill CDO exists"), SkillCDO);

		// 7.1 Verify 4 new orthogonal capability/teardown tags are registered and valid, and legacy tag is unregistered
		const FGameplayTag TagCancelByDodge = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.CancelableBy.Dodge")), false);
		const FGameplayTag TagCancelByDefense = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.CancelableBy.Defense")), false);
		const FGameplayTag TagCancelByReaction = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.CancelableBy.Reaction")), false);
		const FGameplayTag TagTeardownOnUnpossess = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.Teardown.OnUnpossess")), false);
		const FGameplayTag TagLegacyMeleeSkill = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Skill.Melee")), false);

		TestTrue(TEXT("Tag Ability.Action.CancelableBy.Dodge is valid"), TagCancelByDodge.IsValid());
		TestTrue(TEXT("Tag Ability.Action.CancelableBy.Defense is valid"), TagCancelByDefense.IsValid());
		TestTrue(TEXT("Tag Ability.Action.CancelableBy.Reaction is valid"), TagCancelByReaction.IsValid());
		TestTrue(TEXT("Tag Ability.Action.Teardown.OnUnpossess is valid"), TagTeardownOnUnpossess.IsValid());
		TestFalse(TEXT("Legacy Ability.Skill.Melee tag is NOT registered/valid"), TagLegacyMeleeSkill.IsValid());

		// 7.2 Verify UPlayerMeleeSkillAbility CDO has all 4 capability tags
		if (SkillCDO)
		{
			TestTrue(TEXT("Skill CDO contains CancelableBy.Dodge"), SkillCDO->AbilityTags.HasTagExact(TagCancelByDodge));
			TestTrue(TEXT("Skill CDO contains CancelableBy.Defense"), SkillCDO->AbilityTags.HasTagExact(TagCancelByDefense));
			TestTrue(TEXT("Skill CDO contains CancelableBy.Reaction"), SkillCDO->AbilityTags.HasTagExact(TagCancelByReaction));
			TestTrue(TEXT("Skill CDO contains Teardown.OnUnpossess"), SkillCDO->AbilityTags.HasTagExact(TagTeardownOnUnpossess));
		}

		// 7.3 Verify Guard CDO DefenseCancelableAbilityTags
		if (GuardCDO)
		{
			const FGameplayTagContainer& GuardCancelTags = GuardCDO->GetTestDefenseCancelableAbilityTags();
			TestEqual(TEXT("Guard CDO DefenseCancelableAbilityTags has exactly 5 tags"), GuardCancelTags.Num(), 5);
			TestTrue(TEXT("Guard CDO cancels Ability.Attack.Primary"), GuardCancelTags.HasTagExact(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Primary")), false)));
			TestTrue(TEXT("Guard CDO cancels Ability.Attack.Light"), GuardCancelTags.HasTagExact(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Light")), false)));
			TestTrue(TEXT("Guard CDO cancels Ability.Attack.Charged"), GuardCancelTags.HasTagExact(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Charged")), false)));
			TestTrue(TEXT("Guard CDO cancels Ability.Attack.Sprint"), GuardCancelTags.HasTagExact(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Sprint")), false)));
			TestTrue(TEXT("Guard CDO cancels Ability.Action.CancelableBy.Defense"), GuardCancelTags.HasTagExact(TagCancelByDefense));
		}

		// 7.4 Verify Parry CDO DefenseCancelableAbilityTags
		if (ParryCDO)
		{
			const FGameplayTagContainer& ParryCancelTags = ParryCDO->GetTestDefenseCancelableAbilityTags();
			TestEqual(TEXT("Parry CDO DefenseCancelableAbilityTags has exactly 5 tags"), ParryCancelTags.Num(), 5);
			TestTrue(TEXT("Parry CDO cancels Ability.Attack.Primary"), ParryCancelTags.HasTagExact(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Primary")), false)));
			TestTrue(TEXT("Parry CDO cancels Ability.Attack.Light"), ParryCancelTags.HasTagExact(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Light")), false)));
			TestTrue(TEXT("Parry CDO cancels Ability.Attack.Charged"), ParryCancelTags.HasTagExact(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Charged")), false)));
			TestTrue(TEXT("Parry CDO cancels Ability.Attack.Sprint"), ParryCancelTags.HasTagExact(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Sprint")), false)));
			TestTrue(TEXT("Parry CDO cancels Ability.Action.CancelableBy.Defense"), ParryCancelTags.HasTagExact(TagCancelByDefense));
		}
	}

	// -------------------------------------------------------------------------
	// SECTION 8: Charged Attack Pause-Latched Cancel Window Lifecycle
	// -------------------------------------------------------------------------
	UChargedAttackAbility* ChargedAbility = NewObject<UChargedAttackAbility>(Player, TEXT("Test_ChargedActionWindowAbilityInstance"));
	TestNotNull(TEXT("Created ChargedAbility instance for action window testing"), ChargedAbility);
	if (ChargedAbility)
	{
		ChargedAbility->SetTestCurrentActorInfo(ASC->AbilityActorInfo.Get());
		UAnimMontage* ValidChargedMontage = NewObject<UAnimMontage>(GetTransientPackage(), TEXT("Test_ValidChargedMontage"));
		ChargedAbility->Test_SetBoundMontageForTest(ValidChargedMontage);

		const FGameplayTag TagHoldReady = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Attack.Charged.HoldReady")), false);
		TestTrue(TEXT("Tag Event.Attack.Charged.HoldReady is valid"), TagHoldReady.IsValid());

		FGameplayEventData BeginPayload;
		BeginPayload.EventTag = TagCancelWindowBegin;
		BeginPayload.Instigator = Player;
		BeginPayload.Target = Player;
		BeginPayload.OptionalObject = ValidChargedMontage;

		FGameplayEventData EndPayload;
		EndPayload.EventTag = TagCancelWindowEnd;
		EndPayload.Instigator = Player;
		EndPayload.Target = Player;
		EndPayload.OptionalObject = ValidChargedMontage;

		FGameplayEventData HoldReadyPayload;
		HoldReadyPayload.EventTag = TagHoldReady;
		HoldReadyPayload.Instigator = Player;
		HoldReadyPayload.Target = Player;
		HoldReadyPayload.OptionalObject = ValidChargedMontage;

		// 8.1 Scenario A: Legal window Begin -> HoldReady Pause -> Pseudo End does NOT strip CanCancel tags
		{
			ChargedAbility->Test_OnDodgeCancelWindowBegin(BeginPayload);
			TestTrue(TEXT("Charged window Begin grants CanCancel.Dodge"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
			TestTrue(TEXT("Charged window Begin grants CanCancel.Defense"), ASC->HasMatchingGameplayTag(TagCanCancelDefense));

			// Enter Hold (Pause)
			ChargedAbility->Test_SimulateHoldReady();
			TestTrue(TEXT("Charged ability marked paused at HoldReady"), ChargedAbility->Test_IsMontagePausedAtHoldReady());
			TestTrue(TEXT("Charged cancel window latched across pause"), ChargedAbility->Test_IsHoldCancelWindowLatched());

			// Pseudo End fired by engine pause truncation
			ChargedAbility->Test_OnDodgeCancelWindowEnd(EndPayload);
			TestTrue(TEXT("Pause pseudo-End does NOT remove CanCancel.Dodge"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
			TestTrue(TEXT("Pause pseudo-End does NOT remove CanCancel.Defense"), ASC->HasMatchingGameplayTag(TagCanCancelDefense));

			// Dodge CanActivate succeeds while latched in Hold
			ASC->AddLooseGameplayTag(TagAttacking);
			TestTrue(TEXT("Dodge CanActivate succeeds during Charged Hold while window is latched"),
				DodgeInstance->CanActivateAbility(DodgeSpecHandle, ASC->AbilityActorInfo.Get()));
			ASC->RemoveLooseGameplayTag(TagAttacking);

			// 8.2 Scenario C: Resume (BeginRelease) -> Real End removes both tags and clears latch
			ChargedAbility->Test_BeginRelease(0.5f);
			TestFalse(TEXT("BeginRelease resumes montage pause state"), ChargedAbility->Test_IsMontagePausedAtHoldReady());

			// Real End on timeline
			ChargedAbility->Test_OnDodgeCancelWindowEnd(EndPayload);
			TestFalse(TEXT("Real End removes CanCancel.Dodge"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
			TestFalse(TEXT("Real End removes CanCancel.Defense"), ASC->HasMatchingGameplayTag(TagCanCancelDefense));
			TestFalse(TEXT("Real End clears latch"), ChargedAbility->Test_IsHoldCancelWindowLatched());
		}

		// 8.3 Scenario B: Enter Hold without pre-existing cancel window -> Does NOT gain tags or latch
		{
			UChargedAttackAbility* UnwindowedChargedAbility = NewObject<UChargedAttackAbility>(Player, TEXT("Test_UnwindowedChargedAbilityInstance"));
			UnwindowedChargedAbility->SetTestCurrentActorInfo(ASC->AbilityActorInfo.Get());
			UnwindowedChargedAbility->Test_SetBoundMontageForTest(ValidChargedMontage);

			UnwindowedChargedAbility->Test_SimulateHoldReady();
			TestTrue(TEXT("Unwindowed ability marked paused at HoldReady"), UnwindowedChargedAbility->Test_IsMontagePausedAtHoldReady());
			TestFalse(TEXT("Unwindowed ability does NOT latch cancel window"), UnwindowedChargedAbility->Test_IsHoldCancelWindowLatched());
			TestFalse(TEXT("Unwindowed ability does NOT gain CanCancel.Dodge"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
			TestFalse(TEXT("Unwindowed ability does NOT gain CanCancel.Defense"), ASC->HasMatchingGameplayTag(TagCanCancelDefense));

			// End payload during pause
			UnwindowedChargedAbility->Test_OnDodgeCancelWindowEnd(EndPayload);
			TestFalse(TEXT("CanCancel.Dodge remains absent"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
			TestFalse(TEXT("CanCancel.Defense remains absent"), ASC->HasMatchingGameplayTag(TagCanCancelDefense));
		}

		// 8.4 Scenario D: EndAbility unconditionally cleans up latch and all tags
		{
			ChargedAbility->Test_OnDodgeCancelWindowBegin(BeginPayload);
			ChargedAbility->Test_SimulateHoldReady();
			TestTrue(TEXT("Charged ability re-latched before EndAbility"), ChargedAbility->Test_IsHoldCancelWindowLatched());

			ChargedAbility->EndAbility(ChargedAbility->GetCurrentAbilitySpecHandle(), ASC->AbilityActorInfo.Get(), ChargedAbility->GetCurrentActivationInfo(), true, true);
			TestFalse(TEXT("EndAbility clears latch"), ChargedAbility->Test_IsHoldCancelWindowLatched());
			TestFalse(TEXT("EndAbility removes CanCancel.Dodge"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
			TestFalse(TEXT("EndAbility removes CanCancel.Defense"), ASC->HasMatchingGameplayTag(TagCanCancelDefense));
		}
	}

	// -------------------------------------------------------------------------
	// SECTION 9: Dodge Cancel Window & Rate Window Lifecycle (Slice B)
	// -------------------------------------------------------------------------
	UDodgeAbility* DodgeAbility = NewObject<UDodgeAbility>(Player, TEXT("Test_DodgeLifecycleAbilityInstance"));
	TestNotNull(TEXT("Created DodgeAbility instance for lifecycle testing"), DodgeAbility);
	if (DodgeAbility)
	{
		DodgeAbility->SetTestCurrentActorInfo(ASC->AbilityActorInfo.Get());

		UAnimMontage* ValidDodgeMontage = NewObject<UAnimMontage>(GetTransientPackage(), TEXT("Test_ValidDodgeMontage"));
		UAnimMontage* WrongDodgeMontage = NewObject<UAnimMontage>(GetTransientPackage(), TEXT("Test_WrongDodgeMontage"));
		UAnimComposite* InnerDodgeSequence = NewObject<UAnimComposite>(GetTransientPackage(), TEXT("Test_DodgeInnerSequence"));
		UAnimComposite* ForeignDodgeSequence = NewObject<UAnimComposite>(GetTransientPackage(), TEXT("Test_DodgeForeignSequence"));

		FSlotAnimationTrack SlotTrack;
		SlotTrack.SlotName = FName(TEXT("DefaultSlot"));
		FAnimSegment Segment;
		Segment.SetAnimReference(InnerDodgeSequence);
		Segment.StartPos = 0.0f;
		Segment.AnimStartTime = 0.0f;
		Segment.AnimEndTime = 1.0f;
		Segment.AnimPlayRate = 1.0f;
		SlotTrack.AnimTrack.AnimSegments.Add(Segment);
		ValidDodgeMontage->SlotAnimTracks.Add(SlotTrack);

		DodgeAbility->SetTestActiveMontage(ValidDodgeMontage);

		// 9.1 Invalid Avatar Payload -> Rejected
		{
			FGameplayEventData BadAvatarPayload;
			BadAvatarPayload.EventTag = TagCancelWindowBegin;
			BadAvatarPayload.Instigator = nullptr;
			BadAvatarPayload.Target = nullptr;
			BadAvatarPayload.OptionalObject = ValidDodgeMontage;

			DodgeAbility->TestOnCancelWindowBegin(BadAvatarPayload);
			TestFalse(TEXT("Bad Avatar payload does not activate DodgeCancelable on Dodge"), DodgeAbility->GetTestDodgeCancelable());
			TestFalse(TEXT("Bad Avatar payload does not add CanCancel.Dodge tag to ASC"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
		}

		// 9.2 Wrong Montage Payload -> Rejected
		{
			FGameplayEventData WrongMontagePayload;
			WrongMontagePayload.EventTag = TagCancelWindowBegin;
			WrongMontagePayload.Instigator = Player;
			WrongMontagePayload.Target = Player;
			WrongMontagePayload.OptionalObject = WrongDodgeMontage;

			DodgeAbility->TestOnCancelWindowBegin(WrongMontagePayload);
			TestFalse(TEXT("Wrong Montage payload does not activate DodgeCancelable on Dodge"), DodgeAbility->GetTestDodgeCancelable());
			TestFalse(TEXT("Wrong Montage payload does not add CanCancel.Dodge tag to ASC"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
		}

		// 9.3 Foreign Sequence Payload -> Rejected
		{
			FGameplayEventData ForeignSeqPayload;
			ForeignSeqPayload.EventTag = TagCancelWindowBegin;
			ForeignSeqPayload.Instigator = Player;
			ForeignSeqPayload.Target = Player;
			ForeignSeqPayload.OptionalObject = ForeignDodgeSequence;

			DodgeAbility->TestOnCancelWindowBegin(ForeignSeqPayload);
			TestFalse(TEXT("Foreign Sequence payload does not activate DodgeCancelable on Dodge"), DodgeAbility->GetTestDodgeCancelable());
			TestFalse(TEXT("Foreign Sequence payload does not add CanCancel.Dodge tag to ASC"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
		}

		// 9.4 Valid Inner Sequence Begin Payload -> Grants CanCancel.Dodge ONLY (NOT CanCancel.Defense)
		{
			FGameplayEventData InnerSeqBeginPayload;
			InnerSeqBeginPayload.EventTag = TagCancelWindowBegin;
			InnerSeqBeginPayload.Instigator = Player;
			InnerSeqBeginPayload.Target = Player;
			InnerSeqBeginPayload.OptionalObject = InnerDodgeSequence;

			DodgeAbility->TestOnCancelWindowBegin(InnerSeqBeginPayload);
			TestTrue(TEXT("Inner Sequence payload activates DodgeCancelable on Dodge"), DodgeAbility->GetTestDodgeCancelable());
			TestTrue(TEXT("Inner Sequence payload adds CanCancel.Dodge tag to ASC"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
			TestFalse(TEXT("Dodge CancelWindow does NOT add CanCancel.Defense to ASC"), ASC->HasMatchingGameplayTag(TagCanCancelDefense));

			// Idempotent duplicate Begin
			DodgeAbility->TestOnCancelWindowBegin(InnerSeqBeginPayload);
			TestTrue(TEXT("Duplicate Begin maintains DodgeCancelable on Dodge"), DodgeAbility->GetTestDodgeCancelable());
		}

		// 9.5 Foreign Sequence End Payload -> Does not close window
		{
			FGameplayEventData ForeignSeqEndPayload;
			ForeignSeqEndPayload.EventTag = TagCancelWindowEnd;
			ForeignSeqEndPayload.Instigator = Player;
			ForeignSeqEndPayload.Target = Player;
			ForeignSeqEndPayload.OptionalObject = ForeignDodgeSequence;

			DodgeAbility->TestOnCancelWindowEnd(ForeignSeqEndPayload);
			TestTrue(TEXT("Foreign Sequence End does not close active cancel window on Dodge"), DodgeAbility->GetTestDodgeCancelable());
			TestTrue(TEXT("CanCancel.Dodge remains active on ASC"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
		}

		// 9.6 Valid Inner Sequence End Payload -> Closes window and removes tag
		{
			FGameplayEventData InnerSeqEndPayload;
			InnerSeqEndPayload.EventTag = TagCancelWindowEnd;
			InnerSeqEndPayload.Instigator = Player;
			InnerSeqEndPayload.Target = Player;
			InnerSeqEndPayload.OptionalObject = InnerDodgeSequence;

			DodgeAbility->TestOnCancelWindowEnd(InnerSeqEndPayload);
			TestFalse(TEXT("Valid Inner Sequence End deactivates DodgeCancelable on Dodge"), DodgeAbility->GetTestDodgeCancelable());
			TestFalse(TEXT("Valid Inner Sequence End removes CanCancel.Dodge from ASC"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
		}

		// 9.7 Rate Window Lifecycle
		{
			// Invalid Rate <= 0.0 -> Rejected
			FGameplayEventData NonPositiveRatePayload;
			NonPositiveRatePayload.EventTag = TagRateWindowBegin;
			NonPositiveRatePayload.Instigator = Player;
			NonPositiveRatePayload.Target = Player;
			NonPositiveRatePayload.OptionalObject = InnerDodgeSequence;
			NonPositiveRatePayload.EventMagnitude = 0.0f;

			DodgeAbility->TestOnRateWindowBegin(NonPositiveRatePayload);
			TestFalse(TEXT("Rate window with magnitude <= 0 is rejected"), DodgeAbility->GetTestRateWindowApplied());

			// Foreign Sequence Rate Begin -> Rejected
			FGameplayEventData ForeignRatePayload;
			ForeignRatePayload.EventTag = TagRateWindowBegin;
			ForeignRatePayload.Instigator = Player;
			ForeignRatePayload.Target = Player;
			ForeignRatePayload.OptionalObject = ForeignDodgeSequence;
			ForeignRatePayload.EventMagnitude = 1.5f;

			DodgeAbility->TestOnRateWindowBegin(ForeignRatePayload);
			TestFalse(TEXT("Rate window with foreign Sequence is rejected"), DodgeAbility->GetTestRateWindowApplied());

			// Foreign Sequence Rate End -> Rejected
			FGameplayEventData ForeignRateEndPayload;
			ForeignRateEndPayload.EventTag = TagRateWindowEnd;
			ForeignRateEndPayload.Instigator = Player;
			ForeignRateEndPayload.Target = Player;
			ForeignRateEndPayload.OptionalObject = ForeignDodgeSequence;

			DodgeAbility->TestOnRateWindowEnd(ForeignRateEndPayload);

			// Valid Rate End restores baseline
			DodgeAbility->TestRestoreBaselineMontageRate();
			TestFalse(TEXT("Restore baseline clears rate window flag"), DodgeAbility->GetTestRateWindowApplied());
		}

		// 9.8 EndAbility cleanup removes CanCancel.Dodge and clears rate state
		{
			FGameplayEventData InnerSeqBeginPayload;
			InnerSeqBeginPayload.EventTag = TagCancelWindowBegin;
			InnerSeqBeginPayload.Instigator = Player;
			InnerSeqBeginPayload.Target = Player;
			InnerSeqBeginPayload.OptionalObject = InnerDodgeSequence;

			DodgeAbility->TestOnCancelWindowBegin(InnerSeqBeginPayload);
			TestTrue(TEXT("CanCancel.Dodge granted before EndAbility"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));

			DodgeAbility->EndAbility(DodgeAbility->GetCurrentAbilitySpecHandle(), ASC->AbilityActorInfo.Get(), DodgeAbility->GetCurrentActivationInfo(), true, true);
			TestFalse(TEXT("EndAbility removes CanCancel.Dodge"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
			TestFalse(TEXT("EndAbility clears DodgeCancelable"), DodgeAbility->GetTestDodgeCancelable());
			TestFalse(TEXT("EndAbility clears RateWindowApplied"), DodgeAbility->GetTestRateWindowApplied());
		}

		// 9.9 Continuous Retrigger Tag Lifecycle:
		// Old instance in recovery CancelWindow -> Old EndAbility cleans CanCancel.Dodge ->
		// New instance starts up in Dodging state -> Second rapid Dodge request during early startup MUST BE REJECTED.
		UDodgeAbility* RetriggerDodgeAbility = NewObject<UDodgeAbility>(Player, TEXT("Test_DodgeRetriggerAbilityInstance"));
		TestNotNull(TEXT("Created RetriggerDodgeAbility instance"), RetriggerDodgeAbility);
		if (RetriggerDodgeAbility)
		{
			RetriggerDodgeAbility->SetTestCurrentActorInfo(ASC->AbilityActorInfo.Get());
			RetriggerDodgeAbility->SetTestActiveMontage(ValidDodgeMontage);

			FGameplayEventData InnerSeqBeginPayload;
			InnerSeqBeginPayload.EventTag = TagCancelWindowBegin;
			InnerSeqBeginPayload.Instigator = Player;
			InnerSeqBeginPayload.Target = Player;
			InnerSeqBeginPayload.OptionalObject = InnerDodgeSequence;

			// Step A: Old instance enters recovery CancelWindow
			RetriggerDodgeAbility->TestOnCancelWindowBegin(InnerSeqBeginPayload);
			TestTrue(TEXT("Old dodge has DodgeCancelable active"), RetriggerDodgeAbility->GetTestDodgeCancelable());
			TestTrue(TEXT("ASC has CanCancel.Dodge tag during old dodge recovery"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));

			// Step B: Retrigger occurs: Old EndAbility runs
			RetriggerDodgeAbility->EndAbility(DodgeSpecHandle, ASC->AbilityActorInfo.Get(), RetriggerDodgeAbility->GetCurrentActivationInfo(), true, false);
			TestFalse(TEXT("Old dodge EndAbility cleared bDodgeCancelable"), RetriggerDodgeAbility->GetTestDodgeCancelable());
			TestFalse(TEXT("Old dodge EndAbility removed CanCancel.Dodge from ASC"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));

			// Step C: New dodge instance starts up (Dodging is active on ASC, no CanCancel.Dodge)
			ASC->AddLooseGameplayTag(TagDodging);
			TestTrue(TEXT("ASC has State.Action.Dodging on startup of second dodge"), ASC->HasMatchingGameplayTag(TagDodging));
			TestFalse(TEXT("ASC does NOT have CanCancel.Dodge tag on startup of second dodge"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));

			// Step D: Second rapid request while in early startup phase of second dodge -> MUST BE REJECTED
			const bool bCanActivateSecondRapid = DodgeCDO->CanActivateAbility(DodgeSpecHandle, ASC->AbilityActorInfo.Get());
			TestFalse(TEXT("Second rapid Dodge request during startup phase MUST BE REJECTED"), bCanActivateSecondRapid);

			// Step E: Clean up loose dodging tag
			ASC->RemoveLooseGameplayTag(TagDodging);
		}
	}

	// -------------------------------------------------------------------------
	// SECTION 10: Player Launch Root Motion Knockdown Dodge Cancel Window Lifecycle
	// -------------------------------------------------------------------------
	UPlayerLaunchReactionAbility* LaunchAbility = NewObject<UPlayerLaunchReactionAbility>(Player, TEXT("Test_PlayerLaunchReactionAbilityInstance"));
	TestNotNull(TEXT("Created PlayerLaunchReactionAbility instance for lifecycle testing"), LaunchAbility);
	if (LaunchAbility)
	{
		LaunchAbility->SetTestCurrentActorInfo(ASC->AbilityActorInfo.Get());

		UAnimMontage* ValidRootMotionKnockdownMontage = NewObject<UAnimMontage>(GetTransientPackage(), TEXT("Test_ValidRootMotionKnockdownMontage"));
		UAnimMontage* WrongRootMotionKnockdownMontage = NewObject<UAnimMontage>(GetTransientPackage(), TEXT("Test_WrongRootMotionKnockdownMontage"));
		UAnimComposite* InnerKnockdownSequence = NewObject<UAnimComposite>(GetTransientPackage(), TEXT("Test_KnockdownInnerSequence"));
		UAnimComposite* ForeignKnockdownSequence = NewObject<UAnimComposite>(GetTransientPackage(), TEXT("Test_KnockdownForeignSequence"));

		FSlotAnimationTrack SlotTrack;
		SlotTrack.SlotName = FName(TEXT("DefaultSlot"));
		FAnimSegment Segment;
		Segment.SetAnimReference(InnerKnockdownSequence);
		Segment.StartPos = 0.0f;
		Segment.AnimStartTime = 0.0f;
		Segment.AnimEndTime = 1.0f;
		Segment.AnimPlayRate = 1.0f;
		SlotTrack.AnimTrack.AnimSegments.Add(Segment);
		ValidRootMotionKnockdownMontage->SlotAnimTracks.Add(SlotTrack);

		LaunchAbility->SetTestRootMotionKnockdownMontage(ValidRootMotionKnockdownMontage);
		LaunchAbility->SetTestActiveMontage(ValidRootMotionKnockdownMontage);

		// 10.1 Non-RootMotionKnockdown Phase -> Events ignored
		{
			FGameplayEventData BeginPayload;
			BeginPayload.EventTag = TagCancelWindowBegin;
			BeginPayload.Instigator = Player;
			BeginPayload.Target = Player;
			BeginPayload.OptionalObject = ValidRootMotionKnockdownMontage;

			LaunchAbility->SetTestBypassAnimInstanceActiveCheck(true);
			LaunchAbility->TestOnCancelWindowBegin(BeginPayload);
			TestFalse(TEXT("CancelWindowBegin during non-RootMotionKnockdown phase is rejected"), LaunchAbility->GetTestDodgeCancelable());
			TestFalse(TEXT("Non-RootMotionKnockdown phase does not add CanCancel.Dodge tag to ASC"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
		}

		// Switch to RootMotionKnockdown phase
		LaunchAbility->SetTestCurrentPhaseToRootMotionKnockdown();

		// 10.2 Inactive / Null AnimInstance Check -> Rejected when active check is enforced
		{
			LaunchAbility->SetTestBypassAnimInstanceActiveCheck(false);
			LaunchAbility->SetTestBoundAnimInstance(nullptr);

			FGameplayEventData BeginPayload;
			BeginPayload.EventTag = TagCancelWindowBegin;
			BeginPayload.Instigator = Player;
			BeginPayload.Target = Player;
			BeginPayload.OptionalObject = ValidRootMotionKnockdownMontage;

			LaunchAbility->TestOnCancelWindowBegin(BeginPayload);
			TestFalse(TEXT("Inactive / null AnimInstance fails closed and rejects Begin"), LaunchAbility->GetTestDodgeCancelable());
			TestFalse(TEXT("Inactive AnimInstance does not add CanCancel.Dodge tag to ASC"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
		}

		// Enable bypass for isolated synthetic identity & payload tests
		LaunchAbility->SetTestBypassAnimInstanceActiveCheck(true);

		// 10.3 Invalid Avatar Payload -> Rejected
		{
			FGameplayEventData BadAvatarPayload;
			BadAvatarPayload.EventTag = TagCancelWindowBegin;
			BadAvatarPayload.Instigator = nullptr;
			BadAvatarPayload.Target = nullptr;
			BadAvatarPayload.OptionalObject = ValidRootMotionKnockdownMontage;

			LaunchAbility->TestOnCancelWindowBegin(BadAvatarPayload);
			TestFalse(TEXT("Bad Avatar payload does not activate DodgeCancelable on Launch"), LaunchAbility->GetTestDodgeCancelable());
			TestFalse(TEXT("Bad Avatar payload does not add CanCancel.Dodge tag to ASC"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
		}

		// 10.4 Wrong Montage Payload -> Rejected
		{
			FGameplayEventData WrongMontagePayload;
			WrongMontagePayload.EventTag = TagCancelWindowBegin;
			WrongMontagePayload.Instigator = Player;
			WrongMontagePayload.Target = Player;
			WrongMontagePayload.OptionalObject = WrongRootMotionKnockdownMontage;

			LaunchAbility->TestOnCancelWindowBegin(WrongMontagePayload);
			TestFalse(TEXT("Wrong Montage payload does not activate DodgeCancelable on Launch"), LaunchAbility->GetTestDodgeCancelable());
			TestFalse(TEXT("Wrong Montage payload does not add CanCancel.Dodge tag to ASC"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
		}

		// 10.5 Foreign Sequence Payload -> Rejected
		{
			FGameplayEventData ForeignSeqPayload;
			ForeignSeqPayload.EventTag = TagCancelWindowBegin;
			ForeignSeqPayload.Instigator = Player;
			ForeignSeqPayload.Target = Player;
			ForeignSeqPayload.OptionalObject = ForeignKnockdownSequence;

			LaunchAbility->TestOnCancelWindowBegin(ForeignSeqPayload);
			TestFalse(TEXT("Foreign Sequence payload does not activate DodgeCancelable on Launch"), LaunchAbility->GetTestDodgeCancelable());
			TestFalse(TEXT("Foreign Sequence payload does not add CanCancel.Dodge tag to ASC"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
		}

		// 10.6 Valid Inner Sequence Begin Payload -> Grants CanCancel.Dodge ONLY (NOT CanCancel.Defense)
		{
			FGameplayEventData InnerSeqBeginPayload;
			InnerSeqBeginPayload.EventTag = TagCancelWindowBegin;
			InnerSeqBeginPayload.Instigator = Player;
			InnerSeqBeginPayload.Target = Player;
			InnerSeqBeginPayload.OptionalObject = InnerKnockdownSequence;

			LaunchAbility->TestOnCancelWindowBegin(InnerSeqBeginPayload);
			TestTrue(TEXT("Inner Sequence payload activates DodgeCancelable on Launch"), LaunchAbility->GetTestDodgeCancelable());
			TestTrue(TEXT("Inner Sequence payload adds CanCancel.Dodge tag to ASC"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
			TestFalse(TEXT("Launch CancelWindow does NOT add CanCancel.Defense to ASC"), ASC->HasMatchingGameplayTag(TagCanCancelDefense));

			// Idempotent duplicate Begin
			LaunchAbility->TestOnCancelWindowBegin(InnerSeqBeginPayload);
			TestTrue(TEXT("Duplicate Begin maintains DodgeCancelable on Launch"), LaunchAbility->GetTestDodgeCancelable());
		}

		// 10.7 Valid End Payload -> Removes CanCancel.Dodge
		{
			FGameplayEventData InnerSeqEndPayload;
			InnerSeqEndPayload.EventTag = TagCancelWindowEnd;
			InnerSeqEndPayload.Instigator = Player;
			InnerSeqEndPayload.Target = Player;
			InnerSeqEndPayload.OptionalObject = InnerKnockdownSequence;

			LaunchAbility->TestOnCancelWindowEnd(InnerSeqEndPayload);
			TestFalse(TEXT("Inner Sequence End payload clears DodgeCancelable on Launch"), LaunchAbility->GetTestDodgeCancelable());
			TestFalse(TEXT("Inner Sequence End payload removes CanCancel.Dodge tag from ASC"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));

			// End before Begin is idempotent fail-closed
			LaunchAbility->TestOnCancelWindowEnd(InnerSeqEndPayload);
			TestFalse(TEXT("End before Begin remains false"), LaunchAbility->GetTestDodgeCancelable());
			TestFalse(TEXT("End before Begin tag remains absent"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
		}

		// 10.8 EndAbility cleanup removes CanCancel.Dodge
		{
			FGameplayEventData InnerSeqBeginPayload;
			InnerSeqBeginPayload.EventTag = TagCancelWindowBegin;
			InnerSeqBeginPayload.Instigator = Player;
			InnerSeqBeginPayload.Target = Player;
			InnerSeqBeginPayload.OptionalObject = InnerKnockdownSequence;

			LaunchAbility->TestOnCancelWindowBegin(InnerSeqBeginPayload);
			TestTrue(TEXT("CanCancel.Dodge granted before Launch EndAbility"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));

			LaunchAbility->EndAbility(LaunchAbility->GetCurrentAbilitySpecHandle(), ASC->AbilityActorInfo.Get(), LaunchAbility->GetCurrentActivationInfo(), true, true);
			TestFalse(TEXT("Launch EndAbility removes CanCancel.Dodge"), ASC->HasMatchingGameplayTag(TagCanCancelDodge));
			TestFalse(TEXT("Launch EndAbility clears DodgeCancelable"), LaunchAbility->GetTestDodgeCancelable());
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
