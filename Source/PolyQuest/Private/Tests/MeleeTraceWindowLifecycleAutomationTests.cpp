#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/MeleeTraceWindowLifecycle.h"
#include "AbilitySystem/Tasks/AbilityTask_MeleeTraceWindow.h"
#include "Animation/Combat/AnimNotifyState_ActionWindows.h"
#include "Character/Player/PlayerCharacter.h"
#include "Combat/Equipment/WeaponEquipmentComponent.h"
#include "Combat/Melee/MeleeWeaponTrailComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "Tests/CombatAutomationFixture.h"
#include "Tests/TestMeleeTrailAbility.h"
#include "Tests/TestProjectileDamageGE.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMeleeTraceWindowLifecycleAutomationTest,
	"PolyQuest.Melee.TraceWindowLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	struct FTraceLifecycleWorldCleanup
	{
		UWorld* World = nullptr;
		~FTraceLifecycleWorldCleanup()
		{
			if (World)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
			}
		}
	};
}

bool FMeleeTraceWindowLifecycleAutomationTest::RunTest(const FString& Parameters)
{
	if (!TestNotNull(TEXT("Engine is available for trace window lifecycle automation"), GEngine))
	{
		return false;
	}

	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("TraceLifecycleTestWorld"));
	WorldContext.SetCurrentWorld(World);
	FTraceLifecycleWorldCleanup Cleanup{ World };
	if (!TestNotNull(TEXT("Test World created successfully"), World))
	{
		return false;
	}

	FURL WorldURL;
	World->InitializeActorsForPlay(WorldURL);
	World->BeginPlay();

	APlayerCharacter* Player = FCombatAutomationFixture::SpawnPlayer(World, FTransform::Identity);
	if (!TestNotNull(TEXT("Player spawned successfully"), Player))
	{
		return false;
	}

	UMeleeWeaponTrailComponent* PlayerTrail = Player->FindComponentByClass<UMeleeWeaponTrailComponent>();
	if (!TestNotNull(TEXT("Player owns UMeleeWeaponTrailComponent"), PlayerTrail))
	{
		return false;
	}
	PlayerTrail->SetTestTrackingEnabled(true);

	UWeaponEquipmentComponent* EquipComp = Player->FindComponentByClass<UWeaponEquipmentComponent>();
	if (!TestNotNull(TEXT("Player owns UWeaponEquipmentComponent"), EquipComp))
	{
		return false;
	}

	USceneComponent* BladeBase = nullptr;
	USceneComponent* BladeTip = nullptr;
	TestTrue(TEXT("Resolve blade markers for default source"), EquipComp->TryGetBladeMarkers(BladeBase, BladeTip));
	if (!TestNotNull(TEXT("BladeBase marker is valid"), BladeBase) || !TestNotNull(TEXT("BladeTip marker is valid"), BladeTip))
	{
		return false;
	}

	UAbilitySystemComponent* PlayerASC = Player->GetAbilitySystemComponent();
	if (!TestNotNull(TEXT("Player ASC is valid"), PlayerASC))
	{
		return false;
	}

	FGameplayAbilitySpecHandle AbilityHandle = PlayerASC->GiveAbility(
		FGameplayAbilitySpec(UTestMeleeTrailAbility::StaticClass(), 1, INDEX_NONE, Player));
	TestTrue(TEXT("UTestMeleeTrailAbility given to Player ASC"), AbilityHandle.IsValid());

	const bool bActivated = PlayerASC->TryActivateAbility(AbilityHandle);
	TestTrue(TEXT("UTestMeleeTrailAbility activated successfully"), bActivated);

	FGameplayAbilitySpec* AbilitySpec = PlayerASC->FindAbilitySpecFromHandle(AbilityHandle);
	UTestMeleeTrailAbility* ActiveAbility = AbilitySpec ? Cast<UTestMeleeTrailAbility>(AbilitySpec->GetPrimaryInstance()) : nullptr;
	if (!TestNotNull(TEXT("Active ability instance found"), ActiveAbility))
	{
		return false;
	}

	// End only the bootstrap Task created in ActivateAbility to release its trail requester,
	// while keeping ActiveAbility as a real ASC-hosted active Ability.
	if (UAbilityTask_MeleeTraceWindow* BootstrapTask = ActiveAbility->GetActiveTraceWindowTask())
	{
		BootstrapTask->EndTask();
	}
	TestNull(TEXT("Trail active requester released after bootstrap task ended"), PlayerTrail->GetTestActiveRequester());

	// -------------------------------------------------------------------------
	// CASE 1: Scalar Route (First Open, Keep Open, Closed Task Replacement)
	// -------------------------------------------------------------------------
	TObjectPtr<UAbilityTask_MeleeTraceWindow> Task = nullptr;
	TWeakObjectPtr<const UAnimNotifyState_AttackTraceWindow> NotifyStateWeak = nullptr;

	UAnimNotifyState_AttackTraceWindow* NotifyStateA = NewObject<UAnimNotifyState_AttackTraceWindow>(GetTransientPackage());
	NotifyStateWeak = NotifyStateA;

	TSubclassOf<UGameplayEffect> DamageGEClass = UTestProjectileDamageGE::StaticClass();

	// 1.1 First open succeeds
	FMeleeTraceWindowLifecycle::OpenOrKeepScalar(
		ActiveAbility,
		Task,
		DamageGEClass,
		1.0f,
		FGameplayTag(),
		0.0f,
		0.0f,
		TArray<FName>());

	TestNotNull(TEXT("Case 1.1: First open produces non-null Task"), Task.Get());
	TestTrue(TEXT("Case 1.1: Task is open"), Task && Task->IsTraceWindowOpen());
	TestTrue(TEXT("Case 1.1: Trail component active requester is Task"), PlayerTrail->GetTestActiveRequester() == static_cast<const UObject*>(Task.Get()));

	// 1.2 Second open keeps the same active Task
	UAbilityTask_MeleeTraceWindow* FirstTask = Task.Get();
	FMeleeTraceWindowLifecycle::OpenOrKeepScalar(
		ActiveAbility,
		Task,
		DamageGEClass,
		1.0f,
		FGameplayTag(),
		0.0f,
		0.0f,
		TArray<FName>());

	TestEqual(TEXT("Case 1.2: Second open keeps identical Task pointer"), Task.Get(), FirstTask);
	TestTrue(TEXT("Case 1.2: Task remains open"), Task && Task->IsTraceWindowOpen());

	// 1.3 Closed retained Task is replaced safely
	Task->EndTask();
	TestFalse(TEXT("Case 1.3: Task is closed after EndTask"), Task->IsTraceWindowOpen());

	FMeleeTraceWindowLifecycle::OpenOrKeepScalar(
		ActiveAbility,
		Task,
		DamageGEClass,
		1.0f,
		FGameplayTag(),
		0.0f,
		0.0f,
		TArray<FName>());

	TestNotNull(TEXT("Case 1.3: OpenOrKeepScalar replaces closed task with new Task"), Task.Get());
	TestNotEqual(TEXT("Case 1.3: New Task is distinct from closed Task"), Task.Get(), FirstTask);
	TestTrue(TEXT("Case 1.3: New Task is open"), Task && Task->IsTraceWindowOpen());

	// -------------------------------------------------------------------------
	// CASE 2: CloseAndClear Route
	// -------------------------------------------------------------------------
	FMeleeTraceWindowLifecycle::CloseAndClear(Task, NotifyStateWeak);
	TestNull(TEXT("Case 2.1: Task pointer cleared to nullptr"), Task.Get());
	TestFalse(TEXT("Case 2.1: Notify weak pointer is reset"), NotifyStateWeak.IsValid());
	TestNull(TEXT("Case 2.1: Trail active requester is released"), PlayerTrail->GetTestActiveRequester());

	FMeleeTraceWindowLifecycle::CloseAndClear(Task, NotifyStateWeak);
	TestNull(TEXT("Case 2.2: Second CloseAndClear is safe no-op"), Task.Get());

	// -------------------------------------------------------------------------
	// CASE 3: Map Route (Charged-Style SetByCaller Map)
	// -------------------------------------------------------------------------
	TMap<FGameplayTag, float> SetByCallerMagnitudes;
	SetByCallerMagnitudes.Add(FGameplayTag::RequestGameplayTag(FName(TEXT("Data.Damage.Charged")), false), -36.0f);
	SetByCallerMagnitudes.Add(FGameplayTag::RequestGameplayTag(FName(TEXT("Data.Poise.Charged")), false), 50.0f);

	NotifyStateWeak = NotifyStateA;

	// 3.1 Map open succeeds
	FMeleeTraceWindowLifecycle::OpenOrKeepMagnitudes(
		ActiveAbility,
		Task,
		DamageGEClass,
		1.0f,
		SetByCallerMagnitudes,
		TArray<FName>());

	TestNotNull(TEXT("Case 3.1: Map overload produces non-null Task"), Task.Get());
	TestTrue(TEXT("Case 3.1: Map Task is open"), Task && Task->IsTraceWindowOpen());
	TestTrue(TEXT("Case 3.1: Trail active requester is Map Task"), PlayerTrail->GetTestActiveRequester() == static_cast<const UObject*>(Task.Get()));

	// 3.2 Second open keeps the same Task
	UAbilityTask_MeleeTraceWindow* MapTask = Task.Get();
	FMeleeTraceWindowLifecycle::OpenOrKeepMagnitudes(
		ActiveAbility,
		Task,
		DamageGEClass,
		1.0f,
		SetByCallerMagnitudes,
		TArray<FName>());

	TestEqual(TEXT("Case 3.2: Map second open keeps identical Task pointer"), Task.Get(), MapTask);
	TestTrue(TEXT("Case 3.2: Map Task remains open"), Task && Task->IsTraceWindowOpen());

	// 3.3 Close and clear Map Task
	FMeleeTraceWindowLifecycle::CloseAndClear(Task, NotifyStateWeak);
	TestNull(TEXT("Case 3.3: Map Task cleared to nullptr"), Task.Get());
	TestFalse(TEXT("Case 3.3: Notify weak pointer is reset"), NotifyStateWeak.IsValid());
	TestNull(TEXT("Case 3.3: Trail active requester is released"), PlayerTrail->GetTestActiveRequester());

	// -------------------------------------------------------------------------
	// CASE 4: Synchronous Activation Failure (Coincident Endpoints Fail-Closed)
	// -------------------------------------------------------------------------
	{
		const FTransform OriginalTipRelativeTransform = BladeTip->GetRelativeTransform();

		// Make BladeTip coincident with BladeBase in world space to trigger synchronous fail-closed inside ReadyForActivation
		BladeTip->SetWorldLocation(BladeBase->GetComponentLocation());

		// 4.1 Scalar route fail-closed
		TObjectPtr<UAbilityTask_MeleeTraceWindow> FailTaskScalar = nullptr;
		FMeleeTraceWindowLifecycle::OpenOrKeepScalar(
			ActiveAbility,
			FailTaskScalar,
			DamageGEClass,
			1.0f,
			FGameplayTag(),
			0.0f,
			0.0f,
			TArray<FName>());

		TestNull(TEXT("Case 4.1: Synchronously failed scalar activation leaves Task nullptr"), FailTaskScalar.Get());
		TestFalse(TEXT("Case 4.1: Trail component is not tracking"), PlayerTrail->IsTestTrackingActive());
		TestNull(TEXT("Case 4.1: Trail active requester remains null"), PlayerTrail->GetTestActiveRequester());

		// 4.2 Map route fail-closed
		TObjectPtr<UAbilityTask_MeleeTraceWindow> FailTaskMag = nullptr;
		FMeleeTraceWindowLifecycle::OpenOrKeepMagnitudes(
			ActiveAbility,
			FailTaskMag,
			DamageGEClass,
			1.0f,
			SetByCallerMagnitudes,
			TArray<FName>());

		TestNull(TEXT("Case 4.2: Synchronously failed map activation leaves Task nullptr"), FailTaskMag.Get());
		TestFalse(TEXT("Case 4.2: Trail component is not tracking"), PlayerTrail->IsTestTrackingActive());
		TestNull(TEXT("Case 4.2: Trail active requester remains null"), PlayerTrail->GetTestActiveRequester());

		// Restore original Tip transform before proceeding
		BladeTip->SetRelativeTransform(OriginalTipRelativeTransform);
	}

	// -------------------------------------------------------------------------
	// CASE 5: Null / Un-instanced Ability Safeguards
	// -------------------------------------------------------------------------
	// 5.1 Null OwningAbility is safe no-op
	TObjectPtr<UAbilityTask_MeleeTraceWindow> NullTask = nullptr;
	FMeleeTraceWindowLifecycle::OpenOrKeepScalar(
		nullptr,
		NullTask,
		DamageGEClass,
		1.0f,
		FGameplayTag(),
		0.0f,
		0.0f,
		TArray<FName>());
	TestNull(TEXT("Case 5.1: Null OwningAbility leaves Task nullptr"), NullTask.Get());

	// 5.2 Inactive ability with null ActorInfo leaves Task null
	UTestMeleeTrailAbility* InactiveAbility = NewObject<UTestMeleeTrailAbility>(World);
	TObjectPtr<UAbilityTask_MeleeTraceWindow> InactiveTask = nullptr;
	FMeleeTraceWindowLifecycle::OpenOrKeepScalar(
		InactiveAbility,
		InactiveTask,
		DamageGEClass,
		1.0f,
		FGameplayTag(),
		0.0f,
		0.0f,
		TArray<FName>());
	TestNull(TEXT("Case 5.2: Inactive ability with null ActorInfo leaves Task nullptr"), InactiveTask.Get());

	// 5.3 Retained closed task passed with null Avatar is cleared
	FMeleeTraceWindowLifecycle::OpenOrKeepScalar(
		ActiveAbility,
		InactiveTask,
		DamageGEClass,
		1.0f,
		FGameplayTag(),
		0.0f,
		0.0f,
		TArray<FName>());
	TestNotNull(TEXT("Case 5.3: Valid Task created"), InactiveTask.Get());
	InactiveTask->EndTask();
	TestFalse(TEXT("Case 5.3: Task closed"), InactiveTask->IsTraceWindowOpen());

	FMeleeTraceWindowLifecycle::OpenOrKeepScalar(
		InactiveAbility,
		InactiveTask,
		DamageGEClass,
		1.0f,
		FGameplayTag(),
		0.0f,
		0.0f,
		TArray<FName>());
	TestNull(TEXT("Case 5.3: Closed Task cleared to nullptr when avatar resolution fails"), InactiveTask.Get());

	// Clean up real active ability at test teardown
	ActiveAbility->EndTestAbility();
	PlayerASC->ClearAbility(AbilityHandle);
	return true;
}

#endif
