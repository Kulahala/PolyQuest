#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/PlayerGuardAbility.h"
#include "AbilitySystem/CharacterAttributeSet.h"
#include "Camera/CameraShakeBase.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Character/Player/PlayerCharacter.h"
#include "Combat/Melee/MeleeHitResolver.h"
#include "Combat/Projectile/CombatProjectileHitResolver.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/PolyQuestPlayerController.h"
#include "GameplayTagContainer.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "Tests/CombatAutomationFixture.h"
#include "Tests/TestGuardStaminaCostGE.h"
#include "Tests/TestHitFeedbackCameraShake.h"
#include "Tests/TestParryCounterPoiseGE.h"
#include "Tests/TestProjectileDamageGE.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPlayerDefenseAudioAutomationTest,
	"PolyQuest.Combat.DefenseAudio",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	struct FPlayerDefenseAudioWorldCleanup
	{
		UWorld* World = nullptr;
		~FPlayerDefenseAudioWorldCleanup()
		{
			if (World)
			{
				UGameplayStatics::SetGlobalTimeDilation(World, 1.0f);
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
		}
	};

	void TickPlayerDefenseAudioTestWorld(UWorld* World, const float DeltaSeconds)
	{
		if (World)
		{
			World->Tick(ELevelTick::LEVELTICK_All, DeltaSeconds);
			++GFrameCounter;
		}
	}

	void AdvancePlayerDefenseAudioTestWorld(UWorld* World, float DeltaSeconds)
	{
		constexpr float MaxTickStepSeconds = 0.05f;
		while (DeltaSeconds > KINDA_SMALL_NUMBER)
		{
			const float TickStep = FMath::Min(DeltaSeconds, MaxTickStepSeconds);
			TickPlayerDefenseAudioTestWorld(World, TickStep);
			DeltaSeconds -= TickStep;
		}
	}

	bool ApplyDamageSpec(
		UAbilitySystemComponent* SourceASC,
		UAbilitySystemComponent* TargetASC,
		AActor* InstigatorActor,
		const FGameplayTagContainer* DynamicTags = nullptr,
		const FHitResult* HitResult = nullptr)
	{
		if (!SourceASC || !TargetASC)
		{
			return false;
		}

		FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
		if (InstigatorActor)
		{
			Context.AddInstigator(InstigatorActor, InstigatorActor);
		}
		if (HitResult)
		{
			Context.AddHitResult(*HitResult, true);
		}

		FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(UTestProjectileDamageGE::StaticClass(), 1.0f, Context);
		if (!Spec.IsValid() || !Spec.Data.IsValid())
		{
			return false;
		}
		if (DynamicTags)
		{
			Spec.Data->AppendDynamicAssetTags(*DynamicTags);
		}

		return TargetASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()).WasSuccessfullyApplied();
	}
}

bool FPlayerDefenseAudioAutomationTest::RunTest(const FString& Parameters)
{
	if (!TestNotNull(TEXT("Engine is available"), GEngine))
	{
		return false;
	}

	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("PlayerDefenseAudioTestWorld"));
	WorldContext.SetCurrentWorld(World);
	FPlayerDefenseAudioWorldCleanup Cleanup{ World };

	if (!TestNotNull(TEXT("Test World created"), World))
	{
		return false;
	}

	FURL WorldURL;
	World->InitializeActorsForPlay(WorldURL);
	World->BeginPlay();

	APlayerCharacter* Player = FCombatAutomationFixture::SpawnPlayer(
		World,
		FTransform(FRotator::ZeroRotator, FVector::ZeroVector));
	AEnemyCharacter* Enemy = FCombatAutomationFixture::SpawnPassiveEnemy(
		World,
		FTransform(FRotator(0.0f, 180.0f, 0.0f), FVector(200.0f, 0.0f, 0.0f)));
	APolyQuestPlayerController* Controller = World->SpawnActor<APolyQuestPlayerController>();

	if (!TestNotNull(TEXT("Player fixture created"), Player)
		|| !TestNotNull(TEXT("Enemy fixture created"), Enemy)
		|| !TestNotNull(TEXT("Controller created"), Controller))
	{
		return false;
	}

	Controller->DispatchBeginPlay();
	Controller->SetAsLocalPlayerController();
	Controller->Possess(Player);

	UAbilitySystemComponent* PlayerASC = Player->GetAbilitySystemComponent();
	UAbilitySystemComponent* EnemyASC = Enemy->GetAbilitySystemComponent();
	if (!TestNotNull(TEXT("Player ASC available"), PlayerASC) || !TestNotNull(TEXT("Enemy ASC available"), EnemyASC))
	{
		return false;
	}

	// Set baseline attributes
	PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxHealthAttribute(), 1000.0f);
	PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 1000.0f);
	PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxStaminaAttribute(), 100.0f);
	PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 100.0f);
	PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxPoiseAttribute(), 100.0f);
	PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetPoiseAttribute(), 100.0f);

	EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxHealthAttribute(), 1000.0f);
	EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 1000.0f);
	EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxPoiseAttribute(), 100.0f);
	EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetPoiseAttribute(), 100.0f);

	// Setup Camera Shake for Player
	Player->ConfigureTestHitFeedbackCameraShakes(
		UTestSmallHitFeedbackCameraShake::StaticClass(),
		UTestBigHitFeedbackCameraShake::StaticClass(),
		UTestLaunchHitFeedbackCameraShake::StaticClass());

	// Setup transient Guard ability on Player
	const FGameplayTag GuardAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Defense.Guard")), false);
	const FGameplayTag GuardingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Guarding")), false);
	TestTrue(TEXT("Guard ability tag is valid"), GuardAbilityTag.IsValid());
	TestTrue(TEXT("Guarding state tag is valid"), GuardingStateTag.IsValid());

	FGameplayAbilitySpec GuardSpec(UPlayerGuardAbility::StaticClass(), 1, INDEX_NONE, Player);
	GuardSpec.DynamicAbilityTags.AddTag(GuardAbilityTag);
	const FGameplayAbilitySpecHandle GuardSpecHandle = PlayerASC->GiveAbility(GuardSpec);
	TestTrue(TEXT("Guard spec handle is valid"), GuardSpecHandle.IsValid());

	FGameplayAbilitySpec* AbilitySpec = PlayerASC->FindAbilitySpecFromHandle(GuardSpecHandle);
	UPlayerGuardAbility* TestGuardAbility = AbilitySpec ? Cast<UPlayerGuardAbility>(AbilitySpec->GetPrimaryInstance()) : nullptr;
	if (!TestNotNull(TEXT("Primary guard ability instance created"), TestGuardAbility))
	{
		return false;
	}

	TestGuardAbility->SetTestGuardStaminaCostGameplayEffectClass(UTestGuardStaminaCostGE::StaticClass());
	TestGuardAbility->SetTestGuardActive(true);
	PlayerASC->AddLooseGameplayTag(GuardingStateTag);

	USoundWave* TestGuardSound = NewObject<USoundWave>(Player);
	TestGuardAbility->SetTestGuardSuccessSound(TestGuardSound);
	TestGuardAbility->SetTestBypassAudioPlayback(true);

	USoundWave* TestReceivedHitSound = NewObject<USoundWave>(Player);
	Player->SetTestReceivedHitSound(TestReceivedHitSound);
	Player->SetTestBypassReceivedHitAudioPlayback(true);

	// =========================================================================
	// SECTION 1: Guard Success Audio (Melee, Projectile, Guard Break Absorbed)
	// =========================================================================
	{
		const FVector NonZeroImpactPoint(100.0f, 0.0f, 50.0f);
		FHitResult ValidHitResult;
		ValidHitResult.HitObjectHandle = FActorInstanceHandle(Player);
		ValidHitResult.ImpactPoint = NonZeroImpactPoint;

		// 1.1 Melee Guard Success
		FMeleeHitRequest MeleeRequest;
		MeleeRequest.SourceActor = Enemy;
		MeleeRequest.SourceAbilitySystemComponent = EnemyASC;
		MeleeRequest.HitResult = ValidHitResult;
		MeleeRequest.DamageGameplayEffectClass = UTestProjectileDamageGE::StaticClass();
		MeleeRequest.GuardStaminaDamage = 20.0f;
		MeleeRequest.AbilityLevel = 1.0f;

		TestEqual(TEXT("Initial guard sound count is 0"), TestGuardAbility->GetTestGuardSuccessSoundDispatchCount(), 0);
		const bool bMeleeGuarded = FMeleeHitResolver::TryResolveHit(MeleeRequest);
		TestTrue(TEXT("Melee hit is successfully resolved via Guard"), bMeleeGuarded);
		TestEqual(TEXT("Guard sound dispatched once for melee guard"), TestGuardAbility->GetTestGuardSuccessSoundDispatchCount(), 1);
		TestEqual(TEXT("Guard sound location matches impact point"), TestGuardAbility->GetTestLastGuardSuccessSoundLocation(), NonZeroImpactPoint);
		TestEqual(TEXT("Player stamina reduced by 20"), PlayerASC->GetNumericAttribute(UCharacterAttributeSet::GetStaminaAttribute()), 80.0f);
		TestEqual(TEXT("Player health untouched"), PlayerASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute()), 1000.0f);

		// 1.2 Projectile Guard Success
		const FVector ProjectileImpactPoint(120.0f, 0.0f, 60.0f);
		FHitResult ProjectileHitResult;
		ProjectileHitResult.HitObjectHandle = FActorInstanceHandle(Player);
		ProjectileHitResult.ImpactPoint = ProjectileImpactPoint;

		FCombatProjectileHitRequest ProjectileRequest;
		ProjectileRequest.SourceActor = Enemy;
		ProjectileRequest.SourceAbilitySystemComponent = EnemyASC;
		ProjectileRequest.HitResult = ProjectileHitResult;
		ProjectileRequest.DamageGameplayEffectClass = UTestProjectileDamageGE::StaticClass();
		ProjectileRequest.GuardStaminaDamage = 30.0f;
		ProjectileRequest.AbilityLevel = 1.0f;

		const bool bProjGuarded = FCombatProjectileHitResolver::TryResolveHit(ProjectileRequest);
		TestTrue(TEXT("Projectile hit is successfully resolved via Guard"), bProjGuarded);
		TestEqual(TEXT("Guard sound dispatched twice after projectile guard"), TestGuardAbility->GetTestGuardSuccessSoundDispatchCount(), 2);
		TestEqual(TEXT("Guard sound location matches projectile impact point"), TestGuardAbility->GetTestLastGuardSuccessSoundLocation(), ProjectileImpactPoint);
		TestEqual(TEXT("Player stamina reduced by 30 to 50"), PlayerASC->GetNumericAttribute(UCharacterAttributeSet::GetStaminaAttribute()), 50.0f);

		// 1.3 Guard Break Absorbed Contact
		MeleeRequest.GuardStaminaDamage = 100.0f; // Exceeds remaining 50 stamina
		const bool bGuardBreakGuarded = FMeleeHitResolver::TryResolveHit(MeleeRequest);
		TestTrue(TEXT("Guard Break hit is still absorbed and returns true"), bGuardBreakGuarded);
		TestEqual(TEXT("Guard sound dispatched once on guard break absorption"), TestGuardAbility->GetTestGuardSuccessSoundDispatchCount(), 3);
		TestEqual(TEXT("Player stamina reached 0"), PlayerASC->GetNumericAttribute(UCharacterAttributeSet::GetStaminaAttribute()), 0.0f);
	}

	// =========================================================================
	// SECTION 2: Guard Failure / Negative & Location Fallbacks
	// =========================================================================
	{
		// Restore stamina & guard active
		PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetStaminaAttribute(), 100.0f);
		TestGuardAbility->SetTestGuardActive(true);
		if (!PlayerASC->HasMatchingGameplayTag(GuardingStateTag))
		{
			PlayerASC->AddLooseGameplayTag(GuardingStateTag);
		}

		const int32 BaseGuardSoundCount = TestGuardAbility->GetTestGuardSuccessSoundDispatchCount();

		// 2.1 Attacker outside guard arc (behind player)
		AEnemyCharacter* RearEnemy = FCombatAutomationFixture::SpawnPassiveEnemy(
			World,
			FTransform(FRotator::ZeroRotator, FVector(-200.0f, 0.0f, 0.0f)));
		if (TestNotNull(TEXT("Rear enemy spawned"), RearEnemy))
		{
			FHitResult RearHitResult;
			RearHitResult.HitObjectHandle = FActorInstanceHandle(Player);
			RearHitResult.ImpactPoint = FVector(-100.0f, 0.0f, 50.0f);

			const bool bRearGuard = TestGuardAbility->TryGuardMeleeHit(RearEnemy, 20.0f, RearHitResult);
			TestFalse(TEXT("Guard fails for rear attacker"), bRearGuard);
			TestEqual(TEXT("Guard sound count unchanged on arc failure"), TestGuardAbility->GetTestGuardSuccessSoundDispatchCount(), BaseGuardSoundCount);
			RearEnemy->Destroy();
		}

		// 2.2 Inactive Guard
		TestGuardAbility->SetTestGuardActive(false);
		FHitResult ValidHit;
		ValidHit.HitObjectHandle = FActorInstanceHandle(Player);
		ValidHit.ImpactPoint = FVector(100.0f, 0.0f, 50.0f);

		TestFalse(TEXT("Guard fails when inactive"), TestGuardAbility->TryGuardMeleeHit(Enemy, 20.0f, ValidHit));
		TestEqual(TEXT("Guard sound count unchanged when inactive"), TestGuardAbility->GetTestGuardSuccessSoundDispatchCount(), BaseGuardSoundCount);
		TestGuardAbility->SetTestGuardActive(true);

		// 2.3 Missing Guard GE class
		TestGuardAbility->SetTestGuardStaminaCostGameplayEffectClass(nullptr);
		TestFalse(TEXT("Guard fails when stamina GE is null"), TestGuardAbility->TryGuardMeleeHit(Enemy, 20.0f, ValidHit));
		TestEqual(TEXT("Guard sound count unchanged when GE missing"), TestGuardAbility->GetTestGuardSuccessSoundDispatchCount(), BaseGuardSoundCount);
		TestGuardAbility->SetTestGuardStaminaCostGameplayEffectClass(UTestGuardStaminaCostGE::StaticClass());

		// 2.4 Null sound asset (graceful silence, guard still succeeds)
		TestGuardAbility->SetTestGuardSuccessSound(nullptr);
		const bool bNullSoundGuard = TestGuardAbility->TryGuardMeleeHit(Enemy, 10.0f, ValidHit);
		TestTrue(TEXT("Guard succeeds with null sound asset"), bNullSoundGuard);
		TestEqual(TEXT("Guard sound count unchanged when sound is null"), TestGuardAbility->GetTestGuardSuccessSoundDispatchCount(), BaseGuardSoundCount);
		TestGuardAbility->SetTestGuardSuccessSound(TestGuardSound);

		// 2.5 HitResult Location Fallbacks (Wrong Actor, Zero, NaN, Inf)
		// A: Wrong Actor in HitResult -> falls back to Player ActorLocation
		FHitResult MismatchedHit;
		MismatchedHit.HitObjectHandle = FActorInstanceHandle(Enemy);
		MismatchedHit.ImpactPoint = FVector(500.0f, 500.0f, 500.0f);
		TestTrue(TEXT("Guard succeeds with mismatched hit actor"), TestGuardAbility->TryGuardMeleeHit(Enemy, 10.0f, MismatchedHit));
		TestEqual(TEXT("Guard sound count incremented"), TestGuardAbility->GetTestGuardSuccessSoundDispatchCount(), BaseGuardSoundCount + 1);
		TestEqual(TEXT("Guard sound fell back to Player location"), TestGuardAbility->GetTestLastGuardSuccessSoundLocation(), Player->GetActorLocation());

		// B: Zero ImpactPoint -> falls back to Player ActorLocation
		FHitResult ZeroHit;
		ZeroHit.HitObjectHandle = FActorInstanceHandle(Player);
		ZeroHit.ImpactPoint = FVector::ZeroVector;
		TestTrue(TEXT("Guard succeeds with zero impact point"), TestGuardAbility->TryGuardMeleeHit(Enemy, 10.0f, ZeroHit));
		TestEqual(TEXT("Guard sound fell back to Player location for zero point"), TestGuardAbility->GetTestLastGuardSuccessSoundLocation(), Player->GetActorLocation());

		// C: NaN ImpactPoint -> falls back to Player ActorLocation
		FHitResult NaNHit;
		NaNHit.HitObjectHandle = FActorInstanceHandle(Player);
		NaNHit.ImpactPoint = FVector(NAN, 0.0f, 0.0f);
		TestTrue(TEXT("Guard succeeds with NaN impact point"), TestGuardAbility->TryGuardMeleeHit(Enemy, 10.0f, NaNHit));
		TestEqual(TEXT("Guard sound fell back to Player location for NaN point"), TestGuardAbility->GetTestLastGuardSuccessSoundLocation(), Player->GetActorLocation());

		// D: Inf ImpactPoint -> falls back to Player ActorLocation
		FHitResult InfHit;
		InfHit.HitObjectHandle = FActorInstanceHandle(Player);
		InfHit.ImpactPoint = FVector(INFINITY, 0.0f, 0.0f);
		TestTrue(TEXT("Guard succeeds with Inf impact point"), TestGuardAbility->TryGuardMeleeHit(Enemy, 10.0f, InfHit));
		TestEqual(TEXT("Guard sound fell back to Player location for Inf point"), TestGuardAbility->GetTestLastGuardSuccessSoundLocation(), Player->GetActorLocation());
	}

	// =========================================================================
	// SECTION 3: Player Received Hit Audio (Authority, Non-lethal, Enemy Team)
	// =========================================================================
	{
		TestEqual(TEXT("Initial player hit sound count is 0"), Player->GetTestReceivedHitSoundDispatchCount(), 0);

		// 3.1 Valid Enemy Non-lethal Hit Damage (Small / Big / Launch)
		FHitResult ValidPlayerHit;
		ValidPlayerHit.HitObjectHandle = FActorInstanceHandle(Player);
		const FVector HitImpactPoint(50.0f, 20.0f, 80.0f);
		ValidPlayerHit.ImpactPoint = HitImpactPoint;

		FGameplayTagContainer SmallTags;
		SmallTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Data.Reaction.Small")), false));

		const bool bAppliedSmall = ApplyDamageSpec(EnemyASC, PlayerASC, Enemy, &SmallTags, &ValidPlayerHit);
		TestTrue(TEXT("Enemy small damage applied"), bAppliedSmall);
		TestEqual(TEXT("ReceivedHitSound dispatched once"), Player->GetTestReceivedHitSoundDispatchCount(), 1);
		TestEqual(TEXT("ReceivedHitSound location matches impact point"), Player->GetTestLastReceivedHitSoundLocation(), HitImpactPoint);
		TestEqual(TEXT("Camera shake triggered for small hit"), Player->GetTestHitFeedbackCameraShakeStartCount(), 1);

		// Big Tier Damage
		FGameplayTagContainer BigTags;
		BigTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Data.Reaction.Big")), false));
		const bool bAppliedBig = ApplyDamageSpec(EnemyASC, PlayerASC, Enemy, &BigTags, &ValidPlayerHit);
		TestTrue(TEXT("Enemy big damage applied"), bAppliedBig);
		TestEqual(TEXT("ReceivedHitSound dispatched twice after big damage"), Player->GetTestReceivedHitSoundDispatchCount(), 2);
		TestEqual(TEXT("Camera shake triggered for big hit"), Player->GetTestHitFeedbackCameraShakeStartCount(), 2);

		// 3.2 HitResult Location Fallback for Player Received Hit
		FHitResult MismatchedPlayerHit;
		MismatchedPlayerHit.HitObjectHandle = FActorInstanceHandle(Enemy);
		MismatchedPlayerHit.ImpactPoint = FVector(999.0f, 999.0f, 999.0f);
		const bool bAppliedFallback = ApplyDamageSpec(EnemyASC, PlayerASC, Enemy, &SmallTags, &MismatchedPlayerHit);
		TestTrue(TEXT("Damage with mismatched hit actor applied"), bAppliedFallback);
		TestEqual(TEXT("ReceivedHitSound dispatched count 3"), Player->GetTestReceivedHitSoundDispatchCount(), 3);
		TestEqual(TEXT("ReceivedHitSound fell back to Player location"), Player->GetTestLastReceivedHitSoundLocation(), Player->GetActorLocation());

		// Inf ImpactPoint -> falls back to Player ActorLocation
		FHitResult InfPlayerHit;
		InfPlayerHit.HitObjectHandle = FActorInstanceHandle(Player);
		InfPlayerHit.ImpactPoint = FVector(INFINITY, 0.0f, 0.0f);
		const bool bAppliedInf = ApplyDamageSpec(EnemyASC, PlayerASC, Enemy, &SmallTags, &InfPlayerHit);
		TestTrue(TEXT("Damage with Inf impact point applied"), bAppliedInf);
		TestEqual(TEXT("ReceivedHitSound dispatched count 4"), Player->GetTestReceivedHitSoundDispatchCount(), 4);
		TestEqual(TEXT("ReceivedHitSound fell back to Player location for Inf"), Player->GetTestLastReceivedHitSoundLocation(), Player->GetActorLocation());

		// Stunned Player still triggers hit sound on non-lethal damage
		const FGameplayTag StunnedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
		if (StunnedTag.IsValid())
		{
			PlayerASC->AddLooseGameplayTag(StunnedTag);
			const bool bAppliedStunned = ApplyDamageSpec(EnemyASC, PlayerASC, Enemy, &SmallTags, &ValidPlayerHit);
			TestTrue(TEXT("Damage to stunned player applied"), bAppliedStunned);
			TestEqual(TEXT("ReceivedHitSound dispatched for stunned player"), Player->GetTestReceivedHitSoundDispatchCount(), 5);
			PlayerASC->RemoveLooseGameplayTag(StunnedTag);
		}

		// Invalid multi-tier reaction tags skip reaction montage event but still dispatch ReceivedHitSound
		FGameplayTagContainer InvalidReactionTags;
		InvalidReactionTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Data.Reaction.Small")), false));
		InvalidReactionTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Data.Reaction.Big")), false));
		const bool bAppliedInvalidTags = ApplyDamageSpec(EnemyASC, PlayerASC, Enemy, &InvalidReactionTags, &ValidPlayerHit);
		TestTrue(TEXT("Damage with invalid reaction tags applied"), bAppliedInvalidTags);
		TestEqual(TEXT("ReceivedHitSound dispatched for invalid reaction tags"), Player->GetTestReceivedHitSoundDispatchCount(), 6);

		// 3.3 Friendly Source / No Team / Invalid Tag (Should NOT trigger hit sound)
		const int32 SoundCountBeforeFriendly = Player->GetTestReceivedHitSoundDispatchCount();

		// Player damaging themselves (Team.Player vs Team.Enemy)
		const bool bSelfDamage = ApplyDamageSpec(PlayerASC, PlayerASC, Player, nullptr, nullptr);
		TestTrue(TEXT("Self damage applied"), bSelfDamage);
		TestEqual(TEXT("ReceivedHitSound NOT dispatched for friendly/self damage"), Player->GetTestReceivedHitSoundDispatchCount(), SoundCountBeforeFriendly);

		// Damage with non-enemy / no-team instigator (AActor without UCombatTeamAgent)
		AActor* PlainActor = World->SpawnActor<AActor>();
		const bool bNoTeamDamage = ApplyDamageSpec(EnemyASC, PlayerASC, PlainActor, nullptr, nullptr);
		TestTrue(TEXT("No-team damage applied"), bNoTeamDamage);
		TestEqual(TEXT("ReceivedHitSound NOT dispatched without enemy team tag"), Player->GetTestReceivedHitSoundDispatchCount(), SoundCountBeforeFriendly);
		if (PlainActor)
		{
			PlainActor->Destroy();
		}

		// 3.4 Healing / Attribute increase (Should NOT trigger hit sound)
		FGameplayEffectContextHandle HealContext = EnemyASC->MakeEffectContext();
		HealContext.AddInstigator(Enemy, Enemy);
		UGameplayEffect* TransientHealGE = NewObject<UGameplayEffect>(GetTransientPackage());
		TransientHealGE->DurationPolicy = EGameplayEffectDurationType::Instant;
		FGameplayModifierInfo HealMod;
		HealMod.Attribute = UCharacterAttributeSet::GetHealthAttribute();
		HealMod.ModifierOp = EGameplayModOp::Additive;
		HealMod.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(50.0f));
		TransientHealGE->Modifiers.Add(HealMod);

		FGameplayEffectSpec HealSpec(TransientHealGE, HealContext, 1.0f);
		PlayerASC->ApplyGameplayEffectSpecToSelf(HealSpec);
		TestEqual(TEXT("ReceivedHitSound NOT dispatched on healing"), Player->GetTestReceivedHitSoundDispatchCount(), SoundCountBeforeFriendly);

		// Direct Base value write (bypasses GEModData)
		PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 500.0f);
		TestEqual(TEXT("ReceivedHitSound NOT dispatched on direct base change"), Player->GetTestReceivedHitSoundDispatchCount(), SoundCountBeforeFriendly);

		// 3.5 Poise-only change (Health untouched)
		FGameplayEffectContextHandle PoiseContext = EnemyASC->MakeEffectContext();
		PoiseContext.AddInstigator(Enemy, Enemy);
		FGameplayEffectSpecHandle PoiseSpec = EnemyASC->MakeOutgoingSpec(UTestParryCounterPoiseGE::StaticClass(), 1.0f, PoiseContext);
		if (PoiseSpec.IsValid() && PoiseSpec.Data.IsValid())
		{
			PoiseSpec.Data->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(FName(TEXT("Data.Poise.Parry")), false), -20.0f);
			PlayerASC->ApplyGameplayEffectSpecToSelf(*PoiseSpec.Data.Get());
		}
		TestEqual(TEXT("ReceivedHitSound NOT dispatched for poise-only effect"), Player->GetTestReceivedHitSoundDispatchCount(), SoundCountBeforeFriendly);

		// 3.6 Lethal damage (NewValue <= 0) & Dead State
		PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 10.0f);
		const bool bLethalDamage = ApplyDamageSpec(EnemyASC, PlayerASC, Enemy, nullptr, nullptr); // Drops health by 25 to <= 0
		TestTrue(TEXT("Lethal damage applied"), bLethalDamage);
		TestEqual(TEXT("ReceivedHitSound NOT dispatched for lethal damage"), Player->GetTestReceivedHitSoundDispatchCount(), SoundCountBeforeFriendly);

		// Dead tag check
		PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 500.0f);
		const FGameplayTag DeadTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
		if (DeadTag.IsValid())
		{
			PlayerASC->AddLooseGameplayTag(DeadTag);
			ApplyDamageSpec(EnemyASC, PlayerASC, Enemy, nullptr, nullptr);
			TestEqual(TEXT("ReceivedHitSound NOT dispatched when player has Dead tag"), Player->GetTestReceivedHitSoundDispatchCount(), SoundCountBeforeFriendly);
			PlayerASC->RemoveLooseGameplayTag(DeadTag);
		}

		// 3.7 Null ReceivedHitSound (graceful silence, camera shake still plays)
		PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 500.0f);
		AdvancePlayerDefenseAudioTestWorld(World, 0.25f);
		FGameplayTagContainer LaunchTags;
		LaunchTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Data.Reaction.Launch")), false));
		Player->SetTestReceivedHitSound(nullptr);
		const int32 ShakeCountBeforeNull = Player->GetTestHitFeedbackCameraShakeStartCount();
		const bool bAppliedNullSound = ApplyDamageSpec(EnemyASC, PlayerASC, Enemy, &LaunchTags, nullptr);
		TestTrue(TEXT("Damage with null sound asset applied"), bAppliedNullSound);
		TestEqual(TEXT("ReceivedHitSound count unchanged with null sound"), Player->GetTestReceivedHitSoundDispatchCount(), SoundCountBeforeFriendly);
		TestTrue(TEXT("Overlay flash still triggered with null sound"), Player->IsTestHitFeedbackOverlayActive());
		TestEqual(TEXT("Camera shake still triggered with null sound"), Player->GetTestHitFeedbackCameraShakeStartCount(), ShakeCountBeforeNull + 1);
		Player->SetTestReceivedHitSound(TestReceivedHitSound);
		AdvancePlayerDefenseAudioTestWorld(World, 0.25f);
	}

	// =========================================================================
	// SECTION 4: Deduplication & Multi-Modifier within Same GE Spec vs Independent
	// =========================================================================
	{
		PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 800.0f);
		const int32 SoundCountBeforeDedupe = Player->GetTestReceivedHitSoundDispatchCount();

		// 4.1 Single GE Spec containing TWO Health Modifiers -> Exactly 1 hit sound
		FGameplayEffectContextHandle MultiModContext = EnemyASC->MakeEffectContext();
		MultiModContext.AddInstigator(Enemy, Enemy);

		UGameplayEffect* TransientMultiModGE = NewObject<UGameplayEffect>(GetTransientPackage());
		TransientMultiModGE->DurationPolicy = EGameplayEffectDurationType::Instant;
		FGameplayModifierInfo Mod1;
		Mod1.Attribute = UCharacterAttributeSet::GetHealthAttribute();
		Mod1.ModifierOp = EGameplayModOp::Additive;
		Mod1.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(-10.0f));
		TransientMultiModGE->Modifiers.Add(Mod1);

		FGameplayModifierInfo Mod2;
		Mod2.Attribute = UCharacterAttributeSet::GetHealthAttribute();
		Mod2.ModifierOp = EGameplayModOp::Additive;
		Mod2.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(-15.0f));
		TransientMultiModGE->Modifiers.Add(Mod2);

		FGameplayEffectSpec MultiSpec(TransientMultiModGE, MultiModContext, 1.0f);
		const bool bAppliedMulti = PlayerASC->ApplyGameplayEffectSpecToSelf(MultiSpec).WasSuccessfullyApplied();
		TestTrue(TEXT("Multi-modifier GE applied"), bAppliedMulti);
		TestEqual(TEXT("Multi-modifier GE triggered ReceivedHitSound exactly ONCE"), Player->GetTestReceivedHitSoundDispatchCount(), SoundCountBeforeDedupe + 1);

		// 4.2 Two independent GE Specs (even reusing the same Context) -> Each triggers 1 hit sound
		const int32 SoundCountBeforeIndependent = Player->GetTestReceivedHitSoundDispatchCount();
		FGameplayEffectContextHandle SharedContext = EnemyASC->MakeEffectContext();
		SharedContext.AddInstigator(Enemy, Enemy);

		FGameplayEffectSpecHandle SpecA = EnemyASC->MakeOutgoingSpec(UTestProjectileDamageGE::StaticClass(), 1.0f, SharedContext);
		FGameplayEffectSpecHandle SpecB = EnemyASC->MakeOutgoingSpec(UTestProjectileDamageGE::StaticClass(), 1.0f, SharedContext);
		if (SpecA.IsValid() && SpecA.Data.IsValid() && SpecB.IsValid() && SpecB.Data.IsValid())
		{
			PlayerASC->ApplyGameplayEffectSpecToSelf(*SpecA.Data.Get());
			TestEqual(TEXT("Spec A triggered hit sound"), Player->GetTestReceivedHitSoundDispatchCount(), SoundCountBeforeIndependent + 1);

			PlayerASC->ApplyGameplayEffectSpecToSelf(*SpecB.Data.Get());
			TestEqual(TEXT("Spec B triggered hit sound"), Player->GetTestReceivedHitSoundDispatchCount(), SoundCountBeforeIndependent + 2);
		}
	}

	// =========================================================================
	// SECTION 5: Lifecycle & Safety Verification
	// =========================================================================
	{
		// EndPlay & unpossess safety
		PlayerASC->ClearAllAbilities();
		if (Controller)
		{
			Controller->UnPossess();
		}
		TestTrue(TEXT("Teardown succeeded cleanly without crash"), true);
	}

	return true;
}

#endif
