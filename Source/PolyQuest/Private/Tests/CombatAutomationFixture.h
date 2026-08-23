#pragma once

#include "CoreMinimal.h"

class AEnemyCharacter;
class APlayerCharacter;
class UWorld;

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Native-only test actor construction that completes required startup authoring
 * before FinishSpawning can enter BeginPlay in an already-running test World.
 */
struct FCombatAutomationFixture
{
	using FPlayerPreBeginPlaySetup = TFunction<void(APlayerCharacter&)>;
	using FEnemyPreBeginPlaySetup = TFunction<void(AEnemyCharacter&)>;

	static APlayerCharacter* SpawnPlayer(
		UWorld* World,
		const FTransform& Transform = FTransform::Identity,
		FPlayerPreBeginPlaySetup PreBeginPlaySetup = {});

	static AEnemyCharacter* SpawnPassiveEnemy(
		UWorld* World,
		const FTransform& Transform = FTransform::Identity,
		FEnemyPreBeginPlaySetup PreBeginPlaySetup = {});
};

#endif
