#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/Abilities/PlayerGuardAbility.h"
#include "AbilitySystem/Abilities/PlayerParryAbility.h"
#include "AbilitySystem/CharacterAttributeSet.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Character/Player/PlayerCharacter.h"
#include "Combat/CombatImpactEffectContext.h"
#include "Combat/Equipment/ProjectileDefinition.h"
#include "Combat/Projectile/CombatProjectile.h"
#include "Combat/Projectile/CombatProjectileHitResolver.h"
#include "Combat/Reaction/HitReactionImpactResolver.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/PolyQuestPlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "Kismet/GameplayStatics.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Tests/CombatAutomationFixture.h"
#include "Tests/TestGuardStaminaCostGE.h"
#include "Tests/TestProjectileDamageGE.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FProjectileImpactGeometryAutomationTest,
	"PolyQuest.Projectile.ImpactGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	struct FTestWorldScopeCleanup
	{
		UWorld* World = nullptr;
		~FTestWorldScopeCleanup()
		{
			if (World)
			{
				UGameplayStatics::SetGlobalTimeDilation(World, 1.0f);
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
		}
	};

	void TickTestWorld(UWorld* World, const float DeltaSeconds)
	{
		if (World)
		{
			World->Tick(ELevelTick::LEVELTICK_All, DeltaSeconds);
			++GFrameCounter;
		}
	}
}

bool FProjectileImpactGeometryAutomationTest::RunTest(const FString& Parameters)
{
	if (!TestNotNull(TEXT("GEngine is available"), GEngine))
	{
		return false;
	}

	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("ProjectileImpactGeometryTestWorld"));
	WorldContext.SetCurrentWorld(World);
	FTestWorldScopeCleanup WorldCleanup{ World };

	if (!TestNotNull(TEXT("Test World created"), World))
	{
		return false;
	}

	FURL WorldURL;
	World->InitializeActorsForPlay(WorldURL);
	World->BeginPlay();
	World->SetBegunPlay(true);

	// Tags
	const FGameplayTag TagTeamPlayer = FGameplayTag::RequestGameplayTag(FName(TEXT("Team.Player")), false);
	const FGameplayTag TagTeamEnemy = FGameplayTag::RequestGameplayTag(FName(TEXT("Team.Enemy")), false);
	const FGameplayTag TagDead = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
	const FGameplayTag TagInvulnerable = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Invulnerable")), false);
	const FGameplayTag TagGuardAbility = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Defense.Guard")), false);
	const FGameplayTag TagGuardingState = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Guarding")), false);
	const FGameplayTag TagParryAbility = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Defense.Parry")), false);
	const FGameplayTag TagParryingState = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Parrying")), false);

	TestTrue(TEXT("Tag Team.Player is valid"), TagTeamPlayer.IsValid());
	TestTrue(TEXT("Tag Team.Enemy is valid"), TagTeamEnemy.IsValid());
	TestTrue(TEXT("Tag State.Status.Dead is valid"), TagDead.IsValid());
	TestTrue(TEXT("Tag State.Status.Invulnerable is valid"), TagInvulnerable.IsValid());
	TestTrue(TEXT("Tag Ability.Defense.Guard is valid"), TagGuardAbility.IsValid());
	TestTrue(TEXT("Tag State.Action.Guarding is valid"), TagGuardingState.IsValid());

	// Spawn Player and Enemy fixtures
	APlayerCharacter* Player = FCombatAutomationFixture::SpawnPlayer(World, FTransform(FRotator::ZeroRotator, FVector::ZeroVector));
	AEnemyCharacter* Enemy = FCombatAutomationFixture::SpawnPassiveEnemy(World, FTransform(FRotator(0.0f, 180.0f, 0.0f), FVector(200.0f, 0.0f, 0.0f)));

	if (!TestNotNull(TEXT("Player spawned"), Player) || !TestNotNull(TEXT("Enemy spawned"), Enemy))
	{
		return false;
	}

	// Assert native constructor team configuration
	const FGameplayTag ActualPlayerTeam = ICombatTeamAgent::Execute_GetCombatTeamTag(Player);
	const FGameplayTag ActualEnemyTeam = ICombatTeamAgent::Execute_GetCombatTeamTag(Enemy);
	TestEqual(TEXT("Player native team is Team.Player"), ActualPlayerTeam, TagTeamPlayer);
	TestEqual(TEXT("Enemy native team is Team.Enemy"), ActualEnemyTeam, TagTeamEnemy);

	UAbilitySystemComponent* PlayerASC = Player->GetAbilitySystemComponent();
	UAbilitySystemComponent* EnemyASC = Enemy->GetAbilitySystemComponent();
	if (!TestNotNull(TEXT("Player ASC available"), PlayerASC) || !TestNotNull(TEXT("Enemy ASC available"), EnemyASC))
	{
		return false;
	}

	// Initialize baseline attributes
	PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxHealthAttribute(), 1000.0f);
	PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 1000.0f);
	PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxStaminaAttribute(), 100.0f);
	PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 100.0f);

	EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxHealthAttribute(), 1000.0f);
	EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 1000.0f);

	// Setup transient Guard Ability on Player
	FGameplayAbilitySpec GuardSpec(UPlayerGuardAbility::StaticClass(), 1, INDEX_NONE, Player);
	GuardSpec.DynamicAbilityTags.AddTag(TagGuardAbility);
	const FGameplayAbilitySpecHandle GuardSpecHandle = PlayerASC->GiveAbility(GuardSpec);
	TestTrue(TEXT("Guard spec handle is valid"), GuardSpecHandle.IsValid());

	FGameplayAbilitySpec* FoundGuardSpec = PlayerASC->FindAbilitySpecFromHandle(GuardSpecHandle);
	UPlayerGuardAbility* GuardAbility = FoundGuardSpec ? Cast<UPlayerGuardAbility>(FoundGuardSpec->GetPrimaryInstance()) : nullptr;
	if (!TestNotNull(TEXT("Primary Guard Ability instance available"), GuardAbility))
	{
		return false;
	}

	GuardAbility->SetTestGuardStaminaCostGameplayEffectClass(UTestGuardStaminaCostGE::StaticClass());
	GuardAbility->SetTestGuardActive(true);
	GuardAbility->SetTestBypassAudioPlayback(true);
	Player->SetTestBypassReceivedHitAudioPlayback(true);
	PlayerASC->AddLooseGameplayTag(TagGuardingState);

	// Setup transient Parry Ability on Player for Parry rejection tests
	FGameplayAbilitySpec ParrySpec(UPlayerParryAbility::StaticClass(), 1, INDEX_NONE, Player);
	ParrySpec.DynamicAbilityTags.AddTag(TagParryAbility);
	const FGameplayAbilitySpecHandle ParrySpecHandle = PlayerASC->GiveAbility(ParrySpec);
	FGameplayAbilitySpec* FoundParrySpec = PlayerASC->FindAbilitySpecFromHandle(ParrySpecHandle);
	UPlayerParryAbility* ParryAbility = FoundParrySpec ? Cast<UPlayerParryAbility>(FoundParrySpec->GetPrimaryInstance()) : nullptr;
	if (ParryAbility)
	{
		ParryAbility->SetTestBypassAudioPlayback(true);
	}

	// =========================================================================
	// SECTION 1: 退化几何与归一化 (Degenerate Geometry & Fallback Suppression)
	// =========================================================================
	{
		// Player faces +X (ZeroRotator), Enemy is at +X (200, 0, 0)
		Player->SetActorRotation(FRotator::ZeroRotator);
		PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 1000.0f);
		PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 100.0f);

		FHitResult StandardHit(Player, nullptr, FVector(100.0f, 0.0f, 0.0f), FVector(1.0f, 0.0f, 0.0f));

		// 1.1 Valid planar incoming direction from +X -> consumed by Guard
		FCombatProjectileHitRequest ValidRequest;
		ValidRequest.SourceActor = Enemy;
		ValidRequest.SourceAbilitySystemComponent = EnemyASC;
		ValidRequest.DamageGameplayEffectClass = UTestProjectileDamageGE::StaticClass();
		ValidRequest.GuardStaminaDamage = 20.0f;
		ValidRequest.HitResult = StandardHit;
		ValidRequest.WorldIncomingDirection = FVector(1.0f, 0.0f, 0.0f);

		const bool bValidGuarded = FCombatProjectileHitResolver::TryResolveHit(ValidRequest);
		TestTrue(TEXT("1.1 Valid planar incoming direction is consumed by Guard"), bValidGuarded);
		TestEqual(TEXT("1.1 Player stamina reduced by 20"), PlayerASC->GetNumericAttribute(UCharacterAttributeSet::GetStaminaAttribute()), 80.0f);
		TestEqual(TEXT("1.1 Player health undamaged"), PlayerASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute()), 1000.0f);

		// 1.2 Explicit zero direction: Guard rejects, damage applies normally, NO attacker position fallback
		PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 100.0f);
		FCombatProjectileHitRequest ZeroDirRequest = ValidRequest;
		ZeroDirRequest.WorldIncomingDirection = FVector::ZeroVector;

		const bool bZeroResolved = FCombatProjectileHitResolver::TryResolveHit(ZeroDirRequest);
		TestTrue(TEXT("1.2 Zero direction hit resolves through damage"), bZeroResolved);
		TestEqual(TEXT("1.2 Zero direction hit bypasses Guard (stamina untouched)"), PlayerASC->GetNumericAttribute(UCharacterAttributeSet::GetStaminaAttribute()), 100.0f);
		TestEqual(TEXT("1.2 Zero direction hit applies 25 damage"), PlayerASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute()), 975.0f);

		// 1.3 Pure vertical direction: Guard rejects, damage applies normally
		PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 1000.0f);
		FCombatProjectileHitRequest VerticalRequest = ValidRequest;
		VerticalRequest.WorldIncomingDirection = FVector(0.0f, 0.0f, -1.0f);

		const bool bVertResolved = FCombatProjectileHitResolver::TryResolveHit(VerticalRequest);
		TestTrue(TEXT("1.3 Pure vertical direction resolves through damage"), bVertResolved);
		TestEqual(TEXT("1.3 Pure vertical direction bypasses Guard"), PlayerASC->GetNumericAttribute(UCharacterAttributeSet::GetStaminaAttribute()), 100.0f);
		TestEqual(TEXT("1.3 Pure vertical direction applies damage"), PlayerASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute()), 975.0f);

		// 1.4 NaN / Inf direction: Guard rejects, damage applies normally
		PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 1000.0f);
		FCombatProjectileHitRequest NanRequest = ValidRequest;
		NanRequest.WorldIncomingDirection = FVector(NAN, 0.0f, 0.0f);

		const bool bNanResolved = FCombatProjectileHitResolver::TryResolveHit(NanRequest);
		TestTrue(TEXT("1.4 NaN direction resolves through damage"), bNanResolved);
		TestEqual(TEXT("1.4 NaN direction bypasses Guard"), PlayerASC->GetNumericAttribute(UCharacterAttributeSet::GetStaminaAttribute()), 100.0f);
		TestEqual(TEXT("1.4 NaN direction applies damage"), PlayerASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute()), 975.0f);
	}

	// =========================================================================
	// SECTION 2: 目标朝向与防御弧覆盖 (Target Yaw & Guard Arc Coverage)
	// =========================================================================
	{
		// Projectile incoming direction is from +X toward origin: (1, 0, 0)
		FHitResult Hit(Player, nullptr, FVector(50.0f, 0.0f, 0.0f), FVector(1.0f, 0.0f, 0.0f));
		FCombatProjectileHitRequest ArcRequest;
		ArcRequest.SourceActor = Enemy;
		ArcRequest.SourceAbilitySystemComponent = EnemyASC;
		ArcRequest.DamageGameplayEffectClass = UTestProjectileDamageGE::StaticClass();
		ArcRequest.GuardStaminaDamage = 15.0f;
		ArcRequest.HitResult = Hit;
		ArcRequest.WorldIncomingDirection = FVector(1.0f, 0.0f, 0.0f);

		// 2.1 Target facing attacker (+X, Yaw = 0): inside arc -> Guard consumes
		Player->SetActorRotation(FRotator::ZeroRotator);
		PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 1000.0f);
		PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 100.0f);

		TestTrue(TEXT("2.1 Facing attacker: Guard consumes hit"), FCombatProjectileHitResolver::TryResolveHit(ArcRequest));
		TestEqual(TEXT("2.1 Stamina reduced by 15"), PlayerASC->GetNumericAttribute(UCharacterAttributeSet::GetStaminaAttribute()), 85.0f);
		TestEqual(TEXT("2.1 Health undamaged"), PlayerASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute()), 1000.0f);

		// 2.2 Target turned away (Yaw = 180): outside arc -> Guard fails, damage applied
		Player->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f));
		PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 100.0f);

		TestTrue(TEXT("2.2 Turned away: Damage resolves"), FCombatProjectileHitResolver::TryResolveHit(ArcRequest));
		TestEqual(TEXT("2.2 Stamina untouched on rear hit"), PlayerASC->GetNumericAttribute(UCharacterAttributeSet::GetStaminaAttribute()), 100.0f);
		TestEqual(TEXT("2.2 Health damaged on rear hit"), PlayerASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute()), 975.0f);

		// 2.3 Arc boundary tests with GuardHalfArcDegrees = 60.0f
		// 2.3a Just inside boundary: Yaw = 59.0f -> Dot = Cos(59) >= Cos(60) -> Guard succeeds
		Player->SetActorRotation(FRotator(0.0f, 59.0f, 0.0f));
		PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 1000.0f);
		PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 100.0f);
		TestTrue(TEXT("2.3a Yaw 59 deg is inside guard arc"), FCombatProjectileHitResolver::TryResolveHit(ArcRequest));
		TestEqual(TEXT("2.3a Stamina reduced inside arc"), PlayerASC->GetNumericAttribute(UCharacterAttributeSet::GetStaminaAttribute()), 85.0f);

		// 2.3b Exact boundary: Yaw = 60.0f -> Dot = Cos(60) >= Cos(60) (inclusive) -> Guard succeeds
		Player->SetActorRotation(FRotator(0.0f, 60.0f, 0.0f));
		PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 100.0f);
		TestTrue(TEXT("2.3b Yaw 60 deg is on inclusive guard boundary"), FCombatProjectileHitResolver::TryResolveHit(ArcRequest));
		TestEqual(TEXT("2.3b Stamina reduced on boundary"), PlayerASC->GetNumericAttribute(UCharacterAttributeSet::GetStaminaAttribute()), 85.0f);

		// 2.3c Just outside boundary: Yaw = 61.0f -> Dot = Cos(61) < Cos(60) -> Guard fails
		Player->SetActorRotation(FRotator(0.0f, 61.0f, 0.0f));
		PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 100.0f);
		TestTrue(TEXT("2.3c Yaw 61 deg resolves as damage outside arc"), FCombatProjectileHitResolver::TryResolveHit(ArcRequest));
		TestEqual(TEXT("2.3c Stamina untouched outside arc"), PlayerASC->GetNumericAttribute(UCharacterAttributeSet::GetStaminaAttribute()), 100.0f);
		TestEqual(TEXT("2.3c Health damaged outside arc"), PlayerASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute()), 975.0f);

		// Reset Player rotation to ZeroRotator
		Player->SetActorRotation(FRotator::ZeroRotator);
	}

	// =========================================================================
	// SECTION 3: 射手横移与转弯追踪 (Attacker Movement & Turning Homing)
	// =========================================================================
	{
		Player->SetActorRotation(FRotator::ZeroRotator);
		PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 1000.0f);
		PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 100.0f);

		// 3.1 Attacker moves behind player (-200, 0, 0) during flight, but incoming direction is still (+X)
		Enemy->SetActorLocation(FVector(-200.0f, 0.0f, 0.0f));

		FHitResult Hit(Player, nullptr, FVector(50.0f, 0.0f, 0.0f), FVector(1.0f, 0.0f, 0.0f));
		FCombatProjectileHitRequest FlankedRequest;
		FlankedRequest.SourceActor = Enemy;
		FlankedRequest.SourceAbilitySystemComponent = EnemyASC;
		FlankedRequest.DamageGameplayEffectClass = UTestProjectileDamageGE::StaticClass();
		FlankedRequest.GuardStaminaDamage = 25.0f;
		FlankedRequest.HitResult = Hit;
		FlankedRequest.WorldIncomingDirection = FVector(1.0f, 0.0f, 0.0f); // Incoming from +X

		const bool bFlankedGuarded = FCombatProjectileHitResolver::TryResolveHit(FlankedRequest);
		TestTrue(TEXT("3.1 Attacker behind player does not affect guard when incoming direction is from front"), bFlankedGuarded);
		TestEqual(TEXT("3.1 Stamina reduced by 25"), PlayerASC->GetNumericAttribute(UCharacterAttributeSet::GetStaminaAttribute()), 75.0f);
		TestEqual(TEXT("3.1 Health undamaged"), PlayerASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute()), 1000.0f);

		// Reset Enemy location
		Enemy->SetActorLocation(FVector(200.0f, 0.0f, 0.0f));

		// 3.2 Homing projectile turned: current velocity is from +Y (0, -3000, 0), incoming dir is (0, 1, 0)
		// Attacker is at +X (200, 0, 0), ImpactNormal is (1, 0, 0)
		// Player faces +X -> incoming from +Y is 90 degrees off (outside 60-deg guard arc)
		PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 100.0f);
		FCombatProjectileHitRequest TurnedRequest = FlankedRequest;
		TurnedRequest.WorldIncomingDirection = FVector(0.0f, 1.0f, 0.0f); // 90 deg off from Player facing

		TestTrue(TEXT("3.2 Turned projectile resolves as damage"), FCombatProjectileHitResolver::TryResolveHit(TurnedRequest));
		TestEqual(TEXT("3.2 Turned projectile bypasses front guard (stamina untouched)"), PlayerASC->GetNumericAttribute(UCharacterAttributeSet::GetStaminaAttribute()), 100.0f);
		TestEqual(TEXT("3.2 Turned projectile inflicts damage"), PlayerASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute()), 975.0f);
	}

	// =========================================================================
	// SECTION 4: 防御策略与体力消耗 (Guard Policy, Stamina & Parry Immunity)
	// =========================================================================
	{
		Player->SetActorRotation(FRotator::ZeroRotator);
		FHitResult Hit(Player, nullptr, FVector(50.0f, 0.0f, 0.0f), FVector(1.0f, 0.0f, 0.0f));
		FCombatProjectileHitRequest DefenseRequest;
		DefenseRequest.SourceActor = Enemy;
		DefenseRequest.SourceAbilitySystemComponent = EnemyASC;
		DefenseRequest.DamageGameplayEffectClass = UTestProjectileDamageGE::StaticClass();
		DefenseRequest.HitResult = Hit;
		DefenseRequest.WorldIncomingDirection = FVector(1.0f, 0.0f, 0.0f);

		// 4.1 Exact stamina exhaustion: 30 stamina left, 30 damage -> absorbed, stamina 0, health untouched
		PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 1000.0f);
		PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 30.0f);
		DefenseRequest.GuardStaminaDamage = 30.0f;

		TestTrue(TEXT("4.1 Exact stamina exhaustion hit is absorbed"), FCombatProjectileHitResolver::TryResolveHit(DefenseRequest));
		TestEqual(TEXT("4.1 Stamina reached 0"), PlayerASC->GetNumericAttribute(UCharacterAttributeSet::GetStaminaAttribute()), 0.0f);
		TestEqual(TEXT("4.1 Health undamaged on exact exhaustion"), PlayerASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute()), 1000.0f);

		// Re-enable guard for next test
		GuardAbility->SetTestGuardActive(true);
		if (!PlayerASC->HasMatchingGameplayTag(TagGuardingState))
		{
			PlayerASC->AddLooseGameplayTag(TagGuardingState);
		}

		// 4.2 Over-exhaustion: 10 stamina left, 50 damage -> absorbed, stamina 0, health untouched
		PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 10.0f);
		DefenseRequest.GuardStaminaDamage = 50.0f;

		TestTrue(TEXT("4.2 Over-exhaustion hit is absorbed"), FCombatProjectileHitResolver::TryResolveHit(DefenseRequest));
		TestEqual(TEXT("4.2 Stamina clamped to 0"), PlayerASC->GetNumericAttribute(UCharacterAttributeSet::GetStaminaAttribute()), 0.0f);
		TestEqual(TEXT("4.2 Health undamaged on over-exhaustion"), PlayerASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute()), 1000.0f);

		// 4.3 Initial zero stamina: Guard rejected, full damage applied
		GuardAbility->SetTestGuardActive(true);
		PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 0.0f);
		DefenseRequest.GuardStaminaDamage = 20.0f;

		TestTrue(TEXT("4.3 Initial zero stamina resolves through damage"), FCombatProjectileHitResolver::TryResolveHit(DefenseRequest));
		TestEqual(TEXT("4.3 Health reduced by 25 on zero stamina"), PlayerASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute()), 975.0f);

		// 4.4 Parry active does NOT deflect projectile
		PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 1000.0f);
		PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 100.0f);
		GuardAbility->SetTestGuardActive(false);
		PlayerASC->RemoveLooseGameplayTag(TagGuardingState);

		if (ParryAbility)
		{
			ParryAbility->TestSetParryWindowOpen(true);
			PlayerASC->AddLooseGameplayTag(TagParryingState);

			TestTrue(TEXT("4.4 Projectile on parrying player resolves via damage"), FCombatProjectileHitResolver::TryResolveHit(DefenseRequest));
			TestEqual(TEXT("4.4 Projectile does NOT trigger parry success feedback"), ParryAbility->GetTestParrySuccessFeedbackCount(), 0);
			TestEqual(TEXT("4.4 Player receives damage despite parry active"), PlayerASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute()), 975.0f);

			ParryAbility->TestSetParryWindowOpen(false);
			PlayerASC->RemoveLooseGameplayTag(TagParryingState);
		}

		// Restore Guard
		GuardAbility->SetTestGuardActive(true);
		PlayerASC->AddLooseGameplayTag(TagGuardingState);
	}

	// =========================================================================
	// SECTION 5: 资格校验与拒绝 (Eligibility Rejection)
	// =========================================================================
	{
		PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 1000.0f);
		PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 100.0f);

		FHitResult Hit(Player, nullptr, FVector(50.0f, 0.0f, 0.0f), FVector(1.0f, 0.0f, 0.0f));
		FCombatProjectileHitRequest RejectRequest;
		RejectRequest.SourceActor = Enemy;
		RejectRequest.SourceAbilitySystemComponent = EnemyASC;
		RejectRequest.DamageGameplayEffectClass = UTestProjectileDamageGE::StaticClass();
		RejectRequest.GuardStaminaDamage = 20.0f;
		RejectRequest.HitResult = Hit;
		RejectRequest.WorldIncomingDirection = FVector(1.0f, 0.0f, 0.0f);

		// 5.1 Missing SourceActor
		FCombatProjectileHitRequest MissingSourceRequest = RejectRequest;
		MissingSourceRequest.SourceActor = nullptr;
		TestFalse(TEXT("5.1 Rejects null SourceActor"), FCombatProjectileHitResolver::TryResolveHit(MissingSourceRequest));

		// 5.2 Death teardown is terminal: never reuse this source in later live-hit tests.
		AEnemyCharacter* DeadSource = FCombatAutomationFixture::SpawnPassiveEnemy(
			World, FTransform(FRotator::ZeroRotator, FVector(2000.0f, 0.0f, 0.0f)));
		if (TestNotNull(TEXT("5.2 Dedicated dead source spawned"), DeadSource))
		{
			UAbilitySystemComponent* DeadSourceASC = DeadSource->GetAbilitySystemComponent();
			if (TestNotNull(TEXT("5.2 Dedicated dead source ASC exists"), DeadSourceASC))
			{
				FCombatProjectileHitRequest DeadSourceRequest = RejectRequest;
				DeadSourceRequest.SourceActor = DeadSource;
				DeadSourceRequest.SourceAbilitySystemComponent = DeadSourceASC;
				DeadSourceASC->AddLooseGameplayTag(TagDead);
				TestFalse(TEXT("5.2 Rejects dead source"), FCombatProjectileHitResolver::TryResolveHit(DeadSourceRequest));
			}
			DeadSource->Destroy();
		}

		// 5.3 Dead Target
		PlayerASC->AddLooseGameplayTag(TagDead);
		TestFalse(TEXT("5.3 Rejects dead target"), FCombatProjectileHitResolver::TryResolveHit(RejectRequest));
		PlayerASC->RemoveLooseGameplayTag(TagDead);

		// 5.4 Invulnerable Target
		PlayerASC->AddLooseGameplayTag(TagInvulnerable);
		TestFalse(TEXT("5.4 Rejects invulnerable target"), FCombatProjectileHitResolver::TryResolveHit(RejectRequest));
		PlayerASC->RemoveLooseGameplayTag(TagInvulnerable);

		// 5.5 Same Team (Friendly)
		AEnemyCharacter* FriendlyEnemy = FCombatAutomationFixture::SpawnPassiveEnemy(World, FTransform(FRotator::ZeroRotator, FVector(50.0f, 0.0f, 0.0f)));
		if (FriendlyEnemy)
		{
			FCombatProjectileHitRequest FriendlyRequest = RejectRequest;
			FriendlyRequest.HitResult = FHitResult(FriendlyEnemy, nullptr, FriendlyEnemy->GetActorLocation(), FVector::UpVector);
			TestFalse(TEXT("5.5 Rejects friendly same-team target"), FCombatProjectileHitResolver::TryResolveHit(FriendlyRequest));
			FriendlyEnemy->Destroy();
		}
	}

	// =========================================================================
	// SECTION 6: Context 多态、复制与序列化 (Context Polymorphism & Lifecycle)
	// =========================================================================
	{
		// 6.1 Correct derived type and snapshot value
		FCombatImpactEffectContext* CustomContext = new FCombatImpactEffectContext();
		CustomContext->SetWorldIncomingDirection(FVector(1.0f, 0.0f, 0.0f));
		CustomContext->AddInstigator(Enemy, Enemy);
		FHitResult TestHit(Player, nullptr, FVector(100.0f, 50.0f, 25.0f), FVector(1.0f, 0.0f, 0.0f));
		CustomContext->AddHitResult(TestHit, true);

		FGameplayEffectContextHandle Handle(CustomContext);
		TestTrue(TEXT("6.1 ContextHandle is valid"), Handle.IsValid());
		TestNotNull(TEXT("6.1 Raw pointer is non-null"), Handle.Get());
		TestTrue(TEXT("6.1 ScriptStruct is child of FCombatImpactEffectContext"),
			Handle.Get()->GetScriptStruct()->IsChildOf(FCombatImpactEffectContext::StaticStruct()));

		const FCombatImpactEffectContext* Casted = static_cast<const FCombatImpactEffectContext*>(Handle.Get());
		TestEqual(TEXT("6.1 Snapshot incoming direction matches"), Casted->GetWorldIncomingDirection(), FVector(1.0f, 0.0f, 0.0f));

		// 6.2 Duplicate behavior and deep copy of HitResult
		FGameplayEffectContextHandle DupHandle = Handle.Duplicate();
		TestTrue(TEXT("6.2 Duplicate handle is valid"), DupHandle.IsValid());
		TestTrue(TEXT("6.2 Duplicate ScriptStruct is FCombatImpactEffectContext"),
			DupHandle.Get()->GetScriptStruct()->IsChildOf(FCombatImpactEffectContext::StaticStruct()));

		const FCombatImpactEffectContext* DupCasted = static_cast<const FCombatImpactEffectContext*>(DupHandle.Get());
		TestEqual(TEXT("6.2 Duplicated direction preserved"), DupCasted->GetWorldIncomingDirection(), FVector(1.0f, 0.0f, 0.0f));
		TestNotNull(TEXT("6.2 Duplicated HitResult exists"), DupHandle.GetHitResult());
		TestEqual(TEXT("6.2 Duplicated HitResult ImpactPoint preserved"), DupHandle.GetHitResult()->ImpactPoint, FVector(100.0f, 50.0f, 25.0f));

		// Verify deep copy: pointers are distinct and mutating original does not mutate duplicate
		TestTrue(TEXT("6.2 Duplicate HitResult is distinct pointer (deep copy)"),
			Handle.GetHitResult() != DupHandle.GetHitResult());
		FHitResult MutatedHit = *Handle.GetHitResult();
		MutatedHit.ImpactPoint = FVector(999.0f, 999.0f, 999.0f);
		Handle.AddHitResult(MutatedHit, true);
		TestEqual(TEXT("6.2 Original HitResult updated"), Handle.GetHitResult()->ImpactPoint, FVector(999.0f, 999.0f, 999.0f));
		TestEqual(TEXT("6.2 Duplicate HitResult is independent (deep copy)"), DupHandle.GetHitResult()->ImpactPoint, FVector(100.0f, 50.0f, 25.0f));

		// 6.3 Native NetSerialize roundtrip
		FCombatImpactEffectContext NetContext;
		NetContext.SetWorldIncomingDirection(FVector(0.0f, 1.0f, 0.0f));

		TArray<uint8> SerialBuffer;
		FMemoryWriter Writer(SerialBuffer);
		bool bWriteSuccess = false;
		NetContext.NetSerialize(Writer, nullptr, bWriteSuccess);
		TestTrue(TEXT("6.3 NetSerialize write returned true"), bWriteSuccess);
		TestTrue(TEXT("6.3 Buffer has data"), SerialBuffer.Num() > 0);

		FMemoryReader Reader(SerialBuffer);
		FCombatImpactEffectContext DeserializedContext;
		bool bReadSuccess = false;
		DeserializedContext.NetSerialize(Reader, nullptr, bReadSuccess);
		TestTrue(TEXT("6.3 NetSerialize read returned true"), bReadSuccess);
		TestEqual(TEXT("6.3 Deserialized direction matches original"), DeserializedContext.GetWorldIncomingDirection(), FVector(0.0f, 1.0f, 0.0f));
	}

	// =========================================================================
	// SECTION 7: 受击与死亡方向解析 (Reaction & Death Direction)
	// =========================================================================
	{
		// 7.1 Explicit incoming direction: Target facing (0, 0, 0), incoming (+X, 0, 0) -> Local dir is (1, 0, 0)
		FCombatImpactEffectContext* FrontContext = new FCombatImpactEffectContext();
		FrontContext->SetWorldIncomingDirection(FVector(1.0f, 0.0f, 0.0f));
		FGameplayEffectContextHandle FrontHandle(FrontContext);

		Player->SetActorRotation(FRotator::ZeroRotator);
		const FVector LocalDirFront = FHitReactionImpactResolver::ResolveImpactDirectionFromContext(FrontHandle, nullptr, Player);
		TestEqual(TEXT("7.1 Front incoming direction resolves to local (1, 0, 0)"), LocalDirFront, FVector(1.0f, 0.0f, 0.0f));

		// Target rotated 90 deg (facing +Y): incoming (+X) is on target's right side -> Local dir is (0, 1, 0)
		Player->SetActorRotation(FRotator(0.0f, 90.0f, 0.0f));
		const FVector LocalDirRotated = FHitReactionImpactResolver::ResolveImpactDirectionFromContext(FrontHandle, nullptr, Player);
		TestEqual(TEXT("7.1 Incoming (+X) on +Y facing target resolves to local right (0, -1, 0)"), LocalDirRotated, FVector(0.0f, -1.0f, 0.0f));
		Player->SetActorRotation(FRotator::ZeroRotator);

		// 7.2 Explicit zero direction: resolves strictly to ZeroVector without fallback
		FCombatImpactEffectContext* ZeroContext = new FCombatImpactEffectContext();
		ZeroContext->SetWorldIncomingDirection(FVector::ZeroVector);
		ZeroContext->AddInstigator(Enemy, Enemy); // Even with Instigator present!
		FGameplayEffectContextHandle ZeroHandle(ZeroContext);

		const FVector LocalDirZero = FHitReactionImpactResolver::ResolveImpactDirectionFromContext(ZeroHandle, nullptr, Player);
		TestEqual(TEXT("7.2 Explicit zero direction returns ZeroVector without instigator fallback"), LocalDirZero, FVector::ZeroVector);

		// 7.3 Ordinary context: preserves Instigator priority over ImpactNormal
		FGameplayEffectContextHandle NormalContext = PlayerASC->MakeEffectContext();
		NormalContext.AddInstigator(Enemy, Enemy); // Enemy is at +X (200, 0, 0)
		FHitResult NormalHit;
		NormalHit.ImpactNormal = FVector(0.0f, 1.0f, 0.0f); // ImpactNormal points +Y
		NormalContext.AddHitResult(NormalHit, true);

		Player->SetActorRotation(FRotator::ZeroRotator);
		const FVector LocalDirOrdinary = FHitReactionImpactResolver::ResolveImpactDirectionFromContext(NormalContext, nullptr, Player);
		TestEqual(TEXT("7.3 Ordinary context prioritizes Instigator (+X) over ImpactNormal (+Y)"), LocalDirOrdinary, FVector(1.0f, 0.0f, 0.0f));
	}

	// =========================================================================
	// SECTION 8: 真实物理移动碰撞与单次投递集成 (Real UWorld Movement & Single Delivery)
	// =========================================================================
	{
		// Clean state: reset Enemy health
		TestEqual(TEXT("8.0 Enemy has not previously consumed death teardown"), Enemy->GetTestDeathRagdollConsumeCount(), 0);
		EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 1000.0f);
		const float EnemyInitialHealth = EnemyASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute());

		// Place target Enemy at (300, 0, 50) and stabilize against gravity fall
		if (UCharacterMovementComponent* EnemyMove = Enemy->GetCharacterMovement())
		{
			EnemyMove->DisableMovement();
		}
		Enemy->SetActorLocation(FVector(300.0f, 0.0f, 50.0f));
		Enemy->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f));
		Player->SetActorLocation(FVector(0.0f, 500.0f, 50.0f));

		// Assert collision preconditions on target
		UCapsuleComponent* TargetCapsule = Enemy->GetCapsuleComponent();
		TestNotNull(TEXT("8.0 Target has valid CapsuleComponent"), TargetCapsule);
		if (TargetCapsule)
		{
			TargetCapsule->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
			TestTrue(TEXT("8.0 Target Capsule is registered"), TargetCapsule->IsRegistered());
			TestTrue(TEXT("8.0 Target Capsule has collision enabled"), TargetCapsule->IsCollisionEnabled());
			TestTrue(TEXT("8.0 Target Capsule generates overlap events"), TargetCapsule->GetGenerateOverlapEvents());
		}

		// Spawn projectile at (0, 0, 50) facing +X
		ACombatProjectile* Projectile = World->SpawnActor<ACombatProjectile>(
			ACombatProjectile::StaticClass(),
			FVector(0.0f, 0.0f, 50.0f),
			FRotator::ZeroRotator);

		TestNotNull(TEXT("8.0 Projectile spawned"), Projectile);
		if (Projectile)
		{
			// Precondition checks on projectile components
			USphereComponent* ProjCollision = Projectile->GetCollisionComponent();
			UProjectileMovementComponent* ProjMove = Projectile->GetMovementComponent();

			TestNotNull(TEXT("8.0 Projectile CollisionComponent exists"), ProjCollision);
			TestNotNull(TEXT("8.0 Projectile MovementComponent exists"), ProjMove);
			if (ProjCollision && ProjMove)
			{
				TestTrue(TEXT("8.0 Projectile CollisionComponent is registered"), ProjCollision->IsRegistered());
				TestTrue(TEXT("8.0 Projectile CollisionComponent is query-enabled"), ProjCollision->IsCollisionEnabled());
				TestTrue(TEXT("8.0 Projectile MovementComponent is active"), ProjMove->IsActive());
				TestEqual(TEXT("8.0 Projectile MovementComponent is bound to CollisionComponent"), ProjMove->UpdatedComponent.Get(), Cast<USceneComponent>(ProjCollision));
			}

			// Author valid projectile definition
			UProjectileDefinition* Def = NewObject<UProjectileDefinition>(GetTransientPackage(), TEXT("Test_RealCollisionDef"));
			Def->InitialSpeed = 3000.0f;
			Def->MaxSpeed = 3000.0f;
			Def->LifespanSeconds = 5.0f;
			Def->CollisionRadius = 20.0f;
			Def->DamageGameplayEffectClass = UTestProjectileDamageGE::StaticClass();

			FCombatProjectileLaunchRequest LaunchRequest;
			LaunchRequest.Definition = Def;
			LaunchRequest.SourceActor = Player;
			LaunchRequest.SourceAbilitySystemComponent = PlayerASC;
			LaunchRequest.InitialFlightDirection = FVector(0.0f, 1.0f, 0.0f);

			TestTrue(TEXT("8.0 Projectile InitializeProjectile succeeds"), Projectile->InitializeProjectile(LaunchRequest));
			Projectile->DispatchBeginPlay();
			// Turn after launch: the impact must use current velocity, not launch direction or shooter bearing.
			if (ProjMove)
			{
				ProjMove->Velocity = FVector(3000.0f, 0.0f, 0.0f);
				ProjMove->UpdateComponentVelocity();
			}

			FGameplayEffectContextHandle CapturedImpactContext;
			bool bCapturedProjectileSourceObject = false;
			const FDelegateHandle CaptureHandle = EnemyASC->GetGameplayAttributeValueChangeDelegate(
				UCharacterAttributeSet::GetHealthAttribute()).AddLambda(
				[&CapturedImpactContext, &bCapturedProjectileSourceObject, Projectile](const FOnAttributeChangeData& Data)
				{
					if (Data.GEModData && Data.NewValue < Data.OldValue)
					{
						CapturedImpactContext = Data.GEModData->EffectSpec.GetContext().Duplicate();
						// SourceObject is weak: a projectile without a trail is destroyed before World::Tick returns.
						bCapturedProjectileSourceObject = CapturedImpactContext.GetSourceObject() == Projectile;
					}
				});

			// Real UWorld Tick advance: 0.05s steps, max 10 steps (0.5s total)
			const FVector StartLocation = Projectile->GetActorLocation();
			bool bImpactDetected = false;

			for (int32 Step = 1; Step <= 10; ++Step)
			{
				TickTestWorld(World, 0.05f);

				const float CurrentEnemyHealth = EnemyASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute());
				if (CurrentEnemyHealth < EnemyInitialHealth)
				{
					bImpactDetected = true;
					break;
				}
			}

			EnemyASC->GetGameplayAttributeValueChangeDelegate(
				UCharacterAttributeSet::GetHealthAttribute()).Remove(CaptureHandle);

			TestTrue(TEXT("8.1 Real UWorld collision impact occurred within 10 steps"), bImpactDetected);
			TestTrue(TEXT("8.1 Projectile moved forward from start location"), Projectile->GetActorLocation().X > StartLocation.X);
			TestEqual(TEXT("8.1 Enemy took 25 damage from real physical impact"),
				EnemyASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute()), EnemyInitialHealth - 25.0f);

			const FGameplayEffectContext* RawImpactContext = CapturedImpactContext.Get();
			const bool bHasImpactSnapshot = RawImpactContext && RawImpactContext->GetScriptStruct()
				&& RawImpactContext->GetScriptStruct()->IsChildOf(FCombatImpactEffectContext::StaticStruct());
			TestTrue(TEXT("8.1 Actual damage GE carries derived impact context"), bHasImpactSnapshot);
			if (bHasImpactSnapshot)
			{
				const FCombatImpactEffectContext* ImpactContext = static_cast<const FCombatImpactEffectContext*>(RawImpactContext);
				TestEqual(TEXT("8.1 Actual snapshot follows negative current velocity"),
					ImpactContext->GetWorldIncomingDirection(), FVector(-1.0f, 0.0f, 0.0f));
				TestTrue(TEXT("8.1 Actual context preserves instigator"), CapturedImpactContext.GetInstigator() == Player);
				TestTrue(TEXT("8.1 Actual context preserves effect causer"), CapturedImpactContext.GetEffectCauser() == Player);
				TestTrue(TEXT("8.1 Actual context preserves source ASC"),
					CapturedImpactContext.GetOriginalInstigatorAbilitySystemComponent() == PlayerASC);
				TestTrue(TEXT("8.1 Actual context preserves projectile source object at delivery"), bCapturedProjectileSourceObject);
				const FHitResult* ImpactHit = CapturedImpactContext.GetHitResult();
				TestTrue(TEXT("8.1 Actual context preserves hit target"), ImpactHit && ImpactHit->GetActor() == Enemy);
				TestTrue(TEXT("8.1 Actual snapshot resolves to target-local front"),
					FHitReactionImpactResolver::ResolveImpactDirectionFromContext(CapturedImpactContext, nullptr, Enemy)
						.Equals(FVector(1.0f, 0.0f, 0.0f), KINDA_SMALL_NUMBER));
			}

			// 8.2 Single Delivery Assertion: duplicate overlap must NOT apply damage again
			const float HealthAfterFirstHit = EnemyASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute());
			if (ProjCollision)
			{
				// Manual callback trigger to verify fail-closed delivery gate
				ProjCollision->OnComponentBeginOverlap.Broadcast(
					ProjCollision,
					Enemy,
					TargetCapsule,
					0,
					false,
					FHitResult());
			}

			TestEqual(TEXT("8.2 Duplicate overlap does not deliver damage again (Single Delivery invariant)"),
				EnemyASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute()), HealthAfterFirstHit);

			// 8.3 Reuse the captured collision context for a lethal GE; this is a GAS consumer check, not another flight.
			Enemy->ConfigureTestDeathRagdollImpact(FName(TEXT("pelvis")), 400.0f, 150.0f);
			EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 20.0f);
			const FGameplayEffectSpecHandle LethalSpec = PlayerASC->MakeOutgoingSpec(
				UTestProjectileDamageGE::StaticClass(), 1.0f, CapturedImpactContext.Duplicate());
			TestTrue(TEXT("8.3 Lethal spec is valid"), LethalSpec.IsValid());
			if (LethalSpec.IsValid())
			{
				EnemyASC->ApplyGameplayEffectSpecToSelf(*LethalSpec.Data.Get());
				TestTrue(TEXT("8.3 Enemy dies from lethal GE"), Enemy->IsDead());
				TestTrue(TEXT("8.3 Death impulse moves away from captured incoming side"),
					Enemy->GetTestLastDeathRagdollVelocityChange().Equals(FVector(400.0f, 0.0f, 150.0f), 1.0f));
			}

			// 8.4 Resolve only after both real source actors have been destroyed.
			Projectile->Destroy();
			Player->Destroy();
			TestTrue(TEXT("8.4 Projectile and shooter entered destruction"),
				Projectile->IsActorBeingDestroyed() && Player->IsActorBeingDestroyed());
			TestTrue(TEXT("8.4 Captured snapshot survives projectile and shooter destruction"),
				FHitReactionImpactResolver::ResolveImpactDirectionFromContext(CapturedImpactContext, nullptr, Enemy)
					.Equals(FVector(1.0f, 0.0f, 0.0f), KINDA_SMALL_NUMBER));
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
