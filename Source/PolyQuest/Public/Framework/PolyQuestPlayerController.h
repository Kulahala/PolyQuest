// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PolyQuestPlayerController.generated.h"

class UInputMappingContext;

/**
 * Product PlayerController root that installs desktop Enhanced Input mappings.
 */
UCLASS(Abstract)
class POLYQUEST_API APolyQuestPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	/** Mapping contexts installed for local desktop players. */
	UPROPERTY(EditAnywhere, Category ="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;
};
