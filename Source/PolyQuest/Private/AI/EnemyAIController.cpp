#include "AI/EnemyAIController.h"

#include "AbilitySystemComponent.h"
#include "AI/EnemyAIProfile.h"
#include "Character/Enemy/EnemyCharacter.h"
#include "Character/Player/PlayerCharacter.h"
#include "Components/StateTreeAIComponent.h"
#include "Combat/Enemy/EnemyAttackSet.h"
#include "Engine/World.h"
#include "Navigation/PathFollowingComponent.h"
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

bool AEnemyAIController::CalculateTargetRelativeRepositionPoint(
	const FVector& TargetLocation,
	const FVector& EnemyLocation,
	float PreferredCombatDistance,
	float LateralRepositionDistance,
	float EngagementRange,
	bool bUseRightSide,
	FVector& OutRepositionPoint)
{
	OutRepositionPoint = FVector::ZeroVector;

	if (!FMath::IsFinite(PreferredCombatDistance) || PreferredCombatDistance <= 0.0f
		|| !FMath::IsFinite(LateralRepositionDistance) || LateralRepositionDistance < 0.0f
		|| !FMath::IsFinite(EngagementRange) || EngagementRange <= 0.0f
		|| TargetLocation.ContainsNaN() || EnemyLocation.ContainsNaN())
	{
		return false;
	}

	if (PreferredCombatDistance > EngagementRange)
	{
		return false;
	}

	const FVector2D Target2D(TargetLocation.X, TargetLocation.Y);
	const FVector2D Enemy2D(EnemyLocation.X, EnemyLocation.Y);
	const FVector2D TargetToEnemy2D = Enemy2D - Target2D;

	FVector2D RadialDir = FVector2D::ZeroVector;
	if (TargetToEnemy2D.IsNearlyZero())
	{
		RadialDir = FVector2D(1.0f, 0.0f);
	}
	else
	{
		RadialDir = TargetToEnemy2D.GetSafeNormal();
	}

	if (RadialDir.IsNearlyZero())
	{
		return false;
	}

	// Lateral perpendicular direction (relative to target-to-enemy radial vector)
	// Right = (Y, -X), Left = (-Y, X)
	const FVector2D SideDir = bUseRightSide
		? FVector2D(RadialDir.Y, -RadialDir.X)
		: FVector2D(-RadialDir.Y, RadialDir.X);

	// Clamp lateral distance so that the total distance from target doesn't exceed EngagementRange
	const float MaxLateralDistanceSq = FMath::Max(0.0f, (EngagementRange * EngagementRange) - (PreferredCombatDistance * PreferredCombatDistance));
	const float MaxLateralDistance = FMath::Sqrt(MaxLateralDistanceSq);
	const float EffectiveLateralDistance = FMath::Min(LateralRepositionDistance, MaxLateralDistance);

	const FVector2D TargetRelativeOffset2D = (RadialDir * PreferredCombatDistance) + (SideDir * EffectiveLateralDistance);
	const FVector2D RepositionPoint2D = Target2D + TargetRelativeOffset2D;

	// Authoritative verification: RepositionPoint must be within EngagementRange from TargetLocation
	if (FVector2D::Distance(Target2D, RepositionPoint2D) > (EngagementRange + KINDA_SMALL_NUMBER))
	{
		return false;
	}

	OutRepositionPoint = FVector(RepositionPoint2D.X, RepositionPoint2D.Y, EnemyLocation.Z);
	return true;
}

void AEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	HomeLocation = InPawn ? InPawn->GetActorLocation() : FVector::ZeroVector;
	ConfigureSight();
	MeleeRange = 0.0f;
	MeleeAttackCooldownEndTime = 0.0f;
	CachedLeashRadius = 0.0f;
	bHasValidAttackSet = false;
	bHasValidAIProfile = false;
	StopCooldownReposition(true);

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

	const UEnemyAIProfile* AIProfile = EnemyCharacter ? EnemyCharacter->GetAIProfile() : nullptr;
	FString ProfileValidationReason;
	if (!AIProfile || !AIProfile->IsValidAIProfile(ProfileValidationReason))
	{
		if (!bHasLoggedInvalidAIProfile)
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("Enemy AI for '%s' did not start: AIProfile is invalid (%s)."), *GetNameSafe(InPawn), *ProfileValidationReason);
			bHasLoggedInvalidAIProfile = true;
		}
		return;
	}

	if (AIProfile->GetPreferredCombatDistance() > AttackSet->GetEngagementRange())
	{
		if (!bHasLoggedInvalidAIProfile)
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("Enemy AI for '%s' did not start: AIProfile PreferredCombatDistance (%f) exceeds AttackSet EngagementRange (%f)."),
				*GetNameSafe(InPawn), AIProfile->GetPreferredCombatDistance(), AttackSet->GetEngagementRange());
			bHasLoggedInvalidAIProfile = true;
		}
		return;
	}

	if (AIProfile->GetLeashRadius() <= HomeAcceptanceRadius)
	{
		if (!bHasLoggedInvalidAIProfile)
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("Enemy AI for '%s' did not start: AIProfile LeashRadius (%f) must be greater than HomeAcceptanceRadius (%f)."),
				*GetNameSafe(InPawn), AIProfile->GetLeashRadius(), HomeAcceptanceRadius);
			bHasLoggedInvalidAIProfile = true;
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
	CachedLeashRadius = AIProfile->GetLeashRadius();
	bHasValidAttackSet = true;
	bHasValidAIProfile = true;

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

	StopCooldownReposition(true);
	StopMovement();
	ClearCurrentTarget(false);
	MeleeRange = 0.0f;
	MeleeAttackCooldownEndTime = 0.0f;
	CachedLeashRadius = 0.0f;
	bHasValidAttackSet = false;
	bHasValidAIProfile = false;

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

bool AEnemyAIController::HasValidAIProfile() const
{
	return !IsControlledEnemyDead() && bHasValidAIProfile && CachedLeashRadius > 0.0f;
}

const UEnemyAIProfile* AEnemyAIController::GetAIProfile() const
{
	const AEnemyCharacter* EnemyCharacter = Cast<AEnemyCharacter>(GetPawn());
	return EnemyCharacter ? EnemyCharacter->GetAIProfile() : nullptr;
}

bool AEnemyAIController::IsExceedingLeash() const
{
	if (IsControlledEnemyDead() || !bHasValidAIProfile || CachedLeashRadius <= 0.0f)
	{
		return false;
	}

	const APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return false;
	}

	return FVector::Dist2D(ControlledPawn->GetActorLocation(), HomeLocation) > CachedLeashRadius;
}

bool AEnemyAIController::CanRequestCooldownReposition() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	return !IsControlledEnemyDead()
		&& !IsEnemyStunned()
		&& !IsEnemyHitReactionActive()
		&& !IsEnemyMeleeAttackActive()
		&& !bIsRepositioning
		&& HasValidCombatTarget()
		&& HasValidAttackSet()
		&& HasValidAIProfile()
		&& IsMeleeAttackOnCooldown()
		&& World->GetTimeSeconds() >= NextAllowedRepositionTime
		&& !IsExceedingLeash();
}

bool AEnemyAIController::IsRepositionTemporarilyIntervalGated() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const bool bValidBaseState = !IsControlledEnemyDead()
		&& !IsEnemyStunned()
		&& !IsEnemyHitReactionActive()
		&& !IsEnemyMeleeAttackActive()
		&& !bIsRepositioning
		&& HasValidCombatTarget()
		&& HasValidAttackSet()
		&& HasValidAIProfile()
		&& IsMeleeAttackOnCooldown()
		&& !IsExceedingLeash();

	return bValidBaseState && (World->GetTimeSeconds() < NextAllowedRepositionTime);
}

bool AEnemyAIController::TryRequestCooldownReposition()
{
	if (!CanRequestCooldownReposition())
	{
		return false;
	}

	const UWorld* World = GetWorld();
	const AEnemyCharacter* EnemyCharacter = Cast<AEnemyCharacter>(GetPawn());
	const UEnemyAIProfile* AIProfile = EnemyCharacter ? EnemyCharacter->GetAIProfile() : nullptr;
	const APlayerCharacter* Target = GetCurrentTarget();
	const APawn* ControlledPawn = GetPawn();
	if (!World || !AIProfile || !Target || !ControlledPawn)
	{
		return false;
	}

	// Direction arbitration:
	// 1. Initial attempt or previous attempt succeeded: switch side.
	// 2. Previous attempt failed:
	//    - If failed once on current side (FailedAttemptsOnCurrentSide == 1): retry same side once.
	//    - If retry failed again (FailedAttemptsOnCurrentSide >= 2): force switch side and reset failure count.
	bool bUseRightSide = false;
	if (RepositionAttemptsInCurrentCooldown == 0)
	{
		bUseRightSide = !bLastRepositionUsedRightSide;
		FailedAttemptsOnCurrentSide = 0;
	}
	else if (bLastRepositionSucceeded)
	{
		bUseRightSide = !bLastRepositionUsedRightSide;
		FailedAttemptsOnCurrentSide = 0;
	}
	else if (FailedAttemptsOnCurrentSide == 1)
	{
		// Retry once on same side
		bUseRightSide = bLastRepositionUsedRightSide;
	}
	else
	{
		// Consecutive retry failure: force alternate side
		bUseRightSide = !bLastRepositionUsedRightSide;
		FailedAttemptsOnCurrentSide = 0;
	}

	RepositionAttemptsInCurrentCooldown++;
	NextAllowedRepositionTime = World->GetTimeSeconds() + AIProfile->GetRepositionRetryDelay();

	FVector RepositionPoint = FVector::ZeroVector;
	const bool bPointValid = CalculateTargetRelativeRepositionPoint(
		Target->GetActorLocation(),
		ControlledPawn->GetActorLocation(),
		AIProfile->GetPreferredCombatDistance(),
		AIProfile->GetLateralRepositionDistance(),
		MeleeRange,
		bUseRightSide,
		RepositionPoint);

	if (!bPointValid)
	{
		bLastRepositionSucceeded = false;
		bLastRepositionUsedRightSide = bUseRightSide;
		FailedAttemptsOnCurrentSide++;
		return false;
	}

	FAIMoveRequest MoveRequest(RepositionPoint);
	MoveRequest.SetAcceptanceRadius(AIProfile->GetRepositionAcceptanceRadius());
	MoveRequest.SetUsePathfinding(true);
	MoveRequest.SetAllowPartialPath(true);
	MoveRequest.SetProjectGoalLocation(true);
	MoveRequest.SetReachTestIncludesAgentRadius(false);
	MoveRequest.SetReachTestIncludesGoalRadius(false);

	const FPathFollowingRequestResult MoveResult = MoveTo(MoveRequest);
	if (MoveResult.Code == EPathFollowingRequestResult::RequestSuccessful)
	{
		bIsRepositioning = true;
		CurrentRepositionRequestID = MoveResult.MoveId;
		bLastRepositionUsedRightSide = bUseRightSide;
		return true;
	}
	else if (MoveResult.Code == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		bIsRepositioning = false;
		CurrentRepositionRequestID = FAIRequestID::InvalidRequest;
		bLastRepositionUsedRightSide = bUseRightSide;
		bLastRepositionSucceeded = true;
		FailedAttemptsOnCurrentSide = 0;
		return true;
	}

	// Immediate move failure
	bIsRepositioning = false;
	CurrentRepositionRequestID = FAIRequestID::InvalidRequest;
	bLastRepositionUsedRightSide = bUseRightSide;
	bLastRepositionSucceeded = false;
	FailedAttemptsOnCurrentSide++;
	return false;
}

void AEnemyAIController::StopCooldownReposition(bool bResetAttempts)
{
	if (bIsRepositioning)
	{
		StopMovement();
		bIsRepositioning = false;
		CurrentRepositionRequestID = FAIRequestID::InvalidRequest;
	}

	if (bResetAttempts)
	{
		RepositionAttemptsInCurrentCooldown = 0;
		FailedAttemptsOnCurrentSide = 0;
		bLastRepositionSucceeded = false;
		NextAllowedRepositionTime = 0.0f;
	}
}

void AEnemyAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);

	if (CurrentRepositionRequestID.IsValid() && RequestID == CurrentRepositionRequestID)
	{
		bIsRepositioning = false;
		CurrentRepositionRequestID = FAIRequestID::InvalidRequest;
		bLastRepositionSucceeded = Result.IsSuccess();
		if (bLastRepositionSucceeded)
		{
			FailedAttemptsOnCurrentSide = 0;
		}
		else
		{
			FailedAttemptsOnCurrentSide++;
		}
	}
}

void AEnemyAIController::StartMeleeAttackCooldown(float CooldownAfterAttack)
{
	const UWorld* World = GetWorld();
	if (IsControlledEnemyDead() || !World || !FMath::IsFinite(CooldownAfterAttack) || CooldownAfterAttack < 0.0f)
	{
		return;
	}

	MeleeAttackCooldownEndTime = World->GetTimeSeconds() + CooldownAfterAttack;
	StopCooldownReposition(true);
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

	StopCooldownReposition(true);
	StopMovement();
	ClearCurrentTarget(false);
	MeleeRange = 0.0f;
	MeleeAttackCooldownEndTime = 0.0f;
	CachedLeashRadius = 0.0f;
	bHasValidAttackSet = false;
	bHasValidAIProfile = false;
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
	StopCooldownReposition(true);
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
