// Copyright Epic Games, Inc. All Rights Reserved.

#include "Camera/CameraModifier_FovPunch.h"

UCameraModifier_FovPunch::UCameraModifier_FovPunch()
{
	Priority = 127;
}

bool UCameraModifier_FovPunch::ModifyCamera(float DeltaTime, FMinimalViewInfo& InOutPOV)
{
	Super::ModifyCamera(DeltaTime, InOutPOV);

	if (!bIsPunchActive)
	{
		return false;
	}

	CurrentPunchOffset = FMath::FInterpTo(CurrentPunchOffset, 0.0f, DeltaTime, RecoveryInterpSpeed);
	if (FMath::IsNearlyZero(CurrentPunchOffset, 0.005f))
	{
		CurrentPunchOffset = 0.0f;
		bIsPunchActive = false;
		return false;
	}

	InOutPOV.FOV += CurrentPunchOffset;
	return false;
}

void UCameraModifier_FovPunch::TriggerPunch(const float PunchDegrees)
{
	if (PunchDegrees <= 0.0f)
	{
		return;
	}

	CurrentPunchOffset = FMath::Clamp(CurrentPunchOffset - PunchDegrees, -MaxPunchDegrees, 0.0f);
	bIsPunchActive = true;
}
