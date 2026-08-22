#include "Character/Enemy/EnemyCharacter.h"

#include "AI/EnemyAIController.h"
#include "AI/EnemyAIProfile.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystem/CharacterAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Combat/Reaction/HitReactionClassifier.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "PolyQuest.h"
#include "TimerManager.h"

AEnemyCharacter::AEnemyCharacter()
{
	CombatTeamTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Team.Enemy")), false);
	DeadStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
	HitReactionEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Enemy.Big")), false);
	SmallHitReactionEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Enemy.Small")), false);
	LaunchReactionEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Enemy.Launch")), false);
	StanceBreakEventTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Event.Reaction.Enemy.StanceBreak")), false);
	PoiseRecoveryDataTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Data.Poise.Recovery")), false);
	StunnedStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Stunned")), false);
	AIControllerClass = AEnemyAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	bUseControllerRotationYaw = true;

	GetCharacterMovement()->bOrientRotationToMovement = false;
}

void AEnemyCharacter::BeginPlay()
{
	if (!DeadStateTag.IsValid())
	{
		DeadStateTag = FGameplayTag::RequestGameplayTag(FName(TEXT("State.Status.Dead")), false);
	}

	Super::BeginPlay();
	BindDeathEvents();

	if (!HasValidPoiseRecoveryConfiguration() && !bHasLoggedInvalidPoiseRecoveryConfiguration)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Enemy '%s' has invalid Poise recovery configuration: a recovery GameplayEffect, valid Data.Poise.Recovery tag, positive MaxPoise/rate, and a positive tick interval are required."), *GetNameSafe(this));
		bHasLoggedInvalidPoiseRecoveryConfiguration = true;
	}
}

void AEnemyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bDeathTeardownStarted = true;
	ClearPoiseRecovery();
	if (UWorld* World = GetWorld())
	{
		if (PendingStanceBreakTimerHandle.IsValid())
		{
			World->GetTimerManager().ClearTimer(PendingStanceBreakTimerHandle);
		}
	}
	PendingStanceBreakTimerHandle.Invalidate();
	bStanceBreakDispatchPending = false;
	bLaunchStanceBreakDeferralActive = false;
	bPendingDeferredStanceBreak = false;
	ActivePoiseBreakingEffectSpec = nullptr;
	UnbindDeathEvents();
	Super::EndPlay(EndPlayReason);
}

bool AEnemyCharacter::IsDead() const
{
	const UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent();
	return CharacterASC && DeadStateTag.IsValid()
		&& CharacterASC->HasMatchingGameplayTag(DeadStateTag);
}

bool AEnemyCharacter::IsPoiseBroken() const
{
	const UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent();
	return CharacterASC && CharacterASC->GetNumericAttribute(UCharacterAttributeSet::GetPoiseAttribute()) <= 0.0f;
}

bool AEnemyCharacter::HasValidPoiseRecoveryConfiguration() const
{
	const UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent();
	const float CurrentMaxPoise = CharacterASC
		? CharacterASC->GetNumericAttribute(UCharacterAttributeSet::GetMaxPoiseAttribute())
		: 0.0f;

	return PoiseRecoveryGameplayEffectClass != nullptr
		&& PoiseRecoveryDataTag.IsValid()
		&& CurrentMaxPoise > 0.0f
		&& PoiseRecoveryRate > 0.0f
		&& PoiseRecoveryTickIntervalSeconds > 0.0f
		&& PoiseRecoveryDelaySeconds >= 0.0f;
}

void AEnemyCharacter::BindDeathEvents()
{
	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent();
	if (!CharacterASC || DeathBoundAbilitySystemComponent.Get() == CharacterASC)
	{
		return;
	}

	UnbindDeathEvents();
	DeathBoundAbilitySystemComponent = CharacterASC;
	HealthAttributeChangedHandle = CharacterASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetHealthAttribute())
		.AddUObject(this, &AEnemyCharacter::OnHealthAttributeChanged);
	PoiseAttributeChangedHandle = CharacterASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetPoiseAttribute())
		.AddUObject(this, &AEnemyCharacter::OnPoiseAttributeChanged);

	if (DeadStateTag.IsValid())
	{
		DeadStateTagChangedHandle = CharacterASC->RegisterGameplayTagEvent(DeadStateTag)
			.AddUObject(this, &AEnemyCharacter::OnDeadStateTagChanged);
	}

	if (IsDead())
	{
		HandleDeath();
	}
	else if (CharacterASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute()) <= 0.0f)
	{
		SetDeadState();
	}
}

void AEnemyCharacter::UnbindDeathEvents()
{
	UAbilitySystemComponent* BoundASC = DeathBoundAbilitySystemComponent.Get();
	if (BoundASC)
	{
		if (HealthAttributeChangedHandle.IsValid())
		{
			BoundASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetHealthAttribute())
				.Remove(HealthAttributeChangedHandle);
		}
		if (PoiseAttributeChangedHandle.IsValid())
		{
			BoundASC->GetGameplayAttributeValueChangeDelegate(UCharacterAttributeSet::GetPoiseAttribute())
				.Remove(PoiseAttributeChangedHandle);
		}
		if (DeadStateTagChangedHandle.IsValid() && DeadStateTag.IsValid())
		{
			BoundASC->UnregisterGameplayTagEvent(DeadStateTagChangedHandle, DeadStateTag);
		}
	}

	HealthAttributeChangedHandle.Reset();
	PoiseAttributeChangedHandle.Reset();
	DeadStateTagChangedHandle.Reset();
	DeathBoundAbilitySystemComponent.Reset();
}

void AEnemyCharacter::OnHealthAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	if (!HasAuthority() || bDeathTeardownStarted || IsActorBeingDestroyed())
	{
		return;
	}

	if (ChangeData.NewValue <= 0.0f)
	{
		if (!IsDead())
		{
			SetDeadState();
		}
		return;
	}

	if (ChangeData.NewValue >= ChangeData.OldValue || !ChangeData.GEModData)
	{
		return;
	}

	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent();
	if (!CharacterASC)
	{
		return;
	}

	const bool bIsStunned = StunnedStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(StunnedStateTag);
	if (IsDead() || bIsStunned)
	{
		ActivePoiseBreakingEffectSpec = nullptr;
		return;
	}

	const bool bIsSameSpecPoiseBreak = (ActivePoiseBreakingEffectSpec != nullptr && &ChangeData.GEModData->EffectSpec == ActivePoiseBreakingEffectSpec);
	ActivePoiseBreakingEffectSpec = nullptr;

	FGameplayTagContainer AssetTags;
	ChangeData.GEModData->EffectSpec.GetAllAssetTags(AssetTags);

	const EHitReactionTier ReactionTier = FHitReactionClassifier::ClassifyReactionTier(AssetTags);
	if (ReactionTier == EHitReactionTier::Invalid)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Enemy '%s' received invalid multi-tier hit reaction tags from effect '%s'; skipping reaction event."),
			*GetNameSafe(this), *GetNameSafe(ChangeData.GEModData->EffectSpec.Def));
		return;
	}

	if (IsPoiseBroken())
	{
		const bool bAllowSameSpecLaunch = (bIsSameSpecPoiseBreak && ReactionTier == EHitReactionTier::Launch);
		if (!bAllowSameSpecLaunch)
		{
			return;
		}
	}

	FGameplayTag TargetEventTag;
	if (ReactionTier == EHitReactionTier::Small)
	{
		TargetEventTag = SmallHitReactionEventTag;
	}
	else if (ReactionTier == EHitReactionTier::Big)
	{
		TargetEventTag = HitReactionEventTag;
	}
	else if (ReactionTier == EHitReactionTier::Launch)
	{
		TargetEventTag = LaunchReactionEventTag;
	}
	else
	{
		// None
		return;
	}

	if (!TargetEventTag.IsValid())
	{
		return;
	}

	FGameplayEventData ReactionEventData;
	ReactionEventData.EventTag = TargetEventTag;
	ReactionEventData.Instigator = ChangeData.GEModData->EffectSpec.GetContext().GetInstigator();
	ReactionEventData.Target = this;
	ReactionEventData.EventMagnitude = ChangeData.OldValue - ChangeData.NewValue;
	ReactionEventData.ContextHandle = ChangeData.GEModData->EffectSpec.GetContext();
	CharacterASC->HandleGameplayEvent(TargetEventTag, &ReactionEventData);
}

void AEnemyCharacter::BeginLaunchStanceBreakDeferral()
{
	bLaunchStanceBreakDeferralActive = true;

	if (UWorld* World = GetWorld())
	{
		if (PendingStanceBreakTimerHandle.IsValid())
		{
			World->GetTimerManager().ClearTimer(PendingStanceBreakTimerHandle);
		}
	}
	PendingStanceBreakTimerHandle.Invalidate();
	bStanceBreakDispatchPending = false;
	ActivePoiseBreakingEffectSpec = nullptr;

	if (IsPoiseBroken())
	{
		bPendingDeferredStanceBreak = true;
	}
}

void AEnemyCharacter::CompleteLaunchStanceBreakDeferral()
{
	const bool bHadPendingStanceBreak = bPendingDeferredStanceBreak;
	bPendingDeferredStanceBreak = false;
	bLaunchStanceBreakDeferralActive = false;
	ActivePoiseBreakingEffectSpec = nullptr;

	if (bHadPendingStanceBreak)
	{
		TryDispatchStanceBreak();
	}
}

void AEnemyCharacter::AbortLaunchStanceBreakDeferral()
{
	const bool bHadPendingStanceBreak = bPendingDeferredStanceBreak;
	bPendingDeferredStanceBreak = false;
	bLaunchStanceBreakDeferralActive = false;
	ActivePoiseBreakingEffectSpec = nullptr;

	if (bHadPendingStanceBreak && HasAuthority() && !bDeathTeardownStarted && !IsDead() && !IsActorBeingDestroyed() && IsPoiseBroken())
	{
		RestorePoiseToMax();
	}
}

void AEnemyCharacter::OnPoiseAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	if (!HasAuthority() || bDeathTeardownStarted || IsDead())
	{
		ActivePoiseBreakingEffectSpec = nullptr;
		return;
	}

	if (FMath::IsNearlyEqual(ChangeData.NewValue, ChangeData.OldValue))
	{
		return;
	}

	if (ChangeData.NewValue > 0.0f)
	{
		ActivePoiseBreakingEffectSpec = nullptr;
		if (bLaunchStanceBreakDeferralActive)
		{
			bPendingDeferredStanceBreak = false;
		}

		if (UWorld* World = GetWorld())
		{
			if (PendingStanceBreakTimerHandle.IsValid())
			{
				World->GetTimerManager().ClearTimer(PendingStanceBreakTimerHandle);
			}
		}
		PendingStanceBreakTimerHandle.Invalidate();
		bStanceBreakDispatchPending = false;

		if (ChangeData.NewValue < ChangeData.OldValue)
		{
			ClearPoiseRecovery();
			StartPoiseRecovery();
		}
		return;
	}

	if (ChangeData.NewValue >= ChangeData.OldValue)
	{
		ActivePoiseBreakingEffectSpec = nullptr;
		return;
	}

	ClearPoiseRecovery();

	if (bLaunchStanceBreakDeferralActive)
	{
		ActivePoiseBreakingEffectSpec = nullptr;
		bPendingDeferredStanceBreak = true;
		return;
	}

	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent();
	if (!CharacterASC || CharacterASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute()) <= 0.0f
		|| !StanceBreakEventTag.IsValid() || bStanceBreakDispatchPending)
	{
		ActivePoiseBreakingEffectSpec = nullptr;
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		ActivePoiseBreakingEffectSpec = nullptr;
		return;
	}

	ActivePoiseBreakingEffectSpec = ChangeData.GEModData ? &ChangeData.GEModData->EffectSpec : nullptr;

	bStanceBreakDispatchPending = true;
	PendingStanceBreakTimerHandle = World->GetTimerManager().SetTimerForNextTick(this, &AEnemyCharacter::DispatchPendingStanceBreak);
}

void AEnemyCharacter::DispatchPendingStanceBreak()
{
	ActivePoiseBreakingEffectSpec = nullptr;
	if (UWorld* World = GetWorld())
	{
		if (PendingStanceBreakTimerHandle.IsValid())
		{
			World->GetTimerManager().ClearTimer(PendingStanceBreakTimerHandle);
		}
	}
	PendingStanceBreakTimerHandle.Invalidate();
	bStanceBreakDispatchPending = false;
	TryDispatchStanceBreak();
}

bool AEnemyCharacter::TryDispatchStanceBreak()
{
	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent();
	if (!HasAuthority() || bDeathTeardownStarted || IsDead() || IsActorBeingDestroyed() || !CharacterASC
		|| CharacterASC->GetNumericAttribute(UCharacterAttributeSet::GetHealthAttribute()) <= 0.0f || !IsPoiseBroken())
	{
		return false;
	}

	if (!HasValidPoiseRecoveryConfiguration())
	{
		if (!bHasLoggedInvalidPoiseRecoveryConfiguration)
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("Enemy '%s' cannot dispatch Stance Break: Poise recovery configuration is invalid."), *GetNameSafe(this));
			bHasLoggedInvalidPoiseRecoveryConfiguration = true;
		}
		return false;
	}

	FGameplayEventData StanceBreakEventData;
	StanceBreakEventData.EventTag = StanceBreakEventTag;
	StanceBreakEventData.Target = this;
	StanceBreakEventData.EventMagnitude = 0.0f;

	const int32 TriggeredAbilityCount = CharacterASC->HandleGameplayEvent(StanceBreakEventTag, &StanceBreakEventData);
	if (TriggeredAbilityCount <= 0)
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Enemy '%s' reached zero Poise but no Stance Break Ability accepted the event; restoring Poise to avoid a permanent broken state."), *GetNameSafe(this));
		if (!RestorePoiseToMax())
		{
			UE_LOG(LogPolyQuest, Warning, TEXT("Enemy '%s' could not restore Poise after a rejected Stance Break event; verify the recovery GameplayEffect modifies Poise with Data.Poise.Recovery."), *GetNameSafe(this));
		}
		return false;
	}

	return true;
}

void AEnemyCharacter::StartPoiseRecovery()
{
	if (!HasAuthority() || bDeathTeardownStarted || IsDead() || IsPoiseBroken() || !HasValidPoiseRecoveryConfiguration())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		PoiseRecoveryTimerHandle,
		this,
		&AEnemyCharacter::OnPoiseRecoveryTick,
		PoiseRecoveryTickIntervalSeconds,
		true,
		PoiseRecoveryDelaySeconds);
}

void AEnemyCharacter::ClearPoiseRecovery()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PoiseRecoveryTimerHandle);
	}
	PoiseRecoveryTimerHandle.Invalidate();
}

void AEnemyCharacter::OnPoiseRecoveryTick()
{
	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent();
	if (!HasAuthority() || bDeathTeardownStarted || IsDead() || IsActorBeingDestroyed() || !CharacterASC
		|| !HasValidPoiseRecoveryConfiguration() || (StunnedStateTag.IsValid() && CharacterASC->HasMatchingGameplayTag(StunnedStateTag)))
	{
		ClearPoiseRecovery();
		return;
	}

	const float CurrentPoise = CharacterASC->GetNumericAttribute(UCharacterAttributeSet::GetPoiseAttribute());
	const float MaxPoise = CharacterASC->GetNumericAttribute(UCharacterAttributeSet::GetMaxPoiseAttribute());
	if (CurrentPoise <= 0.0f || CurrentPoise >= MaxPoise)
	{
		ClearPoiseRecovery();
		return;
	}

	if (!ApplyPoiseRecoveryMagnitude(PoiseRecoveryRate * PoiseRecoveryTickIntervalSeconds))
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Enemy '%s' Poise recovery stopped because the configured GameplayEffect did not advance Poise."), *GetNameSafe(this));
		ClearPoiseRecovery();
		return;
	}

	if (CharacterASC->GetNumericAttribute(UCharacterAttributeSet::GetPoiseAttribute()) >= MaxPoise)
	{
		ClearPoiseRecovery();
	}
}

bool AEnemyCharacter::ApplyPoiseRecoveryMagnitude(float Magnitude)
{
	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent();
	if (!HasAuthority() || !CharacterASC || Magnitude <= 0.0f || !PoiseRecoveryGameplayEffectClass || !PoiseRecoveryDataTag.IsValid())
	{
		return false;
	}

	const FGameplayEffectSpecHandle RecoverySpecHandle = CharacterASC->MakeOutgoingSpec(
		PoiseRecoveryGameplayEffectClass,
		1.0f,
		CharacterASC->MakeEffectContext());
	if (!RecoverySpecHandle.IsValid() || !RecoverySpecHandle.Data.IsValid())
	{
		return false;
	}

	RecoverySpecHandle.Data->SetSetByCallerMagnitude(PoiseRecoveryDataTag, Magnitude);
	const float PreviousPoise = CharacterASC->GetNumericAttribute(UCharacterAttributeSet::GetPoiseAttribute());
	CharacterASC->ApplyGameplayEffectSpecToSelf(*RecoverySpecHandle.Data.Get());
	const float CurrentPoise = CharacterASC->GetNumericAttribute(UCharacterAttributeSet::GetPoiseAttribute());
	const float MaxPoise = CharacterASC->GetNumericAttribute(UCharacterAttributeSet::GetMaxPoiseAttribute());
	return CurrentPoise > PreviousPoise + KINDA_SMALL_NUMBER || CurrentPoise >= MaxPoise;
}

bool AEnemyCharacter::RestorePoiseToMax()
{
	ClearPoiseRecovery();

	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent();
	if (!HasAuthority() || bDeathTeardownStarted || IsDead() || IsActorBeingDestroyed() || !CharacterASC)
	{
		return false;
	}

	const float CurrentPoise = CharacterASC->GetNumericAttribute(UCharacterAttributeSet::GetPoiseAttribute());
	const float MaxPoise = CharacterASC->GetNumericAttribute(UCharacterAttributeSet::GetMaxPoiseAttribute());
	if (MaxPoise <= 0.0f)
	{
		return false;
	}

	return CurrentPoise >= MaxPoise - KINDA_SMALL_NUMBER
		|| ApplyPoiseRecoveryMagnitude(MaxPoise - FMath::Max(CurrentPoise, 0.0f));
}

void AEnemyCharacter::OnDeadStateTagChanged(const FGameplayTag, int32 NewCount)
{
	if (NewCount > 0)
	{
		HandleDeath();
	}
}

void AEnemyCharacter::SetDeadState()
{
	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent();
	if (!HasAuthority() || !CharacterASC || !DeadStateTag.IsValid())
	{
		return;
	}

	CharacterASC->SetLooseGameplayTagCount(DeadStateTag, 1);
}

void AEnemyCharacter::HandleDeath()
{
	UAbilitySystemComponent* CharacterASC = GetAbilitySystemComponent();
	if (!HasAuthority() || bDeathTeardownStarted || !CharacterASC || !IsDead())
	{
		return;
	}

	bDeathTeardownStarted = true;
	ClearPoiseRecovery();
	if (UWorld* World = GetWorld())
	{
		if (PendingStanceBreakTimerHandle.IsValid())
		{
			World->GetTimerManager().ClearTimer(PendingStanceBreakTimerHandle);
		}
	}
	PendingStanceBreakTimerHandle.Invalidate();
	bStanceBreakDispatchPending = false;
	bLaunchStanceBreakDeferralActive = false;
	bPendingDeferredStanceBreak = false;
	ActivePoiseBreakingEffectSpec = nullptr;
	// A Dead Tag granted by any legal source becomes terminal in C2; revival is out of scope.
	CharacterASC->SetLooseGameplayTagCount(DeadStateTag, 1);
	CharacterASC->SetNumericAttributeBase(UCharacterAttributeSet::GetHealthAttribute(), 0.0f);

	if (AEnemyAIController* EnemyAIController = Cast<AEnemyAIController>(GetController()))
	{
		EnemyAIController->HandleControlledEnemyDeath();
	}

	CharacterASC->CancelAllAbilities();

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
		MovementComponent->DisableMovement();
	}

	StartDeathRagdoll();
}

void AEnemyCharacter::StartDeathRagdoll()
{
	if (!bUseRagdollOnDeath || bDeathRagdollStarted)
	{
		return;
	}

	USkeletalMeshComponent* SkeletalMesh = GetMesh();
	if (!SkeletalMesh || !SkeletalMesh->GetPhysicsAsset())
	{
		UE_LOG(LogPolyQuest, Warning, TEXT("Enemy '%s' cannot enter death ragdoll: SkeletalMesh and Physics Asset are required."), *GetNameSafe(this));
		return;
	}

	DisableFixedWeaponDisplayCollision();

	UCapsuleComponent* CharacterCapsule = GetCapsuleComponent();
	if (CharacterCapsule)
	{
		CharacterCapsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		CharacterCapsule->SetGenerateOverlapEvents(false);
	}

	SkeletalMesh->SetCollisionProfileName(FName(TEXT("Ragdoll")));
	SkeletalMesh->SetSimulatePhysics(true);
	SkeletalMesh->WakeAllRigidBodies();
	bDeathRagdollStarted = true;
}
