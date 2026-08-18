#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "MeleeTraceSourceComponent.generated.h"

class USceneComponent;
class UMeleeWeaponDefinition;
class UWeaponEquipmentComponent;

/**
 * Resolves blade markers and sweep shape for melee tracing. Owners with an
 * equipped weapon resolve them from UWeaponEquipmentComponent; every other
 * owner (the enemy fixture) falls back to the authored fixed component-name
 * lookup with this component's own trace defaults.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class POLYQUEST_API UMeleeTraceSourceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMeleeTraceSourceComponent();

	virtual void BeginPlay() override;

	/** Returns false, with one focused warning, until the equipped markers or authored fixture names resolve valid samples. */
	bool TryGetBladeEndpoints(FVector& OutBladeBase, FVector& OutBladeTip);

	ECollisionChannel GetTraceChannel() const { return TraceChannel; }
	float GetTraceRadius() const;
	int32 GetBladeSubdivisions() const;

private:
	bool ResolveConfiguredComponents(USceneComponent*& OutWeaponDisplay, USceneComponent*& OutBladeBase, USceneComponent*& OutBladeTip);
	void WarnInvalidConfiguration(const FString& Reason);

	/** The optional equipped-weapon owner; cached once in BeginPlay, null on the enemy fixture path. */
	TWeakObjectPtr<UWeaponEquipmentComponent> CachedEquipmentComponent;

	/** Optional static melee weapon definition used as a geometry provider on non-equipped (e.g. enemy) owners. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee Trace|Source", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMeleeWeaponDefinition> StaticMeleeWeaponDefinition;

	/** The actual weapon display component. This v1 name is shared by player and first-enemy fixed fixtures without equipment. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Melee Trace|Source", meta = (AllowPrivateAccess = "true"))
	FName WeaponDisplayComponentName = TEXT("WeaponMesh");

	/** A non-colliding SceneComponent attached below WeaponDisplayComponentName at the blade root. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Melee Trace|Source", meta = (AllowPrivateAccess = "true"))
	FName BladeTraceBaseComponentName = TEXT("BladeTraceBase");

	/** A non-colliding SceneComponent attached below WeaponDisplayComponentName at the blade tip. */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Melee Trace|Source", meta = (AllowPrivateAccess = "true"))
	FName BladeTraceTipComponentName = TEXT("BladeTraceTip");

	/** Fixed-fixture sweep radius; equipped players read the weapon definition instead. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee Trace|Trace", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float TraceRadius = 12.0f;

	/** Must match the project MeleeTrace trace channel in DefaultEngine.ini. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee Trace|Trace", meta = (AllowPrivateAccess = "true"))
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_GameTraceChannel1;

	/** Fixed-fixture sphere-sweep samples; equipped players read the weapon definition instead. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee Trace|Trace", meta = (AllowPrivateAccess = "true", ClampMin = "1", ClampMax = "8"))
	int32 BladeSubdivisions = 4;

	bool bConfigurationWarningIssued = false;
};
