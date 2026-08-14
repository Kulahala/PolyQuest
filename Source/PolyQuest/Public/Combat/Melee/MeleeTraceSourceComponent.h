#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "MeleeTraceSourceComponent.generated.h"

class USceneComponent;

/**
 * Resolves Blueprint-authored blade markers for the fixed player weapon fixture.
 * It deliberately has no transform of its own: the fixed fixture's Blueprint
 * component names resolve the marker transforms used by an active AbilityTask.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class POLYQUEST_API UMeleeTraceSourceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMeleeTraceSourceComponent();

	/** Returns false, with one focused warning, until all authored component names resolve valid markers. */
	bool TryGetBladeEndpoints(FVector& OutBladeBase, FVector& OutBladeTip);

	ECollisionChannel GetTraceChannel() const { return TraceChannel; }
	float GetTraceRadius() const { return TraceRadius; }
	int32 GetBladeSubdivisions() const { return BladeSubdivisions; }

private:
	bool ResolveConfiguredComponents(USceneComponent*& OutWeaponDisplay, USceneComponent*& OutBladeBase, USceneComponent*& OutBladeTip);
	void WarnInvalidConfiguration(const FString& Reason);

	/** The actual weapon display component. This default matches BP_Player's read-back WeaponMesh component. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Melee Trace|Source", meta = (AllowPrivateAccess = "true"))
	FName WeaponDisplayComponentName = TEXT("WeaponMesh");

	/** A non-colliding SceneComponent attached below WeaponDisplayComponentName at the blade root. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Melee Trace|Source", meta = (AllowPrivateAccess = "true"))
	FName BladeTraceBaseComponentName = TEXT("BladeTraceBase");

	/** A non-colliding SceneComponent attached below WeaponDisplayComponentName at the blade tip. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Melee Trace|Source", meta = (AllowPrivateAccess = "true"))
	FName BladeTraceTipComponentName = TEXT("BladeTraceTip");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee Trace|Trace", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float TraceRadius = 12.0f;

	/** Must match the project MeleeTrace trace channel in DefaultEngine.ini. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee Trace|Trace", meta = (AllowPrivateAccess = "true"))
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_GameTraceChannel1;

	/** Sphere-sweep samples along the blade. This is intentionally bounded for the fixed straight-sword fixture. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee Trace|Trace", meta = (AllowPrivateAccess = "true", ClampMin = "1", ClampMax = "8"))
	int32 BladeSubdivisions = 4;

	bool bConfigurationWarningIssued = false;
};
