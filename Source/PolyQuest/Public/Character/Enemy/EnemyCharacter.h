#pragma once

#include "CoreMinimal.h"
#include "Character/BaseCharacter.h"
#include "EnemyCharacter.generated.h"

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
};
