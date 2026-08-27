#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "AbilitySystem/CharacterAttributeSet.h"
#include "Camera/CameraShakeBase.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Character/Player/PlayerCharacter.h"
#include "Engine/Engine.h"
#include "EngineGlobals.h"
#include "Engine/World.h"
#include "Framework/PolyQuestPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Tests/CombatAutomationFixture.h"
#include "Tests/TestHitFeedbackCameraShake.h"
#include "Tests/TestPoiseRecoveryGE.h"
#include "Tests/TestProjectileDamageGE.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatHitFeedbackAutomationTest, "PolyQuest.Combat.HitFeedback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

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

	void TickHitFeedbackTestWorld(UWorld* World, const float DeltaSeconds)
	{
		if (World)
		{
			World->Tick(ELevelTick::LEVELTICK_All, DeltaSeconds);
			++GFrameCounter;
		}
	}

	void AdvanceHitFeedbackTimer(UWorld* World, float DeltaSeconds)
	{
		constexpr float MaxTickStepSeconds = 0.05f;
		while (DeltaSeconds > KINDA_SMALL_NUMBER)
		{
			const float TickStep = FMath::Min(DeltaSeconds, MaxTickStepSeconds);
			TickHitFeedbackTestWorld(World, TickStep);
			DeltaSeconds -= TickStep;
		}
	}

	bool ApplyDamage(
		UAbilitySystemComponent* SourceASC,
		UAbilitySystemComponent* TargetASC,
		const FGameplayTagContainer* DynamicTags = nullptr,
		const FHitResult* HitResult = nullptr)
	{
		if (!SourceASC || !TargetASC)
		{
			return false;
		}

		FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
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

bool FCombatHitFeedbackAutomationTest::RunTest(const FString& Parameters)
{
	if (!TestNotNull(TEXT("Engine is available for the hit feedback fixture"), GEngine))
	{
		return false;
	}

	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("CombatHitFeedbackTestWorld"));
	WorldContext.SetCurrentWorld(World);
	FWorldCleanup Cleanup{ World };
	if (!TestNotNull(TEXT("Test World created"), World))
	{
		return false;
	}

	FURL WorldURL;
	World->InitializeActorsForPlay(WorldURL);
	World->BeginPlay();
	APlayerCharacter* Player = FCombatAutomationFixture::SpawnPlayer(World, FTransform(FRotator::ZeroRotator, FVector::ZeroVector));
	AEnemyCharacter* Enemy = FCombatAutomationFixture::SpawnPassiveEnemy(World, FTransform(FRotator::ZeroRotator, FVector(250.0f, 0.0f, 0.0f)));
	APolyQuestPlayerController* Controller = World->SpawnActor<APolyQuestPlayerController>();
	if (!TestNotNull(TEXT("Player fixture created"), Player) || !TestNotNull(TEXT("Enemy fixture created"), Enemy) || !TestNotNull(TEXT("Local PlayerController created"), Controller))
	{
		return false;
	}
	Controller->DispatchBeginPlay();
	Controller->SetAsLocalPlayerController();
	Controller->Possess(Player);
	TestTrue(TEXT("Controller is local for Player camera feedback"), Controller->IsLocalController());
	TestNotNull(TEXT("Controller has a PlayerCameraManager for shake feedback"), Controller->PlayerCameraManager.Get());

	UAbilitySystemComponent* SourceASC = Player->GetAbilitySystemComponent();
	UAbilitySystemComponent* PlayerASC = Player->GetAbilitySystemComponent();
	UAbilitySystemComponent* EnemyASC = Enemy->GetAbilitySystemComponent();
	TestNotNull(TEXT("Player ASC available"), PlayerASC);
	TestNotNull(TEXT("Enemy ASC available"), EnemyASC);
	if (!PlayerASC || !EnemyASC)
	{
		return false;
	}

	const FGameplayTag SmallReactionTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Data.Reaction.Small")), false);
	const FGameplayTag BigReactionTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Data.Reaction.Big")), false);
	const FGameplayTag LaunchReactionTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Data.Reaction.Launch")), false);
	const FGameplayTag StunnedTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);

	PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxHealthAttribute(), 1000.0f);
	PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 1000.0f);
	EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxHealthAttribute(), 1000.0f);
	EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 1000.0f);
	UMaterialInterface* PlayerBaseline = Player->GetMesh()->GetOverlayMaterial();

	// 1. None tier hit -> overlay flashes, but no camera shake.
	TestTrue(TEXT("Nonlethal Player Health GE applies"), ApplyDamage(SourceASC, PlayerASC));
	TestTrue(TEXT("Player flash becomes active"), Player->IsTestHitFeedbackOverlayActive());
	TestTrue(TEXT("Player overlay uses the configured flash material"), Player->GetTestActiveHitFeedbackOverlayMaterial() == Player->GetMesh()->GetOverlayMaterial());
	TestEqual(TEXT("None tier hit does not increment shake start count"), Player->GetTestHitFeedbackCameraShakeStartCount(), 0);
	TestNull(TEXT("None tier hit leaves active camera shake null"), Player->GetTestActiveHitFeedbackCameraShake());

	// 2. Same-tier restart keeps overlay active and valid, shake remains null.
	TickHitFeedbackTestWorld(World, 0.06f);
	TestTrue(TEXT("Second nonlethal Player Health GE applies"), ApplyDamage(SourceASC, PlayerASC));
	TestEqual(TEXT("Repeated None tier hit does not increment shake count"), Player->GetTestHitFeedbackCameraShakeStartCount(), 0);
	TestNull(TEXT("Active shake remains null"), Player->GetTestActiveHitFeedbackCameraShake());
	TickHitFeedbackTestWorld(World, 0.06f);
	TestTrue(TEXT("Repeated hit refresh keeps flash active past first expiry"), Player->IsTestHitFeedbackOverlayActive());
	TestTrue(TEXT("Repeated hit refresh keeps timer valid"), Player->HasTestHitFeedbackOverlayTimer());
	AdvanceHitFeedbackTimer(World, 0.15f);
	TestFalse(TEXT("Refreshed flash expires after the refreshed duration"), Player->IsTestHitFeedbackOverlayActive());

	// 3. Small tier selects Small test shake.
	FGameplayTagContainer SmallTags;
	SmallTags.AddTag(SmallReactionTag);
	TestTrue(TEXT("Small tier Health GE applies"), ApplyDamage(SourceASC, PlayerASC, &SmallTags));
	TestEqual(TEXT("Small tier hit increments shake count"), Player->GetTestHitFeedbackCameraShakeStartCount(), 1);
	UCameraShakeBase* SmallShake = Player->GetTestLastHitFeedbackCameraShake();
	TestNotNull(TEXT("Small tier started a valid shake instance"), SmallShake);
	TestTrue(TEXT("Small tier selects Small test shake class"), SmallShake && SmallShake->IsA<UTestSmallHitFeedbackCameraShake>());
	TestTrue(TEXT("Small shake is active"), SmallShake && SmallShake->IsActive());
	TestTrue(TEXT("Player active shake matches Small shake"), Player->GetTestActiveHitFeedbackCameraShake() == SmallShake);

	// 4. Repeated Small tier reuses single-instance Small shake.
	TickHitFeedbackTestWorld(World, 0.06f);
	TestTrue(TEXT("Repeated Small tier Health GE applies"), ApplyDamage(SourceASC, PlayerASC, &SmallTags));
	TestEqual(TEXT("Repeated Small tier increments shake count"), Player->GetTestHitFeedbackCameraShakeStartCount(), 2);
	TestTrue(TEXT("Repeated Small tier reuses active Small shake"), SmallShake && Player->GetTestLastHitFeedbackCameraShake() == SmallShake);
	TestTrue(TEXT("Small shake remains active after same-tier hit"), SmallShake && SmallShake->IsActive());
	AdvanceHitFeedbackTimer(World, 0.25f);

	// 5. Big tier selects Big test shake and stops Small shake.
	FGameplayTagContainer BigTags;
	BigTags.AddTag(BigReactionTag);
	TestTrue(TEXT("Big tier Health GE applies"), ApplyDamage(SourceASC, PlayerASC, &BigTags));
	TestEqual(TEXT("Big tier hit increments shake count"), Player->GetTestHitFeedbackCameraShakeStartCount(), 3);
	UCameraShakeBase* BigShake = Player->GetTestLastHitFeedbackCameraShake();
	TestNotNull(TEXT("Big tier started a valid shake instance"), BigShake);
	TestTrue(TEXT("Big tier selects Big test shake class"), BigShake && BigShake->IsA<UTestBigHitFeedbackCameraShake>());
	TestTrue(TEXT("Big shake is active"), BigShake && BigShake->IsActive());
	TestFalse(TEXT("Small shake stopped on transition to Big tier"), SmallShake && SmallShake->IsActive());
	TestTrue(TEXT("Player active shake matches Big shake"), Player->GetTestActiveHitFeedbackCameraShake() == BigShake);
	AdvanceHitFeedbackTimer(World, 0.25f);

	// 6. Launch tier selects Launch test shake and stops Big shake.
	FGameplayTagContainer LaunchTags;
	LaunchTags.AddTag(LaunchReactionTag);
	TestTrue(TEXT("Launch tier Health GE applies"), ApplyDamage(SourceASC, PlayerASC, &LaunchTags));
	TestEqual(TEXT("Launch tier hit increments shake count"), Player->GetTestHitFeedbackCameraShakeStartCount(), 4);
	UCameraShakeBase* LaunchShake = Player->GetTestLastHitFeedbackCameraShake();
	TestNotNull(TEXT("Launch tier started a valid shake instance"), LaunchShake);
	TestTrue(TEXT("Launch tier selects Launch test shake class"), LaunchShake && LaunchShake->IsA<UTestLaunchHitFeedbackCameraShake>());
	TestTrue(TEXT("Launch shake is active"), LaunchShake && LaunchShake->IsActive());
	TestFalse(TEXT("Big shake stopped on transition to Launch tier"), BigShake && BigShake->IsActive());
	TestTrue(TEXT("Player active shake matches Launch shake"), Player->GetTestActiveHitFeedbackCameraShake() == LaunchShake);
	AdvanceHitFeedbackTimer(World, 0.25f);

	// 7. Enemy damage does not increase Player shake count.
	const int32 PlayerShakeCountBeforeEnemyHit = Player->GetTestHitFeedbackCameraShakeStartCount();
	TestTrue(TEXT("Nonlethal Enemy Health GE applies"), ApplyDamage(SourceASC, EnemyASC));
	TestTrue(TEXT("Enemy flash becomes active"), Enemy->IsTestHitFeedbackOverlayActive());
	TestTrue(TEXT("Enemy overlay uses the configured flash material"), Enemy->GetTestActiveHitFeedbackOverlayMaterial() == Enemy->GetMesh()->GetOverlayMaterial());
	TestEqual(TEXT("Enemy damage does not increase Player shake count"), Player->GetTestHitFeedbackCameraShakeStartCount(), PlayerShakeCountBeforeEnemyHit);

	// Direct base writes do not carry GEModData and must not start a new flash.
	AdvanceHitFeedbackTimer(World, 0.25f);
	TestFalse(TEXT("Player flash timer expires"), Player->IsTestHitFeedbackOverlayActive());
	TestFalse(TEXT("Enemy flash timer expires"), Enemy->IsTestHitFeedbackOverlayActive());
	TestTrue(TEXT("Player baseline overlay is restored"), Player->GetMesh()->GetOverlayMaterial() == PlayerBaseline);
	PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 40.0f);
	TestFalse(TEXT("Direct Health base damage does not flash"), Player->IsTestHitFeedbackOverlayActive());
	PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 90.0f);
	TestFalse(TEXT("Direct Health base healing does not flash"), Player->IsTestHitFeedbackOverlayActive());

	// A real positive Health GameplayEffect is also a non-damage path and must not flash.
	PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 90.0f);
	UGameplayEffect* HealingEffect = NewObject<UGameplayEffect>(Player);
	HealingEffect->DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayModifierInfo HealingModifier;
	HealingModifier.Attribute = UCharacterAttributeSet::GetHealthAttribute();
	HealingModifier.ModifierOp = EGameplayModOp::Additive;
	HealingModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(5.0f));
	HealingEffect->Modifiers.Add(HealingModifier);
	FGameplayEffectContextHandle HealingContext = SourceASC->MakeEffectContext();
	TestTrue(TEXT("Positive Health GameplayEffect applies"), PlayerASC->ApplyGameplayEffectToSelf(HealingEffect, 1.0f, HealingContext).WasSuccessfullyApplied());
	TestFalse(TEXT("Positive Health GameplayEffect does not flash"), Player->IsTestHitFeedbackOverlayActive());

	// 8. Invalid multi-tier tags flash overlay but do not change active tier shake.
	PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 1000.0f);
	FGameplayTagContainer InvalidReactionTags;
	InvalidReactionTags.AddTag(SmallReactionTag);
	InvalidReactionTags.AddTag(BigReactionTag);
	TestTrue(TEXT("Invalid multi-tier damage applies"), ApplyDamage(SourceASC, PlayerASC, &InvalidReactionTags));
	TestTrue(TEXT("Invalid multi-tier damage still flashes"), Player->IsTestHitFeedbackOverlayActive());
	TestEqual(TEXT("Invalid multi-tier damage does not increment shake start count"), Player->GetTestHitFeedbackCameraShakeStartCount(), PlayerShakeCountBeforeEnemyHit);
	TestTrue(TEXT("Active Launch shake remains active through invalid multi-tier hit"), LaunchShake && LaunchShake->IsActive());
	TestTrue(TEXT("Player active shake remains Launch shake"), Player->GetTestActiveHitFeedbackCameraShake() == LaunchShake);
	AdvanceHitFeedbackTimer(World, 0.25f);

	// 9. Stunned Player receives tier-matched shake.
	PlayerASC->AddLooseGameplayTag(StunnedTag);
	TestTrue(TEXT("Stunned damage with Big tier applies"), ApplyDamage(SourceASC, PlayerASC, &BigTags));
	TestTrue(TEXT("Stunned damage still flashes"), Player->IsTestHitFeedbackOverlayActive());
	TestEqual(TEXT("Stunned Player damage increments shake start count"), Player->GetTestHitFeedbackCameraShakeStartCount(), PlayerShakeCountBeforeEnemyHit + 1);
	UCameraShakeBase* StunnedBigShake = Player->GetTestLastHitFeedbackCameraShake();
	TestNotNull(TEXT("Stunned damage started a valid shake instance"), StunnedBigShake);
	TestTrue(TEXT("Stunned damage selects Big tier camera shake class"), StunnedBigShake && StunnedBigShake->IsA<UTestBigHitFeedbackCameraShake>());
	TestTrue(TEXT("Stunned Big shake is active"), StunnedBigShake && StunnedBigShake->IsActive());
	TestFalse(TEXT("Previous Launch shake stopped when Big tier started in Stunned"), LaunchShake && LaunchShake->IsActive());
	PlayerASC->RemoveLooseGameplayTag(StunnedTag);
	AdvanceHitFeedbackTimer(World, 0.25f);

	// Enemy Poise-broken and Poise-only feedback checks.
	Enemy->BeginLaunchStanceBreakDeferral();
	EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetPoiseAttribute(), 0.0f);
	TestTrue(TEXT("Poise-broken Enemy damage applies"), ApplyDamage(SourceASC, EnemyASC));
	TestTrue(TEXT("Poise-broken Enemy damage still flashes"), Enemy->IsTestHitFeedbackOverlayActive());
	Enemy->AbortLaunchStanceBreakDeferral();
	AdvanceHitFeedbackTimer(World, 0.25f);

	EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetPoiseAttribute(), 100.0f);
	EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxPoiseAttribute(), 100.0f);
	Enemy->BeginLaunchStanceBreakDeferral();
	FGameplayEffectContextHandle PoiseContext = SourceASC->MakeEffectContext();
	FGameplayEffectSpecHandle PoiseSpec = SourceASC->MakeOutgoingSpec(UTestPoiseDamageOnlyGE::StaticClass(), 1.0f, PoiseContext);
	TestTrue(TEXT("Poise-only GameplayEffect applies"), PoiseSpec.IsValid() && PoiseSpec.Data.IsValid() && EnemyASC->ApplyGameplayEffectSpecToSelf(*PoiseSpec.Data.Get()).WasSuccessfullyApplied());
	TestFalse(TEXT("Poise-only GameplayEffect does not flash"), Enemy->IsTestHitFeedbackOverlayActive());
	Enemy->AbortLaunchStanceBreakDeferral();

	// An external owner may replace the overlay while B1 is active; expiry must preserve it.
	TestTrue(TEXT("External-overlay setup damage applies"), ApplyDamage(SourceASC, PlayerASC));
	UMaterialInstanceDynamic* ExternalOverlay = UMaterialInstanceDynamic::Create(UMaterial::GetDefaultMaterial(MD_Surface), Player);
	Player->GetMesh()->SetOverlayMaterial(ExternalOverlay);
	AdvanceHitFeedbackTimer(World, 0.25f);
	TestFalse(TEXT("External overlay does not leave B1 flash active"), Player->IsTestHitFeedbackOverlayActive());
	TestTrue(TEXT("External overlay is preserved after B1 expiry"), Player->GetMesh()->GetOverlayMaterial() == ExternalOverlay);

	// A later hit adopts the external value as the new restoration target.
	PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 1000.0f);
	TestTrue(TEXT("External-overlay second setup damage applies"), ApplyDamage(SourceASC, PlayerASC));
	UMaterialInstanceDynamic* ExternalOverlayAfterHit = UMaterialInstanceDynamic::Create(UMaterial::GetDefaultMaterial(MD_Surface), Player);
	Player->GetMesh()->SetOverlayMaterial(ExternalOverlayAfterHit);
	TickHitFeedbackTestWorld(World, 0.05f);
	TestTrue(TEXT("Second hit after external replacement applies"), ApplyDamage(SourceASC, PlayerASC));
	AdvanceHitFeedbackTimer(World, 0.25f);
	TestTrue(TEXT("Later hit restores external overlay captured during the active flash"), Player->GetMesh()->GetOverlayMaterial() == ExternalOverlayAfterHit);

	// A lethal application is rejected before the overlay trigger.
	PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 25.0f);
	TestTrue(TEXT("Lethal Health GE applies"), ApplyDamage(SourceASC, PlayerASC));
	TestFalse(TEXT("Lethal Health damage does not flash"), Player->IsTestHitFeedbackOverlayActive());

	PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 1000.0f);
	PlayerASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false));
	TestTrue(TEXT("Dead Player Health GE applies"), ApplyDamage(SourceASC, PlayerASC));
	TestFalse(TEXT("Dead Player does not flash"), Player->IsTestHitFeedbackOverlayActive());
	PlayerASC->RemoveLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false));

	// -------------------------------------------------------------------------
	// SECTION: C3K Combat Impact Feedback Coverage
	// -------------------------------------------------------------------------

	// Re-initialize healthy baseline
	PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 1000.0f);
	EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 1000.0f);
	Controller->TriggerTestRestoreCombatImpactHitStop();
	UGameplayStatics::SetGlobalTimeDilation(World, 1.0f);
	TestFalse(TEXT("Controller hit-stop initially inactive"), Controller->IsTestHitStopActive());
	TestEqual(TEXT("Initial global time dilation is 1.0"), UGameplayStatics::GetGlobalTimeDilation(World), 1.0f);

	// 10. Small Preset Mapping
	const int32 InitialHitStopCount = Enemy->GetTestCombatImpactHitStopRequestCount();
	TestTrue(TEXT("Player hits Enemy with Small reaction tier"), ApplyDamage(SourceASC, EnemyASC, &SmallTags));
	TestEqual(TEXT("Small hit requests hit-stop"), Enemy->GetTestCombatImpactHitStopRequestCount(), InitialHitStopCount + 1);
	TestEqual(TEXT("Small hit selects 0.02s duration"), Enemy->GetTestLastImpactHitStopDuration(), 0.02f);
	TestEqual(TEXT("Small hit selects 0.15 dilation"), Enemy->GetTestLastImpactHitStopTimeDilation(), 0.15f);
	TestTrue(TEXT("Controller hit-stop is active after Small hit"), Controller->IsTestHitStopActive());
	TestTrue(TEXT("Global time dilation is set to 0.15"), FMath::IsNearlyEqual(UGameplayStatics::GetGlobalTimeDilation(World), 0.15f, KINDA_SMALL_NUMBER));

	// Tick across real-time duration and verify controller restoration via Tick()
	AdvanceHitFeedbackTimer(World, 0.05f);
	TestFalse(TEXT("Hit-stop expires after real-time duration"), Controller->IsTestHitStopActive());
	TestTrue(TEXT("Global time dilation restored to 1.0"), FMath::IsNearlyEqual(UGameplayStatics::GetGlobalTimeDilation(World), 1.0f, KINDA_SMALL_NUMBER));

	// 11. Big Preset Mapping
	TestTrue(TEXT("Player hits Enemy with Big reaction tier"), ApplyDamage(SourceASC, EnemyASC, &BigTags));
	TestEqual(TEXT("Big hit selects 0.05s duration"), Enemy->GetTestLastImpactHitStopDuration(), 0.05f);
	TestEqual(TEXT("Big hit selects 0.05 dilation"), Enemy->GetTestLastImpactHitStopTimeDilation(), 0.05f);
	TestTrue(TEXT("Global time dilation is set to 0.05"), FMath::IsNearlyEqual(UGameplayStatics::GetGlobalTimeDilation(World), 0.05f, KINDA_SMALL_NUMBER));
	AdvanceHitFeedbackTimer(World, 0.08f);
	TestFalse(TEXT("Big hit-stop expires"), Controller->IsTestHitStopActive());
	TestTrue(TEXT("Global time dilation restored to 1.0 after Big hit"), FMath::IsNearlyEqual(UGameplayStatics::GetGlobalTimeDilation(World), 1.0f, KINDA_SMALL_NUMBER));

	// 12. Launch Preset Mapping
	TestTrue(TEXT("Player hits Enemy with Launch reaction tier"), ApplyDamage(SourceASC, EnemyASC, &LaunchTags));
	TestEqual(TEXT("Launch hit selects 0.04s duration"), Enemy->GetTestLastImpactHitStopDuration(), 0.04f);
	TestEqual(TEXT("Launch hit selects 0.05 dilation"), Enemy->GetTestLastImpactHitStopTimeDilation(), 0.05f);
	TestTrue(TEXT("Global time dilation is set to 0.05 for Launch"), FMath::IsNearlyEqual(UGameplayStatics::GetGlobalTimeDilation(World), 0.05f, KINDA_SMALL_NUMBER));
	AdvanceHitFeedbackTimer(World, 0.08f);
	TestFalse(TEXT("Launch hit-stop expires"), Controller->IsTestHitStopActive());
	TestTrue(TEXT("Global time dilation restored to 1.0 after Launch hit"), FMath::IsNearlyEqual(UGameplayStatics::GetGlobalTimeDilation(World), 1.0f, KINDA_SMALL_NUMBER));

	// 13. None Tier uses Small preset for C3K
	TestTrue(TEXT("Player hits Enemy with None reaction tier"), ApplyDamage(SourceASC, EnemyASC));
	TestEqual(TEXT("None tier hit selects Small preset duration 0.02s"), Enemy->GetTestLastImpactHitStopDuration(), 0.02f);
	TestEqual(TEXT("None tier hit selects Small preset dilation 0.15"), Enemy->GetTestLastImpactHitStopTimeDilation(), 0.15f);
	TestTrue(TEXT("Global time dilation set to 0.15 for None tier"), FMath::IsNearlyEqual(UGameplayStatics::GetGlobalTimeDilation(World), 0.15f, KINDA_SMALL_NUMBER));
	AdvanceHitFeedbackTimer(World, 0.05f);
	TestFalse(TEXT("None tier hit-stop expires"), Controller->IsTestHitStopActive());

	// 14. Invalid multi-tier uses Small preset for C3K while warning/skipping reaction event
	TestTrue(TEXT("Player hits Enemy with Invalid multi-tier"), ApplyDamage(SourceASC, EnemyASC, &InvalidReactionTags));
	TestEqual(TEXT("Invalid multi-tier selects Small preset duration 0.02s"), Enemy->GetTestLastImpactHitStopDuration(), 0.02f);
	TestEqual(TEXT("Invalid multi-tier selects Small preset dilation 0.15"), Enemy->GetTestLastImpactHitStopTimeDilation(), 0.15f);
	TestTrue(TEXT("Global time dilation set to 0.15 for Invalid tier"), FMath::IsNearlyEqual(UGameplayStatics::GetGlobalTimeDilation(World), 0.15f, KINDA_SMALL_NUMBER));
	AdvanceHitFeedbackTimer(World, 0.05f);
	TestFalse(TEXT("Invalid tier hit-stop expires"), Controller->IsTestHitStopActive());

	// 15. Stunned Enemy receives C3K feedback before early return
	EnemyASC->AddLooseGameplayTag(StunnedTag);
	TestTrue(TEXT("Stunned Enemy hit with Big reaction tier"), ApplyDamage(SourceASC, EnemyASC, &BigTags));
	TestEqual(TEXT("Stunned Enemy hit selects Big preset duration"), Enemy->GetTestLastImpactHitStopDuration(), 0.05f);
	TestEqual(TEXT("Stunned Enemy hit selects Big preset dilation"), Enemy->GetTestLastImpactHitStopTimeDilation(), 0.05f);
	TestTrue(TEXT("Stunned Enemy hit activates hit-stop"), Controller->IsTestHitStopActive());
	EnemyASC->RemoveLooseGameplayTag(StunnedTag);
	AdvanceHitFeedbackTimer(World, 0.08f);

	// 16. Rapid hit monotonic arbitration:
	// 16.1 Big followed by Small: Dilation stays at 0.05 (min), expiry extends to max
	TestTrue(TEXT("Rapid hit 1: Big hit applies"), ApplyDamage(SourceASC, EnemyASC, &BigTags));
	const double BigExpiry = Controller->GetTestHitStopExpireRealTimeSeconds();
	TestTrue(TEXT("Rapid hit 2: Small hit applies immediately"), ApplyDamage(SourceASC, EnemyASC, &SmallTags));
	TestTrue(TEXT("Dilation stays at 0.05 after subsequent weaker Small hit"), FMath::IsNearlyEqual(UGameplayStatics::GetGlobalTimeDilation(World), 0.05f, KINDA_SMALL_NUMBER));
	TestTrue(TEXT("Expiry is at least the Big hit expiry"), Controller->GetTestHitStopExpireRealTimeSeconds() >= BigExpiry);
	AdvanceHitFeedbackTimer(World, 0.08f);
	TestFalse(TEXT("Rapid hit stop expires completely"), Controller->IsTestHitStopActive());
	TestTrue(TEXT("Global time dilation restored after rapid hit sequence"), FMath::IsNearlyEqual(UGameplayStatics::GetGlobalTimeDilation(World), 1.0f, KINDA_SMALL_NUMBER));

	// 16.2 Small followed by Big: Dilation drops to 0.05 (min), expiry updates to later
	TestTrue(TEXT("Rapid hit 3: Small hit applies"), ApplyDamage(SourceASC, EnemyASC, &SmallTags));
	TestTrue(TEXT("Dilation starts at 0.15"), FMath::IsNearlyEqual(UGameplayStatics::GetGlobalTimeDilation(World), 0.15f, KINDA_SMALL_NUMBER));
	TestTrue(TEXT("Rapid hit 4: Big hit applies immediately"), ApplyDamage(SourceASC, EnemyASC, &BigTags));
	TestTrue(TEXT("Dilation decreases to 0.05 after stronger Big hit"), FMath::IsNearlyEqual(UGameplayStatics::GetGlobalTimeDilation(World), 0.05f, KINDA_SMALL_NUMBER));
	AdvanceHitFeedbackTimer(World, 0.08f);
	TestFalse(TEXT("Arbitrated hit stop expires"), Controller->IsTestHitStopActive());

	// 17. External Time Dilation Override & Preservation
	TestTrue(TEXT("External override setup: Big hit applies"), ApplyDamage(SourceASC, EnemyASC, &BigTags));
	TestTrue(TEXT("Hit-stop active at 0.05"), Controller->IsTestHitStopActive());
	// External system overrides global dilation to 0.5
	UGameplayStatics::SetGlobalTimeDilation(World, 0.5f);
	// Single Tick detects override and relinquishes C3K ownership without overwriting 0.5
	TickHitFeedbackTestWorld(World, 0.01f);
	TestFalse(TEXT("Controller hit-stop relinquished after external dilation change"), Controller->IsTestHitStopActive());
	TestTrue(TEXT("External dilation 0.5 is preserved"), FMath::IsNearlyEqual(UGameplayStatics::GetGlobalTimeDilation(World), 0.5f, KINDA_SMALL_NUMBER));

	// Subsequent C3K hit captures 0.5 as new baseline and restores to 0.5
	TestTrue(TEXT("New Small hit applies under external 0.5 baseline"), ApplyDamage(SourceASC, EnemyASC, &SmallTags));
	TestEqual(TEXT("Captured pre-hit-stop baseline is 0.5"), Controller->GetTestPreHitStopGlobalTimeDilation(), 0.5f);
	TestTrue(TEXT("Dilation temporarily set to 0.15"), FMath::IsNearlyEqual(UGameplayStatics::GetGlobalTimeDilation(World), 0.15f, KINDA_SMALL_NUMBER));
	AdvanceHitFeedbackTimer(World, 0.05f);
	TestFalse(TEXT("Hit-stop expires"), Controller->IsTestHitStopActive());
	TestTrue(TEXT("Global time dilation restored to external baseline 0.5"), FMath::IsNearlyEqual(UGameplayStatics::GetGlobalTimeDilation(World), 0.5f, KINDA_SMALL_NUMBER));
	// Reset back to standard 1.0
	UGameplayStatics::SetGlobalTimeDilation(World, 1.0f);

	// 18. Sound and Blood HitResult Routing & Normal Alignment
	// 18.1 Valid HitResult targeting Enemy
	const int32 SoundCountBefore = Enemy->GetTestImpactSoundDispatchCount();
	const int32 BloodCountBefore = Enemy->GetTestImpactBloodDispatchCount();

	FHitResult ValidHit;
	ValidHit.HitObjectHandle = FActorInstanceHandle(Enemy);
	ValidHit.ImpactPoint = FVector(250.0f, 10.0f, 85.0f);
	ValidHit.ImpactNormal = FVector(0.0f, 0.0f, 1.0f);
	TestTrue(TEXT("Damage with valid Enemy HitResult applies"), ApplyDamage(SourceASC, EnemyASC, &SmallTags, &ValidHit));
	TestEqual(TEXT("Sound dispatch count incremented"), Enemy->GetTestImpactSoundDispatchCount(), SoundCountBefore + 1);
	TestEqual(TEXT("Sound uses exact ImpactPoint"), Enemy->GetTestLastImpactSoundLocation(), FVector(250.0f, 10.0f, 85.0f));
	TestEqual(TEXT("Blood dispatch count incremented"), Enemy->GetTestImpactBloodDispatchCount(), BloodCountBefore + 1);
	TestEqual(TEXT("Blood uses exact ImpactPoint"), Enemy->GetTestLastImpactBloodLocation(), FVector(250.0f, 10.0f, 85.0f));
	TestEqual(TEXT("Blood normal is normalized Z vector"), Enemy->GetTestLastImpactBloodNormal(), FVector(0.0f, 0.0f, 1.0f));
	const FRotator ExpectedZRot = FRotationMatrix::MakeFromZ(FVector(0.0f, 0.0f, 1.0f)).Rotator();
	TestTrue(TEXT("Blood rotation aligns local +Z to impact normal"), Enemy->GetTestLastImpactBloodRotation().Equals(ExpectedZRot, 1e-2f));
	AdvanceHitFeedbackTimer(World, 0.05f);

	// 18.2 Non-unit diagonal normal is safely normalized
	FHitResult DiagonalHit;
	DiagonalHit.HitObjectHandle = FActorInstanceHandle(Enemy);
	DiagonalHit.ImpactPoint = FVector(260.0f, 20.0f, 90.0f);
	DiagonalHit.ImpactNormal = FVector(50.0f, 0.0f, 50.0f);
	TestTrue(TEXT("Damage with diagonal HitResult applies"), ApplyDamage(SourceASC, EnemyASC, &SmallTags, &DiagonalHit));
	const FVector ExpectedDiagNormal = FVector(50.0f, 0.0f, 50.0f).GetSafeNormal();
	TestTrue(TEXT("Blood normal safely normalized"), Enemy->GetTestLastImpactBloodNormal().Equals(ExpectedDiagNormal, 1e-4f));
	const FRotator ExpectedDiagRot = FRotationMatrix::MakeFromZ(ExpectedDiagNormal).Rotator();
	TestTrue(TEXT("Blood rotation matches normalized diagonal matrix"), Enemy->GetTestLastImpactBloodRotation().Equals(ExpectedDiagRot, 1e-2f));
	AdvanceHitFeedbackTimer(World, 0.05f);

	// 18.3 Missing HitResult: Sound falls back to Enemy actor location, Blood skipped
	const int32 BloodCountBeforeMissing = Enemy->GetTestImpactBloodDispatchCount();
	const int32 SoundCountBeforeMissing = Enemy->GetTestImpactSoundDispatchCount();
	TestTrue(TEXT("Damage with no HitResult applies"), ApplyDamage(SourceASC, EnemyASC, &SmallTags, nullptr));
	TestEqual(TEXT("Sound dispatches on missing HitResult"), Enemy->GetTestImpactSoundDispatchCount(), SoundCountBeforeMissing + 1);
	TestEqual(TEXT("Sound location falls back to Enemy ActorLocation"), Enemy->GetTestLastImpactSoundLocation(), Enemy->GetActorLocation());
	TestEqual(TEXT("Blood is skipped on missing HitResult"), Enemy->GetTestImpactBloodDispatchCount(), BloodCountBeforeMissing);
	TestTrue(TEXT("Hit-stop still functions without HitResult"), Controller->IsTestHitStopActive());
	AdvanceHitFeedbackTimer(World, 0.05f);

	// 18.4 Zero Normal: Sound dispatches, Blood skipped
	const int32 BloodCountBeforeZeroNorm = Enemy->GetTestImpactBloodDispatchCount();
	FHitResult ZeroNormHit;
	ZeroNormHit.HitObjectHandle = FActorInstanceHandle(Enemy);
	ZeroNormHit.ImpactPoint = FVector(250.0f, 10.0f, 85.0f);
	ZeroNormHit.ImpactNormal = FVector::ZeroVector;
	TestTrue(TEXT("Damage with zero-normal HitResult applies"), ApplyDamage(SourceASC, EnemyASC, &SmallTags, &ZeroNormHit));
	TestEqual(TEXT("Blood skipped on zero normal"), Enemy->GetTestImpactBloodDispatchCount(), BloodCountBeforeZeroNorm);
	TestTrue(TEXT("Hit-stop still functions with zero normal"), Controller->IsTestHitStopActive());
	AdvanceHitFeedbackTimer(World, 0.05f);

	// 18.5 HitResult targeting Player (Mismatched Actor): Blood skipped
	const int32 BloodCountBeforeMismatch = Enemy->GetTestImpactBloodDispatchCount();
	FHitResult MismatchedHit;
	MismatchedHit.HitObjectHandle = FActorInstanceHandle(Player);
	MismatchedHit.ImpactPoint = FVector(0.0f, 0.0f, 50.0f);
	MismatchedHit.ImpactNormal = FVector(1.0f, 0.0f, 0.0f);
	TestTrue(TEXT("Damage with mismatched HitResult applies to Enemy"), ApplyDamage(SourceASC, EnemyASC, &SmallTags, &MismatchedHit));
	TestEqual(TEXT("Blood skipped when HitResult actor is not Enemy"), Enemy->GetTestImpactBloodDispatchCount(), BloodCountBeforeMismatch);
	TestTrue(TEXT("Hit-stop still functions with mismatched HitResult"), Controller->IsTestHitStopActive());
	AdvanceHitFeedbackTimer(World, 0.05f);

	// 18.6 Multi-modifier GE deduplication: single GE with two negative Health modifiers produces exactly one feedback burst
	const int32 MultiModSoundBefore = Enemy->GetTestImpactSoundDispatchCount();
	const int32 MultiModBloodBefore = Enemy->GetTestImpactBloodDispatchCount();
	const int32 MultiModHitStopBefore = Enemy->GetTestCombatImpactHitStopRequestCount();

	UGameplayEffect* MultiModGE = NewObject<UGameplayEffect>(GetTransientPackage());
	MultiModGE->DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo HealthMod1;
	HealthMod1.Attribute = UCharacterAttributeSet::GetHealthAttribute();
	HealthMod1.ModifierOp = EGameplayModOp::Additive;
	HealthMod1.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(-5.0f));
	MultiModGE->Modifiers.Add(HealthMod1);

	FGameplayModifierInfo HealthMod2;
	HealthMod2.Attribute = UCharacterAttributeSet::GetHealthAttribute();
	HealthMod2.ModifierOp = EGameplayModOp::Additive;
	HealthMod2.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(-5.0f));
	MultiModGE->Modifiers.Add(HealthMod2);

	FGameplayEffectContextHandle MultiModContext = SourceASC->MakeEffectContext();
	MultiModContext.AddHitResult(ValidHit, true);
	FGameplayEffectSpec MultiModSpec(MultiModGE, MultiModContext, 1.0f);
	MultiModSpec.AppendDynamicAssetTags(SmallTags);

	TestTrue(TEXT("Multi-modifier GE applied successfully"), EnemyASC->ApplyGameplayEffectSpecToSelf(MultiModSpec).WasSuccessfullyApplied());
	TestEqual(TEXT("Sound count incremented exactly once for multi-modifier GE"), Enemy->GetTestImpactSoundDispatchCount(), MultiModSoundBefore + 1);
	TestEqual(TEXT("Blood count incremented exactly once for multi-modifier GE"), Enemy->GetTestImpactBloodDispatchCount(), MultiModBloodBefore + 1);
	TestEqual(TEXT("Hit-stop requested exactly once for multi-modifier GE"), Enemy->GetTestCombatImpactHitStopRequestCount(), MultiModHitStopBefore + 1);
	AdvanceHitFeedbackTimer(World, 0.05f);

	// 18.7 Two independent GE applications sharing the same EffectContext: each produces one feedback dispatch
	const int32 IndependentSoundBefore = Enemy->GetTestImpactSoundDispatchCount();
	const int32 IndependentBloodBefore = Enemy->GetTestImpactBloodDispatchCount();
	const int32 IndependentHitStopBefore = Enemy->GetTestCombatImpactHitStopRequestCount();

	FGameplayEffectContextHandle SharedContext = SourceASC->MakeEffectContext();
	SharedContext.AddHitResult(ValidHit, true);

	FGameplayEffectSpec IndependentSpec1(MultiModGE, SharedContext, 1.0f);
	IndependentSpec1.AppendDynamicAssetTags(SmallTags);
	FGameplayEffectSpec IndependentSpec2(MultiModGE, SharedContext, 1.0f);
	IndependentSpec2.AppendDynamicAssetTags(SmallTags);

	TestTrue(TEXT("Independent GE 1 applied successfully"), EnemyASC->ApplyGameplayEffectSpecToSelf(IndependentSpec1).WasSuccessfullyApplied());
	TestEqual(TEXT("Sound count incremented for independent GE 1"), Enemy->GetTestImpactSoundDispatchCount(), IndependentSoundBefore + 1);
	TestEqual(TEXT("Blood count incremented for independent GE 1"), Enemy->GetTestImpactBloodDispatchCount(), IndependentBloodBefore + 1);
	TestEqual(TEXT("Hit-stop requested for independent GE 1"), Enemy->GetTestCombatImpactHitStopRequestCount(), IndependentHitStopBefore + 1);

	TestTrue(TEXT("Independent GE 2 applied successfully with same context"), EnemyASC->ApplyGameplayEffectSpecToSelf(IndependentSpec2).WasSuccessfullyApplied());
	TestEqual(TEXT("Sound count incremented for independent GE 2"), Enemy->GetTestImpactSoundDispatchCount(), IndependentSoundBefore + 2);
	TestEqual(TEXT("Blood count incremented for independent GE 2"), Enemy->GetTestImpactBloodDispatchCount(), IndependentBloodBefore + 2);
	TestEqual(TEXT("Hit-stop requested for independent GE 2"), Enemy->GetTestCombatImpactHitStopRequestCount(), IndependentHitStopBefore + 2);
	AdvanceHitFeedbackTimer(World, 0.05f);

	// 19. Team / Source Exclusions
	// 19.1 Enemy attacking Player: Player receives overlay & camera shake, but NO C3K hit-stop, sound, or blood
	const int32 EnemyHitStopBeforePlayerHit = Enemy->GetTestCombatImpactHitStopRequestCount();
	const int32 EnemySoundBeforePlayerHit = Enemy->GetTestImpactSoundDispatchCount();
	const int32 EnemyBloodBeforePlayerHit = Enemy->GetTestImpactBloodDispatchCount();
	TestTrue(TEXT("Enemy-to-Player damage applies"), ApplyDamage(EnemyASC, PlayerASC, &BigTags));
	TestEqual(TEXT("Enemy hit-stop request count unchanged on Player damage"), Enemy->GetTestCombatImpactHitStopRequestCount(), EnemyHitStopBeforePlayerHit);
	TestEqual(TEXT("Enemy sound dispatch unchanged on Player damage"), Enemy->GetTestImpactSoundDispatchCount(), EnemySoundBeforePlayerHit);
	TestEqual(TEXT("Enemy blood dispatch unchanged on Player damage"), Enemy->GetTestImpactBloodDispatchCount(), EnemyBloodBeforePlayerHit);
	TestFalse(TEXT("Controller hit-stop not active on Player damage"), Controller->IsTestHitStopActive());
	AdvanceHitFeedbackTimer(World, 0.25f);

	// 19.2 Non-Player team instigator (Enemy-to-Enemy self hit): no C3K feedback
	TestTrue(TEXT("Enemy-to-Enemy damage applies"), ApplyDamage(EnemyASC, EnemyASC, &BigTags));
	TestEqual(TEXT("Enemy hit-stop request count unchanged on non-Player instigator"), Enemy->GetTestCombatImpactHitStopRequestCount(), EnemyHitStopBeforePlayerHit);
	TestEqual(TEXT("Enemy sound dispatch unchanged on non-Player instigator"), Enemy->GetTestImpactSoundDispatchCount(), EnemySoundBeforePlayerHit);
	TestEqual(TEXT("Enemy blood dispatch unchanged on non-Player instigator"), Enemy->GetTestImpactBloodDispatchCount(), EnemyBloodBeforePlayerHit);
	TestFalse(TEXT("Controller hit-stop not active on non-Player instigator"), Controller->IsTestHitStopActive());
	AdvanceHitFeedbackTimer(World, 0.25f);

	// 20. Lethal & Dead Enemy exclusions
	EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 25.0f);
	TestTrue(TEXT("Lethal Enemy Health GE applies"), ApplyDamage(SourceASC, EnemyASC));
	TestTrue(TEXT("Lethal Enemy enters its existing Dead state"), Enemy->IsDead());
	TestFalse(TEXT("Lethal Enemy damage does not flash"), Enemy->IsTestHitFeedbackOverlayActive());
	TestTrue(TEXT("Dead Enemy Health GE applies"), ApplyDamage(SourceASC, EnemyASC));
	TestFalse(TEXT("Dead Enemy does not flash"), Enemy->IsTestHitFeedbackOverlayActive());

	// 21. Player Destroy / EndPlay stops active shake and clears state
	PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 1000.0f);
	TestTrue(TEXT("Active flash setup before Destroy applies"), ApplyDamage(SourceASC, PlayerASC, &LaunchTags));
	UCameraShakeBase* ActiveShakeBeforeDestroy = Player->GetTestLastHitFeedbackCameraShake();
	TestNotNull(TEXT("Active shake exists before Destroy"), ActiveShakeBeforeDestroy);
	TestTrue(TEXT("Active shake is playing before Destroy"), ActiveShakeBeforeDestroy && ActiveShakeBeforeDestroy->IsActive());
	UMaterialInstanceDynamic* DestroyRestoreOverlay = UMaterialInstanceDynamic::Create(UMaterial::GetDefaultMaterial(MD_Surface), Player);
	Player->GetMesh()->SetOverlayMaterial(DestroyRestoreOverlay);
	Player->Destroy();
	TestFalse(TEXT("Player Destroy stops active camera shake"), ActiveShakeBeforeDestroy && ActiveShakeBeforeDestroy->IsActive());
	TestFalse(TEXT("Destroy clears active feedback state"), Player->IsTestHitFeedbackOverlayActive());
	TestFalse(TEXT("Destroy clears feedback timer"), Player->HasTestHitFeedbackOverlayTimer());
	TestTrue(TEXT("Destroy restores cached external overlay"), Player->GetMesh()->GetOverlayMaterial() == DestroyRestoreOverlay);

	// 22. Controller Destroy / EndPlay hit-stop cleanup while active
	APolyQuestPlayerController* DestroyTestController = World->SpawnActor<APolyQuestPlayerController>();
	DestroyTestController->DispatchBeginPlay();
	DestroyTestController->SetAsLocalPlayerController();
	DestroyTestController->RequestCombatImpactHitStop(0.05f, 0.05f);
	TestTrue(TEXT("Hit-stop is active at 0.05 before Controller Destroy"), DestroyTestController->IsTestHitStopActive());
	TestTrue(TEXT("Global time dilation is 0.05 while hit-stop active"), FMath::IsNearlyEqual(UGameplayStatics::GetGlobalTimeDilation(World), 0.05f, KINDA_SMALL_NUMBER));
	const bool bControllerDestroyed = DestroyTestController->Destroy();
	TestTrue(TEXT("Controller Destroy succeeded"), bControllerDestroyed);
	TestTrue(TEXT("Global time dilation restored to 1.0 on Controller Destroy / EndPlay"), FMath::IsNearlyEqual(UGameplayStatics::GetGlobalTimeDilation(World), 1.0f, KINDA_SMALL_NUMBER));

	// 23. Approximate but distinct external value preservation on Controller Destroy (difference 0.0005 > KINDA_SMALL_NUMBER 0.0001)
	APolyQuestPlayerController* SecondController = World->SpawnActor<APolyQuestPlayerController>();
	SecondController->DispatchBeginPlay();
	SecondController->SetAsLocalPlayerController();
	SecondController->RequestCombatImpactHitStop(0.05f, 0.05f);
	TestTrue(TEXT("Second Controller active hit-stop"), SecondController->IsTestHitStopActive());
	// External system modifies global dilation to an approximate but distinct value 0.0505
	UGameplayStatics::SetGlobalTimeDilation(World, 0.0505f);
	// Immediate Controller Destroy must preserve 0.0505 and not revert to 1.0
	SecondController->Destroy();
	TestTrue(TEXT("Global time dilation preserves external 0.0505 value after Controller Destroy"), FMath::IsNearlyEqual(UGameplayStatics::GetGlobalTimeDilation(World), 0.0505f, KINDA_SMALL_NUMBER));
	UGameplayStatics::SetGlobalTimeDilation(World, 1.0f);

	Enemy->Destroy();
	return true;
}

#endif
