#pragma once

#include "CoreMinimal.h"
#include "Combat/Equipment/WeaponDefinition.h"
#include "MeleeWeaponDefinition.generated.h"

USTRUCT(BlueprintType)
struct POLYQUEST_API FOwnerMeshMeleeTraceSource
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trace Source", meta = (ToolTip = "角色自身网格体接触源的唯一标识名称（如 RightFist、LeftFist）。"))
	FName TraceSourceName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trace Source", meta = (ToolTip = "接触源挂接的角色骨骼网格体插槽名称。"))
	FName OwnerMeshSocketName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trace Source", meta = (ToolTip = "刀刃/接触根部标记相对于骨骼插槽的局部偏移。"))
	FVector BladeBaseMarkerRelativeLocation = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Trace Source", meta = (ToolTip = "刀刃/接触尖端标记相对于骨骼插槽的局部偏移。"))
	FVector BladeTipMarkerRelativeLocation = FVector::ZeroVector;
};

/**
 * The compatible melee subclass of UWeaponDefinition: blade trace markers in
 * weapon-mesh or owner-socket local space, sweep shape, and the BaseGrantedActions
 * source the current LMB/Sprint base chain relies on. Display, slot, action, Defense
 * Profile, and direct attack route fields are owned by the base class; the promoted
 * field names keep the serialized values of the existing DataAssets.
 */
UCLASS(BlueprintType)
class POLYQUEST_API UMeleeWeaponDefinition : public UWeaponDefinition
{
	GENERATED_BODY()

public:
	virtual bool IsValidWeaponDefinition(FString& OutReason) const override;

	/**
	 * Validates this definition purely as a Static Mesh trace geometry provider (for static enemy display binding).
	 * Ignores player-specific fields (BaseGrantedActions, DefenseProfile, PrimaryAttackAbilityTag, AttachSocketName, etc.).
	 */
	bool IsValidStaticMeshTraceGeometry(FString& OutReason) const;

	/**
	 * Trace markers resolve against the character SkeletalMesh socket named by
	 * AttachSocketName instead of a spawned weapon display: the Unarmed
	 * hand-contact source. Bidirectionally validated against WeaponMesh so the
	 * two contact sources are never combined or both omitted.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Trace Markers", meta = (ToolTip = "为 true 时判定点依附于角色骨骼插槽（如空手拳击），为 false 时依附于武器显示网格体；启用时不可同时配置 WeaponMesh 或武器静态网格插槽。"))
	bool bUseOwnerMeshSocketForTrace = false;

	/** Blade-root marker spawned relative to the display mesh, or to the owner-mesh socket when bUseOwnerMeshSocketForTrace is set. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Trace Markers", meta = (ToolTip = "刀刃根部追踪标记相对于武器显示网格体或角色插槽的局部偏移。"))
	FVector BladeBaseMarkerRelativeLocation = FVector::ZeroVector;

	/** Blade-tip marker spawned relative to the display mesh, or to the owner-mesh socket when bUseOwnerMeshSocketForTrace is set; must differ from the base marker. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Trace Markers", meta = (ToolTip = "刀刃尖端追踪标记相对于武器显示网格体或角色插槽的局部偏移；必须与根部标记不同。"))
	FVector BladeTipMarkerRelativeLocation = FVector::ZeroVector;

	/** Authored named owner-mesh contact sources (e.g. RightFist, LeftFist). Valid only with bUseOwnerMeshSocketForTrace. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Trace Profiles", meta = (ToolTip = "角色骨骼网格体多接触源配置列表，仅在 bUseOwnerMeshSocketForTrace 为 true 时生效。"))
	TArray<FOwnerMeshMeleeTraceSource> OwnerMeshTraceSources;

	/** Default trace source name resolved when no explicit source is requested or for legacy queries. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Trace Profiles", meta = (ToolTip = "未指定接触源时默认使用的配置源名称；OwnerMeshTraceSources 非空时必须指向列表中的一项。"))
	FName DefaultOwnerMeshTraceSourceName = NAME_None;

	/** Resolves a requested trace source name to a valid authored profile source name. */
	bool TryResolveTraceSourceName(FName RequestedSourceName, FName& OutResolvedSourceName) const;

	/** Named Static Mesh socket for the blade root/base marker. If set, BladeTipSocketName must also be set and resolve to a valid socket on WeaponMesh. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Trace Sockets", meta = (ToolTip = "武器显示网格体上的刀刃根部插槽名称；与 BladeTipSocketName 成对生效。"))
	FName BladeBaseSocketName = NAME_None;

	/** Named Static Mesh socket for the blade tip marker. If set, BladeBaseSocketName must also be set and resolve to a valid socket on WeaponMesh. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Trace Sockets", meta = (ToolTip = "武器显示网格体上的刀刃尖端插槽名称；与 BladeBaseSocketName 成对生效。"))
	FName BladeTipSocketName = NAME_None;

	/** Returns true if this displayed melee weapon opts into Static Mesh trace socket attachment. */
	bool UsesDisplayMeshTraceSockets() const;

	/** Sweep sphere radius for this weapon's melee trace. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Trace", meta = (ClampMin = "0.01", ToolTip = "近战判定球扫掠半径（厘米）。"))
	float TraceRadius = 12.0f;

	/** Sphere-sweep samples along this weapon's blade. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Trace", meta = (ClampMin = "1", ClampMax = "8", ToolTip = "刀刃根部到尖端之间的球扫掠采样插值段数。"))
	int32 BladeSubdivisions = 4;

	/** Target snap displacement distance used by front/backstab execution alignment (cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Execution", meta = (ClampMin = "1.0", Units = "Centimeters", ToolTip = "处决对齐目标锚点位移距离（厘米）。"))
	float ExecutionSnapDistance = 190.0f;
};
