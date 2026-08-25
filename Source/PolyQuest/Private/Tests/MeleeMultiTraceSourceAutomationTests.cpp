#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "AbilitySystem/CharacterAttributeSet.h"
#include "AbilitySystem/Tasks/AbilityTask_MeleeTraceWindow.h"
#include "Animation/Combat/AnimNotifyState_ActionWindows.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Character/Player/PlayerCharacter.h"
#include "Combat/Equipment/MeleeWeaponDefinition.h"
#include "Combat/Equipment/WeaponEquipmentComponent.h"
#include "Combat/Melee/MeleeTraceSourceComponent.h"
#include "Combat/Melee/MeleeWeaponTrailComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Tests/CombatAutomationFixture.h"
#include "Tests/TestMeleeTrailAbility.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMeleeMultiTraceSourceAutomationTest,
	"PolyQuest.Melee.MultiTraceSource",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	struct FMultiTraceTestWorldCleanup
	{
		UWorld* World = nullptr;
		~FMultiTraceTestWorldCleanup()
		{
			if (World)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
		}
	};
}

bool FMeleeMultiTraceSourceAutomationTest::RunTest(const FString& Parameters)
{
	if (!TestNotNull(TEXT("Engine is available for multi-source trace automation"), GEngine))
	{
		return false;
	}

	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("MeleeMultiTraceTestWorld"));
	WorldContext.SetCurrentWorld(World);
	FMultiTraceTestWorldCleanup Cleanup{ World };
	if (!TestNotNull(TEXT("Test World created successfully"), World))
	{
		return false;
	}

	FURL WorldURL;
	World->InitializeActorsForPlay(WorldURL);
	World->BeginPlay();

	// -------------------------------------------------------------------------
	// CASE 1: Definition Validation & Trace Source Name Resolution
	// -------------------------------------------------------------------------
	{
		// 1.1 Valid multi-source unarmed weapon definition
		UMeleeWeaponDefinition* ValidMultiSourceDef = NewObject<UMeleeWeaponDefinition>(GetTransientPackage());
		ValidMultiSourceDef->HandSlot = EWeaponHandSlot::MainHandOneHanded;
		ValidMultiSourceDef->LocomotionMode = EWeaponLocomotionMode::Default;
		ValidMultiSourceDef->AttachSocketName = FName(TEXT("Weapon_R"));
		ValidMultiSourceDef->WeaponMesh = nullptr;
		ValidMultiSourceDef->bUseOwnerMeshSocketForTrace = true;
		ValidMultiSourceDef->DefaultOwnerMeshTraceSourceName = FName(TEXT("RightFist"));

		FOwnerMeshMeleeTraceSource RightFistSource;
		RightFistSource.TraceSourceName = FName(TEXT("RightFist"));
		RightFistSource.OwnerMeshSocketName = FName(TEXT("hand_r"));
		RightFistSource.BladeBaseMarkerRelativeLocation = FVector::ZeroVector;
		RightFistSource.BladeTipMarkerRelativeLocation = FVector(0.0f, 0.0f, 20.0f);

		FOwnerMeshMeleeTraceSource LeftFistSource;
		LeftFistSource.TraceSourceName = FName(TEXT("LeftFist"));
		LeftFistSource.OwnerMeshSocketName = FName(TEXT("hand_l"));
		LeftFistSource.BladeBaseMarkerRelativeLocation = FVector::ZeroVector;
		LeftFistSource.BladeTipMarkerRelativeLocation = FVector(0.0f, 0.0f, 20.0f);

		ValidMultiSourceDef->OwnerMeshTraceSources.Add(RightFistSource);
		ValidMultiSourceDef->OwnerMeshTraceSources.Add(LeftFistSource);

		FString ValidationReason;
		TestTrue(TEXT("Case 1.1: Valid multi-source definition passes validation"), ValidMultiSourceDef->IsValidWeaponDefinition(ValidationReason));

		FName ResolvedName = NAME_None;
		TestTrue(TEXT("Case 1.1: Resolve explicitly named source 'RightFist'"), ValidMultiSourceDef->TryResolveTraceSourceName(FName(TEXT("RightFist")), ResolvedName));
		TestEqual(TEXT("Case 1.1: Resolved name matches"), ResolvedName, FName(TEXT("RightFist")));

		TestTrue(TEXT("Case 1.1: Resolve explicitly named source 'LeftFist'"), ValidMultiSourceDef->TryResolveTraceSourceName(FName(TEXT("LeftFist")), ResolvedName));
		TestEqual(TEXT("Case 1.1: Resolved name matches"), ResolvedName, FName(TEXT("LeftFist")));

		TestTrue(TEXT("Case 1.1: Resolve NAME_None to DefaultOwnerMeshTraceSourceName"), ValidMultiSourceDef->TryResolveTraceSourceName(NAME_None, ResolvedName));
		TestEqual(TEXT("Case 1.1: Resolved name is default 'RightFist'"), ResolvedName, FName(TEXT("RightFist")));

		TestFalse(TEXT("Case 1.1: Resolve unknown source returns false"), ValidMultiSourceDef->TryResolveTraceSourceName(FName(TEXT("NonExistentSource")), ResolvedName));

		// 1.2 Duplicate profile names fail validation
		UMeleeWeaponDefinition* DuplicateSourceDef = DuplicateObject<UMeleeWeaponDefinition>(ValidMultiSourceDef, GetTransientPackage());
		DuplicateSourceDef->OwnerMeshTraceSources[1].TraceSourceName = FName(TEXT("RightFist"));
		TestFalse(TEXT("Case 1.2: Duplicate trace source name fails validation"), DuplicateSourceDef->IsValidWeaponDefinition(ValidationReason));

		// 1.3 Coincident endpoints fail validation
		UMeleeWeaponDefinition* CoincidentDef = DuplicateObject<UMeleeWeaponDefinition>(ValidMultiSourceDef, GetTransientPackage());
		CoincidentDef->OwnerMeshTraceSources[0].BladeTipMarkerRelativeLocation = CoincidentDef->OwnerMeshTraceSources[0].BladeBaseMarkerRelativeLocation;
		TestFalse(TEXT("Case 1.3: Coincident endpoints fail validation"), CoincidentDef->IsValidWeaponDefinition(ValidationReason));

		// 1.4 Default source not in array fails validation
		UMeleeWeaponDefinition* MissingDefaultDef = DuplicateObject<UMeleeWeaponDefinition>(ValidMultiSourceDef, GetTransientPackage());
		MissingDefaultDef->DefaultOwnerMeshTraceSourceName = FName(TEXT("UnknownDefault"));
		TestFalse(TEXT("Case 1.4: Default source not in array fails validation"), MissingDefaultDef->IsValidWeaponDefinition(ValidationReason));

		// 1.5 Non-OwnerMesh mode defining OwnerMeshTraceSources fails validation
		UMeleeWeaponDefinition* StaticMeshModeWithProfilesDef = DuplicateObject<UMeleeWeaponDefinition>(ValidMultiSourceDef, GetTransientPackage());
		StaticMeshModeWithProfilesDef->bUseOwnerMeshSocketForTrace = false;
		TestFalse(TEXT("Case 1.5: StaticMesh mode defining OwnerMeshTraceSources fails validation"), StaticMeshModeWithProfilesDef->IsValidWeaponDefinition(ValidationReason));
	}

	// -------------------------------------------------------------------------
	// SETUP CHARACTERS FOR RUNTIME TESTING
	// -------------------------------------------------------------------------
	APlayerCharacter* Player = FCombatAutomationFixture::SpawnPlayer(World, FTransform(FVector(0.0f, 0.0f, 100.0f)));
	if (!TestNotNull(TEXT("Player spawned successfully"), Player))
	{
		return false;
	}

	AEnemyCharacter* Enemy = FCombatAutomationFixture::SpawnPassiveEnemy(World, FTransform(FVector(80.0f, 0.0f, 100.0f)));
	if (!TestNotNull(TEXT("Enemy spawned successfully"), Enemy))
	{
		return false;
	}

	const FGameplayTag TagTeamPlayer = FGameplayTag::RequestGameplayTag(FName(TEXT("Team.Player")), false);
	const FGameplayTag TagTeamEnemy = FGameplayTag::RequestGameplayTag(FName(TEXT("Team.Enemy")), false);
	Player->SetTestCombatTeamTag(TagTeamPlayer);
	Enemy->SetTestCombatTeamTag(TagTeamEnemy);

	UWeaponEquipmentComponent* EquipComp = Player->FindComponentByClass<UWeaponEquipmentComponent>();
	if (!TestNotNull(TEXT("Player has UWeaponEquipmentComponent"), EquipComp))
	{
		return false;
	}

	UMeleeTraceSourceComponent* TraceSourceComp = Player->GetMeleeTraceSource();
	if (!TestNotNull(TEXT("Player has UMeleeTraceSourceComponent"), TraceSourceComp))
	{
		return false;
	}

	UMeleeWeaponTrailComponent* TrailComp = Player->FindComponentByClass<UMeleeWeaponTrailComponent>();
	if (!TestNotNull(TEXT("Player has UMeleeWeaponTrailComponent"), TrailComp))
	{
		return false;
	}

	// -------------------------------------------------------------------------
	// CASE 2: Equipment Component Marker Creation & Lookup
	// -------------------------------------------------------------------------
	{
		// Equip the multi-source unarmed weapon
		UMeleeWeaponDefinition* MultiSourceWeapon = NewObject<UMeleeWeaponDefinition>(GetTransientPackage());
		MultiSourceWeapon->HandSlot = EWeaponHandSlot::MainHandOneHanded;
		MultiSourceWeapon->LocomotionMode = EWeaponLocomotionMode::Default;
		MultiSourceWeapon->AttachSocketName = FName(TEXT("Weapon_R"));
		MultiSourceWeapon->WeaponMesh = nullptr;
		MultiSourceWeapon->bUseOwnerMeshSocketForTrace = true;
		MultiSourceWeapon->DefaultOwnerMeshTraceSourceName = FName(TEXT("RightFist"));

		FOwnerMeshMeleeTraceSource RightSource;
		RightSource.TraceSourceName = FName(TEXT("RightFist"));
		RightSource.OwnerMeshSocketName = FName(TEXT("hand_r"));
		RightSource.BladeBaseMarkerRelativeLocation = FVector(0.0f, 0.0f, 0.0f);
		RightSource.BladeTipMarkerRelativeLocation = FVector(0.0f, 0.0f, 25.0f);

		FOwnerMeshMeleeTraceSource LeftSource;
		LeftSource.TraceSourceName = FName(TEXT("LeftFist"));
		LeftSource.OwnerMeshSocketName = FName(TEXT("hand_l"));
		LeftSource.BladeBaseMarkerRelativeLocation = FVector(0.0f, 0.0f, 0.0f);
		LeftSource.BladeTipMarkerRelativeLocation = FVector(0.0f, 0.0f, 25.0f);

		MultiSourceWeapon->OwnerMeshTraceSources.Add(RightSource);
		MultiSourceWeapon->OwnerMeshTraceSources.Add(LeftSource);

		const bool bEquipSuccess = EquipComp->EquipWeapon(MultiSourceWeapon);
		TestTrue(TEXT("Case 2: Multi-source weapon equipped successfully"), bEquipSuccess);

		USceneComponent* RightBase = nullptr;
		USceneComponent* RightTip = nullptr;
		TestTrue(TEXT("Case 2: TryGetBladeMarkers for 'RightFist' succeeds"), EquipComp->TryGetBladeMarkers(FName(TEXT("RightFist")), RightBase, RightTip));
		TestNotNull(TEXT("Case 2: RightBase marker is valid"), RightBase);
		TestNotNull(TEXT("Case 2: RightTip marker is valid"), RightTip);

		USceneComponent* LeftBase = nullptr;
		USceneComponent* LeftTip = nullptr;
		TestTrue(TEXT("Case 2: TryGetBladeMarkers for 'LeftFist' succeeds"), EquipComp->TryGetBladeMarkers(FName(TEXT("LeftFist")), LeftBase, LeftTip));
		TestNotNull(TEXT("Case 2: LeftBase marker is valid"), LeftBase);
		TestNotNull(TEXT("Case 2: LeftTip marker is valid"), LeftTip);
		TestTrue(TEXT("Case 2: Right and Left markers are distinct components"), RightBase != LeftBase && RightTip != LeftTip);

		USceneComponent* DefaultBase = nullptr;
		USceneComponent* DefaultTip = nullptr;
		TestTrue(TEXT("Case 2: TryGetBladeMarkers with NAME_None returns default markers"), EquipComp->TryGetBladeMarkers(NAME_None, DefaultBase, DefaultTip));
		TestTrue(TEXT("Case 2: Default markers point to 'RightFist'"), DefaultBase == RightBase && DefaultTip == RightTip);

		USceneComponent* NonExistentBase = nullptr;
		USceneComponent* NonExistentTip = nullptr;
		TestFalse(TEXT("Case 2: TryGetBladeMarkers for non-existent source fails"), EquipComp->TryGetBladeMarkers(FName(TEXT("InvalidSource")), NonExistentBase, NonExistentTip));
		TestNull(TEXT("Case 2: NonExistentBase is null on failure"), NonExistentBase);
		TestNull(TEXT("Case 2: NonExistentTip is null on failure"), NonExistentTip);
	}

	// -------------------------------------------------------------------------
	// CASE 3: MeleeTraceSourceComponent Endpoint Resolution
	// -------------------------------------------------------------------------
	{
		FVector RightBasePos, RightTipPos;
		TestTrue(TEXT("Case 3: MeleeTraceSourceComponent resolves 'RightFist'"), TraceSourceComp->TryGetBladeEndpoints(FName(TEXT("RightFist")), RightBasePos, RightTipPos));
		TestFalse(TEXT("Case 3: RightBasePos is finite"), RightBasePos.ContainsNaN());
		TestFalse(TEXT("Case 3: RightTipPos is finite"), RightTipPos.ContainsNaN());
		TestTrue(TEXT("Case 3: Right endpoints are distinct"), FVector::DistSquared(RightBasePos, RightTipPos) > KINDA_SMALL_NUMBER);

		FVector LeftBasePos, LeftTipPos;
		TestTrue(TEXT("Case 3: MeleeTraceSourceComponent resolves 'LeftFist'"), TraceSourceComp->TryGetBladeEndpoints(FName(TEXT("LeftFist")), LeftBasePos, LeftTipPos));
		TestFalse(TEXT("Case 3: LeftBasePos is finite"), LeftBasePos.ContainsNaN());
		TestFalse(TEXT("Case 3: LeftTipPos is finite"), LeftTipPos.ContainsNaN());
		TestTrue(TEXT("Case 3: Left endpoints are distinct"), FVector::DistSquared(LeftBasePos, LeftTipPos) > KINDA_SMALL_NUMBER);

		FVector DefaultBasePos, DefaultTipPos;
		TestTrue(TEXT("Case 3: MeleeTraceSourceComponent resolves NAME_None to default"), TraceSourceComp->TryGetBladeEndpoints(NAME_None, DefaultBasePos, DefaultTipPos));
		TestEqual(TEXT("Case 3: Default endpoints match 'RightFist'"), DefaultBasePos, RightBasePos);
		TestEqual(TEXT("Case 3: Default endpoints match 'RightFist'"), DefaultTipPos, RightTipPos);

		FVector InvalidBasePos, InvalidTipPos;
		TestFalse(TEXT("Case 3: MeleeTraceSourceComponent rejects invalid source"), TraceSourceComp->TryGetBladeEndpoints(FName(TEXT("Invalid")), InvalidBasePos, InvalidTipPos));
	}

	// -------------------------------------------------------------------------
	// CASE 4: Equipment Teardown and Rollback
	// -------------------------------------------------------------------------
	{
		USceneComponent* RightBase = nullptr;
		USceneComponent* RightTip = nullptr;
		TestTrue(TEXT("Case 4: RightFist markers exist before teardown"), EquipComp->TryGetBladeMarkers(FName(TEXT("RightFist")), RightBase, RightTip));

		USceneComponent* LeftBase = nullptr;
		USceneComponent* LeftTip = nullptr;
		TestTrue(TEXT("Case 4: LeftFist markers exist before teardown"), EquipComp->TryGetBladeMarkers(FName(TEXT("LeftFist")), LeftBase, LeftTip));

		// Equip another weapon to tear down existing multi-source markers
		UMeleeWeaponDefinition* NewUnarmedDef = NewObject<UMeleeWeaponDefinition>(GetTransientPackage());
		NewUnarmedDef->HandSlot = EWeaponHandSlot::MainHandOneHanded;
		NewUnarmedDef->LocomotionMode = EWeaponLocomotionMode::Default;
		NewUnarmedDef->AttachSocketName = FName(TEXT("Weapon_R"));
		NewUnarmedDef->bUseOwnerMeshSocketForTrace = true;
		NewUnarmedDef->BladeBaseMarkerRelativeLocation = FVector(0.0f, 0.0f, 5.0f);
		NewUnarmedDef->BladeTipMarkerRelativeLocation = FVector(0.0f, 0.0f, 20.0f);
		EquipComp->EquipWeapon(NewUnarmedDef);

		TestFalse(TEXT("Case 4: Teardown clears RightFist markers"), EquipComp->TryGetBladeMarkers(FName(TEXT("RightFist")), RightBase, RightTip));
		TestFalse(TEXT("Case 4: Teardown clears LeftFist markers"), EquipComp->TryGetBladeMarkers(FName(TEXT("LeftFist")), LeftBase, LeftTip));
	}

	// -------------------------------------------------------------------------
	// CASE 5: Multi-Source Swept Collision, World Ticking, and Hit Deduplication
	// -------------------------------------------------------------------------
	{
		// Re-equip multi-source weapon with generous trace radius for deterministic sweep coverage
		UMeleeWeaponDefinition* MultiSourceWeapon = NewObject<UMeleeWeaponDefinition>(GetTransientPackage());
		MultiSourceWeapon->HandSlot = EWeaponHandSlot::MainHandOneHanded;
		MultiSourceWeapon->LocomotionMode = EWeaponLocomotionMode::Default;
		MultiSourceWeapon->AttachSocketName = FName(TEXT("Weapon_R"));
		MultiSourceWeapon->bUseOwnerMeshSocketForTrace = true;
		MultiSourceWeapon->TraceRadius = 100.0f;
		MultiSourceWeapon->BladeSubdivisions = 4;
		MultiSourceWeapon->DefaultOwnerMeshTraceSourceName = FName(TEXT("RightFist"));

		FOwnerMeshMeleeTraceSource RightSource;
		RightSource.TraceSourceName = FName(TEXT("RightFist"));
		RightSource.OwnerMeshSocketName = FName(TEXT("hand_r"));
		RightSource.BladeBaseMarkerRelativeLocation = FVector(0.0f, -30.0f, 0.0f);
		RightSource.BladeTipMarkerRelativeLocation = FVector(0.0f, -30.0f, 50.0f);

		FOwnerMeshMeleeTraceSource LeftSource;
		LeftSource.TraceSourceName = FName(TEXT("LeftFist"));
		LeftSource.OwnerMeshSocketName = FName(TEXT("hand_l"));
		LeftSource.BladeBaseMarkerRelativeLocation = FVector(0.0f, 30.0f, 0.0f);
		LeftSource.BladeTipMarkerRelativeLocation = FVector(0.0f, 30.0f, 50.0f);

		MultiSourceWeapon->OwnerMeshTraceSources.Add(RightSource);
		MultiSourceWeapon->OwnerMeshTraceSources.Add(LeftSource);
		EquipComp->EquipWeapon(MultiSourceWeapon);

		const UCharacterAttributeSet* EnemyAttr = Enemy->GetAbilitySystemComponent() ? Enemy->GetAbilitySystemComponent()->GetSet<UCharacterAttributeSet>() : nullptr;
		TestNotNull(TEXT("Case 5: Enemy owns valid AttributeSet"), EnemyAttr);
		const float InitialHealth = EnemyAttr ? EnemyAttr->GetHealth() : 0.0f;
		TestTrue(TEXT("Case 5: Initial health is positive"), InitialHealth > 0.0f);

		auto AdvanceWorld = [World](float DeltaSeconds = 0.033f)
		{
			World->Tick(ELevelTick::LEVELTICK_All, DeltaSeconds);
			++GFrameCounter;
		};

		// 5.1 Dual-source simultaneously overlapping single enemy -> exactly 1 hit damage delivery
		Player->SetActorLocation(FVector(0.0f, 0.0f, 100.0f));
		Player->GetMesh()->UpdateComponentToWorld();
		AdvanceWorld(0.033f);

		UAbilitySystemComponent* PlayerASC = Player->GetAbilitySystemComponent();
		FGameplayAbilitySpec Spec(UTestMeleeTrailAbility::StaticClass(), 1);
		FGameplayAbilitySpecHandle Handle = PlayerASC->GiveAbility(Spec);
		PlayerASC->TryActivateAbility(Handle);

		UTestMeleeTrailAbility* ActiveAbility = Cast<UTestMeleeTrailAbility>(PlayerASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance());
		TestNotNull(TEXT("Case 5.1: Test ability activated with dual sources"), ActiveAbility);

		UAnimNotifyState_AttackTraceWindow* NotifyState51 = NewObject<UAnimNotifyState_AttackTraceWindow>(GetTransientPackage());
		NotifyState51->TraceSourceNames = { FName(TEXT("RightFist")), FName(TEXT("LeftFist")) };
		ActiveAbility->HandleTestTraceWindowBegin(NotifyState51);

		UAbilityTask_MeleeTraceWindow* Task = ActiveAbility ? ActiveAbility->GetActiveTraceWindowTask() : nullptr;
		TestNotNull(TEXT("Case 5.1: AbilityTask_MeleeTraceWindow created"), Task);
		TestTrue(TEXT("Case 5.1: Dual-source trace window is open"), Task && Task->IsTraceWindowOpen());

		// Move player forward across the enemy and tick the world to drive sweep collision
		Player->SetActorLocation(FVector(80.0f, 0.0f, 100.0f));
		Player->GetMesh()->UpdateComponentToWorld();
		AdvanceWorld(0.033f);

		const float HealthAfterFirstSweep = EnemyAttr ? EnemyAttr->GetHealth() : 0.0f;
		// UTestProjectileDamageGE applies 25.0f base damage
		TestEqual(TEXT("Case 5.1: Exactly one damage instance applied despite dual-source coverage (deduplication success)"), HealthAfterFirstSweep, InitialHealth - 25.0f);

		// Tick again in the same window: target is already in DeliveredTargets, so health must remain unchanged
		Player->SetActorLocation(FVector(100.0f, 0.0f, 100.0f));
		Player->GetMesh()->UpdateComponentToWorld();
		AdvanceWorld(0.033f);
		const float HealthAfterSecondSweep = EnemyAttr ? EnemyAttr->GetHealth() : 0.0f;
		TestEqual(TEXT("Case 5.1: Subsequent tick in same window does not re-apply damage to delivered target"), HealthAfterSecondSweep, HealthAfterFirstSweep);

		if (ActiveAbility)
		{
			ActiveAbility->EndTestAbility();
		}
		PlayerASC->ClearAbility(Handle);

		// 5.2a Right-only authored window selects and delivers through RightFist.
		Player->SetActorLocation(FVector(0.0f, 0.0f, 100.0f));
		Player->GetMesh()->UpdateComponentToWorld();
		AdvanceWorld(0.033f);

		TrailComp->SetTestTrackingEnabled(true);

		FGameplayAbilitySpec RightSourceSpec(UTestMeleeTrailAbility::StaticClass(), 1);
		FGameplayAbilitySpecHandle RightHandle = PlayerASC->GiveAbility(RightSourceSpec);
		PlayerASC->TryActivateAbility(RightHandle);

		UTestMeleeTrailAbility* ActiveRightAbility = Cast<UTestMeleeTrailAbility>(PlayerASC->FindAbilitySpecFromHandle(RightHandle)->GetPrimaryInstance());
		TestNotNull(TEXT("Case 5.2a: Right-only ability activated"), ActiveRightAbility);

		UAnimNotifyState_AttackTraceWindow* NotifyStateRight = NewObject<UAnimNotifyState_AttackTraceWindow>(GetTransientPackage());
		NotifyStateRight->TraceSourceNames = { FName(TEXT("RightFist")) };
		if (ActiveRightAbility)
		{
			ActiveRightAbility->HandleTestTraceWindowBegin(NotifyStateRight);
		}

		TestTrue(TEXT("Case 5.2a: Right-only window activates RightFist trail tracking"), TrailComp->IsTestTrackingActive(FName(TEXT("RightFist"))));
		TestFalse(TEXT("Case 5.2a: Right-only window does not activate LeftFist trail tracking"), TrailComp->IsTestTrackingActive(FName(TEXT("LeftFist"))));

		Player->SetActorLocation(FVector(80.0f, 0.0f, 100.0f));
		Player->GetMesh()->UpdateComponentToWorld();
		AdvanceWorld(0.033f);

		const float HealthAfterRightSweep = EnemyAttr ? EnemyAttr->GetHealth() : 0.0f;
		TestEqual(TEXT("Case 5.2a: Right-only source independently delivers damage on a separate window"), HealthAfterRightSweep, HealthAfterFirstSweep - 25.0f);

		if (ActiveRightAbility)
		{
			ActiveRightAbility->EndTestAbility();
		}
		PlayerASC->ClearAbility(RightHandle);

		// 5.2b Left-only authored window selects and delivers through LeftFist.
		Player->SetActorLocation(FVector(0.0f, 0.0f, 100.0f));
		Player->GetMesh()->UpdateComponentToWorld();
		AdvanceWorld(0.033f);

		FGameplayAbilitySpec SingleSourceSpec(UTestMeleeTrailAbility::StaticClass(), 1);
		FGameplayAbilitySpecHandle SingleHandle = PlayerASC->GiveAbility(SingleSourceSpec);
		PlayerASC->TryActivateAbility(SingleHandle);

		UTestMeleeTrailAbility* ActiveSingleAbility = Cast<UTestMeleeTrailAbility>(PlayerASC->FindAbilitySpecFromHandle(SingleHandle)->GetPrimaryInstance());
		TestNotNull(TEXT("Case 5.2b: Left-only ability activated"), ActiveSingleAbility);

		UAnimNotifyState_AttackTraceWindow* NotifyState52 = NewObject<UAnimNotifyState_AttackTraceWindow>(GetTransientPackage());
		NotifyState52->TraceSourceNames = { FName(TEXT("LeftFist")) };
		if (ActiveSingleAbility)
		{
			ActiveSingleAbility->HandleTestTraceWindowBegin(NotifyState52);
		}

		TestTrue(TEXT("Case 5.2b: Left-only window activates LeftFist trail tracking"), TrailComp->IsTestTrackingActive(FName(TEXT("LeftFist"))));
		TestFalse(TEXT("Case 5.2b: Left-only window does not leave RightFist trail tracking active"), TrailComp->IsTestTrackingActive(FName(TEXT("RightFist"))));

		Player->SetActorLocation(FVector(80.0f, 0.0f, 100.0f));
		Player->GetMesh()->UpdateComponentToWorld();
		AdvanceWorld(0.033f);

		const float HealthAfterSingleSweep = EnemyAttr ? EnemyAttr->GetHealth() : 0.0f;
		TestEqual(TEXT("Case 5.2b: Left-only source independently delivers damage on a separate window"), HealthAfterSingleSweep, HealthAfterRightSweep - 25.0f);

		if (ActiveSingleAbility)
		{
			ActiveSingleAbility->EndTestAbility();
		}
		PlayerASC->ClearAbility(SingleHandle);
		TrailComp->SetTestTrackingEnabled(false);

		// 5.3 Mid-window coincident endpoints fail-closed
		Player->SetActorLocation(FVector(0.0f, 0.0f, 100.0f));
		Player->GetMesh()->UpdateComponentToWorld();
		AdvanceWorld(0.033f);

		// Enable test tracking to observe trail lifecycle across fail-closed transition
		TrailComp->SetTestTrackingEnabled(true);

		FGameplayAbilitySpec FailClosedSpec(UTestMeleeTrailAbility::StaticClass(), 1);
		FGameplayAbilitySpecHandle FailClosedHandle = PlayerASC->GiveAbility(FailClosedSpec);
		PlayerASC->TryActivateAbility(FailClosedHandle);

		UTestMeleeTrailAbility* ActiveFailClosedAbility = Cast<UTestMeleeTrailAbility>(PlayerASC->FindAbilitySpecFromHandle(FailClosedHandle)->GetPrimaryInstance());
		TestNotNull(TEXT("Case 5.3: FailClosed test ability activated"), ActiveFailClosedAbility);

		UAnimNotifyState_AttackTraceWindow* NotifyState53 = NewObject<UAnimNotifyState_AttackTraceWindow>(GetTransientPackage());
		NotifyState53->TraceSourceNames = { FName(TEXT("RightFist")), FName(TEXT("LeftFist")) };
		ActiveFailClosedAbility->HandleTestTraceWindowBegin(NotifyState53);

		UAbilityTask_MeleeTraceWindow* FailClosedTask = ActiveFailClosedAbility ? ActiveFailClosedAbility->GetActiveTraceWindowTask() : nullptr;
		TestTrue(TEXT("Case 5.3: Window starts open"), FailClosedTask && FailClosedTask->IsTraceWindowOpen());
		TestTrue(TEXT("Case 5.3: RightFist tracking active before degradation"), TrailComp->IsTestTrackingActive(FName(TEXT("RightFist"))));
		TestTrue(TEXT("Case 5.3: LeftFist tracking active before degradation"), TrailComp->IsTestTrackingActive(FName(TEXT("LeftFist"))));

		const float HealthBeforeFailClosed = EnemyAttr ? EnemyAttr->GetHealth() : 0.0f;

		// Force LeftFist markers to coincide in relative and world space to simulate runtime degradation
		USceneComponent* LeftBaseMarker = nullptr;
		USceneComponent* LeftTipMarker = nullptr;
		if (EquipComp->TryGetBladeMarkers(FName(TEXT("LeftFist")), LeftBaseMarker, LeftTipMarker) && LeftBaseMarker && LeftTipMarker)
		{
			LeftTipMarker->SetRelativeLocation(LeftBaseMarker->GetRelativeLocation());
			LeftTipMarker->UpdateComponentToWorld();
		}

		// Move player across enemy and tick world: task should fail-closed during TraceCurrentSegment Phase 1
		Player->SetActorLocation(FVector(80.0f, 0.0f, 100.0f));
		Player->GetMesh()->UpdateComponentToWorld();
		AdvanceWorld(0.033f);

		// Assertions: Task closed, health completely untouched, both source trail tracking inactive & requesters cleared
		TestFalse(TEXT("Case 5.3: Task fail-closed during TickTask when endpoint degenerates"), FailClosedTask && FailClosedTask->IsTraceWindowOpen());
		TestEqual(TEXT("Case 5.3: Enemy health unchanged during fail-closed tick"), EnemyAttr ? EnemyAttr->GetHealth() : 0.0f, HealthBeforeFailClosed);
		TestFalse(TEXT("Case 5.3: RightFist tracking is inactive after fail-closed"), TrailComp->IsTestTrackingActive(FName(TEXT("RightFist"))));
		TestFalse(TEXT("Case 5.3: LeftFist tracking is inactive after fail-closed"), TrailComp->IsTestTrackingActive(FName(TEXT("LeftFist"))));
		TestNull(TEXT("Case 5.3: RightFist requester token cleared after fail-closed"), TrailComp->GetTestActiveRequester(FName(TEXT("RightFist"))));
		TestNull(TEXT("Case 5.3: LeftFist requester token cleared after fail-closed"), TrailComp->GetTestActiveRequester(FName(TEXT("LeftFist"))));

		if (ActiveFailClosedAbility)
		{
			ActiveFailClosedAbility->EndTestAbility();
		}
		PlayerASC->ClearAbility(FailClosedHandle);

		// Restore test tracking disabled state so Case 9 is unaffected
		TrailComp->SetTestTrackingEnabled(false);
	}

	// -------------------------------------------------------------------------
	// CASE 6: Strict Rejection of Non-Existent Sources, Duplicate/Resolved Sources, and Explicit NAME_None
	// -------------------------------------------------------------------------
	{
		// Restore valid markers
		UMeleeWeaponDefinition* MultiSourceWeapon = NewObject<UMeleeWeaponDefinition>(GetTransientPackage());
		MultiSourceWeapon->HandSlot = EWeaponHandSlot::MainHandOneHanded;
		MultiSourceWeapon->LocomotionMode = EWeaponLocomotionMode::Default;
		MultiSourceWeapon->AttachSocketName = FName(TEXT("Weapon_R"));
		MultiSourceWeapon->bUseOwnerMeshSocketForTrace = true;
		MultiSourceWeapon->DefaultOwnerMeshTraceSourceName = FName(TEXT("RightFist"));

		FOwnerMeshMeleeTraceSource RightSource;
		RightSource.TraceSourceName = FName(TEXT("RightFist"));
		RightSource.OwnerMeshSocketName = FName(TEXT("hand_r"));
		RightSource.BladeBaseMarkerRelativeLocation = FVector(0.0f, -30.0f, 0.0f);
		RightSource.BladeTipMarkerRelativeLocation = FVector(0.0f, -30.0f, 50.0f);

		FOwnerMeshMeleeTraceSource LeftSource;
		LeftSource.TraceSourceName = FName(TEXT("LeftFist"));
		LeftSource.OwnerMeshSocketName = FName(TEXT("hand_l"));
		LeftSource.BladeBaseMarkerRelativeLocation = FVector(0.0f, 30.0f, 0.0f);
		LeftSource.BladeTipMarkerRelativeLocation = FVector(0.0f, 30.0f, 50.0f);

		MultiSourceWeapon->OwnerMeshTraceSources = { RightSource, LeftSource };
		EquipComp->EquipWeapon(MultiSourceWeapon);

		UAbilitySystemComponent* PlayerASC = Player->GetAbilitySystemComponent();

		// 6.1 Non-existent source rejected at Task Activate
		FGameplayAbilitySpec SpecBadSource(UTestMeleeTrailAbility::StaticClass(), 1);
		FGameplayAbilitySpecHandle HandleBadSource = PlayerASC->GiveAbility(SpecBadSource);
		UTestMeleeTrailAbility* AbilityBadSource = Cast<UTestMeleeTrailAbility>(PlayerASC->FindAbilitySpecFromHandle(HandleBadSource)->GetPrimaryInstance());
		if (AbilityBadSource)
		{
			AbilityBadSource->TestTraceSourceNames = { FName(TEXT("NonExistentSource")) };
		}
		PlayerASC->TryActivateAbility(HandleBadSource);
		UTestMeleeTrailAbility* ActiveAbilityBadSource = Cast<UTestMeleeTrailAbility>(PlayerASC->FindAbilitySpecFromHandle(HandleBadSource)->GetPrimaryInstance());
		UAbilityTask_MeleeTraceWindow* TaskBadSource = ActiveAbilityBadSource ? ActiveAbilityBadSource->GetActiveTraceWindowTask() : nullptr;
		TestFalse(TEXT("Case 6.1: Task fails closed immediately when requested source does not exist"), TaskBadSource && TaskBadSource->IsTraceWindowOpen());
		if (ActiveAbilityBadSource)
		{
			ActiveAbilityBadSource->EndTestAbility();
		}
		PlayerASC->ClearAbility(HandleBadSource);

		// 6.2 Explicit NAME_None in non-empty source list rejected
		FGameplayAbilitySpec SpecExplicitNone(UTestMeleeTrailAbility::StaticClass(), 1);
		FGameplayAbilitySpecHandle HandleExplicitNone = PlayerASC->GiveAbility(SpecExplicitNone);
		UTestMeleeTrailAbility* AbilityExplicitNone = Cast<UTestMeleeTrailAbility>(PlayerASC->FindAbilitySpecFromHandle(HandleExplicitNone)->GetPrimaryInstance());
		if (AbilityExplicitNone)
		{
			AbilityExplicitNone->TestTraceSourceNames = { NAME_None, FName(TEXT("RightFist")) };
		}
		PlayerASC->TryActivateAbility(HandleExplicitNone);
		UTestMeleeTrailAbility* ActiveAbilityExplicitNone = Cast<UTestMeleeTrailAbility>(PlayerASC->FindAbilitySpecFromHandle(HandleExplicitNone)->GetPrimaryInstance());
		UAbilityTask_MeleeTraceWindow* TaskExplicitNone = ActiveAbilityExplicitNone ? ActiveAbilityExplicitNone->GetActiveTraceWindowTask() : nullptr;
		TestFalse(TEXT("Case 6.2: Task fails closed immediately when non-empty list contains explicit NAME_None"), TaskExplicitNone && TaskExplicitNone->IsTraceWindowOpen());
		if (ActiveAbilityExplicitNone)
		{
			ActiveAbilityExplicitNone->EndTestAbility();
		}
		PlayerASC->ClearAbility(HandleExplicitNone);

		// 6.3 Duplicate requested source names rejected
		FGameplayAbilitySpec SpecDuplicate(UTestMeleeTrailAbility::StaticClass(), 1);
		FGameplayAbilitySpecHandle HandleDuplicate = PlayerASC->GiveAbility(SpecDuplicate);
		UTestMeleeTrailAbility* AbilityDuplicate = Cast<UTestMeleeTrailAbility>(PlayerASC->FindAbilitySpecFromHandle(HandleDuplicate)->GetPrimaryInstance());
		if (AbilityDuplicate)
		{
			AbilityDuplicate->TestTraceSourceNames = { FName(TEXT("RightFist")), FName(TEXT("RightFist")) };
		}
		PlayerASC->TryActivateAbility(HandleDuplicate);
		UTestMeleeTrailAbility* ActiveAbilityDuplicate = Cast<UTestMeleeTrailAbility>(PlayerASC->FindAbilitySpecFromHandle(HandleDuplicate)->GetPrimaryInstance());
		UAbilityTask_MeleeTraceWindow* TaskDuplicate = ActiveAbilityDuplicate ? ActiveAbilityDuplicate->GetActiveTraceWindowTask() : nullptr;
		TestFalse(TEXT("Case 6.3: Task fails closed immediately when requested sources contain duplicates"), TaskDuplicate && TaskDuplicate->IsTraceWindowOpen());
		if (ActiveAbilityDuplicate)
		{
			ActiveAbilityDuplicate->EndTestAbility();
		}
		PlayerASC->ClearAbility(HandleDuplicate);
	}

	// -------------------------------------------------------------------------
	// CASE 7: MeleeTraceSourceComponent Named Query & Coincident Endpoint Detection
	// -------------------------------------------------------------------------
	{
		TestNotNull(TEXT("Case 7: Player has MeleeTraceSourceComponent"), TraceSourceComp);

		FName ResolvedName = NAME_None;
		TestTrue(TEXT("Case 7: Resolves RightFist"), TraceSourceComp->TryResolveTraceSourceName(FName(TEXT("RightFist")), ResolvedName));
		TestEqual(TEXT("Case 7: Resolved name is RightFist"), ResolvedName, FName(TEXT("RightFist")));

		TestTrue(TEXT("Case 7: Resolves LeftFist"), TraceSourceComp->TryResolveTraceSourceName(FName(TEXT("LeftFist")), ResolvedName));
		TestEqual(TEXT("Case 7: Resolved name is LeftFist"), ResolvedName, FName(TEXT("LeftFist")));

		FName InvalidResolved = NAME_None;
		TestFalse(TEXT("Case 7: Cannot resolve non-existent source"), TraceSourceComp->TryResolveTraceSourceName(FName(TEXT("Invalid")), InvalidResolved));

		FVector BaseLoc = FVector::ZeroVector;
		FVector TipLoc = FVector::ZeroVector;
		TestTrue(TEXT("Case 7: TryGetBladeEndpoints succeeds for RightFist"), TraceSourceComp->TryGetBladeEndpoints(FName(TEXT("RightFist")), BaseLoc, TipLoc));
		TestFalse(TEXT("Case 7: Base and Tip for RightFist are not coincident"), BaseLoc.Equals(TipLoc));

		TestTrue(TEXT("Case 7: TryGetBladeEndpoints succeeds for LeftFist"), TraceSourceComp->TryGetBladeEndpoints(FName(TEXT("LeftFist")), BaseLoc, TipLoc));
		TestFalse(TEXT("Case 7: Base and Tip for LeftFist are not coincident"), BaseLoc.Equals(TipLoc));
	}

	// -------------------------------------------------------------------------
	// CASE 8: Real Ability NotifyState Lifecycle & Stale End-Event Protection
	// -------------------------------------------------------------------------
	{
		UAbilitySystemComponent* PlayerASC = Player->GetAbilitySystemComponent();
		FGameplayAbilitySpec Spec(UTestMeleeTrailAbility::StaticClass(), 1);
		FGameplayAbilitySpecHandle Handle = PlayerASC->GiveAbility(Spec);
		PlayerASC->TryActivateAbility(Handle);

		UTestMeleeTrailAbility* Ability = Cast<UTestMeleeTrailAbility>(PlayerASC->FindAbilitySpecFromHandle(Handle)->GetPrimaryInstance());
		TestNotNull(TEXT("Case 8: Ability activated for notify test"), Ability);

		UAnimNotifyState_AttackTraceWindow* NotifyStateA = NewObject<UAnimNotifyState_AttackTraceWindow>(GetTransientPackage());
		NotifyStateA->TraceSourceNames = { FName(TEXT("RightFist")) };

		UAnimNotifyState_AttackTraceWindow* NotifyStateB = NewObject<UAnimNotifyState_AttackTraceWindow>(GetTransientPackage());
		NotifyStateB->TraceSourceNames = { FName(TEXT("LeftFist")) };

		// NotifyStateA begins -> opens task
		Ability->HandleTestTraceWindowBegin(NotifyStateA);
		TestTrue(TEXT("Case 8: Window opened by NotifyStateA"), Ability->GetActiveTraceWindowTask() && Ability->GetActiveTraceWindowTask()->IsTraceWindowOpen());
		TestEqual(TEXT("Case 8: Active NotifyState is A"), Ability->GetActiveTraceNotifyState(), Cast<const UAnimNotifyState_AttackTraceWindow>(NotifyStateA));

		// Stale NotifyStateB ends -> must NOT close window
		Ability->HandleTestTraceWindowEnd(NotifyStateB);
		TestTrue(TEXT("Case 8: Stale NotifyStateB End does NOT close active window"), Ability->GetActiveTraceWindowTask() && Ability->GetActiveTraceWindowTask()->IsTraceWindowOpen());
		TestEqual(TEXT("Case 8: Active NotifyState remains A"), Ability->GetActiveTraceNotifyState(), Cast<const UAnimNotifyState_AttackTraceWindow>(NotifyStateA));

		// Matching NotifyStateA ends -> successfully closes window
		Ability->HandleTestTraceWindowEnd(NotifyStateA);
		TestNull(TEXT("Case 8: Matching NotifyStateA End closes active window"), Ability->GetActiveTraceWindowTask());
		TestNull(TEXT("Case 8: Active NotifyState cleared"), Ability->GetActiveTraceNotifyState());

		Ability->EndTestAbility();
		PlayerASC->ClearAbility(Handle);
	}

	// -------------------------------------------------------------------------
	// CASE 9: Multi-Source Niagara Trail Component Lifecycle & Seam Separation
	// -------------------------------------------------------------------------
	{
		// 9.1 Silent no-op when asset is null (TestTracking disabled)
		TrailComp->SetTestTrackingEnabled(false);
		TrailComp->SetAsset(nullptr);
		USceneComponent* Requester1 = NewObject<USceneComponent>(Player);
		TrailComp->StartTrail(Requester1, FName(TEXT("LeftFist")), FVector(0.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 100.0f));
		TestFalse(TEXT("Case 9.1: StartTrail is silent no-op when asset is null"), TrailComp->IsActive());
		TrailComp->EndTrail(Requester1, FName(TEXT("LeftFist")));

		// 9.2 Observation Seam Verification (TestTracking enabled -> exercises seam call recording)
		TrailComp->SetTestTrackingEnabled(true);
		const int32 StartBaseline = TrailComp->GetTestStartCallCount(FName(TEXT("RightFist")));
		const int32 EndBaseline = TrailComp->GetTestEndCallCount(FName(TEXT("RightFist")));

		USceneComponent* Requester2 = NewObject<USceneComponent>(Player);
		TrailComp->StartTrail(Requester2, FName(TEXT("RightFist")), FVector(10.0f, 0.0f, 0.0f), FVector(10.0f, 0.0f, 50.0f));
		TestTrue(TEXT("Case 9.2: Test tracking records RightFist as active"), TrailComp->IsTestTrackingActive(FName(TEXT("RightFist"))));
		TestEqual(TEXT("Case 9.2: Start call count incremented from baseline"), TrailComp->GetTestStartCallCount(FName(TEXT("RightFist"))), StartBaseline + 1);

		TrailComp->EndTrail(Requester2, FName(TEXT("RightFist")));
		TestFalse(TEXT("Case 9.2: Test tracking records RightFist as ended"), TrailComp->IsTestTrackingActive(FName(TEXT("RightFist"))));
		TestEqual(TEXT("Case 9.2: End call count incremented from baseline"), TrailComp->GetTestEndCallCount(FName(TEXT("RightFist"))), EndBaseline + 1);
		TrailComp->SetTestTrackingEnabled(false);

		// 9.3 Source-keyed Stale-Token Regression
		TrailComp->SetTestTrackingEnabled(true);
		USceneComponent* RequesterA = NewObject<USceneComponent>(Player);
		USceneComponent* RequesterB = NewObject<USceneComponent>(Player);

		// Step 1: Requester A acquires RightFist
		TrailComp->StartTrail(RequesterA, FName(TEXT("RightFist")), FVector(0.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 50.0f));
		TestTrue(TEXT("Case 9.3: RightFist active for Requester A"), TrailComp->IsTestTrackingActive(FName(TEXT("RightFist"))));
		TestEqual(TEXT("Case 9.3: RightFist requester is A"), TrailComp->GetTestActiveRequester(FName(TEXT("RightFist"))), Cast<const UObject>(RequesterA));

		// Step 2: Requester B takes over RightFist and also acquires LeftFist
		TrailComp->StartTrail(RequesterB, FName(TEXT("RightFist")), FVector(0.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 60.0f));
		TrailComp->StartTrail(RequesterB, FName(TEXT("LeftFist")), FVector(0.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 60.0f));
		TestTrue(TEXT("Case 9.3: RightFist active for Requester B"), TrailComp->IsTestTrackingActive(FName(TEXT("RightFist"))));
		TestEqual(TEXT("Case 9.3: RightFist requester updated to B"), TrailComp->GetTestActiveRequester(FName(TEXT("RightFist"))), Cast<const UObject>(RequesterB));
		TestTrue(TEXT("Case 9.3: LeftFist active for Requester B"), TrailComp->IsTestTrackingActive(FName(TEXT("LeftFist"))));
		TestEqual(TEXT("Case 9.3: LeftFist requester is B"), TrailComp->GetTestActiveRequester(FName(TEXT("LeftFist"))), Cast<const UObject>(RequesterB));

		// Step 3: Requester A issues late EndTrail for RightFist -> Must NOT affect Requester B's RightFist or LeftFist
		TrailComp->EndTrail(RequesterA, FName(TEXT("RightFist")));
		TestTrue(TEXT("Case 9.3: Late EndTrail from A does NOT end RightFist for B"), TrailComp->IsTestTrackingActive(FName(TEXT("RightFist"))));
		TestEqual(TEXT("Case 9.3: RightFist requester remains B after late A end"), TrailComp->GetTestActiveRequester(FName(TEXT("RightFist"))), Cast<const UObject>(RequesterB));
		TestTrue(TEXT("Case 9.3: LeftFist remains active for B"), TrailComp->IsTestTrackingActive(FName(TEXT("LeftFist"))));
		TestEqual(TEXT("Case 9.3: LeftFist requester remains B"), TrailComp->GetTestActiveRequester(FName(TEXT("LeftFist"))), Cast<const UObject>(RequesterB));

		// Step 4: Requester B normally ends both sources -> Both sources cleaned up
		TrailComp->EndTrail(RequesterB, FName(TEXT("RightFist")));
		TrailComp->EndTrail(RequesterB, FName(TEXT("LeftFist")));
		TestFalse(TEXT("Case 9.3: RightFist cleaned up after B ends"), TrailComp->IsTestTrackingActive(FName(TEXT("RightFist"))));
		TestNull(TEXT("Case 9.3: RightFist requester cleared"), TrailComp->GetTestActiveRequester(FName(TEXT("RightFist"))));
		TestFalse(TEXT("Case 9.3: LeftFist cleaned up after B ends"), TrailComp->IsTestTrackingActive(FName(TEXT("LeftFist"))));
		TestNull(TEXT("Case 9.3: LeftFist requester cleared"), TrailComp->GetTestActiveRequester(FName(TEXT("LeftFist"))));

		TrailComp->SetTestTrackingEnabled(false);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
