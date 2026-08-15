#pragma once

#include "CoreMinimal.h"
#include "Character/BaseCharacter.h"
#include "EnemyCharacter.generated.h"

class UEnemyAttackProfile;

/**
 * First native enemy endpoint. It inherits the shared ASC, melee trace source,
 * and startup-ability grant lifecycle from ABaseCharacter.
 */
UCLASS(Blueprintable)
class POLYQUEST_API AEnemyCharacter : public ABaseCharacter
{
	GENERATED_BODY()

public:
	AEnemyCharacter();

	/** Static authored attack input. Runtime cooldown and active attack state do not live on the Character. */
	UFUNCTION(BlueprintPure, Category = "Combat|Enemy")
	UEnemyAttackProfile* GetAttackProfile() const { return AttackProfile; }

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Enemy", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnemyAttackProfile> AttackProfile;
};
