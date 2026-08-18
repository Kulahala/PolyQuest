#include "Combat/Enemy/EnemyAttackSet.h"

#include "Combat/Enemy/EnemyAttackProfile.h"

bool UEnemyAttackSet::IsAttackSetValid(FString& OutReason) const
{
	OutReason.Empty();

	if (!FMath::IsFinite(EngagementRange) || EngagementRange <= 0.0f)
	{
		OutReason = FString::Printf(TEXT("AttackSet '%s' has non-positive or non-finite EngagementRange (%f)."), *GetNameSafe(this), EngagementRange);
		return false;
	}

	if (Entries.IsEmpty())
	{
		OutReason = FString::Printf(TEXT("AttackSet '%s' has an empty Entries list."), *GetNameSafe(this));
		return false;
	}

	TSet<const UEnemyAttackProfile*> SeenProfiles;
	bool bHasReachableProfile = false;
	float TotalSetWeight = 0.0f;

	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const FEnemyAttackSetEntry& Entry = Entries[Index];

		if (!Entry.AttackProfile)
		{
			OutReason = FString::Printf(TEXT("AttackSet '%s' entry [%d] has a null AttackProfile."), *GetNameSafe(this), Index);
			return false;
		}

		if (!Entry.AttackProfile->IsValidAttackProfile())
		{
			OutReason = FString::Printf(TEXT("AttackSet '%s' entry [%d] references an invalid AttackProfile '%s'."), *GetNameSafe(this), Index, *GetNameSafe(Entry.AttackProfile));
			return false;
		}

		if (!FMath::IsFinite(Entry.SelectionWeight) || Entry.SelectionWeight <= 0.0f)
		{
			OutReason = FString::Printf(TEXT("AttackSet '%s' entry [%d] has a non-positive or non-finite SelectionWeight (%f)."), *GetNameSafe(this), Index, Entry.SelectionWeight);
			return false;
		}

		if (SeenProfiles.Contains(Entry.AttackProfile.Get()))
		{
			OutReason = FString::Printf(TEXT("AttackSet '%s' entry [%d] contains duplicate AttackProfile '%s'."), *GetNameSafe(this), Index, *GetNameSafe(Entry.AttackProfile));
			return false;
		}
		SeenProfiles.Add(Entry.AttackProfile.Get());

		TotalSetWeight += Entry.SelectionWeight;

		if (Entry.AttackProfile->GetAttackRange() >= EngagementRange)
		{
			bHasReachableProfile = true;
		}
	}

	if (!FMath::IsFinite(TotalSetWeight) || TotalSetWeight <= 0.0f)
	{
		OutReason = FString::Printf(TEXT("AttackSet '%s' has non-positive or non-finite total selection weight (%f)."), *GetNameSafe(this), TotalSetWeight);
		return false;
	}

	if (!bHasReachableProfile)
	{
		OutReason = FString::Printf(TEXT("AttackSet '%s' has no AttackProfile whose AttackRange reaches EngagementRange (%f)."), *GetNameSafe(this), EngagementRange);
		return false;
	}

	return true;
}

const UEnemyAttackProfile* UEnemyAttackSet::SelectAttackProfile(float TargetDistance2D, float NormalizedRandomFraction) const
{
	FString ValidationReason;
	if (!IsAttackSetValid(ValidationReason))
	{
		return nullptr;
	}

	if (!FMath::IsFinite(TargetDistance2D) || TargetDistance2D < 0.0f || TargetDistance2D > EngagementRange)
	{
		return nullptr;
	}

	if (!FMath::IsFinite(NormalizedRandomFraction) || NormalizedRandomFraction < 0.0f || NormalizedRandomFraction > 1.0f)
	{
		return nullptr;
	}

	// Filter eligible profiles whose authored AttackRange covers the current target distance
	TArray<const FEnemyAttackSetEntry*, TInlineAllocator<8>> EligibleEntries;
	float TotalEligibleWeight = 0.0f;

	for (const FEnemyAttackSetEntry& Entry : Entries)
	{
		if (Entry.AttackProfile && Entry.AttackProfile->GetAttackRange() >= TargetDistance2D)
		{
			EligibleEntries.Add(&Entry);
			TotalEligibleWeight += Entry.SelectionWeight;
		}
	}

	if (EligibleEntries.IsEmpty() || !FMath::IsFinite(TotalEligibleWeight) || TotalEligibleWeight <= 0.0f)
	{
		return nullptr;
	}

	const float TargetThreshold = NormalizedRandomFraction * TotalEligibleWeight;
	float AccumulatedWeight = 0.0f;
	const UEnemyAttackProfile* SelectedProfile = nullptr;

	for (const FEnemyAttackSetEntry* EntryPtr : EligibleEntries)
	{
		AccumulatedWeight += EntryPtr->SelectionWeight;
		SelectedProfile = EntryPtr->AttackProfile.Get();
		if (TargetThreshold <= AccumulatedWeight)
		{
			return SelectedProfile;
		}
	}

	return SelectedProfile;
}
