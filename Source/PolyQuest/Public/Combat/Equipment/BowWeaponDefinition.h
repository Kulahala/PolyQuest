#pragma once

#include "CoreMinimal.h"
#include "Combat/Equipment/WeaponDefinition.h"
#include "BowWeaponDefinition.generated.h"

class UProjectileDefinition;

/**
 * Authored weapon definition for player bows (two-handed ranged weapon).
 * References a default projectile definition and launch socket on the weapon mesh.
 * Owns no runtime projectile handles, targets, or flight state.
 */
UCLASS(BlueprintType)
class POLYQUEST_API UBowWeaponDefinition : public UWeaponDefinition
{
	GENERATED_BODY()

public:
	UBowWeaponDefinition();

	virtual bool IsValidWeaponDefinition(FString& OutReason) const override;

	/** The default projectile definition spawned by this bow when no custom projectile is selected. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Bow", meta = (ToolTip = "该弓默认发射的投射物数据定义资产（ProjectileDefinition）。"))
	TObjectPtr<UProjectileDefinition> DefaultProjectileDefinition;

	/** Socket name on the WeaponMesh from which projectiles are launched. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Bow", meta = (ToolTip = "武器显示网格体上用于生成投射物的发射插槽名称。"))
	FName LaunchSocketName = TEXT("Socket_Arrow");
};
