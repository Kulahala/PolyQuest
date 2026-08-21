#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/Abilities/BowDrawFireAbility.h"
#include "AbilitySystem/Abilities/DodgeAbility.h"
#include "AbilitySystem/Abilities/PrimaryAttackAbility.h"
#include "AbilitySystem/CharacterAttributeSet.h"
#include "Animation/AnimMontage.h"
#include "Character/Player/PlayerCharacter.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Combat/Equipment/BowWeaponDefinition.h"
#include "Combat/Equipment/ProjectileDefinition.h"
#include "Combat/Equipment/WeaponEquipmentComponent.h"
#include "Combat/Input/CombatLoadoutDefinition.h"
#include "Combat/Projectile/CombatProjectile.h"
#include "Combat/Projectile/CombatProjectileHitResolver.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "Tests/TestProjectileDamageGE.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProjectileLifecycleAutomationTest, "PolyQuest.Projectile.Lifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FProjectileLifecycleAutomationTest::RunTest(const FString& Parameters)
{
	// 1. Setup Test World with valid WorldContext
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("ProjectileLifecycleTestWorld"));
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

	const FGameplayTag TagInputPrimaryAttack = FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false);
	const FGameplayTag TagAbilityPrimaryAttack = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Primary")), false);
	const FGameplayTag TagBowDrawReady = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Attack.Bow.DrawReady")), false);
	const FGameplayTag TagBowRelease = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Attack.Bow.Release")), false);
	const FGameplayTag TagInputReleased = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Input.Released")), false);
	const FGameplayTag TagInputCanceled = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Input.Canceled")), false);
	const FGameplayTag TagDead = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
	const FGameplayTag TagInvulnerable = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Invulnerable")), false);
	const FGameplayTag TagCharging = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Charging")), false);
	const FGameplayTag TagTeamPlayer = FGameplayTag::RequestGameplayTag(FName(TEXT("Team.Player")), false);
	const FGameplayTag TagTeamEnemy = FGameplayTag::RequestGameplayTag(FName(TEXT("Team.Enemy")), false);

	TestTrue(TEXT("Tag Input.PrimaryAttack is valid"), TagInputPrimaryAttack.IsValid());
	TestTrue(TEXT("Tag Ability.Attack.Primary is valid"), TagAbilityPrimaryAttack.IsValid());
	TestTrue(TEXT("Tag Event.Attack.Bow.DrawReady is valid"), TagBowDrawReady.IsValid());
	TestTrue(TEXT("Tag Event.Attack.Bow.Release is valid"), TagBowRelease.IsValid());
	TestTrue(TEXT("Tag Event.Input.Released is valid"), TagInputReleased.IsValid());
	TestTrue(TEXT("Tag Event.Input.Canceled is valid"), TagInputCanceled.IsValid());
	TestTrue(TEXT("Tag State.Status.Dead is valid"), TagDead.IsValid());
	TestTrue(TEXT("Tag State.Status.Invulnerable is valid"), TagInvulnerable.IsValid());
	TestTrue(TEXT("Tag State.Action.Charging is valid"), TagCharging.IsValid());
	TestTrue(TEXT("Tag Team.Player is valid"), TagTeamPlayer.IsValid());
	TestTrue(TEXT("Tag Team.Enemy is valid"), TagTeamEnemy.IsValid());

	// -------------------------------------------------------------------------
	// SECTION 1: UProjectileDefinition Validation
	// -------------------------------------------------------------------------
	{
		UProjectileDefinition* ValidProjDef = NewObject<UProjectileDefinition>(GetTransientPackage(), TEXT("Test_ValidProjDef"));
		ValidProjDef->InitialSpeed = 3000.0f;
		ValidProjDef->MaxSpeed = 3000.0f;
		ValidProjDef->LifespanSeconds = 5.0f;
		ValidProjDef->CollisionRadius = 12.0f;
		ValidProjDef->DamageGameplayEffectClass = UTestProjectileDamageGE::StaticClass();

		FString Reason;
		TestTrue(TEXT("Valid ProjectileDefinition passes validation"), ValidProjDef->IsValidProjectileDefinition(Reason));

		// 1.1 Non-positive / Non-finite InitialSpeed
		ValidProjDef->InitialSpeed = 0.0f;
		TestFalse(TEXT("ProjectileDefinition rejects InitialSpeed == 0"), ValidProjDef->IsValidProjectileDefinition(Reason));

		ValidProjDef->InitialSpeed = -100.0f;
		TestFalse(TEXT("ProjectileDefinition rejects negative InitialSpeed"), ValidProjDef->IsValidProjectileDefinition(Reason));

		ValidProjDef->InitialSpeed = NAN;
		TestFalse(TEXT("ProjectileDefinition rejects NaN InitialSpeed"), ValidProjDef->IsValidProjectileDefinition(Reason));

		ValidProjDef->InitialSpeed = INFINITY;
		TestFalse(TEXT("ProjectileDefinition rejects +INF InitialSpeed"), ValidProjDef->IsValidProjectileDefinition(Reason));

		ValidProjDef->InitialSpeed = 3000.0f;

		// 1.2 MaxSpeed < InitialSpeed or non-finite
		ValidProjDef->MaxSpeed = 2000.0f;
		TestFalse(TEXT("ProjectileDefinition rejects MaxSpeed < InitialSpeed"), ValidProjDef->IsValidProjectileDefinition(Reason));

		ValidProjDef->MaxSpeed = NAN;
		TestFalse(TEXT("ProjectileDefinition rejects NaN MaxSpeed"), ValidProjDef->IsValidProjectileDefinition(Reason));

		ValidProjDef->MaxSpeed = 3000.0f;

		// 1.3 Lifespan <= 0 or non-finite
		ValidProjDef->LifespanSeconds = 0.0f;
		TestFalse(TEXT("ProjectileDefinition rejects LifespanSeconds == 0"), ValidProjDef->IsValidProjectileDefinition(Reason));

		ValidProjDef->LifespanSeconds = NAN;
		TestFalse(TEXT("ProjectileDefinition rejects NaN LifespanSeconds"), ValidProjDef->IsValidProjectileDefinition(Reason));

		ValidProjDef->LifespanSeconds = 5.0f;

		// 1.4 CollisionRadius <= 0 or non-finite
		ValidProjDef->CollisionRadius = -5.0f;
		TestFalse(TEXT("ProjectileDefinition rejects negative CollisionRadius"), ValidProjDef->IsValidProjectileDefinition(Reason));

		ValidProjDef->CollisionRadius = NAN;
		TestFalse(TEXT("ProjectileDefinition rejects NaN CollisionRadius"), ValidProjDef->IsValidProjectileDefinition(Reason));

		ValidProjDef->CollisionRadius = 12.0f;

		// 1.5 Missing DamageGameplayEffectClass
		ValidProjDef->DamageGameplayEffectClass = nullptr;
		TestFalse(TEXT("ProjectileDefinition rejects null DamageGameplayEffectClass"), ValidProjDef->IsValidProjectileDefinition(Reason));

		ValidProjDef->DamageGameplayEffectClass = UTestProjectileDamageGE::StaticClass();

		// 1.6 Non-finite DisplayScale or zero
		ValidProjDef->DisplayScale = FVector(0.0f, 1.0f, 1.0f);
		TestFalse(TEXT("ProjectileDefinition rejects zero DisplayScale"), ValidProjDef->IsValidProjectileDefinition(Reason));

		ValidProjDef->DisplayScale = FVector::OneVector;
		TestTrue(TEXT("Restored ProjectileDefinition passes validation"), ValidProjDef->IsValidProjectileDefinition(Reason));
	}

	// -------------------------------------------------------------------------
	// SECTION 2: UBowWeaponDefinition Validation (Isolation without asset mutation)
	// -------------------------------------------------------------------------
	{
		// Create transient static mesh without touching or mutating any Content assets
		UStaticMesh* TransientBowMesh = NewObject<UStaticMesh>(GetTransientPackage(), TEXT("Test_TransientBowMesh"));
		const FName SocketNameBowLaunch(TEXT("Socket_Bow_Launch"));
		UStaticMeshSocket* LaunchSocket = NewObject<UStaticMeshSocket>(TransientBowMesh);
		LaunchSocket->SocketName = SocketNameBowLaunch;
		LaunchSocket->RelativeLocation = FVector(0.0f, 0.0f, 20.0f);
		TransientBowMesh->AddSocket(LaunchSocket);

		UProjectileDefinition* ValidProjDef = NewObject<UProjectileDefinition>(GetTransientPackage(), TEXT("Test_BowProjDef"));
		ValidProjDef->InitialSpeed = 3000.0f;
		ValidProjDef->MaxSpeed = 3000.0f;
		ValidProjDef->LifespanSeconds = 5.0f;
		ValidProjDef->CollisionRadius = 12.0f;
		ValidProjDef->DamageGameplayEffectClass = UTestProjectileDamageGE::StaticClass();

		UCombatLoadoutDefinition* BowLoadout = NewObject<UCombatLoadoutDefinition>(GetTransientPackage(), TEXT("Test_BowLoadout"));
		BowLoadout->AddTestInputAbilityRoute(TagInputPrimaryAttack, TagAbilityPrimaryAttack);

		UBowWeaponDefinition* BowDef = NewObject<UBowWeaponDefinition>(GetTransientPackage(), TEXT("Test_BowDef"));
		TestEqual(TEXT("BowDef default AttachSocketName is Bow_L"), BowDef->AttachSocketName, FName(TEXT("Bow_L")));
		BowDef->HandSlot = EWeaponHandSlot::MainHandTwoHanded;
		BowDef->AttachSocketName = TEXT("Bow_L");
		BowDef->WeaponMesh = TransientBowMesh;
		BowDef->LaunchSocketName = SocketNameBowLaunch;
		BowDef->DefaultProjectileDefinition = ValidProjDef;
		BowDef->AssociatedLoadout = BowLoadout;
		BowDef->BaseGrantedActions.Add(UBowDrawFireAbility::StaticClass());

		FString Reason;
		TestTrue(TEXT("Valid BowWeaponDefinition passes validation"), BowDef->IsValidWeaponDefinition(Reason));

		// 2.1 HandSlot != MainHandTwoHanded
		BowDef->HandSlot = EWeaponHandSlot::MainHandOneHanded;
		TestFalse(TEXT("BowWeaponDefinition rejects non-TwoHanded slot"), BowDef->IsValidWeaponDefinition(Reason));
		BowDef->HandSlot = EWeaponHandSlot::MainHandTwoHanded;

		// 2.2 Missing WeaponMesh
		BowDef->WeaponMesh = nullptr;
		TestFalse(TEXT("BowWeaponDefinition rejects null WeaponMesh"), BowDef->IsValidWeaponDefinition(Reason));
		BowDef->WeaponMesh = TransientBowMesh;

		// 2.3 Non-existent Launch Socket
		BowDef->LaunchSocketName = TEXT("NonExistent_Socket_Name");
		TestFalse(TEXT("BowWeaponDefinition rejects missing LaunchSocket on mesh"), BowDef->IsValidWeaponDefinition(Reason));
		BowDef->LaunchSocketName = SocketNameBowLaunch;

		// 2.4 Missing / Invalid DefaultProjectileDefinition
		BowDef->DefaultProjectileDefinition = nullptr;
		TestFalse(TEXT("BowWeaponDefinition rejects null DefaultProjectileDefinition"), BowDef->IsValidWeaponDefinition(Reason));
		BowDef->DefaultProjectileDefinition = ValidProjDef;

		// 2.5 Loadout not mapping PrimaryAttack to Ability.Attack.Primary
		UCombatLoadoutDefinition* BadLoadout = NewObject<UCombatLoadoutDefinition>(GetTransientPackage(), TEXT("Test_BadBowLoadout"));
		BowDef->AssociatedLoadout = BadLoadout;
		TestFalse(TEXT("BowWeaponDefinition rejects Loadout missing PrimaryAttack mapping"), BowDef->IsValidWeaponDefinition(Reason));
		BowDef->AssociatedLoadout = BowLoadout;

		// 2.6 BaseGrantedActions granting melee UPrimaryAttackAbility
		BowDef->BaseGrantedActions.Reset();
		BowDef->BaseGrantedActions.Add(UPrimaryAttackAbility::StaticClass());
		TestFalse(TEXT("BowWeaponDefinition rejects granting UPrimaryAttackAbility"), BowDef->IsValidWeaponDefinition(Reason));

		// 2.7 BaseGrantedActions granting non-Bow ability carrying hierarchical Primary tag
		BowDef->BaseGrantedActions.Reset();
		BowDef->BaseGrantedActions.Add(UDodgeAbility::StaticClass());
		TestFalse(TEXT("BowWeaponDefinition rejects non-Primary action when Primary is required"), BowDef->IsValidWeaponDefinition(Reason));

		// 2.8 BaseGrantedActions with duplicate Primary actions
		BowDef->BaseGrantedActions.Reset();
		BowDef->BaseGrantedActions.Add(UBowDrawFireAbility::StaticClass());
		BowDef->BaseGrantedActions.Add(UBowDrawFireAbility::StaticClass());
		TestFalse(TEXT("BowWeaponDefinition rejects duplicate Primary actions"), BowDef->IsValidWeaponDefinition(Reason));

		BowDef->BaseGrantedActions.Reset();
		BowDef->BaseGrantedActions.Add(UBowDrawFireAbility::StaticClass());
		TestTrue(TEXT("Restored BowWeaponDefinition passes validation"), BowDef->IsValidWeaponDefinition(Reason));
	}

	// -------------------------------------------------------------------------
	// SECTION 3: ACombatProjectile Runtime Actor, Collision Policy & Contact Semantics
	// -------------------------------------------------------------------------
	{
		ACombatProjectile* Projectile = World->SpawnActor<ACombatProjectile>();
		TestNotNull(TEXT("Spawned ACombatProjectile actor"), Projectile);

		if (Projectile)
		{
			// Verify Root Component is USphereComponent
			USphereComponent* SphereComp = Projectile->GetCollisionComponent();
			TestNotNull(TEXT("Projectile collision component exists"), SphereComp);
			TestEqual(TEXT("Projectile RootComponent is CollisionComponent"), Projectile->GetRootComponent(), Cast<USceneComponent>(SphereComp));

			// Verify collision policy: QueryOnly, ignores Camera
			TestEqual(TEXT("Collision is QueryOnly"), SphereComp->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);
			TestEqual(TEXT("Collision ignores ECC_Camera"), SphereComp->GetCollisionResponseToChannel(ECC_Camera), ECollisionResponse::ECR_Ignore);
			TestEqual(TEXT("Display mesh ignores ECC_Camera"), Projectile->GetMeshComponent()->GetCollisionResponseToChannel(ECC_Camera), ECollisionResponse::ECR_Ignore);
			TestEqual(TEXT("Display mesh has NoCollision"), Projectile->GetMeshComponent()->GetCollisionEnabled(), ECollisionEnabled::NoCollision);

			// Verify movement dynamics: 0 gravity, no bounce, no homing
			UProjectileMovementComponent* MovementComp = Projectile->GetMovementComponent();
			TestNotNull(TEXT("Projectile movement component exists"), MovementComp);
			TestEqual(TEXT("Projectile GravityScale is 0.0"), MovementComp->ProjectileGravityScale, 0.0f);
			TestFalse(TEXT("Projectile bShouldBounce is false"), MovementComp->bShouldBounce);
			TestFalse(TEXT("Projectile bIsHomingProjectile is false"), MovementComp->bIsHomingProjectile);

			// Test initialization with valid definition
			UProjectileDefinition* ProjDef = NewObject<UProjectileDefinition>(GetTransientPackage(), TEXT("Test_RuntimeProjDef"));
			ProjDef->InitialSpeed = 2500.0f;
			ProjDef->MaxSpeed = 2500.0f;
			ProjDef->LifespanSeconds = 4.0f;
			ProjDef->CollisionRadius = 15.0f;
			ProjDef->DamageGameplayEffectClass = UTestProjectileDamageGE::StaticClass();

			FCombatProjectileLaunchRequest LaunchReq;
			LaunchReq.Definition = ProjDef;

			TestTrue(TEXT("InitializeProjectile succeeds with valid definition"), Projectile->InitializeProjectile(LaunchReq));
			TestEqual(TEXT("Sphere radius updated from definition"), SphereComp->GetUnscaledSphereRadius(), 15.0f);
			TestEqual(TEXT("Movement speed updated from definition"), MovementComp->InitialSpeed, 2500.0f);
			TestEqual(TEXT("Projectile lifespan set from definition"), Projectile->GetLifeSpan(), 4.0f);

			// 3.1 Non-pawn blocking impact destroys projectile without applying damage
			ACombatProjectile* BlockProj = World->SpawnActor<ACombatProjectile>();
			if (BlockProj)
			{
				BlockProj->InitializeProjectile(LaunchReq);
				FHitResult WallHit;
				WallHit.bBlockingHit = true;
				WallHit.ImpactPoint = FVector(100.0f, 0.0f, 0.0f);
				// Calling OnProjectileHit with nullptr OtherActor (simulating static geometry block)
				BlockProj->DispatchBeginPlay();
				BlockProj->GetCollisionComponent()->OnComponentHit.Broadcast(
					BlockProj->GetCollisionComponent(),
					nullptr,
					nullptr,
					FVector::ZeroVector,
					WallHit);
				TestTrue(TEXT("Projectile hitting static world geometry is destroyed"), BlockProj->IsActorBeingDestroyed());
			}

			Projectile->Destroy();
		}
	}

	// -------------------------------------------------------------------------
	// SECTION 4: FCombatProjectileHitResolver GAS Delivery & Observable Effect
	// -------------------------------------------------------------------------
	{
		APlayerCharacter* PlayerSource = World->SpawnActor<APlayerCharacter>();
		AEnemyCharacter* EnemyTarget = World->SpawnActor<AEnemyCharacter>();
		TestNotNull(TEXT("Player source spawned"), PlayerSource);
		TestNotNull(TEXT("Enemy target spawned"), EnemyTarget);

		if (PlayerSource && EnemyTarget)
		{
			PlayerSource->SetActorLocation(FVector(0.0f, 0.0f, 50.0f));
			EnemyTarget->SetActorLocation(FVector(200.0f, 0.0f, 50.0f));
			PlayerSource->SetTestCombatTeamTag(TagTeamPlayer);
			EnemyTarget->SetTestCombatTeamTag(TagTeamEnemy);
			PlayerSource->DispatchBeginPlay();
			EnemyTarget->DispatchBeginPlay();

			UAbilitySystemComponent* SourceASC = PlayerSource->GetAbilitySystemComponent();
			UAbilitySystemComponent* TargetASC = EnemyTarget->GetAbilitySystemComponent();

			TestNotNull(TEXT("Player source ASC exists"), SourceASC);
			TestNotNull(TEXT("Enemy target ASC exists"), TargetASC);

			// Target initial health
			const float InitialHealth = TargetASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute());
			TestTrue(TEXT("Target initial Health is positive"), InitialHealth > 0.0f);

			FCombatProjectileHitRequest ValidRequest;
			ValidRequest.SourceActor = PlayerSource;
			ValidRequest.SourceAbilitySystemComponent = SourceASC;
			ValidRequest.DamageGameplayEffectClass = UTestProjectileDamageGE::StaticClass();
			ValidRequest.HitResult = FHitResult(EnemyTarget, nullptr, EnemyTarget->GetActorLocation(), FVector::UpVector);

			// Diagnostic precondition assertions for HitResolver
			ICombatTeamAgent* SourceAgent = Cast<ICombatTeamAgent>(PlayerSource);
			ICombatTeamAgent* TargetAgent = Cast<ICombatTeamAgent>(EnemyTarget);
			TestNotNull(TEXT("Source implements ICombatTeamAgent"), SourceAgent);
			TestNotNull(TEXT("Target implements ICombatTeamAgent"), TargetAgent);

			const FGameplayTag SourceTeam = SourceAgent ? SourceAgent->GetCombatTeamTag_Implementation() : FGameplayTag();
			const FGameplayTag TargetTeam = TargetAgent ? TargetAgent->GetCombatTeamTag_Implementation() : FGameplayTag();
			TestTrue(TEXT("Source CombatTeamTag is valid"), SourceTeam.IsValid());
			TestTrue(TEXT("Target CombatTeamTag is valid"), TargetTeam.IsValid());
			TestFalse(TEXT("Source and Target have different team tags"), SourceTeam.MatchesTagExact(TargetTeam));

			// 4.1 Valid hit to hostile enemy target applies Damage GE and observes Health reduction
			TestTrue(TEXT("HitResolver applies Damage GE to hostile enemy target"), FCombatProjectileHitResolver::TryResolveHit(ValidRequest));
			const float PostHitHealth = TargetASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute());
			TestEqual(TEXT("Target Health reduced by 25.0 after projectile hit"), PostHitHealth, InitialHealth - 25.0f);

			// 4.2 Self hit rejection
			FCombatProjectileHitRequest SelfRequest = ValidRequest;
			SelfRequest.HitResult = FHitResult(PlayerSource, nullptr, PlayerSource->GetActorLocation(), FVector::UpVector);
			TestFalse(TEXT("HitResolver rejects hit against self"), FCombatProjectileHitResolver::TryResolveHit(SelfRequest));

			// 4.3 Same-team hit rejection & projectile ignore behavior
			APlayerCharacter* FriendlyPlayer = World->SpawnActor<APlayerCharacter>();
			if (FriendlyPlayer)
			{
				FriendlyPlayer->SetTestCombatTeamTag(TagTeamPlayer);
				FriendlyPlayer->DispatchBeginPlay();
				FCombatProjectileHitRequest TeamRequest = ValidRequest;
				TeamRequest.HitResult = FHitResult(FriendlyPlayer, nullptr, FriendlyPlayer->GetActorLocation(), FVector::UpVector);
				TestFalse(TEXT("HitResolver rejects hit against same team"), FCombatProjectileHitResolver::TryResolveHit(TeamRequest));

				// Test that projectile overlapping friendly pawn continues flight without destroying
				ACombatProjectile* FriendlyOverlapProj = World->SpawnActor<ACombatProjectile>();
				if (FriendlyOverlapProj)
				{
					UProjectileDefinition* ProjDef = NewObject<UProjectileDefinition>(GetTransientPackage(), TEXT("Test_TeamProjDef"));
					ProjDef->InitialSpeed = 3000.0f;
					ProjDef->MaxSpeed = 3000.0f;
					ProjDef->LifespanSeconds = 5.0f;
					ProjDef->CollisionRadius = 12.0f;
					ProjDef->DamageGameplayEffectClass = UTestProjectileDamageGE::StaticClass();

					FCombatProjectileLaunchRequest TeamLaunchReq;
					TeamLaunchReq.Definition = ProjDef;
					TeamLaunchReq.SourceActor = PlayerSource;
					TeamLaunchReq.SourceAbilitySystemComponent = SourceASC;

					FriendlyOverlapProj->InitializeProjectile(TeamLaunchReq);
					FriendlyOverlapProj->DispatchBeginPlay();

					FriendlyOverlapProj->GetCollisionComponent()->OnComponentBeginOverlap.Broadcast(
						FriendlyOverlapProj->GetCollisionComponent(),
						FriendlyPlayer,
						Cast<UPrimitiveComponent>(FriendlyPlayer->GetRootComponent()),
						0,
						false,
						FHitResult());

					TestFalse(TEXT("Projectile overlapping friendly player is NOT destroyed (continues flight)"), FriendlyOverlapProj->IsActorBeingDestroyed());
					FriendlyOverlapProj->Destroy();
				}

				FriendlyPlayer->Destroy();
			}

			// 4.4 Dead target rejection & projectile ignore behavior
			TargetASC->AddLooseGameplayTag(TagDead);
			TestFalse(TEXT("HitResolver rejects hit against dead target"), FCombatProjectileHitResolver::TryResolveHit(ValidRequest));
			TargetASC->RemoveLooseGameplayTag(TagDead);

			// 4.5 Invulnerable target rejection & projectile ignore behavior
			TargetASC->AddLooseGameplayTag(TagInvulnerable);
			TestFalse(TEXT("HitResolver rejects hit against invulnerable target"), FCombatProjectileHitResolver::TryResolveHit(ValidRequest));
			TargetASC->RemoveLooseGameplayTag(TagInvulnerable);

			// 4.6 Dead source rejection
			SourceASC->AddLooseGameplayTag(TagDead);
			TestFalse(TEXT("HitResolver rejects hit from dead source"), FCombatProjectileHitResolver::TryResolveHit(ValidRequest));
			SourceASC->RemoveLooseGameplayTag(TagDead);

			// 4.7 End-to-end projectile overlap hit delivery relies on internal GE snapshot, NOT post-launch Definition
			AEnemyCharacter* FreshEnemyTarget = World->SpawnActor<AEnemyCharacter>();
			if (FreshEnemyTarget)
			{
				FreshEnemyTarget->SetActorLocation(FVector(200.0f, 0.0f, 50.0f));
				FreshEnemyTarget->SetTestCombatTeamTag(TagTeamEnemy);
				FreshEnemyTarget->DispatchBeginPlay();

				UAbilitySystemComponent* FreshTargetASC = FreshEnemyTarget->GetAbilitySystemComponent();
				TestNotNull(TEXT("Fresh enemy target ASC exists"), FreshTargetASC);

				if (FreshTargetASC)
				{
					const float PreSnapshotHitHealth = FreshTargetASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute());
					TestTrue(TEXT("Fresh target initial Health is positive"), PreSnapshotHitHealth > 0.0f);

					ACombatProjectile* SnapshotProj = World->SpawnActor<ACombatProjectile>();
					if (SnapshotProj)
					{
						UProjectileDefinition* SnapshotDef = NewObject<UProjectileDefinition>(GetTransientPackage(), TEXT("Test_SnapshotDef"));
						SnapshotDef->InitialSpeed = 3000.0f;
						SnapshotDef->MaxSpeed = 3000.0f;
						SnapshotDef->LifespanSeconds = 5.0f;
						SnapshotDef->CollisionRadius = 12.0f;
						SnapshotDef->DamageGameplayEffectClass = UTestProjectileDamageGE::StaticClass();

						FCombatProjectileLaunchRequest SnapshotLaunchReq;
						SnapshotLaunchReq.Definition = SnapshotDef;
						SnapshotLaunchReq.SourceActor = PlayerSource;
						SnapshotLaunchReq.SourceAbilitySystemComponent = SourceASC;

						TestTrue(TEXT("InitializeProjectile succeeds for snapshot test"), SnapshotProj->InitializeProjectile(SnapshotLaunchReq));
						TestTrue(TEXT("Projectile has valid Damage GE snapshot"), SnapshotProj->GetTestCachedDamageGameplayEffectClass() == UTestProjectileDamageGE::StaticClass());

						// Corrupt the Definition object after launch initialization to prove runtime hit delivery does NOT access Definition
						SnapshotDef->DamageGameplayEffectClass = nullptr;

						SnapshotProj->DispatchBeginPlay();

						FHitResult SweepHit(FreshEnemyTarget, Cast<UPrimitiveComponent>(FreshEnemyTarget->GetRootComponent()), FreshEnemyTarget->GetActorLocation(), FVector::UpVector);
						SnapshotProj->GetCollisionComponent()->OnComponentBeginOverlap.Broadcast(
							SnapshotProj->GetCollisionComponent(),
							FreshEnemyTarget,
							Cast<UPrimitiveComponent>(FreshEnemyTarget->GetRootComponent()),
							0,
							true,
							SweepHit);

						const float PostSnapshotHitHealth = FreshTargetASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute());
						TestEqual(TEXT("Target Health reduced by snapshot GE even after Definition was corrupted"), PostSnapshotHitHealth, PreSnapshotHitHealth - 25.0f);
						TestTrue(TEXT("Projectile successfully delivered hit and is destroyed"), SnapshotProj->IsActorBeingDestroyed());
					}
				}

				FreshEnemyTarget->Destroy();
			}

			PlayerSource->Destroy();
			EnemyTarget->Destroy();
		}
	}

	// -------------------------------------------------------------------------
	// SECTION 5: UBowDrawFireAbility Lifecycle, Identity Gate & Event Validation
	// -------------------------------------------------------------------------
	{
		APlayerCharacter* Player = World->SpawnActor<APlayerCharacter>();
		APlayerCharacter* OtherActor = World->SpawnActor<APlayerCharacter>();
		TestNotNull(TEXT("Player character spawned for ability tests"), Player);
		TestNotNull(TEXT("Other character spawned for ability tests"), OtherActor);

		if (Player && OtherActor)
		{
			Player->SetTestCombatTeamTag(TagTeamPlayer);
			OtherActor->SetTestCombatTeamTag(TagTeamPlayer);
			Player->DispatchBeginPlay();
			OtherActor->DispatchBeginPlay();
			UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
			TestNotNull(TEXT("Player ASC exists"), ASC);

			if (ASC)
			{
				FGameplayAbilitySpec BowSpec(UBowDrawFireAbility::StaticClass(), 1, INDEX_NONE, Player);
				const FGameplayAbilitySpecHandle BowSpecHandle = ASC->GiveAbility(BowSpec);
				TestTrue(TEXT("Bow ability spec handle is valid"), BowSpecHandle.IsValid());

				// 5.1 CanActivateAbility and TryActivateAbility reject when Input.PrimaryAttack is NOT held
				const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(BowSpecHandle);
				TestNotNull(TEXT("Bow ability spec found"), Spec);
				if (Spec && Spec->Ability)
				{
					TestFalse(TEXT("CanActivateAbility rejects when PrimaryAttack input is not held"),
						Spec->Ability->CanActivateAbility(BowSpecHandle, ASC->AbilityActorInfo.Get()));
				}
				TestFalse(TEXT("TryActivateAbility fails when PrimaryAttack input is not held"),
					ASC->TryActivateAbility(BowSpecHandle));

				// 5.2 Ability Tags contain Ability.Attack.Primary and State.Action.Charging
				const UBowDrawFireAbility* BowCDO = UBowDrawFireAbility::StaticClass()->GetDefaultObject<UBowDrawFireAbility>();
				TestNotNull(TEXT("Bow ability CDO exists"), BowCDO);
				if (BowCDO)
				{
					TestTrue(TEXT("Bow CDO carries Ability.Attack.Primary tag"), BowCDO->AbilityTags.HasTagExact(TagAbilityPrimaryAttack));
					TestTrue(TEXT("Bow CDO carries State.Action.Charging in ActivationOwnedTags"), BowCDO->GetTestActivationOwnedTags().HasTagExact(TagCharging));
				}

				// 5.3 Unified Avatar Identity Validation for Events
				UBowDrawFireAbility* BowAbility = Spec ? Cast<UBowDrawFireAbility>(Spec->GetPrimaryInstance()) : nullptr;
				if (!BowAbility)
				{
					BowAbility = NewObject<UBowDrawFireAbility>(Player, TEXT("Test_BowAbilityInstance"));
					BowAbility->SetTestCurrentActorInfo(ASC->AbilityActorInfo.Get());
				}
				TestNotNull(TEXT("Bow ability instance created for event gate testing"), BowAbility);

				if (BowAbility)
				{
					UAnimMontage* MockMontage = NewObject<UAnimMontage>(GetTransientPackage(), TEXT("Test_MockBowMontage"));
					UAnimMontage* WrongMontage = NewObject<UAnimMontage>(GetTransientPackage(), TEXT("Test_WrongMontage"));
					BowAbility->SetTestBowMontage(MockMontage);

					// --- 5.3a DrawReady Event Gates ---
					BowAbility->SetTestBowStateDrawing();

					// Missing Instigator: must not advance state
					FGameplayEventData MissingInstigatorEvent;
					MissingInstigatorEvent.Target = Player;
					MissingInstigatorEvent.OptionalObject = MockMontage;
					BowAbility->TestOnDrawReadyEvent(MissingInstigatorEvent);
					TestEqual(TEXT("DrawReady with missing Instigator does not advance BowState"), BowAbility->GetTestBowState(), (uint8)1 /* Drawing */);

					// Missing Target: must not advance state
					FGameplayEventData MissingTargetEvent;
					MissingTargetEvent.Instigator = Player;
					MissingTargetEvent.OptionalObject = MockMontage;
					BowAbility->TestOnDrawReadyEvent(MissingTargetEvent);
					TestEqual(TEXT("DrawReady with missing Target does not advance BowState"), BowAbility->GetTestBowState(), (uint8)1 /* Drawing */);

					// Wrong Instigator: must not advance state
					FGameplayEventData WrongInstigatorEvent;
					WrongInstigatorEvent.Instigator = OtherActor;
					WrongInstigatorEvent.Target = Player;
					WrongInstigatorEvent.OptionalObject = MockMontage;
					BowAbility->TestOnDrawReadyEvent(WrongInstigatorEvent);
					TestEqual(TEXT("DrawReady with wrong Instigator does not advance BowState"), BowAbility->GetTestBowState(), (uint8)1 /* Drawing */);

					// Wrong Target: must not advance state
					FGameplayEventData WrongTargetEvent;
					WrongTargetEvent.Instigator = Player;
					WrongTargetEvent.Target = OtherActor;
					WrongTargetEvent.OptionalObject = MockMontage;
					BowAbility->TestOnDrawReadyEvent(WrongTargetEvent);
					TestEqual(TEXT("DrawReady with wrong Target does not advance BowState"), BowAbility->GetTestBowState(), (uint8)1 /* Drawing */);

					// Wrong Montage: must not advance state
					FGameplayEventData WrongMontageEvent;
					WrongMontageEvent.Instigator = Player;
					WrongMontageEvent.Target = Player;
					WrongMontageEvent.OptionalObject = WrongMontage;
					BowAbility->TestOnDrawReadyEvent(WrongMontageEvent);
					TestEqual(TEXT("DrawReady with wrong Montage does not advance BowState"), BowAbility->GetTestBowState(), (uint8)1 /* Drawing */);

					// Valid Identity & Montage: successfully advances Drawing -> Holding
					FGameplayEventData ValidDrawReadyEvent;
					ValidDrawReadyEvent.Instigator = Player;
					ValidDrawReadyEvent.Target = Player;
					ValidDrawReadyEvent.OptionalObject = MockMontage;
					BowAbility->TestOnDrawReadyEvent(ValidDrawReadyEvent);
					TestEqual(TEXT("DrawReady with valid Avatar identity advances to Holding"), BowAbility->GetTestBowState(), (uint8)2 /* Holding */);

					// --- 5.3b InputReleased Event Gates ---
					BowAbility->SetTestBowStateDrawing();

					// Wrong identity on InputReleased: does not record release request
					FGameplayEventData WrongInputReleaseEvent;
					WrongInputReleaseEvent.Instigator = OtherActor;
					WrongInputReleaseEvent.Target = Player;
					WrongInputReleaseEvent.InstigatorTags.AddTag(TagInputPrimaryAttack);
					BowAbility->TestOnInputReleased(WrongInputReleaseEvent);
					TestFalse(TEXT("InputReleased with wrong identity does not record release request"), BowAbility->GetTestReleaseRequested());

					// Wrong tag on InputReleased: does not record release request
					FGameplayEventData WrongTagInputReleaseEvent;
					WrongTagInputReleaseEvent.Instigator = Player;
					WrongTagInputReleaseEvent.Target = Player;
					WrongTagInputReleaseEvent.InstigatorTags.AddTag(TagDead);
					BowAbility->TestOnInputReleased(WrongTagInputReleaseEvent);
					TestFalse(TEXT("InputReleased with wrong tag does not record release request"), BowAbility->GetTestReleaseRequested());

					// Valid InputReleased during Drawing: records release request
					FGameplayEventData ValidInputReleaseEvent;
					ValidInputReleaseEvent.Instigator = Player;
					ValidInputReleaseEvent.Target = Player;
					ValidInputReleaseEvent.InstigatorTags.AddTag(TagInputPrimaryAttack);
					BowAbility->TestOnInputReleased(ValidInputReleaseEvent);
					TestTrue(TEXT("InputReleased with valid identity & tag records release request"), BowAbility->GetTestReleaseRequested());

					// --- 5.3c Release Animation Event Gates ---
					// In Holding state (not yet Releasing): Release event must NOT spawn projectile
					BowAbility->SetTestBowStateHolding();
					FGameplayEventData ValidReleaseAnimEvent;
					ValidReleaseAnimEvent.Instigator = Player;
					ValidReleaseAnimEvent.Target = Player;
					ValidReleaseAnimEvent.OptionalObject = MockMontage;
					BowAbility->TestOnReleaseAnimEvent(ValidReleaseAnimEvent);
					TestFalse(TEXT("Release event while in Holding state does not spawn projectile"), BowAbility->GetTestSpawnedProjectile());

					// In Releasing state but with wrong identity: must NOT spawn projectile
					BowAbility->SetTestBowStateReleasing();
					FGameplayEventData WrongIdentityReleaseAnimEvent;
					WrongIdentityReleaseAnimEvent.Instigator = OtherActor;
					WrongIdentityReleaseAnimEvent.Target = Player;
					WrongIdentityReleaseAnimEvent.OptionalObject = MockMontage;
					BowAbility->TestOnReleaseAnimEvent(WrongIdentityReleaseAnimEvent);
					TestFalse(TEXT("Release event with wrong identity does not spawn projectile"), BowAbility->GetTestSpawnedProjectile());
				}
			}

			Player->Destroy();
			OtherActor->Destroy();
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
