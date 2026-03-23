// Fill out your copyright notice in the Description page of Project Settings.


#include "BTTask_RaiseAlarm.h"
#include "AIController.h"
#include "Perception/AISense_Hearing.h" // Access Unreal's sound broadcast system
#include "Engine/Engine.h"				//  Enable debug messages
#include "BehaviorTree/BlackboardComponent.h"


UBTTask_RaiseAlarm::UBTTask_RaiseAlarm()
{
	NodeName = TEXT("Raise Alarm");
}

EBTNodeResult::Type UBTTask_RaiseAlarm::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	// Get the AI controller that owns this behavior tree
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController) return EBTNodeResult::Failed;

	// Get the characater the sound will originate from
	APawn* AIPawn = AIController->GetPawn();
	if (!AIPawn) return EBTNodeResult::Failed;

	// Broadcast a noise event at the character's location
	// Parameters are the world context, location of the noise, volume (1.0f is max), the actor that made the noise, and the max range of the noise (0.0f means infinite),Tag name
	UAISense_Hearing::ReportNoiseEvent(GetWorld(), AIPawn->GetActorLocation(), 1.0f, AIPawn, 2000.0f, FName("Alarm"));

	// Debug message
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("Guard: We have a man down!"));
	}

	// Clear the "bSpottedBody" key so that the guard can run their investigate BT properly without the possibility of a double call
	OwnerComp.GetBlackboardComponent()->ClearValue(FName("bSpottedBody"));

	return EBTNodeResult::Succeeded;

}

