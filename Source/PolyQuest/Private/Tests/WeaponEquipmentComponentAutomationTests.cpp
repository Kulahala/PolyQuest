#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/Abilities/BowDrawFireAbility.h"
#include "AbilitySystem/Abilities/PrimaryAttackAbility.h"
#include "AbilitySystem/Abilities/PlayerGuardAbility.h"
#include "AbilitySystem/Abilities/PlayerGuardBreakAbility.h"
#include "AbilitySystem/Abilities/PlayerParryAbility.h"
#include "AbilitySystem/Abilities/SprintAttackAbility.h"
#include "Character/Player/PlayerCharacter.h"
#include "Combat/Input/CombatLoadoutDefinition.h"
#include "Combat/Equipment/BowWeaponDefinition.h"
#include "Combat/Equipment/DefenseProfileDefinition.h"
#include "Combat/Equipment/MeleeWeaponDefinition.h"
#include "Combat/Equipment/OffHandWeaponDefinition.h"
#include "Combat/Equipment/ProjectileDefinition.h"
#include "Combat/Equipment/WeaponEquipmentComponent.h"
#include "Combat/Equipment/WorldWeaponPickup.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Tests/TestProjectileDamageGE.h"

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
	UStaticMesh* SwordMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, TEXT("/Game/PolygonDungeons/Meshes/Weapons/SM_Wep_Ornate_Sword_02")));
	UStaticMesh* ShieldMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, TEXT("/Game/PolygonDungeons/Meshes/Weapons/SM_Wep_Shield_Round_01")));

	TestNotNull(TEXT("Hero skeletal mesh loaded"), HeroMesh);
	TestNotNull(TEXT("Sword static mesh loaded"), SwordMesh);
	TestNotNull(TEXT("Shield static mesh loaded"), ShieldMesh);

	const FName SocketNameTraceBase(TEXT("Trace_Base"));
	const FName SocketNameTraceTip(TEXT("Trace_Tip"));
	TestNotNull(TEXT("SwordMesh contains socket Trace_Base"), SwordMesh ? SwordMesh->FindSocket(SocketNameTraceBase) : nullptr);
	TestNotNull(TEXT("SwordMesh contains socket Trace_Tip"), SwordMesh ? SwordMesh->FindSocket(SocketNameTraceTip) : nullptr);

	// Create test loadouts
	const FGameplayTag TagInputPrimaryAttack = FGameplayTag::RequestGameplayTag(TEXT("Input.PrimaryAttack"));
	const FGameplayTag TagAbilityPrimaryAttack = FGameplayTag::RequestGameplayTag(TEXT("Ability.Attack.Primary"));
	const FGameplayTag TagAbilitySprintAttack = FGameplayTag::RequestGameplayTag(TEXT("Ability.Attack.Sprint"));
	const FGameplayTag TagBowDrawReady = FGameplayTag::RequestGameplayTag(TEXT("Event.Attack.Bow.DrawReady"), false);
	const FGameplayTag TagBowRelease = FGameplayTag::RequestGameplayTag(TEXT("Event.Attack.Bow.Release"), false);
	TestTrue(TEXT("Bow DrawReady event tag is registered"), TagBowDrawReady.IsValid());
	TestTrue(TEXT("Bow Release event tag is registered"), TagBowRelease.IsValid());

	UCombatLoadoutDefinition* SwordLoadout = NewObject<UCombatLoadoutDefinition>(GetTransientPackage(), TEXT("Test_SwordLoadout"));
	SwordLoadout->AddTestInputAbilityRoute(TagInputPrimaryAttack, TagAbilityPrimaryAttack);

	UCombatLoadoutDefinition* TwoHandedLoadout = NewObject<UCombatLoadoutDefinition>(GetTransientPackage(), TEXT("Test_TwoHandedLoadout"));
	TwoHandedLoadout->AddTestInputAbilityRoute(TagInputPrimaryAttack, TagAbilityPrimaryAttack);

	// Create test definitions
	UMeleeWeaponDefinition* SwordDef = NewObject<UMeleeWeaponDefinition>(GetTransientPackage(), TEXT("Test_Sword"));
	SwordDef->HandSlot = EWeaponHandSlot::MainHandOneHanded;
	SwordDef->LocomotionMode = EWeaponLocomotionMode::LightSword;
	SwordDef->AttachSocketName = TEXT("Weapon_R");
	SwordDef->WeaponMesh = SwordMesh;
	SwordDef->BladeBaseSocketName = SocketNameTraceBase;
	SwordDef->BladeTipSocketName = SocketNameTraceTip;
	SwordDef->PrimaryAttackAbilityTag = TagAbilityPrimaryAttack;
	SwordDef->AssociatedLoadout = SwordLoadout;
	SwordDef->BaseGrantedActions.Add(UPrimaryAttackAbility::StaticClass());
	SwordDef->ExclusiveCombatActions.Add(UPlayerGuardAbility::StaticClass());
	SwordDef->DefaultPreparedActions.Add(UPlayerGuardAbility::StaticClass());
	SwordDef->bUseOwnerMeshSocketForTrace = false;
	SwordDef->BladeBaseMarkerRelativeLocation = FVector(0, 0, 10);
	SwordDef->BladeTipMarkerRelativeLocation = FVector(0, 0, 100);
	SwordDef->TraceRadius = 10.0f;
	SwordDef->BladeSubdivisions = 4;
	SwordDef->DisplayScale = FVector(1.25f, 0.75f, 1.5f);
	SwordDef->WorldPickupDisplayTransform = FTransform(FRotator(0.0f, 90.0f, 90.0f), FVector(0.0f, 0.0f, 5.0f), FVector(1.0f));

	UMeleeWeaponDefinition* TwoHandedDef = NewObject<UMeleeWeaponDefinition>(GetTransientPackage(), TEXT("Test_TwoHanded"));
	TwoHandedDef->HandSlot = EWeaponHandSlot::MainHandTwoHanded;
	TwoHandedDef->LocomotionMode = EWeaponLocomotionMode::Default;
	TwoHandedDef->AttachSocketName = TEXT("Weapon_R");
	TwoHandedDef->WeaponMesh = SwordMesh;
	TwoHandedDef->BladeBaseSocketName = SocketNameTraceBase;
	TwoHandedDef->BladeTipSocketName = SocketNameTraceTip;
	TwoHandedDef->PrimaryAttackAbilityTag = TagAbilityPrimaryAttack;
	TwoHandedDef->AssociatedLoadout = TwoHandedLoadout;
	TwoHandedDef->BaseGrantedActions.Add(UPrimaryAttackAbility::StaticClass());
	TwoHandedDef->ExclusiveCombatActions.Add(UPlayerGuardBreakAbility::StaticClass());
	TwoHandedDef->DefaultPreparedActions.Add(UPlayerGuardBreakAbility::StaticClass());
	TwoHandedDef->bUseOwnerMeshSocketForTrace = false;
	TwoHandedDef->BladeBaseMarkerRelativeLocation = FVector(0, 0, 10);
	TwoHandedDef->BladeTipMarkerRelativeLocation = FVector(0, 0, 150);
	TwoHandedDef->TraceRadius = 15.0f;
	TwoHandedDef->BladeSubdivisions = 4;
	TwoHandedDef->WorldPickupDisplayTransform = FTransform(FRotator(0.0f, 45.0f, 0.0f), FVector(0.0f, 5.0f, 10.0f), FVector(1.0f));

	UMeleeWeaponDefinition* UnarmedDef = NewObject<UMeleeWeaponDefinition>(GetTransientPackage(), TEXT("Test_Unarmed"));
	UnarmedDef->HandSlot = EWeaponHandSlot::MainHandOneHanded;
	UnarmedDef->LocomotionMode = EWeaponLocomotionMode::Default;
	UnarmedDef->AttachSocketName = TEXT("Weapon_R");
	UnarmedDef->WeaponMesh = nullptr;
	UnarmedDef->bUseOwnerMeshSocketForTrace = true;
	UnarmedDef->BladeBaseMarkerRelativeLocation = FVector(0, 0, 5);
	UnarmedDef->BladeTipMarkerRelativeLocation = FVector(0, 0, 20);
	UnarmedDef->TraceRadius = 8.0f;
	UnarmedDef->BladeSubdivisions = 2;
	UnarmedDef->DisplayScale = FVector(1.5f, 0.5f, 1.25f);
	UnarmedDef->BaseGrantedActions.Add(UPrimaryAttackAbility::StaticClass());
	UnarmedDef->PrimaryAttackAbilityTag = TagAbilityPrimaryAttack;

	UOffHandWeaponDefinition* ShieldDef = NewObject<UOffHandWeaponDefinition>(GetTransientPackage(), TEXT("Test_Shield"));
	ShieldDef->HandSlot = EWeaponHandSlot::OffHand;
	ShieldDef->LocomotionMode = EWeaponLocomotionMode::Default;
	ShieldDef->AttachSocketName = TEXT("Weapon_L");
	ShieldDef->WeaponMesh = ShieldMesh;
	ShieldDef->bProvidesShieldPresentation = true;
	ShieldDef->DisplayScale = FVector(0.8f, 1.2f, 1.4f);
	ShieldDef->WorldPickupDisplayTransform = FTransform(FRotator(90.0f, 0.0f, 0.0f), FVector(10.0f, 0.0f, 0.0f), FVector(1.0f));

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

	FTransform NoMainHandSocketTransform(FQuat(0.2f, 0.3f, 0.4f, 0.5f), FVector(10.0f), FVector(2.0f));
	TestFalse(TEXT("Main-hand display socket query rejects no equipped main hand"), EquipmentComp->TryGetEquippedMainHandDisplaySocketTransform(SocketNameTraceBase, NoMainHandSocketTransform));
	TestTrue(TEXT("Main-hand display socket query resets output on no equipped main hand"), NoMainHandSocketTransform.Equals(FTransform::Identity));

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

	auto FindEquippedDisplay = [Player](UStaticMesh* ExpectedMesh, const FName ExpectedAttachSocket) -> UStaticMeshComponent*
	{
		if (!Player || !Player->GetMesh() || !ExpectedMesh)
		{
			return nullptr;
		}

		for (UStaticMeshComponent* StaticMeshComponent : TInlineComponentArray<UStaticMeshComponent*>(Player))
		{
			if (StaticMeshComponent && StaticMeshComponent->IsRegistered() && StaticMeshComponent->GetStaticMesh() == ExpectedMesh
				&& StaticMeshComponent->GetAttachParent() == Player->GetMesh()
				&& StaticMeshComponent->GetAttachSocketName() == ExpectedAttachSocket)
			{
				return StaticMeshComponent;
			}
		}

		return nullptr;
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

		UStaticMeshComponent* SwordDisplayComponent = FindEquippedDisplay(SwordMesh, SwordDef->AttachSocketName);
		UStaticMeshComponent* ShieldDisplayComponent = FindEquippedDisplay(ShieldMesh, ShieldDef->AttachSocketName);
		TestNotNull(TEXT("Equipped Sword display component is found"), SwordDisplayComponent);
		TestNotNull(TEXT("Equipped Shield display component is found"), ShieldDisplayComponent);
		if (SwordDisplayComponent)
		{
			TestTrue(TEXT("Equipped Sword display scale matches its definition"), SwordDisplayComponent->GetRelativeScale3D().Equals(SwordDef->DisplayScale, KINDA_SMALL_NUMBER));
			TestTrue(TEXT("Equipped Sword display location remains its definition offset"), SwordDisplayComponent->GetRelativeLocation().Equals(SwordDef->DisplayLocationOffset, KINDA_SMALL_NUMBER));
			TestTrue(TEXT("Equipped Sword display rotation remains its definition offset"), SwordDisplayComponent->GetRelativeRotation().Equals(SwordDef->DisplayRotationOffset, KINDA_SMALL_NUMBER));
		}
		if (ShieldDisplayComponent)
		{
			TestTrue(TEXT("Equipped Shield display scale matches its definition"), ShieldDisplayComponent->GetRelativeScale3D().Equals(ShieldDef->DisplayScale, KINDA_SMALL_NUMBER));
			TestTrue(TEXT("Equipped Shield display location remains its definition offset"), ShieldDisplayComponent->GetRelativeLocation().Equals(ShieldDef->DisplayLocationOffset, KINDA_SMALL_NUMBER));
			TestTrue(TEXT("Equipped Shield display rotation remains its definition offset"), ShieldDisplayComponent->GetRelativeRotation().Equals(ShieldDef->DisplayRotationOffset, KINDA_SMALL_NUMBER));
		}

		// Assert blade markers are attached to the authored Static Mesh sockets and resolve to distinct locations
		USceneComponent* BladeBaseComp = nullptr;
		USceneComponent* BladeTipComp = nullptr;
		TestTrue(TEXT("TryGetBladeMarkers succeeds for equipped Sword"), EquipmentComp->TryGetBladeMarkers(BladeBaseComp, BladeTipComp));
		TestNotNull(TEXT("BladeBaseComp is valid"), BladeBaseComp);
		TestNotNull(TEXT("BladeTipComp is valid"), BladeTipComp);
		if (BladeBaseComp && BladeTipComp)
		{
			TestEqual(TEXT("BladeBaseComp attached to Trace_Base socket"), BladeBaseComp->GetAttachSocketName(), SocketNameTraceBase);
			TestEqual(TEXT("BladeTipComp attached to Trace_Tip socket"), BladeTipComp->GetAttachSocketName(), SocketNameTraceTip);
			TestNotEqual(TEXT("Blade markers have distinct world locations"), BladeBaseComp->GetComponentLocation(), BladeTipComp->GetComponentLocation());
		}

		FTransform ValidMainHandSocketTransform;
		TestTrue(TEXT("Main-hand display socket query resolves Trace_Base"), EquipmentComp->TryGetEquippedMainHandDisplaySocketTransform(SocketNameTraceBase, ValidMainHandSocketTransform));
		const FVector ValidSocketLocation = ValidMainHandSocketTransform.GetLocation();
		const FQuat ValidSocketRotation = ValidMainHandSocketTransform.GetRotation();
		const FVector ValidSocketScale = ValidMainHandSocketTransform.GetScale3D();
		TestTrue(TEXT("Main-hand display socket query returns the live world-space socket location"),
			BladeBaseComp && ValidSocketLocation.Equals(BladeBaseComp->GetComponentLocation(), KINDA_SMALL_NUMBER));

		FTransform ValidMainHandTipSocketTransform;
		TestTrue(TEXT("Main-hand display socket query resolves scaled Trace_Tip"), EquipmentComp->TryGetEquippedMainHandDisplaySocketTransform(SocketNameTraceTip, ValidMainHandTipSocketTransform));
		TestTrue(TEXT("Main-hand display socket query returns the scaled Trace_Tip marker location"),
			BladeTipComp && ValidMainHandTipSocketTransform.GetLocation().Equals(BladeTipComp->GetComponentLocation(), KINDA_SMALL_NUMBER));
		TestTrue(TEXT("Main-hand display socket query returns a finite transform"),
			FMath::IsFinite(ValidSocketLocation.X) && FMath::IsFinite(ValidSocketLocation.Y) && FMath::IsFinite(ValidSocketLocation.Z)
			&& FMath::IsFinite(ValidSocketRotation.X) && FMath::IsFinite(ValidSocketRotation.Y) && FMath::IsFinite(ValidSocketRotation.Z) && FMath::IsFinite(ValidSocketRotation.W)
			&& FMath::IsFinite(ValidSocketScale.X) && FMath::IsFinite(ValidSocketScale.Y) && FMath::IsFinite(ValidSocketScale.Z));

		FTransform MissingMainHandSocketTransform(FQuat(0.2f, 0.3f, 0.4f, 0.5f), FVector(10.0f), FVector(2.0f));
		TestFalse(TEXT("Main-hand display socket query rejects a missing socket"), EquipmentComp->TryGetEquippedMainHandDisplaySocketTransform(TEXT("Missing_Bow_Launch_Socket"), MissingMainHandSocketTransform));
		TestTrue(TEXT("Main-hand display socket query resets output on missing socket"), MissingMainHandSocketTransform.Equals(FTransform::Identity));

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

		// Direction 3: Preflight failure on invalid socket authoring
		// 3.1 Incomplete socket pair (Base set, Tip None)
		UMeleeWeaponDefinition* IncompleteSocketDef = NewObject<UMeleeWeaponDefinition>(GetTransientPackage(), TEXT("Test_IncompleteSocketDef"));
		IncompleteSocketDef->HandSlot = EWeaponHandSlot::MainHandOneHanded;
		IncompleteSocketDef->AttachSocketName = TEXT("Weapon_R");
		IncompleteSocketDef->WeaponMesh = SwordMesh;
		IncompleteSocketDef->BladeBaseSocketName = SocketNameTraceBase;
		IncompleteSocketDef->BladeTipSocketName = NAME_None;
		IncompleteSocketDef->PrimaryAttackAbilityTag = TagAbilityPrimaryAttack;
		IncompleteSocketDef->AssociatedLoadout = SwordLoadout;
		IncompleteSocketDef->BaseGrantedActions.Add(UPrimaryAttackAbility::StaticClass());
		IncompleteSocketDef->ExclusiveCombatActions.Add(UPlayerGuardAbility::StaticClass());
		IncompleteSocketDef->DefaultPreparedActions.Add(UPlayerGuardAbility::StaticClass());

		FString IncompleteReason;
		TestFalse(TEXT("Preflight rejects incomplete socket pair"), EquipmentComp->TestDirectPreflight(IncompleteSocketDef, nullptr, IncompleteReason));

		// 3.2 Non-existent socket on mesh
		UMeleeWeaponDefinition* MissingSocketDef = NewObject<UMeleeWeaponDefinition>(GetTransientPackage(), TEXT("Test_MissingSocketDef"));
		MissingSocketDef->HandSlot = EWeaponHandSlot::MainHandOneHanded;
		MissingSocketDef->AttachSocketName = TEXT("Weapon_R");
		MissingSocketDef->WeaponMesh = SwordMesh;
		MissingSocketDef->BladeBaseSocketName = SocketNameTraceBase;
		MissingSocketDef->BladeTipSocketName = FName(TEXT("NonExistent_Tip_Socket"));
		MissingSocketDef->PrimaryAttackAbilityTag = TagAbilityPrimaryAttack;
		MissingSocketDef->AssociatedLoadout = SwordLoadout;
		MissingSocketDef->BaseGrantedActions.Add(UPrimaryAttackAbility::StaticClass());
		MissingSocketDef->ExclusiveCombatActions.Add(UPlayerGuardAbility::StaticClass());
		MissingSocketDef->DefaultPreparedActions.Add(UPlayerGuardAbility::StaticClass());

		FString MissingReason;
		TestFalse(TEXT("Preflight rejects non-existent socket name on mesh"), EquipmentComp->TestDirectPreflight(MissingSocketDef, nullptr, MissingReason));

		// 3.3 Distinct socket names with identical RelativeLocations on mesh
		const FName SocketCoincidentBase(TEXT("Test_Socket_Coincident_Base"));
		const FName SocketCoincidentTip(TEXT("Test_Socket_Coincident_Tip"));

		UStaticMeshSocket* CoincidentBaseSocket = NewObject<UStaticMeshSocket>(SwordMesh);
		CoincidentBaseSocket->SocketName = SocketCoincidentBase;
		CoincidentBaseSocket->RelativeLocation = FVector(0.0f, 0.0f, 50.0f);
		SwordMesh->AddSocket(CoincidentBaseSocket);

		UStaticMeshSocket* CoincidentTipSocket = NewObject<UStaticMeshSocket>(SwordMesh);
		CoincidentTipSocket->SocketName = SocketCoincidentTip;
		CoincidentTipSocket->RelativeLocation = FVector(0.0f, 0.0f, 50.0f);
		SwordMesh->AddSocket(CoincidentTipSocket);

		UMeleeWeaponDefinition* CoincidentSocketDef = NewObject<UMeleeWeaponDefinition>(GetTransientPackage(), TEXT("Test_CoincidentSocketDef"));
		CoincidentSocketDef->HandSlot = EWeaponHandSlot::MainHandOneHanded;
		CoincidentSocketDef->AttachSocketName = TEXT("Weapon_R");
		CoincidentSocketDef->WeaponMesh = SwordMesh;
		CoincidentSocketDef->BladeBaseSocketName = SocketCoincidentBase;
		CoincidentSocketDef->BladeTipSocketName = SocketCoincidentTip;
		CoincidentSocketDef->PrimaryAttackAbilityTag = TagAbilityPrimaryAttack;
		CoincidentSocketDef->AssociatedLoadout = SwordLoadout;
		CoincidentSocketDef->BaseGrantedActions.Add(UPrimaryAttackAbility::StaticClass());
		CoincidentSocketDef->ExclusiveCombatActions.Add(UPlayerGuardAbility::StaticClass());
		CoincidentSocketDef->DefaultPreparedActions.Add(UPlayerGuardAbility::StaticClass());

		FString CoincidentReason;
		const bool bCoincidentPreflight = EquipmentComp->TestDirectPreflight(CoincidentSocketDef, nullptr, CoincidentReason);
		TestFalse(TEXT("Preflight rejects distinct socket names with identical RelativeLocations"), bCoincidentPreflight);
		TestTrue(TEXT("Preflight reason explains coincident socket locations"), CoincidentReason.Contains(TEXT("identical RelativeLocations")));

		const bool bEquipCoincidentResult = EquipmentComp->EquipWeapon(CoincidentSocketDef);
		TestFalse(TEXT("Direct EquipWeapon with coincident socket definition fails"), bEquipCoincidentResult);
		TestEqual(TEXT("Main hand remains TwoHanded after coincident equip rejection"), EquipmentComp->GetCurrentMainHandWeapon(), Cast<UWeaponDefinition>(TwoHandedDef));
		TestNull(TEXT("Off hand remains null after coincident equip rejection"), EquipmentComp->GetCurrentOffHandWeapon());

		// Clean up transient test sockets from SwordMesh
		SwordMesh->RemoveSocket(CoincidentBaseSocket);
		SwordMesh->RemoveSocket(CoincidentTipSocket);

		// 3.4 Invalid WorldPickupDisplayTransform (NaN / Inf) preflight rejection
		UMeleeWeaponDefinition* NaNTransformDef = NewObject<UMeleeWeaponDefinition>(GetTransientPackage(), TEXT("Test_NaNTransformDef"));
		NaNTransformDef->HandSlot = EWeaponHandSlot::MainHandOneHanded;
		NaNTransformDef->AttachSocketName = TEXT("Weapon_R");
		NaNTransformDef->WeaponMesh = SwordMesh;
		NaNTransformDef->BladeBaseSocketName = SocketNameTraceBase;
		NaNTransformDef->BladeTipSocketName = SocketNameTraceTip;
		NaNTransformDef->PrimaryAttackAbilityTag = TagAbilityPrimaryAttack;
		NaNTransformDef->AssociatedLoadout = SwordLoadout;
		NaNTransformDef->BaseGrantedActions.Add(UPrimaryAttackAbility::StaticClass());
		NaNTransformDef->ExclusiveCombatActions.Add(UPlayerGuardAbility::StaticClass());
		NaNTransformDef->DefaultPreparedActions.Add(UPlayerGuardAbility::StaticClass());
		NaNTransformDef->WorldPickupDisplayTransform = FTransform(FQuat(NAN, 0.0f, 0.0f, 1.0f), FVector::ZeroVector, FVector::OneVector);

		FString NaNTransformReason;
		TestFalse(TEXT("IsValidWeaponDefinition rejects NaN WorldPickupDisplayTransform"), NaNTransformDef->IsValidWeaponDefinition(NaNTransformReason));
		const bool bNaNTransformPreflight = EquipmentComp->TestDirectPreflight(NaNTransformDef, nullptr, NaNTransformReason);
		TestFalse(TEXT("Preflight rejects NaN WorldPickupDisplayTransform definition"), bNaNTransformPreflight);
		TestTrue(TEXT("Preflight reason explains invalid WorldPickupDisplayTransform"), NaNTransformReason.Contains(TEXT("WorldPickupDisplayTransform")));

		UMeleeWeaponDefinition* InvalidDisplayScaleDef = NewObject<UMeleeWeaponDefinition>(GetTransientPackage(), TEXT("Test_InvalidDisplayScaleDef"));
		InvalidDisplayScaleDef->HandSlot = EWeaponHandSlot::MainHandOneHanded;
		InvalidDisplayScaleDef->LocomotionMode = EWeaponLocomotionMode::LightSword;
		InvalidDisplayScaleDef->AttachSocketName = TEXT("Weapon_R");
		InvalidDisplayScaleDef->WeaponMesh = SwordMesh;
		InvalidDisplayScaleDef->BladeBaseSocketName = SocketNameTraceBase;
		InvalidDisplayScaleDef->BladeTipSocketName = SocketNameTraceTip;
		InvalidDisplayScaleDef->PrimaryAttackAbilityTag = TagAbilityPrimaryAttack;
		InvalidDisplayScaleDef->AssociatedLoadout = SwordLoadout;
		InvalidDisplayScaleDef->BaseGrantedActions.Add(UPrimaryAttackAbility::StaticClass());
		InvalidDisplayScaleDef->ExclusiveCombatActions.Add(UPlayerGuardAbility::StaticClass());
		InvalidDisplayScaleDef->DefaultPreparedActions.Add(UPlayerGuardAbility::StaticClass());
		InvalidDisplayScaleDef->TraceRadius = 10.0f;
		InvalidDisplayScaleDef->BladeSubdivisions = 4;

		FString InvalidDisplayScaleReason;
		InvalidDisplayScaleDef->DisplayScale = FVector::ZeroVector;
		TestFalse(TEXT("IsValidWeaponDefinition rejects zero DisplayScale"), InvalidDisplayScaleDef->IsValidWeaponDefinition(InvalidDisplayScaleReason));
		TestFalse(TEXT("Preflight rejects zero DisplayScale before teardown"), EquipmentComp->TestDirectPreflight(InvalidDisplayScaleDef, nullptr, InvalidDisplayScaleReason));
		TestFalse(TEXT("Direct equip rejects zero DisplayScale before applying composition"), EquipmentComp->EquipWeapon(InvalidDisplayScaleDef));
		TestEqual(TEXT("Main hand remains TwoHanded after zero DisplayScale rejection"), EquipmentComp->GetCurrentMainHandWeapon(), Cast<UWeaponDefinition>(TwoHandedDef));
		TestNull(TEXT("Off hand remains null after zero DisplayScale rejection"), EquipmentComp->GetCurrentOffHandWeapon());

		InvalidDisplayScaleDef->DisplayScale = FVector(-1.0f, 1.0f, 1.0f);
		TestFalse(TEXT("IsValidWeaponDefinition rejects negative DisplayScale"), InvalidDisplayScaleDef->IsValidWeaponDefinition(InvalidDisplayScaleReason));
		TestFalse(TEXT("Preflight rejects negative DisplayScale"), EquipmentComp->TestDirectPreflight(InvalidDisplayScaleDef, nullptr, InvalidDisplayScaleReason));

		InvalidDisplayScaleDef->DisplayScale = FVector(NAN, 1.0f, 1.0f);
		TestFalse(TEXT("IsValidWeaponDefinition rejects NaN DisplayScale"), InvalidDisplayScaleDef->IsValidWeaponDefinition(InvalidDisplayScaleReason));
		TestFalse(TEXT("Preflight rejects NaN DisplayScale"), EquipmentComp->TestDirectPreflight(InvalidDisplayScaleDef, nullptr, InvalidDisplayScaleReason));

		InvalidDisplayScaleDef->DisplayScale = FVector(INFINITY, 1.0f, 1.0f);
		TestFalse(TEXT("IsValidWeaponDefinition rejects Inf DisplayScale"), InvalidDisplayScaleDef->IsValidWeaponDefinition(InvalidDisplayScaleReason));
		TestFalse(TEXT("Preflight rejects Inf DisplayScale"), EquipmentComp->TestDirectPreflight(InvalidDisplayScaleDef, nullptr, InvalidDisplayScaleReason));
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
			UStaticMeshComponent* MeshComp = Drop ? Drop->FindComponentByClass<UStaticMeshComponent>() : nullptr;
			TestNotNull(TEXT("Dropped pickup has UStaticMeshComponent"), MeshComp);

			if (Drop->GetWeaponDefinition() == SwordDef)
			{
				bFoundDroppedSword = true;
				TestFalse(TEXT("Dropped sword rejects former owner during 0.5s cooldown"), Drop->CanInteract(Player));
				if (MeshComp)
				{
					TestEqual(TEXT("Dropped sword mesh matches SwordDef"), MeshComp->GetStaticMesh().Get(), SwordMesh);
					TestTrue(TEXT("Dropped sword relative transform matches SwordDef"), MeshComp->GetRelativeTransform().Equals(SwordDef->WorldPickupDisplayTransform, 1e-3f));

					const FBox MeshBox = SwordMesh->GetBoundingBox();
					const FVector Corners[8] = {
						FVector(MeshBox.Min.X, MeshBox.Min.Y, MeshBox.Min.Z),
						FVector(MeshBox.Min.X, MeshBox.Min.Y, MeshBox.Max.Z),
						FVector(MeshBox.Min.X, MeshBox.Max.Y, MeshBox.Min.Z),
						FVector(MeshBox.Min.X, MeshBox.Max.Y, MeshBox.Max.Z),
						FVector(MeshBox.Max.X, MeshBox.Min.Y, MeshBox.Min.Z),
						FVector(MeshBox.Max.X, MeshBox.Min.Y, MeshBox.Max.Z),
						FVector(MeshBox.Max.X, MeshBox.Max.Y, MeshBox.Min.Z),
						FVector(MeshBox.Max.X, MeshBox.Max.Y, MeshBox.Max.Z)
					};

					float LowestWorldZ = MAX_FLT;
					for (int32 Index = 0; Index < 8; ++Index)
					{
						const FVector WorldPt = Drop->GetActorLocation() + SwordDef->WorldPickupDisplayTransform.TransformPosition(Corners[Index]);
						LowestWorldZ = FMath::Min(LowestWorldZ, WorldPt.Z);
					}
					// Floor is at Z = 0.0f, expected clearance is 2.0cm
					TestNearlyEqual(TEXT("Dropped sword lowest point is 2cm above floor"), LowestWorldZ, 2.0f, 0.01f);
				}
			}
			else if (Drop->GetWeaponDefinition() == ShieldDef)
			{
				bFoundDroppedShield = true;
				TestFalse(TEXT("Dropped shield rejects former owner during 0.5s cooldown"), Drop->CanInteract(Player));
				if (MeshComp)
				{
					TestEqual(TEXT("Dropped shield mesh matches ShieldDef"), MeshComp->GetStaticMesh().Get(), ShieldMesh);
					TestTrue(TEXT("Dropped shield relative transform matches ShieldDef"), MeshComp->GetRelativeTransform().Equals(ShieldDef->WorldPickupDisplayTransform, 1e-3f));

					const FBox MeshBox = ShieldMesh->GetBoundingBox();
					const FVector Corners[8] = {
						FVector(MeshBox.Min.X, MeshBox.Min.Y, MeshBox.Min.Z),
						FVector(MeshBox.Min.X, MeshBox.Min.Y, MeshBox.Max.Z),
						FVector(MeshBox.Min.X, MeshBox.Max.Y, MeshBox.Min.Z),
						FVector(MeshBox.Min.X, MeshBox.Max.Y, MeshBox.Max.Z),
						FVector(MeshBox.Max.X, MeshBox.Min.Y, MeshBox.Min.Z),
						FVector(MeshBox.Max.X, MeshBox.Min.Y, MeshBox.Max.Z),
						FVector(MeshBox.Max.X, MeshBox.Max.Y, MeshBox.Min.Z),
						FVector(MeshBox.Max.X, MeshBox.Max.Y, MeshBox.Max.Z)
					};

					float LowestWorldZ = MAX_FLT;
					for (int32 Index = 0; Index < 8; ++Index)
					{
						const FVector WorldPt = Drop->GetActorLocation() + ShieldDef->WorldPickupDisplayTransform.TransformPosition(Corners[Index]);
						LowestWorldZ = FMath::Min(LowestWorldZ, WorldPt.Z);
					}
					TestNearlyEqual(TEXT("Dropped shield lowest point is 2cm above floor"), LowestWorldZ, 2.0f, 0.01f);
				}
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
			UStaticMeshComponent* MeshComp = Drops52[0]->FindComponentByClass<UStaticMeshComponent>();
			TestNotNull(TEXT("Dropped TwoHanded has UStaticMeshComponent"), MeshComp);
			if (MeshComp)
			{
				TestEqual(TEXT("Dropped TwoHanded mesh matches TwoHandedDef"), MeshComp->GetStaticMesh().Get(), SwordMesh);
				TestTrue(TEXT("Dropped TwoHanded relative transform matches TwoHandedDef"), MeshComp->GetRelativeTransform().Equals(TwoHandedDef->WorldPickupDisplayTransform, 1e-3f));

				const FBox MeshBox = SwordMesh->GetBoundingBox();
				const FVector Corners[8] = {
					FVector(MeshBox.Min.X, MeshBox.Min.Y, MeshBox.Min.Z),
					FVector(MeshBox.Min.X, MeshBox.Min.Y, MeshBox.Max.Z),
					FVector(MeshBox.Min.X, MeshBox.Max.Y, MeshBox.Min.Z),
					FVector(MeshBox.Min.X, MeshBox.Max.Y, MeshBox.Max.Z),
					FVector(MeshBox.Max.X, MeshBox.Min.Y, MeshBox.Min.Z),
					FVector(MeshBox.Max.X, MeshBox.Min.Y, MeshBox.Max.Z),
					FVector(MeshBox.Max.X, MeshBox.Max.Y, MeshBox.Min.Z),
					FVector(MeshBox.Max.X, MeshBox.Max.Y, MeshBox.Max.Z)
				};

				float LowestWorldZ = MAX_FLT;
				for (int32 Index = 0; Index < 8; ++Index)
				{
					const FVector WorldPt = Drops52[0]->GetActorLocation() + TwoHandedDef->WorldPickupDisplayTransform.TransformPosition(Corners[Index]);
					LowestWorldZ = FMath::Min(LowestWorldZ, WorldPt.Z);
				}
				TestNearlyEqual(TEXT("Dropped TwoHanded lowest point is 2cm above floor"), LowestWorldZ, 2.0f, 0.01f);
			}
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

	// 12. Test Composite Defense Profile Resolution & Preflight Validation (TODO-03A4)
	{
		const FGameplayTag TagShieldGuard = FGameplayTag::RequestGameplayTag(TEXT("Ability.Defense.Guard.Shield"));
		const FGameplayTag TagShieldParry = FGameplayTag::RequestGameplayTag(TEXT("Ability.Defense.Parry.Shield"));
		const FGameplayTag TagGenericGuard = FGameplayTag::RequestGameplayTag(TEXT("Ability.Defense.Guard"));
		const FGameplayTag TagGenericParry = FGameplayTag::RequestGameplayTag(TEXT("Ability.Defense.Parry"));
		const FGameplayTag TagInputGuard = FGameplayTag::RequestGameplayTag(TEXT("Input.Guard"));
		const FGameplayTag TagInputParry = FGameplayTag::RequestGameplayTag(TEXT("Input.Parry"));
		const FGameplayTag TagInputPrimary = FGameplayTag::RequestGameplayTag(TEXT("Input.PrimaryAttack"));

		// 12.1 Valid Shield DefenseProfile with profile-specific child tags
		UDefenseProfileDefinition* ValidShieldProfile = NewObject<UDefenseProfileDefinition>(GetTransientPackage(), TEXT("Test_ValidShieldProfile"));
		ValidShieldProfile->GuardAbilityTag = TagShieldGuard;
		ValidShieldProfile->ParryAbilityTag = TagShieldParry;
		TestTrue(TEXT("ValidShieldProfile passes IsProfileValid"), ValidShieldProfile->IsProfileValid());

		// In Section 12, use a sword definition whose prepared actions do not conflict with shield base grants
		UMeleeWeaponDefinition* SwordForShieldDef = NewObject<UMeleeWeaponDefinition>(GetTransientPackage(), TEXT("Test_SwordForShield"));
		SwordForShieldDef->HandSlot = EWeaponHandSlot::MainHandOneHanded;
		SwordForShieldDef->LocomotionMode = EWeaponLocomotionMode::LightSword;
		SwordForShieldDef->AttachSocketName = TEXT("Weapon_R");
		SwordForShieldDef->WeaponMesh = SwordMesh;
		SwordForShieldDef->BladeBaseSocketName = SocketNameTraceBase;
		SwordForShieldDef->BladeTipSocketName = SocketNameTraceTip;
		SwordForShieldDef->PrimaryAttackAbilityTag = TagAbilityPrimaryAttack;
		SwordForShieldDef->AssociatedLoadout = SwordLoadout;
		SwordForShieldDef->BaseGrantedActions.Add(UPrimaryAttackAbility::StaticClass());
		SwordForShieldDef->ExclusiveCombatActions.Add(UPlayerGuardBreakAbility::StaticClass());
		SwordForShieldDef->DefaultPreparedActions.Add(UPlayerGuardBreakAbility::StaticClass());

		UOffHandWeaponDefinition* ShieldWithProfileDef = NewObject<UOffHandWeaponDefinition>(GetTransientPackage(), TEXT("Test_ShieldWithProfile"));
		ShieldWithProfileDef->HandSlot = EWeaponHandSlot::OffHand;
		ShieldWithProfileDef->LocomotionMode = EWeaponLocomotionMode::Default;
		ShieldWithProfileDef->AttachSocketName = TEXT("Weapon_L");
		ShieldWithProfileDef->WeaponMesh = ShieldMesh;
		ShieldWithProfileDef->DefenseProfile = ValidShieldProfile;
		ShieldWithProfileDef->BaseGrantedActions.Add(UPlayerGuardAbility::StaticClass());
		ShieldWithProfileDef->BaseGrantedActions.Add(UPlayerParryAbility::StaticClass());

		// Without specific tags on CDO, Preflight must reject
		FString MissingTagsReason;
		TestFalse(TEXT("Preflight rejects Shield profile when CDOs lack specific shield tags"), EquipmentComp->TestDirectPreflight(SwordForShieldDef, ShieldWithProfileDef, MissingTagsReason));
		TestTrue(TEXT("Missing tags reason names unmatching action"), MissingTagsReason.Contains(TEXT("no BaseGrantedAction matching both")));

		// Editor-authored GameplayTag containers commonly serialize only the specialized child.
		UPlayerGuardAbility* GuardCDO = GetMutableDefault<UPlayerGuardAbility>(UPlayerGuardAbility::StaticClass());
		UPlayerParryAbility* ParryCDO = GetMutableDefault<UPlayerParryAbility>(UPlayerParryAbility::StaticClass());
		GuardCDO->AbilityTags.RemoveTag(TagGenericGuard);
		GuardCDO->AbilityTags.AddTag(TagShieldGuard);
		ParryCDO->AbilityTags.RemoveTag(TagGenericParry);
		ParryCDO->AbilityTags.AddTag(TagShieldParry);

		FString ChildOnlyTagPreflightReason;
		TestTrue(TEXT("Preflight accepts editor-style child-only Shield CDO tags"), EquipmentComp->TestDirectPreflight(SwordForShieldDef, ShieldWithProfileDef, ChildOnlyTagPreflightReason));

		// Equip Sword + ShieldWithProfile
		const bool bEquipShieldSuccess = EquipmentComp->EquipWeapon(SwordForShieldDef) && EquipmentComp->EquipWeapon(ShieldWithProfileDef);
		TestTrue(TEXT("Equip Sword and ShieldWithProfile succeeds"), bEquipShieldSuccess);
		FString ShieldGuardBindingDiagnostic;
		TestTrue(TEXT("Shield Guard is a current component-owned grant"), EquipmentComp->VerifyGrantedAbilityBinding(UPlayerGuardAbility::StaticClass(), ShieldGuardBindingDiagnostic));
		FString ShieldParryBindingDiagnostic;
		TestTrue(TEXT("Shield Parry is a current component-owned grant"), EquipmentComp->VerifyGrantedAbilityBinding(UPlayerParryAbility::StaticClass(), ShieldParryBindingDiagnostic));

		// Verify Defense Tag resolution overrides to Shield tags
		FGameplayTag ResolvedGuardTag;
		TestTrue(TEXT("TryResolveInputIntent resolves Guard to Shield tag"), EquipmentComp->TryResolveInputIntent(TagInputGuard, ResolvedGuardTag));
		TestEqual(TEXT("Resolved Guard tag is Ability.Defense.Guard.Shield"), ResolvedGuardTag, TagShieldGuard);

		FGameplayTag ResolvedParryTag;
		TestTrue(TEXT("TryResolveInputIntent resolves Parry to Shield tag"), EquipmentComp->TryResolveInputIntent(TagInputParry, ResolvedParryTag));
		TestEqual(TEXT("Resolved Parry tag is Ability.Defense.Parry.Shield"), ResolvedParryTag, TagShieldParry);

		// MainHand attack tag resolution remains intact
		FGameplayTag ResolvedPrimaryTag;
		TestTrue(TEXT("MainHand primary attack tag resolution intact with Shield"), EquipmentComp->TryResolveInputIntent(TagInputPrimary, ResolvedPrimaryTag));
		TestEqual(TEXT("Resolved Primary attack tag is Ability.Attack.Primary"), ResolvedPrimaryTag, FGameplayTag::RequestGameplayTag(TEXT("Ability.Attack.Primary")));

		// 12.2 Malformed Profile Rejection Tests
		// 12.2.1 Empty / Invalid tag
		UDefenseProfileDefinition* InvalidTagProfile = NewObject<UDefenseProfileDefinition>(GetTransientPackage(), TEXT("Test_InvalidTagProfile"));
		InvalidTagProfile->GuardAbilityTag = FGameplayTag();
		InvalidTagProfile->ParryAbilityTag = TagShieldParry;
		TestFalse(TEXT("InvalidTagProfile fails IsProfileValid"), InvalidTagProfile->IsProfileValid());

		UOffHandWeaponDefinition* InvalidProfileShield = NewObject<UOffHandWeaponDefinition>(GetTransientPackage(), TEXT("Test_InvalidProfileShield"));
		InvalidProfileShield->HandSlot = EWeaponHandSlot::OffHand;
		InvalidProfileShield->LocomotionMode = EWeaponLocomotionMode::Default;
		InvalidProfileShield->AttachSocketName = TEXT("Weapon_L");
		InvalidProfileShield->WeaponMesh = ShieldMesh;
		InvalidProfileShield->DefenseProfile = InvalidTagProfile;
		InvalidProfileShield->BaseGrantedActions.Add(UPlayerGuardAbility::StaticClass());
		InvalidProfileShield->BaseGrantedActions.Add(UPlayerParryAbility::StaticClass());

		FString InvalidProfileReason;
		TestFalse(TEXT("Preflight rejects invalid DefenseProfile"), EquipmentComp->TestDirectPreflight(SwordForShieldDef, InvalidProfileShield, InvalidProfileReason));

		// 12.2.2 Identical Guard and Parry tag
		UDefenseProfileDefinition* DuplicateTagProfile = NewObject<UDefenseProfileDefinition>(GetTransientPackage(), TEXT("Test_DuplicateTagProfile"));
		DuplicateTagProfile->GuardAbilityTag = TagShieldGuard;
		DuplicateTagProfile->ParryAbilityTag = TagShieldGuard;
		TestFalse(TEXT("DuplicateTagProfile fails IsProfileValid"), DuplicateTagProfile->IsProfileValid());

		// 12.2.3 Same ability class satisfying both Guard and Parry
		GuardCDO->AbilityTags.AddTag(TagShieldParry);

		UOffHandWeaponDefinition* SameActionShield = NewObject<UOffHandWeaponDefinition>(GetTransientPackage(), TEXT("Test_SameActionShield"));
		SameActionShield->HandSlot = EWeaponHandSlot::OffHand;
		SameActionShield->LocomotionMode = EWeaponLocomotionMode::Default;
		SameActionShield->AttachSocketName = TEXT("Weapon_L");
		SameActionShield->WeaponMesh = ShieldMesh;
		SameActionShield->DefenseProfile = ValidShieldProfile;
		SameActionShield->BaseGrantedActions.Add(UPlayerGuardAbility::StaticClass()); // single action class matching both

		FString SameActionReason;
		TestFalse(TEXT("Preflight rejects single ability class for both Guard and Parry"), EquipmentComp->TestDirectPreflight(SwordForShieldDef, SameActionShield, SameActionReason));
		TestTrue(TEXT("Same action reason names duplicate class"), SameActionReason.Contains(TEXT("must not be the same ability class")));

		GuardCDO->AbilityTags.RemoveTag(TagShieldParry);
		GuardCDO->AbilityTags.RemoveTag(TagGenericParry);

		// 12.3 World pickup swap to TwoHanded clears Shield and restores generic default defense resolution
		AWorldWeaponPickup* TwoHandedPickup12 = World->SpawnActor<AWorldWeaponPickup>();
		TwoHandedPickup12->SetActorLocation(FVector(50.0f, 0.0f, 0.0f));
		TwoHandedPickup12->SetWeaponDefinition(TwoHandedDef);

		const bool bWorldEquip2HSuccess = EquipmentComp->TryEquipWorldPickup(TwoHandedPickup12);
		TestTrue(TEXT("TryEquipWorldPickup(TwoHanded) replaces Sword + Shield"), bWorldEquip2HSuccess);
		TestNull(TEXT("OffHand is cleared after TwoHanded pickup swap"), EquipmentComp->GetCurrentOffHandWeapon());
		FString RemovedGuardBindingDiagnostic;
		TestFalse(TEXT("Shield Guard component grant is cleared after TwoHanded swap"), EquipmentComp->VerifyGrantedAbilityBinding(UPlayerGuardAbility::StaticClass(), RemovedGuardBindingDiagnostic));
		FString RemovedParryBindingDiagnostic;
		TestFalse(TEXT("Shield Parry component grant is cleared after TwoHanded swap"), EquipmentComp->VerifyGrantedAbilityBinding(UPlayerParryAbility::StaticClass(), RemovedParryBindingDiagnostic));
		UAbilitySystemComponent* TestASC = Player->GetAbilitySystemComponent();
		TestNotNull(TEXT("Player ASC remains available after TwoHanded swap"), TestASC);
		TestNull(TEXT("No stale Shield Guard spec remains on ASC after TwoHanded swap"), TestASC ? TestASC->FindAbilitySpecFromClass(UPlayerGuardAbility::StaticClass()) : nullptr);
		TestNull(TEXT("No stale Shield Parry spec remains on ASC after TwoHanded swap"), TestASC ? TestASC->FindAbilitySpecFromClass(UPlayerParryAbility::StaticClass()) : nullptr);

		FGameplayTag TwoHandedResolvedGuard;
		TestTrue(TEXT("TwoHanded resolves Guard to generic default"), EquipmentComp->TryResolveInputIntent(TagInputGuard, TwoHandedResolvedGuard));
		TestEqual(TEXT("Resolved Guard tag is generic Ability.Defense.Guard"), TwoHandedResolvedGuard, TagGenericGuard);

		FGameplayTag TwoHandedResolvedParry;
		TestTrue(TEXT("TwoHanded resolves Parry to generic default"), EquipmentComp->TryResolveInputIntent(TagInputParry, TwoHandedResolvedParry));
		TestEqual(TEXT("Resolved Parry tag is generic Ability.Defense.Parry"), TwoHandedResolvedParry, TagGenericParry);

		// 12.4 TwoHanded -> Shield normalization with the actual profiled Shield definition
		AWorldWeaponPickup* ProfileShieldPickup = World->SpawnActor<AWorldWeaponPickup>();
		ProfileShieldPickup->SetActorLocation(FVector(50.0f, 0.0f, 0.0f));
		ProfileShieldPickup->SetWeaponDefinition(ShieldWithProfileDef);

		const bool bWorldEquipProfileShieldSuccess = EquipmentComp->TryEquipWorldPickup(ProfileShieldPickup);
		TestTrue(TEXT("TryEquipWorldPickup(profiled Shield) converts TwoHanded to Unarmed + Shield"), bWorldEquipProfileShieldSuccess);
		TestEqual(TEXT("Profiled Shield normalization uses Unarmed fallback as main hand"), EquipmentComp->GetCurrentMainHandWeapon(), Cast<UWeaponDefinition>(UnarmedDef));
		TestEqual(TEXT("Profiled Shield normalization equips the profiled Shield off hand"), EquipmentComp->GetCurrentOffHandWeapon(), Cast<UWeaponDefinition>(ShieldWithProfileDef));
		TestTrue(TEXT("Profiled Shield pickup source was consumed"), ProfileShieldPickup->IsActorBeingDestroyed());
		FString ReequippedGuardBindingDiagnostic;
		TestTrue(TEXT("Shield Guard is re-granted after TwoHanded -> Shield normalization"), EquipmentComp->VerifyGrantedAbilityBinding(UPlayerGuardAbility::StaticClass(), ReequippedGuardBindingDiagnostic));
		FString ReequippedParryBindingDiagnostic;
		TestTrue(TEXT("Shield Parry is re-granted after TwoHanded -> Shield normalization"), EquipmentComp->VerifyGrantedAbilityBinding(UPlayerParryAbility::StaticClass(), ReequippedParryBindingDiagnostic));
		FGameplayTag ReequippedGuardTag;
		TestTrue(TEXT("Normalized profiled Shield resolves Guard to Shield tag"), EquipmentComp->TryResolveInputIntent(TagInputGuard, ReequippedGuardTag));
		TestEqual(TEXT("Normalized profiled Shield Guard tag is Ability.Defense.Guard.Shield"), ReequippedGuardTag, TagShieldGuard);
		FGameplayTag ReequippedParryTag;
		TestTrue(TEXT("Normalized profiled Shield resolves Parry to Shield tag"), EquipmentComp->TryResolveInputIntent(TagInputParry, ReequippedParryTag));
		TestEqual(TEXT("Normalized profiled Shield Parry tag is Ability.Defense.Parry.Shield"), ReequippedParryTag, TagShieldParry);

		// Clean up dropped pickups and CDO test modifications
		TArray<AWorldWeaponPickup*> Drops12;
		GetWorldDroppedPickups(Drops12);
		for (AWorldWeaponPickup* Drop : Drops12)
		{
			Drop->Destroy();
		}

		GuardCDO->AbilityTags.RemoveTag(TagShieldGuard);
		GuardCDO->AbilityTags.AddTag(TagGenericGuard);
		ParryCDO->AbilityTags.RemoveTag(TagShieldParry);
		ParryCDO->AbilityTags.AddTag(TagGenericParry);
	}

	// -------------------------------------------------------------------------
	// 13. Test Locomotion Mode Resolution, Data Preflight & Non-Drift Rollback
	// -------------------------------------------------------------------------
	{
		// 13.1 Authoring & Data Preflight Fail-Closed Checks
		{
			// Verify Bow numeric value is explicitly 4 (retained after SwordShield removal)
			TestEqual(TEXT("Bow locomotion mode ordinal is explicitly 4"), static_cast<uint8>(EWeaponLocomotionMode::Bow), static_cast<uint8>(4));

			// Base definition rejecting unmapped / legacy ordinal 3 as base LocomotionMode
			UMeleeWeaponDefinition* BadBaseDef = NewObject<UMeleeWeaponDefinition>(GetTransientPackage(), TEXT("Test_BadBaseDef"));
			BadBaseDef->HandSlot = EWeaponHandSlot::MainHandOneHanded;
			BadBaseDef->AttachSocketName = TEXT("Weapon_R");
			BadBaseDef->WeaponMesh = SwordMesh;
			BadBaseDef->BladeBaseSocketName = SocketNameTraceBase;
			BadBaseDef->BladeTipSocketName = SocketNameTraceTip;
			BadBaseDef->PrimaryAttackAbilityTag = TagAbilityPrimaryAttack;
			BadBaseDef->AssociatedLoadout = SwordLoadout;
			BadBaseDef->BaseGrantedActions.Add(UPrimaryAttackAbility::StaticClass());
			BadBaseDef->LocomotionMode = static_cast<EWeaponLocomotionMode>(3);

			FString BaseReason;
			TestFalse(TEXT("Base weapon definition rejects unmapped LocomotionMode ordinal 3"), BadBaseDef->IsValidWeaponDefinition(BaseReason));
			TestTrue(TEXT("Reason reports invalid enum value for ordinal 3"), BaseReason.Contains(TEXT("invalid enum value")));

			BadBaseDef->LocomotionMode = static_cast<EWeaponLocomotionMode>(199);
			TestFalse(TEXT("Base weapon definition rejects out-of-range enum value"), BadBaseDef->IsValidWeaponDefinition(BaseReason));

			// OffHand definition rejecting non-Default base LocomotionMode, wrong HandSlot, and missing mesh
			UOffHandWeaponDefinition* BadOffHandDef = NewObject<UOffHandWeaponDefinition>(GetTransientPackage(), TEXT("Test_BadOffHandDef"));
			BadOffHandDef->HandSlot = EWeaponHandSlot::OffHand;
			BadOffHandDef->AttachSocketName = TEXT("Weapon_L");
			BadOffHandDef->WeaponMesh = ShieldMesh;

			// Invalid Base LocomotionMode on OffHand (must be Default)
			BadOffHandDef->LocomotionMode = EWeaponLocomotionMode::LightSword;
			FString OffHandReason;
			TestFalse(TEXT("OffHand definition rejects non-Default base LocomotionMode"), BadOffHandDef->IsValidWeaponDefinition(OffHandReason));
			BadOffHandDef->LocomotionMode = EWeaponLocomotionMode::Default;

			// Invalid HandSlot on OffHand (must be OffHand)
			BadOffHandDef->HandSlot = EWeaponHandSlot::MainHandOneHanded;
			TestFalse(TEXT("OffHand definition rejects non-OffHand HandSlot"), BadOffHandDef->IsValidWeaponDefinition(OffHandReason));
			BadOffHandDef->HandSlot = EWeaponHandSlot::OffHand;

			// Missing WeaponMesh on OffHand
			BadOffHandDef->WeaponMesh = nullptr;
			TestFalse(TEXT("OffHand definition rejects null WeaponMesh"), BadOffHandDef->IsValidWeaponDefinition(OffHandReason));
			BadOffHandDef->WeaponMesh = ShieldMesh;

			// Valid OffHand passes IsValidWeaponDefinition
			TestTrue(TEXT("Valid OffHand definition passes IsValidWeaponDefinition"), BadOffHandDef->IsValidWeaponDefinition(OffHandReason));

			// Generic OffHand definition with bProvidesShieldPresentation = false
			UOffHandWeaponDefinition* GenericOffHandDef = NewObject<UOffHandWeaponDefinition>(GetTransientPackage(), TEXT("Test_GenericOffHandDef13"));
			GenericOffHandDef->HandSlot = EWeaponHandSlot::OffHand;
			GenericOffHandDef->LocomotionMode = EWeaponLocomotionMode::Default;
			GenericOffHandDef->AttachSocketName = TEXT("Weapon_L");
			GenericOffHandDef->WeaponMesh = ShieldMesh;
			GenericOffHandDef->bProvidesShieldPresentation = false;
			FString GenericOffHandReason;
			TestTrue(TEXT("GenericOffHandDef passes IsValidWeaponDefinition"), GenericOffHandDef->IsValidWeaponDefinition(GenericOffHandReason));
		}

		// 13.2 Transient Bow Fixture Validation
		UStaticMesh* TransientBowMesh = NewObject<UStaticMesh>(GetTransientPackage(), TEXT("Test_TransBowMesh13"));
		const FName SocketNameBowLaunch(TEXT("Socket_Bow_Launch"));
		UStaticMeshSocket* LaunchSocket = NewObject<UStaticMeshSocket>(TransientBowMesh);
		LaunchSocket->SocketName = SocketNameBowLaunch;
		LaunchSocket->RelativeLocation = FVector(0.0f, 0.0f, 20.0f);
		TransientBowMesh->AddSocket(LaunchSocket);

		UProjectileDefinition* TransBowProjDef = NewObject<UProjectileDefinition>(GetTransientPackage(), TEXT("Test_TransBowProjDef13"));
		TransBowProjDef->InitialSpeed = 3000.0f;
		TransBowProjDef->MaxSpeed = 3000.0f;
		TransBowProjDef->LifespanSeconds = 5.0f;
		TransBowProjDef->CollisionRadius = 12.0f;
		TransBowProjDef->DamageGameplayEffectClass = UTestProjectileDamageGE::StaticClass();

		UCombatLoadoutDefinition* BowLoadout = NewObject<UCombatLoadoutDefinition>(GetTransientPackage(), TEXT("Test_BowLoadout13"));
		BowLoadout->AddTestInputAbilityRoute(TagInputPrimaryAttack, TagAbilityPrimaryAttack);

		UBowWeaponDefinition* BowDef = NewObject<UBowWeaponDefinition>(GetTransientPackage(), TEXT("Test_BowDef13"));
		TestEqual(TEXT("BowDef default LocomotionMode is Bow"), BowDef->LocomotionMode, EWeaponLocomotionMode::Bow);
		BowDef->HandSlot = EWeaponHandSlot::MainHandTwoHanded;
		BowDef->AttachSocketName = TEXT("Bow_L");
		BowDef->WeaponMesh = TransientBowMesh;
		BowDef->LaunchSocketName = SocketNameBowLaunch;
		BowDef->DefaultProjectileDefinition = TransBowProjDef;
		BowDef->PrimaryAttackAbilityTag = TagAbilityPrimaryAttack;
		BowDef->AssociatedLoadout = BowLoadout;
		BowDef->BaseGrantedActions.Add(UBowDrawFireAbility::StaticClass());
		BowDef->DisplayScale = FVector(1.6f, 0.7f, 1.3f);

		FString BowValidationReason;
		TestTrue(TEXT("Transient BowDef passes IsValidWeaponDefinition"), BowDef->IsValidWeaponDefinition(BowValidationReason));

		BowDef->LocomotionMode = EWeaponLocomotionMode::Default;
		TestFalse(TEXT("BowDef rejects LocomotionMode != Bow"), BowDef->IsValidWeaponDefinition(BowValidationReason));
		BowDef->LocomotionMode = EWeaponLocomotionMode::Bow;

		// 13.3 Locomotion Mode Resolution Across Transaction Matrix
		// 13.3.1 Initial clean state -> Default
		// First equip TwoHandedDef via world pickup to clear any lingering OffHand from previous sections
		AWorldWeaponPickup* TwoHandedResetPickup = World->SpawnActor<AWorldWeaponPickup>();
		TwoHandedResetPickup->SetActorLocation(FVector(50.0f, 0.0f, 0.0f));
		TwoHandedResetPickup->SetWeaponDefinition(TwoHandedDef);
		TestTrue(TEXT("Equip TwoHanded pickup to clear OffHand"), EquipmentComp->TryEquipWorldPickup(TwoHandedResetPickup));
		TestNull(TEXT("OffHand is cleared after TwoHanded reset"), EquipmentComp->GetCurrentOffHandWeapon());

		// Now equip Unarmed to establish clean Unarmed baseline (Main=Unarmed, OffHand=null)
		AWorldWeaponPickup* UnarmedCleanupPickup = World->SpawnActor<AWorldWeaponPickup>();
		UnarmedCleanupPickup->SetActorLocation(FVector(50.0f, 0.0f, 0.0f));
		UnarmedCleanupPickup->SetWeaponDefinition(UnarmedDef);
		TestTrue(TEXT("Equip Unarmed to establish baseline"), EquipmentComp->TryEquipWorldPickup(UnarmedCleanupPickup));
		TestEqual(TEXT("Unarmed equipped resolves LocomotionMode to Default"), EquipmentComp->GetResolvedLocomotionMode(), EWeaponLocomotionMode::Default);
		TestFalse(TEXT("Unarmed baseline has no Shield presentation"), EquipmentComp->HasShieldEquipped());
		TestNull(TEXT("OffHand remains null with Unarmed baseline"), EquipmentComp->GetCurrentOffHandWeapon());

		USceneComponent* UnarmedBladeBase = nullptr;
		USceneComponent* UnarmedBladeTip = nullptr;
		TestTrue(TEXT("Unarmed trace markers resolve"), EquipmentComp->TryGetBladeMarkers(UnarmedBladeBase, UnarmedBladeTip));
		TestTrue(TEXT("Unarmed BladeBase remains attached to the owner SkeletalMesh"), UnarmedBladeBase && UnarmedBladeBase->GetAttachParent() == Player->GetMesh());
		TestTrue(TEXT("Unarmed BladeTip remains attached to the owner SkeletalMesh"), UnarmedBladeTip && UnarmedBladeTip->GetAttachParent() == Player->GetMesh());

		bool bUnarmedDisplayExists = false;
		for (UStaticMeshComponent* StaticMeshComponent : TInlineComponentArray<UStaticMeshComponent*>(Player))
		{
			if (StaticMeshComponent && StaticMeshComponent->IsRegistered() && StaticMeshComponent->GetStaticMesh()
				&& StaticMeshComponent->GetAttachParent() == Player->GetMesh()
				&& StaticMeshComponent->GetAttachSocketName() == UnarmedDef->AttachSocketName)
			{
				bUnarmedDisplayExists = true;
				break;
			}
		}
		TestFalse(TEXT("Unarmed DisplayScale does not create a display mesh"), bUnarmedDisplayExists);

		// 13.3.2 Equip Sword -> LightSword
		AWorldWeaponPickup* SwordPickup13 = World->SpawnActor<AWorldWeaponPickup>();
		SwordPickup13->SetActorLocation(FVector(50.0f, 0.0f, 0.0f));
		SwordPickup13->SetWeaponDefinition(SwordDef);
		TestTrue(TEXT("Equip Sword pickup succeeds"), EquipmentComp->TryEquipWorldPickup(SwordPickup13));
		TestEqual(TEXT("Sword equipped resolves LocomotionMode to LightSword"), EquipmentComp->GetResolvedLocomotionMode(), EWeaponLocomotionMode::LightSword);
		TestFalse(TEXT("Single Sword has no Shield presentation"), EquipmentComp->HasShieldEquipped());

		// 13.3.3 Equip Shield while holding Sword -> LightSword (MainHand-only) + Shield presentation true
		AWorldWeaponPickup* ShieldPickup13 = World->SpawnActor<AWorldWeaponPickup>();
		ShieldPickup13->SetActorLocation(FVector(50.0f, 0.0f, 0.0f));
		ShieldPickup13->SetWeaponDefinition(ShieldDef);
		TestTrue(TEXT("Equip Shield pickup succeeds"), EquipmentComp->TryEquipWorldPickup(ShieldPickup13));
		TestEqual(TEXT("Sword + Shield resolves LocomotionMode to LightSword (MainHand-only)"), EquipmentComp->GetResolvedLocomotionMode(), EWeaponLocomotionMode::LightSword);
		TestTrue(TEXT("Sword + Shield provides Shield presentation"), EquipmentComp->HasShieldEquipped());

		// 13.3.3b Equip GenericOffHand while holding Sword -> LightSword + Shield presentation false
		UOffHandWeaponDefinition* GenericOffHandDef = NewObject<UOffHandWeaponDefinition>(GetTransientPackage(), TEXT("Test_GenericOffHandDef13_Pickup"));
		GenericOffHandDef->HandSlot = EWeaponHandSlot::OffHand;
		GenericOffHandDef->LocomotionMode = EWeaponLocomotionMode::Default;
		GenericOffHandDef->AttachSocketName = TEXT("Weapon_L");
		GenericOffHandDef->WeaponMesh = ShieldMesh;
		GenericOffHandDef->bProvidesShieldPresentation = false;

		AWorldWeaponPickup* GenericOffHandPickup13 = World->SpawnActor<AWorldWeaponPickup>();
		GenericOffHandPickup13->SetActorLocation(FVector(50.0f, 0.0f, 0.0f));
		GenericOffHandPickup13->SetWeaponDefinition(GenericOffHandDef);
		TestTrue(TEXT("Equip GenericOffHand pickup succeeds"), EquipmentComp->TryEquipWorldPickup(GenericOffHandPickup13));
		TestEqual(TEXT("Sword + GenericOffHand resolves LocomotionMode to LightSword"), EquipmentComp->GetResolvedLocomotionMode(), EWeaponLocomotionMode::LightSword);
		TestFalse(TEXT("Sword + GenericOffHand does not provide Shield presentation"), EquipmentComp->HasShieldEquipped());

		// Re-equip Shield to prepare for Bow pickup replacement test
		AWorldWeaponPickup* ReequipShieldPickup13 = World->SpawnActor<AWorldWeaponPickup>();
		ReequipShieldPickup13->SetActorLocation(FVector(50.0f, 0.0f, 0.0f));
		ReequipShieldPickup13->SetWeaponDefinition(ShieldDef);
		TestTrue(TEXT("Re-equip Shield pickup succeeds"), EquipmentComp->TryEquipWorldPickup(ReequipShieldPickup13));
		TestTrue(TEXT("Re-equipped Shield provides Shield presentation"), EquipmentComp->HasShieldEquipped());

		// 13.3.4 Equip Bow via world pickup -> Bow (OffHand cleared atomically)
		AWorldWeaponPickup* BowPickup13 = World->SpawnActor<AWorldWeaponPickup>();
		BowPickup13->SetActorLocation(FVector(50.0f, 0.0f, 0.0f));
		BowPickup13->SetWeaponDefinition(BowDef);
		TestTrue(TEXT("Equip Bow pickup replaces Sword + Shield atomically"), EquipmentComp->TryEquipWorldPickup(BowPickup13));
		TestEqual(TEXT("Bow equipped resolves LocomotionMode to Bow"), EquipmentComp->GetResolvedLocomotionMode(), EWeaponLocomotionMode::Bow);
		TestFalse(TEXT("Bow equipped has no Shield presentation"), EquipmentComp->HasShieldEquipped());
		TestNull(TEXT("OffHand is null when Bow is equipped"), EquipmentComp->GetCurrentOffHandWeapon());

		UStaticMeshComponent* BowDisplayComponent = FindEquippedDisplay(TransientBowMesh, BowDef->AttachSocketName);
		TestNotNull(TEXT("Equipped Bow display component is found"), BowDisplayComponent);
		if (BowDisplayComponent)
		{
			TestTrue(TEXT("Equipped Bow display scale matches its definition"), BowDisplayComponent->GetRelativeScale3D().Equals(BowDef->DisplayScale, KINDA_SMALL_NUMBER));

			FTransform BowLaunchSocketTransform;
			TestTrue(TEXT("Bow launch Socket query succeeds on scaled display"), EquipmentComp->TryGetEquippedMainHandDisplaySocketTransform(SocketNameBowLaunch, BowLaunchSocketTransform));
			const FVector ExpectedLaunchLocation = BowDisplayComponent->GetComponentTransform().TransformPosition(LaunchSocket->RelativeLocation);
			TestTrue(TEXT("Bow launch Socket world location inherits display scale"), BowLaunchSocketTransform.GetLocation().Equals(ExpectedLaunchLocation, KINDA_SMALL_NUMBER));
		}

		// 13.3.5 TwoHanded (Bow) -> Shield normalization -> Unarmed + Shield resolves to Default (plus Shield presentation)
		AWorldWeaponPickup* NormShieldPickup = World->SpawnActor<AWorldWeaponPickup>();
		NormShieldPickup->SetActorLocation(FVector(50.0f, 0.0f, 0.0f));
		NormShieldPickup->SetWeaponDefinition(ShieldDef);
		TestTrue(TEXT("TwoHanded -> Shield normalization succeeds"), EquipmentComp->TryEquipWorldPickup(NormShieldPickup));
		TestEqual(TEXT("Main hand is Unarmed fallback"), EquipmentComp->GetCurrentMainHandWeapon(), Cast<UWeaponDefinition>(UnarmedDef));
		TestEqual(TEXT("Off hand is Shield"), EquipmentComp->GetCurrentOffHandWeapon(), Cast<UWeaponDefinition>(ShieldDef));
		TestEqual(TEXT("Unarmed + Shield resolves LocomotionMode to Default"), EquipmentComp->GetResolvedLocomotionMode(), EWeaponLocomotionMode::Default);
		TestTrue(TEXT("Unarmed + Shield provides Shield presentation"), EquipmentComp->HasShieldEquipped());

		// 13.3.6 HeavySword main hand (LocomotionMode == HeavySword)
		TwoHandedDef->LocomotionMode = EWeaponLocomotionMode::HeavySword;
		AWorldWeaponPickup* TwoHandedPickup13 = World->SpawnActor<AWorldWeaponPickup>();
		TwoHandedPickup13->SetActorLocation(FVector(50.0f, 0.0f, 0.0f));
		TwoHandedPickup13->SetWeaponDefinition(TwoHandedDef);
		TestTrue(TEXT("Equip HeavySword TwoHanded pickup succeeds"), EquipmentComp->TryEquipWorldPickup(TwoHandedPickup13));
		TestEqual(TEXT("HeavySword resolves LocomotionMode to HeavySword"), EquipmentComp->GetResolvedLocomotionMode(), EWeaponLocomotionMode::HeavySword);
		TestFalse(TEXT("HeavySword has no Shield presentation"), EquipmentComp->HasShieldEquipped());
		TwoHandedDef->LocomotionMode = EWeaponLocomotionMode::Default;

		// 13.4 Rollback and Mode Non-Drift Verification
		// Transition to { SwordDef, ShieldDef }
		AWorldWeaponPickup* SetupSword = World->SpawnActor<AWorldWeaponPickup>();
		SetupSword->SetActorLocation(FVector(50.0f, 0.0f, 0.0f));
		SetupSword->SetWeaponDefinition(SwordDef);
		TestTrue(TEXT("Setup Sword succeeds"), EquipmentComp->TryEquipWorldPickup(SetupSword));

		AWorldWeaponPickup* SetupShield = World->SpawnActor<AWorldWeaponPickup>();
		SetupShield->SetActorLocation(FVector(50.0f, 0.0f, 0.0f));
		SetupShield->SetWeaponDefinition(ShieldDef);
		TestTrue(TEXT("Setup Shield succeeds"), EquipmentComp->TryEquipWorldPickup(SetupShield));
		TestEqual(TEXT("Pre-rollback mode is LightSword"), EquipmentComp->GetResolvedLocomotionMode(), EWeaponLocomotionMode::LightSword);
		TestTrue(TEXT("Pre-rollback Shield presentation is true"), EquipmentComp->HasShieldEquipped());

		// 13.4.1 Injected Apply Failure Rollback from { Sword, Shield } -> Bow
		EquipmentComp->SetInjectApplyFailureOnce(true);
		AWorldWeaponPickup* FailApplyBow = World->SpawnActor<AWorldWeaponPickup>();
		FailApplyBow->SetActorLocation(FVector(50.0f, 0.0f, 0.0f));
		FailApplyBow->SetWeaponDefinition(BowDef);
		TestFalse(TEXT("Injected apply failure causes TryEquipWorldPickup(Bow) to fail"), EquipmentComp->TryEquipWorldPickup(FailApplyBow));
		TestEqual(TEXT("Main hand restored to SwordDef after Apply failure"), EquipmentComp->GetCurrentMainHandWeapon(), Cast<UWeaponDefinition>(SwordDef));
		TestEqual(TEXT("Off hand restored to ShieldDef after Apply failure"), EquipmentComp->GetCurrentOffHandWeapon(), Cast<UWeaponDefinition>(ShieldDef));
		TestEqual(TEXT("LocomotionMode restored to LightSword after Apply failure (no drift)"), EquipmentComp->GetResolvedLocomotionMode(), EWeaponLocomotionMode::LightSword);
		TestTrue(TEXT("Shield presentation restored to true after Apply failure (no drift)"), EquipmentComp->HasShieldEquipped());
		if (FailApplyBow && !FailApplyBow->IsActorBeingDestroyed())
		{
			FailApplyBow->Destroy();
		}

		// 13.4.2 Injected Drop Failure Rollback from { Sword, Shield } -> Bow
		EquipmentComp->SetInjectDropFailureOnce(true);
		AWorldWeaponPickup* FailDropBow = World->SpawnActor<AWorldWeaponPickup>();
		FailDropBow->SetActorLocation(FVector(50.0f, 0.0f, 0.0f));
		FailDropBow->SetWeaponDefinition(BowDef);
		TestFalse(TEXT("Injected drop failure causes TryEquipWorldPickup(Bow) to fail"), EquipmentComp->TryEquipWorldPickup(FailDropBow));
		TestEqual(TEXT("Main hand restored to SwordDef after Drop failure"), EquipmentComp->GetCurrentMainHandWeapon(), Cast<UWeaponDefinition>(SwordDef));
		TestEqual(TEXT("Off hand restored to ShieldDef after Drop failure"), EquipmentComp->GetCurrentOffHandWeapon(), Cast<UWeaponDefinition>(ShieldDef));
		TestEqual(TEXT("LocomotionMode restored to LightSword after Drop failure (no drift)"), EquipmentComp->GetResolvedLocomotionMode(), EWeaponLocomotionMode::LightSword);
		TestTrue(TEXT("Shield presentation restored to true after Drop failure (no drift)"), EquipmentComp->HasShieldEquipped());
		if (FailDropBow && !FailDropBow->IsActorBeingDestroyed())
		{
			FailDropBow->Destroy();
		}

		UStaticMeshComponent* RestoredSwordDisplay = FindEquippedDisplay(SwordMesh, SwordDef->AttachSocketName);
		UStaticMeshComponent* RestoredShieldDisplay = FindEquippedDisplay(ShieldMesh, ShieldDef->AttachSocketName);
		TestNotNull(TEXT("Rollback restores the Sword display component"), RestoredSwordDisplay);
		TestNotNull(TEXT("Rollback restores the Shield display component"), RestoredShieldDisplay);
		TestTrue(TEXT("Rollback restores the Sword definition scale"), RestoredSwordDisplay && RestoredSwordDisplay->GetRelativeScale3D().Equals(SwordDef->DisplayScale, KINDA_SMALL_NUMBER));
		TestTrue(TEXT("Rollback restores the Shield definition scale"), RestoredShieldDisplay && RestoredShieldDisplay->GetRelativeScale3D().Equals(ShieldDef->DisplayScale, KINDA_SMALL_NUMBER));

		// 13.4.3 Injected Apply Failure Rollback from single Sword (LightSword) -> Bow
		AWorldWeaponPickup* ClearOffHandForSword = World->SpawnActor<AWorldWeaponPickup>();
		ClearOffHandForSword->SetActorLocation(FVector(50.0f, 0.0f, 0.0f));
		ClearOffHandForSword->SetWeaponDefinition(TwoHandedDef);
		TestTrue(TEXT("Equip TwoHanded to clear OffHand before single Sword test"), EquipmentComp->TryEquipWorldPickup(ClearOffHandForSword));

		AWorldWeaponPickup* SingleSwordPickup = World->SpawnActor<AWorldWeaponPickup>();
		SingleSwordPickup->SetActorLocation(FVector(50.0f, 0.0f, 0.0f));
		SingleSwordPickup->SetWeaponDefinition(SwordDef);
		TestTrue(TEXT("Equip single Sword succeeds"), EquipmentComp->TryEquipWorldPickup(SingleSwordPickup));
		TestEqual(TEXT("Single Sword main hand is SwordDef"), EquipmentComp->GetCurrentMainHandWeapon(), Cast<UWeaponDefinition>(SwordDef));
		TestNull(TEXT("Single Sword off hand is null"), EquipmentComp->GetCurrentOffHandWeapon());
		TestEqual(TEXT("Single Sword LocomotionMode is LightSword"), EquipmentComp->GetResolvedLocomotionMode(), EWeaponLocomotionMode::LightSword);
		TestFalse(TEXT("Single Sword has no Shield presentation"), EquipmentComp->HasShieldEquipped());

		EquipmentComp->SetInjectApplyFailureOnce(true);
		AWorldWeaponPickup* FailApplyBowSingle = World->SpawnActor<AWorldWeaponPickup>();
		FailApplyBowSingle->SetActorLocation(FVector(50.0f, 0.0f, 0.0f));
		FailApplyBowSingle->SetWeaponDefinition(BowDef);
		TestFalse(TEXT("Injected apply failure causes swap from single Sword to fail"), EquipmentComp->TryEquipWorldPickup(FailApplyBowSingle));
		TestEqual(TEXT("Main hand remains SwordDef"), EquipmentComp->GetCurrentMainHandWeapon(), Cast<UWeaponDefinition>(SwordDef));
		TestNull(TEXT("Off hand remains null"), EquipmentComp->GetCurrentOffHandWeapon());
		TestEqual(TEXT("LocomotionMode remains LightSword after rollback with no drift"), EquipmentComp->GetResolvedLocomotionMode(), EWeaponLocomotionMode::LightSword);
		TestFalse(TEXT("Shield presentation remains false after rollback with no drift"), EquipmentComp->HasShieldEquipped());
		if (FailApplyBowSingle && !FailApplyBowSingle->IsActorBeingDestroyed())
		{
			FailApplyBowSingle->Destroy();
		}

		// 13.5 Authored Ability CDO Tag Contract Verification
		{
			const FGameplayTag TagGenericGuarding = FGameplayTag::RequestGameplayTag(TEXT("State.Action.Guarding"));
			const FGameplayTag TagShieldGuarding = FGameplayTag::RequestGameplayTag(TEXT("State.Action.Guarding.Shield"));
			TestTrue(TEXT("Tag State.Action.Guarding is registered"), TagGenericGuarding.IsValid());
			TestTrue(TEXT("Tag State.Action.Guarding.Shield is registered"), TagShieldGuarding.IsValid());

			UClass* ShieldGuardClass = StaticLoadClass(UGameplayAbility::StaticClass(), nullptr, TEXT("/Game/_Abilities/Weapon/Shield/GA_PlayerShieldGuard.GA_PlayerShieldGuard_C"));
			TestNotNull(TEXT("GA_PlayerShieldGuard blueprint class loaded"), ShieldGuardClass);

			UClass* SwordGuardClass = StaticLoadClass(UGameplayAbility::StaticClass(), nullptr, TEXT("/Game/_Abilities/Weapon/LightSword/Guard/GA_Guard_Sowrd.GA_Guard_Sowrd_C"));
			TestNotNull(TEXT("GA_Guard_Sowrd blueprint class loaded"), SwordGuardClass);

			if (ShieldGuardClass && SwordGuardClass)
			{
				const UGameplayAbility* ShieldGuardCDO = GetDefault<UGameplayAbility>(ShieldGuardClass);
				const UGameplayAbility* SwordGuardCDO = GetDefault<UGameplayAbility>(SwordGuardClass);
				TestNotNull(TEXT("ShieldGuard CDO is valid"), ShieldGuardCDO);
				TestNotNull(TEXT("SwordGuard CDO is valid"), SwordGuardCDO);

				FStructProperty* TagContainerProp = CastField<FStructProperty>(UGameplayAbility::StaticClass()->FindPropertyByName(TEXT("ActivationOwnedTags")));
				TestNotNull(TEXT("ActivationOwnedTags property found on UGameplayAbility"), TagContainerProp);

				if (ShieldGuardCDO && SwordGuardCDO && TagContainerProp && TagContainerProp->Struct == FGameplayTagContainer::StaticStruct())
				{
					const FGameplayTagContainer* ShieldActivationTags = TagContainerProp->ContainerPtrToValuePtr<FGameplayTagContainer>(ShieldGuardCDO);
					const FGameplayTagContainer* SwordActivationTags = TagContainerProp->ContainerPtrToValuePtr<FGameplayTagContainer>(SwordGuardCDO);
					TestNotNull(TEXT("Shield Guard ActivationOwnedTags container resolved"), ShieldActivationTags);
					TestNotNull(TEXT("Sword Guard ActivationOwnedTags container resolved"), SwordActivationTags);

					if (ShieldActivationTags && SwordActivationTags)
					{
						TestTrue(TEXT("GA_PlayerShieldGuard CDO contains exact State.Action.Guarding.Shield tag"), ShieldActivationTags->HasTagExact(TagShieldGuarding));
						TestTrue(TEXT("GA_PlayerShieldGuard CDO hierarchically matches generic State.Action.Guarding"), ShieldActivationTags->HasTag(TagGenericGuarding));

						TestFalse(TEXT("GA_Guard_Sowrd CDO does not contain State.Action.Guarding.Shield"), SwordActivationTags->HasTagExact(TagShieldGuarding));
						TestTrue(TEXT("GA_Guard_Sowrd CDO hierarchically matches generic State.Action.Guarding"), SwordActivationTags->HasTag(TagGenericGuarding));
					}
				}
			}
		}

		// Clean up dropped pickups in Section 13
		TArray<AWorldWeaponPickup*> Drops13;
		GetWorldDroppedPickups(Drops13);
		for (AWorldWeaponPickup* Drop : Drops13)
		{
			Drop->Destroy();
		}
	}

	// -------------------------------------------------------------------------
	// 14. Test Direct Combat Route Contract, Conflict Isolation & Fail-Closed Validation (TODO-03I1)
	// -------------------------------------------------------------------------
	{
		// 14.1 Pure Direct Route Equip & Resolution without AssociatedLoadout
		UMeleeWeaponDefinition* DirectSwordDef = NewObject<UMeleeWeaponDefinition>(GetTransientPackage(), TEXT("Test_DirectSword"));
		DirectSwordDef->HandSlot = EWeaponHandSlot::MainHandOneHanded;
		DirectSwordDef->LocomotionMode = EWeaponLocomotionMode::LightSword;
		DirectSwordDef->AttachSocketName = TEXT("Weapon_R");
		DirectSwordDef->WeaponMesh = SwordMesh;
		DirectSwordDef->BladeBaseSocketName = SocketNameTraceBase;
		DirectSwordDef->BladeTipSocketName = SocketNameTraceTip;
		DirectSwordDef->PrimaryAttackAbilityTag = TagAbilityPrimaryAttack;
		DirectSwordDef->SprintAttackAbilityTag = TagAbilitySprintAttack;
		DirectSwordDef->AssociatedLoadout = nullptr;
		DirectSwordDef->BaseGrantedActions.Add(UPrimaryAttackAbility::StaticClass());
		DirectSwordDef->BaseGrantedActions.Add(USprintAttackAbility::StaticClass());
		DirectSwordDef->bUseOwnerMeshSocketForTrace = false;
		DirectSwordDef->BladeBaseMarkerRelativeLocation = FVector(0, 0, 10);
		DirectSwordDef->BladeTipMarkerRelativeLocation = FVector(0, 0, 100);
		DirectSwordDef->TraceRadius = 10.0f;
		DirectSwordDef->BladeSubdivisions = 4;

		FString DirectSwordReason;
		TestTrue(TEXT("14.1 DirectSwordDef with null AssociatedLoadout passes IsValidWeaponDefinition"), DirectSwordDef->IsValidWeaponDefinition(DirectSwordReason));
		TestTrue(TEXT("14.1 DirectSwordDef equips successfully"), EquipmentComp->EquipWeapon(DirectSwordDef));

		FGameplayTag ResolvedPrimaryTag;
		TestTrue(TEXT("14.1 TryResolveInputIntent resolves PrimaryAttack from direct field"), EquipmentComp->TryResolveInputIntent(TagInputPrimaryAttack, ResolvedPrimaryTag));
		TestEqual(TEXT("14.1 Resolved Primary tag matches direct PrimaryAttackAbilityTag"), ResolvedPrimaryTag, TagAbilityPrimaryAttack);

		FGameplayTag ResolvedSprintTag;
		TestTrue(TEXT("14.1 TryGetSprintAttackAbilityTag resolves Sprint Attack from direct field"), EquipmentComp->TryGetSprintAttackAbilityTag(ResolvedSprintTag));
		TestEqual(TEXT("14.1 Resolved Sprint tag matches direct SprintAttackAbilityTag"), ResolvedSprintTag, TagAbilitySprintAttack);

		TestNull(TEXT("14.1 ActiveCombatLoadout mirror is cleared when equipping weapon with null AssociatedLoadout"), Player->GetActiveCombatLoadout());

		// 14.2 Direct Route Priority & Loadout Conflict Isolation
		UCombatLoadoutDefinition* ConflictingLoadout = NewObject<UCombatLoadoutDefinition>(GetTransientPackage(), TEXT("Test_ConflictingLoadout"));
		const FGameplayTag TagConflictingAbility = FGameplayTag::RequestGameplayTag(TEXT("Ability.Defense.Guard"));
		ConflictingLoadout->AddTestInputAbilityRoute(TagInputPrimaryAttack, TagConflictingAbility);

		UMeleeWeaponDefinition* ConflictSwordDef = NewObject<UMeleeWeaponDefinition>(GetTransientPackage(), TEXT("Test_ConflictSword"));
		ConflictSwordDef->HandSlot = EWeaponHandSlot::MainHandOneHanded;
		ConflictSwordDef->LocomotionMode = EWeaponLocomotionMode::LightSword;
		ConflictSwordDef->AttachSocketName = TEXT("Weapon_R");
		ConflictSwordDef->WeaponMesh = SwordMesh;
		ConflictSwordDef->BladeBaseSocketName = SocketNameTraceBase;
		ConflictSwordDef->BladeTipSocketName = SocketNameTraceTip;
		ConflictSwordDef->PrimaryAttackAbilityTag = TagAbilityPrimaryAttack;
		ConflictSwordDef->AssociatedLoadout = ConflictingLoadout;
		ConflictSwordDef->BaseGrantedActions.Add(UPrimaryAttackAbility::StaticClass());
		ConflictSwordDef->bUseOwnerMeshSocketForTrace = false;
		ConflictSwordDef->BladeBaseMarkerRelativeLocation = FVector(0, 0, 10);
		ConflictSwordDef->BladeTipMarkerRelativeLocation = FVector(0, 0, 100);
		ConflictSwordDef->TraceRadius = 10.0f;
		ConflictSwordDef->BladeSubdivisions = 4;

		TestTrue(TEXT("14.2 ConflictSwordDef equips successfully"), EquipmentComp->EquipWeapon(ConflictSwordDef));
		FGameplayTag ConflictResolvedTag;
		TestTrue(TEXT("14.2 TryResolveInputIntent succeeds despite conflicting AssociatedLoadout"), EquipmentComp->TryResolveInputIntent(TagInputPrimaryAttack, ConflictResolvedTag));
		TestEqual(TEXT("14.2 Direct route wins over conflicting AssociatedLoadout"), ConflictResolvedTag, TagAbilityPrimaryAttack);

		// 14.3 Missing / Unmatched Direct Route Fail-Closed Validation
		// 14.3.1 Missing PrimaryAttackAbilityTag
		UMeleeWeaponDefinition* MissingPrimaryDef = NewObject<UMeleeWeaponDefinition>(GetTransientPackage(), TEXT("Test_MissingPrimaryDef"));
		MissingPrimaryDef->HandSlot = EWeaponHandSlot::MainHandOneHanded;
		MissingPrimaryDef->AttachSocketName = TEXT("Weapon_R");
		MissingPrimaryDef->WeaponMesh = SwordMesh;
		MissingPrimaryDef->BladeBaseSocketName = SocketNameTraceBase;
		MissingPrimaryDef->BladeTipSocketName = SocketNameTraceTip;
		MissingPrimaryDef->BaseGrantedActions.Add(UPrimaryAttackAbility::StaticClass());
		// PrimaryAttackAbilityTag left invalid (empty)
		FString MissingPrimaryReason;
		TestFalse(TEXT("14.3.1 IsValidWeaponDefinition rejects MainHand with missing PrimaryAttackAbilityTag"), MissingPrimaryDef->IsValidWeaponDefinition(MissingPrimaryReason));
		TestFalse(TEXT("14.3.1 Preflight rejects MainHand with missing PrimaryAttackAbilityTag"), EquipmentComp->TestDirectPreflight(MissingPrimaryDef, nullptr, MissingPrimaryReason));

		// 14.3.2 Unmatched PrimaryAttackAbilityTag
		UMeleeWeaponDefinition* UnmatchedPrimaryDef = NewObject<UMeleeWeaponDefinition>(GetTransientPackage(), TEXT("Test_UnmatchedPrimaryDef"));
		UnmatchedPrimaryDef->HandSlot = EWeaponHandSlot::MainHandOneHanded;
		UnmatchedPrimaryDef->AttachSocketName = TEXT("Weapon_R");
		UnmatchedPrimaryDef->WeaponMesh = SwordMesh;
		UnmatchedPrimaryDef->BladeBaseSocketName = SocketNameTraceBase;
		UnmatchedPrimaryDef->BladeTipSocketName = SocketNameTraceTip;
		UnmatchedPrimaryDef->PrimaryAttackAbilityTag = FGameplayTag::RequestGameplayTag(TEXT("Ability.Defense.Guard"));
		UnmatchedPrimaryDef->BaseGrantedActions.Add(UPrimaryAttackAbility::StaticClass());
		FString UnmatchedPrimaryReason;
		TestFalse(TEXT("14.3.2 IsValidWeaponDefinition rejects PrimaryAttackAbilityTag not matching any BaseGrantedAction CDO"), UnmatchedPrimaryDef->IsValidWeaponDefinition(UnmatchedPrimaryReason));

		// 14.3.3 Unmatched SprintAttackAbilityTag
		UMeleeWeaponDefinition* UnmatchedSprintDef = NewObject<UMeleeWeaponDefinition>(GetTransientPackage(), TEXT("Test_UnmatchedSprintDef"));
		UnmatchedSprintDef->HandSlot = EWeaponHandSlot::MainHandOneHanded;
		UnmatchedSprintDef->AttachSocketName = TEXT("Weapon_R");
		UnmatchedSprintDef->WeaponMesh = SwordMesh;
		UnmatchedSprintDef->BladeBaseSocketName = SocketNameTraceBase;
		UnmatchedSprintDef->BladeTipSocketName = SocketNameTraceTip;
		UnmatchedSprintDef->PrimaryAttackAbilityTag = TagAbilityPrimaryAttack;
		UnmatchedSprintDef->SprintAttackAbilityTag = TagAbilitySprintAttack;
		UnmatchedSprintDef->BaseGrantedActions.Add(UPrimaryAttackAbility::StaticClass());
		// BaseGrantedActions lacks USprintAttackAbility
		FString UnmatchedSprintReason;
		TestFalse(TEXT("14.3.3 IsValidWeaponDefinition rejects SprintAttackAbilityTag not matching any BaseGrantedAction CDO"), UnmatchedSprintDef->IsValidWeaponDefinition(UnmatchedSprintReason));

		// 14.4 OffHand Attack Tag Misconfiguration Rejection
		UOffHandWeaponDefinition* IllegalOffHandDef = NewObject<UOffHandWeaponDefinition>(GetTransientPackage(), TEXT("Test_IllegalOffHandDef"));
		IllegalOffHandDef->HandSlot = EWeaponHandSlot::OffHand;
		IllegalOffHandDef->LocomotionMode = EWeaponLocomotionMode::Default;
		IllegalOffHandDef->AttachSocketName = TEXT("Weapon_L");
		IllegalOffHandDef->WeaponMesh = ShieldMesh;
		IllegalOffHandDef->PrimaryAttackAbilityTag = TagAbilityPrimaryAttack;
		FString IllegalOffHandReason;
		TestFalse(TEXT("14.4 IsValidWeaponDefinition rejects OffHand with configured PrimaryAttackAbilityTag"), IllegalOffHandDef->IsValidWeaponDefinition(IllegalOffHandReason));

		// 14.5 Missing Sprint direct route returns false without stale route
		UMeleeWeaponDefinition* NoSprintDef = NewObject<UMeleeWeaponDefinition>(GetTransientPackage(), TEXT("Test_NoSprintDef"));
		NoSprintDef->HandSlot = EWeaponHandSlot::MainHandOneHanded;
		NoSprintDef->LocomotionMode = EWeaponLocomotionMode::LightSword;
		NoSprintDef->AttachSocketName = TEXT("Weapon_R");
		NoSprintDef->WeaponMesh = SwordMesh;
		NoSprintDef->BladeBaseSocketName = SocketNameTraceBase;
		NoSprintDef->BladeTipSocketName = SocketNameTraceTip;
		NoSprintDef->PrimaryAttackAbilityTag = TagAbilityPrimaryAttack;
		NoSprintDef->BaseGrantedActions.Add(UPrimaryAttackAbility::StaticClass());
		NoSprintDef->bUseOwnerMeshSocketForTrace = false;
		NoSprintDef->BladeBaseMarkerRelativeLocation = FVector(0, 0, 10);
		NoSprintDef->BladeTipMarkerRelativeLocation = FVector(0, 0, 100);
		NoSprintDef->TraceRadius = 10.0f;
		NoSprintDef->BladeSubdivisions = 4;

		TestTrue(TEXT("14.5 NoSprintDef equips successfully"), EquipmentComp->EquipWeapon(NoSprintDef));
		FGameplayTag EmptySprintTag;
		TestFalse(TEXT("14.5 TryGetSprintAttackAbilityTag returns false when sprint direct route is empty"), EquipmentComp->TryGetSprintAttackAbilityTag(EmptySprintTag));
		TestFalse(TEXT("14.5 EmptySprintTag remains invalid"), EmptySprintTag.IsValid());
	}

	Player->Destroy();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
