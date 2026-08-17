#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/Abilities/PrimaryAttackAbility.h"
#include "AbilitySystem/Abilities/PlayerGuardAbility.h"
#include "AbilitySystem/Abilities/PlayerGuardBreakAbility.h"
#include "Character/Player/PlayerCharacter.h"
#include "Combat/Input/CombatLoadoutDefinition.h"
#include "Combat/Equipment/MeleeWeaponDefinition.h"
#include "Combat/Equipment/OffHandWeaponDefinition.h"
#include "Combat/Equipment/WeaponEquipmentComponent.h"
#include "Combat/Equipment/WorldWeaponPickup.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponEquipmentComponentTransactionMatrixTest, "PolyQuest.Equipment.TransactionMatrix", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWeaponEquipmentComponentTransactionMatrixTest::RunTest(const FString& Parameters)
{
	// 1. Setup Test World with valid WorldContext
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("WeaponEquipmentTestWorld"));
	WorldContext.SetCurrentWorld(World);
	if (!TestNotNull(TEXT("Test World created successfully"), World))
	{
		return false;
	}

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

	// Spawn test floor with collision for visibility projection
	AStaticMeshActor* FloorActor = World->SpawnActor<AStaticMeshActor>();
	UStaticMesh* CubeMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	if (FloorActor && CubeMesh)
	{
		FloorActor->GetStaticMeshComponent()->SetStaticMesh(CubeMesh);
		FloorActor->SetActorLocation(FVector(0.0f, 0.0f, -50.0f));
		FloorActor->SetActorScale3D(FVector(20.0f, 20.0f, 1.0f));
		FloorActor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
	}

	// Load real project fixtures
	USkeletalMesh* HeroMesh = Cast<USkeletalMesh>(StaticLoadObject(USkeletalMesh::StaticClass(), nullptr, TEXT("/Game/PolygonDungeons/Meshes/Characters/SK_Character_Hero_Knight_Male")));
	UStaticMesh* SwordMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, TEXT("/Game/PolygonDungeons/Meshes/Weapons/SM_Wep_Straightsword_01")));
	UStaticMesh* ShieldMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, TEXT("/Game/PolygonDungeons/Meshes/Weapons/SM_Wep_Shield_Round_01")));

	TestNotNull(TEXT("Hero skeletal mesh loaded"), HeroMesh);
	TestNotNull(TEXT("Sword static mesh loaded"), SwordMesh);
	TestNotNull(TEXT("Shield static mesh loaded"), ShieldMesh);

	// Create test loadouts
	UCombatLoadoutDefinition* SwordLoadout = NewObject<UCombatLoadoutDefinition>(GetTransientPackage(), TEXT("Test_SwordLoadout"));
	UCombatLoadoutDefinition* TwoHandedLoadout = NewObject<UCombatLoadoutDefinition>(GetTransientPackage(), TEXT("Test_TwoHandedLoadout"));

	// Create test definitions
	UMeleeWeaponDefinition* SwordDef = NewObject<UMeleeWeaponDefinition>(GetTransientPackage(), TEXT("Test_Sword"));
	SwordDef->HandSlot = EWeaponHandSlot::MainHandOneHanded;
	SwordDef->AttachSocketName = TEXT("Weapon_R");
	SwordDef->WeaponMesh = SwordMesh;
	SwordDef->AssociatedLoadout = SwordLoadout;
	SwordDef->BaseGrantedActions.Add(UPrimaryAttackAbility::StaticClass());
	SwordDef->ExclusiveCombatActions.Add(UPlayerGuardAbility::StaticClass());
	SwordDef->DefaultPreparedActions.Add(UPlayerGuardAbility::StaticClass());
	SwordDef->bUseOwnerMeshSocketForTrace = false;
	SwordDef->BladeBaseMarkerRelativeLocation = FVector(0, 0, 10);
	SwordDef->BladeTipMarkerRelativeLocation = FVector(0, 0, 100);
	SwordDef->TraceRadius = 10.0f;
	SwordDef->BladeSubdivisions = 4;

	UMeleeWeaponDefinition* TwoHandedDef = NewObject<UMeleeWeaponDefinition>(GetTransientPackage(), TEXT("Test_TwoHanded"));
	TwoHandedDef->HandSlot = EWeaponHandSlot::MainHandTwoHanded;
	TwoHandedDef->AttachSocketName = TEXT("Weapon_R");
	TwoHandedDef->WeaponMesh = SwordMesh;
	TwoHandedDef->AssociatedLoadout = TwoHandedLoadout;
	TwoHandedDef->BaseGrantedActions.Add(UPrimaryAttackAbility::StaticClass());
	TwoHandedDef->ExclusiveCombatActions.Add(UPlayerGuardBreakAbility::StaticClass());
	TwoHandedDef->DefaultPreparedActions.Add(UPlayerGuardBreakAbility::StaticClass());
	TwoHandedDef->bUseOwnerMeshSocketForTrace = false;
	TwoHandedDef->BladeBaseMarkerRelativeLocation = FVector(0, 0, 10);
	TwoHandedDef->BladeTipMarkerRelativeLocation = FVector(0, 0, 150);
	TwoHandedDef->TraceRadius = 15.0f;
	TwoHandedDef->BladeSubdivisions = 4;

	UMeleeWeaponDefinition* UnarmedDef = NewObject<UMeleeWeaponDefinition>(GetTransientPackage(), TEXT("Test_Unarmed"));
	UnarmedDef->HandSlot = EWeaponHandSlot::MainHandOneHanded;
	UnarmedDef->AttachSocketName = TEXT("Weapon_R");
	UnarmedDef->WeaponMesh = nullptr;
	UnarmedDef->bUseOwnerMeshSocketForTrace = true;
	UnarmedDef->BladeBaseMarkerRelativeLocation = FVector(0, 0, 5);
	UnarmedDef->BladeTipMarkerRelativeLocation = FVector(0, 0, 20);
	UnarmedDef->TraceRadius = 8.0f;
	UnarmedDef->BladeSubdivisions = 2;

	UOffHandWeaponDefinition* ShieldDef = NewObject<UOffHandWeaponDefinition>(GetTransientPackage(), TEXT("Test_Shield"));
	ShieldDef->HandSlot = EWeaponHandSlot::OffHand;
	ShieldDef->AttachSocketName = TEXT("Weapon_L");
	ShieldDef->WeaponMesh = ShieldMesh;

	// Spawn test player and initialize mesh
	APlayerCharacter* Player = World->SpawnActor<APlayerCharacter>();
	TestNotNull(TEXT("Spawned test player"), Player);
	if (Player)
	{
		Player->SetActorLocation(FVector(0.0f, 0.0f, 50.0f));
		if (Player->GetMesh() && HeroMesh)
		{
			Player->GetMesh()->SetSkeletalMeshAsset(HeroMesh);
		}
	}

	UWeaponEquipmentComponent* EquipmentComp = Player ? Player->FindComponentByClass<UWeaponEquipmentComponent>() : nullptr;
	TestNotNull(TEXT("Equipment component on player"), EquipmentComp);

	if (!EquipmentComp || !Player)
	{
		return false;
	}

	// Wire Unarmed fallback
	if (FProperty* Prop = EquipmentComp->GetClass()->FindPropertyByName(TEXT("UnarmedFallbackDefinition")))
	{
		if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Prop))
		{
			ObjProp->SetObjectPropertyValue_InContainer(EquipmentComp, UnarmedDef);
		}
	}

	// Helper to count and find active dropped pickups in world
	auto GetWorldDroppedPickups = [World](TArray<AWorldWeaponPickup*>& OutPickups)
	{
		OutPickups.Reset();
		for (TActorIterator<AWorldWeaponPickup> It(World); It; ++It)
		{
			AWorldWeaponPickup* Pickup = *It;
			if (Pickup && !Pickup->IsActorBeingDestroyed())
			{
				OutPickups.Add(Pickup);
			}
		}
	};

	// 2. Test Target Composition Construction & Conversions (Pure calculation)
	{
		// 2.1 OneHanded
		UWeaponDefinition* TargetMain = nullptr;
		UWeaponDefinition* TargetOff = nullptr;
		FString Reason;
		bool bBuilt = EquipmentComp->BuildTargetCompositionForIncoming(SwordDef, TargetMain, TargetOff, Reason);
		TestTrue(TEXT("BuildTargetComposition for OneHanded succeeds"), bBuilt);
		TestEqual(TEXT("Target main is Sword"), TargetMain, Cast<UWeaponDefinition>(SwordDef));
		TestNull(TEXT("Target off is null initially"), TargetOff);

		// 2.2 TwoHanded conversion (clears offhand)
		bBuilt = EquipmentComp->BuildTargetCompositionForIncoming(TwoHandedDef, TargetMain, TargetOff, Reason);
		TestTrue(TEXT("BuildTargetComposition for TwoHanded succeeds"), bBuilt);
		TestEqual(TEXT("Target main is TwoHanded"), TargetMain, Cast<UWeaponDefinition>(TwoHandedDef));
		TestNull(TEXT("Target off is null for TwoHanded"), TargetOff);

		// 2.3 TwoHanded -> Shield conversion (converts main hand to Unarmed fallback)
		EquipmentComp->EquipWeapon(TwoHandedDef);
		bBuilt = EquipmentComp->BuildTargetCompositionForIncoming(ShieldDef, TargetMain, TargetOff, Reason);
		TestTrue(TEXT("BuildTargetComposition for Shield while holding TwoHanded succeeds"), bBuilt);
		TestEqual(TEXT("Target main is converted to Unarmed fallback"), TargetMain, Cast<UWeaponDefinition>(UnarmedDef));
		TestEqual(TEXT("Target off is Shield"), TargetOff, Cast<UWeaponDefinition>(ShieldDef));
	}

	// 3. Test Displaced Definitions Calculation
	{
		TArray<UWeaponDefinition*> Displaced;

		// 3.1 Standard 1H -> 1H swap
		EquipmentComp->CalculateDisplacedDefinitions(SwordDef, nullptr, TwoHandedDef, nullptr, Displaced);
		TestEqual(TEXT("Displaced count for 1H -> 2H is 1"), Displaced.Num(), 1);
		TestTrue(TEXT("Displaced contains Sword"), Displaced.Contains(SwordDef));

		// 3.2 2H + Shield composition swap: {Sword, Shield} -> {TwoHanded, null}
		EquipmentComp->CalculateDisplacedDefinitions(SwordDef, ShieldDef, TwoHandedDef, nullptr, Displaced);
		TestEqual(TEXT("Displaced count for {Sword, Shield} -> 2H is 2"), Displaced.Num(), 2);
		TestTrue(TEXT("Displaced contains Sword"), Displaced.Contains(SwordDef));
		TestTrue(TEXT("Displaced contains Shield"), Displaced.Contains(ShieldDef));

		// 3.3 TwoHanded -> Shield swap: {TwoHanded, null} -> {Unarmed, Shield}
		EquipmentComp->CalculateDisplacedDefinitions(TwoHandedDef, nullptr, UnarmedDef, ShieldDef, Displaced);
		TestEqual(TEXT("Displaced count for 2H -> Shield is 1"), Displaced.Num(), 1);
		TestTrue(TEXT("Displaced contains TwoHanded"), Displaced.Contains(TwoHandedDef));

		// 3.4 Unarmed fallback is NEVER displaced
		EquipmentComp->CalculateDisplacedDefinitions(UnarmedDef, ShieldDef, SwordDef, ShieldDef, Displaced);
		TestEqual(TEXT("Unarmed fallback is not displaced when replaced by Sword"), Displaced.Num(), 0);
	}

	// 4. Test Direct EquipWeapon Two-Directional Conflict Rejection (Zero Changes)
	{
		// Direction 1: Holding { Sword, Shield }, direct EquipWeapon(TwoHanded) must reject with zero changes via RunPreflight
		TestTrue(TEXT("Equip Sword succeeded"), EquipmentComp->EquipWeapon(SwordDef));
		TestTrue(TEXT("Equip Shield succeeded"), EquipmentComp->EquipWeapon(ShieldDef));
		TestEqual(TEXT("Equipped main hand is Sword"), EquipmentComp->GetCurrentMainHandWeapon(), Cast<UWeaponDefinition>(SwordDef));
		TestEqual(TEXT("Equipped off hand is Shield"), EquipmentComp->GetCurrentOffHandWeapon(), Cast<UWeaponDefinition>(ShieldDef));
		TestEqual(TEXT("Equipped active loadout is SwordLoadout"), Player->GetActiveCombatLoadout(), SwordLoadout);

		FString PreflightReason1;
		const bool bPreflight1 = EquipmentComp->TestDirectPreflight(TwoHandedDef, ShieldDef, PreflightReason1);
		TestFalse(TEXT("Direct preflight rejects {TwoHanded, Shield}"), bPreflight1);
		TestTrue(TEXT("Preflight reason explains TwoHanded conflict"), PreflightReason1.Contains(TEXT("TwoHanded")));

		const bool bEquip2HResult = EquipmentComp->EquipWeapon(TwoHandedDef);
		TestFalse(TEXT("Direct EquipWeapon(TwoHanded) while holding Shield fails"), bEquip2HResult);
		TestEqual(TEXT("Main hand remains Sword on conflict rejection"), EquipmentComp->GetCurrentMainHandWeapon(), Cast<UWeaponDefinition>(SwordDef));
		TestEqual(TEXT("Off hand remains Shield on conflict rejection"), EquipmentComp->GetCurrentOffHandWeapon(), Cast<UWeaponDefinition>(ShieldDef));
		TestEqual(TEXT("Active loadout remains SwordLoadout on conflict rejection"), Player->GetActiveCombatLoadout(), SwordLoadout);

		// Direction 2: Holding { TwoHanded, null }, direct EquipWeapon(Shield) must reject with zero changes via RunPreflight
		AWorldWeaponPickup* TwoHandedSetupPickup = World->SpawnActor<AWorldWeaponPickup>();
		TwoHandedSetupPickup->SetActorLocation(FVector(50.0f, 0.0f, 0.0f));
		TwoHandedSetupPickup->SetWeaponDefinition(TwoHandedDef);
		TestTrue(TEXT("Setup transition to TwoHanded via world pickup"), EquipmentComp->TryEquipWorldPickup(TwoHandedSetupPickup));
		TestEqual(TEXT("Setup main hand is TwoHanded"), EquipmentComp->GetCurrentMainHandWeapon(), Cast<UWeaponDefinition>(TwoHandedDef));
		TestNull(TEXT("Setup off hand is null"), EquipmentComp->GetCurrentOffHandWeapon());
		TestEqual(TEXT("Active loadout transitioned to TwoHandedLoadout"), Player->GetActiveCombatLoadout(), TwoHandedLoadout);

		// Clean up dropped setup pickups from world
		TArray<AWorldWeaponPickup*> SetupDrops;
		GetWorldDroppedPickups(SetupDrops);
		for (AWorldWeaponPickup* Drop : SetupDrops)
		{
			Drop->Destroy();
		}

		FString PreflightReason2;
		const bool bPreflight2 = EquipmentComp->TestDirectPreflight(TwoHandedDef, ShieldDef, PreflightReason2);
		TestFalse(TEXT("Direct preflight rejects {TwoHanded, Shield}"), bPreflight2);
		TestTrue(TEXT("Preflight reason explains TwoHanded conflict"), PreflightReason2.Contains(TEXT("TwoHanded")));

		const bool bEquipShieldResult = EquipmentComp->EquipWeapon(ShieldDef);
		TestFalse(TEXT("Direct EquipWeapon(Shield) while holding TwoHanded fails"), bEquipShieldResult);
		TestEqual(TEXT("Main hand remains TwoHanded on conflict rejection"), EquipmentComp->GetCurrentMainHandWeapon(), Cast<UWeaponDefinition>(TwoHandedDef));
		TestNull(TEXT("Off hand remains null on conflict rejection"), EquipmentComp->GetCurrentOffHandWeapon());
		TestEqual(TEXT("Active loadout remains TwoHandedLoadout on conflict rejection"), Player->GetActiveCombatLoadout(), TwoHandedLoadout);
	}

	// 5. Test World Pickup Full Success Transactions (Normalizations and Drop Spawns)
	{
		// 5.1 {Sword, Shield} + Pickup(TwoHanded) -> {TwoHanded, null} + Displaces {Sword, Shield}
		EquipmentComp->EquipWeapon(SwordDef);
		EquipmentComp->EquipWeapon(ShieldDef);

		AWorldWeaponPickup* TwoHandedPickup = World->SpawnActor<AWorldWeaponPickup>();
		TestNotNull(TEXT("Spawned TwoHanded pickup"), TwoHandedPickup);
		TwoHandedPickup->SetActorLocation(FVector(50.0f, 0.0f, 0.0f));
		TwoHandedPickup->SetWeaponDefinition(TwoHandedDef);

		const bool bWorldEquip2H = EquipmentComp->TryEquipWorldPickup(TwoHandedPickup);
		TestTrue(TEXT("TryEquipWorldPickup(TwoHanded) succeeds"), bWorldEquip2H);
		TestEqual(TEXT("Active main hand is now TwoHanded"), EquipmentComp->GetCurrentMainHandWeapon(), Cast<UWeaponDefinition>(TwoHandedDef));
		TestNull(TEXT("Active off hand is now cleared"), EquipmentComp->GetCurrentOffHandWeapon());
		TestEqual(TEXT("Active combat loadout is now TwoHandedLoadout"), Player->GetActiveCombatLoadout(), TwoHandedLoadout);
		FString TwoHandedDiag;
		TestTrue(TEXT("Slot 0 binding updated to UPlayerGuardBreakAbility for TwoHanded"), EquipmentComp->VerifyPreparedSlotBinding(0, UPlayerGuardBreakAbility::StaticClass(), TwoHandedDiag));
		TestTrue(TEXT("Source TwoHanded pickup was consumed/destroyed"), TwoHandedPickup->IsActorBeingDestroyed());

		// Verify that exactly 2 dropped pickups exist in world: Sword and Shield
		TArray<AWorldWeaponPickup*> Drops51;
		GetWorldDroppedPickups(Drops51);
		TestEqual(TEXT("Exactly 2 displaced pickups dropped in world"), Drops51.Num(), 2);

		bool bFoundDroppedSword = false;
		bool bFoundDroppedShield = false;
		for (AWorldWeaponPickup* Drop : Drops51)
		{
			if (Drop->GetWeaponDefinition() == SwordDef)
			{
				bFoundDroppedSword = true;
				TestFalse(TEXT("Dropped sword rejects former owner during 0.5s cooldown"), Drop->CanInteract(Player));
			}
			else if (Drop->GetWeaponDefinition() == ShieldDef)
			{
				bFoundDroppedShield = true;
				TestFalse(TEXT("Dropped shield rejects former owner during 0.5s cooldown"), Drop->CanInteract(Player));
			}
			Drop->Destroy();
		}
		TestTrue(TEXT("Dropped sword was generated in world"), bFoundDroppedSword);
		TestTrue(TEXT("Dropped shield was generated in world"), bFoundDroppedShield);

		// 5.2 {TwoHanded, null} + Pickup(Shield) -> {Unarmed, Shield} + Displaces {TwoHanded}
		AWorldWeaponPickup* ShieldPickup = World->SpawnActor<AWorldWeaponPickup>();
		TestNotNull(TEXT("Spawned Shield pickup"), ShieldPickup);
		ShieldPickup->SetActorLocation(FVector(50.0f, 0.0f, 0.0f));
		ShieldPickup->SetWeaponDefinition(ShieldDef);

		const bool bWorldEquipShield = EquipmentComp->TryEquipWorldPickup(ShieldPickup);
		TestTrue(TEXT("TryEquipWorldPickup(Shield) succeeds"), bWorldEquipShield);
		TestEqual(TEXT("Active main hand converted to Unarmed"), EquipmentComp->GetCurrentMainHandWeapon(), Cast<UWeaponDefinition>(UnarmedDef));
		TestEqual(TEXT("Active off hand is now Shield"), EquipmentComp->GetCurrentOffHandWeapon(), Cast<UWeaponDefinition>(ShieldDef));
		FString UnarmedDiag;
		TestTrue(TEXT("Slot 0 binding cleared for Unarmed fallback"), EquipmentComp->VerifyPreparedSlotBinding(0, nullptr, UnarmedDiag));
		TestTrue(TEXT("Source Shield pickup was consumed/destroyed"), ShieldPickup->IsActorBeingDestroyed());

		// Verify that exactly 1 dropped pickup exists in world: TwoHanded
		TArray<AWorldWeaponPickup*> Drops52;
		GetWorldDroppedPickups(Drops52);
		TestEqual(TEXT("Exactly 1 displaced pickup dropped in world for 2H"), Drops52.Num(), 1);
		if (Drops52.Num() > 0)
		{
			TestEqual(TEXT("Dropped pickup is TwoHanded"), Drops52[0]->GetWeaponDefinition(), Cast<UWeaponDefinition>(TwoHandedDef));
			Drops52[0]->Destroy();
		}
	}

	// 6. Test World Pickup Injected Failure Rollbacks & Source Pickup Retention
	{
		// Reset to { Sword, Shield }
		EquipmentComp->EquipWeapon(SwordDef);
		EquipmentComp->EquipWeapon(ShieldDef);

		FString InitialDiag;
		TestTrue(TEXT("Initial slot 0 binding is UPlayerGuardAbility"), EquipmentComp->VerifyPreparedSlotBinding(0, UPlayerGuardAbility::StaticClass(), InitialDiag));

		// 6.1 Injected Apply Failure
		AWorldWeaponPickup* FailApplyPickup = World->SpawnActor<AWorldWeaponPickup>();
		FailApplyPickup->SetActorLocation(FVector(50.0f, 0.0f, 0.0f));
		FailApplyPickup->SetWeaponDefinition(TwoHandedDef);

		EquipmentComp->SetInjectApplyFailureOnce(true);
		const bool bApplyFailResult = EquipmentComp->TryEquipWorldPickup(FailApplyPickup);
		TestFalse(TEXT("TryEquipWorldPickup with injected apply failure returns false"), bApplyFailResult);
		TestEqual(TEXT("Main hand identity restored to Sword on apply failure"), EquipmentComp->GetCurrentMainHandWeapon(), Cast<UWeaponDefinition>(SwordDef));
		TestEqual(TEXT("Off hand identity restored to Shield on apply failure"), EquipmentComp->GetCurrentOffHandWeapon(), Cast<UWeaponDefinition>(ShieldDef));
		TestEqual(TEXT("Loadout restored to SwordLoadout on apply failure"), Player->GetActiveCombatLoadout(), SwordLoadout);
		FString ApplyFailDiag;
		TestTrue(TEXT("Slot 0 binding restored to UPlayerGuardAbility on apply failure"), EquipmentComp->VerifyPreparedSlotBinding(0, UPlayerGuardAbility::StaticClass(), ApplyFailDiag));
		TestFalse(TEXT("Source pickup is NOT destroyed on apply failure"), FailApplyPickup->IsActorBeingDestroyed());

		// Assert no leaked dropped pickups on apply failure
		TArray<AWorldWeaponPickup*> Drops61;
		GetWorldDroppedPickups(Drops61);
		// (Only FailApplyPickup should exist)
		TestEqual(TEXT("No dropped pickups created on apply failure"), Drops61.Num(), 1);

		FailApplyPickup->Destroy();

		// 6.2 Injected Drop Failure
		AWorldWeaponPickup* FailDropPickup = World->SpawnActor<AWorldWeaponPickup>();
		FailDropPickup->SetActorLocation(FVector(50.0f, 0.0f, 0.0f));
		FailDropPickup->SetWeaponDefinition(TwoHandedDef);

		EquipmentComp->SetInjectDropFailureOnce(true);
		const bool bDropFailResult = EquipmentComp->TryEquipWorldPickup(FailDropPickup);
		TestFalse(TEXT("TryEquipWorldPickup with injected drop failure returns false"), bDropFailResult);
		TestEqual(TEXT("Main hand identity restored to Sword on drop failure"), EquipmentComp->GetCurrentMainHandWeapon(), Cast<UWeaponDefinition>(SwordDef));
		TestEqual(TEXT("Off hand identity restored to Shield on drop failure"), EquipmentComp->GetCurrentOffHandWeapon(), Cast<UWeaponDefinition>(ShieldDef));
		TestEqual(TEXT("Loadout restored to SwordLoadout on drop failure"), Player->GetActiveCombatLoadout(), SwordLoadout);
		FString DropFailDiag;
		TestTrue(TEXT("Slot 0 binding restored to UPlayerGuardAbility on drop failure"), EquipmentComp->VerifyPreparedSlotBinding(0, UPlayerGuardAbility::StaticClass(), DropFailDiag));
		TestFalse(TEXT("Source pickup is NOT destroyed on drop failure"), FailDropPickup->IsActorBeingDestroyed());

		// Assert all provisional drops were destroyed on drop failure
		TArray<AWorldWeaponPickup*> Drops62;
		GetWorldDroppedPickups(Drops62);
		TestEqual(TEXT("All provisional drops destroyed on drop failure"), Drops62.Num(), 1);

		FailDropPickup->Destroy();
	}

	// 7. Test State.Status.Dead Gate
	{
		const FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
		UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
		if (ASC && DeadTag.IsValid())
		{
			ASC->AddLooseGameplayTag(DeadTag);

			AWorldWeaponPickup* DeadTestPickup = World->SpawnActor<AWorldWeaponPickup>();
			DeadTestPickup->SetActorLocation(FVector(50.0f, 0.0f, 0.0f));
			DeadTestPickup->SetWeaponDefinition(SwordDef);

			TestFalse(TEXT("Dead player cannot interact with world pickup"), DeadTestPickup->CanInteract(Player));
			TestFalse(TEXT("Dead player cannot direct equip weapon"), EquipmentComp->EquipWeapon(SwordDef));
			TestFalse(TEXT("Dead player cannot try equip world pickup"), EquipmentComp->TryEquipWorldPickup(DeadTestPickup));

			DeadTestPickup->Destroy();
			ASC->RemoveLooseGameplayTag(DeadTag);
		}
	}

	// 8. Test TryEquipWorldPickup Same-Definition No-Op Rejection
	{
		EquipmentComp->EquipWeapon(SwordDef);
		AWorldWeaponPickup* SameMainPickup = World->SpawnActor<AWorldWeaponPickup>();
		TestNotNull(TEXT("Spawned same-definition test pickup"), SameMainPickup);
		SameMainPickup->SetActorLocation(FVector(50.0f, 0.0f, 0.0f));
		SameMainPickup->SetWeaponDefinition(SwordDef);

		const bool bSameEquipResult = EquipmentComp->TryEquipWorldPickup(SameMainPickup);
		TestFalse(TEXT("TryEquipWorldPickup with same definition returns false (no-op)"), bSameEquipResult);
		TestFalse(TEXT("Same definition source pickup is not destroyed"), SameMainPickup->IsActorBeingDestroyed());

		SameMainPickup->Destroy();
	}

	// 9. Test FormerOwner Rejection on AWorldWeaponPickup
	{
		AWorldWeaponPickup* TestPickup = World->SpawnActor<AWorldWeaponPickup>();
		TestNotNull(TEXT("Spawned test pickup"), TestPickup);

		TestPickup->SetWeaponDefinition(SwordDef);
		TestTrue(TEXT("Can interact without requester returns false"), TestPickup->CanInteract(nullptr) == false);

		TestTrue(TEXT("Can interact with player when no former owner"), TestPickup->CanInteract(Player));

		// Initialize as dropped pickup with 0.5s rejection
		TestPickup->InitializeDroppedPickup(SwordDef, Player, 0.5f);
		TestFalse(TEXT("Former owner rejected during cooldown"), TestPickup->CanInteract(Player));

		TestPickup->Destroy();
	}

	// 10. Test Prepared Slot Activation Validation Checks
	{
		// Invalid index
		TestFalse(TEXT("Slot -1 rejected"), EquipmentComp->TryActivatePreparedSlot(-1));
		TestFalse(TEXT("Slot 4 rejected (out of bounds)"), EquipmentComp->TryActivatePreparedSlot(4));

		// Empty slot (no class configured)
		TestFalse(TEXT("Unconfigured slot 0 returns false without crash"), EquipmentComp->TryActivatePreparedSlot(0));
	}

	// 11. Test Keep-If-Compatible Layout Rebuild
	{
		TArray<TSubclassOf<UGameplayAbility>> PreparedLayout;
		EquipmentComp->ComputeKeepIfCompatibleLayout(SwordDef, ShieldDef, PreparedLayout);
		TestEqual(TEXT("Prepared layout has 4 slots"), PreparedLayout.Num(), 4);
	}

	Player->Destroy();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
