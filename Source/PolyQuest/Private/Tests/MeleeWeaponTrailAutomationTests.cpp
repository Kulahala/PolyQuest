#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Tasks/AbilityTask_MeleeTraceWindow.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Character/Player/PlayerCharacter.h"
#include "Combat/Equipment/WeaponEquipmentComponent.h"
#include "Combat/Melee/MeleeTraceSourceComponent.h"
#include "Combat/Melee/MeleeWeaponTrailComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "NiagaraSystem.h"
#include "Tests/CombatAutomationFixture.h"
#include "Tests/TestMeleeTrailAbility.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMeleeWeaponTrailAutomationTest,
	"PolyQuest.Melee.WeaponTrail",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	struct FMeleeWeaponTrailWorldCleanup
	{
		UWorld* World = nullptr;
		~FMeleeWeaponTrailWorldCleanup()
		{
			if (World)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
		}
	};
}

bool FMeleeWeaponTrailAutomationTest::RunTest(const FString& Parameters)
{
	if (!TestNotNull(TEXT("Engine is available for melee weapon trail automation"), GEngine))
	{
		return false;
	}

	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("MeleeWeaponTrailTestWorld"));
	WorldContext.SetCurrentWorld(World);
	FMeleeWeaponTrailWorldCleanup Cleanup{ World };
	if (!TestNotNull(TEXT("Test World created successfully"), World))
	{
		return false;
	}

	FURL WorldURL;
	World->InitializeActorsForPlay(WorldURL);
	World->BeginPlay();

	// -------------------------------------------------------------------------
	// SECTION 1: Default Subobject Setup & Attachment Contract
	// -------------------------------------------------------------------------
	APlayerCharacter* Player = FCombatAutomationFixture::SpawnPlayer(World, FTransform::Identity);
	if (!TestNotNull(TEXT("Player spawned successfully"), Player))
	{
		return false;
	}

	UMeleeWeaponTrailComponent* PlayerTrail = Player->FindComponentByClass<UMeleeWeaponTrailComponent>();
	if (!TestNotNull(TEXT("Player owns UMeleeWeaponTrailComponent"), PlayerTrail))
	{
		return false;
	}

	TestTrue(TEXT("Trail component is attached to RootComponent"), PlayerTrail->GetAttachParent() == Player->GetRootComponent());
	TestFalse(TEXT("Trail component bAutoActivate is false"), PlayerTrail->bAutoActivate);
	TestFalse(TEXT("Trail component bAutoManageAttachment is false"), PlayerTrail->bAutoManageAttachment);
	TestFalse(TEXT("Trail component bAutoDestroy is false"), PlayerTrail->GetAutoDestroy());
	TestFalse(TEXT("Trail component starts inactive"), PlayerTrail->IsActive());
	TestNull(TEXT("Trail component starts with no active requester"), PlayerTrail->GetTestActiveRequester());

	// -------------------------------------------------------------------------
	// SECTION 2: Active Trail Lifecycle with Real ASC-Hosted Task
	// -------------------------------------------------------------------------
	PlayerTrail->SetTestTrackingEnabled(true);

	FVector ExpectedInitialBase = FVector::ZeroVector;
	FVector ExpectedInitialTip = FVector::ZeroVector;
	TestTrue(TEXT("Player trace source provides blade endpoints"), Player->GetMeleeTraceSource()->TryGetBladeEndpoints(ExpectedInitialBase, ExpectedInitialTip));

	UAbilitySystemComponent* PlayerASC = Player->GetAbilitySystemComponent();
	if (!TestNotNull(TEXT("Player ASC is valid"), PlayerASC))
	{
		return false;
	}

	FGameplayAbilitySpecHandle AbilityHandle = PlayerASC->GiveAbility(
		FGameplayAbilitySpec(UTestMeleeTrailAbility::StaticClass(), 1, INDEX_NONE, Player));
	TestTrue(TEXT("UTestMeleeTrailAbility given to Player ASC"), AbilityHandle.IsValid());

	const bool bActivated = PlayerASC->TryActivateAbility(AbilityHandle);
	TestTrue(TEXT("UTestMeleeTrailAbility activated successfully"), bActivated);

	FGameplayAbilitySpec* AbilitySpec = PlayerASC->FindAbilitySpecFromHandle(AbilityHandle);
	UTestMeleeTrailAbility* ActiveAbility = AbilitySpec ? Cast<UTestMeleeTrailAbility>(AbilitySpec->GetPrimaryInstance()) : nullptr;
	if (!TestNotNull(TEXT("Active ability instance found"), ActiveAbility))
	{
		return false;
	}

	UAbilityTask_MeleeTraceWindow* ActiveTask = ActiveAbility->GetActiveTraceWindowTask();
	if (!TestNotNull(TEXT("Active trace window task exists"), ActiveTask))
	{
		return false;
	}

	TestTrue(TEXT("Trail component activated upon task start"), PlayerTrail->IsTestTrackingActive());
	TestTrue(TEXT("Trail component active requester is active task"), PlayerTrail->GetTestActiveRequester() == static_cast<const UObject*>(ActiveTask));
	TestTrue(TEXT("Initial BladeBase matches captured sample"), PlayerTrail->GetTestLastBladeBase().Equals(ExpectedInitialBase, 0.1f));
	TestTrue(TEXT("Initial BladeTip matches captured sample"), PlayerTrail->GetTestLastBladeTip().Equals(ExpectedInitialTip, 0.1f));
	TestEqual(TEXT("Trail component StartTrail call count is 1"), PlayerTrail->GetTestStartCallCount(), 1);

	// Tick the world to simulate task tick and endpoint forwarding
	World->Tick(ELevelTick::LEVELTICK_All, 0.02f);
	TestTrue(TEXT("Trail component UpdateTrail called during task tick"), PlayerTrail->GetTestUpdateCallCount() >= 1);
	TestTrue(TEXT("Trail component remains active during window"), PlayerTrail->IsTestTrackingActive());
	TestTrue(TEXT("Trail component active requester remains active task"), PlayerTrail->GetTestActiveRequester() == static_cast<const UObject*>(ActiveTask));

	// End ability and verify normal deactivation
	ActiveAbility->EndTestAbility();
	TestFalse(TEXT("Trail component deactivated upon task end"), PlayerTrail->IsTestTrackingActive());
	TestNull(TEXT("Trail component active requester cleared"), PlayerTrail->GetTestActiveRequester());
	TestEqual(TEXT("Trail component EndTrail call count is 1"), PlayerTrail->GetTestEndCallCount(), 1);

	// -------------------------------------------------------------------------
	// SECTION 3: Token Arbitration & Stale Task OnDestroy Race Protection
	// -------------------------------------------------------------------------
	APlayerCharacter* ArbitrationPlayer = FCombatAutomationFixture::SpawnPlayer(World, FTransform(FRotator::ZeroRotator, FVector(500.0f, 0.0f, 0.0f)));
	if (!TestNotNull(TEXT("ArbitrationPlayer spawned"), ArbitrationPlayer))
	{
		return false;
	}

	UMeleeWeaponTrailComponent* ArbitrationTrail = ArbitrationPlayer->FindComponentByClass<UMeleeWeaponTrailComponent>();
	if (!TestNotNull(TEXT("ArbitrationTrail found"), ArbitrationTrail))
	{
		return false;
	}
	ArbitrationTrail->SetTestTrackingEnabled(true);

	UAbilitySystemComponent* ArbitrationASC = ArbitrationPlayer->GetAbilitySystemComponent();
	if (!TestNotNull(TEXT("Arbitration ASC valid"), ArbitrationASC))
	{
		return false;
	}

	FGameplayAbilitySpecHandle HandleA = ArbitrationASC->GiveAbility(
		FGameplayAbilitySpec(UTestMeleeTrailAbility::StaticClass(), 1, INDEX_NONE, ArbitrationPlayer));
	FGameplayAbilitySpecHandle HandleB = ArbitrationASC->GiveAbility(
		FGameplayAbilitySpec(UTestMeleeTrailAbility::StaticClass(), 1, INDEX_NONE, ArbitrationPlayer));
	TestTrue(TEXT("HandleA valid"), HandleA.IsValid());
	TestTrue(TEXT("HandleB valid"), HandleB.IsValid());

	const bool bActivatedA = ArbitrationASC->TryActivateAbility(HandleA);
	TestTrue(TEXT("AbilityA activated"), bActivatedA);

	FGameplayAbilitySpec* SpecA = ArbitrationASC->FindAbilitySpecFromHandle(HandleA);
	UTestMeleeTrailAbility* AbilityA = SpecA ? Cast<UTestMeleeTrailAbility>(SpecA->GetPrimaryInstance()) : nullptr;
	if (!TestNotNull(TEXT("AbilityA instance valid"), AbilityA))
	{
		return false;
	}
	UAbilityTask_MeleeTraceWindow* TaskA = AbilityA->GetActiveTraceWindowTask();
	if (!TestNotNull(TEXT("TaskA valid"), TaskA))
	{
		return false;
	}

	TestTrue(TEXT("Trail active under TaskA"), ArbitrationTrail->IsTestTrackingActive());
	TestTrue(TEXT("Active requester is TaskA"), ArbitrationTrail->GetTestActiveRequester() == static_cast<const UObject*>(TaskA));

	// Ability B activates and Task B takes over active requester ownership
	const bool bActivatedB = ArbitrationASC->TryActivateAbility(HandleB);
	TestTrue(TEXT("AbilityB activated"), bActivatedB);

	FGameplayAbilitySpec* SpecB = ArbitrationASC->FindAbilitySpecFromHandle(HandleB);
	UTestMeleeTrailAbility* AbilityB = SpecB ? Cast<UTestMeleeTrailAbility>(SpecB->GetPrimaryInstance()) : nullptr;
	if (!TestNotNull(TEXT("AbilityB instance valid"), AbilityB))
	{
		return false;
	}
	UAbilityTask_MeleeTraceWindow* TaskB = AbilityB->GetActiveTraceWindowTask();
	if (!TestNotNull(TEXT("TaskB valid"), TaskB))
	{
		return false;
	}

	TestTrue(TEXT("Trail active under TaskB"), ArbitrationTrail->IsTestTrackingActive());
	TestTrue(TEXT("Active requester is TaskB"), ArbitrationTrail->GetTestActiveRequester() == static_cast<const UObject*>(TaskB));

	// Stale TaskA ends / calls OnDestroy -> EndTrail(TaskA); must NOT deactivate or clear TaskB's trail
	AbilityA->EndTestAbility();
	TestTrue(TEXT("Late EndTrail from TaskA did NOT deactivate trail"), ArbitrationTrail->IsTestTrackingActive());
	TestTrue(TEXT("Active requester remains TaskB"), ArbitrationTrail->GetTestActiveRequester() == static_cast<const UObject*>(TaskB));

	// Active TaskB ends -> trail deactivates cleanly
	AbilityB->EndTestAbility();
	TestFalse(TEXT("Trail deactivated after TaskB ended"), ArbitrationTrail->IsTestTrackingActive());
	TestNull(TEXT("Active requester reset to null"), ArbitrationTrail->GetTestActiveRequester());

	// -------------------------------------------------------------------------
	// SECTION 4: Active Task Endpoint Invalidation & Trace Geometry Failure
	// -------------------------------------------------------------------------
	APlayerCharacter* InvalidationPlayer = FCombatAutomationFixture::SpawnPlayer(World, FTransform(FRotator::ZeroRotator, FVector(1000.0f, 0.0f, 0.0f)));
	if (!TestNotNull(TEXT("InvalidationPlayer spawned"), InvalidationPlayer))
	{
		return false;
	}

	UMeleeWeaponTrailComponent* InvalidationTrail = InvalidationPlayer->FindComponentByClass<UMeleeWeaponTrailComponent>();
	if (!TestNotNull(TEXT("InvalidationTrail found"), InvalidationTrail))
	{
		return false;
	}
	InvalidationTrail->SetTestTrackingEnabled(true);

	UAbilitySystemComponent* InvalidationASC = InvalidationPlayer->GetAbilitySystemComponent();
	if (!TestNotNull(TEXT("Invalidation ASC valid"), InvalidationASC))
	{
		return false;
	}

	FGameplayAbilitySpecHandle InvalidationHandle = InvalidationASC->GiveAbility(
		FGameplayAbilitySpec(UTestMeleeTrailAbility::StaticClass(), 1, INDEX_NONE, InvalidationPlayer));
	TestTrue(TEXT("Invalidation ability given"), InvalidationHandle.IsValid());

	const bool bInvalidationActivated = InvalidationASC->TryActivateAbility(InvalidationHandle);
	TestTrue(TEXT("Invalidation ability activated"), bInvalidationActivated);

	FGameplayAbilitySpec* InvalidationSpec = InvalidationASC->FindAbilitySpecFromHandle(InvalidationHandle);
	UTestMeleeTrailAbility* InvalidationAbility = InvalidationSpec ? Cast<UTestMeleeTrailAbility>(InvalidationSpec->GetPrimaryInstance()) : nullptr;
	if (!TestNotNull(TEXT("InvalidationAbility valid"), InvalidationAbility))
	{
		return false;
	}

	UAbilityTask_MeleeTraceWindow* InvalidationTask = InvalidationAbility->GetActiveTraceWindowTask();
	if (!TestNotNull(TEXT("InvalidationTask valid"), InvalidationTask))
	{
		return false;
	}

	TestTrue(TEXT("InvalidationTask trace window is open"), InvalidationTask->IsTraceWindowOpen());
	TestTrue(TEXT("InvalidationTrail is active before endpoint invalidation"), InvalidationTrail->IsTestTrackingActive());
	TestTrue(TEXT("InvalidationTrail active requester is InvalidationTask"), InvalidationTrail->GetTestActiveRequester() == static_cast<const UObject*>(InvalidationTask));

	UWeaponEquipmentComponent* InvalidationEquipment = InvalidationPlayer->FindComponentByClass<UWeaponEquipmentComponent>();
	if (!TestNotNull(TEXT("InvalidationEquipment valid"), InvalidationEquipment))
	{
		return false;
	}

	USceneComponent* BladeBaseMarker = nullptr;
	USceneComponent* BladeTipMarker = nullptr;
	TestTrue(TEXT("TryGetBladeMarkers returned valid markers"), InvalidationEquipment->TryGetBladeMarkers(BladeBaseMarker, BladeTipMarker));
	if (!TestNotNull(TEXT("BladeBaseMarker valid"), BladeBaseMarker) || !TestNotNull(TEXT("BladeTipMarker valid"), BladeTipMarker))
	{
		return false;
	}

	// Move Tip to match Base world location, causing CaptureCurrentBladeEndpoints() to fail on next tick
	BladeTipMarker->SetWorldLocation(BladeBaseMarker->GetComponentLocation());
	TestTrue(
		TEXT("Tip marker moved to coincident world position with base marker"),
		BladeTipMarker->GetComponentLocation().Equals(BladeBaseMarker->GetComponentLocation(), KINDA_SMALL_NUMBER));

	// Tick world to trigger TraceCurrentSegment() and endpoint failure
	World->Tick(ELevelTick::LEVELTICK_All, 0.02f);

	TestFalse(TEXT("Trace window task closed after endpoint failure"), InvalidationTask->IsTraceWindowOpen());
	TestNull(TEXT("Trail active requester cleared after endpoint failure"), InvalidationTrail->GetTestActiveRequester());
	TestFalse(TEXT("Trail test tracking inactive after endpoint failure"), InvalidationTrail->IsTestTrackingActive());

	// Verify subsequent trace window / ability operates normally once valid geometry is restored
	BladeTipMarker->SetRelativeLocation(FVector(20.0f, 0.0f, 0.0f));
	InvalidationAbility->EndTestAbility();

	FGameplayAbilitySpecHandle SubsequentHandle = InvalidationASC->GiveAbility(
		FGameplayAbilitySpec(UTestMeleeTrailAbility::StaticClass(), 1, INDEX_NONE, InvalidationPlayer));
	TestTrue(TEXT("Subsequent ability given"), SubsequentHandle.IsValid());

	const bool bSubsequentActivated = InvalidationASC->TryActivateAbility(SubsequentHandle);
	TestTrue(TEXT("Subsequent ability activated"), bSubsequentActivated);

	FGameplayAbilitySpec* SubsequentSpec = InvalidationASC->FindAbilitySpecFromHandle(SubsequentHandle);
	UTestMeleeTrailAbility* SubsequentAbility = SubsequentSpec ? Cast<UTestMeleeTrailAbility>(SubsequentSpec->GetPrimaryInstance()) : nullptr;
	if (TestNotNull(TEXT("SubsequentAbility valid"), SubsequentAbility))
	{
		UAbilityTask_MeleeTraceWindow* SubsequentTask = SubsequentAbility->GetActiveTraceWindowTask();
		if (TestNotNull(TEXT("SubsequentTask valid"), SubsequentTask))
		{
			TestTrue(TEXT("Subsequent trace window opened normally"), SubsequentTask->IsTraceWindowOpen());
			TestTrue(TEXT("Subsequent trail tracking is active"), InvalidationTrail->IsTestTrackingActive());
			TestTrue(TEXT("Subsequent trail requester is SubsequentTask"), InvalidationTrail->GetTestActiveRequester() == static_cast<const UObject*>(SubsequentTask));
		}
		SubsequentAbility->EndTestAbility();
		TestFalse(TEXT("Subsequent trail tracking deactivated cleanly"), InvalidationTrail->IsTestTrackingActive());
	}

	// -------------------------------------------------------------------------
	// SECTION 5: Destruction Safety & Teardown with Real Active Task (Player & Enemy)
	// -------------------------------------------------------------------------
	// 5.1 Player Destruction Teardown
	APlayerCharacter* DestructionPlayer = FCombatAutomationFixture::SpawnPlayer(World, FTransform(FRotator::ZeroRotator, FVector(1500.0f, 0.0f, 0.0f)));
	if (TestNotNull(TEXT("DestructionPlayer spawned"), DestructionPlayer))
	{
		UMeleeWeaponTrailComponent* DestructionTrail = DestructionPlayer->FindComponentByClass<UMeleeWeaponTrailComponent>();
		UAbilitySystemComponent* DestructionASC = DestructionPlayer->GetAbilitySystemComponent();
		if (TestNotNull(TEXT("DestructionTrail found"), DestructionTrail) && TestNotNull(TEXT("DestructionASC valid"), DestructionASC))
		{
			DestructionTrail->SetTestTrackingEnabled(true);

			FGameplayAbilitySpecHandle DestructionHandle = DestructionASC->GiveAbility(
				FGameplayAbilitySpec(UTestMeleeTrailAbility::StaticClass(), 1, INDEX_NONE, DestructionPlayer));
			TestTrue(TEXT("Player destruction ability given"), DestructionHandle.IsValid());

			const bool bDestructionActivated = DestructionASC->TryActivateAbility(DestructionHandle);
			TestTrue(TEXT("Player destruction ability activated"), bDestructionActivated);

			FGameplayAbilitySpec* DestructionSpec = DestructionASC->FindAbilitySpecFromHandle(DestructionHandle);
			UTestMeleeTrailAbility* DestructionAbility = DestructionSpec ? Cast<UTestMeleeTrailAbility>(DestructionSpec->GetPrimaryInstance()) : nullptr;
			if (TestNotNull(TEXT("DestructionAbility valid"), DestructionAbility))
			{
				UAbilityTask_MeleeTraceWindow* DestructionTask = DestructionAbility->GetActiveTraceWindowTask();
				TestNotNull(TEXT("Player destruction task active before destroy"), DestructionTask);
				TestTrue(TEXT("Player destruction trail active before destroy"), DestructionTrail->IsTestTrackingActive());

				DestructionPlayer->Destroy();
				World->Tick(ELevelTick::LEVELTICK_All, 0.02f);

				TestNull(TEXT("Player active trace window task cleared after actor destruction/ability teardown"), DestructionAbility->GetActiveTraceWindowTask());
			}
		}
	}

	// 5.2 Enemy Destruction Teardown with Content-Free Legacy Trace Fixture
	AEnemyCharacter* DestructionEnemy = FCombatAutomationFixture::SpawnPassiveEnemy(
		World,
		FTransform(FRotator::ZeroRotator, FVector(2000.0f, 0.0f, 0.0f)),
		[](AEnemyCharacter& InEnemy)
		{
			USceneComponent* WeaponMesh = NewObject<USceneComponent>(&InEnemy, TEXT("WeaponMesh"));
			WeaponMesh->RegisterComponent();
			WeaponMesh->AttachToComponent(InEnemy.GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);

			USceneComponent* BladeBase = NewObject<USceneComponent>(&InEnemy, TEXT("BladeTraceBase"));
			BladeBase->RegisterComponent();
			BladeBase->AttachToComponent(WeaponMesh, FAttachmentTransformRules::KeepRelativeTransform);
			BladeBase->SetRelativeLocation(FVector(0.0f, 0.0f, 10.0f));

			USceneComponent* BladeTip = NewObject<USceneComponent>(&InEnemy, TEXT("BladeTraceTip"));
			BladeTip->RegisterComponent();
			BladeTip->AttachToComponent(WeaponMesh, FAttachmentTransformRules::KeepRelativeTransform);
			BladeTip->SetRelativeLocation(FVector(0.0f, 0.0f, 80.0f));
		});

	if (TestNotNull(TEXT("DestructionEnemy spawned"), DestructionEnemy))
	{
		UMeleeWeaponTrailComponent* EnemyTrail = DestructionEnemy->FindComponentByClass<UMeleeWeaponTrailComponent>();
		UAbilitySystemComponent* EnemyASC = DestructionEnemy->GetAbilitySystemComponent();
		if (TestNotNull(TEXT("EnemyTrail found"), EnemyTrail) && TestNotNull(TEXT("EnemyASC valid"), EnemyASC))
		{
			EnemyTrail->SetTestTrackingEnabled(true);

			FGameplayAbilitySpecHandle EnemyDestructionHandle = EnemyASC->GiveAbility(
				FGameplayAbilitySpec(UTestMeleeTrailAbility::StaticClass(), 1, INDEX_NONE, DestructionEnemy));
			TestTrue(TEXT("Enemy destruction ability given"), EnemyDestructionHandle.IsValid());

			const bool bEnemyDestructionActivated = EnemyASC->TryActivateAbility(EnemyDestructionHandle);
			TestTrue(TEXT("Enemy destruction ability activated"), bEnemyDestructionActivated);

			FGameplayAbilitySpec* EnemyDestructionSpec = EnemyASC->FindAbilitySpecFromHandle(EnemyDestructionHandle);
			UTestMeleeTrailAbility* EnemyDestructionAbility = EnemyDestructionSpec ? Cast<UTestMeleeTrailAbility>(EnemyDestructionSpec->GetPrimaryInstance()) : nullptr;
			if (TestNotNull(TEXT("EnemyDestructionAbility valid"), EnemyDestructionAbility))
			{
				UAbilityTask_MeleeTraceWindow* EnemyDestructionTask = EnemyDestructionAbility->GetActiveTraceWindowTask();
				TestNotNull(TEXT("Enemy destruction task active before destroy"), EnemyDestructionTask);
				TestTrue(TEXT("Enemy destruction trail active before destroy"), EnemyTrail->IsTestTrackingActive());
				TestTrue(TEXT("Enemy destruction trail requester is EnemyDestructionTask"), EnemyTrail->GetTestActiveRequester() == static_cast<const UObject*>(EnemyDestructionTask));

				DestructionEnemy->Destroy();
				World->Tick(ELevelTick::LEVELTICK_All, 0.02f);

				TestNull(TEXT("Enemy active trace window task cleared after actor destruction/ability teardown"), EnemyDestructionAbility->GetActiveTraceWindowTask());
			}
		}
	}

	// -------------------------------------------------------------------------
	// SECTION 6: No-Asset Silent No-Op & Trace/Resolver Integrity
	// -------------------------------------------------------------------------
	APlayerCharacter* NoAssetPlayer = FCombatAutomationFixture::SpawnPlayer(World, FTransform(FRotator::ZeroRotator, FVector(2500.0f, 0.0f, 0.0f)));
	if (!TestNotNull(TEXT("NoAssetPlayer spawned"), NoAssetPlayer))
	{
		return false;
	}

	UMeleeWeaponTrailComponent* NoAssetTrail = NoAssetPlayer->FindComponentByClass<UMeleeWeaponTrailComponent>();
	if (!TestNotNull(TEXT("NoAssetPlayer owns UMeleeWeaponTrailComponent"), NoAssetTrail))
	{
		return false;
	}

	TestNull(TEXT("NoAssetTrail starts with no Niagara System asset assigned"), NoAssetTrail->GetAsset());
	TestFalse(TEXT("NoAssetTrail test tracking is disabled"), NoAssetTrail->IsTestTrackingActive());

	UAbilitySystemComponent* NoAssetASC = NoAssetPlayer->GetAbilitySystemComponent();
	if (!TestNotNull(TEXT("NoAsset ASC is valid"), NoAssetASC))
	{
		return false;
	}

	FGameplayAbilitySpecHandle NoAssetAbilityHandle = NoAssetASC->GiveAbility(
		FGameplayAbilitySpec(UTestMeleeTrailAbility::StaticClass(), 1, INDEX_NONE, NoAssetPlayer));
	TestTrue(TEXT("UTestMeleeTrailAbility given to NoAsset Player ASC"), NoAssetAbilityHandle.IsValid());

	const bool bNoAssetActivated = NoAssetASC->TryActivateAbility(NoAssetAbilityHandle);
	TestTrue(TEXT("UTestMeleeTrailAbility activated successfully on NoAsset Player"), bNoAssetActivated);

	FGameplayAbilitySpec* NoAssetSpec = NoAssetASC->FindAbilitySpecFromHandle(NoAssetAbilityHandle);
	UTestMeleeTrailAbility* ActiveNoAssetAbility = NoAssetSpec ? Cast<UTestMeleeTrailAbility>(NoAssetSpec->GetPrimaryInstance()) : nullptr;
	if (!TestNotNull(TEXT("ActiveNoAssetAbility valid"), ActiveNoAssetAbility))
	{
		return false;
	}

	UAbilityTask_MeleeTraceWindow* NoAssetTask = ActiveNoAssetAbility->GetActiveTraceWindowTask();
	if (!TestNotNull(TEXT("Active trace window task exists on NoAsset Player"), NoAssetTask))
	{
		return false;
	}

	TestTrue(TEXT("Trace window is open and valid"), NoAssetTask->IsTraceWindowOpen());
	TestFalse(TEXT("No-asset trail component remains inactive upon task start"), NoAssetTrail->IsActive());
	TestNull(TEXT("No-asset trail component has no active requester"), NoAssetTrail->GetTestActiveRequester());

	// World tick simulates active trace sweeping without Niagara asset
	World->Tick(ELevelTick::LEVELTICK_All, 0.02f);
	TestTrue(TEXT("Trace window remains open after tick"), NoAssetTask->IsTraceWindowOpen());
	TestFalse(TEXT("No-asset trail component still inactive after world tick"), NoAssetTrail->IsActive());

	ActiveNoAssetAbility->EndTestAbility();
	TestFalse(TEXT("No-asset trail component remains inactive after task end"), NoAssetTrail->IsActive());
	TestNull(TEXT("No-asset trail active requester remains null"), NoAssetTrail->GetTestActiveRequester());

	return true;
}

#endif
