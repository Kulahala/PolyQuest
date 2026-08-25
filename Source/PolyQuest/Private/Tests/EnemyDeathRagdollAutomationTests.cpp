#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "AbilitySystem/CharacterAttributeSet.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Character/Player/PlayerCharacter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "Tests/CombatAutomationFixture.h"
#include "Tests/TestProjectileDamageGE.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyDeathRagdollAutomationTest, "PolyQuest.Enemy.DeathRagdoll", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyDeathRagdollAutomationTest::RunTest(const FString& Parameters)
{
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("EnemyDeathRagdollTestWorld"));
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

	const FGameplayTag TagDead = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
	TestTrue(TEXT("State.Status.Dead tag is valid"), TagDead.IsValid());

	// -------------------------------------------------------------------------
	// SECTION 1: Valid Lethal Context Captures Correct Velocity & Once-Only Consumption
	// -------------------------------------------------------------------------
	{
		APlayerCharacter* Player = FCombatAutomationFixture::SpawnPlayer(World, FTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, 0.0f)));
		AEnemyCharacter* Enemy = FCombatAutomationFixture::SpawnPassiveEnemy(World, FTransform(FRotator::ZeroRotator, FVector(100.0f, 0.0f, 0.0f)));
		TestNotNull(TEXT("1: Player spawned"), Player);
		TestNotNull(TEXT("1: Enemy spawned"), Enemy);

		if (Player && Enemy)
		{
			UAbilitySystemComponent* EnemyASC = Enemy->GetAbilitySystemComponent();
			TestNotNull(TEXT("1: Enemy ASC valid"), EnemyASC);

			if (EnemyASC)
			{
				// Configure authored parameters
				Enemy->ConfigureTestDeathRagdollImpact(FName(TEXT("pelvis")), 400.0f, 150.0f);
				EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 20.0f);
				EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxHealthAttribute(), 100.0f);

				TestFalse(TEXT("1: Enemy initially not dead"), Enemy->IsDead());
				TestEqual(TEXT("1: Capture count initially 0"), Enemy->GetTestDeathRagdollCaptureCount(), 0);
				TestEqual(TEXT("1: Consume count initially 0"), Enemy->GetTestDeathRagdollConsumeCount(), 0);

				// Apply lethal projectile GE (-25 Health) with Player as Instigator
				FGameplayEffectContextHandle Context = EnemyASC->MakeEffectContext();
				Context.AddInstigator(Player, Player);
				const FGameplayEffectSpecHandle SpecHandle = EnemyASC->MakeOutgoingSpec(UTestProjectileDamageGE::StaticClass(), 1.0f, Context);
				TestTrue(TEXT("1: SpecHandle valid"), SpecHandle.IsValid() && SpecHandle.Data.IsValid());

				if (SpecHandle.IsValid() && SpecHandle.Data.IsValid())
				{
					EnemyASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

					TestTrue(TEXT("1: Enemy is dead after lethal damage"), Enemy->IsDead());
					TestEqual(TEXT("1: Enemy Health reached 0"), EnemyASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute()), 0.0f);

					// Player at (0,0,0), Enemy at (100,0,0) -> Attacker-away direction is (+1, 0, 0)
					const FVector ExpectedVelocity(400.0f, 0.0f, 150.0f);
					const FVector ActualVelocity = Enemy->GetTestLastDeathRagdollVelocityChange();
					TestTrue(TEXT("1: Velocity direction matches attacker-away (+X) and configured magnitude"),
						ActualVelocity.Equals(ExpectedVelocity, 1.0f));

					TestEqual(TEXT("1: Exactly 1 capture occurred"), Enemy->GetTestDeathRagdollCaptureCount(), 1);
					TestEqual(TEXT("1: Exactly 1 consume occurred"), Enemy->GetTestDeathRagdollConsumeCount(), 1);
					TestEqual(TEXT("1: Pending velocity was consumed and cleared"),
						Enemy->GetTestPendingDeathRagdollVelocityChange(), FVector::ZeroVector);
				}
			}

			Player->Destroy();
			Enemy->Destroy();
		}
	}

	// -------------------------------------------------------------------------
	// SECTION 2: Context Isolation (Attacker A Nonlethal -> Attacker B Lethal)
	// -------------------------------------------------------------------------
	{
		APlayerCharacter* AttackerA = FCombatAutomationFixture::SpawnPlayer(World, FTransform(FRotator::ZeroRotator, FVector(-100.0f, 0.0f, 0.0f)));
		APlayerCharacter* AttackerB = FCombatAutomationFixture::SpawnPlayer(World, FTransform(FRotator::ZeroRotator, FVector(0.0f, -100.0f, 0.0f)));
		AEnemyCharacter* Enemy = FCombatAutomationFixture::SpawnPassiveEnemy(World, FTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, 0.0f)));

		TestNotNull(TEXT("2: AttackerA spawned"), AttackerA);
		TestNotNull(TEXT("2: AttackerB spawned"), AttackerB);
		TestNotNull(TEXT("2: Enemy spawned"), Enemy);

		if (AttackerA && AttackerB && Enemy)
		{
			UAbilitySystemComponent* EnemyASC = Enemy->GetAbilitySystemComponent();
			TestNotNull(TEXT("2: Enemy ASC valid"), EnemyASC);

			if (EnemyASC)
			{
				Enemy->ConfigureTestDeathRagdollImpact(FName(TEXT("pelvis")), 450.0f, 180.0f);
				EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 40.0f);
				EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxHealthAttribute(), 100.0f);

				// Step 1: Attacker A hits for 25 damage (nonlethal, health 40 -> 15)
				FGameplayEffectContextHandle ContextA = EnemyASC->MakeEffectContext();
				ContextA.AddInstigator(AttackerA, AttackerA);
				const FGameplayEffectSpecHandle SpecHandleA = EnemyASC->MakeOutgoingSpec(UTestProjectileDamageGE::StaticClass(), 1.0f, ContextA);
				TestTrue(TEXT("2: SpecHandleA valid"), SpecHandleA.IsValid() && SpecHandleA.Data.IsValid());

				if (SpecHandleA.IsValid() && SpecHandleA.Data.IsValid())
				{
					EnemyASC->ApplyGameplayEffectSpecToSelf(*SpecHandleA.Data.Get());

					TestFalse(TEXT("2: Enemy still alive after hit A"), Enemy->IsDead());
					TestEqual(TEXT("2: Health is 15 after hit A"), EnemyASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute()), 15.0f);
					TestEqual(TEXT("2: No capture occurred on nonlethal hit"), Enemy->GetTestDeathRagdollCaptureCount(), 0);
					TestEqual(TEXT("2: Pending velocity remains zero on nonlethal hit"),
						Enemy->GetTestPendingDeathRagdollVelocityChange(), FVector::ZeroVector);
				}

				// Step 2: Attacker B hits for 25 damage (lethal, health 15 -> 0)
				FGameplayEffectContextHandle ContextB = EnemyASC->MakeEffectContext();
				ContextB.AddInstigator(AttackerB, AttackerB);
				const FGameplayEffectSpecHandle SpecHandleB = EnemyASC->MakeOutgoingSpec(UTestProjectileDamageGE::StaticClass(), 1.0f, ContextB);
				TestTrue(TEXT("2: SpecHandleB valid"), SpecHandleB.IsValid() && SpecHandleB.Data.IsValid());

				if (SpecHandleB.IsValid() && SpecHandleB.Data.IsValid())
				{
					EnemyASC->ApplyGameplayEffectSpecToSelf(*SpecHandleB.Data.Get());

					TestTrue(TEXT("2: Enemy dead after hit B"), Enemy->IsDead());
					TestEqual(TEXT("2: Health reached 0 after hit B"), EnemyASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute()), 0.0f);

					// Attacker B is at (0, -100, 0), Enemy at (0, 0, 0) -> Attacker-away direction is (0, +1, 0)
					const FVector ExpectedVelocity(0.0f, 450.0f, 180.0f);
					const FVector ActualVelocity = Enemy->GetTestLastDeathRagdollVelocityChange();
					TestTrue(TEXT("2: Velocity matches Attacker B direction (+Y), not Attacker A (+X)"),
						ActualVelocity.Equals(ExpectedVelocity, 1.0f));

					TestEqual(TEXT("2: Capture count is exactly 1"), Enemy->GetTestDeathRagdollCaptureCount(), 1);
					TestEqual(TEXT("2: Consume count is exactly 1"), Enemy->GetTestDeathRagdollConsumeCount(), 1);
				}
			}

			AttackerA->Destroy();
			AttackerB->Destroy();
			Enemy->Destroy();
		}
	}

	// -------------------------------------------------------------------------
	// SECTION 3: Fail-Closed Behavior (Invalid Context, Coincident, Bad Config, Direct Dead Tag)
	// -------------------------------------------------------------------------
	// 3.1 Empty/Invalid Context
	{
		AEnemyCharacter* Enemy = FCombatAutomationFixture::SpawnPassiveEnemy(World, FTransform(FRotator::ZeroRotator, FVector(100.0f, 0.0f, 0.0f)));
		TestNotNull(TEXT("3.1: Enemy spawned"), Enemy);

		if (Enemy)
		{
			UAbilitySystemComponent* EnemyASC = Enemy->GetAbilitySystemComponent();
			TestNotNull(TEXT("3.1: Enemy ASC valid"), EnemyASC);
			if (EnemyASC)
			{
				Enemy->ConfigureTestDeathRagdollImpact(FName(TEXT("pelvis")), 450.0f, 180.0f);
				EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 20.0f);

				// Apply lethal GE with empty context (no Instigator)
				const FGameplayEffectContextHandle EmptyContext;
				const FGameplayEffectSpecHandle SpecHandle = EnemyASC->MakeOutgoingSpec(UTestProjectileDamageGE::StaticClass(), 1.0f, EmptyContext);
				TestTrue(TEXT("3.1: Empty-context SpecHandle valid"), SpecHandle.IsValid() && SpecHandle.Data.IsValid());
				if (SpecHandle.IsValid() && SpecHandle.Data.IsValid())
				{
					EnemyASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

					TestTrue(TEXT("3.1: Enemy is dead"), Enemy->IsDead());
					TestEqual(TEXT("3.1: Empty context yields zero capture count"), Enemy->GetTestDeathRagdollCaptureCount(), 0);
					TestEqual(TEXT("3.1: Last captured velocity is zero"), Enemy->GetTestLastDeathRagdollVelocityChange(), FVector::ZeroVector);
					TestEqual(TEXT("3.1: Consume count is 1 (safe teardown)"), Enemy->GetTestDeathRagdollConsumeCount(), 1);
				}
			}
			Enemy->Destroy();
		}
	}

	// 3.2 Coincident Location (Zero Direction)
	{
		APlayerCharacter* Player = FCombatAutomationFixture::SpawnPlayer(World, FTransform(FRotator::ZeroRotator, FVector(50.0f, 50.0f, 0.0f)));
		AEnemyCharacter* Enemy = FCombatAutomationFixture::SpawnPassiveEnemy(World, FTransform(FRotator::ZeroRotator, FVector(50.0f, 50.0f, 0.0f)));
		TestNotNull(TEXT("3.2: Player spawned"), Player);
		TestNotNull(TEXT("3.2: Enemy spawned"), Enemy);

		if (Player && Enemy)
		{
			UAbilitySystemComponent* EnemyASC = Enemy->GetAbilitySystemComponent();
			TestNotNull(TEXT("3.2: Enemy ASC valid"), EnemyASC);
			if (EnemyASC)
			{
				Enemy->ConfigureTestDeathRagdollImpact(FName(TEXT("pelvis")), 450.0f, 180.0f);
				EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 20.0f);

				FGameplayEffectContextHandle Context = EnemyASC->MakeEffectContext();
				Context.AddInstigator(Player, Player);
				const FGameplayEffectSpecHandle SpecHandle = EnemyASC->MakeOutgoingSpec(UTestProjectileDamageGE::StaticClass(), 1.0f, Context);
				TestTrue(TEXT("3.2: Coincident-location SpecHandle valid"), SpecHandle.IsValid() && SpecHandle.Data.IsValid());
				if (SpecHandle.IsValid() && SpecHandle.Data.IsValid())
				{
					EnemyASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

					TestTrue(TEXT("3.2: Enemy is dead"), Enemy->IsDead());
					TestEqual(TEXT("3.2: Coincident location yields zero capture count"), Enemy->GetTestDeathRagdollCaptureCount(), 0);
					TestEqual(TEXT("3.2: Last captured velocity is zero"), Enemy->GetTestLastDeathRagdollVelocityChange(), FVector::ZeroVector);
				}
			}
			Player->Destroy();
			Enemy->Destroy();
		}
	}

	// 3.3 Zero/Negative Horizontal Speed Configuration
	{
		APlayerCharacter* Player = FCombatAutomationFixture::SpawnPlayer(World, FTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, 0.0f)));
		AEnemyCharacter* Enemy = FCombatAutomationFixture::SpawnPassiveEnemy(World, FTransform(FRotator::ZeroRotator, FVector(100.0f, 0.0f, 0.0f)));

		if (Player && Enemy)
		{
			UAbilitySystemComponent* EnemyASC = Enemy->GetAbilitySystemComponent();
			TestNotNull(TEXT("3.3: Enemy ASC valid"), EnemyASC);
			if (EnemyASC)
			{
				// Horizontal speed set to 0.0f
				Enemy->ConfigureTestDeathRagdollImpact(FName(TEXT("pelvis")), 0.0f, 180.0f);
				EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 20.0f);

				FGameplayEffectContextHandle Context = EnemyASC->MakeEffectContext();
				Context.AddInstigator(Player, Player);
				const FGameplayEffectSpecHandle SpecHandle = EnemyASC->MakeOutgoingSpec(UTestProjectileDamageGE::StaticClass(), 1.0f, Context);
				TestTrue(TEXT("3.3: Zero-speed SpecHandle valid"), SpecHandle.IsValid() && SpecHandle.Data.IsValid());
				if (SpecHandle.IsValid() && SpecHandle.Data.IsValid())
				{
					EnemyASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

					TestTrue(TEXT("3.3: Enemy is dead"), Enemy->IsDead());
					TestEqual(TEXT("3.3: Zero horizontal speed config yields zero capture count"), Enemy->GetTestDeathRagdollCaptureCount(), 0);
					TestEqual(TEXT("3.3: Last captured velocity is zero"), Enemy->GetTestLastDeathRagdollVelocityChange(), FVector::ZeroVector);
				}
			}
			Player->Destroy();
			Enemy->Destroy();
		}
	}

	// 3.4 Direct Dead Tag Receipt
	{
		AEnemyCharacter* Enemy = FCombatAutomationFixture::SpawnPassiveEnemy(World, FTransform(FRotator::ZeroRotator, FVector(100.0f, 0.0f, 0.0f)));

		if (Enemy)
		{
			UAbilitySystemComponent* EnemyASC = Enemy->GetAbilitySystemComponent();
			TestNotNull(TEXT("3.4: Enemy ASC valid"), EnemyASC);
			if (EnemyASC)
			{
				Enemy->ConfigureTestDeathRagdollImpact(FName(TEXT("pelvis")), 450.0f, 180.0f);
				EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 100.0f);

				// Directly add Dead tag without Health GE
				EnemyASC->SetLooseGameplayTagCount(TagDead, 1);

				TestTrue(TEXT("3.4: Enemy is dead from direct tag"), Enemy->IsDead());
				TestEqual(TEXT("3.4: Direct Dead tag yields zero capture count"), Enemy->GetTestDeathRagdollCaptureCount(), 0);
				TestEqual(TEXT("3.4: Last captured velocity is zero"), Enemy->GetTestLastDeathRagdollVelocityChange(), FVector::ZeroVector);
				TestEqual(TEXT("3.4: Consume count is 1"), Enemy->GetTestDeathRagdollConsumeCount(), 1);
			}
			Enemy->Destroy();
		}
	}

	// -------------------------------------------------------------------------
	// SECTION 4: Repeated Health & Dead Callbacks Do Not Re-Capture or Re-Consume
	// -------------------------------------------------------------------------
	{
		APlayerCharacter* Player = FCombatAutomationFixture::SpawnPlayer(World, FTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, 0.0f)));
		AEnemyCharacter* Enemy = FCombatAutomationFixture::SpawnPassiveEnemy(World, FTransform(FRotator::ZeroRotator, FVector(100.0f, 0.0f, 0.0f)));

		if (Player && Enemy)
		{
			UAbilitySystemComponent* EnemyASC = Enemy->GetAbilitySystemComponent();
			if (EnemyASC)
			{
				Enemy->ConfigureTestDeathRagdollImpact(FName(TEXT("pelvis")), 450.0f, 180.0f);
				EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 20.0f);

				// First lethal hit
				FGameplayEffectContextHandle Context = EnemyASC->MakeEffectContext();
				Context.AddInstigator(Player, Player);
				const FGameplayEffectSpecHandle SpecHandle1 = EnemyASC->MakeOutgoingSpec(UTestProjectileDamageGE::StaticClass(), 1.0f, Context);
				if (SpecHandle1.IsValid() && SpecHandle1.Data.IsValid())
				{
					EnemyASC->ApplyGameplayEffectSpecToSelf(*SpecHandle1.Data.Get());
				}

				TestTrue(TEXT("4: Enemy is dead"), Enemy->IsDead());
				TestEqual(TEXT("4: Initial capture count is 1"), Enemy->GetTestDeathRagdollCaptureCount(), 1);
				TestEqual(TEXT("4: Initial consume count is 1"), Enemy->GetTestDeathRagdollConsumeCount(), 1);

				// Subsequent lethal hit on already-dead Enemy
				const FGameplayEffectSpecHandle SpecHandle2 = EnemyASC->MakeOutgoingSpec(UTestProjectileDamageGE::StaticClass(), 1.0f, Context);
				if (SpecHandle2.IsValid() && SpecHandle2.Data.IsValid())
				{
					EnemyASC->ApplyGameplayEffectSpecToSelf(*SpecHandle2.Data.Get());
				}

				TestEqual(TEXT("4: Subsequent hit does NOT increment capture count"), Enemy->GetTestDeathRagdollCaptureCount(), 1);
				TestEqual(TEXT("4: Subsequent hit does NOT increment consume count"), Enemy->GetTestDeathRagdollConsumeCount(), 1);

				// Repeat Dead tag event
				EnemyASC->SetLooseGameplayTagCount(TagDead, 1);
				TestEqual(TEXT("4: Repeated Dead tag does NOT increment capture count"), Enemy->GetTestDeathRagdollCaptureCount(), 1);
				TestEqual(TEXT("4: Repeated Dead tag does NOT increment consume count"), Enemy->GetTestDeathRagdollConsumeCount(), 1);
			}
			Player->Destroy();
			Enemy->Destroy();
		}
	}

	// -------------------------------------------------------------------------
	// SECTION 5: Default Passive Enemy Fixture (Disabled Ragdoll) Runs Safely
	// -------------------------------------------------------------------------
	{
		APlayerCharacter* Player = FCombatAutomationFixture::SpawnPlayer(World, FTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, 0.0f)));
		AEnemyCharacter* Enemy = FCombatAutomationFixture::SpawnPassiveEnemy(World, FTransform(FRotator::ZeroRotator, FVector(100.0f, 0.0f, 0.0f)));

		if (Player && Enemy)
		{
			UAbilitySystemComponent* EnemyASC = Enemy->GetAbilitySystemComponent();
			if (EnemyASC)
			{
				// Default passive fixture has bUseRagdollOnDeath = false and no physics asset
				EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 20.0f);

				FGameplayEffectContextHandle Context = EnemyASC->MakeEffectContext();
				Context.AddInstigator(Player, Player);
				const FGameplayEffectSpecHandle SpecHandle = EnemyASC->MakeOutgoingSpec(UTestProjectileDamageGE::StaticClass(), 1.0f, Context);
				if (SpecHandle.IsValid() && SpecHandle.Data.IsValid())
				{
					EnemyASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
				}

				TestTrue(TEXT("5: Passive fixture enemy dies cleanly"), Enemy->IsDead());
				TestEqual(TEXT("5: Capture count is 1"), Enemy->GetTestDeathRagdollCaptureCount(), 1);
				TestEqual(TEXT("5: Consume count is 1 (pending cleared at entry)"), Enemy->GetTestDeathRagdollConsumeCount(), 1);
				TestEqual(TEXT("5: Pending velocity cleared despite disabled ragdoll"),
					Enemy->GetTestPendingDeathRagdollVelocityChange(), FVector::ZeroVector);
			}
			Player->Destroy();
			Enemy->Destroy();
		}
	}

	return true;
}

#endif
