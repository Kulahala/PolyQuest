#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FloorTriggerVolume.generated.h"

class UBoxComponent;
class AFloorVolume;

/**
 * Floor transition trigger volume placed at entry/exit points (e.g. top and bottom of staircases).
 * Overlaps exclusively with APlayerCharacter to submit deterministic floor transitions with hysteresis.
 */
UCLASS(BlueprintType)
class POLYQUEST_API AFloorTriggerVolume : public AActor
{
	GENERATED_BODY()

public:
	AFloorTriggerVolume();

	/** Target floor index to activate or transition toward upon entry. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floor", meta = (ToolTip = "目标楼层索引（例如 1 为一层，2 为二层）"))
	int32 TargetFloorIndex = 1;

	/** Direct reference to the target floor volume governing this level (optional; auto-resolved if unset). */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Floor", meta = (ToolTip = "所关联的目标楼层管理容器（可选；若未指定则自动查找）"))
	TObjectPtr<AFloorVolume> TargetFloorVolume;

	/** Trigger box component defining the overlap boundary. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Floor")
	TObjectPtr<UBoxComponent> TriggerBox;

	UFUNCTION()
	void HandleTriggerBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

protected:
	virtual void BeginPlay() override;
};
