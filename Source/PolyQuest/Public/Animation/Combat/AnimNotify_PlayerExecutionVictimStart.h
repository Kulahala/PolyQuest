#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_PlayerExecutionVictimStart.generated.h"

/**
 * Sends the semantic victim start timing event for paired player execution abilities (front & backstab).
 * Dispatches Event.Action.Execution.Request.VictimStart to the player's own AbilitySystemComponent.
 * Payload: Instigator = Owner, Target = Owner, OptionalObject = Animation.
 *
 * NOTE FOR ARTISTS / DESIGNERS:
 * This notify can be placed at frame 0, at the physical contact point, or in the same frame as Hit.
 * It drives victim presentation only and does not directly damage, release, or mutate the target.
 */
UCLASS()
class POLYQUEST_API UAnimNotify_PlayerExecutionVictimStart : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
};
