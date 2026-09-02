#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "GameplayTagContainer.h"
#include "ExecutionLockContext.generated.h"

class AActor;
class UAbilitySystemComponent;
class UGameplayAbility;

/**
 * Transient execution context representing a synchronized paired execution session
 * between a player attacker ability and an enemy victim ability.
 */
UCLASS(Transient)
class POLYQUEST_API UExecutionLockContext : public UObject
{
	GENERATED_BODY()

public:
	UExecutionLockContext();

	/** Initializes the session on the source player ability side. */
	void InitializeSession(
		UGameplayAbility* InSourceAbility,
		AActor* InSourceActor,
		UAbilitySystemComponent* InSourceASC,
		AActor* InTargetActor,
		const FGameplayTag& InRequestTag,
		uint32 InActivationToken);

	/** Invoked synchronously by the target victim ability to accept the execution lock. */
	bool AcceptVictim(
		UGameplayAbility* InVictimAbility,
		AActor* InVictimActor,
		UAbilitySystemComponent* InVictimASC,
		const FGameplayTag& InRequestTag,
		uint32 InActivationToken);

	/** Checks if the session is active, valid, and matches the given ability and activation token. */
	bool IsCurrent(const UGameplayAbility* InAbility, uint32 InToken) const;

	/** Verifies that a melee hit request is fully authorized by this execution session. */
	bool IsHitAuthorized(
		const UObject* InSourceObject,
		const AActor* InSourceActor,
		const UAbilitySystemComponent* InSourceASC,
		const AActor* InTargetActor,
		const UAbilitySystemComponent* InTargetASC) const;

	/** Marks that a Release event has been dispatched. */
	void MarkReleaseSent();

	/** Invalidate the session. All subsequent authorization or callback checks will fail-closed. */
	void InvalidateSession();

	bool IsActive() const { return bActive; }
	bool IsVictimAccepted() const { return bVictimAccepted; }
	bool IsReleaseSent() const { return bReleaseSent; }
	uint32 GetSourceActivationToken() const { return SourceActivationToken; }
	const FGameplayTag& GetRequestTag() const { return RequestTag; }

	AActor* GetSourceActor() const { return SourceActor.Get(); }
	AActor* GetTargetActor() const { return TargetActor.Get(); }
	UGameplayAbility* GetSourceAbility() const { return SourceAbility.Get(); }
	UGameplayAbility* GetVictimAbility() const { return VictimAbility.Get(); }
	UAbilitySystemComponent* GetSourceASC() const { return SourceASC.Get(); }
	UAbilitySystemComponent* GetVictimASC() const { return VictimASC.Get(); }

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> SourceActor;

	UPROPERTY(Transient)
	TWeakObjectPtr<UGameplayAbility> SourceAbility;

	UPROPERTY(Transient)
	TWeakObjectPtr<UAbilitySystemComponent> SourceASC;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> TargetActor;

	UPROPERTY(Transient)
	TWeakObjectPtr<UGameplayAbility> VictimAbility;

	UPROPERTY(Transient)
	TWeakObjectPtr<UAbilitySystemComponent> VictimASC;

	FGameplayTag RequestTag;
	uint32 SourceActivationToken = 0;
	bool bVictimAccepted = false;
	bool bActive = false;
	bool bReleaseSent = false;
};
