#pragma once

#include "Camera/CameraShakeBase.h"
#include "TestHitFeedbackCameraShake.generated.h"

UCLASS()
class UTestHitFeedbackCameraShakePattern : public UCameraShakePattern
{
	GENERATED_BODY()

public:
	UTestHitFeedbackCameraShakePattern(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer)
	{
	}

private:
	virtual bool IsFinishedImpl() const override { return false; }
};

UCLASS()
class UTestHitFeedbackCameraShake : public UCameraShakeBase
{
	GENERATED_BODY()

public:
	UTestHitFeedbackCameraShake(const FObjectInitializer& ObjectInitializer);
};
