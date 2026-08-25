#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "AbilitySystem/CharacterAttributeSet.h"
#include "Character/Player/PlayerCharacter.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Combat/Equipment/ProjectileDefinition.h"
#include "Combat/Projectile/CombatProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameplayTagContainer.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Tests/CombatAutomationFixture.h"
#include "Tests/TestProjectileDamageGE.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectileFlightTrailAutomationTest,
	"PolyQuest.Projectile.FlightTrail",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	struct FProjectileFlightTrailWorldCleanup
	{
		UWorld* World = nullptr;
		~FProjectileFlightTrailWorldCleanup()
		{
			if (World)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
		}
	};

	void TickFlightTrailTestWorld(UWorld* World, const float DeltaSeconds)
	{
		if (World)
		{
			World->Tick(ELevelTick::LEVELTICK_All, DeltaSeconds);
			++GFrameCounter;
		}
	}

	void AdvanceFlightTrailTimer(UWorld* World, float DeltaSeconds)
	{
		constexpr float MaxTickStepSeconds = 0.05f;
		while (DeltaSeconds > KINDA_SMALL_NUMBER)
		{
			const float TickStep = FMath::Min(DeltaSeconds, MaxTickStepSeconds);
			TickFlightTrailTestWorld(World, TickStep);
			DeltaSeconds -= TickStep;
		}
	}
}

bool FProjectileFlightTrailAutomationTest::RunTest(const FString& Parameters)
{
	if (!TestNotNull(TEXT("Engine is available for projectile flight trail automation"), GEngine))
	{
		return false;
	}

	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("ProjectileFlightTrailTestWorld"));
	WorldContext.SetCurrentWorld(World);
	FProjectileFlightTrailWorldCleanup Cleanup{ World };
	if (!TestNotNull(TEXT("Test World created successfully"), World))
	{
		return false;
	}

	FURL WorldURL;
	World->InitializeActorsForPlay(WorldURL);
	World->BeginPlay();

	const FGameplayTag TagTeamPlayer = FGameplayTag::RequestGameplayTag(FName(TEXT("Team.Player")), false);
	const FGameplayTag TagTeamEnemy = FGameplayTag::RequestGameplayTag(FName(TEXT("Team.Enemy")), false);
	TestTrue(TEXT("Tag Team.Player is valid"), TagTeamPlayer.IsValid());
	TestTrue(TEXT("Tag Team.Enemy is valid"), TagTeamEnemy.IsValid());

	// -------------------------------------------------------------------------
	// SECTION 1: CDO / Component Construction & Default Invariant Contract
	// -------------------------------------------------------------------------
	{
		const ACombatProjectile* CDO = ACombatProjectile::StaticClass()->GetDefaultObject<ACombatProjectile>();
		if (TestNotNull(TEXT("ACombatProjectile CDO exists"), CDO))
		{
			const UNiagaraComponent* TrailComp = CDO->GetFlightTrailComponent();
			TestNotNull(TEXT("CDO owns FlightTrailComponent"), TrailComp);
			if (TrailComp)
			{
				TestFalse(TEXT("FlightTrailComponent bAutoActivate is false by default"), TrailComp->bAutoActivate);
				TestFalse(TEXT("FlightTrailComponent bAutoManageAttachment is false by default"), TrailComp->bAutoManageAttachment);

				const FBoolProperty* AutoDestroyProp = CastField<FBoolProperty>(UNiagaraComponent::StaticClass()->FindPropertyByName(TEXT("bAutoDestroy")));
				const bool bAutoDestroyVal = AutoDestroyProp ? AutoDestroyProp->GetPropertyValue_InContainer(TrailComp) : false;
				TestFalse(TEXT("FlightTrailComponent auto-destroy is false by default"), bAutoDestroyVal);
				TestTrue(TEXT("FlightTrailComponent is attached to ProjectileMeshComponent"), TrailComp->GetAttachParent() == CDO->GetMeshComponent());
			}
		}
	}

	// -------------------------------------------------------------------------
	// SECTION 2: Silent No-Op with Null System
	// -------------------------------------------------------------------------
	{
		UProjectileDefinition* NullSystemDef = NewObject<UProjectileDefinition>(GetTransientPackage(), TEXT("PFT_NullSystemDef"));
		NullSystemDef->InitialSpeed = 3000.0f;
		NullSystemDef->MaxSpeed = 3000.0f;
		NullSystemDef->LifespanSeconds = 5.0f;
		NullSystemDef->CollisionRadius = 12.0f;
		NullSystemDef->DamageGameplayEffectClass = UTestProjectileDamageGE::StaticClass();
		NullSystemDef->FlightTrailSystem = nullptr;
		NullSystemDef->FlightTrailSocketName = NAME_None;

		ACombatProjectile* Projectile = World->SpawnActor<ACombatProjectile>();
		if (TestNotNull(TEXT("Spawned projectile for null-system test"), Projectile))
		{
			Projectile->SetTestFlightTrailTrackingEnabled(true);

			FCombatProjectileLaunchRequest LaunchReq;
			LaunchReq.Definition = NullSystemDef;

			TestTrue(TEXT("InitializeProjectile succeeds with null trail system"), Projectile->InitializeProjectile(LaunchReq));
			TestFalse(TEXT("Flight trail is inactive when System is null"), Projectile->IsTestFlightTrailActive());
			TestNull(TEXT("Test flight trail system is null"), Projectile->GetTestFlightTrailSystem());
			TestFalse(TEXT("Null system does not flag root fallback"), Projectile->DidTestFlightTrailUseRootFallback());
			TestEqual(TEXT("Movement speed configured normally"), Projectile->GetMovementComponent()->InitialSpeed, 3000.0f);
			TestEqual(TEXT("Lifespan configured normally"), Projectile->GetLifeSpan(), 5.0f);

			Projectile->Destroy();
		}
	}

	// -------------------------------------------------------------------------
	// SECTION 3: Valid Transient Mesh with FX_Trail Socket + Valid System
	// -------------------------------------------------------------------------
	{
		UStaticMesh* TransientMesh = NewObject<UStaticMesh>(GetTransientPackage(), TEXT("PFT_ValidSocketMesh"));
		const FName SocketNameFXTrail(TEXT("FX_Trail"));
		UStaticMeshSocket* TrailSocket = NewObject<UStaticMeshSocket>(TransientMesh);
		TrailSocket->SocketName = SocketNameFXTrail;
		TrailSocket->RelativeLocation = FVector(0.0f, 0.0f, -50.0f);
		TransientMesh->AddSocket(TrailSocket);

		UNiagaraSystem* MockSystem = NewObject<UNiagaraSystem>(GetTransientPackage(), TEXT("PFT_MockNiagaraSystem_Valid"));

		UProjectileDefinition* SocketDef = NewObject<UProjectileDefinition>(GetTransientPackage(), TEXT("PFT_SocketDef"));
		SocketDef->InitialSpeed = 3000.0f;
		SocketDef->MaxSpeed = 3000.0f;
		SocketDef->LifespanSeconds = 5.0f;
		SocketDef->CollisionRadius = 12.0f;
		SocketDef->DamageGameplayEffectClass = UTestProjectileDamageGE::StaticClass();
		SocketDef->ProjectileMesh = TransientMesh;
		SocketDef->FlightTrailSystem = MockSystem;
		SocketDef->FlightTrailSocketName = SocketNameFXTrail;

		ACombatProjectile* Projectile = World->SpawnActor<ACombatProjectile>();
		if (TestNotNull(TEXT("Spawned projectile for valid socket test"), Projectile))
		{
			Projectile->SetTestFlightTrailTrackingEnabled(true);

			FCombatProjectileLaunchRequest LaunchReq;
			LaunchReq.Definition = SocketDef;

			TestTrue(TEXT("InitializeProjectile succeeds with valid socket"), Projectile->InitializeProjectile(LaunchReq));
			TestTrue(TEXT("Flight trail is active"), Projectile->IsTestFlightTrailActive());
			TestTrue(TEXT("Flight trail system matches definition"), Projectile->GetTestFlightTrailSystem() == MockSystem);
			TestEqual(TEXT("Resolved socket is FX_Trail"), Projectile->GetTestFlightTrailAttachSocketName(), SocketNameFXTrail);
			TestFalse(TEXT("Root fallback was not used for valid socket"), Projectile->DidTestFlightTrailUseRootFallback());

			Projectile->Destroy();
		}
	}

	// -------------------------------------------------------------------------
	// SECTION 4: FlightTrailSocketName == NAME_None (Normal Root Attachment)
	// -------------------------------------------------------------------------
	{
		UStaticMesh* TransientMesh = NewObject<UStaticMesh>(GetTransientPackage(), TEXT("PFT_RootAttachMesh"));
		UNiagaraSystem* MockSystem = NewObject<UNiagaraSystem>(GetTransientPackage(), TEXT("PFT_MockNiagaraSystem_Root"));

		UProjectileDefinition* RootDef = NewObject<UProjectileDefinition>(GetTransientPackage(), TEXT("PFT_RootDef"));
		RootDef->InitialSpeed = 3000.0f;
		RootDef->MaxSpeed = 3000.0f;
		RootDef->LifespanSeconds = 5.0f;
		RootDef->CollisionRadius = 12.0f;
		RootDef->DamageGameplayEffectClass = UTestProjectileDamageGE::StaticClass();
		RootDef->ProjectileMesh = TransientMesh;
		RootDef->FlightTrailSystem = MockSystem;
		RootDef->FlightTrailSocketName = NAME_None;

		ACombatProjectile* Projectile = World->SpawnActor<ACombatProjectile>();
		if (TestNotNull(TEXT("Spawned projectile for NAME_None root attach test"), Projectile))
		{
			Projectile->SetTestFlightTrailTrackingEnabled(true);

			FCombatProjectileLaunchRequest LaunchReq;
			LaunchReq.Definition = RootDef;

			TestTrue(TEXT("InitializeProjectile succeeds with NAME_None socket"), Projectile->InitializeProjectile(LaunchReq));
			TestTrue(TEXT("Flight trail is active for NAME_None root attach"), Projectile->IsTestFlightTrailActive());
			TestTrue(TEXT("Flight trail system matches definition"), Projectile->GetTestFlightTrailSystem() == MockSystem);
			TestEqual(TEXT("Resolved socket is NAME_None"), Projectile->GetTestFlightTrailAttachSocketName(), FName(NAME_None));
			TestFalse(TEXT("NAME_None root attach is not flagged as fallback"), Projectile->DidTestFlightTrailUseRootFallback());

			Projectile->Destroy();
		}
	}

	// -------------------------------------------------------------------------
	// SECTION 5: Missing / Non-Existent Socket Warning & Root Fallback
	// -------------------------------------------------------------------------
	{
		UStaticMesh* TransientMesh = NewObject<UStaticMesh>(GetTransientPackage(), TEXT("PFT_MissingSocketMesh"));
		UNiagaraSystem* MockSystem = NewObject<UNiagaraSystem>(GetTransientPackage(), TEXT("PFT_MockNiagaraSystem_MissingSocket"));

		const FName MissingSocketName(TEXT("NonExistent_Socket_XYZ"));

		UProjectileDefinition* MissingSocketDef = NewObject<UProjectileDefinition>(GetTransientPackage(), TEXT("PFT_MissingSocketDef"));
		MissingSocketDef->InitialSpeed = 3000.0f;
		MissingSocketDef->MaxSpeed = 3000.0f;
		MissingSocketDef->LifespanSeconds = 5.0f;
		MissingSocketDef->CollisionRadius = 12.0f;
		MissingSocketDef->DamageGameplayEffectClass = UTestProjectileDamageGE::StaticClass();
		MissingSocketDef->ProjectileMesh = TransientMesh;
		MissingSocketDef->FlightTrailSystem = MockSystem;
		MissingSocketDef->FlightTrailSocketName = MissingSocketName;

		// Expect exactly one Warning for the missing socket fallback
		AddExpectedError(TEXT("requested FlightTrailSocket 'NonExistent_Socket_XYZ', but it was not found on mesh"), EAutomationExpectedErrorFlags::Contains, 1);

		ACombatProjectile* Projectile = World->SpawnActor<ACombatProjectile>();
		if (TestNotNull(TEXT("Spawned projectile for missing socket fallback test"), Projectile))
		{
			Projectile->SetTestFlightTrailTrackingEnabled(true);

			FCombatProjectileLaunchRequest LaunchReq;
			LaunchReq.Definition = MissingSocketDef;

			TestTrue(TEXT("InitializeProjectile succeeds even when socket is missing"), Projectile->InitializeProjectile(LaunchReq));
			TestTrue(TEXT("Flight trail is active after root fallback"), Projectile->IsTestFlightTrailActive());
			TestTrue(TEXT("Flight trail system matches definition"), Projectile->GetTestFlightTrailSystem() == MockSystem);
			TestEqual(TEXT("Fallback attached to NAME_None"), Projectile->GetTestFlightTrailAttachSocketName(), FName(NAME_None));
			TestTrue(TEXT("Root fallback flag is true"), Projectile->DidTestFlightTrailUseRootFallback());
			TestEqual(TEXT("Movement speed remains configured"), Projectile->GetMovementComponent()->InitialSpeed, 3000.0f);
			TestEqual(TEXT("Collision sphere radius configured"), Projectile->GetCollisionComponent()->GetUnscaledSphereRadius(), 12.0f);

			Projectile->Destroy();
		}
	}

	// -------------------------------------------------------------------------
	// SECTION 6: Terminal Fade-Out, Impact Delivery & Teardown Cleanup Matrix
	// -------------------------------------------------------------------------
	{
		UNiagaraSystem* MockSystem = NewObject<UNiagaraSystem>(GetTransientPackage(), TEXT("PFT_MockNiagaraSystem_Teardown"));

		UProjectileDefinition* TeardownDef = NewObject<UProjectileDefinition>(GetTransientPackage(), TEXT("PFT_TeardownDef"));
		TeardownDef->InitialSpeed = 3000.0f;
		TeardownDef->MaxSpeed = 3000.0f;
		TeardownDef->LifespanSeconds = 5.0f;
		TeardownDef->CollisionRadius = 12.0f;
		TeardownDef->DamageGameplayEffectClass = UTestProjectileDamageGE::StaticClass();
		TeardownDef->FlightTrailSystem = MockSystem;
		TeardownDef->FlightTrailSocketName = NAME_None;
		TeardownDef->FlightTrailFinishTimeoutSeconds = 0.35f;

		// 6.1 Blocking Impact (Static Geometry / Wall) -> Terminal Fade + Mesh Hidden + OnSystemFinished Completion
		{
			ACombatProjectile* BlockProj = World->SpawnActor<ACombatProjectile>();
			if (TestNotNull(TEXT("Spawned projectile for blocking impact test"), BlockProj))
			{
				BlockProj->SetTestFlightTrailTrackingEnabled(true);

				FCombatProjectileLaunchRequest LaunchReq;
				LaunchReq.Definition = TeardownDef;

				BlockProj->InitializeProjectile(LaunchReq);
				BlockProj->DispatchBeginPlay();
				TestTrue(TEXT("Trail active before blocking hit"), BlockProj->IsTestFlightTrailActive());

				FHitResult WallHit;
				WallHit.bBlockingHit = true;
				WallHit.ImpactPoint = FVector(100.0f, 0.0f, 0.0f);

				BlockProj->GetCollisionComponent()->OnComponentHit.Broadcast(
					BlockProj->GetCollisionComponent(),
					nullptr,
					nullptr,
					FVector::ZeroVector,
					WallHit);

				TestTrue(TEXT("Terminal fade active after blocking hit"), BlockProj->IsTestTerminalFlightTrailFadeOutActive());
				TestEqual(TEXT("Collision disabled immediately upon blocking hit"), BlockProj->GetCollisionComponent()->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
				TestTrue(TEXT("Projectile mesh hidden upon blocking hit"), BlockProj->GetMeshComponent()->bHiddenInGame);
				TestTrue(TEXT("Flight trail detached from mesh upon blocking hit"), BlockProj->GetFlightTrailComponent()->GetAttachParent() != BlockProj->GetMeshComponent());
				TestFalse(TEXT("Actor remains alive waiting for trail completion"), BlockProj->IsActorBeingDestroyed());

				// Simulate Niagara system finishing naturally
				BlockProj->GetFlightTrailComponent()->OnSystemFinished.Broadcast(BlockProj->GetFlightTrailComponent());
				TestTrue(TEXT("Projectile is destroyed after OnSystemFinished completes"), BlockProj->IsActorBeingDestroyed());
			}
		}

		// 6.2 Hostile Pawn Impact (Immediate Hit Delivery + Terminal Fade + Single Hit Invariant)
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

				if (SourceASC && TargetASC)
				{
					const float PreHitHealth = TargetASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute());
					TestTrue(TEXT("Target initial Health is positive"), PreHitHealth > 0.0f);

					ACombatProjectile* PawnProj = World->SpawnActor<ACombatProjectile>();
					if (TestNotNull(TEXT("Spawned projectile for pawn impact test"), PawnProj))
					{
						PawnProj->SetTestFlightTrailTrackingEnabled(true);

						FCombatProjectileLaunchRequest PawnLaunchReq;
						PawnLaunchReq.Definition = TeardownDef;
						PawnLaunchReq.SourceActor = PlayerSource;
						PawnLaunchReq.SourceAbilitySystemComponent = SourceASC;

						PawnProj->InitializeProjectile(PawnLaunchReq);
						PawnProj->DispatchBeginPlay();
						TestTrue(TEXT("Trail active before hostile pawn hit"), PawnProj->IsTestFlightTrailActive());

						FHitResult SweepHit(EnemyTarget, Cast<UPrimitiveComponent>(EnemyTarget->GetRootComponent()), EnemyTarget->GetActorLocation(), FVector::UpVector);
						PawnProj->GetCollisionComponent()->OnComponentBeginOverlap.Broadcast(
							PawnProj->GetCollisionComponent(),
							EnemyTarget,
							Cast<UPrimitiveComponent>(EnemyTarget->GetRootComponent()),
							0,
							true,
							SweepHit);

						const float PostHitHealth = TargetASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute());
						TestEqual(TEXT("Target Health reduced immediately by Damage GE"), PostHitHealth, PreHitHealth - 25.0f);
						TestTrue(TEXT("Terminal fade active after hostile pawn hit"), PawnProj->IsTestTerminalFlightTrailFadeOutActive());
						TestTrue(TEXT("Projectile mesh hidden upon pawn hit"), PawnProj->GetMeshComponent()->bHiddenInGame);
						TestEqual(TEXT("Collision disabled upon pawn hit"), PawnProj->GetCollisionComponent()->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
						TestFalse(TEXT("Actor remains alive waiting for trail completion"), PawnProj->IsActorBeingDestroyed());

						// Attempt second hit while in terminal fade -> must not deliver duplicate damage
						PawnProj->GetCollisionComponent()->OnComponentBeginOverlap.Broadcast(
							PawnProj->GetCollisionComponent(),
							EnemyTarget,
							Cast<UPrimitiveComponent>(EnemyTarget->GetRootComponent()),
							0,
							true,
							SweepHit);
						TestEqual(TEXT("Health unchanged on redundant overlap attempt"), TargetASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute()), PostHitHealth);

						// Complete trail fade
						PawnProj->GetFlightTrailComponent()->OnSystemFinished.Broadcast(PawnProj->GetFlightTrailComponent());
						TestTrue(TEXT("Projectile is destroyed after OnSystemFinished"), PawnProj->IsActorBeingDestroyed());
					}
				}

				PlayerSource->Destroy();
				EnemyTarget->Destroy();
			}
		}

		// 6.3 Timeout Fallback when OnSystemFinished never arrives
		{
			UProjectileDefinition* TimeoutDef = NewObject<UProjectileDefinition>(GetTransientPackage(), TEXT("PFT_TimeoutDef"));
			TimeoutDef->InitialSpeed = 3000.0f;
			TimeoutDef->MaxSpeed = 3000.0f;
			TimeoutDef->LifespanSeconds = 5.0f;
			TimeoutDef->CollisionRadius = 12.0f;
			TimeoutDef->DamageGameplayEffectClass = UTestProjectileDamageGE::StaticClass();
			TimeoutDef->FlightTrailSystem = MockSystem;
			TimeoutDef->FlightTrailSocketName = NAME_None;
			TimeoutDef->FlightTrailFinishTimeoutSeconds = 0.05f;

			ACombatProjectile* TimeoutProj = World->SpawnActor<ACombatProjectile>();
			if (TestNotNull(TEXT("Spawned projectile for timeout test"), TimeoutProj))
			{
				TimeoutProj->SetTestFlightTrailTrackingEnabled(true);

				FCombatProjectileLaunchRequest LaunchReq;
				LaunchReq.Definition = TimeoutDef;

				TimeoutProj->InitializeProjectile(LaunchReq);
				TimeoutProj->DispatchBeginPlay();

				FHitResult WallHit;
				WallHit.bBlockingHit = true;
				WallHit.ImpactPoint = FVector(100.0f, 0.0f, 0.0f);

				TimeoutProj->GetCollisionComponent()->OnComponentHit.Broadcast(
					TimeoutProj->GetCollisionComponent(),
					nullptr,
					nullptr,
					FVector::ZeroVector,
					WallHit);

				TestTrue(TEXT("Terminal fade active before timeout"), TimeoutProj->IsTestTerminalFlightTrailFadeOutActive());
				TestFalse(TEXT("Actor not destroyed immediately on hit"), TimeoutProj->IsActorBeingDestroyed());

				// Advance world clock past the 0.05s timeout
				AdvanceFlightTrailTimer(World, 0.15f);
				TestTrue(TEXT("Actor destroyed automatically via timeout fallback"), TimeoutProj->IsActorBeingDestroyed());
			}
		}

		// 6.4 LifeSpanExpired triggers Terminal Fade
		{
			UProjectileDefinition* ExpireDef = NewObject<UProjectileDefinition>(GetTransientPackage(), TEXT("PFT_ExpireDef"));
			ExpireDef->InitialSpeed = 3000.0f;
			ExpireDef->MaxSpeed = 3000.0f;
			ExpireDef->LifespanSeconds = 5.0f;
			ExpireDef->CollisionRadius = 12.0f;
			ExpireDef->DamageGameplayEffectClass = UTestProjectileDamageGE::StaticClass();
			ExpireDef->FlightTrailSystem = MockSystem;
			ExpireDef->FlightTrailSocketName = NAME_None;
			ExpireDef->FlightTrailFinishTimeoutSeconds = 0.35f;

			ACombatProjectile* ExpireProj = World->SpawnActor<ACombatProjectile>();
			if (TestNotNull(TEXT("Spawned projectile for LifeSpanExpired test"), ExpireProj))
			{
				ExpireProj->SetTestFlightTrailTrackingEnabled(true);

				FCombatProjectileLaunchRequest LaunchReq;
				LaunchReq.Definition = ExpireDef;

				ExpireProj->InitializeProjectile(LaunchReq);
				ExpireProj->DispatchBeginPlay();

				ExpireProj->LifeSpanExpired();
				TestTrue(TEXT("Terminal fade active after LifeSpanExpired"), ExpireProj->IsTestTerminalFlightTrailFadeOutActive());
				TestTrue(TEXT("Mesh hidden after LifeSpanExpired"), ExpireProj->GetMeshComponent()->bHiddenInGame);
				TestEqual(TEXT("Collision disabled after LifeSpanExpired"), ExpireProj->GetCollisionComponent()->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
				TestFalse(TEXT("Actor remains alive during fade after LifeSpanExpired"), ExpireProj->IsActorBeingDestroyed());

				ExpireProj->GetFlightTrailComponent()->OnSystemFinished.Broadcast(ExpireProj->GetFlightTrailComponent());
				TestTrue(TEXT("Actor destroyed after OnSystemFinished following LifeSpanExpired"), ExpireProj->IsActorBeingDestroyed());
			}
		}

		// 6.5 Null System Immediate Destruction on Hit & LifeSpan
		{
			UProjectileDefinition* NullTrailDef = NewObject<UProjectileDefinition>(GetTransientPackage(), TEXT("PFT_NullTrailTeardownDef"));
			NullTrailDef->InitialSpeed = 3000.0f;
			NullTrailDef->MaxSpeed = 3000.0f;
			NullTrailDef->LifespanSeconds = 5.0f;
			NullTrailDef->CollisionRadius = 12.0f;
			NullTrailDef->DamageGameplayEffectClass = UTestProjectileDamageGE::StaticClass();
			NullTrailDef->FlightTrailSystem = nullptr;
			NullTrailDef->FlightTrailSocketName = NAME_None;

			ACombatProjectile* NullHitProj = World->SpawnActor<ACombatProjectile>();
			if (TestNotNull(TEXT("Spawned null-trail projectile"), NullHitProj))
			{
				NullHitProj->SetTestFlightTrailTrackingEnabled(true);

				FCombatProjectileLaunchRequest LaunchReq;
				LaunchReq.Definition = NullTrailDef;

				NullHitProj->InitializeProjectile(LaunchReq);
				NullHitProj->DispatchBeginPlay();

				FHitResult WallHit;
				WallHit.bBlockingHit = true;
				WallHit.ImpactPoint = FVector(100.0f, 0.0f, 0.0f);

				NullHitProj->GetCollisionComponent()->OnComponentHit.Broadcast(
					NullHitProj->GetCollisionComponent(),
					nullptr,
					nullptr,
					FVector::ZeroVector,
					WallHit);

				TestTrue(TEXT("Null trail projectile is destroyed immediately on hit without lingering"), NullHitProj->IsActorBeingDestroyed());
			}
		}

		// 6.6 External Destroy / EndPlay Immediate Cleanup
		{
			ACombatProjectile* DestroyProj = World->SpawnActor<ACombatProjectile>();
			if (TestNotNull(TEXT("Spawned projectile for external destroy test"), DestroyProj))
			{
				DestroyProj->SetTestFlightTrailTrackingEnabled(true);

				FCombatProjectileLaunchRequest LaunchReq;
				LaunchReq.Definition = TeardownDef;

				DestroyProj->InitializeProjectile(LaunchReq);
				DestroyProj->DispatchBeginPlay();
				TestTrue(TEXT("Trail active before external destroy"), DestroyProj->IsTestFlightTrailActive());

				DestroyProj->Destroy();
				TestFalse(TEXT("Trail stopped immediately after external destroy / EndPlay"), DestroyProj->IsTestFlightTrailActive());
			}
		}

		// 6.7 Invalid / Non-Finite Timeout Warning with Valid System -> Falls back to 0.35s and destroys on timeout
		{
			UProjectileDefinition* BadTimeoutDef = NewObject<UProjectileDefinition>(GetTransientPackage(), TEXT("PFT_BadTimeoutDef"));
			BadTimeoutDef->InitialSpeed = 3000.0f;
			BadTimeoutDef->MaxSpeed = 3000.0f;
			BadTimeoutDef->LifespanSeconds = 5.0f;
			BadTimeoutDef->CollisionRadius = 12.0f;
			BadTimeoutDef->DamageGameplayEffectClass = UTestProjectileDamageGE::StaticClass();
			BadTimeoutDef->FlightTrailSystem = MockSystem;
			BadTimeoutDef->FlightTrailSocketName = NAME_None;
			BadTimeoutDef->FlightTrailFinishTimeoutSeconds = INFINITY;

			AddExpectedError(TEXT("has non-positive or non-finite FlightTrailFinishTimeoutSeconds"), EAutomationExpectedErrorFlags::Contains, 1);

			ACombatProjectile* BadTimeoutProj = World->SpawnActor<ACombatProjectile>();
			if (TestNotNull(TEXT("Spawned projectile for bad timeout test"), BadTimeoutProj))
			{
				BadTimeoutProj->SetTestFlightTrailTrackingEnabled(true);

				FCombatProjectileLaunchRequest LaunchReq;
				LaunchReq.Definition = BadTimeoutDef;

				TestTrue(TEXT("Initialize succeeds with non-finite timeout and falls back to 0.35s"), BadTimeoutProj->InitializeProjectile(LaunchReq));
				BadTimeoutProj->DispatchBeginPlay();

				FHitResult WallHit;
				WallHit.bBlockingHit = true;
				WallHit.ImpactPoint = FVector(100.0f, 0.0f, 0.0f);

				BadTimeoutProj->GetCollisionComponent()->OnComponentHit.Broadcast(
					BadTimeoutProj->GetCollisionComponent(),
					nullptr,
					nullptr,
					FVector::ZeroVector,
					WallHit);

				TestTrue(TEXT("Terminal fade active before fallback timeout"), BadTimeoutProj->IsTestTerminalFlightTrailFadeOutActive());
				TestFalse(TEXT("Actor not destroyed immediately on hit"), BadTimeoutProj->IsActorBeingDestroyed());

				// Advance past 0.35s fallback window (0.45s)
				AdvanceFlightTrailTimer(World, 0.45f);
				TestTrue(TEXT("Actor destroyed automatically via 0.35s fallback timeout"), BadTimeoutProj->IsActorBeingDestroyed());
			}
		}

		// 6.8 Null FlightTrailSystem + Invalid Timeout -> Completely silent (no warning), immediate destruction on hit
		{
			UProjectileDefinition* NullSystemBadTimeoutDef = NewObject<UProjectileDefinition>(GetTransientPackage(), TEXT("PFT_NullSystemBadTimeoutDef"));
			NullSystemBadTimeoutDef->InitialSpeed = 3000.0f;
			NullSystemBadTimeoutDef->MaxSpeed = 3000.0f;
			NullSystemBadTimeoutDef->LifespanSeconds = 5.0f;
			NullSystemBadTimeoutDef->CollisionRadius = 12.0f;
			NullSystemBadTimeoutDef->DamageGameplayEffectClass = UTestProjectileDamageGE::StaticClass();
			NullSystemBadTimeoutDef->FlightTrailSystem = nullptr;
			NullSystemBadTimeoutDef->FlightTrailSocketName = NAME_None;
			NullSystemBadTimeoutDef->FlightTrailFinishTimeoutSeconds = -1.0f;

			// Note: No AddExpectedError here! Null system must be completely silent even with invalid timeout.

			ACombatProjectile* NullBadTimeoutProj = World->SpawnActor<ACombatProjectile>();
			if (TestNotNull(TEXT("Spawned projectile for null system bad timeout test"), NullBadTimeoutProj))
			{
				NullBadTimeoutProj->SetTestFlightTrailTrackingEnabled(true);

				FCombatProjectileLaunchRequest LaunchReq;
				LaunchReq.Definition = NullSystemBadTimeoutDef;

				TestTrue(TEXT("Initialize succeeds silently with null system and bad timeout"), NullBadTimeoutProj->InitializeProjectile(LaunchReq));
				NullBadTimeoutProj->DispatchBeginPlay();
				TestFalse(TEXT("Flight trail is inactive for null system"), NullBadTimeoutProj->IsTestFlightTrailActive());

				FHitResult WallHit;
				WallHit.bBlockingHit = true;
				WallHit.ImpactPoint = FVector(100.0f, 0.0f, 0.0f);

				NullBadTimeoutProj->GetCollisionComponent()->OnComponentHit.Broadcast(
					NullBadTimeoutProj->GetCollisionComponent(),
					nullptr,
					nullptr,
					FVector::ZeroVector,
					WallHit);

				TestTrue(TEXT("Null trail projectile is destroyed immediately on hit even with bad timeout"), NullBadTimeoutProj->IsActorBeingDestroyed());
			}
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
