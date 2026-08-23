#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Character/BaseCharacter.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Combat/Equipment/MeleeWeaponDefinition.h"
#include "Combat/Melee/MeleeTraceSourceComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/World.h"
#include "Tests/CombatAutomationFixture.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMeleeTraceSourceGeometryTest,
	"PolyQuest.Melee.TraceSourceGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	void SetStaticMeleeWeaponDefinitionReflected(UMeleeTraceSourceComponent* Comp, UMeleeWeaponDefinition* Def)
	{
		if (FObjectProperty* Prop = CastField<FObjectProperty>(Comp->GetClass()->FindPropertyByName(TEXT("StaticMeleeWeaponDefinition"))))
		{
			Prop->SetObjectPropertyValue_InContainer(Comp, Def);
		}
	}
}

bool FMeleeTraceSourceGeometryTest::RunTest(const FString& Parameters)
{
	// 1. Setup Test World
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("MeleeTraceSourceTestWorld"));
	WorldContext.SetCurrentWorld(World);
	if (!TestNotNull(TEXT("Test World created successfully"), World))
	{
		return false;
	}

	struct FTestWorldScopeCleanup
	{
		UWorld* WorldToDestroy;
		~FTestWorldScopeCleanup()
		{
			if (WorldToDestroy)
			{
				GEngine->DestroyWorldContext(WorldToDestroy);
				WorldToDestroy->DestroyWorld(false);
			}
		}
	} WorldScopeCleanup{ World };

	// Create transient UStaticMesh and UStaticMeshSocket fixture to avoid uncommitted Content asset dependency
	UStaticMesh* FixtureMesh = NewObject<UStaticMesh>(GetTransientPackage(), TEXT("Test_TraceFixtureMesh"));
	if (!TestNotNull(TEXT("FixtureMesh created successfully"), FixtureMesh))
	{
		return false;
	}

	UStaticMeshSocket* BaseSocket = NewObject<UStaticMeshSocket>(FixtureMesh);
	BaseSocket->SocketName = TEXT("Trace_Base");
	BaseSocket->RelativeLocation = FVector(0.0f, 0.0f, 15.0f);
	FixtureMesh->AddSocket(BaseSocket);

	UStaticMeshSocket* TipSocket = NewObject<UStaticMeshSocket>(FixtureMesh);
	TipSocket->SocketName = TEXT("Trace_Tip");
	TipSocket->RelativeLocation = FVector(0.0f, 0.0f, 85.0f);
	FixtureMesh->AddSocket(TipSocket);

	if (!TestNotNull(TEXT("BaseSocket Trace_Base exists on FixtureMesh"), FixtureMesh->FindSocket(TEXT("Trace_Base"))) ||
		!TestNotNull(TEXT("TipSocket Trace_Tip exists on FixtureMesh"), FixtureMesh->FindSocket(TEXT("Trace_Tip"))))
	{
		return false;
	}

	// 2. Test IsValidStaticMeshTraceGeometry pure contract
	{
		UMeleeWeaponDefinition* TestDef = NewObject<UMeleeWeaponDefinition>(GetTransientPackage(), TEXT("Test_GeomDef"));
		TestDef->WeaponMesh = FixtureMesh;
		TestDef->BladeBaseSocketName = TEXT("Trace_Base");
		TestDef->BladeTipSocketName = TEXT("Trace_Tip");
		TestDef->TraceRadius = 15.0f;
		TestDef->BladeSubdivisions = 5;

		FString Reason;
		TestTrue(TEXT("Valid StaticMesh trace geometry passes"), TestDef->IsValidStaticMeshTraceGeometry(Reason));

		// Missing mesh
		TestDef->WeaponMesh = nullptr;
		TestFalse(TEXT("Missing WeaponMesh is rejected"), TestDef->IsValidStaticMeshTraceGeometry(Reason));
		TestDef->WeaponMesh = FixtureMesh;

		// bUseOwnerMeshSocketForTrace cannot be used as StaticMesh geometry
		TestDef->bUseOwnerMeshSocketForTrace = true;
		TestFalse(TEXT("bUseOwnerMeshSocketForTrace is rejected for StaticMesh geometry"), TestDef->IsValidStaticMeshTraceGeometry(Reason));
		TestDef->bUseOwnerMeshSocketForTrace = false;

		// Missing or incomplete socket names
		TestDef->BladeBaseSocketName = NAME_None;
		TestFalse(TEXT("None BladeBaseSocketName is rejected"), TestDef->IsValidStaticMeshTraceGeometry(Reason));
		TestDef->BladeBaseSocketName = TEXT("Trace_Base");

		TestDef->BladeTipSocketName = NAME_None;
		TestFalse(TEXT("None BladeTipSocketName is rejected"), TestDef->IsValidStaticMeshTraceGeometry(Reason));
		TestDef->BladeTipSocketName = TEXT("Trace_Tip");

		// Identical socket names
		TestDef->BladeTipSocketName = TEXT("Trace_Base");
		TestFalse(TEXT("Identical socket names are rejected"), TestDef->IsValidStaticMeshTraceGeometry(Reason));
		TestDef->BladeTipSocketName = TEXT("Trace_Tip");

		// Non-existent socket names
		TestDef->BladeBaseSocketName = TEXT("NonExistent_Base_Socket");
		TestFalse(TEXT("Non-existent socket name is rejected"), TestDef->IsValidStaticMeshTraceGeometry(Reason));
		TestDef->BladeBaseSocketName = TEXT("Trace_Base");

		// Invalid radius & subdivisions
		TestDef->TraceRadius = 0.0f;
		TestFalse(TEXT("Zero TraceRadius is rejected"), TestDef->IsValidStaticMeshTraceGeometry(Reason));
		TestDef->TraceRadius = -5.0f;
		TestFalse(TEXT("Negative TraceRadius is rejected"), TestDef->IsValidStaticMeshTraceGeometry(Reason));
		TestDef->TraceRadius = std::numeric_limits<float>::quiet_NaN();
		TestFalse(TEXT("NaN TraceRadius is rejected"), TestDef->IsValidStaticMeshTraceGeometry(Reason));
		TestDef->TraceRadius = std::numeric_limits<float>::infinity();
		TestFalse(TEXT("Infinite TraceRadius is rejected"), TestDef->IsValidStaticMeshTraceGeometry(Reason));
		TestDef->TraceRadius = 15.0f;

		TestDef->BladeSubdivisions = 0;
		TestFalse(TEXT("Zero BladeSubdivisions is rejected"), TestDef->IsValidStaticMeshTraceGeometry(Reason));
		TestDef->BladeSubdivisions = 9;
		TestFalse(TEXT("Out of bounds BladeSubdivisions is rejected"), TestDef->IsValidStaticMeshTraceGeometry(Reason));
		TestDef->BladeSubdivisions = 5;

		TestTrue(TEXT("Restored definition is valid"), TestDef->IsValidStaticMeshTraceGeometry(Reason));
	}

	// 3. Test StaticMeleeWeaponDefinition Resolution on Actor
	{
		FActorSpawnParameters SpawnParams;
		AActor* TestOwner = World->SpawnActor<AActor>(AActor::StaticClass(), FVector(100.0f, 200.0f, 300.0f), FRotator::ZeroRotator, SpawnParams);
		TestNotNull(TEXT("TestOwner spawned successfully"), TestOwner);

		UStaticMeshComponent* DisplayComp = NewObject<UStaticMeshComponent>(TestOwner, TEXT("WeaponMesh"));
		DisplayComp->RegisterComponent();
		DisplayComp->SetStaticMesh(FixtureMesh);
		TestOwner->SetRootComponent(DisplayComp);

		// Also attach legacy markers to test fail-closed protection (must NOT fall back when static def is invalid)
		USceneComponent* LegacyBase = NewObject<USceneComponent>(TestOwner, TEXT("BladeTraceBase"));
		LegacyBase->RegisterComponent();
		LegacyBase->AttachToComponent(DisplayComp, FAttachmentTransformRules::KeepRelativeTransform);
		LegacyBase->SetRelativeLocation(FVector(0.0f, 0.0f, 10.0f));

		USceneComponent* LegacyTip = NewObject<USceneComponent>(TestOwner, TEXT("BladeTraceTip"));
		LegacyTip->RegisterComponent();
		LegacyTip->AttachToComponent(DisplayComp, FAttachmentTransformRules::KeepRelativeTransform);
		LegacyTip->SetRelativeLocation(FVector(0.0f, 0.0f, 90.0f));

		UMeleeTraceSourceComponent* TraceSource = NewObject<UMeleeTraceSourceComponent>(TestOwner, TEXT("MeleeTraceSource"));
		TraceSource->RegisterComponent();

		UMeleeWeaponDefinition* ValidDef = NewObject<UMeleeWeaponDefinition>(GetTransientPackage(), TEXT("Test_ValidDef"));
		ValidDef->WeaponMesh = FixtureMesh;
		ValidDef->BladeBaseSocketName = TEXT("Trace_Base");
		ValidDef->BladeTipSocketName = TEXT("Trace_Tip");
		ValidDef->TraceRadius = 22.0f;
		ValidDef->BladeSubdivisions = 7;

		// 3.1 Valid static definition resolution
		SetStaticMeleeWeaponDefinitionReflected(TraceSource, ValidDef);
		TestEqual(TEXT("TraceRadius read from static definition"), TraceSource->GetTraceRadius(), 22.0f);
		TestEqual(TEXT("BladeSubdivisions read from static definition"), TraceSource->GetBladeSubdivisions(), 7);

		FVector BladeBasePos = FVector::ZeroVector;
		FVector BladeTipPos = FVector::ZeroVector;
		TestTrue(TEXT("TryGetBladeEndpoints succeeds with valid static definition"), TraceSource->TryGetBladeEndpoints(BladeBasePos, BladeTipPos));
		TestFalse(TEXT("Endpoints are distinct"), BladeBasePos.Equals(BladeTipPos, KINDA_SMALL_NUMBER));
		TestEqual(TEXT("BladeBasePos matches Socket location"), BladeBasePos, DisplayComp->GetSocketLocation(TEXT("Trace_Base")));
		TestEqual(TEXT("BladeTipPos matches Socket location"), BladeTipPos, DisplayComp->GetSocketLocation(TEXT("Trace_Tip")));

		// 3.2 Fail-closed: Static definition configured with invalid socket name must FAIL and NOT fall back to legacy markers
		UMeleeWeaponDefinition* InvalidSocketDef = NewObject<UMeleeWeaponDefinition>(GetTransientPackage(), TEXT("Test_InvalidSocketDef"));
		InvalidSocketDef->WeaponMesh = FixtureMesh;
		InvalidSocketDef->BladeBaseSocketName = TEXT("Invalid_Socket_Base");
		InvalidSocketDef->BladeTipSocketName = TEXT("Trace_Tip");
		SetStaticMeleeWeaponDefinitionReflected(TraceSource, InvalidSocketDef);

		FVector FailedBasePos = FVector::ZeroVector;
		FVector FailedTipPos = FVector::ZeroVector;
		TestFalse(TEXT("Invalid static definition fails closed"), TraceSource->TryGetBladeEndpoints(FailedBasePos, FailedTipPos));
		TestFalse(TEXT("Invalid socket def does not fall back to legacy markers"), FailedBasePos.Equals(LegacyBase->GetComponentLocation(), KINDA_SMALL_NUMBER));

		// 3.3 Fail-closed (P3-1.1): Missing WeaponMesh display component on Owner
		{
			AActor* OwnerNoDisplay = World->SpawnActor<AActor>(AActor::StaticClass(), FVector(100.0f, 200.0f, 300.0f), FRotator::ZeroRotator, SpawnParams);
			USceneComponent* NoDisplayRoot = NewObject<USceneComponent>(OwnerNoDisplay, TEXT("Root"));
			NoDisplayRoot->RegisterComponent();
			OwnerNoDisplay->SetRootComponent(NoDisplayRoot);

			USceneComponent* NoDisplayLegacyBase = NewObject<USceneComponent>(OwnerNoDisplay, TEXT("BladeTraceBase"));
			NoDisplayLegacyBase->RegisterComponent();
			NoDisplayLegacyBase->AttachToComponent(NoDisplayRoot, FAttachmentTransformRules::KeepRelativeTransform);
			NoDisplayLegacyBase->SetRelativeLocation(FVector(0.0f, 0.0f, 10.0f));

			USceneComponent* NoDisplayLegacyTip = NewObject<USceneComponent>(OwnerNoDisplay, TEXT("BladeTraceTip"));
			NoDisplayLegacyTip->RegisterComponent();
			NoDisplayLegacyTip->AttachToComponent(NoDisplayRoot, FAttachmentTransformRules::KeepRelativeTransform);
			NoDisplayLegacyTip->SetRelativeLocation(FVector(0.0f, 0.0f, 90.0f));

			UMeleeTraceSourceComponent* NoDisplayTraceSource = NewObject<UMeleeTraceSourceComponent>(OwnerNoDisplay, TEXT("MeleeTraceSource"));
			NoDisplayTraceSource->RegisterComponent();
			SetStaticMeleeWeaponDefinitionReflected(NoDisplayTraceSource, ValidDef);

			FailedBasePos = FVector::ZeroVector;
			FailedTipPos = FVector::ZeroVector;
			TestFalse(TEXT("Missing WeaponMesh component fails closed"), NoDisplayTraceSource->TryGetBladeEndpoints(FailedBasePos, FailedTipPos));
			TestFalse(TEXT("Missing WeaponMesh does not fall back to legacy markers"), FailedBasePos.Equals(NoDisplayLegacyBase->GetComponentLocation(), KINDA_SMALL_NUMBER));
			OwnerNoDisplay->Destroy();
		}

		// 3.4 Fail-closed (P3-1.2): Component named WeaponMesh is wrong component type (USceneComponent instead of UStaticMeshComponent)
		{
			AActor* OwnerWrongComp = World->SpawnActor<AActor>(AActor::StaticClass(), FVector(100.0f, 200.0f, 300.0f), FRotator::ZeroRotator, SpawnParams);
			USceneComponent* WrongDisplayComp = NewObject<USceneComponent>(OwnerWrongComp, TEXT("WeaponMesh"));
			WrongDisplayComp->RegisterComponent();
			OwnerWrongComp->SetRootComponent(WrongDisplayComp);

			USceneComponent* WrongLegacyBase = NewObject<USceneComponent>(OwnerWrongComp, TEXT("BladeTraceBase"));
			WrongLegacyBase->RegisterComponent();
			WrongLegacyBase->AttachToComponent(WrongDisplayComp, FAttachmentTransformRules::KeepRelativeTransform);
			WrongLegacyBase->SetRelativeLocation(FVector(0.0f, 0.0f, 10.0f));

			USceneComponent* WrongLegacyTip = NewObject<USceneComponent>(OwnerWrongComp, TEXT("BladeTraceTip"));
			WrongLegacyTip->RegisterComponent();
			WrongLegacyTip->AttachToComponent(WrongDisplayComp, FAttachmentTransformRules::KeepRelativeTransform);
			WrongLegacyTip->SetRelativeLocation(FVector(0.0f, 0.0f, 90.0f));

			UMeleeTraceSourceComponent* WrongCompTraceSource = NewObject<UMeleeTraceSourceComponent>(OwnerWrongComp, TEXT("MeleeTraceSource"));
			WrongCompTraceSource->RegisterComponent();
			SetStaticMeleeWeaponDefinitionReflected(WrongCompTraceSource, ValidDef);

			FailedBasePos = FVector::ZeroVector;
			FailedTipPos = FVector::ZeroVector;
			TestFalse(TEXT("Wrong component type for WeaponMesh fails closed"), WrongCompTraceSource->TryGetBladeEndpoints(FailedBasePos, FailedTipPos));
			TestFalse(TEXT("Wrong component type does not fall back to legacy markers"), FailedBasePos.Equals(WrongLegacyBase->GetComponentLocation(), KINDA_SMALL_NUMBER));
			OwnerWrongComp->Destroy();
		}

		// 3.5 Fail-closed (P3-1.3): Display component exists with different non-null StaticMesh
		{
			UStaticMesh* DifferentMesh = NewObject<UStaticMesh>(GetTransientPackage(), TEXT("Test_DifferentMesh"));
			DisplayComp->SetStaticMesh(DifferentMesh);
			SetStaticMeleeWeaponDefinitionReflected(TraceSource, ValidDef);

			FailedBasePos = FVector::ZeroVector;
			FailedTipPos = FVector::ZeroVector;
			TestFalse(TEXT("Display with different non-null StaticMesh fails closed"), TraceSource->TryGetBladeEndpoints(FailedBasePos, FailedTipPos));
			TestFalse(TEXT("Different StaticMesh does not fall back to legacy markers"), FailedBasePos.Equals(LegacyBase->GetComponentLocation(), KINDA_SMALL_NUMBER));

			// Also test null mesh on display component
			DisplayComp->SetStaticMesh(nullptr);
			TestFalse(TEXT("Null StaticMesh on display fails closed"), TraceSource->TryGetBladeEndpoints(FailedBasePos, FailedTipPos));
			TestFalse(TEXT("Null StaticMesh does not fall back to legacy markers"), FailedBasePos.Equals(LegacyBase->GetComponentLocation(), KINDA_SMALL_NUMBER));
			DisplayComp->SetStaticMesh(FixtureMesh);
		}

		// 3.6 Fail-closed (P3-1.4): Coincident socket RelativeLocation
		{
			UStaticMesh* CoincidentMesh = NewObject<UStaticMesh>(GetTransientPackage(), TEXT("Test_CoincidentMesh"));
			UStaticMeshSocket* CoincidentBase = NewObject<UStaticMeshSocket>(CoincidentMesh);
			CoincidentBase->SocketName = TEXT("Trace_Base");
			CoincidentBase->RelativeLocation = FVector(0.0f, 0.0f, 50.0f);
			CoincidentMesh->AddSocket(CoincidentBase);

			UStaticMeshSocket* CoincidentTip = NewObject<UStaticMeshSocket>(CoincidentMesh);
			CoincidentTip->SocketName = TEXT("Trace_Tip");
			CoincidentTip->RelativeLocation = FVector(0.0f, 0.0f, 50.0f);
			CoincidentMesh->AddSocket(CoincidentTip);

			UMeleeWeaponDefinition* CoincidentDef = NewObject<UMeleeWeaponDefinition>(GetTransientPackage(), TEXT("Test_CoincidentDef"));
			CoincidentDef->WeaponMesh = CoincidentMesh;
			CoincidentDef->BladeBaseSocketName = TEXT("Trace_Base");
			CoincidentDef->BladeTipSocketName = TEXT("Trace_Tip");

			FString CoincidentReason;
			TestFalse(TEXT("Coincident socket locations rejected by IsValidStaticMeshTraceGeometry"), CoincidentDef->IsValidStaticMeshTraceGeometry(CoincidentReason));

			DisplayComp->SetStaticMesh(CoincidentMesh);
			SetStaticMeleeWeaponDefinitionReflected(TraceSource, CoincidentDef);

			FailedBasePos = FVector::ZeroVector;
			FailedTipPos = FVector::ZeroVector;
			TestFalse(TEXT("Coincident socket locations fail closed on TraceSource"), TraceSource->TryGetBladeEndpoints(FailedBasePos, FailedTipPos));
			TestFalse(TEXT("Coincident socket locations do not fall back to legacy markers"), FailedBasePos.Equals(LegacyBase->GetComponentLocation(), KINDA_SMALL_NUMBER));
			DisplayComp->SetStaticMesh(FixtureMesh);
		}

		// 3.7 Fail-closed (P3-1.5): Non-finite (NaN / Inf) socket RelativeLocation
		{
			UStaticMesh* NonFiniteMesh = NewObject<UStaticMesh>(GetTransientPackage(), TEXT("Test_NonFiniteMesh"));
			UStaticMeshSocket* NonFiniteBase = NewObject<UStaticMeshSocket>(NonFiniteMesh);
			NonFiniteBase->SocketName = TEXT("Trace_Base");
			NonFiniteBase->RelativeLocation = FVector(0.0f, 0.0f, std::numeric_limits<float>::quiet_NaN());
			NonFiniteMesh->AddSocket(NonFiniteBase);

			UStaticMeshSocket* NonFiniteTip = NewObject<UStaticMeshSocket>(NonFiniteMesh);
			NonFiniteTip->SocketName = TEXT("Trace_Tip");
			NonFiniteTip->RelativeLocation = FVector(0.0f, 0.0f, 80.0f);
			NonFiniteMesh->AddSocket(NonFiniteTip);

			UMeleeWeaponDefinition* NonFiniteDef = NewObject<UMeleeWeaponDefinition>(GetTransientPackage(), TEXT("Test_NonFiniteDef"));
			NonFiniteDef->WeaponMesh = NonFiniteMesh;
			NonFiniteDef->BladeBaseSocketName = TEXT("Trace_Base");
			NonFiniteDef->BladeTipSocketName = TEXT("Trace_Tip");

			FString NonFiniteReason;
			TestFalse(TEXT("NaN socket location rejected by IsValidStaticMeshTraceGeometry"), NonFiniteDef->IsValidStaticMeshTraceGeometry(NonFiniteReason));

			DisplayComp->SetStaticMesh(NonFiniteMesh);
			SetStaticMeleeWeaponDefinitionReflected(TraceSource, NonFiniteDef);

			FailedBasePos = FVector::ZeroVector;
			FailedTipPos = FVector::ZeroVector;
			TestFalse(TEXT("NaN socket location fails closed on TraceSource"), TraceSource->TryGetBladeEndpoints(FailedBasePos, FailedTipPos));
			TestFalse(TEXT("NaN socket location does not fall back to legacy markers"), FailedBasePos.Equals(LegacyBase->GetComponentLocation(), KINDA_SMALL_NUMBER));

			// Infinite socket location
			NonFiniteBase->RelativeLocation = FVector(0.0f, 0.0f, 10.0f);
			NonFiniteTip->RelativeLocation = FVector(0.0f, 0.0f, std::numeric_limits<float>::infinity());
			TestFalse(TEXT("Infinite socket location rejected by IsValidStaticMeshTraceGeometry"), NonFiniteDef->IsValidStaticMeshTraceGeometry(NonFiniteReason));
			TestFalse(TEXT("Infinite socket location fails closed on TraceSource"), TraceSource->TryGetBladeEndpoints(FailedBasePos, FailedTipPos));
			TestFalse(TEXT("Infinite socket location does not fall back to legacy markers"), FailedBasePos.Equals(LegacyBase->GetComponentLocation(), KINDA_SMALL_NUMBER));

			DisplayComp->SetStaticMesh(FixtureMesh);
		}

		// 3.8 Null static definition must restore legacy fallback
		SetStaticMeleeWeaponDefinitionReflected(TraceSource, nullptr);
		TestEqual(TEXT("Default TraceRadius when static def is null"), TraceSource->GetTraceRadius(), 12.0f);
		TestEqual(TEXT("Default BladeSubdivisions when static def is null"), TraceSource->GetBladeSubdivisions(), 4);

		FVector FallbackBasePos = FVector::ZeroVector;
		FVector FallbackTipPos = FVector::ZeroVector;
		TestTrue(TEXT("Null static definition resolves legacy marker fallback"), TraceSource->TryGetBladeEndpoints(FallbackBasePos, FallbackTipPos));
		TestEqual(TEXT("Fallback base matches legacy component location"), FallbackBasePos, LegacyBase->GetComponentLocation());
		TestEqual(TEXT("Fallback tip matches legacy component location"), FallbackTipPos, LegacyTip->GetComponentLocation());

		TestOwner->Destroy();
	}

	// 4. Test DisableFixedWeaponDisplayCollision and Camera collision policy on BaseCharacter via FinishSpawning lifecycle
	{
		UStaticMeshComponent* FixedWeaponDisplay = nullptr;
		AEnemyCharacter* TestCharacter = FCombatAutomationFixture::SpawnPassiveEnemy(
			World,
			FTransform::Identity,
			[&FixedWeaponDisplay](AEnemyCharacter& InCharacter)
			{
				FixedWeaponDisplay = NewObject<UStaticMeshComponent>(&InCharacter, TEXT("WeaponMesh"));
				FixedWeaponDisplay->RegisterComponent();
				FixedWeaponDisplay->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				FixedWeaponDisplay->SetCollisionResponseToAllChannels(ECR_Block);
				FixedWeaponDisplay->SetGenerateOverlapEvents(true);
			});
		TestNotNull(TEXT("TestCharacter spawned successfully"), TestCharacter);
		TestNotNull(TEXT("Fixed weapon display created before BeginPlay"), FixedWeaponDisplay);
		if (!TestCharacter || !FixedWeaponDisplay)
		{
			return false;
		}

		TestEqual(TEXT("CapsuleComponent ignores Camera channel"), TestCharacter->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Camera), ECR_Ignore);
		TestEqual(TEXT("Mesh ignores Camera channel"), TestCharacter->GetMesh()->GetCollisionResponseToChannel(ECC_Camera), ECR_Ignore);
		TestEqual(TEXT("Fixed display collision is NoCollision"), FixedWeaponDisplay->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
		TestEqual(TEXT("Fixed display ignores Camera channel"), FixedWeaponDisplay->GetCollisionResponseToChannel(ECC_Camera), ECR_Ignore);
		TestEqual(TEXT("Fixed display ignores Visibility channel"), FixedWeaponDisplay->GetCollisionResponseToChannel(ECC_Visibility), ECR_Ignore);
		TestEqual(TEXT("Fixed display ignores WorldStatic channel"), FixedWeaponDisplay->GetCollisionResponseToChannel(ECC_WorldStatic), ECR_Ignore);
		TestFalse(TEXT("Fixed display disables overlap generation"), FixedWeaponDisplay->GetGenerateOverlapEvents());

		TestCharacter->Destroy();
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
