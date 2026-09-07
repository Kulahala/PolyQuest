#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/ChargedAttackAbility.h"
#include "Character/Player/PlayerCharacter.h"
#include "Combat/Equipment/MeleeWeaponDefinition.h"
#include "Combat/Equipment/WeaponEquipmentComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameplayTagContainer.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Tests/CombatAutomationFixture.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FChargedAttackNiagaraFeedbackAutomationTest,
	"PolyQuest.Combat.ChargedAttackNiagaraFeedback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	struct FChargedAttackVFXTestWorldCleanup
	{
		UWorld* World = nullptr;
		~FChargedAttackVFXTestWorldCleanup()
		{
			if (World)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
		}
	};

	void TickChargedAttackTestWorld(UWorld* World, const float DeltaSeconds)
	{
		if (World)
		{
			World->Tick(ELevelTick::LEVELTICK_All, DeltaSeconds);
			++GFrameCounter;
		}
	}

	void AdvanceChargedAttackTimer(UWorld* World, float DeltaSeconds)
	{
		constexpr float MaxTickStepSeconds = 0.05f;
		while (DeltaSeconds > KINDA_SMALL_NUMBER)
		{
			const float TickStep = FMath::Min(DeltaSeconds, MaxTickStepSeconds);
			TickChargedAttackTestWorld(World, TickStep);
			DeltaSeconds -= TickStep;
		}
	}
}

bool FChargedAttackNiagaraFeedbackAutomationTest::RunTest(const FString& Parameters)
{
	if (!TestNotNull(TEXT("Engine is available"), GEngine))
	{
		return false;
	}

	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("ChargedAttackVFXTestWorld"));
	WorldContext.SetCurrentWorld(World);
	FChargedAttackVFXTestWorldCleanup Cleanup{ World };
	if (!TestNotNull(TEXT("Test World created successfully"), World))
	{
		return false;
	}

	FURL WorldURL;
	World->InitializeActorsForPlay(WorldURL);
	World->BeginPlay();

	APlayerCharacter* Player = FCombatAutomationFixture::SpawnPlayer(World);
	if (!TestNotNull(TEXT("Spawned test player"), Player))
	{
		return false;
	}

	USkeletalMesh* HeroMesh = Cast<USkeletalMesh>(StaticLoadObject(USkeletalMesh::StaticClass(), nullptr, TEXT("/Game/PolygonDungeons/Meshes/Characters/SK_Character_Hero_Knight_Male")));
	if (HeroMesh && Player->GetMesh())
	{
		Player->GetMesh()->SetSkeletalMesh(HeroMesh);
	}

	UStaticMesh* SwordMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, TEXT("/Game/PolygonDungeons/Meshes/Weapons/SM_Wep_Ornate_Sword_02")));
	if (!SwordMesh)
	{
		SwordMesh = NewObject<UStaticMesh>(GetTransientPackage(), TEXT("Test_FallbackSwordMesh"));
	}

	UWeaponEquipmentComponent* EquipmentComp = Player->FindComponentByClass<UWeaponEquipmentComponent>();
	if (!TestNotNull(TEXT("Player owns WeaponEquipmentComponent"), EquipmentComp))
	{
		return false;
	}

	UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
	if (!TestNotNull(TEXT("Player owns AbilitySystemComponent"), ASC))
	{
		return false;
	}

	const FGameplayTag PrimaryAttackInputTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Input.PrimaryAttack")), false);
	const FGameplayTag ChargedReleaseHandoffEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Attack.Charged.ReleaseHandoff")), false);
	const FGameplayTag ChargingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Charging")), false);
	TestTrue(TEXT("PrimaryAttack input tag is valid"), PrimaryAttackInputTag.IsValid());
	TestTrue(TEXT("ChargedReleaseHandoff event tag is valid"), ChargedReleaseHandoffEventTag.IsValid());
	TestTrue(TEXT("ChargingState tag is valid"), ChargingStateTag.IsValid());

	// Author test Unarmed definition with Weapon_R (default) and Weapon_L
	UMeleeWeaponDefinition* UnarmedDef = NewObject<UMeleeWeaponDefinition>(GetTransientPackage(), TEXT("Test_UnarmedDef"));
	UnarmedDef->HandSlot = EWeaponHandSlot::MainHandOneHanded;
	UnarmedDef->LocomotionMode = EWeaponLocomotionMode::Default;
	UnarmedDef->AttachSocketName = FName(TEXT("Weapon_R"));
	UnarmedDef->bUseOwnerMeshSocketForTrace = true;
	UnarmedDef->DefaultOwnerMeshTraceSourceName = FName(TEXT("RightFist"));

	FOwnerMeshMeleeTraceSource RightSource;
	RightSource.TraceSourceName = FName(TEXT("RightFist"));
	RightSource.OwnerMeshSocketName = FName(TEXT("Weapon_R"));
	RightSource.BladeBaseMarkerRelativeLocation = FVector(0.0f, 0.0f, 5.0f);
	RightSource.BladeTipMarkerRelativeLocation = FVector(0.0f, 0.0f, 20.0f);

	FOwnerMeshMeleeTraceSource LeftSource;
	LeftSource.TraceSourceName = FName(TEXT("Weapon_L"));
	LeftSource.OwnerMeshSocketName = FName(TEXT("Weapon_L"));
	LeftSource.BladeBaseMarkerRelativeLocation = FVector(0.0f, 0.0f, 5.0f);
	LeftSource.BladeTipMarkerRelativeLocation = FVector(0.0f, 0.0f, 20.0f);

	UnarmedDef->OwnerMeshTraceSources.Add(RightSource);
	UnarmedDef->OwnerMeshTraceSources.Add(LeftSource);
	UnarmedDef->PrimaryAttackAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Charged")), false);
	UnarmedDef->BaseGrantedActions.Add(UChargedAttackAbility::StaticClass());

	// Author test Display-mesh Sword definition
	UMeleeWeaponDefinition* SwordDef = NewObject<UMeleeWeaponDefinition>(GetTransientPackage(), TEXT("Test_SwordDef"));
	SwordDef->HandSlot = EWeaponHandSlot::MainHandOneHanded;
	SwordDef->LocomotionMode = EWeaponLocomotionMode::LightSword;
	SwordDef->AttachSocketName = FName(TEXT("Weapon_R"));
	SwordDef->WeaponMesh = SwordMesh;
	SwordDef->bUseOwnerMeshSocketForTrace = false;
	SwordDef->BladeBaseMarkerRelativeLocation = FVector(0.0f, 0.0f, 10.0f);
	SwordDef->BladeTipMarkerRelativeLocation = FVector(0.0f, 0.0f, 100.0f);
	SwordDef->PrimaryAttackAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Charged")), false);
	SwordDef->BaseGrantedActions.Add(UChargedAttackAbility::StaticClass());

	// Equip Unarmed definition
	TestTrue(TEXT("Equip Unarmed definition succeeds"), EquipmentComp->EquipWeapon(UnarmedDef));

	UNiagaraSystem* MockSystem = NewObject<UNiagaraSystem>(GetTransientPackage(), TEXT("Test_MockNiagaraSystem"));

	// -------------------------------------------------------------------------
	// SECTION 1: StartsGatherOnlyAfterConfirmedCharging & Release Handoff No-Op
	// -------------------------------------------------------------------------
	{
		UChargedAttackAbility* Ability = NewObject<UChargedAttackAbility>(Player, TEXT("Test_Section1_Ability"));
		Ability->SetTestCurrentActorInfo(ASC->AbilityActorInfo.Get());
		Ability->SetTestChargeVFXSystem(MockSystem);
		Ability->SetTestChargeVFXTraceSourceName(NAME_None);
		Ability->SetTestChargeVFXTrackingEnabled(true);
		Ability->SetTestMaximumChargeDuration(1.2f);

		// Case 1.1: Release handoff branch - SetCharging is not called; VFX must not start
		FGameplayEventData HandoffPayload;
		HandoffPayload.EventTag = ChargedReleaseHandoffEventTag;
		HandoffPayload.Instigator = Player;
		HandoffPayload.Target = Player;
		HandoffPayload.InstigatorTags.AddTag(PrimaryAttackInputTag);
		HandoffPayload.EventMagnitude = 0.5f;

		Ability->Test_BeginRelease(HandoffPayload.EventMagnitude);
		TestTrue(TEXT("Release handoff marked release started"), Ability->Test_IsReleaseStarted());
		TestEqual(TEXT("Release handoff did not call StartChargeFeedback"), Ability->GetTestStartChargeFeedbackCallCount(), 0);
		TestFalse(TEXT("Release handoff VFX is not active"), Ability->IsTestChargeVFXActive());

		// Case 1.2: Confirmed charging branch - Starts Gather (0.0f) exactly once
		UChargedAttackAbility* ChargingAbility = NewObject<UChargedAttackAbility>(Player, TEXT("Test_Section1_ChargingAbility"));
		ChargingAbility->SetTestCurrentActorInfo(ASC->AbilityActorInfo.Get());
		ChargingAbility->SetTestChargeVFXSystem(MockSystem);
		ChargingAbility->SetTestChargeVFXTraceSourceName(NAME_None);
		ChargingAbility->SetTestChargeVFXTrackingEnabled(true);
		ChargingAbility->SetTestMaximumChargeDuration(1.2f);

		ChargingAbility->Test_SetAbilityActive(true);
		ChargingAbility->Test_SetChargingStateApplied(true);
		ChargingAbility->Test_StartChargeFeedback();

		TestEqual(TEXT("StartChargeFeedback called once"), ChargingAbility->GetTestStartChargeFeedbackCallCount(), 1);
		TestTrue(TEXT("Charge VFX is active in Gather"), ChargingAbility->IsTestChargeVFXActive());
		TestEqual(TEXT("Charge phase is Gather (0.0f)"), ChargingAbility->GetTestRecordedChargePhase(), 0.0f);
		TestEqual(TEXT("Default source resolved to Weapon_R"), ChargingAbility->GetTestAttachSocketName(), FName(TEXT("Weapon_R")));

		ChargingAbility->Test_CleanupChargeFeedback();
	}

	// -------------------------------------------------------------------------
	// SECTION 2: UsesHeldDurationAndBypassesFullDelay
	// -------------------------------------------------------------------------
	{
		// Case 2.1: HeldDuration >= MaximumChargeDuration: Starts directly as Full, bypasses WaitDelay
		UChargedAttackAbility* FullAbility = NewObject<UChargedAttackAbility>(Player, TEXT("Test_Section2_FullAbility"));
		FullAbility->SetTestCurrentActorInfo(ASC->AbilityActorInfo.Get());
		FullAbility->SetTestChargeVFXSystem(MockSystem);
		FullAbility->SetTestChargeVFXTraceSourceName(NAME_None);
		FullAbility->SetTestChargeVFXTrackingEnabled(true);
		FullAbility->SetTestMaximumChargeDuration(0.5f);

		// Simulate input held for 0.6s (>= 0.5s)
		Player->TriggerTestHandleCombatInputStarted(PrimaryAttackInputTag);
		AdvanceChargedAttackTimer(World, 0.6f);

		FullAbility->Test_SetAbilityActive(true);
		FullAbility->Test_SetChargingStateApplied(true);
		FullAbility->Test_StartChargeFeedback();

		TestTrue(TEXT("Full charge VFX active"), FullAbility->IsTestChargeVFXActive());
		TestEqual(TEXT("Immediate phase is Full (1.0f)"), FullAbility->GetTestRecordedChargePhase(), 1.0f);
		TestNull(TEXT("WaitDelayTask was bypassed entirely"), FullAbility->GetTestWaitDelayTask());
		TestEqual(TEXT("Recorded delay duration is 0.0f"), FullAbility->GetTestDelayDuration(), 0.0f);

		FullAbility->Test_CleanupChargeFeedback();
		Player->TriggerTestHandleCombatInputEnded(PrimaryAttackInputTag, false);

		// Case 2.2: Partial charge with Delay ticked via World->Tick to Full
		UChargedAttackAbility* DelayAbility = NewObject<UChargedAttackAbility>(Player, TEXT("Test_Section2_DelayAbility"));
		DelayAbility->SetTestCurrentActorInfo(ASC->AbilityActorInfo.Get());
		DelayAbility->SetTestChargeVFXSystem(MockSystem);
		DelayAbility->SetTestChargeVFXTraceSourceName(NAME_None);
		DelayAbility->SetTestChargeVFXTrackingEnabled(true);
		DelayAbility->SetTestMaximumChargeDuration(0.8f);

		// Hold input for 0.2s; Remaining to full = 0.6s
		Player->TriggerTestHandleCombatInputStarted(PrimaryAttackInputTag);
		AdvanceChargedAttackTimer(World, 0.2f);

		DelayAbility->Test_SetAbilityActive(true);
		DelayAbility->Test_SetChargingStateApplied(true);
		DelayAbility->Test_StartChargeFeedback();

		TestTrue(TEXT("Delay ability VFX active"), DelayAbility->IsTestChargeVFXActive());
		TestEqual(TEXT("Initial phase is Gather (0.0f)"), DelayAbility->GetTestRecordedChargePhase(), 0.0f);
		TestNotNull(TEXT("WaitDelayTask created for remaining duration"), DelayAbility->GetTestWaitDelayTask());
		TestEqual(TEXT("Delay duration matches remaining (0.6s)"), DelayAbility->GetTestDelayDuration(), 0.6f);

		// Tick partially by 0.3s (total 0.3s < 0.6s remaining): Still Gather
		AdvanceChargedAttackTimer(World, 0.3f);
		TestEqual(TEXT("Phase remains Gather before threshold"), DelayAbility->GetTestRecordedChargePhase(), 0.0f);
		TestEqual(TEXT("No full callback fired yet"), DelayAbility->GetTestFullCallbackCount(), 0);

		// Tick remaining 0.5s (total 0.8s >= 0.6s remaining): Transitions to Full
		AdvanceChargedAttackTimer(World, 0.5f);
		TestEqual(TEXT("Full callback fired"), DelayAbility->GetTestFullCallbackCount(), 1);
		TestEqual(TEXT("Phase transitioned to Full (1.0f)"), DelayAbility->GetTestRecordedChargePhase(), 1.0f);

		DelayAbility->Test_CleanupChargeFeedback();
		Player->TriggerTestHandleCombatInputEnded(PrimaryAttackInputTag, false);
	}

	// -------------------------------------------------------------------------
	// SECTION 3: CleanupPreventsLateFullPhase
	// -------------------------------------------------------------------------
	{
		UChargedAttackAbility* CleanupAbility = NewObject<UChargedAttackAbility>(Player, TEXT("Test_Section3_CleanupAbility"));
		CleanupAbility->SetTestCurrentActorInfo(ASC->AbilityActorInfo.Get());
		CleanupAbility->SetTestChargeVFXSystem(MockSystem);
		CleanupAbility->SetTestChargeVFXTraceSourceName(NAME_None);
		CleanupAbility->SetTestChargeVFXTrackingEnabled(true);
		CleanupAbility->SetTestMaximumChargeDuration(1.0f);

		CleanupAbility->Test_SetAbilityActive(true);
		CleanupAbility->Test_SetChargingStateApplied(true);
		CleanupAbility->Test_StartChargeFeedback();
		TestTrue(TEXT("VFX active prior to cleanup"), CleanupAbility->IsTestChargeVFXActive());

		// Trigger cleanup
		CleanupAbility->Test_CleanupChargeFeedback();
		TestFalse(TEXT("VFX deactivated by cleanup"), CleanupAbility->IsTestChargeVFXActive());
		TestNull(TEXT("WaitDelayTask cleared by cleanup"), CleanupAbility->GetTestWaitDelayTask());
		TestEqual(TEXT("Cleanup call count is 1"), CleanupAbility->GetTestCleanupChargeFeedbackCallCount(), 1);

		// Simulate stale delay callback arriving after cleanup
		CleanupAbility->Test_SetChargingStateApplied(false);
		CleanupAbility->Test_SimulateChargeFullDelayFinished();

		TestFalse(TEXT("Stale callback does not reactivate VFX"), CleanupAbility->IsTestChargeVFXActive());
		TestEqual(TEXT("Full callback count was not incremented when uncharged"), CleanupAbility->GetTestFullCallbackCount(), 0);
	}

	// -------------------------------------------------------------------------
	// SECTION 4: ResolvesOwnerMeshSourcesFailClosed
	// -------------------------------------------------------------------------
	{
		USceneComponent* AttachParent = nullptr;
		FName AttachSocketName = NAME_None;

		// 4.1 NAME_None resolves to DefaultOwnerMeshTraceSourceName (Weapon_R)
		const bool bResolvedDefault = EquipmentComp->TryResolveMainHandChargeVFXAttachment(NAME_None, AttachParent, AttachSocketName);
		TestTrue(TEXT("NAME_None resolves default owner mesh source"), bResolvedDefault);
		TestEqual(TEXT("Default source resolved to Weapon_R"), AttachSocketName, FName(TEXT("Weapon_R")));
		TestEqual(TEXT("Attach parent is character mesh"), AttachParent, Cast<USceneComponent>(Player->GetMesh()));

		// 4.2 Explicit Weapon_L resolves to LeftSource (Weapon_L)
		AttachParent = nullptr;
		AttachSocketName = NAME_None;
		const bool bResolvedLeft = EquipmentComp->TryResolveMainHandChargeVFXAttachment(FName(TEXT("Weapon_L")), AttachParent, AttachSocketName);
		TestTrue(TEXT("Explicit Weapon_L resolves owner mesh source"), bResolvedLeft);
		TestEqual(TEXT("Explicit source resolved to Weapon_L"), AttachSocketName, FName(TEXT("Weapon_L")));

		// 4.3 Invalid explicit source fails closed and NEVER falls back to Weapon_R
		AttachParent = nullptr;
		AttachSocketName = NAME_None;
		const bool bResolvedInvalid = EquipmentComp->TryResolveMainHandChargeVFXAttachment(FName(TEXT("NonExistentTraceSource")), AttachParent, AttachSocketName);
		TestFalse(TEXT("Invalid explicit source returns false (fail-closed)"), bResolvedInvalid);
		TestNull(TEXT("OutAttachParent is null on fail-closed"), AttachParent);
		TestEqual(TEXT("OutAttachSocketName is NAME_None on fail-closed"), AttachSocketName, FName(NAME_None));

		// 4.4 Source with non-existent socket on skeletal mesh fails closed
		FOwnerMeshMeleeTraceSource MissingSocketSource;
		MissingSocketSource.TraceSourceName = FName(TEXT("MissingSocketSource"));
		MissingSocketSource.OwnerMeshSocketName = FName(TEXT("CompletelyFakeSocket_12345"));
		UnarmedDef->OwnerMeshTraceSources.Add(MissingSocketSource);

		AttachParent = nullptr;
		AttachSocketName = NAME_None;
		const bool bResolvedMissingSocket = EquipmentComp->TryResolveMainHandChargeVFXAttachment(FName(TEXT("MissingSocketSource")), AttachParent, AttachSocketName);
		TestFalse(TEXT("Source pointing to non-existent socket fails closed"), bResolvedMissingSocket);
		TestNull(TEXT("OutAttachParent is null when socket does not exist"), AttachParent);
	}

	// -------------------------------------------------------------------------
	// SECTION 5: ResolvesDisplayMeshRootAttachment
	// -------------------------------------------------------------------------
	{
		// Equip Sword definition (Display-mesh weapon)
		TestTrue(TEXT("Equip Sword definition succeeds"), EquipmentComp->EquipWeapon(SwordDef));

		USceneComponent* AttachParent = nullptr;
		FName AttachSocketName = NAME_None;

		// 5.1 NAME_None attaches to MainHandDisplayComponent root with NAME_None
		const bool bResolvedSwordDefault = EquipmentComp->TryResolveMainHandChargeVFXAttachment(NAME_None, AttachParent, AttachSocketName);
		TestTrue(TEXT("Display-mesh weapon resolves attachment"), bResolvedSwordDefault);
		TestNotNull(TEXT("Display-mesh attach parent is valid"), AttachParent);
		TestTrue(TEXT("Attach parent is UStaticMeshComponent"), AttachParent && AttachParent->IsA<UStaticMeshComponent>());
		TestEqual(TEXT("Attach socket name is NAME_None for display-mesh root"), AttachSocketName, FName(NAME_None));

		// 5.2 Explicit source name is ignored for display-mesh; still attaches to root with NAME_None
		AttachParent = nullptr;
		AttachSocketName = NAME_None;
		const bool bResolvedSwordExplicit = EquipmentComp->TryResolveMainHandChargeVFXAttachment(FName(TEXT("Weapon_L")), AttachParent, AttachSocketName);
		TestTrue(TEXT("Display-mesh resolves attachment with explicit source"), bResolvedSwordExplicit);
		TestNotNull(TEXT("Attach parent is valid"), AttachParent);
		TestEqual(TEXT("Attach socket remains NAME_None"), AttachSocketName, FName(NAME_None));
	}

	// -------------------------------------------------------------------------
	// SECTION 6: NullSystemAndSpawnFailurePreserveGameplay
	// -------------------------------------------------------------------------
	{
		// 6.1 Null ChargeVFXSystem does not block charging or release gameplay
		UChargedAttackAbility* NullSystemAbility = NewObject<UChargedAttackAbility>(Player, TEXT("Test_Section6_NullSystemAbility"));
		NullSystemAbility->SetTestCurrentActorInfo(ASC->AbilityActorInfo.Get());
		NullSystemAbility->SetTestChargeVFXSystem(nullptr);
		NullSystemAbility->SetTestChargeVFXTrackingEnabled(true);
		NullSystemAbility->SetTestMaximumChargeDuration(1.0f);

		NullSystemAbility->Test_SetAbilityActive(true);
		NullSystemAbility->Test_SetChargingStateApplied(true);
		NullSystemAbility->Test_StartChargeFeedback();

		TestFalse(TEXT("Null system leaves VFX inactive"), NullSystemAbility->IsTestChargeVFXActive());
		TestNull(TEXT("No WaitDelayTask created with null system"), NullSystemAbility->GetTestWaitDelayTask());

		NullSystemAbility->Test_BeginRelease(1.0f);
		TestTrue(TEXT("BeginRelease succeeds with null system"), NullSystemAbility->Test_IsReleaseStarted());
		TestTrue(TEXT("Damage multiplier calculated normally"), NullSystemAbility->Test_GetDamageMultiplier() >= 1.0f);

		NullSystemAbility->Test_CleanupChargeFeedback();

		// 6.2 Forced SpawnSystemAttached nullptr does not break gameplay
		UChargedAttackAbility* ForceNullAbility = NewObject<UChargedAttackAbility>(Player, TEXT("Test_Section6_ForceNullAbility"));
		ForceNullAbility->SetTestCurrentActorInfo(ASC->AbilityActorInfo.Get());
		ForceNullAbility->SetTestChargeVFXSystem(MockSystem);
		ForceNullAbility->SetTestChargeVFXTrackingEnabled(true);
		ForceNullAbility->SetTestForceSpawnNull(true);
		ForceNullAbility->SetTestMaximumChargeDuration(1.0f);

		ForceNullAbility->Test_SetAbilityActive(true);
		ForceNullAbility->Test_SetChargingStateApplied(true);
		ForceNullAbility->Test_StartChargeFeedback();

		TestFalse(TEXT("Forced null spawn leaves VFX inactive"), ForceNullAbility->IsTestChargeVFXActive());
		TestEqual(TEXT("Phase is -1.0f when spawn is forced null"), ForceNullAbility->GetTestRecordedChargePhase(), -1.0f);

		ForceNullAbility->Test_BeginRelease(0.5f);
		TestTrue(TEXT("BeginRelease succeeds when spawn returns null"), ForceNullAbility->Test_IsReleaseStarted());

		ForceNullAbility->Test_CleanupChargeFeedback();
		TestEqual(TEXT("Cleanup called cleanly without crashing"), ForceNullAbility->GetTestCleanupChargeFeedbackCallCount(), 1);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
