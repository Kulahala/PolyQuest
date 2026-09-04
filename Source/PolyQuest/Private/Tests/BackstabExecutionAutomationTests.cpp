#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/EnemyStanceBreakAbility.h"
#include "AbilitySystem/Abilities/EnemyVictimExecutionAbility.h"
#include "AbilitySystem/Abilities/PlayerBackstabExecutionAbility.h"
#include "AbilitySystem/Abilities/PlayerFrontExecutionAbility.h"
#include "AbilitySystem/Abilities/PrimaryAttackAbility.h"
#include "AbilitySystem/CharacterAttributeSet.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Character/Player/PlayerCharacter.h"
#include "Combat/Equipment/MeleeWeaponDefinition.h"
#include "Combat/Equipment/WeaponEquipmentComponent.h"
#include "Combat/Melee/MeleeHitResolver.h"
#include "Components/BoxComponent.h"
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
	FBackstabExecutionAutomationTest,
	"PolyQuest.Combat.Backstab",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	struct FBackstabExecutionTestWorldScope
	{
		UWorld* World = nullptr;
		~FBackstabExecutionTestWorldScope()
		{
			if (World)
			{
				UGameplayStatics::SetGlobalTimeDilation(World, 1.0f);
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
		}
	};

	void TickBackstabExecutionTestWorld(UWorld* World, float DeltaSeconds)
	{
		if (World)
		{
			World->Tick(ELevelTick::LEVELTICK_All, DeltaSeconds);
			++GFrameCounter;
		}
	}

	FGameplayAbilitySpecHandle ActivateTestStanceBreak(AEnemyCharacter* InEnemy)
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

bool FBackstabExecutionAutomationTest::RunTest(const FString& Parameters)
{
	// =========================================================================
	// 1. CDO & Tag Contract Verification
	// =========================================================================
	const UPlayerBackstabExecutionAbility* BackstabCDO = UPlayerBackstabExecutionAbility::StaticClass()->GetDefaultObject<UPlayerBackstabExecutionAbility>();
	if (!TestNotNull(TEXT("UPlayerBackstabExecutionAbility CDO exists"), BackstabCDO))
	{
		return false;
	}

	TestEqual(TEXT("InstancingPolicy is InstancedPerActor"),
		BackstabCDO->GetInstancingPolicy(),
		EGameplayAbilityInstancingPolicy::InstancedPerActor);

	TestEqual(TEXT("NetExecutionPolicy is ServerOnly"),
		BackstabCDO->GetNetExecutionPolicy(),
		EGameplayAbilityNetExecutionPolicy::ServerOnly);

	const FGameplayTag TagExecutionBackstab = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.Execution.Backstab")), false);
	const FGameplayTag TagCancelByDodge = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.CancelableBy.Dodge")), false);
	const FGameplayTag TagCancelByDefense = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.CancelableBy.Defense")), false);
	const FGameplayTag TagCancelByReaction = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.CancelableBy.Reaction")), false);
	const FGameplayTag TagTeardownOnUnpossess = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Action.Teardown.OnUnpossess")), false);

	TestTrue(TEXT("Tag Ability.Action.Execution.Backstab is registered"), TagExecutionBackstab.IsValid());
	TestTrue(TEXT("AbilityTags has Ability.Action.Execution.Backstab"), BackstabCDO->GetTestAbilityTags().HasTagExact(TagExecutionBackstab));
	TestFalse(TEXT("AbilityTags has NO CancelableBy.Dodge"), BackstabCDO->GetTestAbilityTags().HasTagExact(TagCancelByDodge));
	TestFalse(TEXT("AbilityTags has NO CancelableBy.Defense"), BackstabCDO->GetTestAbilityTags().HasTagExact(TagCancelByDefense));
	TestFalse(TEXT("AbilityTags has NO CancelableBy.Reaction"), BackstabCDO->GetTestAbilityTags().HasTagExact(TagCancelByReaction));
	TestTrue(TEXT("AbilityTags has Teardown.OnUnpossess"), BackstabCDO->GetTestAbilityTags().HasTagExact(TagTeardownOnUnpossess));

	const FGameplayTag TagAttacking = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Attacking")), false);
	const FGameplayTag TagPlayerLocked = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Execution.PlayerLocked")), false);
	const FGameplayTag TagInvulnerable = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Invulnerable")), false);
	const FGameplayTag TagBlockMove = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Movement")), false);
	const FGameplayTag TagBlockJump = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Jump")), false);

	TestTrue(TEXT("ActivationOwnedTags has State.Action.Attacking"), BackstabCDO->GetTestActivationOwnedTags().HasTagExact(TagAttacking));
	TestTrue(TEXT("ActivationOwnedTags has State.Action.Execution.PlayerLocked"), BackstabCDO->GetTestActivationOwnedTags().HasTagExact(TagPlayerLocked));
	TestTrue(TEXT("ActivationOwnedTags has State.Status.Invulnerable"), BackstabCDO->GetTestActivationOwnedTags().HasTagExact(TagInvulnerable));
	TestTrue(TEXT("ActivationOwnedTags has State.Input.Block.Movement"), BackstabCDO->GetTestActivationOwnedTags().HasTagExact(TagBlockMove));
	TestTrue(TEXT("ActivationOwnedTags has State.Input.Block.Jump"), BackstabCDO->GetTestActivationOwnedTags().HasTagExact(TagBlockJump));

	const FGameplayTag TagDead = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
	const FGameplayTag TagStunned = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
	const FGameplayTag TagExhausted = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Exhausted")), false);
	const FGameplayTag TagSprinting = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Movement.Sprinting")), false);

	TestTrue(TEXT("ActivationBlockedTags has State.Status.Dead"), BackstabCDO->GetTestActivationBlockedTags().HasTagExact(TagDead));
	TestTrue(TEXT("ActivationBlockedTags has State.Status.Stunned"), BackstabCDO->GetTestActivationBlockedTags().HasTagExact(TagStunned));
	TestTrue(TEXT("ActivationBlockedTags has State.Status.Exhausted"), BackstabCDO->GetTestActivationBlockedTags().HasTagExact(TagExhausted));
	TestTrue(TEXT("ActivationBlockedTags has State.Movement.Sprinting"), BackstabCDO->GetTestActivationBlockedTags().HasTagExact(TagSprinting));

	const FGameplayTag TagHitEvent = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Action.Execution.Hit")), false);
	TestTrue(TEXT("Tag Event.Action.Execution.Hit is registered"), TagHitEvent.IsValid());

	// =========================================================================
	// 2. Fail-Closed Default Configuration Verification
	// =========================================================================
	{
		TestFalse(TEXT("CDO default configuration fails CanActivateAbility"),
			BackstabCDO->CanActivateAbility(FGameplayAbilitySpecHandle(), nullptr));
	}

	// =========================================================================
	// 3. World Test Setup
	// =========================================================================
	if (!TestNotNull(TEXT("GEngine is valid"), GEngine))
	{
		return false;
	}

	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("BackstabExecutionTestWorld"));
	WorldContext.SetCurrentWorld(World);
	FBackstabExecutionTestWorldScope ScopeCleanup{ World };

	if (!TestNotNull(TEXT("Test World created"), World))
	{
		return false;
	}

	FURL WorldURL;
	World->InitializeActorsForPlay(WorldURL);
	World->BeginPlay();

	APlayerCharacter* Player = FCombatAutomationFixture::SpawnPlayer(
		World,
		FTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, 0.0f)));
	AEnemyCharacter* Enemy = FCombatAutomationFixture::SpawnPassiveEnemy(
		World,
		FTransform(FRotator(0.0f, 0.0f, 0.0f), FVector(150.0f, 0.0f, 0.0f))); // Enemy at (150, 0, 0) facing (+1, 0, 0) -> Player is directly behind Enemy!
	APolyQuestPlayerController* Controller = World->SpawnActor<APolyQuestPlayerController>();

	if (!TestNotNull(TEXT("Player spawned"), Player)
		|| !TestNotNull(TEXT("Enemy spawned"), Enemy)
		|| !TestNotNull(TEXT("Controller spawned"), Controller))
	{
		return false;
	}

	Controller->Possess(Player);
	UAbilitySystemComponent* PlayerASC = Player->GetAbilitySystemComponent();
	UAbilitySystemComponent* EnemyASC = Enemy->GetAbilitySystemComponent();

	if (!TestNotNull(TEXT("Player ASC valid"), PlayerASC) || !TestNotNull(TEXT("Enemy ASC valid"), EnemyASC))
	{
		return false;
	}

	// Setup Combat Team Tags
	const FGameplayTag TagTeamPlayer = FGameplayTag::RequestGameplayTag(FName(TEXT("Team.Player")), false);
	const FGameplayTag TagTeamEnemy = FGameplayTag::RequestGameplayTag(FName(TEXT("Team.Enemy")), false);
	Player->SetTestCombatTeamTag(TagTeamPlayer);
	Enemy->SetTestCombatTeamTag(TagTeamEnemy);

	FGameplayAbilitySpec VictimSpec(UEnemyVictimExecutionAbility::StaticClass(), 1, INDEX_NONE, Enemy);
	EnemyASC->GiveAbility(VictimSpec);

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

	auto GrantAndConfigureBackstabAbility = [&](APlayerCharacter* InPlayer, UAnimMontage* Montage, TSubclassOf<UGameplayEffect> DamageClass, float MinDist, float MaxDist, float MaxAngle) -> TPair<FGameplayAbilitySpecHandle, UPlayerBackstabExecutionAbility*>
	{
		ResetWeaponExecutionMontages(InPlayer);
		UAbilitySystemComponent* ASC = InPlayer->GetAbilitySystemComponent();
		FGameplayAbilitySpec Spec(UPlayerBackstabExecutionAbility::StaticClass(), 1, INDEX_NONE, InPlayer);
		const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);
		FGameplayAbilitySpec* FoundSpec = ASC->FindAbilitySpecFromHandle(Handle);
		UPlayerBackstabExecutionAbility* Instance = FoundSpec ? Cast<UPlayerBackstabExecutionAbility>(FoundSpec->GetPrimaryInstance()) : nullptr;
		if (Instance)
		{
			Instance->SetTestSkipMontageTaskActivation(true);
			Instance->SetTestExecutionMontage(Montage);
			Instance->SetTestDamageGameplayEffectClass(DamageClass);
			Instance->SetTestExecutionDistances(MinDist, MaxDist);
			Instance->SetTestMaxBackAngleDegrees(MaxAngle);
		}
		return { Handle, Instance };
	};

	// =========================================================================
	// 4. Backstab Geometry Unit Tests (Stateless Check)
	// =========================================================================
	{
		UAnimMontage* SyntheticMontage = NewObject<UAnimMontage>();
		TSubclassOf<UGameplayEffect> DamageGEClass = UTestProjectileDamageGE::StaticClass();

		auto [BackstabHandle, BackstabAbility] = GrantAndConfigureBackstabAbility(Player, SyntheticMontage, DamageGEClass, 50.0f, 250.0f, 60.0f);
		if (TestNotNull(TEXT("BackstabAbility instance valid for geometry check"), BackstabAbility))
		{
			// Case 4.1: Perfect back alignment (Enemy at (150,0,0) facing (+1,0,0), Player at (0,0,0) -> Player is directly behind Enemy)
			Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
			Enemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
			Enemy->SetActorRotation(FRotator(0.0f, 0.0f, 0.0f)); // Enemy forward is (+1, 0, 0), away from player

			float Dist2D = 0.0f, AngleDeg = 0.0f;
			const bool bBackGeoPass = BackstabAbility->TestEvaluateBackstabGeometry(Player, Enemy, Dist2D, AngleDeg);
			TestTrue(TEXT("Backstab geometry passes for directly behind target"), bBackGeoPass);
			TestEqual(TEXT("Distance is approx 150"), FMath::RoundToInt(Dist2D), 150);
			TestTrue(TEXT("Angle is approx 0 degrees"), AngleDeg <= 1.0f);

			// Case 4.2: Angle at 45 degrees (< 60 degrees threshold)
			Enemy->SetActorRotation(FRotator(0.0f, 45.0f, 0.0f));
			TestTrue(TEXT("Backstab geometry passes for 45 deg angle (< 60)"),
				BackstabAbility->TestEvaluateBackstabGeometry(Player, Enemy, Dist2D, AngleDeg));
			TestTrue(TEXT("Angle is approx 45 degrees"), FMath::IsNearlyEqual(AngleDeg, 45.0f, 1.0f));

			// Case 4.3: Angle at 75 degrees (> 60 degrees threshold)
			Enemy->SetActorRotation(FRotator(0.0f, 75.0f, 0.0f));
			TestFalse(TEXT("Backstab geometry rejected for 75 deg angle (> 60)"),
				BackstabAbility->TestEvaluateBackstabGeometry(Player, Enemy, Dist2D, AngleDeg));

			// Case 4.4: Directly from front (Enemy facing towards Player at 180 deg Yaw)
			Enemy->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f)); // Enemy forward is (-1, 0, 0), facing player
			TestFalse(TEXT("Backstab geometry rejected from front (180 deg)"),
				BackstabAbility->TestEvaluateBackstabGeometry(Player, Enemy, Dist2D, AngleDeg));
			TestTrue(TEXT("Front angle relative to back is approx 180 degrees"), AngleDeg >= 179.0f);

			// Case 4.5: Distance too close (< 50cm) or too far (> 250cm)
			Enemy->SetActorRotation(FRotator(0.0f, 0.0f, 0.0f));
			Enemy->SetActorLocation(FVector(30.0f, 0.0f, 0.0f));
			TestFalse(TEXT("Backstab geometry rejected when too close (30cm < 50cm)"),
				BackstabAbility->TestEvaluateBackstabGeometry(Player, Enemy, Dist2D, AngleDeg));

			Enemy->SetActorLocation(FVector(300.0f, 0.0f, 0.0f));
			TestFalse(TEXT("Backstab geometry rejected when too far (300cm > 250cm)"),
				BackstabAbility->TestEvaluateBackstabGeometry(Player, Enemy, Dist2D, AngleDeg));

			// Case 4.6: NaN, Inf, and zero-vector robustness (Fail-Closed Finite Contract)
			float BadDist = 0.0f, BadAngle = 0.0f;
			const FVector GoodPlayerLoc(0.0f, 0.0f, 0.0f);
			const FVector GoodEnemyLoc(150.0f, 0.0f, 0.0f);
			const FVector GoodEnemyForward(1.0f, 0.0f, 0.0f);

			// Player Location NaN / Inf
			TestFalse(TEXT("Backstab geometry rejected for NaN Player X location"),
				UPlayerBackstabExecutionAbility::TestEvaluateBackstabGeometryVectors(
					FVector(NAN, 0.0f, 0.0f), GoodEnemyLoc, GoodEnemyForward, 50.0f, 250.0f, 60.0f, BadDist, BadAngle));

			TestFalse(TEXT("Backstab geometry rejected for Inf Player Y location"),
				UPlayerBackstabExecutionAbility::TestEvaluateBackstabGeometryVectors(
					FVector(0.0f, INFINITY, 0.0f), GoodEnemyLoc, GoodEnemyForward, 50.0f, 250.0f, 60.0f, BadDist, BadAngle));

			// Enemy Location NaN / Inf
			TestFalse(TEXT("Backstab geometry rejected for NaN Enemy Z location"),
				UPlayerBackstabExecutionAbility::TestEvaluateBackstabGeometryVectors(
					GoodPlayerLoc, FVector(150.0f, 0.0f, NAN), GoodEnemyForward, 50.0f, 250.0f, 60.0f, BadDist, BadAngle));

			// Enemy Forward Vector NaN / Inf / Zero
			TestFalse(TEXT("Backstab geometry rejected for NaN Enemy Forward"),
				UPlayerBackstabExecutionAbility::TestEvaluateBackstabGeometryVectors(
					GoodPlayerLoc, GoodEnemyLoc, FVector(NAN, 0.0f, 0.0f), 50.0f, 250.0f, 60.0f, BadDist, BadAngle));

			TestFalse(TEXT("Backstab geometry rejected for zero Enemy Forward vector"),
				UPlayerBackstabExecutionAbility::TestEvaluateBackstabGeometryVectors(
					GoodPlayerLoc, GoodEnemyLoc, FVector(0.0f, 0.0f, 0.0f), 50.0f, 250.0f, 60.0f, BadDist, BadAngle));

			// Coincident positions (zero-distance vector between Player and Enemy)
			TestFalse(TEXT("Backstab geometry rejected for coincident locations (zero distance)"),
				UPlayerBackstabExecutionAbility::TestEvaluateBackstabGeometryVectors(
					FVector(100.0f, 100.0f, 0.0f), FVector(100.0f, 100.0f, 0.0f), GoodEnemyForward, 50.0f, 250.0f, 60.0f, BadDist, BadAngle));

			// Case 4.7: Exact 0-degree threshold contract [0, 90]
			float ZeroAngleDist = 0.0f, ZeroAngleDeg = 0.0f;
			TestTrue(TEXT("Backstab geometry passes for exact 0-degree threshold when perfectly aligned"),
				UPlayerBackstabExecutionAbility::TestEvaluateBackstabGeometryVectors(
					GoodPlayerLoc, GoodEnemyLoc, GoodEnemyForward, 50.0f, 250.0f, 0.0f, ZeroAngleDist, ZeroAngleDeg));
			TestTrue(TEXT("Angle is within 0-deg boundary"), ZeroAngleDeg <= 0.001f);

			// Slightly off-angle (e.g. 5 deg offset) should fail when threshold is 0 deg
			const FVector OffForward = FRotator(0.0f, 5.0f, 0.0f).Vector();
			TestFalse(TEXT("Backstab geometry rejects off-angle when threshold is exact 0 deg"),
				UPlayerBackstabExecutionAbility::TestEvaluateBackstabGeometryVectors(
					GoodPlayerLoc, GoodEnemyLoc, OffForward, 50.0f, 250.0f, 0.0f, ZeroAngleDist, ZeroAngleDeg));

			// Negative threshold (< 0) must be rejected
			TestFalse(TEXT("Backstab geometry rejects negative max angle threshold"),
				UPlayerBackstabExecutionAbility::TestEvaluateBackstabGeometryVectors(
					GoodPlayerLoc, GoodEnemyLoc, GoodEnemyForward, 50.0f, 250.0f, -1.0f, ZeroAngleDist, ZeroAngleDeg));

			// Reset enemy location & rotation
			Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
			Enemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
			Enemy->SetActorRotation(FRotator(0.0f, 0.0f, 0.0f));
		}
		PlayerASC->ClearAbility(BackstabHandle);
		ResetWeaponExecutionMontages(Player);
	}

	// =========================================================================
	// 5. Target Prerequisites & Stunned Exclusion Gate
	// =========================================================================
	{
		UAnimMontage* SyntheticMontage = NewObject<UAnimMontage>();
		TSubclassOf<UGameplayEffect> DamageGEClass = UTestProjectileDamageGE::StaticClass();

		auto [BackstabHandle, BackstabAbility] = GrantAndConfigureBackstabAbility(Player, SyntheticMontage, DamageGEClass, 50.0f, 250.0f, 60.0f);

		FGameplayAbilityActorInfo ActorInfo;
		ActorInfo.InitFromActor(Player, Player, PlayerASC);

		// 5.1 No target locked -> CanActivateAbility should fail
		Player->TestClearLockedTarget();
		TestFalse(TEXT("Fails activation when no target locked"),
			BackstabAbility->CanActivateAbility(BackstabHandle, &ActorInfo));

		// 5.2 Target is Stunned without StanceBreak (S=0, C=1) -> Backstab MUST FAIL
		Player->SetTestLockedTarget(Enemy);
		EnemyASC->AddLooseGameplayTag(TagStunned);
		TestFalse(TEXT("Fails activation when target has external Stunned without StanceBreak (S=0, C=1)"),
			BackstabAbility->CanActivateAbility(BackstabHandle, &ActorInfo));
		EnemyASC->RemoveLooseGameplayTag(TagStunned);

		// 5.2b Target in StanceBreak with matching single Stunned contribution (S=1, C=1) -> CanActivateAbility MUST SUCCEED
		{
			// Use a real StanceBreak activation for the positive ownership path.
			const FGameplayAbilitySpecHandle ActiveStanceBreakHandle = ActivateTestStanceBreak(Enemy);
			FGameplayAbilitySpec* ActiveStanceBreakSpec = EnemyASC->FindAbilitySpecFromHandle(ActiveStanceBreakHandle);
			TestTrue(TEXT("Real StanceBreak ability is active for positive ownership path"),
				ActiveStanceBreakSpec && ActiveStanceBreakSpec->IsActive());
			TestEqual(TEXT("Real StanceBreak contributes exactly one Stunned tag"),
				EnemyASC->GetTagCount(TagStunned), 1);
			TestTrue(TEXT("CanActivateAbility succeeds for target in real StanceBreak from behind (S=1, C=1)"),
				BackstabAbility->CanActivateAbility(BackstabHandle, &ActorInfo));

			EnemyASC->ClearAbility(ActiveStanceBreakHandle);

			// The remaining rows are synthetic state-table edges: they validate the
			// fail-closed count contract without pretending to model tag ownership.
			FGameplayAbilitySpec StanceBreakSpec(UEnemyStanceBreakAbility::StaticClass(), 1, INDEX_NONE, Enemy);
			const FGameplayAbilitySpecHandle EnemySBHandle = EnemyASC->GiveAbility(StanceBreakSpec);
			if (FGameplayAbilitySpec* FoundSBSpec = EnemyASC->FindAbilitySpecFromHandle(EnemySBHandle))
			{
				FoundSBSpec->ActivationInfo.SetActivationConfirmed();
				FoundSBSpec->ActiveCount = 1;
			}

			// S=1, C=0: StanceBreak ability is active but Stunned tag contribution is missing -> MUST FAIL
			TestFalse(TEXT("Fails activation when target has StanceBreak without Stunned tag (S=1, C=0)"),
				BackstabAbility->CanActivateAbility(BackstabHandle, &ActorInfo));

			// 5.2c Target in StanceBreak but with extra Stunned contribution (S=1, C=2) -> MUST FAIL
			EnemyASC->AddLooseGameplayTag(TagStunned);
			EnemyASC->AddLooseGameplayTag(TagStunned);
			TestFalse(TEXT("Fails activation when StanceBreak target has extra Stunned contribution (S=1, C=2)"),
				BackstabAbility->CanActivateAbility(BackstabHandle, &ActorInfo));
			EnemyASC->RemoveLooseGameplayTag(TagStunned);

			// 5.2d Multiple active StanceBreak abilities (S=2, C=1) -> MUST FAIL
			FGameplayAbilitySpec StanceBreakSpec2(UEnemyStanceBreakAbility::StaticClass(), 1, INDEX_NONE, Enemy);
			const FGameplayAbilitySpecHandle EnemySBHandle2 = EnemyASC->GiveAbility(StanceBreakSpec2);
			if (FGameplayAbilitySpec* FoundSBSpec2 = EnemyASC->FindAbilitySpecFromHandle(EnemySBHandle2))
			{
				FoundSBSpec2->ActivationInfo.SetActivationConfirmed();
				FoundSBSpec2->ActiveCount = 1;
			}
			TestFalse(TEXT("Fails activation when target has multiple active StanceBreak abilities (S=2, C=1)"),
				BackstabAbility->CanActivateAbility(BackstabHandle, &ActorInfo));

			// Clean up StanceBreak specs and tag
			EnemyASC->ClearAbility(EnemySBHandle2);
			EnemyASC->ClearAbility(EnemySBHandle);
			EnemyASC->RemoveLooseGameplayTag(TagStunned);
		}

		// 5.3 Target is NOT Stunned, living, valid lock & behind geometry -> CanActivateAbility should SUCCEED
		TestTrue(TEXT("CanActivateAbility succeeds for living non-stunned target from behind"),
			BackstabAbility->CanActivateAbility(BackstabHandle, &ActorInfo));

		// 5.4 Target is Invulnerable -> Should fail
		EnemyASC->AddLooseGameplayTag(TagInvulnerable);
		TestFalse(TEXT("Fails activation when target is Invulnerable"),
			BackstabAbility->CanActivateAbility(BackstabHandle, &ActorInfo));
		EnemyASC->RemoveLooseGameplayTag(TagInvulnerable);

		// 5.5 Weapon ExecutionSnapDistance range check gate [MinExecutionDistance, MaxExecutionDistance]
		{
			UWeaponEquipmentComponent* EquipComp = Player->FindComponentByClass<UWeaponEquipmentComponent>();
			UMeleeWeaponDefinition* WeaponDef = EquipComp ? EquipComp->GetEquippedMainHandMelee() : nullptr;
			if (TestNotNull(TEXT("Main hand melee weapon exists for backstab snap check"), WeaponDef))
			{
				const float OriginalSnapDist = WeaponDef->ExecutionSnapDistance;

				WeaponDef->ExecutionSnapDistance = 300.0f;
				TestFalse(TEXT("CanActivateAbility fails when ExecutionSnapDistance > MaxExecutionDistance"),
					BackstabAbility->CanActivateAbility(BackstabHandle, &ActorInfo));

				WeaponDef->ExecutionSnapDistance = 20.0f;
				TestFalse(TEXT("CanActivateAbility fails when ExecutionSnapDistance < MinExecutionDistance"),
					BackstabAbility->CanActivateAbility(BackstabHandle, &ActorInfo));

				WeaponDef->ExecutionSnapDistance = -10.0f;
				TestFalse(TEXT("CanActivateAbility fails when ExecutionSnapDistance is negative"),
					BackstabAbility->CanActivateAbility(BackstabHandle, &ActorInfo));

				WeaponDef->ExecutionSnapDistance = OriginalSnapDist;
				TestTrue(TEXT("CanActivateAbility succeeds after restoring valid ExecutionSnapDistance"),
					BackstabAbility->CanActivateAbility(BackstabHandle, &ActorInfo));
			}
		}

		// 5.6 Weapon Execution Montage Gate: Missing BackstabExecutionMontage rejects CanActivateAbility
		{
			ResetWeaponExecutionMontages(Player);
			TestFalse(TEXT("CanActivateAbility fails when BackstabExecutionMontage is null"),
				BackstabAbility->CanActivateAbility(BackstabHandle, &ActorInfo));

			// Even if FrontExecutionMontage is set, Backstab execution must reject it
			UAnimMontage* FrontOnlyMontage = NewObject<UAnimMontage>();
			if (UWeaponEquipmentComponent* EquipComp = Player->FindComponentByClass<UWeaponEquipmentComponent>())
			{
				if (UMeleeWeaponDefinition* WeaponDef = EquipComp->GetEquippedMainHandMelee())
				{
					WeaponDef->FrontExecutionMontage = FrontOnlyMontage;
				}
			}
			TestFalse(TEXT("Backstab CanActivateAbility fails when only FrontExecutionMontage is configured"),
				BackstabAbility->CanActivateAbility(BackstabHandle, &ActorInfo));

			// Restore BackstabExecutionMontage
			BackstabAbility->SetTestExecutionMontage(SyntheticMontage);
			TestTrue(TEXT("CanActivateAbility succeeds after restoring BackstabExecutionMontage"),
				BackstabAbility->CanActivateAbility(BackstabHandle, &ActorInfo));
		}

		// Cleanup
		PlayerASC->ClearAbility(BackstabHandle);
		ResetWeaponExecutionMontages(Player);
	}

	// =========================================================================
	// 6. Activation & Target Reservation Gate
	// =========================================================================
	{
		UAnimMontage* SyntheticMontage = NewObject<UAnimMontage>();
		TSubclassOf<UGameplayEffect> DamageGEClass = UTestProjectileDamageGE::StaticClass();

		auto [BackstabHandle, BackstabAbility] = GrantAndConfigureBackstabAbility(Player, SyntheticMontage, DamageGEClass, 50.0f, 250.0f, 60.0f);

		Player->SetTestLockedTarget(Enemy);

		// Activate via primary attack input intent
		Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));

		TestNotNull(TEXT("Backstab ability reserved the valid enemy target"),
			BackstabAbility->GetTestReservedTarget());
		TestEqual(TEXT("Reserved target is indeed Enemy"),
			BackstabAbility->GetTestReservedTarget(), Enemy);

		TestTrue(TEXT("Backstab ability is active"), BackstabAbility->IsActive());
		TestEqual(TEXT("Active execution montage snapshot matches SyntheticMontage"),
			BackstabAbility->GetTestActiveExecutionMontage(), SyntheticMontage);
		UWeaponEquipmentComponent* PlayerEquip = Player->FindComponentByClass<UWeaponEquipmentComponent>();
		const UMeleeWeaponDefinition* ExpectedWeapon = PlayerEquip ? PlayerEquip->GetEquippedMainHandMelee() : nullptr;
		TestEqual(TEXT("Active execution weapon definition snapshot matches equipped melee"),
			BackstabAbility->GetTestActiveExecutionWeaponDefinition(), ExpectedWeapon);

		// Post-activation weapon mutation does NOT alter active snapshot
		UAnimMontage* MutatedMontage = NewObject<UAnimMontage>();
		if (UMeleeWeaponDefinition* WeaponDef = PlayerEquip ? PlayerEquip->GetEquippedMainHandMelee() : nullptr)
		{
			WeaponDef->BackstabExecutionMontage = MutatedMontage;
			WeaponDef->MinExecutionDistance = 30.0f;
			WeaponDef->MaxExecutionDistance = 220.0f;
			WeaponDef->ExecutionSnapDistance = 160.0f;
		}
		TestEqual(TEXT("Active execution montage snapshot remains stable after weapon definition mutation"),
			BackstabAbility->GetTestActiveExecutionMontage(), SyntheticMontage);
		TestEqual(TEXT("Active execution min distance snapshot remains stable after weapon definition mutation"),
			BackstabAbility->GetTestActiveMinExecutionDistance(), 50.0f);
		TestEqual(TEXT("Active execution max distance snapshot remains stable after weapon definition mutation"),
			BackstabAbility->GetTestActiveMaxExecutionDistance(), 250.0f);
		TestEqual(TEXT("Active execution snap distance snapshot remains stable after weapon definition mutation"),
			BackstabAbility->GetTestActiveExecutionSnapDistance(), 190.0f);
		TestTrue(TEXT("Active execution distance snapshot flag is set"),
			BackstabAbility->HasTestActiveExecutionDistanceSnapshot());

		// Equipment swap gating: EquipWeapon and TryEquipWorldPickup must be refused while ability is active
		UMeleeWeaponDefinition* SecondaryWeapon = NewObject<UMeleeWeaponDefinition>(Player, NAME_None, RF_Transient);
		SecondaryWeapon->HandSlot = EWeaponHandSlot::MainHandOneHanded;
		SecondaryWeapon->AttachSocketName = FName(TEXT("Weapon_R"));
		SecondaryWeapon->bUseOwnerMeshSocketForTrace = true;
		TestFalse(TEXT("EquipWeapon refused while backstab execution is active"),
			PlayerEquip->EquipWeapon(SecondaryWeapon));
		TestFalse(TEXT("TryEquipWorldPickup refused while backstab execution is active"),
			PlayerEquip->TryEquipWorldPickup(nullptr));

		// Verify Backstab Snap Transform: Enemy is at (150, 0, 0) facing (+1, 0, 0), Distance is 190
		// Expected Player Location: (150 - 190, 0, 0) = (-40, 0, 0)
		// Expected Player Facing: +Forward = (+1, 0, 0) -> Yaw = 0 deg
		const FVector ExpectedSnapLoc(-40.0f, 0.0f, 0.0f);
		TestTrue(TEXT("Player location snapped to target back within 1cm"),
			FVector::Dist(Player->GetActorLocation(), ExpectedSnapLoc) <= 1.0f);
		TestTrue(TEXT("Player rotation snapped facing target (+Forward) within 1deg"),
			FMath::Abs(FMath::FindDeltaAngleDegrees(Player->GetActorRotation().Yaw, 0.0f)) <= 1.0f);

		// Verify Enemy entered MOVE_None under victim execution lock
		if (UCharacterMovementComponent* EnemyMove = Enemy->GetCharacterMovement())
		{
			TestEqual(TEXT("Enemy movement mode is MOVE_None under backstab execution lock"),
				EnemyMove->MovementMode, MOVE_None);
		}

		// End ability
		BackstabAbility->TestEndAbility();
		TestNull(TEXT("Target reservation cleared on EndAbility"),
			BackstabAbility->GetTestReservedTarget());
		TestNull(TEXT("Active execution montage snapshot cleared on EndAbility"),
			BackstabAbility->GetTestActiveExecutionMontage());
		TestNull(TEXT("Active execution weapon definition snapshot cleared on EndAbility"),
			BackstabAbility->GetTestActiveExecutionWeaponDefinition());

		// Verify Enemy restored to MOVE_Walking after release
		if (UCharacterMovementComponent* EnemyMove = Enemy->GetCharacterMovement())
		{
			TestEqual(TEXT("Enemy movement mode restored to MOVE_Walking after backstab release"),
				EnemyMove->MovementMode, MOVE_Walking);
		}

		// Reset Player location for subsequent sections
		Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
		Player->SetActorRotation(FRotator::ZeroRotator);

		PlayerASC->ClearAbility(BackstabHandle);
		ResetWeaponExecutionMontages(Player);
	}

	// =========================================================================
	// 7. Hit Event Routing, Exactly-Once Consumption & Damage Resolution
	// =========================================================================
	{
		UAnimMontage* SyntheticMontage = NewObject<UAnimMontage>();
		TSubclassOf<UGameplayEffect> DamageGEClass = UTestProjectileDamageGE::StaticClass();

		auto [BackstabHandle, BackstabAbility] = GrantAndConfigureBackstabAbility(Player, SyntheticMontage, DamageGEClass, 50.0f, 250.0f, 60.0f);

		// Prepare Enemy
		if (UCharacterAttributeSet* EnemyAttribs = const_cast<UCharacterAttributeSet*>(EnemyASC->GetSet<UCharacterAttributeSet>()))
		{
			EnemyAttribs->SetHealth(100.0f);
		}

		Player->SetTestLockedTarget(Enemy);
		Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));

		// 7.1 Send mismatched notify (wrong event tag) -> No damage
		FGameplayEventData BadPayload;
		BadPayload.EventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Attack.Light.Combo.BranchWindow.Begin")), false);
		BadPayload.Instigator = Player;
		BadPayload.Target = Player;
		BadPayload.OptionalObject = SyntheticMontage;
		BackstabAbility->TestTriggerHitEvent(BadPayload);
		TestFalse(TEXT("Wrong event tag does not consume damage event"), BackstabAbility->IsTestDamageEventConsumed());

		// 7.2 Send mismatched notify (wrong instigator) -> No damage
		BadPayload.EventTag = TagHitEvent;
		BadPayload.Instigator = Enemy;
		BackstabAbility->TestTriggerHitEvent(BadPayload);
		TestFalse(TEXT("Wrong instigator does not consume damage event"), BackstabAbility->IsTestDamageEventConsumed());

		// 7.3 Send mismatched notify (wrong target) -> No damage
		BadPayload.Instigator = Player;
		BadPayload.Target = Enemy;
		BackstabAbility->TestTriggerHitEvent(BadPayload);
		TestFalse(TEXT("Wrong target does not consume damage event"), BackstabAbility->IsTestDamageEventConsumed());

		// 7.4 Send mismatched notify (wrong montage) -> No damage
		UAnimMontage* WrongMontage = NewObject<UAnimMontage>();
		BadPayload.Target = Player;
		BadPayload.OptionalObject = WrongMontage;
		BackstabAbility->TestTriggerHitEvent(BadPayload);
		TestFalse(TEXT("Wrong montage does not consume damage event"), BackstabAbility->IsTestDamageEventConsumed());

		// 7.5 Strict Montage Check: Missing OptionalObject (nullptr) -> No damage
		BadPayload.OptionalObject = nullptr;
		BackstabAbility->TestTriggerHitEvent(BadPayload);
		TestFalse(TEXT("Missing OptionalObject (nullptr) does not consume damage event"), BackstabAbility->IsTestDamageEventConsumed());

		// Record initial target health before correct hit
		const UCharacterAttributeSet* EnemyAttribSet = EnemyASC->GetSet<UCharacterAttributeSet>();
		const float HealthBeforeHit = EnemyAttribSet ? EnemyAttribSet->GetHealth() : 0.0f;
		TestTrue(TEXT("Enemy initial health is positive before backstab hit"), HealthBeforeHit > 0.0f);

		// 7.6 Correct Hit Event Payload -> Consumes damage event and resolves hit exactly once!
		FGameplayEventData GoodPayload;
		GoodPayload.EventTag = TagHitEvent;
		GoodPayload.Instigator = Player;
		GoodPayload.Target = Player;
		GoodPayload.OptionalObject = SyntheticMontage;
		BackstabAbility->TestTriggerHitEvent(GoodPayload);
		TestTrue(TEXT("Correct hit event consumed damage event exactly once"), BackstabAbility->IsTestDamageEventConsumed());

		const float HealthAfterFirstHit = EnemyAttribSet ? EnemyAttribSet->GetHealth() : 0.0f;
		TestTrue(TEXT("First correct hit event applied damage and reduced health"), HealthAfterFirstHit < HealthBeforeHit);

		// 7.7 Second hit event must be ignored (idempotent / exactly-once damage guarantee)
		BackstabAbility->TestTriggerHitEvent(GoodPayload);
		TestTrue(TEXT("Damage event remains consumed, not re-triggered"), BackstabAbility->IsTestDamageEventConsumed());

		const float HealthAfterSecondHit = EnemyAttribSet ? EnemyAttribSet->GetHealth() : 0.0f;
		TestEqual(TEXT("Second hit event does not apply additional damage"), HealthAfterSecondHit, HealthAfterFirstHit);

		// Cleanup
		BackstabAbility->TestEndAbility();
		PlayerASC->ClearAbility(BackstabHandle);

		// 7.8 TryResolveHit returns false (e.g. Friendly Fire / Same Team) -> EndAbility cleans up and prevents retry
		{
			auto [FailedHandle, FailedAbility] = GrantAndConfigureBackstabAbility(Player, SyntheticMontage, DamageGEClass, 50.0f, 250.0f, 60.0f);
			if (UCharacterAttributeSet* EnemyAttribs = const_cast<UCharacterAttributeSet*>(EnemyASC->GetSet<UCharacterAttributeSet>()))
			{
				EnemyAttribs->SetHealth(100.0f);
			}

			Player->SetTestLockedTarget(Enemy);
			Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));

			TestNotNull(TEXT("Ability active with reserved target before failed resolve"), FailedAbility->GetTestReservedTarget());

			// Set Enemy team to match Player team (Team.Player), causing FMeleeHitResolver::TryResolveHit to return false
			const FGameplayTag OriginalEnemyTeam = ICombatTeamAgent::Execute_GetCombatTeamTag(Enemy);
			const FGameplayTag PlayerTeamTag = ICombatTeamAgent::Execute_GetCombatTeamTag(Player);
			Enemy->SetTestCombatTeamTag(PlayerTeamTag);

			const float HealthBeforeFailedHit = EnemyAttribSet ? EnemyAttribSet->GetHealth() : 0.0f;
			FailedAbility->TestTriggerHitEvent(GoodPayload);

			// Failed hit resolution immediately invoked EndAbility, clearing target reservation and stopping ability
			TestNull(TEXT("Failed hit resolution ended ability and cleared target reservation"), FailedAbility->GetTestReservedTarget());
			TestFalse(TEXT("Failed hit resolution terminated active ability"), FailedAbility->IsActive());
			TestEqual(TEXT("Same team target took no damage"), EnemyAttribSet ? EnemyAttribSet->GetHealth() : 0.0f, HealthBeforeFailedHit);

			// Attempting a second hit event after EndAbility must not apply damage / must not retry
			FailedAbility->TestTriggerHitEvent(GoodPayload);
			TestEqual(TEXT("Subsequent hit event after EndAbility does not apply damage"),
				EnemyAttribSet ? EnemyAttribSet->GetHealth() : 0.0f, HealthBeforeFailedHit);

			Enemy->SetTestCombatTeamTag(OriginalEnemyTeam);
			PlayerASC->ClearAbility(FailedHandle);
			ResetWeaponExecutionMontages(Player);
		}
	}

	// =========================================================================
	// 8. Snapshot Orientation Guarantee (Target Turns After Activation)
	// =========================================================================
	{
		UAnimMontage* SyntheticMontage = NewObject<UAnimMontage>();
		TSubclassOf<UGameplayEffect> DamageGEClass = UTestProjectileDamageGE::StaticClass();

		auto [BackstabHandle, BackstabAbility] = GrantAndConfigureBackstabAbility(Player, SyntheticMontage, DamageGEClass, 50.0f, 250.0f, 60.0f);

		if (UCharacterAttributeSet* EnemyAttribs = const_cast<UCharacterAttributeSet*>(EnemyASC->GetSet<UCharacterAttributeSet>()))
		{
			EnemyAttribs->SetHealth(100.0f);
		}

		// Player at (0,0,0), Enemy at (150,0,0) facing (+1,0,0) -> Player is behind Enemy
		Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
		Enemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
		Enemy->SetActorRotation(FRotator(0.0f, 0.0f, 0.0f));

		Player->SetTestLockedTarget(Enemy);
		Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));

		TestTrue(TEXT("Backstab activated on initial snapshot"), BackstabAbility->IsActive());

		// Enemy now turns around 180 degrees during windup (facing Player at 180 deg)
		Enemy->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f));

		const UCharacterAttributeSet* EnemyAttribSet = EnemyASC->GetSet<UCharacterAttributeSet>();
		const float HealthBeforeTurnHit = EnemyAttribSet ? EnemyAttribSet->GetHealth() : 0.0f;

		// Hit event arrives -> Hit should still SUCCEED per snapshot orientation contract
		FGameplayEventData GoodPayload;
		GoodPayload.EventTag = TagHitEvent;
		GoodPayload.Instigator = Player;
		GoodPayload.Target = Player;
		GoodPayload.OptionalObject = SyntheticMontage;
		BackstabAbility->TestTriggerHitEvent(GoodPayload);

		TestTrue(TEXT("Backstab successfully hit turned enemy via snapshot orientation contract"),
			BackstabAbility->IsTestDamageEventConsumed());
		TestTrue(TEXT("Enemy health was reduced despite turning during execution windup"),
			(EnemyAttribSet ? EnemyAttribSet->GetHealth() : 0.0f) < HealthBeforeTurnHit);

		// Reset enemy rotation
		Enemy->SetActorRotation(FRotator(0.0f, 0.0f, 0.0f));
		BackstabAbility->TestEndAbility();
		PlayerASC->ClearAbility(BackstabHandle);
		ResetWeaponExecutionMontages(Player);
	}

	// =========================================================================
	// 9. Teardown, Stunned-Gain & Range-Loss Fail-Closed Gates
	// =========================================================================
	{
		UAnimMontage* SyntheticMontage = NewObject<UAnimMontage>();
		TSubclassOf<UGameplayEffect> DamageGEClass = UTestProjectileDamageGE::StaticClass();

		// Case 9.1: Target loses VictimLocked tag during execution -> Ability ends immediately
		{
			auto [BackstabHandle, BackstabAbility] = GrantAndConfigureBackstabAbility(Player, SyntheticMontage, DamageGEClass, 50.0f, 250.0f, 60.0f);
			Player->SetTestLockedTarget(Enemy);
			Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));

			TestNotNull(TEXT("Backstab active with reserved target"), BackstabAbility->GetTestReservedTarget());

			const FGameplayTag TagVictimLocked = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Execution.VictimLocked")), false);
			EnemyASC->RemoveLooseGameplayTag(TagVictimLocked);
			TickBackstabExecutionTestWorld(World, 0.01f);

			TestNull(TEXT("Backstab ended and target reservation cleared when target lost VictimLocked"),
				BackstabAbility->GetTestReservedTarget());

			PlayerASC->ClearAbility(BackstabHandle);
			ResetWeaponExecutionMontages(Player);
		}

		// Case 9.2: Target escapes distance range before hit event -> Hit rejected without damage
		{
			auto [BackstabHandle, BackstabAbility] = GrantAndConfigureBackstabAbility(Player, SyntheticMontage, DamageGEClass, 50.0f, 250.0f, 60.0f);
			if (UCharacterAttributeSet* EnemyAttribs = const_cast<UCharacterAttributeSet*>(EnemyASC->GetSet<UCharacterAttributeSet>()))
			{
				EnemyAttribs->SetHealth(100.0f);
			}

			Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
			Enemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
			Enemy->SetActorRotation(FRotator(0.0f, 0.0f, 0.0f));

			Player->SetTestLockedTarget(Enemy);
			Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));

			// Enemy escapes to 400cm (> 250cm max distance)
			Enemy->SetActorLocation(FVector(400.0f, 0.0f, 0.0f));

			const UCharacterAttributeSet* EnemyAttribSet = EnemyASC->GetSet<UCharacterAttributeSet>();
			const float HealthBeforeEscape = EnemyAttribSet ? EnemyAttribSet->GetHealth() : 0.0f;

			FGameplayEventData GoodPayload;
			GoodPayload.EventTag = TagHitEvent;
			GoodPayload.Instigator = Player;
			GoodPayload.Target = Player;
			GoodPayload.OptionalObject = SyntheticMontage;
			BackstabAbility->TestTriggerHitEvent(GoodPayload);

			TestEqual(TEXT("Enemy out of range took no damage on hit event"),
				EnemyAttribSet ? EnemyAttribSet->GetHealth() : 0.0f, HealthBeforeEscape);

			// Reset Enemy location
			Enemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
			BackstabAbility->TestEndAbility();
			PlayerASC->ClearAbility(BackstabHandle);
			ResetWeaponExecutionMontages(Player);
		}
	}

	// =========================================================================
	// 10. Input Routing Priority & Fallback Gates with Direct Primary Assertion
	// =========================================================================
	{
		UAnimMontage* SyntheticMontage = NewObject<UAnimMontage>();
		TSubclassOf<UGameplayEffect> DamageGEClass = UTestProjectileDamageGE::StaticClass();
		const FGameplayTag PrimaryAttackInputTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false);

		// 10.1 Front Execution takes precedence over Backstab when target is Stunned in Front
		{
			auto [BackstabHandle, BackstabAbility] = GrantAndConfigureBackstabAbility(Player, SyntheticMontage, DamageGEClass, 50.0f, 250.0f, 60.0f);

			// Grant both Front Execution and Backstab Execution
			FGameplayAbilitySpec FrontSpec(UPlayerFrontExecutionAbility::StaticClass(), 1, INDEX_NONE, Player);
			const FGameplayAbilitySpecHandle FrontHandle = PlayerASC->GiveAbility(FrontSpec);
			FGameplayAbilitySpec* FoundFrontSpec = PlayerASC->FindAbilitySpecFromHandle(FrontHandle);
			UPlayerFrontExecutionAbility* FrontInstance = FoundFrontSpec ? Cast<UPlayerFrontExecutionAbility>(FoundFrontSpec->GetPrimaryInstance()) : nullptr;
			if (FrontInstance)
			{
				FrontInstance->SetTestSkipMontageTaskActivation(true);
				FrontInstance->SetTestExecutionMontage(SyntheticMontage);
				FrontInstance->SetTestDamageGameplayEffectClass(DamageGEClass);
				FrontInstance->SetTestExecutionDistances(50.0f, 250.0f);
				FrontInstance->SetTestMaxFrontAngleDegrees(60.0f);
			}

			// Target facing Player (180 deg) and Stunned with Poise=0 and active StanceBreak
			Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
			Enemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
			Enemy->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f));
			EnemyASC->AddLooseGameplayTag(TagStunned);
			if (UCharacterAttributeSet* EnemyAttribs = const_cast<UCharacterAttributeSet*>(EnemyASC->GetSet<UCharacterAttributeSet>()))
			{
				EnemyAttribs->SetPoise(0.0f);
			}
			FGameplayAbilitySpec StanceBreakSpec(UEnemyStanceBreakAbility::StaticClass(), 1, INDEX_NONE, Enemy);
			const FGameplayAbilitySpecHandle EnemySBHandle = EnemyASC->GiveAbility(StanceBreakSpec);
			if (FGameplayAbilitySpec* FoundSBSpec = EnemyASC->FindAbilitySpecFromHandle(EnemySBHandle))
			{
				FoundSBSpec->ActivationInfo.SetActivationConfirmed();
				FoundSBSpec->ActiveCount = 1;
			}

			Player->SetTestLockedTarget(Enemy);
			Player->TriggerTestRequestAbilityForInputIntent(PrimaryAttackInputTag);

			TestTrue(TEXT("Front Execution activated in precedence over Backstab"), FrontInstance ? FrontInstance->IsActive() : false);
			TestFalse(TEXT("Backstab was not activated when Front Execution was eligible"), BackstabAbility->IsActive());

			// Cleanup
			if (FrontInstance) FrontInstance->TestEndAbility();
			BackstabAbility->TestEndAbility();
			PlayerASC->ClearAbility(FrontHandle);
			PlayerASC->ClearAbility(BackstabHandle);
			EnemyASC->ClearAbility(EnemySBHandle);
			EnemyASC->RemoveLooseGameplayTag(TagStunned);
			ResetWeaponExecutionMontages(Player);
		}

		// 10.2 Stunned target attacked from behind -> Front & Backstab both reject -> smoothly falls back to direct Primary!
		{
			auto [BackstabHandle, BackstabAbility] = GrantAndConfigureBackstabAbility(Player, SyntheticMontage, DamageGEClass, 50.0f, 250.0f, 60.0f);

			FGameplayAbilitySpec FrontSpec(UPlayerFrontExecutionAbility::StaticClass(), 1, INDEX_NONE, Player);
			const FGameplayAbilitySpecHandle FrontHandle = PlayerASC->GiveAbility(FrontSpec);
			FGameplayAbilitySpec* FoundFrontSpec = PlayerASC->FindAbilitySpecFromHandle(FrontHandle);
			UPlayerFrontExecutionAbility* FrontInstance = FoundFrontSpec ? Cast<UPlayerFrontExecutionAbility>(FoundFrontSpec->GetPrimaryInstance()) : nullptr;
			if (FrontInstance)
			{
				FrontInstance->SetTestSkipMontageTaskActivation(true);
				FrontInstance->SetTestExecutionMontage(SyntheticMontage);
			}

			// Enemy facing away (Yaw 0), Player at (0,0,0) (Player is behind Enemy), Enemy is Stunned
			Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
			Enemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
			Enemy->SetActorRotation(FRotator(0.0f, 0.0f, 0.0f));
			EnemyASC->AddLooseGameplayTag(TagStunned);

			Player->SetTestLockedTarget(Enemy);

			// Establish held-input prerequisite and trigger input handling
			Player->TriggerTestHandleCombatInputStarted(PrimaryAttackInputTag);

			TestFalse(TEXT("Front execution rejected for behind geometry on Stunned target"), FrontInstance ? FrontInstance->IsActive() : false);
			TestFalse(TEXT("Backstab rejected because target is Stunned"), BackstabAbility->IsActive());

			// Assert direct Primary attack ability spec is accepted and active
			FGameplayAbilitySpec* PrimaryAttackSpec = nullptr;
			for (FGameplayAbilitySpec& Spec : PlayerASC->GetActivatableAbilities())
			{
				if (Spec.Ability && Spec.Ability->IsA<UPrimaryAttackAbility>())
				{
					PrimaryAttackSpec = &Spec;
					break;
				}
			}
			TestNotNull(TEXT("Direct PrimaryAttack ability spec exists on Player ASC"), PrimaryAttackSpec);
			TestTrue(TEXT("Direct PrimaryAttack ability is actively running upon execution fallback (Stunned target behind)"),
				PrimaryAttackSpec && PrimaryAttackSpec->IsActive());

			// End input state and clean up
			Player->TriggerTestHandleCombatInputEnded(PrimaryAttackInputTag);
			TickBackstabExecutionTestWorld(World, 0.01f);

			PlayerASC->ClearAbility(FrontHandle);
			PlayerASC->ClearAbility(BackstabHandle);
			EnemyASC->RemoveLooseGameplayTag(TagStunned);
			ResetWeaponExecutionMontages(Player);
		}

		// 10.2b StanceBreak target attacked from behind -> Front rejected, Backstab ACTIVATES (does NOT fallback to Primary)
		{
			auto [BackstabHandle, BackstabAbility] = GrantAndConfigureBackstabAbility(Player, SyntheticMontage, DamageGEClass, 50.0f, 250.0f, 60.0f);

			FGameplayAbilitySpec FrontSpec(UPlayerFrontExecutionAbility::StaticClass(), 1, INDEX_NONE, Player);
			const FGameplayAbilitySpecHandle FrontHandle = PlayerASC->GiveAbility(FrontSpec);
			FGameplayAbilitySpec* FoundFrontSpec = PlayerASC->FindAbilitySpecFromHandle(FrontHandle);
			UPlayerFrontExecutionAbility* FrontInstance = FoundFrontSpec ? Cast<UPlayerFrontExecutionAbility>(FoundFrontSpec->GetPrimaryInstance()) : nullptr;
			if (FrontInstance)
			{
				FrontInstance->SetTestSkipMontageTaskActivation(true);
				FrontInstance->SetTestExecutionMontage(SyntheticMontage);
			}

			// Enemy facing away (Yaw 0), Player at (0,0,0) (Player is behind Enemy), Enemy is in active StanceBreak (S=1, C=1)
			Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
			Enemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
			Enemy->SetActorRotation(FRotator(0.0f, 0.0f, 0.0f));
			if (UCharacterAttributeSet* EnemyAttribs = const_cast<UCharacterAttributeSet*>(EnemyASC->GetSet<UCharacterAttributeSet>()))
			{
				EnemyAttribs->SetPoise(0.0f);
			}
			const FGameplayAbilitySpecHandle EnemySBHandle = ActivateTestStanceBreak(Enemy);
			FGameplayAbilitySpec* FoundSBSpec = EnemyASC->FindAbilitySpecFromHandle(EnemySBHandle);
			TestTrue(TEXT("Real StanceBreak ability is active for Backstab arbitration"),
				FoundSBSpec && FoundSBSpec->IsActive());

			Player->SetTestLockedTarget(Enemy);

			// Establish held-input prerequisite and trigger input handling
			Player->TriggerTestHandleCombatInputStarted(PrimaryAttackInputTag);

			TestFalse(TEXT("Front execution rejected for behind geometry on StanceBreak target"), FrontInstance ? FrontInstance->IsActive() : false);
			TestTrue(TEXT("Backstab activates for behind geometry on valid StanceBreak target (S=1, C=1)"), BackstabAbility ? BackstabAbility->IsActive() : false);

			// Assert direct Primary attack ability was NOT activated (Backstab won arbitration)
			FGameplayAbilitySpec* PrimaryAttackSpec = nullptr;
			for (FGameplayAbilitySpec& Spec : PlayerASC->GetActivatableAbilities())
			{
				if (Spec.Ability && Spec.Ability->IsA<UPrimaryAttackAbility>())
				{
					PrimaryAttackSpec = &Spec;
					break;
				}
			}
			TestNotNull(TEXT("Direct PrimaryAttack ability spec exists on Player ASC"), PrimaryAttackSpec);
			TestFalse(TEXT("Direct PrimaryAttack is NOT active when Backstab activates on StanceBreak target"),
				PrimaryAttackSpec && PrimaryAttackSpec->IsActive());

			// End input state and clean up
			Player->TriggerTestHandleCombatInputEnded(PrimaryAttackInputTag);
			TickBackstabExecutionTestWorld(World, 0.01f);

			if (BackstabAbility)
			{
				BackstabAbility->TestEndAbility(false);
			}
			PlayerASC->ClearAbility(FrontHandle);
			PlayerASC->ClearAbility(BackstabHandle);
			EnemyASC->ClearAbility(EnemySBHandle);
			ResetWeaponExecutionMontages(Player);
		}

		// 10.3 No target locked -> Front & Backstab both reject -> fallback to direct Primary
		{
			auto [BackstabHandle, BackstabAbility] = GrantAndConfigureBackstabAbility(Player, SyntheticMontage, DamageGEClass, 50.0f, 250.0f, 60.0f);

			FGameplayAbilitySpec FrontSpec(UPlayerFrontExecutionAbility::StaticClass(), 1, INDEX_NONE, Player);
			const FGameplayAbilitySpecHandle FrontHandle = PlayerASC->GiveAbility(FrontSpec);
			FGameplayAbilitySpec* FoundFrontSpec = PlayerASC->FindAbilitySpecFromHandle(FrontHandle);
			UPlayerFrontExecutionAbility* FrontInstance = FoundFrontSpec ? Cast<UPlayerFrontExecutionAbility>(FoundFrontSpec->GetPrimaryInstance()) : nullptr;
			if (FrontInstance)
			{
				FrontInstance->SetTestSkipMontageTaskActivation(true);
				FrontInstance->SetTestExecutionMontage(SyntheticMontage);
			}

			Player->TestClearLockedTarget();

			// Establish held-input prerequisite and trigger input handling
			Player->TriggerTestHandleCombatInputStarted(PrimaryAttackInputTag);

			TestFalse(TEXT("Front execution rejected when no target locked"), FrontInstance ? FrontInstance->IsActive() : false);
			TestFalse(TEXT("Backstab rejected when no target locked"), BackstabAbility->IsActive());

			// Assert direct Primary attack ability spec is accepted and active
			FGameplayAbilitySpec* PrimaryAttackSpec = nullptr;
			for (FGameplayAbilitySpec& Spec : PlayerASC->GetActivatableAbilities())
			{
				if (Spec.Ability && Spec.Ability->IsA<UPrimaryAttackAbility>())
				{
					PrimaryAttackSpec = &Spec;
					break;
				}
			}
			TestNotNull(TEXT("Direct PrimaryAttack ability spec exists on Player ASC"), PrimaryAttackSpec);
			TestTrue(TEXT("Direct PrimaryAttack ability is actively running upon execution fallback (No target locked)"),
				PrimaryAttackSpec && PrimaryAttackSpec->IsActive());

			// End input state and clean up
			Player->TriggerTestHandleCombatInputEnded(PrimaryAttackInputTag);
			TickBackstabExecutionTestWorld(World, 0.01f);

			PlayerASC->ClearAbility(FrontHandle);
			PlayerASC->ClearAbility(BackstabHandle);
			ResetWeaponExecutionMontages(Player);
		}
	}

	// =========================================================================
	// 11. Generation Isolation, Reentrancy & Stale Callback Rejection Gate
	// =========================================================================
	{
		UAnimMontage* SyntheticMontage = NewObject<UAnimMontage>();
		TSubclassOf<UGameplayEffect> DamageGEClass = UTestProjectileDamageGE::StaticClass();

		auto [BackstabHandle, BackstabAbility] = GrantAndConfigureBackstabAbility(Player, SyntheticMontage, DamageGEClass, 50.0f, 250.0f, 60.0f);

		Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
		Enemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
		Enemy->SetActorRotation(FRotator(0.0f, 0.0f, 0.0f));
		Player->SetTestLockedTarget(Enemy);

		// --- Generation 1 (Activation A) ---
		Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));
		TestTrue(TEXT("Activation A is active"), BackstabAbility->IsActive());

		const uint32 TokenA = BackstabAbility->GetTestActivationToken();
		UPlayerBackstabExecutionContext* ContextA = BackstabAbility->GetTestActiveContext();
		TestNotNull(TEXT("Context A exists for Generation A"), ContextA);
		TestEqual(TEXT("Context A token matches Token A"), ContextA ? ContextA->Token : 0, TokenA);

		// End Activation A
		BackstabAbility->TestEndAbility();
		TestFalse(TEXT("Activation A ended"), BackstabAbility->IsActive());
		TestNull(TEXT("Context A cleared on EndAbility"), BackstabAbility->GetTestActiveContext());

		// Verify ActivationOwnedTags cleanly removed after EndAbility
		TestFalse(TEXT("EndAbility cleared State.Action.Attacking"),
			PlayerASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Attacking")), false)));
		TestFalse(TEXT("EndAbility cleared State.Input.Block.Movement"),
			PlayerASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Movement")), false)));
		TestFalse(TEXT("EndAbility cleared State.Input.Block.Jump"),
			PlayerASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Jump")), false)));

		// --- Generation 2 (Activation B) with identical target and montage pointers ---
		Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));
		TestTrue(TEXT("Activation B is active"), BackstabAbility->IsActive());

		const uint32 TokenB = BackstabAbility->GetTestActivationToken();
		UPlayerBackstabExecutionContext* ContextB = BackstabAbility->GetTestActiveContext();
		TestNotNull(TEXT("Context B exists for Generation B"), ContextB);
		TestTrue(TEXT("Token B is greater than Token A"), TokenB > TokenA);

		// Case 11.1: Stale Montage completed from Generation A must NOT terminate Activation B
		if (ContextA)
		{
			ContextA->OnMontageCompleted();
			TestTrue(TEXT("Stale MontageCompleted from Generation A was rejected; Activation B remains active"),
				BackstabAbility->IsActive());

			// Case 11.2: Stale TargetDestroyed from Generation A must NOT terminate Activation B
			ContextA->OnTargetDestroyed(Enemy);
			TestTrue(TEXT("Stale TargetDestroyed from Generation A was rejected; Activation B remains active"),
				BackstabAbility->IsActive());

			// Case 11.3: Stale Hit Event from Generation A must NOT trigger or consume in Activation B
			FGameplayEventData StalePayload;
			StalePayload.EventTag = TagHitEvent;
			StalePayload.Instigator = Player;
			StalePayload.Target = Player;
			StalePayload.OptionalObject = SyntheticMontage;
			ContextA->OnHitEventReceived(StalePayload);
			TestFalse(TEXT("Stale Hit Event from Generation A was rejected and did not consume damage event"),
				BackstabAbility->IsTestDamageEventConsumed());
		}

		// Case 11.4: Valid Montage completed from Generation B MUST terminate Activation B
		if (ContextB)
		{
			ContextB->OnMontageCompleted();
			TestFalse(TEXT("Valid MontageCompleted from Generation B terminated Activation B"),
				BackstabAbility->IsActive());
		}

		// Case 11.5: Re-activation with failed prerequisite must fail-closed without leaking owned tags
		EnemyASC->AddLooseGameplayTag(TagStunned); // Invalid prerequisite for Backstab
		Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));

		TestFalse(TEXT("Re-activation with Stunned target failed activation"), BackstabAbility->IsActive());
		TestFalse(TEXT("Failed activation did not leak State.Action.Attacking tag"),
			PlayerASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Attacking")), false)));
		TestFalse(TEXT("Failed activation did not leak State.Input.Block.Movement tag"),
			PlayerASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Movement")), false)));
		TestFalse(TEXT("Failed activation did not leak State.Input.Block.Jump tag"),
			PlayerASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Jump")), false)));

		EnemyASC->RemoveLooseGameplayTag(TagStunned);
		PlayerASC->ClearAbility(BackstabHandle);
		ResetWeaponExecutionMontages(Player);
	}

	// =========================================================================
	// 12. Active Ability + Inactive Startup Task Must Converge Through Cleanup
	// =========================================================================
	{
		UAnimMontage* SyntheticMontage = NewObject<UAnimMontage>();
		TSubclassOf<UGameplayEffect> DamageGEClass = UTestProjectileDamageGE::StaticClass();
		auto [FailureHandle, FailureAbility] = GrantAndConfigureBackstabAbility(Player, SyntheticMontage, DamageGEClass, 50.0f, 250.0f, 60.0f);

		if (TestNotNull(TEXT("Backstab ability instance valid for inactive-task cleanup test"), FailureAbility))
		{
			FailureAbility->SetTestInvalidateWaitHitEventTaskAfterReady(true);
			Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
			Enemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
			Enemy->SetActorRotation(FRotator(0.0f, 0.0f, 0.0f));
			Player->SetTestLockedTarget(Enemy);
			Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));

			TestFalse(TEXT("Inactive WaitGameplayEvent task ends the active Backstab ability"), FailureAbility->IsActive());
			TestNull(TEXT("Inactive-task failure clears the reserved target"), FailureAbility->GetTestReservedTarget());
			TestFalse(TEXT("Inactive-task failure clears State.Action.Attacking"),
				PlayerASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Attacking")), false)));
			TestFalse(TEXT("Inactive-task failure clears State.Input.Block.Movement"),
				PlayerASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Movement")), false)));
			TestFalse(TEXT("Inactive-task failure clears State.Input.Block.Jump"),
				PlayerASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Input.Block.Jump")), false)));
		}

		PlayerASC->ClearAbility(FailureHandle);
		ResetWeaponExecutionMontages(Player);
	}

	// =========================================================================
	// 13. ReadyForActivation Synchronous Re-entry Fail-Closed & Re-activation Gate
	// =========================================================================
	{
		UAnimMontage* SyntheticMontage = NewObject<UAnimMontage>();
		TSubclassOf<UGameplayEffect> DamageGEClass = UTestProjectileDamageGE::StaticClass();

		auto [ReentryHandle, ReentryAbility] = GrantAndConfigureBackstabAbility(Player, SyntheticMontage, DamageGEClass, 50.0f, 250.0f, 60.0f);

		Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
		Enemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
		Enemy->SetActorRotation(FRotator(0.0f, 0.0f, 0.0f));
		Player->SetTestLockedTarget(Enemy);

		// Configure simulated synchronous EndAbility during task ReadyForActivation
		ReentryAbility->SetTestEndAbilityDuringTaskReady(true);
		Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));

		// Verify ability ended cleanly without double-EndAbility crash, and tags/context are cleanly reset
		TestFalse(TEXT("Synchronous EndAbility in ReadyForActivation terminated Backstab cleanly"), ReentryAbility->IsActive());
		TestNull(TEXT("Target reservation cleared after synchronous end"), ReentryAbility->GetTestReservedTarget());
		TestFalse(TEXT("PlayerLocked tag not leaked after synchronous end"),
			PlayerASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Execution.PlayerLocked")), false)));

		// Re-enable normal activation and verify next activation succeeds cleanly
		ReentryAbility->SetTestEndAbilityDuringTaskReady(false);
		Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));
		TestTrue(TEXT("Subsequent activation after synchronous end succeeds"), ReentryAbility->IsActive());
		TestNotNull(TEXT("Subsequent activation reserved target"), ReentryAbility->GetTestReservedTarget());

		ReentryAbility->TestEndAbility();
		PlayerASC->ClearAbility(ReentryHandle);
		ResetWeaponExecutionMontages(Player);
	}

	// =========================================================================
	// 14. Environment Blocking Snap Rollback, Zero Velocity & Formal Release Gate
	// =========================================================================
	{
		UAnimMontage* SyntheticMontage = NewObject<UAnimMontage>();
		TSubclassOf<UGameplayEffect> DamageGEClass = UTestProjectileDamageGE::StaticClass();

		Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
		Player->SetActorRotation(FRotator::ZeroRotator);
		Enemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
		Enemy->SetActorRotation(FRotator(0.0f, 0.0f, 0.0f)); // Enemy forward is (+1, 0, 0)

		auto [BlockExecHandle, BlockExecAbility] = GrantAndConfigureBackstabAbility(Player, SyntheticMontage, DamageGEClass, 50.0f, 250.0f, 60.0f);

		Player->SetTestLockedTarget(Enemy);

		// Spawn temporary blocking actor at the backstab snap target location (-40, 0, 0)
		AActor* BlockingActor = World->SpawnActor<AActor>();
		if (TestNotNull(TEXT("Temporary blocking actor spawned for backstab"), BlockingActor))
		{
			UBoxComponent* BoxComp = NewObject<UBoxComponent>(BlockingActor);
			BoxComp->InitBoxExtent(FVector(50.0f, 50.0f, 100.0f));
			BoxComp->SetCollisionProfileName(TEXT("BlockAll"));
			BlockingActor->SetRootComponent(BoxComp);
			BoxComp->RegisterComponent();
			BlockingActor->SetActorLocation(FVector(-40.0f, 0.0f, 0.0f));

			const FVector OriginalPlayerLoc = Player->GetActorLocation();
			const FRotator OriginalPlayerRot = Player->GetActorRotation();

			// Trigger backstab attempt into the blocking actor
			Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));

			// Snap must fail, ability must end via cancel, and player transform rolled back
			TestFalse(TEXT("Backstab aborted cleanly when snap is blocked"), BlockExecAbility->IsActive());
			TestEqual(TEXT("Player location rolled back to original on backstab block"), Player->GetActorLocation(), OriginalPlayerLoc);
			TestTrue(TEXT("Player rotation rolled back to original on backstab block"),
				FMath::Abs(FMath::FindDeltaAngleDegrees(Player->GetActorRotation().Yaw, OriginalPlayerRot.Yaw)) <= 1.0f);
			TestTrue(TEXT("Player velocity cleared on backstab block rollback"), Player->GetVelocity().IsNearlyZero());
			TestNull(TEXT("Target reservation cleared after blocked backstab abort"), BlockExecAbility->GetTestReservedTarget());

			// Victim must be formally released and restored to MOVE_Walking
			if (UCharacterMovementComponent* EnemyMove = Enemy->GetCharacterMovement())
			{
				TestEqual(TEXT("Victim restored to MOVE_Walking after blocked backstab cancel"), EnemyMove->MovementMode, MOVE_Walking);
			}

			BlockingActor->Destroy();
		}

		PlayerASC->ClearAbility(BlockExecHandle);
		ResetWeaponExecutionMontages(Player);
	}

	// =========================================================================
	// 15. Preserving Other Abilities' Motion Warp Targets Across EndAbility
	// =========================================================================
	{
		UAnimMontage* SyntheticMontage = NewObject<UAnimMontage>();
		TSubclassOf<UGameplayEffect> DamageGEClass = UTestProjectileDamageGE::StaticClass();

		Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
		Player->SetActorRotation(FRotator::ZeroRotator);
		Enemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
		Enemy->SetActorRotation(FRotator(0.0f, 0.0f, 0.0f));

		const FName RegularWarpTargetName(TEXT("MeleeContact"));
		Player->SetMeleeMotionWarpTarget(RegularWarpTargetName, FTransform(FRotator::ZeroRotator, FVector(500.0f, 0.0f, 0.0f)));
		TestTrue(TEXT("Regular motion warp target registered before backstab execution"),
			Player->HasTestMeleeMotionWarpTarget(RegularWarpTargetName));

		auto [ExecHandle, ExecAbility] = GrantAndConfigureBackstabAbility(Player, SyntheticMontage, DamageGEClass, 50.0f, 250.0f, 60.0f);

		Player->SetTestLockedTarget(Enemy);
		Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));

		TestTrue(TEXT("Backstab execution activated successfully"), ExecAbility->IsActive());

		// End ability
		ExecAbility->TestEndAbility();
		TestFalse(TEXT("Backstab execution ability ended"), ExecAbility->IsActive());

		// Invariant: Regular attack's Motion Warp target must NOT have been cleared!
		TestTrue(TEXT("Regular motion warp target preserved after backstab EndAbility"),
			Player->HasTestMeleeMotionWarpTarget(RegularWarpTargetName));

		// Cleanup
		Player->ClearMeleeMotionWarpTargets();
		PlayerASC->ClearAbility(ExecHandle);
		ResetWeaponExecutionMontages(Player);
	}

	// =========================================================================
	// 16. Target Forward Snapshot Invariant During Activation
	// =========================================================================
	{
		UAnimMontage* SyntheticMontage = NewObject<UAnimMontage>();
		TSubclassOf<UGameplayEffect> DamageGEClass = UTestProjectileDamageGE::StaticClass();

		Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
		Player->SetActorRotation(FRotator::ZeroRotator);
		Enemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
		Enemy->SetActorRotation(FRotator(0.0f, 0.0f, 0.0f)); // Enemy forward is (+1, 0, 0)

		auto [ExecHandle, ExecAbility] = GrantAndConfigureBackstabAbility(Player, SyntheticMontage, DamageGEClass, 50.0f, 250.0f, 60.0f);

		Player->SetTestLockedTarget(Enemy);
		Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));

		TestTrue(TEXT("Backstab activated"), ExecAbility->IsActive());

		// Expected location is based on activation forward (+1, 0, 0) -> (-40, 0, 0)
		const FVector SnapLocationAtActivation = Player->GetActorLocation();
		TestTrue(TEXT("Player snapped behind original target orientation"),
			FVector::Dist(SnapLocationAtActivation, FVector(-40.0f, 0.0f, 0.0f)) <= 1.0f);

		// Now enemy turns 90 degrees after activation
		Enemy->SetActorRotation(FRotator(0.0f, 90.0f, 0.0f));

		// Player location must remain anchored to activation snapshot, not updated dynamically
		TestEqual(TEXT("Player location does not track post-activation target rotation"),
			Player->GetActorLocation(), SnapLocationAtActivation);

		ExecAbility->TestEndAbility();
		PlayerASC->ClearAbility(ExecHandle);
		ResetWeaponExecutionMontages(Player);
	}

	// =========================================================================
	// 17. Weapon Execution Montage Gate & Commit Failure (Fail-Closed Gate)
	// =========================================================================
	{
		// 17.1 Missing BackstabExecutionMontage: Input does not activate Backstab, does not establish lock
		{
			ResetWeaponExecutionMontages(Player);
			Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
			Enemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
			Enemy->SetActorRotation(FRotator(0.0f, 0.0f, 0.0f)); // Enemy facing away (Player is behind Enemy)
			Player->SetTestLockedTarget(Enemy);

			// Weapon has NO BackstabExecutionMontage (only FrontExecutionMontage configured)
			UAnimMontage* FrontOnlyMontage = NewObject<UAnimMontage>();
			if (UWeaponEquipmentComponent* EquipComp = Player->FindComponentByClass<UWeaponEquipmentComponent>())
			{
				if (UMeleeWeaponDefinition* WeaponDef = EquipComp->GetEquippedMainHandMelee())
				{
					WeaponDef->FrontExecutionMontage = FrontOnlyMontage;
				}
			}

			FGameplayAbilitySpec Spec(UPlayerBackstabExecutionAbility::StaticClass(), 1, INDEX_NONE, Player);
			const FGameplayAbilitySpecHandle ExecHandle = PlayerASC->GiveAbility(Spec);

			// Trigger PrimaryAttack input intent
			Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));

			FGameplayAbilitySpec* FoundSpec = PlayerASC->FindAbilitySpecFromHandle(ExecHandle);
			UPlayerBackstabExecutionAbility* ExecAbility = FoundSpec ? Cast<UPlayerBackstabExecutionAbility>(FoundSpec->GetPrimaryInstance()) : nullptr;
			if (ExecAbility)
			{
				TestFalse(TEXT("Backstab execution does not activate when BackstabExecutionMontage is missing"), ExecAbility->IsActive());
				TestNull(TEXT("No target reservation when BackstabExecutionMontage is missing"), ExecAbility->GetTestReservedTarget());
			}

			if (UCharacterMovementComponent* EnemyMove = Enemy->GetCharacterMovement())
			{
				TestTrue(TEXT("Enemy movement mode is NOT MOVE_None when backstab execution rejected"), EnemyMove->MovementMode != MOVE_None);
			}

			PlayerASC->ClearAbility(ExecHandle);
			ResetWeaponExecutionMontages(Player);
		}

		// 17.2 Force Commit Failure (Fail-Closed Gate): Aborts immediately without establishing reservation or victim lock
		{
			UAnimMontage* SyntheticMontage = NewObject<UAnimMontage>();
			TSubclassOf<UGameplayEffect> DamageGEClass = UTestProjectileDamageGE::StaticClass();

			auto [ExecHandle, ExecAbility] = GrantAndConfigureBackstabAbility(Player, SyntheticMontage, DamageGEClass, 50.0f, 250.0f, 60.0f);

			Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
			Enemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
			Enemy->SetActorRotation(FRotator(0.0f, 0.0f, 0.0f)); // Enemy facing away
			Player->SetTestLockedTarget(Enemy);

			ExecAbility->SetTestForceCommitAbilityFailure(true);

			Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));

			TestFalse(TEXT("Ability aborted when Commit fails"), ExecAbility->IsActive());
			TestNull(TEXT("No reservation created when Commit fails"), ExecAbility->GetTestReservedTarget());
			TestNull(TEXT("Active montage snapshot cleared on commit failure abort"), ExecAbility->GetTestActiveExecutionMontage());
			TestNull(TEXT("Active weapon snapshot cleared on commit failure abort"), ExecAbility->GetTestActiveExecutionWeaponDefinition());
			TestFalse(TEXT("Active distance snapshot flag cleared on commit failure abort"), ExecAbility->HasTestActiveExecutionDistanceSnapshot());

			if (UCharacterMovementComponent* EnemyMove = Enemy->GetCharacterMovement())
			{
				TestTrue(TEXT("Enemy movement mode is NOT MOVE_None on commit failure"), EnemyMove->MovementMode != MOVE_None);
			}

			ExecAbility->SetTestForceCommitAbilityFailure(false);
			PlayerASC->ClearAbility(ExecHandle);
			ResetWeaponExecutionMontages(Player);
		}

		// 17.3 Invalid Weapon Execution Distance Range (Fail-Closed Gate): Rejects activation before reservation or lock
		{
			UAnimMontage* SyntheticMontage = NewObject<UAnimMontage>();
			TSubclassOf<UGameplayEffect> DamageGEClass = UTestProjectileDamageGE::StaticClass();

			auto [ExecHandle, ExecAbility] = GrantAndConfigureBackstabAbility(Player, SyntheticMontage, DamageGEClass, 0.0f, 250.0f, 60.0f);

			// Corrupt weapon distance range: Max > 250cm violates project hard cap
			if (UWeaponEquipmentComponent* EquipComp = Player->FindComponentByClass<UWeaponEquipmentComponent>())
			{
				if (UMeleeWeaponDefinition* WeaponDef = EquipComp->GetEquippedMainHandMelee())
				{
					WeaponDef->MaxExecutionDistance = 300.0f;
				}
			}

			Player->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
			Enemy->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
			Enemy->SetActorRotation(FRotator(0.0f, 0.0f, 0.0f)); // Enemy facing away
			Player->SetTestLockedTarget(Enemy);

			Player->TriggerTestRequestAbilityForInputIntent(FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false));

			TestFalse(TEXT("Backstab execution does not activate when weapon distance range is invalid"), ExecAbility->IsActive());
			TestNull(TEXT("No target reservation when weapon distance range is invalid"), ExecAbility->GetTestReservedTarget());
			TestFalse(TEXT("No active distance snapshot when weapon distance range is invalid"), ExecAbility->HasTestActiveExecutionDistanceSnapshot());

			if (UCharacterMovementComponent* EnemyMove = Enemy->GetCharacterMovement())
			{
				TestTrue(TEXT("Enemy movement mode is NOT MOVE_None when distance range invalid"), EnemyMove->MovementMode != MOVE_None);
			}

			PlayerASC->ClearAbility(ExecHandle);
			ResetWeaponExecutionMontages(Player);
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
