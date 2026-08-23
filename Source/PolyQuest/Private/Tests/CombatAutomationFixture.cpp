#include "Tests/CombatAutomationFixture.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Character/Player/PlayerCharacter.h"
#include "Combat/Equipment/MeleeWeaponDefinition.h"
#include "Combat/Input/CombatLoadoutDefinition.h"
#include "Tests/TestPoiseRecoveryGE.h"
#include "Tests/TestStaminaRegenGE.h"

namespace
{
	constexpr TCHAR HeroMeshPath[] = TEXT("/Game/PolygonDungeons/Meshes/Characters/SK_Character_Hero_Knight_Male");
	const FName WeaponSocketName(TEXT("Weapon_R"));

	void DispatchBeginPlayOnce(AActor* Actor)
	{
		if (Actor && !Actor->HasActorBegunPlay())
		{
			Actor->DispatchBeginPlay();
		}
	}
}

APlayerCharacter* FCombatAutomationFixture::SpawnPlayer(
	UWorld* World,
	const FTransform& Transform,
	FPlayerPreBeginPlaySetup PreBeginPlaySetup)
{
	if (!World)
	{
		return nullptr;
	}

	APlayerCharacter* Player = World->SpawnActorDeferred<APlayerCharacter>(
		APlayerCharacter::StaticClass(),
		Transform,
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Player)
	{
		return nullptr;
	}

	UCombatLoadoutDefinition* Loadout = NewObject<UCombatLoadoutDefinition>(Player, NAME_None, RF_Transient);
	UMeleeWeaponDefinition* WeaponDefinition = NewObject<UMeleeWeaponDefinition>(Player, NAME_None, RF_Transient);
	USkeletalMesh* HeroMesh = LoadObject<USkeletalMesh>(nullptr, HeroMeshPath);
	USkeletalMeshComponent* PlayerMesh = Player->GetMesh();
	if (!Loadout || !WeaponDefinition || !HeroMesh || !HeroMesh->FindSocket(WeaponSocketName) || !PlayerMesh)
	{
		Player->Destroy();
		return nullptr;
	}

	PlayerMesh->SetSkeletalMeshAsset(HeroMesh);
	WeaponDefinition->HandSlot = EWeaponHandSlot::MainHandOneHanded;
	WeaponDefinition->LocomotionMode = EWeaponLocomotionMode::Default;
	WeaponDefinition->AttachSocketName = WeaponSocketName;
	WeaponDefinition->WeaponMesh = nullptr;
	WeaponDefinition->bUseOwnerMeshSocketForTrace = true;
	WeaponDefinition->BladeBaseMarkerRelativeLocation = FVector::ZeroVector;
	WeaponDefinition->BladeTipMarkerRelativeLocation = FVector(20.0f, 0.0f, 0.0f);
	WeaponDefinition->AssociatedLoadout = Loadout;
	Player->ConfigureTestStartupFixture(Loadout, WeaponDefinition, UTestStaminaRegenGE::StaticClass());

	if (PreBeginPlaySetup)
	{
		PreBeginPlaySetup(*Player);
	}

	Player->FinishSpawning(Transform);
	DispatchBeginPlayOnce(Player);
	return Player;
}

AEnemyCharacter* FCombatAutomationFixture::SpawnPassiveEnemy(
	UWorld* World,
	const FTransform& Transform,
	FEnemyPreBeginPlaySetup PreBeginPlaySetup)
{
	if (!World)
	{
		return nullptr;
	}

	AEnemyCharacter* Enemy = World->SpawnActorDeferred<AEnemyCharacter>(
		AEnemyCharacter::StaticClass(),
		Transform,
		nullptr,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Enemy)
	{
		return nullptr;
	}

	Enemy->ConfigureTestPassiveStartupFixture(UTestPoiseRecoveryGE::StaticClass());
	if (PreBeginPlaySetup)
	{
		PreBeginPlaySetup(*Enemy);
	}

	Enemy->FinishSpawning(Transform);
	DispatchBeginPlayOnce(Enemy);
	return Enemy;
}

#endif
