#include "Tests/TestManagedMontageAbility.h"

FGameplayAbilityTargetDataHandle FManagedMontageTestHelpers::MakeRateWindowTargetData(UAnimInstance* AnimInstance, int32 MontageInstanceID)
{
	FGameplayAbilityTargetData_MontageRateWindowSource* Data = new FGameplayAbilityTargetData_MontageRateWindowSource();
	Data->AnimInstance = AnimInstance;
	Data->MontageInstanceID = MontageInstanceID;
	return FGameplayAbilityTargetDataHandle(Data);
}

FGameplayEventData FManagedMontageTestHelpers::MakeRateWindowEventData(
	const FGameplayTag& EventTag,
	UAnimInstance* AnimInstance,
	int32 MontageInstanceID,
	float Rate,
	AActor* AvatarActor,
	UObject* Animation,
	const UObject* RateNotify)
{
	FGameplayEventData EventData;
	EventData.EventTag = EventTag;
	EventData.EventMagnitude = Rate;

	AActor* EffectiveActor = AvatarActor;
	if (!EffectiveActor && AnimInstance)
	{
		EffectiveActor = AnimInstance->GetOwningActor();
	}
	EventData.Instigator = EffectiveActor;
	EventData.Target = EffectiveActor;
	EventData.OptionalObject = Animation;
	EventData.OptionalObject2 = RateNotify;

	EventData.TargetData = MakeRateWindowTargetData(AnimInstance, MontageInstanceID);
	return EventData;
}

UTestManagedMontageAbility::UTestManagedMontageAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;

	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Action.Attacking"), false));
	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Input.Block.Movement"), false));
}

void UTestManagedMontageAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (TestMontage)
	{
		PlayActionMontage(TestMontage, TestRate, NAME_None, InitialRootMotionScale, 0.0f, bTestAllowInterruptAfterBlendOut);
	}
}

void UTestManagedMontageAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (MontageTask)
	{
		MontageTask->EndTask();
		MontageTask = nullptr;
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

UAbilityTask_PlayActionMontage* UTestManagedMontageAbility::PlayActionMontage(
	UAnimMontage* MontageToPlay,
	float Rate,
	FName StartSection,
	float AnimRootMotionTranslationScale,
	float StartTimeSeconds,
	bool bAllowInterruptAfterBlendOut)
{
	MontageTask = UAbilityTask_PlayActionMontage::PlayActionMontage(
		this,
		NAME_None,
		MontageToPlay,
		Rate,
		StartSection,
		AnimRootMotionTranslationScale,
		StartTimeSeconds,
		bAllowInterruptAfterBlendOut);

	if (MontageTask)
	{
		MontageTask->OnCompleted.AddDynamic(this, &UTestManagedMontageAbility::OnMontageCompleted);
		MontageTask->OnBlendedIn.AddDynamic(this, &UTestManagedMontageAbility::OnMontageBlendedIn);
		MontageTask->OnBlendOut.AddDynamic(this, &UTestManagedMontageAbility::OnMontageBlendOut);
		MontageTask->OnInterrupted.AddDynamic(this, &UTestManagedMontageAbility::OnMontageInterrupted);
		MontageTask->OnCancelled.AddDynamic(this, &UTestManagedMontageAbility::OnMontageCancelled);
		MontageTask->OnFailed.AddDynamic(this, &UTestManagedMontageAbility::OnMontageFailed);
		MontageTask->ReadyForActivation();
	}

	return MontageTask;
}

void UTestManagedMontageAbility::OnMontageCompleted()
{
	bCompletedCalled = true;
	CompletedCallCount++;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UTestManagedMontageAbility::OnMontageBlendedIn()
{
	bBlendedInCalled = true;
	BlendedInCallCount++;
}

void UTestManagedMontageAbility::OnMontageBlendOut()
{
	bBlendOutCalled = true;
	BlendOutCallCount++;
}

void UTestManagedMontageAbility::OnMontageInterrupted()
{
	bInterruptedCalled = true;
	InterruptedCallCount++;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UTestManagedMontageAbility::OnMontageCancelled()
{
	bCancelledCalled = true;
	CancelledCallCount++;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UTestManagedMontageAbility::OnMontageFailed()
{
	bFailedCalled = true;
	FailedCallCount++;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
