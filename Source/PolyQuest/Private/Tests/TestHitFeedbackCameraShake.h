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
	virtual void StopShakePatternImpl(const FCameraShakePatternStopParams& Params) override
	{
		bIsStopped = true;
	}

	virtual void TeardownShakePatternImpl() override
	{
		bIsStopped = true;
	}

	virtual bool IsFinishedImpl() const override
	{
		return bIsStopped;
	}

	bool bIsStopped = false;
};

UCLASS()
class UTestSmallHitFeedbackCameraShake : public UCameraShakeBase
{
	GENERATED_BODY()

public:
	UTestSmallHitFeedbackCameraShake(const FObjectInitializer& ObjectInitializer);
};

UCLASS()
class UTestBigHitFeedbackCameraShake : public UCameraShakeBase
{
	GENERATED_BODY()

public:
	UTestBigHitFeedbackCameraShake(const FObjectInitializer& ObjectInitializer);
};

UCLASS()
class UTestLaunchHitFeedbackCameraShake : public UCameraShakeBase
{
	GENERATED_BODY()

public:
	UTestLaunchHitFeedbackCameraShake(const FObjectInitializer& ObjectInitializer);
};
