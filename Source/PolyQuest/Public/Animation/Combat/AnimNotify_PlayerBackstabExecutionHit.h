#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_PlayerBackstabExecutionHit.generated.h"

/**
 * Sends the semantic hit timing event for the player backstab execution ability.
 * Payload passes Instigator = Owner, Target = Owner, OptionalObject = Animation.
 */
UCLASS()
class POLYQUEST_API UAnimNotify_PlayerBackstabExecutionHit : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
};
