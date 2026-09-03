#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_PlayerExecutionRelease.generated.h"

/**
 * Sends the semantic release request timing event for paired player execution abilities (front & backstab).
 * Dispatches Event.Action.Execution.Request.Release to the player's own AbilitySystemComponent.
 * Payload: Instigator = Owner, Target = Owner, OptionalObject = Animation.
 *
 * NOTE FOR ARTISTS / DESIGNERS:
 * Authored montages must place this Release notify strictly AFTER the execution Hit notify,
 * ideally spaced by at least one montage tick, to ensure clear visual sequencing before lock detachment.
 * Runtime code remains defensively tolerant of out-of-order notify delivery through latching.
 */
UCLASS()
class POLYQUEST_API UAnimNotify_PlayerExecutionRelease : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
};
