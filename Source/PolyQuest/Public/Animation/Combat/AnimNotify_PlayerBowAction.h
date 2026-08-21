#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_PlayerBowAction.generated.h"

/** Sends the semantic draw-ready timing event for player bow abilities. */
UCLASS()
class POLYQUEST_API UAnimNotify_PlayerBowDrawReady : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
};

/** Sends the semantic release timing event to spawn a projectile for player bow abilities. */
UCLASS()
class POLYQUEST_API UAnimNotify_PlayerBowRelease : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
};
