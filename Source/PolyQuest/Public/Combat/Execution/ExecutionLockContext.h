#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "GameplayTagContainer.h"
#include "ExecutionLockContext.generated.h"

class AActor;
class UAbilitySystemComponent;
class UGameplayAbility;
class UEnemyVictimExecutionAbility;
class UExecutionLockContext;

UENUM(BlueprintType)
enum class EExecutionSessionHitState : uint8
{
	Ready,
	Resolving,
	NonLethal,
	DeathPending,
	Finalizing,
	Finalized,
	Failed
};

UENUM(BlueprintType)
enum class EExecutionSessionReleaseState : uint8
{
	NotRequested,
	Requested,
	VictimReleased,
	Finalized,
	Failed
};

/**
 * Stack RAII scope guard ensuring that an execution hit transaction begins and resolves deterministically.
 */
struct POLYQUEST_API FExecutionHitScopeGuard
{
	FExecutionHitScopeGuard(
		UExecutionLockContext* InContext,
		const UObject* InSourceObject,
		const AActor* InSourceActor,
		const UAbilitySystemComponent* InSourceASC,
		const AActor* InTargetActor,
		const UAbilitySystemComponent* InTargetASC);

	FExecutionHitScopeGuard(
		UExecutionLockContext* InContext,
		const AActor* InSourceActor,
		const AActor* InTargetActor);
	~FExecutionHitScopeGuard();

	FExecutionHitScopeGuard(const FExecutionHitScopeGuard&) = delete;
	FExecutionHitScopeGuard& operator=(const FExecutionHitScopeGuard&) = delete;

	bool IsScopeValid() const { return bScopeValid; }
	void SetGESuccess(bool bInSuccess) { bGESuccess = bInSuccess; }
	void SetLethal(bool bInLethal) { bLethal = bInLethal; }
	void SetPendingConfirmed(bool bInConfirmed) { bPendingConfirmed = bInConfirmed; }

private:
	TWeakObjectPtr<UExecutionLockContext> Context;
	bool bScopeValid = false;
	bool bGESuccess = false;
	bool bLethal = false;
	bool bPendingConfirmed = false;
};

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

	/** Begins an authorized hit transaction scope with full credential verification. */
	bool BeginHitScope(
		const UObject* InSourceObject,
		const AActor* InSourceActor,
		const UAbilitySystemComponent* InSourceASC,
		const AActor* InTargetActor,
		const UAbilitySystemComponent* InTargetASC);

	/** Begins an authorized hit transaction scope (convenience overload). */
	bool BeginHitScope(const AActor* InSourceActor, const AActor* InTargetActor);

	/** Completes an authorized hit transaction scope. */
	void CompleteHitScope(bool bLethal, bool bPendingConfirmed);

	/** Aborts an active hit transaction scope. */
	void AbortHitScope();

	/** Begins finalization of execution death (B-stage compatibility). */
	bool BeginFinalization(const UGameplayAbility* InVictimAbility);

	/** Completes finalization of execution death (B-stage compatibility). */
	void CompleteFinalization(const UGameplayAbility* InVictimAbility);

	/** Rolls back finalization to DeathPending if execution death commit failed (B-stage compatibility). */
	void AbortFinalization(const UGameplayAbility* InVictimAbility);

	/** Authorizes and initiates the release sequence from the player ability side. */
	bool TryBeginRelease(
		const UGameplayAbility* InSourceAbility,
		uint32 InToken,
		bool bWasCancelled,
		bool bRequireResolvedHit);

	/** Synchronously called on the victim ability side when formal Release is accepted. */
	bool MarkVictimReleased(const UGameplayAbility* InVictimAbility);

	/** Begins finalization of execution outcome (both lethal and non-lethal). */
	bool BeginOutcomeFinalization(const UGameplayAbility* InVictimAbility);

	/** Completes finalization of execution outcome. */
	void CompleteOutcomeFinalization(const UGameplayAbility* InVictimAbility);

	/** Aborts finalization of execution outcome. */
	void AbortOutcomeFinalization(const UGameplayAbility* InVictimAbility);

	/** Marks that a Release event has been dispatched. */
	void MarkReleaseSent(bool bWasCancelled = false);

	/** Invalidate the session. All subsequent authorization or callback checks will fail-closed. */
	void InvalidateSession();

	bool IsActive() const { return bActive; }
	bool IsVictimAccepted() const { return bVictimAccepted; }
	bool IsReleaseSent() const { return bReleaseSent; }
	bool WasReleaseCancelled() const { return bReleaseWasCancelled; }
	EExecutionSessionHitState GetHitState() const { return HitState; }
	EExecutionSessionReleaseState GetReleaseState() const { return ReleaseState; }
	bool IsVictimReleased() const { return ReleaseState == EExecutionSessionReleaseState::VictimReleased || ReleaseState == EExecutionSessionReleaseState::Finalized; }
	bool IsReleaseRequested() const { return ReleaseState == EExecutionSessionReleaseState::Requested; }
	bool IsResolving() const { return HitState == EExecutionSessionHitState::Resolving; }
	bool IsDeathPending() const { return HitState == EExecutionSessionHitState::DeathPending; }
	bool IsFinalizing() const { return HitState == EExecutionSessionHitState::Finalizing; }
	bool IsFinalized() const { return HitState == EExecutionSessionHitState::Finalized; }

	uint32 GetSourceActivationToken() const { return SourceActivationToken; }
	const FGameplayTag& GetRequestTag() const { return RequestTag; }

	AActor* GetSourceActor() const { return SourceActor.Get(); }
	AActor* GetTargetActor() const { return TargetActor.Get(); }
	UGameplayAbility* GetSourceAbility() const { return SourceAbility.Get(); }
	UGameplayAbility* GetVictimAbility() const { return VictimAbility.Get(); }
	UAbilitySystemComponent* GetSourceASC() const { return SourceASC.Get(); }
	UAbilitySystemComponent* GetVictimASC() const { return VictimASC.Get(); }

#if WITH_DEV_AUTOMATION_TESTS
	void SetTestHitState(EExecutionSessionHitState InState) { HitState = InState; }
	void SetTestReleaseState(EExecutionSessionReleaseState InState) { ReleaseState = InState; }
#endif

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
	EExecutionSessionHitState HitState = EExecutionSessionHitState::Ready;
	EExecutionSessionReleaseState ReleaseState = EExecutionSessionReleaseState::NotRequested;
	bool bVictimAccepted = false;
	bool bActive = false;
	bool bReleaseSent = false;
	bool bReleaseWasCancelled = false;
};
