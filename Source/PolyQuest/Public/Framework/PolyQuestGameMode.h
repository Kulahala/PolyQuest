// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PolyQuestGameMode.generated.h"

/**
 * Product GameMode root configured by BP_GameMode.
 */
UCLASS(Abstract)
class POLYQUEST_API APolyQuestGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	/** Constructor */
	APolyQuestGameMode();
};
