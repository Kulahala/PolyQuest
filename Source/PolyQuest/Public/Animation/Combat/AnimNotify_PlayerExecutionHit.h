#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_PlayerExecutionHit.generated.h"

/**
 * Sends the unified semantic hit timing event for player execution abilities.
 * Payload passes Instigator = Owner, Target = Owner, OptionalObject = Animation.
 */
UCLASS()
class POLYQUEST_API UAnimNotify_PlayerExecutionHit : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
};
