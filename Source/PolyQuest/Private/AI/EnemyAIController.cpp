#include "AI/EnemyAIController.h"

#include "AbilitySystemComponent.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Character/Player/PlayerCharacter.h"
#include "Components/StateTreeAIComponent.h"
#include "Combat/Enemy/EnemyAttackSet.h"
#include "Engine/World.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "PolyQuest.h"

AEnemyAIController::AEnemyAIController()
{
	bStartAILogicOnPossess = false;

	EnemyPerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("EnemyPerceptionComponent"));
	SetPerceptionComponent(*EnemyPerceptionComponent);

	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	StateTreeComponent = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTreeComponent"));
	StateTreeComponent->SetStartLogicAutomatically(false);

	EnemyMeleeAbilityTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.Attack.Enemy.Melee")), false);
	AttackingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.Attacking")), false);
	HitReactingStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Action.HitReacting")), false);
	StunnedStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
	TargetAcquiredEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.AI.Target.Acquired")), false);
	TargetLostEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.AI.Target.Lost")), false);

	ConfigureSight();
	EnemyPerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(this, &AEnemyAIController::HandleTargetPerceptionUpdated);
}

void AEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	HomeLocation = InPawn ? InPawn->GetActorLocation() : FVector::ZeroVector;
	ConfigureSight();
	MeleeRange = 0.0f;
	MeleeAttackCooldownEndTime = 0.0f;
	bHasValidAttackSet = false;

	const AEnemyCharacter* EnemyCharacter = Cast<AEnemyCharacter>(InPawn);
	if (EnemyCharacter && EnemyCharacter->IsDead())
	{
		HandleControlledEnemyDeath();
		return;
	}

	const UEnemyAttackSet* AttackSet = EnemyCharacter ? EnemyCharacter->GetAttackSet() : nullptr;
	FString ValidationReason;
	if (!AttackSet || !AttackSet->IsAttackSetValid(ValidationReason))
	{
		if (!bHasLoggedInvalidAttackSet)
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("Enemy AI for '%s' did not start: AttackSet is invalid (%s)."), *GetNameSafe(InPawn), *ValidationReason);
			bHasLoggedInvalidAttackSet = true;
		}
		return;
	}

	if (!EnemyCharacter || !EnemyCharacter->HasValidPoiseRecoveryConfiguration())
	{
		if (!bHasLoggedInvalidPoiseRecoverySetup)
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("Enemy AI for '%s' did not start: Poise recovery requires a recovery GameplayEffect, valid Data.Poise.Recovery tag, positive MaxPoise/rate, and a positive tick interval."), *GetNameSafe(InPawn));
			bHasLoggedInvalidPoiseRecoverySetup = true;
		}
		return;
	}

	MeleeRange = AttackSet->GetEngagementRange();
	bHasValidAttackSet = true;

	if (StateTreeComponent)
	{
		StateTreeComponent->StartLogic();
		if (HasValidCombatTarget())
		{
			SendStateTreeEvent(TargetAcquiredEventTag);
		}
	}
}

void AEnemyAIController::OnUnPossess()
{
	if (StateTreeComponent)
	{
		StateTreeComponent->StopLogic(TEXT("Enemy controller unpossessed."));
	}

	StopMovement();
	ClearCurrentTarget(false);
	MeleeRange = 0.0f;
	MeleeAttackCooldownEndTime = 0.0f;
	bHasValidAttackSet = false;

	Super::OnUnPossess();
}

APlayerCharacter* AEnemyAIController::GetCurrentTarget() const
{
	return CurrentTarget.Get();
}

bool AEnemyAIController::HasValidCombatTarget() const
{
	return !IsControlledEnemyDead() && GetPawn() && IsValid(CurrentTarget.Get()) && CurrentTarget.Get() != GetPawn();
}

bool AEnemyAIController::HasValidAttackSet() const
{
	return !IsControlledEnemyDead() && bHasValidAttackSet && MeleeRange > 0.0f;
}

bool AEnemyAIController::TryGetCurrentTargetDistance2D(float& OutDistance2D) const
{
	OutDistance2D = 0.0f;
	if (!HasValidCombatTarget())
	{
		return false;
	}

	const APawn* ControlledPawn = GetPawn();
	const APlayerCharacter* Target = GetCurrentTarget();
	if (!ControlledPawn || !Target)
	{
		return false;
	}

	OutDistance2D = FVector::Dist2D(ControlledPawn->GetActorLocation(), Target->GetActorLocation());
	return true;
}

void AEnemyAIController::StartMeleeAttackCooldown(float CooldownAfterAttack)
{
	const UWorld* World = GetWorld();
	if (IsControlledEnemyDead() || !World || !FMath::IsFinite(CooldownAfterAttack) || CooldownAfterAttack < 0.0f)
	{
		return;
	}

	MeleeAttackCooldownEndTime = World->GetTimeSeconds() + CooldownAfterAttack;
}

bool AEnemyAIController::IsMeleeAttackOnCooldown() const
{
	const UWorld* World = GetWorld();
	return !IsControlledEnemyDead() && World && MeleeAttackCooldownEndTime > World->GetTimeSeconds();
}

bool AEnemyAIController::IsCombatTargetInMeleeRange() const
{
	if (!HasValidCombatTarget() || !HasValidAttackSet())
	{
		return false;
	}

	float Distance2D = 0.0f;
	return TryGetCurrentTargetDistance2D(Distance2D) && Distance2D <= MeleeRange;
}

void AEnemyAIController::BeginAlert()
{
	if (IsControlledEnemyDead())
	{
		return;
	}

	StopMovement();

	if (HasValidCombatTarget())
	{
		SetFocus(GetCurrentTarget());
	}
}

bool AEnemyAIController::TryRequestMeleeAttack()
{
	AEnemyCharacter* EnemyCharacter = Cast<AEnemyCharacter>(GetPawn());
	UAbilitySystemComponent* CharacterASC = EnemyCharacter ? EnemyCharacter->GetAbilitySystemComponent() : nullptr;
	if (IsControlledEnemyDead() || IsEnemyStunned() || IsEnemyHitReactionActive() || !CharacterASC || !EnemyMeleeAbilityTag.IsValid() || !HasValidAttackSet() || IsMeleeAttackOnCooldown()
		|| !HasValidCombatTarget() || !IsCombatTargetInMeleeRange())
	{
		return false;
	}

	FGameplayTagContainer AbilityTags;
	AbilityTags.AddTag(EnemyMeleeAbilityTag);
	return CharacterASC->TryActivateAbilitiesByTag(AbilityTags, false);
}

bool AEnemyAIController::IsEnemyMeleeAttackActive() const
{
	const AEnemyCharacter* EnemyCharacter = Cast<AEnemyCharacter>(GetPawn());
	const UAbilitySystemComponent* CharacterASC = EnemyCharacter ? EnemyCharacter->GetAbilitySystemComponent() : nullptr;
	return !IsControlledEnemyDead() && CharacterASC && AttackingStateTag.IsValid()
		&& CharacterASC->HasMatchingGameplayTag(AttackingStateTag);
}

bool AEnemyAIController::IsEnemyHitReactionActive() const
{
	const AEnemyCharacter* EnemyCharacter = Cast<AEnemyCharacter>(GetPawn());
	const UAbilitySystemComponent* CharacterASC = EnemyCharacter ? EnemyCharacter->GetAbilitySystemComponent() : nullptr;
	return !IsControlledEnemyDead() && CharacterASC && HitReactingStateTag.IsValid()
		&& CharacterASC->HasMatchingGameplayTag(HitReactingStateTag);
}

bool AEnemyAIController::IsEnemyStunned() const
{
	const AEnemyCharacter* EnemyCharacter = Cast<AEnemyCharacter>(GetPawn());
	const UAbilitySystemComponent* CharacterASC = EnemyCharacter ? EnemyCharacter->GetAbilitySystemComponent() : nullptr;
	return !IsControlledEnemyDead() && CharacterASC && StunnedStateTag.IsValid()
		&& CharacterASC->HasMatchingGameplayTag(StunnedStateTag);
}

void AEnemyAIController::HandleControlledEnemyDeath()
{
	if (!IsControlledEnemyDead())
	{
		return;
	}

	if (StateTreeComponent)
	{
		StateTreeComponent->StopLogic(TEXT("Controlled enemy died."));
	}

	StopMovement();
	ClearCurrentTarget(false);
	MeleeRange = 0.0f;
	MeleeAttackCooldownEndTime = 0.0f;
	bHasValidAttackSet = false;
}

void AEnemyAIController::HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (IsControlledEnemyDead())
	{
		return;
	}

	APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(Actor);
	if (!PlayerCharacter)
	{
		return;
	}

	if (Stimulus.WasSuccessfullySensed())
	{
		SetCurrentTarget(PlayerCharacter);
		return;
	}

	if (CurrentTarget.Get() == PlayerCharacter)
	{
		ClearCurrentTarget(true);
	}
}

void AEnemyAIController::ConfigureSight()
{
	if (!EnemyPerceptionComponent || !SightConfig)
	{
		return;
	}

	SightConfig->SightRadius = SightRadius;
	SightConfig->LoseSightRadius = LoseSightRadius;
	SightConfig->PeripheralVisionAngleDegrees = PeripheralVisionHalfAngleDegrees;
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	EnemyPerceptionComponent->ConfigureSense(*SightConfig);
	EnemyPerceptionComponent->SetDominantSense(SightConfig->GetSenseImplementation());
	EnemyPerceptionComponent->RequestStimuliListenerUpdate();
}

bool AEnemyAIController::IsControlledEnemyDead() const
{
	const AEnemyCharacter* EnemyCharacter = Cast<AEnemyCharacter>(GetPawn());
	return EnemyCharacter && EnemyCharacter->IsDead();
}

void AEnemyAIController::SetCurrentTarget(APlayerCharacter* NewTarget)
{
	if (IsControlledEnemyDead() || !IsValid(NewTarget))
	{
		return;
	}

	if (CurrentTarget.Get() == NewTarget)
	{
		SetFocus(NewTarget);
		return;
	}

	CurrentTarget = NewTarget;
	SetFocus(NewTarget);
	SendStateTreeEvent(TargetAcquiredEventTag);
}

void AEnemyAIController::ClearCurrentTarget(bool bSendTargetLostEvent)
{
	const bool bHadTarget = CurrentTarget.Get() != nullptr;
	CurrentTarget = nullptr;
	ClearFocus(EAIFocusPriority::Gameplay);

	if (bSendTargetLostEvent && bHadTarget)
	{
		SendStateTreeEvent(TargetLostEventTag);
	}
}

void AEnemyAIController::SendStateTreeEvent(const FGameplayTag& EventTag) const
{
	if (StateTreeComponent && EventTag.IsValid())
	{
		StateTreeComponent->SendStateTreeEvent(EventTag);
	}
}
