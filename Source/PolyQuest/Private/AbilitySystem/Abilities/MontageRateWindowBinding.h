#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystem/Abilities/MontageRateWindowLifecycle.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "GameplayTagContainer.h"

/**
 * Shared stateless helper for binding Montage RateWindow event listeners across player abilities.
 * Preserves exact activation ordering, context identity, token validation, and dual ReadyForActivation checks.
 */
struct FMontageRateWindowBinding
{
	template <typename TAbility, typename TContext>
	static bool Bind(
		TAbility* Ability,
		UAnimInstance* AnimInstance,
		UAnimMontage* Montage,
		const bool& bEndAbilityFlag)
	{
		if (!Ability)
		{
			return false;
		}

		Ability->ClearRateWindow();
		const uint32 BindingToken = ++Ability->RateWindowBindingToken;
		const auto FailBinding = [Ability, BindingToken, &bEndAbilityFlag]()
		{
			if (Ability->RateWindowBindingToken == BindingToken && Ability->IsActive() && !bEndAbilityFlag && Ability->CurrentActorInfo)
			{
				Ability->EndAbility(Ability->CurrentSpecHandle, Ability->CurrentActorInfo, Ability->CurrentActivationInfo, true, true);
			}
			return false;
		};

		if (!Ability->IsActive() || bEndAbilityFlag || !IsValid(AnimInstance) || !IsValid(Montage))
		{
			return FailBinding();
		}
		const FAnimMontageInstance* Instance = AnimInstance->GetActiveInstanceForMontage(Montage);
		if (!Instance || Instance->IsStopped())
		{
			return FailBinding();
		}
		Ability->RateWindowAnimInstance = AnimInstance;
		Ability->RateWindowMontage = Montage;
		Ability->RateWindowMontageInstanceID = Instance->GetInstanceID();
		Ability->RateWindowLifecycle.BindAndCapture(Ability, AnimInstance, Montage, Ability->RateWindowBeginEventTag, Ability->RateWindowEndEventTag);
		if (!Ability->RateWindowLifecycle.IsBound())
		{
			return FailBinding();
		}

		TContext* Context = NewObject<TContext>(Ability);
		Ability->RateWindowContext = Context;
		Context->OwningAbility = Ability;
		Context->Token = BindingToken;
		UAbilityTask_WaitGameplayEvent* BeginTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(Ability, Ability->RateWindowBeginEventTag, nullptr, false, true);
		UAbilityTask_WaitGameplayEvent* EndTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(Ability, Ability->RateWindowEndEventTag, nullptr, false, true);
		Ability->RateWindowBeginTask = BeginTask;
		Ability->RateWindowEndTask = EndTask;
		if (!BeginTask || !EndTask)
		{
			return FailBinding();
		}
		BeginTask->EventReceived.AddDynamic(Context, &TContext::OnBegin);
		EndTask->EventReceived.AddDynamic(Context, &TContext::OnEnd);

		const auto IsCurrentBinding = [Ability, Context, BindingToken, &bEndAbilityFlag]()
		{
			return Ability->IsActive() && !bEndAbilityFlag && Ability->RateWindowBindingToken == BindingToken
				&& Ability->RateWindowContext.Get() == Context;
		};
		BeginTask->ReadyForActivation();
		if (!IsCurrentBinding())
		{
			return false; // A synchronous end/retrigger owns its own cleanup.
		}
		if (Ability->RateWindowBeginTask.Get() != BeginTask || !IsValid(BeginTask) || !BeginTask->IsActive()
			|| Ability->RateWindowEndTask.Get() != EndTask || !IsValid(EndTask) || !Ability->HasOwnedRateWindowMontageInstance())
		{
			return FailBinding();
		}
		EndTask->ReadyForActivation();
		if (!IsCurrentBinding())
		{
			return false;
		}
		if (Ability->RateWindowEndTask.Get() != EndTask || !IsValid(EndTask) || !EndTask->IsActive()
			|| !Ability->HasOwnedRateWindowMontageInstance())
		{
			return FailBinding();
		}
		return true;
	}
};
