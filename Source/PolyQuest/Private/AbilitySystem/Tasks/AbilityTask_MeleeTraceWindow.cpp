#include "AbilitySystem/Tasks/AbilityTask_MeleeTraceWindow.h"

#include "AbilitySystemComponent.h"
#include "Combat/Melee/MeleeHitResolver.h"
#include "Combat/Melee/MeleeTraceSourceComponent.h"
#include "Combat/Melee/MeleeWeaponTrailComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "PolyQuest.h"

UAbilityTask_MeleeTraceWindow::UAbilityTask_MeleeTraceWindow()
{
	bTickingTask = true;
}

UAbilityTask_MeleeTraceWindow* UAbilityTask_MeleeTraceWindow::OpenMeleeTraceWindow(
	UGameplayAbility* OwningAbility,
	UMeleeTraceSourceComponent* InTraceSource,
	TSubclassOf<UGameplayEffect> InDamageGameplayEffectClass,
	float InAbilityLevel,
	FGameplayTag InSetByCallerMagnitudeTag,
	float InSetByCallerMagnitude,
	float InGuardStaminaDamage,
	const TArray<FName>& InTraceSourceNames)
{
	UAbilityTask_MeleeTraceWindow* Task = NewAbilityTask<UAbilityTask_MeleeTraceWindow>(OwningAbility);
	Task->TraceSource = InTraceSource;
	Task->DamageGameplayEffectClass = InDamageGameplayEffectClass;
	Task->AbilityLevel = InAbilityLevel;
	Task->SetByCallerMagnitudeTag = InSetByCallerMagnitudeTag;
	Task->SetByCallerMagnitude = InSetByCallerMagnitude;
	Task->GuardStaminaDamage = FMath::Max(InGuardStaminaDamage, 0.0f);
	Task->RequestedTraceSourceNames = InTraceSourceNames;
	return Task;
}

UAbilityTask_MeleeTraceWindow* UAbilityTask_MeleeTraceWindow::OpenMeleeTraceWindow(
	UGameplayAbility* OwningAbility,
	UMeleeTraceSourceComponent* InTraceSource,
	TSubclassOf<UGameplayEffect> InDamageGameplayEffectClass,
	float InAbilityLevel,
	const TMap<FGameplayTag, float>& InSetByCallerMagnitudes,
	const TArray<FName>& InTraceSourceNames)
{
	UAbilityTask_MeleeTraceWindow* Task = NewAbilityTask<UAbilityTask_MeleeTraceWindow>(OwningAbility);
	Task->TraceSource = InTraceSource;
	Task->DamageGameplayEffectClass = InDamageGameplayEffectClass;
	Task->AbilityLevel = InAbilityLevel;
	Task->SetByCallerMagnitudes = InSetByCallerMagnitudes;
	Task->RequestedTraceSourceNames = InTraceSourceNames;
	return Task;
}

void UAbilityTask_MeleeTraceWindow::Activate()
{
	if (!TraceSource || !DamageGameplayEffectClass || !GetAvatarActor() || !AbilitySystemComponent.IsValid())
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Melee trace window could not open for '%s': trace source, damage effect, avatar, and ASC are required."), *GetNameSafe(GetAvatarActor()));
		EndTask();
		return;
	}

	const bool bExplicitSourcesProvided = RequestedTraceSourceNames.Num() > 0;
	TArray<FName> EffectiveSources = RequestedTraceSourceNames;
	if (!bExplicitSourcesProvided)
	{
		EffectiveSources.Add(NAME_None);
	}

	TSet<FName> SeenRequestedSources;
	TSet<FName> SeenResolvedSources;
	TArray<FTraceSourceSample> InitialSamples;
	InitialSamples.Reserve(EffectiveSources.Num());

	for (const FName& SourceName : EffectiveSources)
	{
		if (bExplicitSourcesProvided && SourceName.IsNone())
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("Melee trace window on '%s' rejected explicit NAME_None in non-empty source list."), *GetNameSafe(GetAvatarActor()));
			EndTask();
			return;
		}

		if (SeenRequestedSources.Contains(SourceName))
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("Melee trace window on '%s' rejected duplicate requested source '%s'."), *GetNameSafe(GetAvatarActor()), *SourceName.ToString());
			EndTask();
			return;
		}
		SeenRequestedSources.Add(SourceName);

		FName ResolvedName = NAME_None;
		if (!TraceSource->TryResolveTraceSourceName(SourceName, ResolvedName))
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("Melee trace window on '%s' failed to resolve source '%s'."), *GetNameSafe(GetAvatarActor()), *SourceName.ToString());
			EndTask();
			return;
		}

		if (SeenResolvedSources.Contains(ResolvedName))
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("Melee trace window on '%s' rejected duplicate resolved source '%s' (from requested '%s')."), *GetNameSafe(GetAvatarActor()), *ResolvedName.ToString(), *SourceName.ToString());
			EndTask();
			return;
		}
		SeenResolvedSources.Add(ResolvedName);

		FVector Base = FVector::ZeroVector;
		FVector Tip = FVector::ZeroVector;
		if (!TraceSource->TryGetBladeEndpoints(ResolvedName, Base, Tip))
		{
			EndTask();
			return;
		}

		if (!FMath::IsFinite(Base.X) || !FMath::IsFinite(Base.Y) || !FMath::IsFinite(Base.Z) ||
			!FMath::IsFinite(Tip.X) || !FMath::IsFinite(Tip.Y) || !FMath::IsFinite(Tip.Z) ||
			Base.Equals(Tip, KINDA_SMALL_NUMBER))
		{
			EndTask();
			return;
		}

		FTraceSourceSample Sample;
		Sample.SourceName = ResolvedName;
		Sample.PreviousBladeBase = Base;
		Sample.PreviousBladeTip = Tip;
		InitialSamples.Add(Sample);
	}

	bWindowOpen = true;
	ActiveSourceSamples = MoveTemp(InitialSamples);

	if (AActor* AvatarActor = GetAvatarActor())
	{
		CachedTrailComponent = AvatarActor->FindComponentByClass<UMeleeWeaponTrailComponent>();
		if (UMeleeWeaponTrailComponent* TrailComponent = CachedTrailComponent.Get())
		{
			for (const FTraceSourceSample& Sample : ActiveSourceSamples)
			{
				TrailComponent->StartTrail(this, Sample.SourceName, Sample.PreviousBladeBase, Sample.PreviousBladeTip);
			}
		}
	}
}

void UAbilityTask_MeleeTraceWindow::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);

	if (!bWindowOpen || ActiveSourceSamples.Num() == 0 || !TraceSource || !GetAvatarActor() || !AbilitySystemComponent.IsValid())
	{
		EndTask();
		return;
	}

	TraceCurrentSegment();
}

void UAbilityTask_MeleeTraceWindow::OnDestroy(bool AbilityIsEnding)
{
	ResetWindowState();
	Super::OnDestroy(AbilityIsEnding);
}

void UAbilityTask_MeleeTraceWindow::TraceCurrentSegment()
{
	// Phase 1: All-or-nothing capture and validation for all active sources before any VFX or Sweep
	TArray<TPair<FVector, FVector>> CurrentEndpoints;
	CurrentEndpoints.SetNum(ActiveSourceSamples.Num());

	for (int32 Index = 0; Index < ActiveSourceSamples.Num(); ++Index)
	{
		FVector CurrentBladeBase = FVector::ZeroVector;
		FVector CurrentBladeTip = FVector::ZeroVector;
		if (!TraceSource->TryGetBladeEndpoints(ActiveSourceSamples[Index].SourceName, CurrentBladeBase, CurrentBladeTip))
		{
			ResetWindowState();
			EndTask();
			return;
		}

		if (!FMath::IsFinite(CurrentBladeBase.X) || !FMath::IsFinite(CurrentBladeBase.Y) || !FMath::IsFinite(CurrentBladeBase.Z) ||
			!FMath::IsFinite(CurrentBladeTip.X) || !FMath::IsFinite(CurrentBladeTip.Y) || !FMath::IsFinite(CurrentBladeTip.Z) ||
			CurrentBladeBase.Equals(CurrentBladeTip, KINDA_SMALL_NUMBER))
		{
			ResetWindowState();
			EndTask();
			return;
		}

		CurrentEndpoints[Index] = MakeTuple(CurrentBladeBase, CurrentBladeTip);
	}

	// Phase 2: Forward trail updates for each source
	if (UMeleeWeaponTrailComponent* TrailComponent = CachedTrailComponent.Get())
	{
		for (int32 Index = 0; Index < ActiveSourceSamples.Num(); ++Index)
		{
			TrailComponent->UpdateTrail(this, ActiveSourceSamples[Index].SourceName, CurrentEndpoints[Index].Key, CurrentEndpoints[Index].Value);
		}
	}

	// Phase 3: Sweep and shared hit delivery deduplication across all sources
	AActor* SourceActor = GetAvatarActor();
	UAbilitySystemComponent* SourceAbilitySystemComponent = AbilitySystemComponent.Get();
	UWorld* World = SourceActor ? SourceActor->GetWorld() : nullptr;
	if (!SourceActor || !SourceAbilitySystemComponent || !World)
	{
		ResetWindowState();
		EndTask();
		return;
	}

	const int32 Subdivisions = FMath::Max(1, TraceSource->GetBladeSubdivisions());
	const FCollisionShape CollisionShape = FCollisionShape::MakeSphere(TraceSource->GetTraceRadius());
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MeleeTraceWindow), false, SourceActor);
	QueryParams.AddIgnoredActor(SourceActor);

	for (int32 SourceIndex = 0; SourceIndex < ActiveSourceSamples.Num(); ++SourceIndex)
	{
		const FVector& PrevBase = ActiveSourceSamples[SourceIndex].PreviousBladeBase;
		const FVector& PrevTip = ActiveSourceSamples[SourceIndex].PreviousBladeTip;
		const FVector& CurrBase = CurrentEndpoints[SourceIndex].Key;
		const FVector& CurrTip = CurrentEndpoints[SourceIndex].Value;

		for (int32 SampleIndex = 0; SampleIndex <= Subdivisions; ++SampleIndex)
		{
			const float Alpha = static_cast<float>(SampleIndex) / static_cast<float>(Subdivisions);
			const FVector Start = FMath::Lerp(PrevBase, PrevTip, Alpha);
			const FVector End = FMath::Lerp(CurrBase, CurrTip, Alpha);
			TArray<FHitResult> HitResults;
			World->SweepMultiByChannel(HitResults, Start, End, FQuat::Identity, TraceSource->GetTraceChannel(), CollisionShape, QueryParams);

			for (const FHitResult& HitResult : HitResults)
			{
				AActor* TargetActor = HitResult.GetActor();
				if (!TargetActor || DeliveredTargets.Contains(TargetActor))
				{
					continue;
				}

				FMeleeHitRequest Request;
				Request.SourceActor = SourceActor;
				Request.SourceAbilitySystemComponent = SourceAbilitySystemComponent;
				Request.DamageGameplayEffectClass = DamageGameplayEffectClass;
				Request.AbilityLevel = AbilityLevel;
				Request.SetByCallerMagnitudeTag = SetByCallerMagnitudeTag;
				Request.SetByCallerMagnitude = SetByCallerMagnitude;
				Request.SetByCallerMagnitudes = SetByCallerMagnitudes;
				Request.GuardStaminaDamage = GuardStaminaDamage;
				Request.SourceObject = TraceSource;
				Request.HitResult = HitResult;
				if (FMeleeHitResolver::TryResolveHit(Request))
				{
					DeliveredTargets.Add(TargetActor);
				}
			}
		}

		ActiveSourceSamples[SourceIndex].PreviousBladeBase = CurrBase;
		ActiveSourceSamples[SourceIndex].PreviousBladeTip = CurrTip;
	}
}

void UAbilityTask_MeleeTraceWindow::ResetWindowState()
{
	if (bWindowOpen)
	{
		bWindowOpen = false;
		if (UMeleeWeaponTrailComponent* TrailComponent = CachedTrailComponent.Get())
		{
			for (const FTraceSourceSample& Sample : ActiveSourceSamples)
			{
				TrailComponent->EndTrail(this, Sample.SourceName);
			}
		}
	}

	ActiveSourceSamples.Reset();
	DeliveredTargets.Reset();
	CachedTrailComponent.Reset();
}
