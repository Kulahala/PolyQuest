#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CombatTeamAgent.generated.h"

/** Minimal combat-side relation contract used only to reject self and same-team melee hits. */
UINTERFACE(BlueprintType)
class POLYQUEST_API UCombatTeamAgent : public UInterface
{
	GENERATED_BODY()
};

class POLYQUEST_API ICombatTeamAgent
{
	GENERATED_BODY()

public:
	/** Actors with equal team identifiers cannot resolve hits against one another. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combat|Team")
	int32 GetCombatTeamId() const;
};
