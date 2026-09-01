#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Combat/Equipment/DefenseProfileDefinition.h"
#include "WeaponDefinition.generated.h"

class UStaticMesh;

/** Which hand slot one authored weapon definition occupies. */
UENUM(BlueprintType)
enum class EWeaponHandSlot : uint8
{
	/** A one-handed item carried in the main hand; leaves the off-hand free. */
	MainHandOneHanded,
	/** A two-handed main-hand item that atomically reserves the off-hand (Bow, Staff, Greatsword). */
	MainHandTwoHanded,
	/** An off-hand item (Shield); it may not coexist with a TwoHanded main hand. */
	OffHand
};

/** The authored locomotion presentation mode for a weapon family. */
UENUM(BlueprintType)
enum class EWeaponLocomotionMode : uint8
{
	Default = 0,
	LightSword = 1,
	HeavySword = 2,

	/** Retains Bow's serialized value 4 while filling the index gap for BlendListByEnum. */
	Deprecated_Reserved = 3 UMETA(Hidden),

	Bow = 4
};

/**
 * The authored base of every player equipment item: hand-slot occupancy,
 * display attachment, combat-action candidate ability classes, the default
 * prepared layout, the optional Defense Profile, and direct attack routes.
 * Subclasses add combat geometry; this base owns no runtime state.
 */
UCLASS(Abstract)
class POLYQUEST_API UWeaponDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Validates slot/display/composition rules; subclasses extend with their combat geometry. */
	virtual bool IsValidWeaponDefinition(FString& OutReason) const;

	/** Which hand slot this item occupies when equipped. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Slot", meta = (ToolTip = "装备占用手部槽位类型（主手单手/主手双手/副手）。"))
	EWeaponHandSlot HandSlot = EWeaponHandSlot::MainHandOneHanded;

	/** Base locomotion mode authored on this weapon definition. Default for unarmed and generic weapons. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Locomotion", meta = (ToolTip = "该武器族系的基础移动姿态模式（Default/LightSword/HeavySword/Bow）。"))
	EWeaponLocomotionMode LocomotionMode = EWeaponLocomotionMode::Default;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Display", meta = (ToolTip = "装备时生成的武器显示静态网格体资产。"))
	TObjectPtr<UStaticMesh> WeaponMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Display", meta = (ToolTip = "武器显示网格体附着到角色骨骼网格体上的插槽名称（如 Weapon_R 或 Weapon_L）。"))
	FName AttachSocketName = TEXT("Weapon_R");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Display", meta = (ToolTip = "武器显示网格体相对于附着插槽的局部位置偏移。"))
	FVector DisplayLocationOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Display", meta = (ToolTip = "武器显示网格体相对于附着插槽的局部旋转偏移。"))
	FRotator DisplayRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Display", meta = (ToolTip = "装备时武器显示网格体的缩放比例；显示网格 Socket、近战判定点和弓箭发射位置会随之缩放。世界拾取物缩放由 WorldPickupDisplayTransform 单独控制。"))
	FVector DisplayScale = FVector::OneVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|World Pickup", meta = (ToolTip = "武器在未装备（世界拾取物）状态下的相对显示变换（位置、旋转、缩放）。"))
	FTransform WorldPickupDisplayTransform = FTransform::Identity;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|World Pickup", meta = (ToolTip = "世界拾取物交互提示中显示的武器名称；留空则回退到基础提示。"))
	FText InteractionDisplayName;

	/** Candidate grouping only: these ability classes join the same runtime candidate union as ExclusiveCombatActions. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Combat Actions", meta = (ToolTip = "可复用战斗技能候选 Ability 类列表，运行时与独占列表合并为候选技能池。"))
	TArray<TSubclassOf<UGameplayAbility>> ReusableCombatActions;

	/** Compat-retained candidate grouping merged into the same runtime candidate union as ReusableCombatActions; no exclusivity rule is implemented in v1 (real exclusive selection belongs to TODO-03D1). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Combat Actions", meta = (ToolTip = "专属战斗技能候选 Ability 类列表，运行时与可复用列表合并为候选技能池。"))
	TArray<TSubclassOf<UGameplayAbility>> ExclusiveCombatActions;

	/** The always-granted-while-equipped ability chain (Light/Charged/Sprint Attack per weapon family); validated disjoint from the candidate lists. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Combat Actions", meta = (ToolTip = "装备此武器期间常驻授予的基础攻击 Ability 列表（如轻攻击/蓄力攻击/冲刺攻击），与候选列表互斥。"))
	TArray<TSubclassOf<UGameplayAbility>> BaseGrantedActions;

	/** The initial 1-4 layout; at most four entries, null entries are legal no-op slots, and duplicate non-null classes are rejected. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Prepared Slots", meta = (ToolTip = "初始 1-4 号快捷技能槽位配置，最多 4 项，项必须属于候选技能列表或留空。"))
	TArray<TSubclassOf<UGameplayAbility>> DefaultPreparedActions;

	/** Optional Defense Profile; effective as fallback from the main hand and as override from the off hand. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Defense", meta = (ToolTip = "可选防御配置资产；主手作为防御回退源，副手作为防御覆盖源。"))
	TObjectPtr<UDefenseProfileDefinition> DefenseProfile;

	/** Direct canonical ability tag routed by Input.PrimaryAttack while this weapon is equipped in the main hand. Must match exactly one BaseGrantedAction CDO. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Combat Actions", meta = (ToolTip = "主手装备时通过普通攻击输入（Input.PrimaryAttack）路由的规范 Ability Tag；必须精确匹配 BaseGrantedActions 中的恰好一个 Ability。"))
	FGameplayTag PrimaryAttackAbilityTag;

	/** Direct optional ability tag routed by Sprint Attack while this weapon is equipped in the main hand. If valid, must match exactly one BaseGrantedAction CDO. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Combat Actions", meta = (ToolTip = "主手装备时通过冲刺攻击路由的可选 Ability Tag；若配置，必须精确匹配 BaseGrantedActions 中的恰好一个 Ability。留空则回退到普通攻击路由。"))
	FGameplayTag SprintAttackAbilityTag;
};

inline bool UWeaponDefinition::IsValidWeaponDefinition(FString& OutReason) const
{
	OutReason.Empty();

	if (!FMath::IsFinite(DisplayScale.X) || !FMath::IsFinite(DisplayScale.Y) || !FMath::IsFinite(DisplayScale.Z))
	{
		OutReason = TEXT("DisplayScale contains non-finite components.");
		return false;
	}

	if (DisplayScale.X <= 0.0f || DisplayScale.Y <= 0.0f || DisplayScale.Z <= 0.0f)
	{
		OutReason = TEXT("DisplayScale components must be strictly positive.");
		return false;
	}

	if (WorldPickupDisplayTransform.ContainsNaN())
	{
		OutReason = TEXT("WorldPickupDisplayTransform contains NaN or non-finite values.");
		return false;
	}

	const FVector PickupTranslation = WorldPickupDisplayTransform.GetTranslation();
	if (!FMath::IsFinite(PickupTranslation.X) || !FMath::IsFinite(PickupTranslation.Y) || !FMath::IsFinite(PickupTranslation.Z))
	{
		OutReason = TEXT("WorldPickupDisplayTransform contains non-finite translation components.");
		return false;
	}

	const FQuat PickupRotation = WorldPickupDisplayTransform.GetRotation();
	if (!FMath::IsFinite(PickupRotation.X) || !FMath::IsFinite(PickupRotation.Y) || !FMath::IsFinite(PickupRotation.Z) || !FMath::IsFinite(PickupRotation.W) || !PickupRotation.IsNormalized())
	{
		OutReason = TEXT("WorldPickupDisplayTransform contains non-finite or unnormalized rotation components.");
		return false;
	}

	const FVector PickupScale = WorldPickupDisplayTransform.GetScale3D();
	if (!FMath::IsFinite(PickupScale.X) || !FMath::IsFinite(PickupScale.Y) || !FMath::IsFinite(PickupScale.Z))
	{
		OutReason = TEXT("WorldPickupDisplayTransform contains non-finite scale components.");
		return false;
	}

	if (AttachSocketName.IsNone())
	{
		OutReason = TEXT("AttachSocketName is not set.");
		return false;
	}

	if (LocomotionMode != EWeaponLocomotionMode::Default
		&& LocomotionMode != EWeaponLocomotionMode::LightSword
		&& LocomotionMode != EWeaponLocomotionMode::HeavySword
		&& LocomotionMode != EWeaponLocomotionMode::Bow)
	{
		OutReason = TEXT("LocomotionMode contains an invalid enum value.");
		return false;
	}

	// Reusable and Exclusive are authoring groupings only; the runtime candidate
	// pool is this one union with no exclusivity rule.
	TSet<TSubclassOf<UGameplayAbility>> CandidateClasses;
	auto AppendCandidateClasses = [&CandidateClasses](const TArray<TSubclassOf<UGameplayAbility>>& Classes, const TCHAR* ListName, FString& Reason) -> bool
	{
		for (const TSubclassOf<UGameplayAbility>& AbilityClass : Classes)
		{
			if (!AbilityClass)
			{
				Reason = FString::Printf(TEXT("%s contains a null entry."), ListName);
				return false;
			}

			if (CandidateClasses.Contains(AbilityClass))
			{
				Reason = FString::Printf(TEXT("%s contains an ability class that already appears in the candidate lists."), ListName);
				return false;
			}

			CandidateClasses.Add(AbilityClass);
		}

		return true;
	};

	if (!AppendCandidateClasses(ReusableCombatActions, TEXT("ReusableCombatActions"), OutReason)
		|| !AppendCandidateClasses(ExclusiveCombatActions, TEXT("ExclusiveCombatActions"), OutReason))
	{
		return false;
	}

	TSet<TSubclassOf<UGameplayAbility>> BaseClasses;
	for (const TSubclassOf<UGameplayAbility>& AbilityClass : BaseGrantedActions)
	{
		if (!AbilityClass)
		{
			OutReason = TEXT("BaseGrantedActions contains a null entry.");
			return false;
		}

		if (BaseClasses.Contains(AbilityClass) || CandidateClasses.Contains(AbilityClass))
		{
			OutReason = TEXT("BaseGrantedActions contains a duplicate ability class or one that is also a combat-action candidate.");
			return false;
		}

		BaseClasses.Add(AbilityClass);
	}

	if (DefaultPreparedActions.Num() > 4)
	{
		OutReason = TEXT("DefaultPreparedActions must hold at most four entries.");
		return false;
	}

	TSet<TSubclassOf<UGameplayAbility>> SeenPreparedClasses;
	for (const TSubclassOf<UGameplayAbility>& PreparedClass : DefaultPreparedActions)
	{
		if (!PreparedClass)
		{
			// A null default entry is a legal no-op prepared slot.
			continue;
		}

		if (!CandidateClasses.Contains(PreparedClass))
		{
			OutReason = TEXT("DefaultPreparedActions contains an ability class outside the weapon's candidate lists.");
			return false;
		}

		if (SeenPreparedClasses.Contains(PreparedClass))
		{
			OutReason = TEXT("DefaultPreparedActions contains a duplicate ability class.");
			return false;
		}

		SeenPreparedClasses.Add(PreparedClass);
	}

	if (HandSlot == EWeaponHandSlot::OffHand)
	{
		if (PrimaryAttackAbilityTag.IsValid())
		{
			OutReason = TEXT("OffHand weapons must not configure PrimaryAttackAbilityTag.");
			return false;
		}

		if (SprintAttackAbilityTag.IsValid())
		{
			OutReason = TEXT("OffHand weapons must not configure SprintAttackAbilityTag.");
			return false;
		}
	}
	else
	{
		if (!PrimaryAttackAbilityTag.IsValid())
		{
			OutReason = TEXT("MainHand weapons must configure a valid PrimaryAttackAbilityTag.");
			return false;
		}

		int32 PrimaryMatchCount = 0;
		for (const TSubclassOf<UGameplayAbility>& ActionClass : BaseGrantedActions)
		{
			if (!ActionClass)
			{
				continue;
			}

			const UGameplayAbility* AbilityCDO = ActionClass.GetDefaultObject();
			if (AbilityCDO && AbilityCDO->AbilityTags.HasTagExact(PrimaryAttackAbilityTag))
			{
				PrimaryMatchCount++;
			}
		}

		if (PrimaryMatchCount != 1)
		{
			OutReason = FString::Printf(TEXT("MainHand weapon must have exactly one BaseGrantedAction matching PrimaryAttackAbilityTag '%s' (found %d)."),
				*PrimaryAttackAbilityTag.ToString(), PrimaryMatchCount);
			return false;
		}

		if (SprintAttackAbilityTag.IsValid())
		{
			int32 SprintMatchCount = 0;
			for (const TSubclassOf<UGameplayAbility>& ActionClass : BaseGrantedActions)
			{
				if (!ActionClass)
				{
					continue;
				}

				const UGameplayAbility* AbilityCDO = ActionClass.GetDefaultObject();
				if (AbilityCDO && AbilityCDO->AbilityTags.HasTagExact(SprintAttackAbilityTag))
				{
					SprintMatchCount++;
				}
			}

			if (SprintMatchCount != 1)
			{
				OutReason = FString::Printf(TEXT("SprintAttackAbilityTag '%s' must match exactly one BaseGrantedAction (found %d)."),
					*SprintAttackAbilityTag.ToString(), SprintMatchCount);
				return false;
			}
		}
	}

	if (DefenseProfile && !DefenseProfile->IsProfileValid())
	{
		OutReason = TEXT("DefenseProfile has an incomplete Guard/Parry tag mapping.");
		return false;
	}

	return true;
}
