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
#include "GameFramework/PlayerController.h"
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

	bool ApplyDamage(UAbilitySystemComponent* SourceASC, UAbilitySystemComponent* TargetASC, const FGameplayTagContainer* DynamicTags = nullptr)
	{
		if (!SourceASC || !TargetASC)
		{
			return false;
		}

		FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
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
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	if (!TestNotNull(TEXT("Player fixture created"), Player) || !TestNotNull(TEXT("Enemy fixture created"), Enemy) || !TestNotNull(TEXT("Local PlayerController created"), Controller))
	{
		return false;
	}
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
	UMaterialInterface* EnemyBaseline = Enemy->GetMesh()->GetOverlayMaterial();

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

	EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 25.0f);
	TestTrue(TEXT("Lethal Enemy Health GE applies"), ApplyDamage(SourceASC, EnemyASC));
	TestTrue(TEXT("Lethal Enemy enters its existing Dead state"), Enemy->IsDead());
	TestFalse(TEXT("Lethal Enemy damage does not flash"), Enemy->IsTestHitFeedbackOverlayActive());
	TestTrue(TEXT("Dead Enemy Health GE applies"), ApplyDamage(SourceASC, EnemyASC));
	TestFalse(TEXT("Dead Enemy does not flash"), Enemy->IsTestHitFeedbackOverlayActive());

	// 10. Destroy / EndPlay stops active shake and clears state.
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

	Enemy->Destroy();
	return true;
}

#endif
