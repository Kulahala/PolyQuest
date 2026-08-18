#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "EnemyAttackSet.generated.h"

class UEnemyAttackProfile;

/** One authored weighted entry in an enemy attack set. */
USTRUCT(BlueprintType)
struct POLYQUEST_API FEnemyAttackSetEntry
{
	GENERATED_BODY()

	/** The static attack profile configuring Montage, Damage GE, AttackRange, and Cooldown. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Attack")
	TObjectPtr<UEnemyAttackProfile> AttackProfile = nullptr;

	/** Positive selection weight relative to other eligible entries in this set. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Attack", meta = (ClampMin = "0.01"))
	float SelectionWeight = 1.0f;
};

/**
 * Immutable authored attack set for an enemy preset.
 * Owns the engagement range and weighted attack entries.
 * Runtime cooldown and active ability snapshots remain on the controller and ability.
 */
UCLASS(BlueprintType)
class POLYQUEST_API UEnemyAttackSet : public UDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * Rejects empty, null, invalid, duplicate, or non-positive weight entries, and non-positive EngagementRange.
	 * Individual AttackRanges govern approach and execution distance rather than set validity.
	 * Returns true only when the set is fully valid for combat execution.
	 */
	bool IsAttackSetValid(FString& OutReason) const;

	UFUNCTION(BlueprintPure, Category = "Enemy Attack")
	float GetEngagementRange() const { return EngagementRange; }

	const TArray<FEnemyAttackSetEntry>& GetEntries() const { return Entries; }

	/**
	 * Pure weighted profile selection for a target within EngagementRange and normalized random fraction [0, 1].
	 * All valid entries in the set participate in weighted selection without filtering by their individual AttackRange.
	 * Returns nullptr if the set is invalid, target is outside EngagementRange, or fraction is out of range.
	 */
	const UEnemyAttackProfile* SelectAttackProfile(float TargetDistance2D, float NormalizedRandomFraction) const;

#if WITH_DEV_AUTOMATION_TESTS
	void SetTestEngagementRange(float InRange) { EngagementRange = InRange; }
	void AddTestEntry(UEnemyAttackProfile* InProfile, float InWeight)
	{
		FEnemyAttackSetEntry Entry;
		Entry.AttackProfile = InProfile;
		Entry.SelectionWeight = InWeight;
		Entries.Add(Entry);
	}
	void ClearTestEntries() { Entries.Empty(); }
#endif

private:
	/**
	 * The combat approach/engagement distance. The AI controller chases until reaching this distance before requesting attacks.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Attack", meta = (AllowPrivateAccess = "true", ClampMin = "0.01", Units = "Centimeters"))
	float EngagementRange = 200.0f;

	/**
	 * Candidate attacks available in this set.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy Attack", meta = (AllowPrivateAccess = "true"))
	TArray<FEnemyAttackSetEntry> Entries;
};
