#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ComboChainDataAsset.generated.h"

class UAnimMontage;

USTRUCT(BlueprintType)
struct POLYQUEST_API FComboChainEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combo")
	TObjectPtr<UAnimMontage> Montage = nullptr;

	/** 是否为此连击段落启用近战动态位移扭曲（Motion Warping）吸附修正。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MotionWarping", meta = (ToolTip = "是否为此连击段落启用近战动态位移扭曲（Motion Warping）吸附修正。"))
	bool bUseMotionWarping = false;

	/** 扭曲目标名称，必须与对应动画蒙太奇中 AnimNotifyState_MotionWarping 窗口的 Warp Target Name 严格一致。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MotionWarping", meta = (EditCondition = "bUseMotionWarping", ToolTip = "扭曲目标名称，必须与对应动画蒙太奇中 AnimNotifyState_MotionWarping 窗口的 Warp Target Name 严格一致。"))
	FName WarpTargetName = FName(TEXT("MeleeContact"));

	/** 期望停距（厘米）。吸附计算时人与目标中心的期望距离。若动画自带前踏 Root Motion，需根据步长预留停距以防贴脸。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MotionWarping", meta = (EditCondition = "bUseMotionWarping", ClampMin = "0.0", UIMin = "0.0", ToolTip = "期望停距（厘米）。吸附计算时人与目标中心的期望距离。若动画自带前踏 Root Motion，需根据步长预留停距以防贴脸。"))
	float WarpStopDistance = 190.0f;

	/** 最大滑步吸附距离（厘米）。允许系统向前额外补正的最大位移。最远有效触发距离 = 期望停距 + 此值。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MotionWarping", meta = (EditCondition = "bUseMotionWarping", ClampMin = "0.0", UIMin = "0.0", ToolTip = "最大滑步吸附距离（厘米）。允许系统向前额外补正的最大位移。最远有效触发距离 = 期望停距 + 此值。"))
	float MaxWarpDistance = 110.0f;

	/** 最大允许吸附夹角（度）。玩家水平朝向与目标方向的最大夹角，超出此范围将不触发吸附。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MotionWarping", meta = (EditCondition = "bUseMotionWarping", ClampMin = "0.0", ClampMax = "180.0", UIMin = "0.0", UIMax = "180.0", ToolTip = "最大允许吸附夹角（度）。玩家水平朝向与目标方向的最大夹角，超出此范围将不触发吸附。"))
	float MaxWarpAngleDegrees = 60.0f;
};

/** Authored linear combo entries. Runtime combo state remains in the active Ability. */
UCLASS(BlueprintType)
class POLYQUEST_API UComboChainDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combo", meta = (TitleProperty = "Montage"))
	TArray<FComboChainEntry> Entries;

	int32 GetEntryCount() const
	{
		return Entries.Num();
	}

	const FComboChainEntry* GetEntry(int32 Index) const
	{
		return Entries.IsValidIndex(Index) ? &Entries[Index] : nullptr;
	}
};
