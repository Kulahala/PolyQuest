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
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Animation/Combat/AnimNotify_PlayerExecutionVictimStart.h"
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
		bool bEnableRootMotion = true,
		USkeleton* Skeleton = nullptr)
	{
		UObject* EffectiveOuter = Outer ? Outer : GetTransientPackage();
		UAnimMontage* Montage = NewObject<UAnimMontage>(EffectiveOuter);
		if (Skeleton)
		{
			Montage->SetSkeleton(Skeleton);
		}
		UAnimSequence* Seq = NewObject<UAnimSequence>(EffectiveOuter);
		if (Skeleton)
		{
			Seq->SetSkeleton(Skeleton);
		}
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

bool FExecutionVictimPresentationAutomationTest::RunTest(const FString& Parameters)
{
	AddExpectedErrorPlain(TEXT("SequencerDataModel"), EAutomationExpectedErrorFlags::Contains, -1);

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

	const FGameplayTag VictimStartTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Request.VictimStart")), false);
	const FGameplayTag FrontHitTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Hit")), false);
	const FGameplayTag BackstabHitTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Hit")), false);
	const FGameplayTag VictimLockedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Execution.VictimLocked")), false);
	const FGameplayTag PlayerLockedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Execution.PlayerLocked")), false);

	USkeleton* SharedSkeleton = NewObject<USkeleton>(GetTransientPackage());
	UAnimMontage* PlayerExecutionMontage = NewObject<UAnimMontage>(GetTransientPackage());
	UAnimMontage* EnemyFrontVictimMontage = ExecutionVictimPresentationAutomation::CreateValidRecoveryMontage(GetTransientPackage(), 2.5f, true, SharedSkeleton);
	UAnimMontage* EnemyBackstabVictimMontage = ExecutionVictimPresentationAutomation::CreateValidRecoveryMontage(GetTransientPackage(), 2.5f, true, SharedSkeleton);
	UAnimInstance* MockAnimInstance = NewObject<UAnimInstance>(Enemy->GetMesh());
	MockAnimInstance->InitializeMontageOnly();
	MockAnimInstance->CurrentSkeleton = SharedSkeleton;
	Enemy->GetMesh()->AnimScriptInstance = MockAnimInstance;

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

	auto GrantAndConfigureVictimAbility = [&](AEnemyCharacter* InEnemy, bool bConfigMontages = true, UAnimMontage* CustomFront = nullptr, UAnimMontage* CustomBackstab = nullptr) -> TPair<FGameplayAbilitySpecHandle, UEnemyVictimExecutionAbility*>
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
			if (bConfigMontages)
			{
				UAnimMontage* FrontToSet = CustomFront ? CustomFront : EnemyFrontVictimMontage;
				UAnimMontage* BackstabToSet = CustomBackstab ? CustomBackstab : EnemyBackstabVictimMontage;
				Instance->SetTestVictimMontages(FrontToSet, BackstabToSet);
			}
			else
			{
				Instance->SetTestVictimMontages(CustomFront, CustomBackstab);
			}
		}
		return { VictimHandle, Instance };
	};

	auto SetupFrontExec = [&](float InEnemyHealth = 100.0f, bool bConfigVictimMontage = true, UAnimMontage* CustomFront = nullptr, UAnimMontage* CustomBackstab = nullptr) -> TTuple<FGameplayAbilitySpecHandle, UPlayerFrontExecutionAbility*, FGameplayAbilitySpecHandle, UEnemyVictimExecutionAbility*>
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

		auto [VictimHandle, VictimAbility] = GrantAndConfigureVictimAbility(Enemy, bConfigVictimMontage, CustomFront, CustomBackstab);

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

	auto SetupBackstabExec = [&](float InEnemyHealth = 100.0f, bool bConfigVictimMontage = true, UAnimMontage* CustomFront = nullptr, UAnimMontage* CustomBackstab = nullptr) -> TTuple<FGameplayAbilitySpecHandle, UPlayerBackstabExecutionAbility*, FGameplayAbilitySpecHandle, UEnemyVictimExecutionAbility*>
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

		auto [VictimHandle, VictimAbility] = GrantAndConfigureVictimAbility(Enemy, bConfigVictimMontage, CustomFront, CustomBackstab);

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

	auto SetupStanceBreakBackstabExec = [&](float InEnemyHealth = 100.0f, bool bConfigVictimMontage = true, UAnimMontage* CustomFront = nullptr, UAnimMontage* CustomBackstab = nullptr) -> TTuple<FGameplayAbilitySpecHandle, UPlayerBackstabExecutionAbility*, FGameplayAbilitySpecHandle, UEnemyVictimExecutionAbility*>
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

		auto [VictimHandle, VictimAbility] = GrantAndConfigureVictimAbility(Enemy, bConfigVictimMontage, CustomFront, CustomBackstab);

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

		// Step A: Hit arrives first (stab: damage resolution only)
		FGameplayEventData HitPayload;
		HitPayload.EventTag = FrontHitTag;
		HitPayload.Instigator = Player;
		HitPayload.Target = Player;
		HitPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerHitEvent(HitPayload);

		TestTrue(TEXT("FrontAbility consumed damage on Hit"), FrontAbility->IsTestDamageEventConsumed());
		TestTrue(TEXT("Victim remains locked after Hit"), EnemyASC->HasMatchingGameplayTag(VictimLockedTag));
		TestFalse(TEXT("Victim presentation not yet started after Hit alone"), VictimAbility->IsTestVictimPresentationStarted());

		// Step B: VictimStart arrives second (draw blade: presentation handoff)
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

		// Step A: Hit arrives first
		FGameplayEventData HitPayload;
		HitPayload.EventTag = BackstabHitTag;
		HitPayload.Instigator = Player;
		HitPayload.Target = Player;
		HitPayload.OptionalObject = PlayerExecutionMontage;
		BackstabAbility->TestTriggerHitEvent(HitPayload);

		TestTrue(TEXT("Backstab consumed damage on Hit"), BackstabAbility->IsTestDamageEventConsumed());
		TestTrue(TEXT("Victim remains locked after Hit"), EnemyASC->HasMatchingGameplayTag(VictimLockedTag));
		TestFalse(TEXT("Victim presentation not yet started after Hit alone"), VictimAbility->IsTestVictimPresentationStarted());

		// Step B: VictimStart arrives second
		FGameplayEventData StartPayload;
		StartPayload.EventTag = VictimStartTag;
		StartPayload.Instigator = Player;
		StartPayload.Target = Player;
		StartPayload.OptionalObject = PlayerExecutionMontage;
		BackstabAbility->TestTriggerVictimStartEvent(StartPayload);

		TestTrue(TEXT("Backstab forwarded VictimStart"), BackstabAbility->IsTestVictimStartForwarded());
		TestTrue(TEXT("Victim presentation started"), VictimAbility->IsTestVictimPresentationStarted());

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

		// Step A: Hit arrives first (stab)
		FGameplayEventData HitPayload;
		HitPayload.EventTag = BackstabHitTag;
		HitPayload.Instigator = Player;
		HitPayload.Target = Player;
		HitPayload.OptionalObject = PlayerExecutionMontage;
		BackstabAbility->TestTriggerHitEvent(HitPayload);

		TestTrue(TEXT("Damage consumed on Backstab Hit in Sec 3B"), BackstabAbility->IsTestDamageEventConsumed());
		TestTrue(TEXT("Victim locked after Hit in Sec 3B"), EnemyASC->HasMatchingGameplayTag(VictimLockedTag));

		// Step B: Trigger VictimStart (draw blade)
		FGameplayEventData StartPayload;
		StartPayload.EventTag = VictimStartTag;
		StartPayload.Instigator = Player;
		StartPayload.Target = Player;
		StartPayload.OptionalObject = PlayerExecutionMontage;
		BackstabAbility->TestTriggerVictimStartEvent(StartPayload);

		TestTrue(TEXT("Backstab forwarded VictimStart"), BackstabAbility->IsTestVictimStartForwarded());
		TestTrue(TEXT("Victim presentation started"), VictimAbility->IsTestVictimPresentationStarted());

		BackstabAbility->TestEndAbility(false);

		// Complete recovery montage presentation
		VictimAbility->TestTriggerVictimMontageCompleted();

		// Contract: Upon OnCompleted & EndAbility for living enemy, Victim restores Poise to MaxPoise,
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
	// 4. Out-of-Order Gating: VictimStart before Hit is Rejected; Subsequent Hit & VictimStart Succeed
	// =========================================================================
	{
		Enemy->RestorePoiseToMax();
		Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

		auto [FrontHandle, FrontAbility, VictimHandle, VictimAbility] = SetupFrontExec(100.0f, true);
		if (!TestNotNull(TEXT("FrontAbility valid in Sec 4"), FrontAbility) ||
			!TestNotNull(TEXT("VictimAbility valid in Sec 4"), VictimAbility))
		{
			return false;
		}

		// Step A: Premature VictimStart arrives BEFORE Hit (Out-of-Order)
		FGameplayEventData PrematureStartPayload;
		PrematureStartPayload.EventTag = VictimStartTag;
		PrematureStartPayload.Instigator = Player;
		PrematureStartPayload.Target = Player;
		PrematureStartPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerVictimStartEvent(PrematureStartPayload);

		TestFalse(TEXT("Premature VictimStart before Hit is rejected on Player"), FrontAbility->IsTestVictimStartForwarded());
		TestFalse(TEXT("Victim presentation NOT started on premature VictimStart"), VictimAbility->IsTestVictimPresentationStarted());
		TestTrue(TEXT("Victim remains locked in execution"), EnemyASC->HasMatchingGameplayTag(VictimLockedTag));

		// Step B: Hit arrives normally (stab)
		FGameplayEventData HitPayload;
		HitPayload.EventTag = FrontHitTag;
		HitPayload.Instigator = Player;
		HitPayload.Target = Player;
		HitPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerHitEvent(HitPayload);

		TestTrue(TEXT("Damage consumed on Hit"), FrontAbility->IsTestDamageEventConsumed());
		TestFalse(TEXT("Victim presentation NOT auto-started by Hit alone"), VictimAbility->IsTestVictimPresentationStarted());
		TestTrue(TEXT("Victim remains locked after Hit"), EnemyASC->HasMatchingGameplayTag(VictimLockedTag));

		// Step C: Valid VictimStart arrives AFTER Hit (draw blade)
		FrontAbility->TestTriggerVictimStartEvent(PrematureStartPayload);

		TestTrue(TEXT("VictimStart forwarded after Hit resolved"), FrontAbility->IsTestVictimStartForwarded());
		TestTrue(TEXT("Victim presentation started on valid VictimStart"), VictimAbility->IsTestVictimPresentationStarted());

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

		// Step A: Hit arrives first (stab)
		FGameplayEventData HitPayload;
		HitPayload.EventTag = FrontHitTag;
		HitPayload.Instigator = Player;
		HitPayload.Target = Player;
		HitPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerHitEvent(HitPayload);

		TestTrue(TEXT("Damage consumed on Hit in Sec 5"), FrontAbility->IsTestDamageEventConsumed());

		// 5A: Wrong animation object on VictimStart (rejected despite Hit resolved)
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

		// Step B: Send valid VictimStart to complete presentation handoff
		FGameplayEventData ValidStartPayload;
		ValidStartPayload.EventTag = VictimStartTag;
		ValidStartPayload.Instigator = Player;
		ValidStartPayload.Target = Player;
		ValidStartPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerVictimStartEvent(ValidStartPayload);

		TestTrue(TEXT("Valid VictimStart accepted and forwarded"), FrontAbility->IsTestVictimStartForwarded());
		TestTrue(TEXT("Victim presentation started"), VictimAbility->IsTestVictimPresentationStarted());

		// End Player ability
		FrontAbility->TestEndAbility(false);
		TestFalse(TEXT("Player ability ended"), FrontAbility->IsActive());

		// 5C: Late VictimStart arrival AFTER Player ability has ended
		FrontAbility->TestTriggerVictimStartEvent(ValidStartPayload);
		TestFalse(TEXT("Late VictimStart rejected on inactive ability"), FrontAbility->IsActive());
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

		// Step A: Hit arrives first (stab)
		FGameplayEventData HitPayload;
		HitPayload.EventTag = FrontHitTag;
		HitPayload.Instigator = Player;
		HitPayload.Target = Player;
		HitPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerHitEvent(HitPayload);

		TestTrue(TEXT("Damage consumed on Hit in Sec 6A"), FrontAbility->IsTestDamageEventConsumed());

		// Step B: Send valid VictimStart: forward succeeds, victim gracefully handles missing montage
		FGameplayEventData StartPayload;
		StartPayload.EventTag = VictimStartTag;
		StartPayload.Instigator = Player;
		StartPayload.Target = Player;
		StartPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerVictimStartEvent(StartPayload);

		TestTrue(TEXT("VictimStart forwarded even when victim has no montage"), FrontAbility->IsTestVictimStartForwarded());
		TestNull(TEXT("No active montage started when unconfigured"), VictimAbility->GetTestActiveVictimMontage());
		TestFalse(TEXT("Victim ability safely ended without montage"), VictimAbility->IsActive());

		FrontAbility->TestEndAbility(false);
		CleanupExec(FrontHandle, VictimHandle);
	}

	// =========================================================================
	// 7. Natural Montage BlendOut & Completion Flow: BlendOut Retains State, Completed Ends Ability
	// =========================================================================
	{
		Enemy->RestorePoiseToMax();
		Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

		auto [FrontHandle, FrontAbility, VictimHandle, VictimAbility] = SetupFrontExec(100.0f, true);
		if (!TestNotNull(TEXT("FrontAbility valid in Sec 7"), FrontAbility) ||
			!TestNotNull(TEXT("VictimAbility valid in Sec 7"), VictimAbility))
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

		TestTrue(TEXT("Hit damage resolved in Sec 7"), FrontAbility->IsTestDamageEventConsumed());

		// Step B: VictimStart arrives second (draw blade)
		FGameplayEventData StartPayload;
		StartPayload.EventTag = VictimStartTag;
		StartPayload.Instigator = Player;
		StartPayload.Target = Player;
		StartPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerVictimStartEvent(StartPayload);

		TestTrue(TEXT("Victim presentation started in Sec 7"), VictimAbility->IsTestVictimPresentationStarted());

		// Step C: Trigger natural BlendOut (locks must be retained)
		VictimAbility->TestTriggerVictimMontageBlendOut();
		TestTrue(TEXT("VictimAbility remains active after BlendOut"), VictimAbility->IsActive());
		TestTrue(TEXT("Victim locked tag retained during BlendOut"), EnemyASC->HasMatchingGameplayTag(VictimLockedTag));

		// Step D: Trigger natural completion (ability ends cleanly)
		VictimAbility->TestTriggerVictimMontageCompleted();
		TestFalse(TEXT("VictimAbility ends on Completed"), VictimAbility->IsActive());
		TestFalse(TEXT("Victim locked tag cleared on Completed"), EnemyASC->HasMatchingGameplayTag(VictimLockedTag));

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
	// 9. Legitimate In-Place Montage (No Root Motion) Plays Presentation And Cleans Up Cleanly
	// =========================================================================
	{
		Enemy->RestorePoiseToMax();
		Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

		UAnimMontage* InPlaceMontage = ExecutionVictimPresentationAutomation::CreateValidRecoveryMontage(GetTransientPackage(), 2.0f, false);
		TestFalse(TEXT("InPlaceMontage has no root motion"), InPlaceMontage->HasRootMotion());

		auto [FrontHandle, FrontAbility, VictimHandle, VictimAbility] = SetupFrontExec(100.0f, false, InPlaceMontage, nullptr);
		if (!TestNotNull(TEXT("FrontAbility valid in Sec 9"), FrontAbility) ||
			!TestNotNull(TEXT("VictimAbility valid in Sec 9"), VictimAbility))
		{
			return false;
		}

		VictimAbility->SetTestBoundAnimInstance(MockAnimInstance);
		VictimAbility->SetTestBypassMontageActiveCheck(true);

		bool bLaunchDispatched = false;
		const FGameplayTag LaunchEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Enemy.Launch")), false);
		FDelegateHandle LaunchHandle = EnemyASC->GenericGameplayEventCallbacks.FindOrAdd(LaunchEventTag)
			.AddLambda([&bLaunchDispatched](const FGameplayEventData* InPayload) { bLaunchDispatched = true; });

		// Step A: Hit arrives first
		FGameplayEventData HitPayload;
		HitPayload.EventTag = FrontHitTag;
		HitPayload.Instigator = Player;
		HitPayload.Target = Player;
		HitPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerHitEvent(HitPayload);

		TestTrue(TEXT("Hit damage resolved in Sec 9"), FrontAbility->IsTestDamageEventConsumed());

		// Step B: VictimStart arrives second
		FGameplayEventData StartPayload;
		StartPayload.EventTag = VictimStartTag;
		StartPayload.Instigator = Player;
		StartPayload.Target = Player;
		StartPayload.OptionalObject = PlayerExecutionMontage;
		FrontAbility->TestTriggerVictimStartEvent(StartPayload);

		// Legitimate in-place montage must start presentation without being rejected for lacking root motion
		TestTrue(TEXT("In-place victim montage started presentation successfully"), VictimAbility->IsTestVictimPresentationStarted());
		TestTrue(TEXT("Victim ability active during in-place recovery"), VictimAbility->IsActive());
		TestFalse(TEXT("Launch reaction NEVER dispatched on in-place recovery"), bLaunchDispatched);

		// Step C: Montage completion finishes ability
		VictimAbility->TestTriggerVictimMontageCompleted();
		TestFalse(TEXT("Victim ability cleanly ended after in-place montage completed"), VictimAbility->IsActive());
		TestEqual(TEXT("Enemy movement mode restored to Walking"), Enemy->GetCharacterMovement()->MovementMode, MOVE_Walking);

		FrontAbility->TestEndAbility(false);
		EnemyASC->GenericGameplayEventCallbacks.FindOrAdd(LaunchEventTag).Remove(LaunchHandle);
		CleanupExec(FrontHandle, VictimHandle);
	}

	// =========================================================================
	// 10. Default Entry / Time 0 Full Presentation Plays Without Section Manipulation
	// =========================================================================
	{
		Enemy->RestorePoiseToMax();
		Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

		const FName Sec1(TEXT("Default"));
		const FName Sec2(TEXT("RecoveryLoop"));

		UAnimMontage* DefaultMontage = ExecutionVictimPresentationAutomation::CreateValidRecoveryMontage(GetTransientPackage(), 2.5f, true);
		DefaultMontage->CompositeSections.Empty();

		FCompositeSection S1;
		S1.SectionName = Sec1;
		S1.NextSectionName = Sec2;
		S1.SetTime(0.0f);

		FCompositeSection S2;
		S2.SectionName = Sec2;
		S2.NextSectionName = NAME_None;
		S2.SetTime(1.0f);

		DefaultMontage->CompositeSections.Add(S1);
		DefaultMontage->CompositeSections.Add(S2);

		TestEqual(TEXT("Shared asset S1 links to S2"), DefaultMontage->CompositeSections[0].NextSectionName, Sec2);

		auto [FrontHandle, FrontAbility, VictimHandle, VictimAbility] = SetupFrontExec(100.0f, false, DefaultMontage, nullptr);
		if (TestNotNull(TEXT("FrontAbility valid in Sec 10"), FrontAbility) && TestNotNull(TEXT("VictimAbility valid in Sec 10"), VictimAbility))
		{
			VictimAbility->SetTestBoundAnimInstance(MockAnimInstance);
			VictimAbility->SetTestBypassMontageActiveCheck(true);

			// Step A: Hit arrives first
			FGameplayEventData HitPayload;
			HitPayload.EventTag = FrontHitTag;
			HitPayload.Instigator = Player;
			HitPayload.Target = Player;
			HitPayload.OptionalObject = PlayerExecutionMontage;
			FrontAbility->TestTriggerHitEvent(HitPayload);

			// Step B: VictimStart arrives second
			FGameplayEventData StartPayload;
			StartPayload.EventTag = VictimStartTag;
			StartPayload.Instigator = Player;
			StartPayload.Target = Player;
			StartPayload.OptionalObject = PlayerExecutionMontage;
			FrontAbility->TestTriggerVictimStartEvent(StartPayload);

			TestEqual(TEXT("Shared asset S1 still links to S2 without mutation"), DefaultMontage->CompositeSections[0].NextSectionName, Sec2);
			TestTrue(TEXT("Victim presentation started from default entry"), VictimAbility->IsTestVictimPresentationStarted());

			FrontAbility->TestEndAbility(false);
			VictimAbility->TestEndAbility(false);
			CleanupExec(FrontHandle, VictimHandle);
		}
	}

	// =========================================================================
	// 11. Direction Snapshot Strictness: Missing Target Direction Fails Without Fallback
	// =========================================================================
	{
		Enemy->RestorePoiseToMax();
		Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

		// Case A: Front execution with Front=None, Backstab=Valid -> Handoff must FAIL (no fallback to Backstab)
		{
			auto [FrontHandle, FrontAbility, VictimHandle, VictimAbility] = SetupFrontExec(100.0f, false, nullptr, EnemyBackstabVictimMontage);
			if (TestNotNull(TEXT("FrontAbility valid in Sec 11A"), FrontAbility) && TestNotNull(TEXT("VictimAbility valid in Sec 11A"), VictimAbility))
			{
				VictimAbility->SetTestBoundAnimInstance(MockAnimInstance);
				VictimAbility->SetTestBypassMontageActiveCheck(true);

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

				TestNull(TEXT("Pending victim montage is null when Front is not configured"), VictimAbility->GetTestPendingVictimMontage());
				TestFalse(TEXT("Victim presentation did NOT start due to null Front montage"), VictimAbility->IsTestVictimPresentationStarted());
				TestFalse(TEXT("Victim ability ended safely after failed handoff"), VictimAbility->IsActive());
				TestFalse(TEXT("Launch reaction NEVER dispatched on failed direction handoff"), bLaunchDispatched);

				FrontAbility->TestEndAbility(false);
				EnemyASC->GenericGameplayEventCallbacks.FindOrAdd(LaunchEventTag).Remove(LaunchHandle);
				CleanupExec(FrontHandle, VictimHandle);
			}
		}

		// Case B: Backstab execution with Backstab=None, Front=Valid -> Handoff must FAIL (no fallback to Front)
		{
			Enemy->RestorePoiseToMax();
			Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

			auto [BackstabHandle, BackstabAbility, VictimHandle, VictimAbility] = SetupBackstabExec(100.0f, false, EnemyFrontVictimMontage, nullptr);
			if (TestNotNull(TEXT("BackstabAbility valid in Sec 11B"), BackstabAbility) && TestNotNull(TEXT("VictimAbility valid in Sec 11B"), VictimAbility))
			{
				VictimAbility->SetTestBoundAnimInstance(MockAnimInstance);
				VictimAbility->SetTestBypassMontageActiveCheck(true);

				bool bLaunchDispatched = false;
				const FGameplayTag LaunchEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Enemy.Launch")), false);
				FDelegateHandle LaunchHandle = EnemyASC->GenericGameplayEventCallbacks.FindOrAdd(LaunchEventTag)
					.AddLambda([&bLaunchDispatched](const FGameplayEventData*) { bLaunchDispatched = true; });

				FGameplayEventData HitPayload;
				HitPayload.EventTag = BackstabHitTag;
				HitPayload.Instigator = Player;
				HitPayload.Target = Player;
				HitPayload.OptionalObject = PlayerExecutionMontage;
				BackstabAbility->TestTriggerHitEvent(HitPayload);

				FGameplayEventData StartPayload;
				StartPayload.EventTag = VictimStartTag;
				StartPayload.Instigator = Player;
				StartPayload.Target = Player;
				StartPayload.OptionalObject = PlayerExecutionMontage;
				BackstabAbility->TestTriggerVictimStartEvent(StartPayload);

				TestNull(TEXT("Pending victim montage is null when Backstab is not configured"), VictimAbility->GetTestPendingVictimMontage());
				TestFalse(TEXT("Victim presentation did NOT start due to null Backstab montage"), VictimAbility->IsTestVictimPresentationStarted());
				TestFalse(TEXT("Victim ability ended safely"), VictimAbility->IsActive());
				TestFalse(TEXT("Launch reaction NEVER dispatched"), bLaunchDispatched);

				BackstabAbility->TestEndAbility(false);
				EnemyASC->GenericGameplayEventCallbacks.FindOrAdd(LaunchEventTag).Remove(LaunchHandle);
				CleanupExec(BackstabHandle, VictimHandle);
			}
		}
	}

	// =========================================================================
	// 12. Mutation of Montages After Activation Does Not Mutate Snapshot
	// =========================================================================
	{
		Enemy->RestorePoiseToMax();
		Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

		UAnimMontage* AltMontage = ExecutionVictimPresentationAutomation::CreateValidRecoveryMontage(GetTransientPackage(), 3.0f, true);

		auto [FrontHandle, FrontAbility, VictimHandle, VictimAbility] = SetupFrontExec(100.0f, false, EnemyFrontVictimMontage, EnemyBackstabVictimMontage);
		if (TestNotNull(TEXT("FrontAbility valid in Sec 12"), FrontAbility) && TestNotNull(TEXT("VictimAbility valid in Sec 12"), VictimAbility))
		{
			VictimAbility->SetTestBoundAnimInstance(MockAnimInstance);
			VictimAbility->SetTestBypassMontageActiveCheck(true);

			TestEqual(TEXT("Pending montage is snapshot to EnemyFrontVictimMontage at activation"),
				VictimAbility->GetTestPendingVictimMontage(), EnemyFrontVictimMontage);

			// Mutate montages after activation
			VictimAbility->SetTestVictimMontages(AltMontage, nullptr);

			// Pending snapshot must remain unchanged!
			TestEqual(TEXT("Pending montage snapshot is NOT mutated by post-activation configuration changes"),
				VictimAbility->GetTestPendingVictimMontage(), EnemyFrontVictimMontage);

			// Hit -> VictimStart
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

			// Plays the snapshotted EnemyFrontVictimMontage, NOT AltMontage
			TestEqual(TEXT("Active victim montage is the snapshotted montage"),
				VictimAbility->GetTestActiveVictimMontage(), EnemyFrontVictimMontage);

			FrontAbility->TestEndAbility(false);
			VictimAbility->TestEndAbility(false);
			CleanupExec(FrontHandle, VictimHandle);
		}
	}

	// =========================================================================
	// 13. Synchronous Cancellation During Startup Movement Mode and ReadyForActivation
	// =========================================================================
	{
		Enemy->RestorePoiseToMax();
		Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

		// Case A: Synchronous cancel during movement mode change
		{
			auto [FrontHandle, FrontAbility, VictimHandle, VictimAbility] = SetupFrontExec(100.0f, true);
			if (TestNotNull(TEXT("FrontAbility valid in Sec 13A"), FrontAbility) && TestNotNull(TEXT("VictimAbility valid in Sec 13A"), VictimAbility))
			{
				VictimAbility->SetTestBoundAnimInstance(MockAnimInstance);
				VictimAbility->SetTestBypassMontageActiveCheck(true);
				VictimAbility->SetTestCancelDuringStartupMovementMode(true);

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

				TestFalse(TEXT("VictimAbility ended synchronously during movement mode startup"), VictimAbility->IsActive());
				TestNull(TEXT("No active montage task leaked"), VictimAbility->GetTestVictimMontageTask());

				FrontAbility->TestEndAbility(false);
				CleanupExec(FrontHandle, VictimHandle);
			}
		}

		// Case B: Synchronous cancel during ReadyForActivation
		{
			Enemy->RestorePoiseToMax();
			Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

			auto [FrontHandle, FrontAbility, VictimHandle, VictimAbility] = SetupFrontExec(100.0f, true);
			if (TestNotNull(TEXT("FrontAbility valid in Sec 13B"), FrontAbility) && TestNotNull(TEXT("VictimAbility valid in Sec 13B"), VictimAbility))
			{
				VictimAbility->SetTestBoundAnimInstance(MockAnimInstance);
				VictimAbility->SetTestBypassMontageActiveCheck(true);
				VictimAbility->SetTestCancelDuringStartupReadyForActivation(true);

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

				TestFalse(TEXT("VictimAbility ended synchronously during ReadyForActivation"), VictimAbility->IsActive());
				TestNull(TEXT("Active victim montage cleared after synchronous startup cancel"), VictimAbility->GetTestActiveVictimMontage());

				FrontAbility->TestEndAbility(false);
				CleanupExec(FrontHandle, VictimHandle);
			}
		}

		// Case C: Real engine play call via ReadyForActivation: synchronous cancel during OnMontageStarted
		// and reentrant play of the same montage asset.
		// Verifies:
		// 1. Startup instance is captured and stopped cleanly with 0.0s blend.
		// 2. Reentrant new instance of the same asset is NOT stopped and continues playing!
		// 3. Ability ends cleanly with no leaked instance or task state.
		{
			Enemy->RestorePoiseToMax();
			Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

			auto [FrontHandle, FrontAbility, VictimHandle, VictimAbility] = SetupFrontExec(100.0f, true, EnemyFrontVictimMontage, EnemyBackstabVictimMontage);
			if (TestNotNull(TEXT("FrontAbility valid in Sec 13C"), FrontAbility) && TestNotNull(TEXT("VictimAbility valid in Sec 13C"), VictimAbility))
			{
				VictimAbility->SetTestBoundAnimInstance(MockAnimInstance);
				VictimAbility->SetTestBypassMontageActiveCheck(false);

				FGameplayEventData HitPayload;
				HitPayload.EventTag = FrontHitTag;
				HitPayload.Instigator = Player;
				HitPayload.Target = Player;
				HitPayload.OptionalObject = PlayerExecutionMontage;
				FrontAbility->TestTriggerHitEvent(HitPayload);

				int32 OldStartupInstanceID = INDEX_NONE;
				FAnimMontageInstance* ReentrantNewInstance = nullptr;
				int32 ReentrantNewInstanceID = INDEX_NONE;
				bool bStartupCallbackFired = false;

				VictimAbility->SetTestOnMontageStartedHook(
					[&](UAnimMontage* StartedMontage)
					{
						if (StartedMontage == EnemyFrontVictimMontage && !bStartupCallbackFired)
						{
							bStartupCallbackFired = true;
							OldStartupInstanceID = VictimAbility->GetTestStartupVictimMontageInstanceID();

							// Synchronously cancel VictimAbility during instance startup
							VictimAbility->CancelAbility(VictimAbility->GetCurrentAbilitySpecHandle(),
								VictimAbility->GetCurrentActorInfo(),
								VictimAbility->GetCurrentActivationInfo(),
								true);

							// Immediately re-entrantly play the same montage asset (simulating external system / new ability)
							const float PlayResult = MockAnimInstance->Montage_Play(EnemyFrontVictimMontage);
							if (PlayResult > 0.0f)
							{
								ReentrantNewInstance = MockAnimInstance->GetActiveInstanceForMontage(EnemyFrontVictimMontage);
								if (ReentrantNewInstance)
								{
									ReentrantNewInstanceID = ReentrantNewInstance->GetInstanceID();
								}
							}
						}
					});

				FGameplayEventData StartPayload;
				StartPayload.EventTag = VictimStartTag;
				StartPayload.Instigator = Player;
				StartPayload.Target = Player;
				StartPayload.OptionalObject = PlayerExecutionMontage;
				FrontAbility->TestTriggerVictimStartEvent(StartPayload);

				TestTrue(TEXT("Startup OnMontageStarted callback fired during ReadyForActivation"), bStartupCallbackFired);
				TestNotEqual(TEXT("Captured valid OldStartupInstanceID"), OldStartupInstanceID, (int32)INDEX_NONE);
				TestNotNull(TEXT("ReentrantNewInstance created successfully"), ReentrantNewInstance);
				TestNotEqual(TEXT("ReentrantNewInstance has distinct ID from old instance"), ReentrantNewInstanceID, OldStartupInstanceID);

				// Assert: Old instance was stopped with 0.0s blend
				FAnimMontageInstance* OldInstance = MockAnimInstance->GetMontageInstanceForID(OldStartupInstanceID);
				if (TestNotNull(TEXT("OldInstance found by ID"), OldInstance))
				{
					TestTrue(TEXT("OldInstance stopped cleanly after cancel"), OldInstance->IsStopped());
					TestTrue(TEXT("OldInstance blend desired value is zero"), FMath::IsNearlyZero(OldInstance->GetBlend().GetDesiredValue()));
				}

				// Assert: Reentrant new instance of same montage asset continues playing!
				TestNotNull(TEXT("Reentrant new instance still exists"), ReentrantNewInstance);
				if (ReentrantNewInstance)
				{
					TestFalse(TEXT("Reentrant new instance was NOT stopped by old ability cleanup!"), ReentrantNewInstance->IsStopped());
					TestTrue(TEXT("Reentrant new instance is still playing"), ReentrantNewInstance->IsPlaying());
				}

				// Assert: VictimAbility state is fully cleaned up
				TestFalse(TEXT("VictimAbility ended synchronously"), VictimAbility->IsActive());
				TestFalse(TEXT("Non-lethal recovery inactive after cancel"), VictimAbility->IsTestNonLethalRecoveryActive());
				TestNull(TEXT("Active victim montage cleared"), VictimAbility->GetTestActiveVictimMontage());
				TestEqual(TEXT("Active instance ID reset to INDEX_NONE"), VictimAbility->GetTestActiveVictimMontageInstanceID(), (int32)INDEX_NONE);
				TestEqual(TEXT("Startup instance ID reset to INDEX_NONE"), VictimAbility->GetTestStartupVictimMontageInstanceID(), (int32)INDEX_NONE);

				// Cleanup instances
				for (FAnimMontageInstance* Inst : MockAnimInstance->MontageInstances)
				{
					if (Inst)
					{
						Inst->Terminate();
						delete Inst;
					}
				}
				MockAnimInstance->MontageInstances.Empty();

				FrontAbility->TestEndAbility(false);
				CleanupExec(FrontHandle, VictimHandle);
			}
		}

		// Case D: Real Montage_Play stops the old group before allocating the victim instance.
		// Cancellation originates from that old instance's native blend-out delegate.
		{
			Enemy->RestorePoiseToMax();
			Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

			auto [FrontHandle, FrontAbility, VictimHandle, VictimAbility] = SetupFrontExec(100.0f, true);
			if (TestNotNull(TEXT("FrontAbility valid in Sec 13D"), FrontAbility) &&
				TestNotNull(TEXT("VictimAbility valid in Sec 13D"), VictimAbility))
			{
				VictimAbility->SetTestBoundAnimInstance(MockAnimInstance);
				VictimAbility->SetTestBypassMontageActiveCheck(false);
				const bool bOriginalCanWalkOffLedges = Enemy->GetCharacterMovement()->bCanWalkOffLedges;

				FGameplayEventData Payload;
				Payload.EventTag = FrontHitTag;
				Payload.Instigator = Player;
				Payload.Target = Player;
				Payload.OptionalObject = PlayerExecutionMontage;
				PlayerASC->HandleGameplayEvent(FrontHitTag, &Payload);

				// Different asset, same group. Start it after pairing so activation cleanup cannot consume the callback.
				TestEqual(TEXT("13D: Old and incoming montages share the same group"),
					EnemyBackstabVictimMontage->GetGroupName(), EnemyFrontVictimMontage->GetGroupName());
				const float OldPlayResult = MockAnimInstance->Montage_Play(EnemyBackstabVictimMontage);
				FAnimMontageInstance* OldInstance = MockAnimInstance->GetActiveInstanceForMontage(EnemyBackstabVictimMontage);
				if (TestTrue(TEXT("13D: Old montage really starts"), OldPlayResult > 0.0f) &&
					TestNotNull(TEXT("13D: Old montage has a real instance"), OldInstance))
				{
					bool bOldStopCallbackFired = false;
					bool bCancelledBeforeInstanceBirth = false;
					bool bNewbornObservedAfterCancellation = false;
					int32 NewbornInstanceID = INDEX_NONE;
					const int32 OldInstanceID = OldInstance->GetInstanceID();
					const FVector ExternalVelocity(113.0f, 37.0f, 0.0f);

					OldInstance->OnMontageBlendingOutStarted.BindLambda(
						[&](UAnimMontage* StoppedMontage, bool bInterrupted)
						{
							bOldStopCallbackFired = true;
							TestTrue(TEXT("13D: New playback interrupted the old montage"), bInterrupted);
							TestEqual(TEXT("13D: Callback belongs to the old asset"), StoppedMontage, EnemyBackstabVictimMontage);
							bCancelledBeforeInstanceBirth = VictimAbility->IsActive() &&
								VictimAbility->GetTestStartupVictimMontageInstanceID() == INDEX_NONE &&
								VictimAbility->GetTestActiveVictimMontageInstanceID() == INDEX_NONE &&
								VictimAbility->GetTestPendingStartupVictimMontage() == EnemyFrontVictimMontage &&
								MockAnimInstance->GetActiveInstanceForMontage(EnemyFrontVictimMontage) == nullptr;
							VictimAbility->CancelAbility(VictimAbility->GetCurrentAbilitySpecHandle(),
								VictimAbility->GetCurrentActorInfo(), VictimAbility->GetCurrentActivationInfo(), true);
							// Simulate an external movement owner after cancellation has finished.
							Enemy->GetCharacterMovement()->Velocity = ExternalVelocity;
						});
					VictimAbility->SetTestOnMontageStartedHook([&](UAnimMontage* StartedMontage)
					{
						if (StartedMontage == EnemyFrontVictimMontage)
						{
							NewbornInstanceID = VictimAbility->GetTestStartupVictimMontageInstanceID();
							bNewbornObservedAfterCancellation = bOldStopCallbackFired && !VictimAbility->IsActive();
						}
					});

					Payload.EventTag = VictimStartTag;
					PlayerASC->HandleGameplayEvent(VictimStartTag, &Payload);

					TestTrue(TEXT("13D: Native old-montage stop callback ran inside the play call"), bOldStopCallbackFired);
					TestTrue(TEXT("13D: Cancellation happened before the incoming instance existed"), bCancelledBeforeInstanceBirth);
					TestTrue(TEXT("13D: Engine created the victim instance after cancellation"), bNewbornObservedAfterCancellation);
					TestNotEqual(TEXT("13D: Captured the real newborn instance ID"), NewbornInstanceID, static_cast<int32>(INDEX_NONE));
					FAnimMontageInstance* NewbornInstance = MockAnimInstance->GetMontageInstanceForID(NewbornInstanceID);
					if (TestNotNull(TEXT("13D: Newborn instance can be resolved by ID"), NewbornInstance))
					{
						TestTrue(TEXT("13D: Late newborn was stopped"), NewbornInstance->IsStopped());
						TestTrue(TEXT("13D: Late newborn has zero blend-out duration"), FMath::IsNearlyZero(NewbornInstance->GetBlend().GetBlendTime()));
						TestTrue(TEXT("13D: Late newborn has zero desired weight"), FMath::IsNearlyZero(NewbornInstance->GetBlend().GetDesiredValue()));
						TestTrue(TEXT("13D: Late newborn no longer owns root motion"), MockAnimInstance->GetRootMotionMontageInstance() != NewbornInstance);
					}
					TestFalse(TEXT("13D: Ability stays ended after ReadyForActivation returns"), VictimAbility->IsActive());
					TestNull(TEXT("13D: No montage task restored after cancellation"), VictimAbility->GetTestVictimMontageTask());
					TestNull(TEXT("13D: Active montage cleared"), VictimAbility->GetTestActiveVictimMontage());
					TestNull(TEXT("13D: Pending victim montage cleared"), VictimAbility->GetTestPendingVictimMontage());
					TestNull(TEXT("13D: Pending startup montage cleared"), VictimAbility->GetTestPendingStartupVictimMontage());
					TestEqual(TEXT("13D: Active ID cleared"), VictimAbility->GetTestActiveVictimMontageInstanceID(), static_cast<int32>(INDEX_NONE));
					TestEqual(TEXT("13D: Startup ID cleared"), VictimAbility->GetTestStartupVictimMontageInstanceID(), static_cast<int32>(INDEX_NONE));
					TestFalse(TEXT("13D: No recovery state survived"), VictimAbility->IsTestNonLethalRecoveryActive());
					TestFalse(TEXT("13D: Cancelled recovery cannot retain lock"), VictimAbility->IsNonLethalRecoveryFrom(Player));
					TestFalse(TEXT("13D: VictimOwned lock removed"), EnemyASC->HasMatchingGameplayTag(VictimLockedTag));
					TestTrue(TEXT("13D: Original ledge policy restored"), Enemy->GetCharacterMovement()->bCanWalkOffLedges == bOriginalCanWalkOffLedges);
					TestTrue(TEXT("13D: Late cleanup preserves external owner's velocity"), Enemy->GetCharacterMovement()->Velocity.Equals(ExternalVelocity));

					// Clear stack captures before ending any remaining fixtures.
					MockAnimInstance->DispatchQueuedAnimEvents();
					if (FAnimMontageInstance* RemainingOldInstance = MockAnimInstance->GetMontageInstanceForID(OldInstanceID))
					{
						RemainingOldInstance->OnMontageBlendingOutStarted.Unbind();
					}
					VictimAbility->SetTestOnMontageStartedHook({});
				}

				FrontAbility->TestEndAbility(false);
				CleanupExec(FrontHandle, VictimHandle);
				for (FAnimMontageInstance* Instance : MockAnimInstance->MontageInstances)
				{
					if (Instance)
					{
						Instance->Terminate();
						delete Instance;
					}
				}
				MockAnimInstance->MontageInstances.Empty();
			}
		}
	}

	// =========================================================================
	// 14. Accurate Instance-Level Stop: Old Fading Out Instance vs New Active Instance
	// =========================================================================
	{
		Enemy->RestorePoiseToMax();
		Enemy->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

		auto [FrontHandle, FrontAbility, VictimHandle, VictimAbility] = SetupFrontExec(100.0f, true);
		if (TestNotNull(TEXT("VictimAbility valid in Sec 14"), VictimAbility))
		{
			UAnimMontage* SharedMontage = EnemyFrontVictimMontage;
			VictimAbility->SetTestBoundAnimInstance(MockAnimInstance);

			// Create two real FAnimMontageInstance objects for the same SharedMontage:
			// OldInstance: represents the instance owned by this ability that needs stopping.
			// UnrelatedNewInstance: represents an active instance (e.g. newly started by another ability/system).
			FAnimMontageInstance* OldInstance = new FAnimMontageInstance(MockAnimInstance);
			OldInstance->Initialize(SharedMontage);
			OldInstance->Play();

			FAnimMontageInstance* UnrelatedNewInstance = new FAnimMontageInstance(MockAnimInstance);
			UnrelatedNewInstance->Initialize(SharedMontage);
			UnrelatedNewInstance->Play();

			MockAnimInstance->MontageInstances.Add(OldInstance);
			MockAnimInstance->MontageInstances.Add(UnrelatedNewInstance);

			TestTrue(TEXT("OldInstance is playing"), OldInstance->IsPlaying());
			TestFalse(TEXT("OldInstance is not stopped"), OldInstance->IsStopped());
			TestTrue(TEXT("UnrelatedNewInstance is playing"), UnrelatedNewInstance->IsPlaying());
			TestFalse(TEXT("UnrelatedNewInstance is not stopped"), UnrelatedNewInstance->IsStopped());

			const int32 OldInstanceID = OldInstance->GetInstanceID();
			const int32 UnrelatedNewInstanceID = UnrelatedNewInstance->GetInstanceID();
			TestNotEqual(TEXT("Instances have distinct IDs"), OldInstanceID, UnrelatedNewInstanceID);

			// Test A: Calling Stop with INDEX_NONE must NOT stop any running instance (guards against stopping unrelated instances)
			VictimAbility->SetTestActiveVictimMontage(SharedMontage);
			VictimAbility->SetTestActiveVictimMontageInstanceID(INDEX_NONE);
			VictimAbility->TestStopVictimMontagePresentation(false);

			TestFalse(TEXT("OldInstance was NOT stopped when ID was INDEX_NONE"), OldInstance->IsStopped());
			TestFalse(TEXT("UnrelatedNewInstance was NOT stopped when ID was INDEX_NONE"), UnrelatedNewInstance->IsStopped());
			TestNull(TEXT("ActiveVictimMontage cleared safely with INDEX_NONE"), VictimAbility->GetTestActiveVictimMontage());
			TestEqual(TEXT("Instance ID remains INDEX_NONE"), VictimAbility->GetTestActiveVictimMontageInstanceID(), (int32)INDEX_NONE);

			// Test B: Calling Stop with OldInstanceID must stop OldInstance specifically, leaving UnrelatedNewInstance untouched
			VictimAbility->SetTestActiveVictimMontage(SharedMontage);
			VictimAbility->SetTestActiveVictimMontageInstanceID(OldInstanceID);

			VictimAbility->TestStopVictimMontagePresentation(false);

			TestTrue(TEXT("OldInstance is stopped after StopVictimMontagePresentation"), OldInstance->IsStopped());
			TestFalse(TEXT("UnrelatedNewInstance remains playing and untouched!"), UnrelatedNewInstance->IsStopped());
			TestTrue(TEXT("UnrelatedNewInstance is still playing"), UnrelatedNewInstance->IsPlaying());
			TestNull(TEXT("ActiveVictimMontage cleared after StopVictimMontagePresentation"), VictimAbility->GetTestActiveVictimMontage());
			TestEqual(TEXT("ActiveVictimMontageInstanceID reset to INDEX_NONE"), VictimAbility->GetTestActiveVictimMontageInstanceID(), (int32)INDEX_NONE);

			// Cleanup instances and array
			OldInstance->Terminate();
			delete OldInstance;
			UnrelatedNewInstance->Terminate();
			delete UnrelatedNewInstance;
			MockAnimInstance->MontageInstances.Empty();

			FrontAbility->TestEndAbility(false);
			VictimAbility->TestEndAbility(false);
			CleanupExec(FrontHandle, VictimHandle);
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
