// Fill out your copyright notice in the Description page of Project Settings.


#include "BTTask_ScanArea.h"
#include "AIController.h"
#include "Math/UnrealMathUtility.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UBTTask_ScanArea::UBTTask_ScanArea()
{
    NodeName = "Scan Area Actively";
    bNotifyTick = true;

    // This allows us to safely use member variables without writing complex NodeMemory structs!
    bCreateNodeInstance = true;
}

EBTNodeResult::Type UBTTask_ScanArea::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    TimeRemaining = ScanDuration;
    TimeUntilNextShift = 0.0f; // Force an immediate look on the very first frame

    // Allows the guard to rotate
    if (AAIController* AICon = OwnerComp.GetAIOwner())
    {
        if (ACharacter* AIChar = Cast<ACharacter>(AICon->GetPawn()))
        {
            AIChar->GetCharacterMovement()->bUseControllerDesiredRotation = true;
            AIChar->GetCharacterMovement()->bOrientRotationToMovement = false;
        }
    }

    return EBTNodeResult::InProgress;
}

void UBTTask_ScanArea::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    TimeRemaining -= DeltaSeconds;
    TimeUntilNextShift -= DeltaSeconds;

    AAIController* AICon = OwnerComp.GetAIOwner();
    if (!AICon || !AICon->GetPawn()) return;

    // Shift their gaze to a new random position
    if (TimeUntilNextShift <= 0.0f)
    {
        FVector PawnLoc = AICon->GetPawn()->GetActorLocation();

        // Pick a random point 500 units away from where they are standing
        FVector RandomPoint = PawnLoc + FMath::VRand() * 500.0f;
        RandomPoint.Z = PawnLoc.Z; // Keep their head level

        AICon->SetFocalPoint(RandomPoint);
        TimeUntilNextShift = 4.0f;
    }

    // When the total scan duration is up, clear the focus and finish
    if (TimeRemaining <= 0.0f)
    {
        AICon->ClearFocus(EAIFocusPriority::Gameplay);

        // Lock spine physics
        if (ACharacter* AIChar = Cast<ACharacter>(AICon->GetPawn()))
        {
            AIChar->GetCharacterMovement()->bUseControllerDesiredRotation = false;
            AIChar->GetCharacterMovement()->bOrientRotationToMovement = true;
        }

        FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
    }
}

EBTNodeResult::Type UBTTask_ScanArea::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    // If the player shoots them while they are searching, clean up the rotation!
    if (AAIController* AICon = OwnerComp.GetAIOwner())
    {
        AICon->ClearFocus(EAIFocusPriority::Gameplay);

		// Lock spine physics
        if (ACharacter* AIChar = Cast<ACharacter>(AICon->GetPawn()))
        {
            AIChar->GetCharacterMovement()->bUseControllerDesiredRotation = false;
            AIChar->GetCharacterMovement()->bOrientRotationToMovement = true;
        }
    }
    return EBTNodeResult::Aborted;
}

