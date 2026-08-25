#include "Combat/Melee/MeleeWeaponTrailComponent.h"

#include "GameFramework/Actor.h"
#include "NiagaraSystem.h"

namespace
{
	const FName BladeBaseParameterName(TEXT("User.BladeBase"));
	const FName BladeTipParameterName(TEXT("User.BladeTip"));
}

UMeleeWeaponTrailComponent::UMeleeWeaponTrailComponent()
{
	bAutoActivate = false;
	bAutoManageAttachment = false;
	SetAutoDestroy(false);
}

bool UMeleeWeaponTrailComponent::GetAutoDestroy() const
{
	static const FBoolProperty* BoolProp = CastField<FBoolProperty>(UNiagaraComponent::StaticClass()->FindPropertyByName(TEXT("bAutoDestroy")));
	return BoolProp ? BoolProp->GetPropertyValue_InContainer(this) : false;
}

void UMeleeWeaponTrailComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (auto& Pair : ChildTrailComponents)
	{
		if (UNiagaraComponent* ChildComp = Pair.Value.Get())
		{
			ChildComp->Deactivate();
			ChildComp->DestroyComponent();
		}
	}
	ChildTrailComponents.Reset();
	SourceActiveRequesters.Reset();

	Super::EndPlay(EndPlayReason);
}

void UMeleeWeaponTrailComponent::StartTrail(const UObject* Requester, const FVector& BladeBase, const FVector& BladeTip)
{
	StartTrail(Requester, NAME_None, BladeBase, BladeTip);
}

void UMeleeWeaponTrailComponent::StartTrail(const UObject* Requester, FName SourceName, const FVector& BladeBase, const FVector& BladeTip)
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bTestTrackingEnabled)
	{
		FTestTrailTrackingState& State = TestTrackingStates.FindOrAdd(SourceName);
		++State.StartCallCount;
		State.LastBladeBase = BladeBase;
		State.LastBladeTip = BladeTip;
		State.ActiveRequester = Requester;
		State.bIsActive = true;
		return;
	}
#endif

	if (!GetAsset())
	{
		return;
	}

	SourceActiveRequesters.FindOrAdd(SourceName) = Requester;

	if (SourceName.IsNone())
	{
		SetBladeEndpoints(BladeBase, BladeTip);
		Activate(true);
	}
	else
	{
		UNiagaraComponent* ChildComp = nullptr;
		if (TObjectPtr<UNiagaraComponent>* FoundComp = ChildTrailComponents.Find(SourceName))
		{
			ChildComp = FoundComp->Get();
		}

		if (!ChildComp)
		{
			AActor* OwnerActor = GetOwner();
			if (!OwnerActor)
			{
				return;
			}

			ChildComp = NewObject<UNiagaraComponent>(OwnerActor, NAME_None, RF_Transient);
			ChildComp->SetAsset(GetAsset());
			ChildComp->bAutoActivate = false;
			ChildComp->bAutoManageAttachment = false;
			ChildComp->SetAutoDestroy(false);
			ChildComp->RegisterComponent();
			if (USceneComponent* RootComp = OwnerActor->GetRootComponent())
			{
				ChildComp->AttachToComponent(RootComp, FAttachmentTransformRules::KeepRelativeTransform);
			}
			ChildTrailComponents.Add(SourceName, ChildComp);
		}

		ChildComp->SetVariablePosition(BladeBaseParameterName, BladeBase);
		ChildComp->SetVariablePosition(BladeTipParameterName, BladeTip);
		ChildComp->Activate(true);
	}
}

void UMeleeWeaponTrailComponent::UpdateTrail(const UObject* Requester, const FVector& BladeBase, const FVector& BladeTip)
{
	UpdateTrail(Requester, NAME_None, BladeBase, BladeTip);
}

void UMeleeWeaponTrailComponent::UpdateTrail(const UObject* Requester, FName SourceName, const FVector& BladeBase, const FVector& BladeTip)
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bTestTrackingEnabled)
	{
		if (FTestTrailTrackingState* State = TestTrackingStates.Find(SourceName))
		{
			if (State->ActiveRequester.Get() == Requester)
			{
				++State->UpdateCallCount;
				State->LastBladeBase = BladeBase;
				State->LastBladeTip = BladeTip;
			}
		}
		return;
	}
#endif

	if (!GetAsset())
	{
		return;
	}

	const TWeakObjectPtr<const UObject>* FoundRequester = SourceActiveRequesters.Find(SourceName);
	if (!FoundRequester || FoundRequester->Get() != Requester)
	{
		return;
	}

	if (SourceName.IsNone())
	{
		SetBladeEndpoints(BladeBase, BladeTip);
	}
	else
	{
		if (TObjectPtr<UNiagaraComponent>* FoundComp = ChildTrailComponents.Find(SourceName))
		{
			if (UNiagaraComponent* ChildComp = FoundComp->Get())
			{
				ChildComp->SetVariablePosition(BladeBaseParameterName, BladeBase);
				ChildComp->SetVariablePosition(BladeTipParameterName, BladeTip);
			}
		}
	}
}

void UMeleeWeaponTrailComponent::EndTrail(const UObject* Requester, FName SourceName)
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bTestTrackingEnabled)
	{
		if (FTestTrailTrackingState* State = TestTrackingStates.Find(SourceName))
		{
			if (State->ActiveRequester.Get() == Requester)
			{
				++State->EndCallCount;
				State->ActiveRequester.Reset();
				State->bIsActive = false;
			}
		}
		return;
	}
#endif

	const TWeakObjectPtr<const UObject>* FoundRequester = SourceActiveRequesters.Find(SourceName);
	if (!FoundRequester || FoundRequester->Get() != Requester)
	{
		return;
	}

	SourceActiveRequesters.Remove(SourceName);

	if (GetAsset())
	{
		if (SourceName.IsNone())
		{
			Deactivate();
		}
		else
		{
			if (TObjectPtr<UNiagaraComponent>* FoundComp = ChildTrailComponents.Find(SourceName))
			{
				if (UNiagaraComponent* ChildComp = FoundComp->Get())
				{
					ChildComp->Deactivate();
				}
			}
		}
	}
}

void UMeleeWeaponTrailComponent::SetBladeEndpoints(const FVector& BladeBase, const FVector& BladeTip)
{
	SetVariablePosition(BladeBaseParameterName, BladeBase);
	SetVariablePosition(BladeTipParameterName, BladeTip);
}

#if WITH_DEV_AUTOMATION_TESTS
bool UMeleeWeaponTrailComponent::IsTestTrackingActive(FName SourceName) const
{
	const FTestTrailTrackingState* State = TestTrackingStates.Find(SourceName);
	return State ? State->bIsActive : false;
}

const UObject* UMeleeWeaponTrailComponent::GetTestActiveRequester(FName SourceName) const
{
	const FTestTrailTrackingState* State = TestTrackingStates.Find(SourceName);
	return State ? State->ActiveRequester.Get() : nullptr;
}

FVector UMeleeWeaponTrailComponent::GetTestLastBladeBase(FName SourceName) const
{
	const FTestTrailTrackingState* State = TestTrackingStates.Find(SourceName);
	return State ? State->LastBladeBase : FVector::ZeroVector;
}

FVector UMeleeWeaponTrailComponent::GetTestLastBladeTip(FName SourceName) const
{
	const FTestTrailTrackingState* State = TestTrackingStates.Find(SourceName);
	return State ? State->LastBladeTip : FVector::ZeroVector;
}

int32 UMeleeWeaponTrailComponent::GetTestStartCallCount(FName SourceName) const
{
	const FTestTrailTrackingState* State = TestTrackingStates.Find(SourceName);
	return State ? State->StartCallCount : 0;
}

int32 UMeleeWeaponTrailComponent::GetTestUpdateCallCount(FName SourceName) const
{
	const FTestTrailTrackingState* State = TestTrackingStates.Find(SourceName);
	return State ? State->UpdateCallCount : 0;
}

int32 UMeleeWeaponTrailComponent::GetTestEndCallCount(FName SourceName) const
{
	const FTestTrailTrackingState* State = TestTrackingStates.Find(SourceName);
	return State ? State->EndCallCount : 0;
}
#endif
