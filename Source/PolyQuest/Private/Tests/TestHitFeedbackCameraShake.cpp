#include "Tests/TestHitFeedbackCameraShake.h"

UTestSmallHitFeedbackCameraShake::UTestSmallHitFeedbackCameraShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UTestHitFeedbackCameraShakePattern>(TEXT("RootShakePattern")))
{
	bSingleInstance = true;
}

UTestBigHitFeedbackCameraShake::UTestBigHitFeedbackCameraShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UTestHitFeedbackCameraShakePattern>(TEXT("RootShakePattern")))
{
	bSingleInstance = true;
}

UTestLaunchHitFeedbackCameraShake::UTestLaunchHitFeedbackCameraShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UTestHitFeedbackCameraShakePattern>(TEXT("RootShakePattern")))
{
	bSingleInstance = true;
}
