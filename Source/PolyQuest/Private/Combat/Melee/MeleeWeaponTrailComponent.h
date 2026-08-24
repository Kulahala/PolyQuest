#pragma once

#include "CoreMinimal.h"
#include "NiagaraComponent.h"
#include "MeleeWeaponTrailComponent.generated.h"

/**
 * Native character-owned Niagara component for melee weapon trails.
 * Follows world-space Blade Base and Tip coordinates during active trace windows.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent = "false"))
class UMeleeWeaponTrailComponent : public UNiagaraComponent
{
	GENERATED_BODY()

public:
	UMeleeWeaponTrailComponent();

	/** Gets whether auto-destroy is enabled on the underlying component. */
	bool GetAutoDestroy() const;

	/** Starts the melee weapon trail for the given requester token if a valid Niagara asset is configured. */
	void StartTrail(const UObject* Requester, const FVector& BladeBase, const FVector& BladeTip);

	/** Updates the blade endpoints for the active requester token. */
	void UpdateTrail(const UObject* Requester, const FVector& BladeBase, const FVector& BladeTip);

	/** Ends the melee weapon trail if the request originates from the active requester token. */
	void EndTrail(const UObject* Requester);

#if WITH_DEV_AUTOMATION_TESTS
	void SetTestTrackingEnabled(bool bEnable) { bTestTrackingEnabled = bEnable; }
	bool IsTestTrackingActive() const { return bTestIsActive; }
	const UObject* GetTestActiveRequester() const { return ActiveRequester.Get(); }
	FVector GetTestLastBladeBase() const { return TestLastBladeBase; }
	FVector GetTestLastBladeTip() const { return TestLastBladeTip; }
	int32 GetTestStartCallCount() const { return TestStartCallCount; }
	int32 GetTestUpdateCallCount() const { return TestUpdateCallCount; }
	int32 GetTestEndCallCount() const { return TestEndCallCount; }
#endif

private:
	void SetBladeEndpoints(const FVector& BladeBase, const FVector& BladeTip);

	TWeakObjectPtr<const UObject> ActiveRequester;

#if WITH_DEV_AUTOMATION_TESTS
	FVector TestLastBladeBase = FVector::ZeroVector;
	FVector TestLastBladeTip = FVector::ZeroVector;
	int32 TestStartCallCount = 0;
	int32 TestUpdateCallCount = 0;
	int32 TestEndCallCount = 0;
	bool bTestTrackingEnabled = false;
	bool bTestIsActive = false;
#endif
};
