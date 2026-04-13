// Fill out your copyright notice in the Description page of Project Settings.


#include "BTTask_TurnToNoise.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/Engine.h"

UBTTask_TurnToNoise::UBTTask_TurnToNoise()
{
    NodeName = TEXT("Turn To Noise & Observe");

    bNotifyTick = true; // Tick
    bCreateNodeInstance = true; // Forces the engine to give every guard their own clean memory instance of this timer
}

EBTNodeResult::Type UBTTask_TurnToNoise::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    AAIController* AICon = OwnerComp.GetAIOwner();
    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();

    if (!AICon || !BB) return EBTNodeResult::Failed;

	// Grab the vector dynamically from whatever key is selected in the editor for this task
    FVector TargetLocation = BB->GetValueAsVector(BlackboardKey.SelectedKeyName);

    // Reach into the guard's body and temporarily force them to rotate in place
    if (APawn* AIPawn = AICon->GetPawn())
    {
        if (ACharacter* AIChar = Cast<ACharacter>(AIPawn))
        {
            AIChar->GetCharacterMovement()->bUseControllerDesiredRotation = true;
            AIChar->GetCharacterMovement()->bOrientRotationToMovement = false;
        }
    }

    // Tell the AI Controller to lock its eyes on that location
    AICon->SetFocalPoint(TargetLocation);

    // Reset our timer
    TimeSpentWaiting = 0.0f;

    // Tell the BT to pause here and wait for the Tick function to finish
    return EBTNodeResult::InProgress;
}

void UBTTask_TurnToNoise::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    // Add to our timer
    TimeSpentWaiting += DeltaSeconds;

    // Has our 1-second "What was that?" pause finished?
    if (TimeSpentWaiting >= ObservationDuration)
    {
        // We are done staring. Clear the focus so the AI can walk normally again.
        if (AAIController* AICon = OwnerComp.GetAIOwner())
        {
            AICon->ClearFocus(EAIFocusPriority::Gameplay);

            // Restore normal walking orientation
            if (APawn* AIPawn = AICon->GetPawn())
            {
                if (ACharacter* AIChar = Cast<ACharacter>(AIPawn))
                {
                    AIChar->GetCharacterMovement()->bUseControllerDesiredRotation = false;
                    AIChar->GetCharacterMovement()->bOrientRotationToMovement = true;
                }
            }
        }

        // DEBUG: Will pop up green so you know it waited the full duration
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("DEBUG: Snap Task Finished!"));

        // Successfully finish the task to let the BT move to the next node
        FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
    }
}

EBTNodeResult::Type UBTTask_TurnToNoise::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    // If the task was aborted midway, reset the rotation settings to the default
    if (AAIController* AICon = OwnerComp.GetAIOwner())
    {
        AICon->ClearFocus(EAIFocusPriority::Gameplay);

        if (APawn* AIPawn = AICon->GetPawn())
        {
            if (ACharacter* AIChar = Cast<ACharacter>(AIPawn))
            {
                AIChar->GetCharacterMovement()->bUseControllerDesiredRotation = false;
                AIChar->GetCharacterMovement()->bOrientRotationToMovement = true;
            }
        }
    }

    // Acknowledge the abort
    return EBTNodeResult::Aborted;
}