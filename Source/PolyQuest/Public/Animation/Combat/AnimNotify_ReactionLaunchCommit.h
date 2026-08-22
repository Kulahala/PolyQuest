#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_ReactionLaunchCommit.generated.h"

/**
 * AnimNotify marking the departure frame of a Launch reaction takeoff.
 * Triggers the Event.Reaction.Launch.Commit gameplay event to commit
 * CharacterMovement-owned flight while the Takeoff Montage holds its airborne pose.
 */
UCLASS()
class POLYQUEST_API UAnimNotify_ReactionLaunchCommit : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
};
