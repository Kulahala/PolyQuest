#include "Tests/TestHitFeedbackCameraShake.h"

UTestHitFeedbackCameraShake::UTestHitFeedbackCameraShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UTestHitFeedbackCameraShakePattern>(TEXT("RootShakePattern")))
{
	bSingleInstance = true;
}
