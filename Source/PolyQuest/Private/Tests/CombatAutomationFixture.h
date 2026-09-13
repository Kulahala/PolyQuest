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

	/** Ticks the test world by DeltaSeconds and increments GFrameCounter once. No-op if World is null. */
	static void TickWorld(UWorld* World, float DeltaSeconds);

	/**
	 * Advances the test world in fixed 0.05s steps (using TickWorld) until DeltaSeconds is exhausted.
	 * Preserves standard float termination (> KINDA_SMALL_NUMBER) and the final partial step.
	 */
	static void AdvanceWorld(UWorld* World, float DeltaSeconds);
};

#endif
