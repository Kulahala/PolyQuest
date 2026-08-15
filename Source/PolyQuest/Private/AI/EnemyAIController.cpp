#include "AI/EnemyAIController.h"

#include "AbilitySystemComponent.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Character/Player/PlayerCharacter.h"
#include "Components/StateTreeAIComponent.h"
#include "Combat/Enemy/EnemyAttackProfile.h"
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
	bHasValidAttackProfile = false;

	const AEnemyCharacter* EnemyCharacter = Cast<AEnemyCharacter>(InPawn);
	const UEnemyAttackProfile* AttackProfile = EnemyCharacter ? EnemyCharacter->GetAttackProfile() : nullptr;
	if (!AttackProfile || !AttackProfile->IsValidAttackProfile())
	{
		if (!bHasLoggedInvalidAttackProfile)
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("Enemy AI for '%s' did not start: AttackProfile requires an AttackMontage, DamageGameplayEffectClass, positive AttackRange, and non-negative CooldownAfterAttack."), *GetNameSafe(InPawn));
			bHasLoggedInvalidAttackProfile = true;
		}
		return;
	}

	MeleeRange = AttackProfile->GetAttackRange();
	bHasValidAttackProfile = true;

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
	bHasValidAttackProfile = false;

	Super::OnUnPossess();
}

APlayerCharacter* AEnemyAIController::GetCurrentTarget() const
{
	return CurrentTarget.Get();
}

bool AEnemyAIController::HasValidCombatTarget() const
{
	return GetPawn() && IsValid(CurrentTarget.Get()) && CurrentTarget.Get() != GetPawn();
}

bool AEnemyAIController::HasValidAttackProfile() const
{
	return bHasValidAttackProfile && MeleeRange > 0.0f;
}

void AEnemyAIController::StartMeleeAttackCooldown(float CooldownAfterAttack)
{
	const UWorld* World = GetWorld();
	if (!World || CooldownAfterAttack < 0.0f)
	{
		return;
	}

	MeleeAttackCooldownEndTime = World->GetTimeSeconds() + CooldownAfterAttack;
}

bool AEnemyAIController::IsMeleeAttackOnCooldown() const
{
	const UWorld* World = GetWorld();
	return World && MeleeAttackCooldownEndTime > World->GetTimeSeconds();
}

bool AEnemyAIController::IsCombatTargetInMeleeRange() const
{
	if (!HasValidCombatTarget() || !HasValidAttackProfile())
	{
		return false;
	}

	const APawn* ControlledPawn = GetPawn();
	const APlayerCharacter* Target = GetCurrentTarget();
	return ControlledPawn && Target
		&& FVector::DistSquared2D(ControlledPawn->GetActorLocation(), Target->GetActorLocation()) <= FMath::Square(MeleeRange);
}

void AEnemyAIController::BeginAlert()
{
	StopMovement();

	if (HasValidCombatTarget())
	{
		SetFocus(GetCurrentTarget());
	}
}

bool AEnemyAIController::TryRequestMeleeAttack()
{
	AEnemyCharacter* EnemyCharacter = Cast<AEnemyCharacter>(GetPawn());
	UAbilitySystemComponent* AbilitySystemComponent = EnemyCharacter ? EnemyCharacter->GetAbilitySystemComponent() : nullptr;
	if (!AbilitySystemComponent || !EnemyMeleeAbilityTag.IsValid() || !HasValidAttackProfile() || IsMeleeAttackOnCooldown()
		|| !HasValidCombatTarget() || !IsCombatTargetInMeleeRange())
	{
		return false;
	}

	FGameplayTagContainer AbilityTags;
	AbilityTags.AddTag(EnemyMeleeAbilityTag);
	return AbilitySystemComponent->TryActivateAbilitiesByTag(AbilityTags, false);
}

bool AEnemyAIController::IsEnemyMeleeAttackActive() const
{
	const AEnemyCharacter* EnemyCharacter = Cast<AEnemyCharacter>(GetPawn());
	const UAbilitySystemComponent* AbilitySystemComponent = EnemyCharacter ? EnemyCharacter->GetAbilitySystemComponent() : nullptr;
	return AbilitySystemComponent && AttackingStateTag.IsValid() && AbilitySystemComponent->HasMatchingGameplayTag(AttackingStateTag);
}

void AEnemyAIController::HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
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

void AEnemyAIController::SetCurrentTarget(APlayerCharacter* NewTarget)
{
	if (!IsValid(NewTarget))
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
