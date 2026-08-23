#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/Abilities/BowDrawFireAbility.h"
#include "AbilitySystem/Abilities/DodgeAbility.h"
#include "AbilitySystem/Abilities/PrimaryAttackAbility.h"
#include "AbilitySystem/CharacterAttributeSet.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimComposite.h"
#include "Character/Player/PlayerCharacter.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Combat/Equipment/BowWeaponDefinition.h"
#include "Combat/Equipment/ProjectileDefinition.h"
#include "Combat/Equipment/WeaponEquipmentComponent.h"
#include "Combat/Input/CombatLoadoutDefinition.h"
#include "Combat/Projectile/CombatProjectile.h"
#include "Combat/Projectile/CombatProjectileHitResolver.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "Tests/CombatAutomationFixture.h"
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
		APlayerCharacter* PlayerSource = FCombatAutomationFixture::SpawnPlayer(World);
		AEnemyCharacter* EnemyTarget = FCombatAutomationFixture::SpawnPassiveEnemy(World);
		TestNotNull(TEXT("Player source spawned"), PlayerSource);
		TestNotNull(TEXT("Enemy target spawned"), EnemyTarget);

		if (PlayerSource && EnemyTarget)
		{
			PlayerSource->SetActorLocation(FVector(0.0f, 0.0f, 50.0f));
			EnemyTarget->SetActorLocation(FVector(200.0f, 0.0f, 50.0f));
			PlayerSource->SetTestCombatTeamTag(TagTeamPlayer);
			EnemyTarget->SetTestCombatTeamTag(TagTeamEnemy);
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
			APlayerCharacter* FriendlyPlayer = FCombatAutomationFixture::SpawnPlayer(World);
			if (FriendlyPlayer)
			{
				FriendlyPlayer->SetTestCombatTeamTag(TagTeamPlayer);
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
			AEnemyCharacter* FreshEnemyTarget = FCombatAutomationFixture::SpawnPassiveEnemy(World);
			if (FreshEnemyTarget)
			{
				FreshEnemyTarget->SetActorLocation(FVector(200.0f, 0.0f, 50.0f));
				FreshEnemyTarget->SetTestCombatTeamTag(TagTeamEnemy);

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
		APlayerCharacter* Player = FCombatAutomationFixture::SpawnPlayer(World);
		APlayerCharacter* OtherActor = FCombatAutomationFixture::SpawnPlayer(World);
		TestNotNull(TEXT("Player character spawned for ability tests"), Player);
		TestNotNull(TEXT("Other character spawned for ability tests"), OtherActor);

		if (Player && OtherActor)
		{
			Player->SetTestCombatTeamTag(TagTeamPlayer);
			OtherActor->SetTestCombatTeamTag(TagTeamPlayer);
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

				// 5.2 Ability Tags contain Ability.Attack.Primary and do not statically carry State.Action.Charging
				const UBowDrawFireAbility* BowCDO = UBowDrawFireAbility::StaticClass()->GetDefaultObject<UBowDrawFireAbility>();
				TestNotNull(TEXT("Bow ability CDO exists"), BowCDO);
				if (BowCDO)
				{
					TestTrue(TEXT("Bow CDO carries Ability.Attack.Primary tag"), BowCDO->AbilityTags.HasTagExact(TagAbilityPrimaryAttack));
					TestFalse(TEXT("Bow CDO does not carry State.Action.Charging in AbilityTags"), BowCDO->AbilityTags.HasTagExact(TagCharging));
					TestFalse(TEXT("Bow CDO does not carry State.Action.Charging in ActivationOwnedTags"), BowCDO->GetTestActivationOwnedTags().HasTagExact(TagCharging));
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
					UAnimComposite* InnerSequence = NewObject<UAnimComposite>(GetTransientPackage(), TEXT("Test_BowInnerSequence"));
					UAnimComposite* ForeignSequence = NewObject<UAnimComposite>(GetTransientPackage(), TEXT("Test_BowForeignSequence"));

					FSlotAnimationTrack SlotTrack;
					SlotTrack.SlotName = FName(TEXT("DefaultSlot"));
					FAnimSegment Segment;
					Segment.SetAnimReference(InnerSequence);
					Segment.StartPos = 0.0f;
					Segment.AnimStartTime = 0.0f;
					Segment.AnimEndTime = 1.0f;
					Segment.AnimPlayRate = 1.0f;
					SlotTrack.AnimTrack.AnimSegments.Add(Segment);
					MockMontage->SlotAnimTracks.Add(SlotTrack);

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

					// Foreign Sequence not in Montage: must not advance state
					FGameplayEventData ForeignSequenceEvent;
					ForeignSequenceEvent.Instigator = Player;
					ForeignSequenceEvent.Target = Player;
					ForeignSequenceEvent.OptionalObject = ForeignSequence;
					BowAbility->TestOnDrawReadyEvent(ForeignSequenceEvent);
					TestEqual(TEXT("DrawReady with foreign Sequence does not advance BowState"), BowAbility->GetTestBowState(), (uint8)1 /* Drawing */);

					// Valid Montage Inner Sequence: successfully advances Drawing -> Holding
					FGameplayEventData ValidInnerSequenceEvent;
					ValidInnerSequenceEvent.Instigator = Player;
					ValidInnerSequenceEvent.Target = Player;
					ValidInnerSequenceEvent.OptionalObject = InnerSequence;
					BowAbility->TestOnDrawReadyEvent(ValidInnerSequenceEvent);
					TestEqual(TEXT("DrawReady with Montage inner Sequence advances to Holding"), BowAbility->GetTestBowState(), (uint8)2 /* Holding */);

					// Valid Direct Montage: successfully advances Drawing -> Holding
					BowAbility->SetTestBowStateDrawing();
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

					// In Releasing state but with foreign Sequence: rejected by identity gate
					FGameplayEventData ForeignSeqReleaseAnimEvent;
					ForeignSeqReleaseAnimEvent.Instigator = Player;
					ForeignSeqReleaseAnimEvent.Target = Player;
					ForeignSeqReleaseAnimEvent.OptionalObject = ForeignSequence;
					BowAbility->TestOnReleaseAnimEvent(ForeignSeqReleaseAnimEvent);
					TestFalse(TEXT("Release event with foreign Sequence rejected by gate"), BowAbility->Test_IsGameplayEventFromActiveMontage(ForeignSeqReleaseAnimEvent));

					// In Releasing state with valid Montage Inner Sequence: accepted by identity gate
					FGameplayEventData ValidInnerSeqReleaseAnimEvent;
					ValidInnerSeqReleaseAnimEvent.Instigator = Player;
					ValidInnerSeqReleaseAnimEvent.Target = Player;
					ValidInnerSeqReleaseAnimEvent.OptionalObject = InnerSequence;
					TestTrue(TEXT("Release event with Montage inner Sequence accepted by gate"), BowAbility->Test_IsGameplayEventFromActiveMontage(ValidInnerSeqReleaseAnimEvent));
				}
			}

			Player->Destroy();
			OtherActor->Destroy();
		}
	}

	// -------------------------------------------------------------------------
	// SECTION 6: Bow Release Locked-Target Preference
	// -------------------------------------------------------------------------
	{
		APlayerCharacter* Player = FCombatAutomationFixture::SpawnPlayer(World, FTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, 100.0f)));
		APlayerController* PlayerController = World->SpawnActor<APlayerController>();
		AEnemyCharacter* LockedEnemy = FCombatAutomationFixture::SpawnPassiveEnemy(World, FTransform(FRotator::ZeroRotator, FVector(600.0f, 600.0f, 100.0f)));
		AEnemyCharacter* AutomaticEnemy = FCombatAutomationFixture::SpawnPassiveEnemy(World, FTransform(FRotator::ZeroRotator, FVector(800.0f, 0.0f, 100.0f)));
		TestNotNull(TEXT("B3 Player spawned"), Player);
		TestNotNull(TEXT("B3 PlayerController spawned"), PlayerController);
		TestNotNull(TEXT("B3 locked Enemy spawned"), LockedEnemy);
		TestNotNull(TEXT("B3 automatic Enemy spawned"), AutomaticEnemy);
		const FName BowAttachSocketName(TEXT("Weapon_R"));
		USkeletalMesh* HeroMesh = Cast<USkeletalMesh>(StaticLoadObject(
			USkeletalMesh::StaticClass(), nullptr, TEXT("/Game/PolygonDungeons/Meshes/Characters/SK_Character_Hero_Knight_Male")));
		TestNotNull(TEXT("B3 Hero skeletal mesh loaded"), HeroMesh);
		TestNotNull(TEXT("B3 Hero mesh contains the equipment attach socket"), HeroMesh ? HeroMesh->FindSocket(BowAttachSocketName) : nullptr);

		if (Player && PlayerController && LockedEnemy && AutomaticEnemy && HeroMesh && HeroMesh->FindSocket(BowAttachSocketName))
		{
			if (Player->GetMesh())
			{
				Player->GetMesh()->SetSkeletalMeshAsset(HeroMesh);
			}

			Player->SetTestCombatTeamTag(TagTeamPlayer);
			LockedEnemy->SetTestCombatTeamTag(TagTeamEnemy);
			AutomaticEnemy->SetTestCombatTeamTag(TagTeamEnemy);
			TestTrue(TEXT("B3 Player fixture applied its persistent Stamina regen effect"), Player->HasTestStaminaRegenEffectApplied());
			PlayerController->Possess(Player);

			UAbilitySystemComponent* PlayerASC = Player->GetAbilitySystemComponent();
			UAbilitySystemComponent* LockedASC = LockedEnemy->GetAbilitySystemComponent();
			TestNotNull(TEXT("B3 Player ASC exists"), PlayerASC);
			TestNotNull(TEXT("B3 locked Enemy ASC exists"), LockedASC);

			Player->SetTestLockOnProjectionHook([](const FVector& WorldPoint, FVector2D& OutScreenPosition, FVector2D& OutViewportSize)
			{
				OutViewportSize = FVector2D(1920.0f, 1080.0f);
				if (WorldPoint.Y > 200.0f)
				{
					OutScreenPosition = FVector2D(1200.0f, 700.0f);
				}
				else if (WorldPoint.X > 100.0f)
				{
					OutScreenPosition = FVector2D(1200.0f, 540.0f);
				}
				else
				{
					OutScreenPosition = FVector2D(960.0f, 540.0f);
				}
				return true;
			});

			UStaticMesh* BowMesh = NewObject<UStaticMesh>(GetTransientPackage(), TEXT("Test_B3BowMesh"));
			const FName BowLaunchSocketName(TEXT("Socket_Bow_Launch"));
			UStaticMeshSocket* BowLaunchSocket = NewObject<UStaticMeshSocket>(BowMesh);
			BowLaunchSocket->SocketName = BowLaunchSocketName;
			BowLaunchSocket->RelativeLocation = FVector(0.0f, 0.0f, 20.0f);
			BowMesh->AddSocket(BowLaunchSocket);

			UProjectileDefinition* ProjectileDefinition = NewObject<UProjectileDefinition>(GetTransientPackage(), TEXT("Test_B3ProjectileDefinition"));
			ProjectileDefinition->InitialSpeed = 3000.0f;
			ProjectileDefinition->MaxSpeed = 3000.0f;
			ProjectileDefinition->LifespanSeconds = 5.0f;
			ProjectileDefinition->CollisionRadius = 12.0f;
			ProjectileDefinition->DamageGameplayEffectClass = UTestProjectileDamageGE::StaticClass();
			ProjectileDefinition->bEnableTargetAssist = true;
			ProjectileDefinition->TargetAssistMaxDistance = 1500.0f;
			ProjectileDefinition->TargetAssistMaxAngleDegrees = 60.0f;
			ProjectileDefinition->TargetAssistMaxHeightDelta = 250.0f;
			ProjectileDefinition->TargetAssistMaxPitchDegrees = 45.0f;
			ProjectileDefinition->bEnableLimitedHoming = true;
			ProjectileDefinition->HomingStartDelaySeconds = 0.06f;
			ProjectileDefinition->HomingDurationSeconds = 1.0f;
			ProjectileDefinition->HomingTurnRateDegreesPerSecond = 180.0f;
			ProjectileDefinition->HomingMaxTotalTurnDegrees = 180.0f;

			UCombatLoadoutDefinition* BowLoadout = NewObject<UCombatLoadoutDefinition>(GetTransientPackage(), TEXT("Test_B3BowLoadout"));
			BowLoadout->AddTestInputAbilityRoute(TagInputPrimaryAttack, TagAbilityPrimaryAttack);

			UBowWeaponDefinition* BowDefinition = NewObject<UBowWeaponDefinition>(GetTransientPackage(), TEXT("Test_B3BowDefinition"));
			BowDefinition->HandSlot = EWeaponHandSlot::MainHandTwoHanded;
			// The transient definition targets the known socket on the real test Hero mesh.
			// This does not alter the authored Bow_L socket used by runtime Bow assets.
			BowDefinition->AttachSocketName = BowAttachSocketName;
			BowDefinition->WeaponMesh = BowMesh;
			BowDefinition->LaunchSocketName = BowLaunchSocketName;
			BowDefinition->DefaultProjectileDefinition = ProjectileDefinition;
			BowDefinition->AssociatedLoadout = BowLoadout;
			BowDefinition->BaseGrantedActions.Add(UBowDrawFireAbility::StaticClass());

			FString BowDefinitionReason;
			TestTrue(TEXT("B3 transient BowDefinition passes validation"), BowDefinition->IsValidWeaponDefinition(BowDefinitionReason));
			UWeaponEquipmentComponent* EquipmentComp = Player->FindComponentByClass<UWeaponEquipmentComponent>();
			TestNotNull(TEXT("B3 Player equipment component exists"), EquipmentComp);
			if (PlayerASC && LockedASC && EquipmentComp)
			{
				TestTrue(TEXT("B3 equips transient Bow"), EquipmentComp->EquipWeapon(BowDefinition));
				FGameplayAbilitySpec* BowAbilitySpec = PlayerASC->FindAbilitySpecFromClass(UBowDrawFireAbility::StaticClass());
				TestNotNull(TEXT("B3 equipped Bow grants its Ability Spec"), BowAbilitySpec);
				const FGameplayAbilitySpecHandle BowAbilitySpecHandle = BowAbilitySpec ? BowAbilitySpec->Handle : FGameplayAbilitySpecHandle();
				TestTrue(TEXT("B3 equipped Bow Ability Spec has a valid Handle"), BowAbilitySpecHandle.IsValid());
				TestEqual(TEXT("B3 equipped Bow Ability Spec keeps level one"), BowAbilitySpec ? BowAbilitySpec->Level : INDEX_NONE, 1);

				if (!BowAbilitySpecHandle.IsValid())
				{
					return false;
				}

				auto CaptureProjectiles = [World]()
				{
					TSet<ACombatProjectile*> ExistingProjectiles;
					for (TActorIterator<ACombatProjectile> It(World); It; ++It)
					{
						if (ACombatProjectile* Projectile = *It)
						{
							ExistingProjectiles.Add(Projectile);
						}
					}
					return ExistingProjectiles;
				};

				auto SpawnReleaseProjectile = [World, Player, PlayerASC, BowAbilitySpecHandle, &CaptureProjectiles](const FName AbilityName)
				{
					const TSet<ACombatProjectile*> ExistingProjectiles = CaptureProjectiles();
					UBowDrawFireAbility* BowAbility = NewObject<UBowDrawFireAbility>(Player, AbilityName);
					if (!BowAbility)
					{
						return static_cast<ACombatProjectile*>(nullptr);
					}

					BowAbility->SetTestCurrentActorInfo(PlayerASC->AbilityActorInfo.Get());
					BowAbility->SetTestCurrentSpecHandle(BowAbilitySpecHandle);
					BowAbility->SetTestTargetAssistScreenProjectionHook([](const FVector&, FVector2D& OutScreenPosition, FVector2D& OutViewportSize)
					{
						OutScreenPosition = FVector2D(960.0f, 540.0f);
						OutViewportSize = FVector2D(1920.0f, 1080.0f);
						return true;
					});
					BowAbility->TestSpawnProjectile();

					for (TActorIterator<ACombatProjectile> It(World); It; ++It)
					{
						ACombatProjectile* Projectile = *It;
						if (Projectile && !ExistingProjectiles.Contains(Projectile))
						{
							return Projectile;
						}
					}

					return static_cast<ACombatProjectile*>(nullptr);
				};

				Player->SetTestLockedTarget(LockedEnemy);
				TestEqual(TEXT("B3 resolves a valid locked target before Release"), Player->ResolveValidLockedTarget(), LockedEnemy);
				ACombatProjectile* LockedProjectile = SpawnReleaseProjectile(TEXT("Test_B3LockedProjectileAbility"));
				TestNotNull(TEXT("B3 Release spawns the locked-target projectile"), LockedProjectile);
				if (LockedProjectile)
				{
					TestEqual(TEXT("B3 valid lock wins over pointer-favored automatic candidate"), LockedProjectile->GetTestTargetActor(), Cast<AActor>(LockedEnemy));
					TestEqual(TEXT("B3 lock preference preserves pointer initial direction"), LockedProjectile->GetTestInitialLaunchDirection(), FVector::ForwardVector);
					TestTrue(TEXT("B3 locked projectile starts its existing limited homing"), LockedProjectile->GetTestHomingActive());

					Player->SetTestLockedTarget(AutomaticEnemy);
					Player->SetTestLockedTarget(nullptr);
					LockedProjectile->Tick(0.1f);
					TestEqual(TEXT("B3 post-Release lock switch and clear do not retarget the projectile"), LockedProjectile->GetTestTargetActor(), Cast<AActor>(LockedEnemy));
				}

				ACombatProjectile* AutomaticProjectile = SpawnReleaseProjectile(TEXT("Test_B3AutomaticProjectileAbility"));
				TestNotNull(TEXT("B3 no-lock Release spawns an automatic-target projectile"), AutomaticProjectile);
				if (AutomaticProjectile)
				{
					TestEqual(TEXT("B3 no-lock path retains B2 pointer-favored automatic target selection"), AutomaticProjectile->GetTestTargetActor(), Cast<AActor>(AutomaticEnemy));
				}

				Player->SetTestLockedTarget(LockedEnemy);
				ProjectileDefinition->bEnableTargetAssist = false;
				ProjectileDefinition->bEnableLimitedHoming = false;
				ACombatProjectile* StraightProjectile = SpawnReleaseProjectile(TEXT("Test_B3StraightProjectileAbility"));
				TestNotNull(TEXT("B3 Target Assist-disabled Release still spawns a straight projectile"), StraightProjectile);
				if (StraightProjectile)
				{
					TestNull(TEXT("B3 Target Assist-disabled Release ignores the Player lock"), StraightProjectile->GetTestTargetActor());
					TestFalse(TEXT("B3 Target Assist-disabled Release does not activate Homing"), StraightProjectile->GetTestHomingActive());
				}
				ProjectileDefinition->bEnableTargetAssist = true;
				ProjectileDefinition->bEnableLimitedHoming = true;

				Player->SetTestLockedTarget(LockedEnemy);
				Player->SetTestLockOnProjectionHook([](const FVector& WorldPoint, FVector2D& OutScreenPosition, FVector2D& OutViewportSize)
				{
					OutViewportSize = FVector2D(1920.0f, 1080.0f);
					OutScreenPosition = WorldPoint.Y > 200.0f ? FVector2D(0.0f, 540.0f) : FVector2D(960.0f, 540.0f);
					return true;
				});
				ACombatProjectile* InvalidLockProjectile = SpawnReleaseProjectile(TEXT("Test_B3InvalidLockProjectileAbility"));
				TestNotNull(TEXT("B3 invalid lock does not block Bow Release"), InvalidLockProjectile);
				TestNull(TEXT("B3 invalid lock is cleared before automatic fallback"), Player->GetLockedTarget());
				if (InvalidLockProjectile)
				{
					TestEqual(TEXT("B3 invalid lock falls back to automatic target selection"), InvalidLockProjectile->GetTestTargetActor(), Cast<AActor>(AutomaticEnemy));
				}

				Player->SetTestLockOnProjectionHook([](const FVector& WorldPoint, FVector2D& OutScreenPosition, FVector2D& OutViewportSize)
				{
					OutViewportSize = FVector2D(1920.0f, 1080.0f);
					if (WorldPoint.Y > 200.0f)
					{
						OutScreenPosition = FVector2D(1200.0f, 700.0f);
					}
					else if (WorldPoint.X > 100.0f)
					{
						OutScreenPosition = FVector2D(1200.0f, 540.0f);
					}
					else
					{
						OutScreenPosition = FVector2D(960.0f, 540.0f);
					}
					return true;
				});
				Player->SetTestLockedTarget(LockedEnemy);
				TestEqual(TEXT("B3 death-handoff fixture validates the initial lock"), Player->ResolveValidLockedTarget(), LockedEnemy);
				LockedASC->AddLooseGameplayTag(TagDead);
				ACombatProjectile* DeathHandoffProjectile = SpawnReleaseProjectile(TEXT("Test_B3DeathHandoffProjectileAbility"));
				TestNotNull(TEXT("B3 death handoff does not block Bow Release"), DeathHandoffProjectile);
				TestEqual(TEXT("B3 dead lock hands off through the existing B2 target lifecycle"), Player->GetLockedTarget(), AutomaticEnemy);
				if (DeathHandoffProjectile)
				{
					TestEqual(TEXT("B3 uses the B2 death-handoff target at Release"), DeathHandoffProjectile->GetTestTargetActor(), Cast<AActor>(AutomaticEnemy));
				}
				LockedASC->RemoveLooseGameplayTag(TagDead);

				for (ACombatProjectile* Projectile : { LockedProjectile, AutomaticProjectile, StraightProjectile, InvalidLockProjectile, DeathHandoffProjectile })
				{
					if (Projectile)
					{
						Projectile->Destroy();
					}
				}
			}

			PlayerController->Destroy();
			Player->Destroy();
			LockedEnemy->Destroy();
			AutomaticEnemy->Destroy();
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
