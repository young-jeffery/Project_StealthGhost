// Fill out your copyright notice in the Description page of Project Settings.

#include "BTTask_RaiseAlarm.h"
#include "AIController.h"
#include "Perception/AISense_Hearing.h"
#include "Engine/Engine.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Project_StealthGhostCharacter.h"

UBTTask_RaiseAlarm::UBTTask_RaiseAlarm()
{
    NodeName = TEXT("Raise Alarm (Delayed)");

    // Because we are using Timers and Variables inside the task, create a separate instance of this node for every guard
    // Otherwise, multiple guards raising an alarm at the same time will overwrite each other's memory
    bCreateNodeInstance = true;
}

EBTNodeResult::Type UBTTask_RaiseAlarm::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    AAIController* AIController = OwnerComp.GetAIOwner();
    if (!AIController || !AIController->GetPawn()) return EBTNodeResult::Failed;

    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    if (!BB) return EBTNodeResult::Failed;

    UObject* BodyObject = BB->GetValueAsObject(FName("SpottedBody"));
    TargetBody = Cast<AProject_StealthGhostCharacter>(BodyObject);

    // Are we just investigating a noise? Or did another guard get here first?
    if (!TargetBody || TargetBody->bHasBeenDiscovered)
    {
        BB->ClearValue(FName("SpottedBody"));
        return EBTNodeResult::Failed; // Let's the BT know this wasn't executed successfully
    }

    // Claim the body so no other guard tries to yell about it right now
    TargetBody->bHasBeenDiscovered = true;


    // Set our blackboard state
    BB->SetValueAsBool(FName("bIsRaisingAlarm"), true);

    // Cache the OwnerComp so the Timer can access it later
    CachedOwnerComp = &OwnerComp;

    // Start the delay timer!
    GetWorld()->GetTimerManager().SetTimer(AlarmTimerHandle, this, &UBTTask_RaiseAlarm::FinishAlarm, AlarmDelay, false);

    // Return InProgress so the Behavior Tree pauses on this node and waits!
    return EBTNodeResult::InProgress;
}

void UBTTask_RaiseAlarm::FinishAlarm()
{
    if (!CachedOwnerComp) return;

    AAIController* AIController = CachedOwnerComp->GetAIOwner();
    if (AIController && AIController->GetPawn())
    {
        // The delay is over, broadcast the Yell!
        UAISense_Hearing::ReportNoiseEvent(GetWorld(), AIController->GetPawn()->GetActorLocation(), 1.0f, AIController->GetPawn(), 5000.0f, FName("Alarm"));

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("Guard: WE HAVE A MAN DOWN!"));
        }
    }

    // Clean up Blackboard
    UBlackboardComponent* BB = CachedOwnerComp->GetBlackboardComponent();
    if (BB)
    {
        BB->ClearValue(FName("SpottedBody"));
        BB->ClearValue(FName("bIsRaisingAlarm"));
    }

    // Tell the Behavior Tree that we are finally done!
    FinishLatentTask(*CachedOwnerComp, EBTNodeResult::Succeeded);
}

EBTNodeResult::Type UBTTask_RaiseAlarm::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    // Cancel the timer so the alarm never goes off
    GetWorld()->GetTimerManager().ClearTimer(AlarmTimerHandle);

    // Unclaim the body. Since we failed to report it, another guard should be able to find it later.
    if (TargetBody)
    {
        TargetBody->bHasBeenDiscovered = false;
    }

    // Clean up the blackboard
    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    if (BB)
    {
        BB->ClearValue(FName("bIsRaisingAlarm"));
    }

    // Acknowledge the abort
    return EBTNodeResult::Aborted;
}