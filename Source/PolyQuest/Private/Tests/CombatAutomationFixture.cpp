#include "Tests/CombatAutomationFixture.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "InputAction.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "AbilitySystem/Abilities/PrimaryAttackAbility.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Character/Player/PlayerCharacter.h"
#include "Combat/Equipment/MeleeWeaponDefinition.h"
#include "Combat/Feedback/CombatFeedbackDataAsset.h"
#include "GameplayTagContainer.h"
#include "Tests/TestExhaustionMoveSpeedGE.h"
#include "Tests/TestPoiseRecoveryGE.h"
#include "Tests/TestStaminaRegenGE.h"
#include "Tests/TestHitFeedbackCameraShake.h"

namespace
{
	constexpr TCHAR HeroMeshPath[] = TEXT("/Game/PolygonDungeons/Meshes/Characters/SK_Character_Hero_Knight_Male");
	const FName WeaponSocketName(TEXT("Weapon_R"));

	UMaterialInterface* CreateTestOverlay(UObject* Outer)
	{
		UMaterial* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
		return DefaultMaterial ? UMaterialInstanceDynamic::Create(DefaultMaterial, Outer) : nullptr;
	}

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

	UMeleeWeaponDefinition* WeaponDefinition = NewObject<UMeleeWeaponDefinition>(Player, NAME_None, RF_Transient);
	UInputAction* TestInputAction = NewObject<UInputAction>(Player, TEXT("TestStartupInputAction"), RF_Transient);
	USkeletalMesh* HeroMesh = LoadObject<USkeletalMesh>(nullptr, HeroMeshPath);
	USkeletalMeshComponent* PlayerMesh = Player->GetMesh();
	if (!WeaponDefinition || !TestInputAction || !HeroMesh || !HeroMesh->FindSocket(WeaponSocketName) || !PlayerMesh)
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
	WeaponDefinition->BaseGrantedActions.Add(UPrimaryAttackAbility::StaticClass());
	WeaponDefinition->PrimaryAttackAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Primary")), false);
	Player->ConfigureTestStartupFixture(
		WeaponDefinition,
		UTestStaminaRegenGE::StaticClass(),
		UTestExhaustionMoveSpeedGE::StaticClass(),
		TestInputAction);
	UPlayerCombatFeedbackDataAsset* PlayerFeedback = NewObject<UPlayerCombatFeedbackDataAsset>(Player, NAME_None, RF_Transient);
	PlayerFeedback->HitFeedbackOverlayMaterial = CreateTestOverlay(Player);
	PlayerFeedback->HitFeedbackOverlayDurationSeconds = 0.10f;
	PlayerFeedback->SmallTier.ReceivedHitCameraShakeClass = UTestSmallHitFeedbackCameraShake::StaticClass();
	PlayerFeedback->BigTier.ReceivedHitCameraShakeClass = UTestBigHitFeedbackCameraShake::StaticClass();
	PlayerFeedback->LaunchTier.ReceivedHitCameraShakeClass = UTestLaunchHitFeedbackCameraShake::StaticClass();
	Player->SetTestCombatFeedbackData(PlayerFeedback);

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
	UEnemyCombatFeedbackDataAsset* EnemyFeedback = NewObject<UEnemyCombatFeedbackDataAsset>(Enemy, NAME_None, RF_Transient);
	EnemyFeedback->HitFeedbackOverlayMaterial = CreateTestOverlay(Enemy);
	EnemyFeedback->HitFeedbackOverlayDurationSeconds = 0.10f;
	EnemyFeedback->SmallTier.ImpactHitStopDurationSeconds = 0.03f;
	EnemyFeedback->SmallTier.ImpactHitStopTimeDilation = 0.1f;
	EnemyFeedback->BigTier.ImpactHitStopDurationSeconds = 0.05f;
	EnemyFeedback->BigTier.ImpactHitStopTimeDilation = 0.03f;
	EnemyFeedback->LaunchTier.ImpactHitStopDurationSeconds = 0.05f;
	EnemyFeedback->LaunchTier.ImpactHitStopTimeDilation = 0.05f;
	Enemy->SetTestCombatFeedbackData(EnemyFeedback);
	if (PreBeginPlaySetup)
	{
		PreBeginPlaySetup(*Enemy);
	}

	Enemy->FinishSpawning(Transform);
	DispatchBeginPlayOnce(Enemy);
	return Enemy;
}

#endif
