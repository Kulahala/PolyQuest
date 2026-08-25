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

	/** Starts the melee weapon trail for the given requester token and source name if a valid Niagara asset is configured. */
	void StartTrail(const UObject* Requester, FName SourceName, const FVector& BladeBase, const FVector& BladeTip);

	/** Starts the melee weapon trail for the default source. */
	void StartTrail(const UObject* Requester, const FVector& BladeBase, const FVector& BladeTip);

	/** Updates the blade endpoints for the active requester token and source name. */
	void UpdateTrail(const UObject* Requester, FName SourceName, const FVector& BladeBase, const FVector& BladeTip);

	/** Updates the blade endpoints for the default source. */
	void UpdateTrail(const UObject* Requester, const FVector& BladeBase, const FVector& BladeTip);

	/** Ends the melee weapon trail if the request originates from the active requester token for that source. */
	void EndTrail(const UObject* Requester, FName SourceName = NAME_None);

#if WITH_DEV_AUTOMATION_TESTS
	void SetTestTrackingEnabled(bool bEnable) { bTestTrackingEnabled = bEnable; }
	bool IsTestTrackingActive(FName SourceName = NAME_None) const;
	const UObject* GetTestActiveRequester(FName SourceName = NAME_None) const;
	FVector GetTestLastBladeBase(FName SourceName = NAME_None) const;
	FVector GetTestLastBladeTip(FName SourceName = NAME_None) const;
	int32 GetTestStartCallCount(FName SourceName = NAME_None) const;
	int32 GetTestUpdateCallCount(FName SourceName = NAME_None) const;
	int32 GetTestEndCallCount(FName SourceName = NAME_None) const;
#endif

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void SetBladeEndpoints(const FVector& BladeBase, const FVector& BladeTip);

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UNiagaraComponent>> ChildTrailComponents;

	TMap<FName, TWeakObjectPtr<const UObject>> SourceActiveRequesters;

#if WITH_DEV_AUTOMATION_TESTS
	struct FTestTrailTrackingState
	{
		FVector LastBladeBase = FVector::ZeroVector;
		FVector LastBladeTip = FVector::ZeroVector;
		int32 StartCallCount = 0;
		int32 UpdateCallCount = 0;
		int32 EndCallCount = 0;
		bool bIsActive = false;
		TWeakObjectPtr<const UObject> ActiveRequester;
	};

	TMap<FName, FTestTrailTrackingState> TestTrackingStates;
	bool bTestTrackingEnabled = false;
#endif
};
