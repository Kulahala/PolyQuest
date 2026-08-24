#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "AbilitySystem/CharacterAttributeSet.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Character/Player/PlayerCharacter.h"
#include "Engine/Engine.h"
#include "EngineGlobals.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Tests/CombatAutomationFixture.h"
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

	PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxHealthAttribute(), 100.0f);
	PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 100.0f);
	EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetMaxHealthAttribute(), 100.0f);
	EnemyASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 100.0f);
	UMaterialInterface* PlayerBaseline = Player->GetMesh()->GetOverlayMaterial();
	UMaterialInterface* EnemyBaseline = Enemy->GetMesh()->GetOverlayMaterial();

	TestTrue(TEXT("Nonlethal Player Health GE applies"), ApplyDamage(SourceASC, PlayerASC));
	TestTrue(TEXT("Player flash becomes active"), Player->IsTestHitFeedbackOverlayActive());
	TestTrue(TEXT("Player overlay uses the configured flash material"), Player->GetTestActiveHitFeedbackOverlayMaterial() == Player->GetMesh()->GetOverlayMaterial());
	TestEqual(TEXT("First Player hit starts one camera shake"), Player->GetTestHitFeedbackCameraShakeStartCount(), 1);
	UCameraShakeBase* FirstShake = Player->GetTestLastHitFeedbackCameraShake();

	TickHitFeedbackTestWorld(World, 0.06f);
	TestTrue(TEXT("Second nonlethal Player Health GE applies"), ApplyDamage(SourceASC, PlayerASC));
	TestEqual(TEXT("Repeated Player hit starts the same configured channel"), Player->GetTestHitFeedbackCameraShakeStartCount(), 2);
	TestTrue(TEXT("Repeated Player hit reuses the active single-instance shake"), FirstShake && Player->GetTestLastHitFeedbackCameraShake() == FirstShake);
	TickHitFeedbackTestWorld(World, 0.06f);
	TestTrue(TEXT("Repeated hit refresh keeps flash active past first expiry"), Player->IsTestHitFeedbackOverlayActive());
	TestTrue(TEXT("Repeated hit refresh keeps timer valid"), Player->HasTestHitFeedbackOverlayTimer());
	AdvanceHitFeedbackTimer(World, 0.15f);
	TestFalse(TEXT("Refreshed flash expires after the refreshed duration"), Player->IsTestHitFeedbackOverlayActive());

	TestTrue(TEXT("Nonlethal Enemy Health GE applies"), ApplyDamage(SourceASC, EnemyASC));
	TestTrue(TEXT("Enemy flash becomes active"), Enemy->IsTestHitFeedbackOverlayActive());
	TestTrue(TEXT("Enemy overlay uses the configured flash material"), Enemy->GetTestActiveHitFeedbackOverlayMaterial() == Enemy->GetMesh()->GetOverlayMaterial());

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

	// Flashing is independent from reaction classification and Stunned/Poise suppression.
	FGameplayTagContainer InvalidReactionTags;
	InvalidReactionTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Data.Reaction.Small")), false));
	InvalidReactionTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Data.Reaction.Big")), false));
	TestTrue(TEXT("Invalid multi-tier damage applies"), ApplyDamage(SourceASC, PlayerASC, &InvalidReactionTags));
	TestTrue(TEXT("Invalid multi-tier damage still flashes"), Player->IsTestHitFeedbackOverlayActive());
	AdvanceHitFeedbackTimer(World, 0.25f);
	PlayerASC->AddLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false));
	TestTrue(TEXT("Stunned damage applies"), ApplyDamage(SourceASC, PlayerASC));
	TestTrue(TEXT("Stunned damage still flashes"), Player->IsTestHitFeedbackOverlayActive());
	PlayerASC->RemoveLooseGameplayTag(FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false));
	AdvanceHitFeedbackTimer(World, 0.25f);
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
	PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 100.0f);
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

	PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 100.0f);
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

	PlayerASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 100.0f);
	TestTrue(TEXT("Active flash setup before Destroy applies"), ApplyDamage(SourceASC, PlayerASC));
	UMaterialInstanceDynamic* DestroyRestoreOverlay = UMaterialInstanceDynamic::Create(UMaterial::GetDefaultMaterial(MD_Surface), Player);
	Player->GetMesh()->SetOverlayMaterial(DestroyRestoreOverlay);
	Player->Destroy();
	TestFalse(TEXT("Destroy clears active feedback state"), Player->IsTestHitFeedbackOverlayActive());
	TestFalse(TEXT("Destroy clears feedback timer"), Player->HasTestHitFeedbackOverlayTimer());
	TestTrue(TEXT("Destroy restores cached external overlay"), Player->GetMesh()->GetOverlayMaterial() == DestroyRestoreOverlay);

	Enemy->Destroy();
	return true;
}

#endif
