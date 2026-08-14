#include "Character/Enemy/EnemyCharacter.h"

#include "AI/EnemyAIController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayTagContainer.h"

AEnemyCharacter::AEnemyCharacter()
{
	CombatTeamTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Team.Enemy")), false);
	AIControllerClass = AEnemyAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	bUseControllerRotationYaw = true;

	GetCharacterMovement()->bOrientRotationToMovement = false;
}
