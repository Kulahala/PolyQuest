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
	float InGuardStaminaDamage)
{
	UAbilityTask_MeleeTraceWindow* Task = NewAbilityTask<UAbilityTask_MeleeTraceWindow>(OwningAbility);
	Task->TraceSource = InTraceSource;
	Task->DamageGameplayEffectClass = InDamageGameplayEffectClass;
	Task->AbilityLevel = InAbilityLevel;
	Task->SetByCallerMagnitudeTag = InSetByCallerMagnitudeTag;
	Task->SetByCallerMagnitude = InSetByCallerMagnitude;
	Task->GuardStaminaDamage = FMath::Max(InGuardStaminaDamage, 0.0f);
	return Task;
}

UAbilityTask_MeleeTraceWindow* UAbilityTask_MeleeTraceWindow::OpenMeleeTraceWindow(
	UGameplayAbility* OwningAbility,
	UMeleeTraceSourceComponent* InTraceSource,
	TSubclassOf<UGameplayEffect> InDamageGameplayEffectClass,
	float InAbilityLevel,
	const TMap<FGameplayTag, float>& InSetByCallerMagnitudes)
{
	UAbilityTask_MeleeTraceWindow* Task = NewAbilityTask<UAbilityTask_MeleeTraceWindow>(OwningAbility);
	Task->TraceSource = InTraceSource;
	Task->DamageGameplayEffectClass = InDamageGameplayEffectClass;
	Task->AbilityLevel = InAbilityLevel;
	Task->SetByCallerMagnitudes = InSetByCallerMagnitudes;
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

	if (!CaptureCurrentBladeEndpoints(PreviousBladeBase, PreviousBladeTip))
	{
		EndTask();
		return;
	}

	bWindowOpen = true;
	bHasPreviousBladeSample = true;

	if (AActor* AvatarActor = GetAvatarActor())
	{
		CachedTrailComponent = AvatarActor->FindComponentByClass<UMeleeWeaponTrailComponent>();
		if (UMeleeWeaponTrailComponent* TrailComponent = CachedTrailComponent.Get())
		{
			TrailComponent->StartTrail(this, PreviousBladeBase, PreviousBladeTip);
		}
	}
}

void UAbilityTask_MeleeTraceWindow::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);

	if (!bWindowOpen || !bHasPreviousBladeSample || !TraceSource || !GetAvatarActor() || !AbilitySystemComponent.IsValid())
	{
		EndTask();
		return;
	}

	TraceCurrentSegment();
}

void UAbilityTask_MeleeTraceWindow::OnDestroy(bool AbilityIsEnding)
{
	if (UMeleeWeaponTrailComponent* TrailComponent = CachedTrailComponent.Get())
	{
		TrailComponent->EndTrail(this);
	}
	CachedTrailComponent.Reset();

	ResetWindowState();
	Super::OnDestroy(AbilityIsEnding);
}

bool UAbilityTask_MeleeTraceWindow::CaptureCurrentBladeEndpoints(FVector& OutBladeBase, FVector& OutBladeTip) const
{
	return TraceSource && TraceSource->TryGetBladeEndpoints(OutBladeBase, OutBladeTip);
}

void UAbilityTask_MeleeTraceWindow::TraceCurrentSegment()
{
	FVector CurrentBladeBase;
	FVector CurrentBladeTip;
	if (!CaptureCurrentBladeEndpoints(CurrentBladeBase, CurrentBladeTip))
	{
		EndTask();
		return;
	}

	if (UMeleeWeaponTrailComponent* TrailComponent = CachedTrailComponent.Get())
	{
		TrailComponent->UpdateTrail(this, CurrentBladeBase, CurrentBladeTip);
	}

	AActor* SourceActor = GetAvatarActor();
	UAbilitySystemComponent* SourceAbilitySystemComponent = AbilitySystemComponent.Get();
	UWorld* World = SourceActor ? SourceActor->GetWorld() : nullptr;
	if (!SourceActor || !SourceAbilitySystemComponent || !World)
	{
		EndTask();
		return;
	}

	const int32 Subdivisions = FMath::Max(1, TraceSource->GetBladeSubdivisions());
	const FCollisionShape CollisionShape = FCollisionShape::MakeSphere(TraceSource->GetTraceRadius());
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MeleeTraceWindow), false, SourceActor);
	QueryParams.AddIgnoredActor(SourceActor);

	for (int32 SampleIndex = 0; SampleIndex <= Subdivisions; ++SampleIndex)
	{
		const float Alpha = static_cast<float>(SampleIndex) / static_cast<float>(Subdivisions);
		const FVector Start = FMath::Lerp(PreviousBladeBase, PreviousBladeTip, Alpha);
		const FVector End = FMath::Lerp(CurrentBladeBase, CurrentBladeTip, Alpha);
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

	PreviousBladeBase = CurrentBladeBase;
	PreviousBladeTip = CurrentBladeTip;
}

void UAbilityTask_MeleeTraceWindow::ResetWindowState()
{
	bWindowOpen = false;
	bHasPreviousBladeSample = false;
	PreviousBladeBase = FVector::ZeroVector;
	PreviousBladeTip = FVector::ZeroVector;
	DeliveredTargets.Reset();
	CachedTrailComponent.Reset();
}
