#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/PlayerGuardAbility.h"
#include "AbilitySystem/Abilities/PlayerParryAbility.h"
#include "AbilitySystem/CharacterAttributeSet.h"
#include "Camera/CameraShakeBase.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Character/Player/PlayerCharacter.h"
#include "Combat/Feedback/CombatFeedbackDataAsset.h"
#include "Combat/Melee/MeleeHitResolver.h"
#include "Combat/Projectile/CombatProjectileHitResolver.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Framework/PolyQuestPlayerController.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "Tests/CombatAutomationFixture.h"
#include "Tests/TestHitFeedbackCameraShake.h"
#include "Tests/TestParryCounterPoiseGE.h"
#include "Tests/TestProjectileDamageGE.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FParrySuccessImpactFeedbackAutomationTest,
	"PolyQuest.Combat.ParrySuccessFeedback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	struct FWorldCleanup
	{
		UWorld* World = nullptr;
		~FWorldCleanup()
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

	void AdvanceTestTimer(UWorld* World, float DeltaSeconds)
	{
		constexpr float MaxTickStepSeconds = 0.05f;
		while (DeltaSeconds > KINDA_SMALL_NUMBER)
		{
			const float TickStep = FMath::Min(DeltaSeconds, MaxTickStepSeconds);
			TickTestWorld(World, TickStep);
			DeltaSeconds -= TickStep;
		}
	}
}

bool FParrySuccessImpactFeedbackAutomationTest::RunTest(const FString& Parameters)
{
	if (!TestNotNull(TEXT("Engine is available"), GEngine))
	{
		return false;
	}

	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("ParrySuccessFeedbackTestWorld"));
	WorldContext.SetCurrentWorld(World);
	FWorldCleanup Cleanup{ World };

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

	// Baseline attributes
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

	// Setup transient Parry ability on Player
	const FGameplayTag ParryAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Defense.Parry")), false);
	const FGameplayTag ParryingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Parrying")), false);
	TestTrue(TEXT("Parry ability tag is valid"), ParryAbilityTag.IsValid());
	TestTrue(TEXT("Parrying state tag is valid"), ParryingStateTag.IsValid());

	FGameplayAbilitySpec ParrySpec(UPlayerParryAbility::StaticClass(), 1, INDEX_NONE, Player);
	ParrySpec.DynamicAbilityTags.AddTag(ParryAbilityTag);
	const FGameplayAbilitySpecHandle ParrySpecHandle = PlayerASC->GiveAbility(ParrySpec);
	TestTrue(TEXT("Parry spec handle is valid"), ParrySpecHandle.IsValid());

	FGameplayAbilitySpec* AbilitySpec = PlayerASC->FindAbilitySpecFromHandle(ParrySpecHandle);
	UPlayerParryAbility* TestParryAbility = AbilitySpec ? Cast<UPlayerParryAbility>(AbilitySpec->GetPrimaryInstance()) : nullptr;
	if (!TestNotNull(TEXT("Primary parry ability instance created"), TestParryAbility))
	{
		return false;
	}

	TestParryAbility->SetTestParryCounterPoiseGameplayEffectClass(UTestParryCounterPoiseGE::StaticClass());
	TestParryAbility->SetTestParryPoiseDamage(100.0f);
	TestParryAbility->TestSetParryWindowOpen(true);
	PlayerASC->AddLooseGameplayTag(ParryingStateTag);

	USoundWave* TestSound = NewObject<USoundWave>(Player);
	UCombatFeedbackDataAsset* PlayerFeedback = Player->GetTestCombatFeedbackData();
	if (PlayerFeedback)
	{
		PlayerFeedback->Defense.ParrySuccessSound = TestSound;
		PlayerFeedback->Defense.ParrySuccessHitStopDurationSeconds = 0.05f;
		PlayerFeedback->Defense.ParrySuccessHitStopTimeDilation = 0.03f;
	}
	TestParryAbility->SetTestBypassAudioPlayback(true);

	// -------------------------------------------------------------------------
	// 1. SECTION: Successful Melee Parry
	// -------------------------------------------------------------------------
	const FVector NonZeroImpactPoint(100.0f, 0.0f, 50.0f);
	FHitResult ValidHitResult;
	ValidHitResult.HitObjectHandle = FActorInstanceHandle(Player);
	ValidHitResult.ImpactPoint = NonZeroImpactPoint;

	FMeleeHitRequest MeleeRequest;
	MeleeRequest.SourceActor = Enemy;
	MeleeRequest.SourceAbilitySystemComponent = EnemyASC;
	MeleeRequest.HitResult = ValidHitResult;
	MeleeRequest.DamageGameplayEffectClass = UTestProjectileDamageGE::StaticClass();
	MeleeRequest.GuardStaminaDamage = 20.0f;
	MeleeRequest.AbilityLevel = 1.0f;

	TestEqual(TEXT("Initial feedback count is 0"), TestParryAbility->GetTestParrySuccessFeedbackCount(), 0);
	TestEqual(TEXT("Initial camera shake count is 0"), Player->GetTestHitFeedbackCameraShakeStartCount(), 0);

	const bool bMeleeParried = FMeleeHitResolver::TryResolveHit(MeleeRequest);
	TestTrue(TEXT("Melee hit is successfully resolved via Parry"), bMeleeParried);

	// Verify player health & stamina untouched
	TestEqual(TEXT("Player health unchanged"), PlayerASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute()), 1000.0f);
	TestEqual(TEXT("Player stamina unchanged"), PlayerASC->GetNumericAttribute(UCharacterAttributeSet::GetStaminaAttribute()), 100.0f);

	// Verify enemy poise counter reduced by 100
	TestEqual(TEXT("Enemy poise reduced by counter GE"), EnemyASC->GetNumericAttribute(UCharacterAttributeSet::GetPoiseAttribute()), 0.0f);

	// Verify presentation feedback triggered
	TestEqual(TEXT("Parry feedback count incremented to 1"), TestParryAbility->GetTestParrySuccessFeedbackCount(), 1);
	TestTrue(TEXT("Controller hit-stop is active"), Controller->IsTestHitStopActive());
	TestTrue(TEXT("Global time dilation is set to 0.03"), FMath::IsNearlyEqual(UGameplayStatics::GetGlobalTimeDilation(World), 0.03f, KINDA_SMALL_NUMBER));

	// Verify camera shake
	TestEqual(TEXT("Big camera shake triggered"), Player->GetTestHitFeedbackCameraShakeStartCount(), 1);
	UCameraShakeBase* BigShake = Player->GetTestLastHitFeedbackCameraShake();
	TestNotNull(TEXT("Valid camera shake instance created"), BigShake);
	TestTrue(TEXT("Camera shake is Big tier class"), BigShake && BigShake->IsA<UTestBigHitFeedbackCameraShake>());

	// Verify sound dispatch location
	TestEqual(TEXT("Sound dispatched once"), TestParryAbility->GetTestParrySuccessSoundDispatchCount(), 1);
	TestEqual(TEXT("Sound dispatched at exact non-zero impact point"), TestParryAbility->GetTestLastParrySuccessSoundLocation(), NonZeroImpactPoint);

	// Advance time to restore Hit-Stop
	AdvanceTestTimer(World, 0.08f);
	TestFalse(TEXT("Hit-stop restored after duration"), Controller->IsTestHitStopActive());
	TestTrue(TEXT("Global time dilation restored to 1.0"), FMath::IsNearlyEqual(UGameplayStatics::GetGlobalTimeDilation(World), 1.0f, KINDA_SMALL_NUMBER));

	// -------------------------------------------------------------------------
	// 2. SECTION: Sound Location & Independent Fallbacks
	// -------------------------------------------------------------------------
	// A: Mismatched or default zero HitResult falls back to Player ActorLocation
	FHitResult ZeroHitResult;
	ZeroHitResult.HitObjectHandle = FActorInstanceHandle(Enemy); // mismatched actor
	ZeroHitResult.ImpactPoint = FVector::ZeroVector;

	EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetPoiseAttribute(), 100.0f);
	const bool bDirectParrySuccess = TestParryAbility->TryParryMeleeHit(Enemy, ZeroHitResult);
	TestTrue(TEXT("Direct parry call succeeded"), bDirectParrySuccess);
	TestEqual(TEXT("Feedback count incremented to 2"), TestParryAbility->GetTestParrySuccessFeedbackCount(), 2);
	TestEqual(TEXT("Sound count incremented to 2"), TestParryAbility->GetTestParrySuccessSoundDispatchCount(), 2);
	TestEqual(TEXT("Sound location fell back to Player ActorLocation"), TestParryAbility->GetTestLastParrySuccessSoundLocation(), Player->GetActorLocation());
	AdvanceTestTimer(World, 0.08f);

	// B: Null sound asset does not block hit-stop or camera shake
	if (PlayerFeedback)
	{
		PlayerFeedback->Defense.ParrySuccessSound = nullptr;
	}
	const int32 ShakeCountBeforeNullSound = Player->GetTestHitFeedbackCameraShakeStartCount();
	const int32 SoundCountBeforeNullSound = TestParryAbility->GetTestParrySuccessSoundDispatchCount();

	EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetPoiseAttribute(), 100.0f);
	TestTrue(TEXT("Parry with null sound succeeds"), TestParryAbility->TryParryMeleeHit(Enemy, ValidHitResult));
	TestEqual(TEXT("Camera shake still triggered without sound"), Player->GetTestHitFeedbackCameraShakeStartCount(), ShakeCountBeforeNullSound + 1);
	TestEqual(TEXT("Sound dispatch count unchanged with null sound"), TestParryAbility->GetTestParrySuccessSoundDispatchCount(), SoundCountBeforeNullSound);
	AdvanceTestTimer(World, 0.08f);

	// Restore sound asset for subsequent tests
	if (PlayerFeedback)
	{
		PlayerFeedback->Defense.ParrySuccessSound = TestSound;
	}

	// C: Invalid hit-stop duration/dilation fail-closed without breaking Parry
	if (PlayerFeedback)
	{
		PlayerFeedback->Defense.ParrySuccessHitStopDurationSeconds = -1.0f;
		PlayerFeedback->Defense.ParrySuccessHitStopTimeDilation = 0.0f;
	}
	EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetPoiseAttribute(), 100.0f);
	TestTrue(TEXT("Parry with invalid hit-stop parameters succeeds"), TestParryAbility->TryParryMeleeHit(Enemy, ValidHitResult));
	TestFalse(TEXT("Invalid hit-stop parameters did not activate hit-stop"), Controller->IsTestHitStopActive());

	// Restore valid hit-stop parameters
	if (PlayerFeedback)
	{
		PlayerFeedback->Defense.ParrySuccessHitStopDurationSeconds = 0.05f;
		PlayerFeedback->Defense.ParrySuccessHitStopTimeDilation = 0.05f;
	}

	// -------------------------------------------------------------------------
	// 3. SECTION: Existing Parry Gates (Rejections & Fail-closed)
	// -------------------------------------------------------------------------
	const int32 FeedbackCountBeforeRejections = TestParryAbility->GetTestParrySuccessFeedbackCount();

	// A: Closed window
	TestParryAbility->TestSetParryWindowOpen(false);
	TestFalse(TEXT("Closed parry window rejects contact"), TestParryAbility->TryParryMeleeHit(Enemy, ValidHitResult));
	TestEqual(TEXT("No feedback on closed window"), TestParryAbility->GetTestParrySuccessFeedbackCount(), FeedbackCountBeforeRejections);
	TestParryAbility->TestSetParryWindowOpen(true);

	// B: Wrong front arc (attacker behind player)
	Enemy->SetActorLocation(FVector(-200.0f, 0.0f, 0.0f));
	TestFalse(TEXT("Attacker behind player rejected by parry arc"), TestParryAbility->TryParryMeleeHit(Enemy, ValidHitResult));
	TestEqual(TEXT("No feedback on arc rejection"), TestParryAbility->GetTestParrySuccessFeedbackCount(), FeedbackCountBeforeRejections);
	Enemy->SetActorLocation(FVector(200.0f, 0.0f, 0.0f));

	// C: Null attacker
	TestFalse(TEXT("Null attacker rejected"), TestParryAbility->TryParryMeleeHit(nullptr, ValidHitResult));
	TestEqual(TEXT("No feedback on null attacker"), TestParryAbility->GetTestParrySuccessFeedbackCount(), FeedbackCountBeforeRejections);

	// D: Invalid local Parry configuration (missing counter GE)
	TestParryAbility->SetTestParryCounterPoiseGameplayEffectClass(nullptr);
	TestFalse(TEXT("Missing counter GE rejects parry"), TestParryAbility->TryParryMeleeHit(Enemy, ValidHitResult));
	TestEqual(TEXT("No feedback on missing counter GE"), TestParryAbility->GetTestParrySuccessFeedbackCount(), FeedbackCountBeforeRejections);
	TestParryAbility->SetTestParryCounterPoiseGameplayEffectClass(UTestParryCounterPoiseGE::StaticClass());

	// E: Attacker without ASC still succeeds and emits feedback (skips only counter)
	ACharacter* DummyAttackerWithoutASC = World->SpawnActor<ACharacter>();
	DummyAttackerWithoutASC->SetActorLocation(FVector(200.0f, 0.0f, 0.0f));
	const int32 FeedbackCountBeforeDummy = TestParryAbility->GetTestParrySuccessFeedbackCount();
	TestTrue(TEXT("Attacker without ASC still yields valid parry success"), TestParryAbility->TryParryMeleeHit(DummyAttackerWithoutASC, ValidHitResult));
	TestEqual(TEXT("Feedback count incremented for attacker without ASC"), TestParryAbility->GetTestParrySuccessFeedbackCount(), FeedbackCountBeforeDummy + 1);
	AdvanceTestTimer(World, 0.08f);
	DummyAttackerWithoutASC->Destroy();

	// -------------------------------------------------------------------------
	// 4. SECTION: Projectile Separation & Guard Regression
	// -------------------------------------------------------------------------
	// With active Parry window open, projectile contact MUST NOT parry
	FCombatProjectileHitRequest ProjectileRequest;
	ProjectileRequest.SourceActor = Enemy;
	ProjectileRequest.SourceAbilitySystemComponent = EnemyASC;
	ProjectileRequest.HitResult = ValidHitResult;
	ProjectileRequest.DamageGameplayEffectClass = UTestProjectileDamageGE::StaticClass();
	ProjectileRequest.GuardStaminaDamage = 20.0f;
	ProjectileRequest.AbilityLevel = 1.0f;

	const int32 FeedbackCountBeforeProjectile = TestParryAbility->GetTestParrySuccessFeedbackCount();
	PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 1000.0f);

	const bool bProjectileResolved = FCombatProjectileHitResolver::TryResolveHit(ProjectileRequest);
	TestTrue(TEXT("Projectile hit is resolved by damage application"), bProjectileResolved);
	TestEqual(TEXT("Projectile did NOT trigger Parry feedback"), TestParryAbility->GetTestParrySuccessFeedbackCount(), FeedbackCountBeforeProjectile);
	TestTrue(TEXT("Player received projectile damage"), PlayerASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute()) < 1000.0f);

	// -------------------------------------------------------------------------
	// 5. SECTION: Hit-Stop Lifecycle & Cleanup Safety
	// -------------------------------------------------------------------------
	// A: Normal expiry restores dilation
	TestParryAbility->TryParryMeleeHit(Enemy, ValidHitResult);
	TestTrue(TEXT("Hit-stop active before expiry"), Controller->IsTestHitStopActive());
	AdvanceTestTimer(World, 0.08f);
	TestFalse(TEXT("Hit-stop expired and restored"), Controller->IsTestHitStopActive());
	TestTrue(TEXT("Global dilation 1.0 after normal expiry"), FMath::IsNearlyEqual(UGameplayStatics::GetGlobalTimeDilation(World), 1.0f, KINDA_SMALL_NUMBER));

	// B: Controller Destroy / EndPlay hit-stop cleanup while active (isolated controller)
	APolyQuestPlayerController* DestroyTestController = World->SpawnActor<APolyQuestPlayerController>();
	DestroyTestController->DispatchBeginPlay();
	DestroyTestController->SetAsLocalPlayerController();
	DestroyTestController->RequestCombatImpactHitStop(0.05f, 0.05f);
	TestTrue(TEXT("Destroy test controller active hit-stop"), DestroyTestController->IsTestHitStopActive());
	TestTrue(TEXT("Global time dilation is 0.05 while hit-stop active"), FMath::IsNearlyEqual(UGameplayStatics::GetGlobalTimeDilation(World), 0.05f, KINDA_SMALL_NUMBER));
	const bool bControllerDestroyed = DestroyTestController->Destroy();
	TestTrue(TEXT("Controller Destroy succeeded"), bControllerDestroyed);
	TestTrue(TEXT("Global time dilation restored to 1.0 on Controller Destroy"), FMath::IsNearlyEqual(UGameplayStatics::GetGlobalTimeDilation(World), 1.0f, KINDA_SMALL_NUMBER));

	Player->Destroy();
	Enemy->Destroy();
	Controller->Destroy();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
