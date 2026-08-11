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
