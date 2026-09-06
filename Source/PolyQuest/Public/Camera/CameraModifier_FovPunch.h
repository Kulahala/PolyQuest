// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraModifier.h"
#include "CameraModifier_FovPunch.generated.h"

/**
 * Native camera modifier for momentary combat FOV punch (compression) on hit impact.
 * Modifies InOutPOV.FOV directly without mutating FollowCamera->FieldOfView,
 * completely eliminating base FOV drift across aiming/sprint transitions.
 */
UCLASS(BlueprintType, Blueprintable)
class POLYQUEST_API UCameraModifier_FovPunch : public UCameraModifier
{
	GENERATED_BODY()

public:
	UCameraModifier_FovPunch();

	virtual bool ModifyCamera(float DeltaTime, struct FMinimalViewInfo& InOutPOV) override;

	/** Triggers an FOV punch in degrees (e.g. 1.0f for small, 1.5f for big, 2.0f for execution). */
	void TriggerPunch(float PunchDegrees);

	float GetCurrentPunchOffset() const { return CurrentPunchOffset; }
	bool IsPunchActive() const { return bIsPunchActive; }

#if WITH_DEV_AUTOMATION_TESTS
	void SetTestPunchOffset(float InOffset) { CurrentPunchOffset = InOffset; bIsPunchActive = !FMath::IsNearlyZero(InOffset); }
	void SetTestInterpSpeed(float InSpeed) { RecoveryInterpSpeed = InSpeed; }
	float GetTestMaxPunchDegrees() const { return MaxPunchDegrees; }
#endif

protected:
	/** Speed of exponential ease-out recovery in Hz. */
	UPROPERTY(EditAnywhere, Category = "Camera|Punch", meta = (ClampMin = "1.0"))
	float RecoveryInterpSpeed = 18.0f;

	/** Hard upper bound on cumulative punch magnitude in degrees to prevent fish-eye. */
	UPROPERTY(EditAnywhere, Category = "Camera|Punch", meta = (ClampMin = "0.5"))
	float MaxPunchDegrees = 3.0f;

private:
	/** Current punch offset in degrees (e.g. -1.5f at peak). */
	float CurrentPunchOffset = 0.0f;

	/** Active state guard for idle gate short-circuiting. */
	bool bIsPunchActive = false;
};
