#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FloorVolume.generated.h"

class UBoxComponent;
class UMaterialParameterCollection;

/**
 * Spatial floor management volume.
 * Gathers contained actors within its bounds on BeginPlay, classifying them into structural elements
 * (handled via MPC FloorCutoffZ height cutoff) and interior props (hidden directly via SetActorHiddenInGame;
 * collision is strictly preserved).
 */
UCLASS(BlueprintType)
class POLYQUEST_API AFloorVolume : public AActor
{
	GENERATED_BODY()

public:
	AFloorVolume();

	virtual void Tick(float DeltaSeconds) override;

	/** Target floor index represented by this volume (e.g. 2 for 2nd floor). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floor", meta = (ToolTip = "当前空间所代表的楼层索引（如 2 表示二楼）"))
	int32 FloorIndex = 2;

	/** Height cutoff scalar when this floor is active (reveals the floor and overhead ceilings). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floor|Cutoff", meta = (ToolTip = "本楼层激活时允许可见的最高 Z 高度（覆盖本层天花板）"))
	float ActiveCutoffZ = 1000.0f;

	/** Height cutoff scalar when this floor is inactive (cuts off the floor and upper structures). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floor|Cutoff", meta = (ToolTip = "本楼层未激活时允许可见的最高 Z 高度（低于二层步行地面）"))
	float InactiveCutoffZ = 320.0f;

	/** Duration of the smooth cutoff height interpolation (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floor|Cutoff", meta = (ClampMin = "0.01", ToolTip = "高度切面平滑过渡时长（秒）"))
	float FadeDuration = 0.25f;

	/** MPC reference used to push FloorCutoffZ. Defaults to /Game/_Materials/SeeThrough/MPC_PlayerGlobals. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rendering|SeeThrough")
	TObjectPtr<UMaterialParameterCollection> PlayerGlobalsMPC;

	/** Box component defining the spatial bounding box for gathering actors. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Floor")
	TObjectPtr<UBoxComponent> BoundsBox;

	/** Whether this floor is currently active (visible to player). Can be configured per instance for initial state. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floor|State", meta = (ToolTip = "初始/当前楼层是否激活（例如二层实例在关卡初始可设为 false）"))
	bool bIsFloorActive = true;

	/** Whether structural actors (floors, walls, arches) should also be hidden in game when this floor is inactive. Collision is strictly preserved. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floor|Visibility", meta = (ToolTip = "当本层未激活时，结构大件（地板/墙体/拱梁）是否也在游戏里隐藏（物理碰撞依然严格保留）"))
	bool bHideStructuralActorsWhenInactive = true;

	/** Managed interior props (tables, chairs, barrels, torches, chandeliers) - toggled via SetActorHiddenInGame. Collision is NEVER disabled. */
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Floor|State")
	TArray<TWeakObjectPtr<AActor>> ManagedInteriorActors;

	/** Managed structural elements (floors, walls, arches, ceilings) - height-clipped via MPC FloorCutoffZ. */
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Floor|State")
	TArray<TWeakObjectPtr<AActor>> ManagedStructuralActors;

	/** Activates or deactivates this floor's visibility. */
	UFUNCTION(BlueprintCallable, Category = "Floor")
	void SetFloorActive(bool bActive, bool bInstant = false);

	/** Manually trigger actor gathering and classification within bounds. */
	UFUNCTION(BlueprintCallable, Category = "Floor")
	void GatherContainedActors();

	/** Helper to determine if an actor qualifies as a structural element vs interior prop. */
	static bool IsStructuralActor(const AActor* CandidateActor);

	/** Returns current interpolated cutoff Z scalar. */
	UFUNCTION(BlueprintPure, Category = "Floor|Cutoff")
	float GetCurrentCutoffZ() const { return CurrentCutoffZ; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;


private:
	float CurrentCutoffZ = 1000.0f;
	float TargetCutoffZ = 1000.0f;
	bool bIsInterpolating = false;

	void UpdateCutoffZ(float DeltaSeconds);
	void PushCutoffZToMPC(float InCutoffZ);
};
