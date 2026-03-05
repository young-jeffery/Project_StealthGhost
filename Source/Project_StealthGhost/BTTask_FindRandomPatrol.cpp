// Fill out your copyright notice in the Description page of Project Settings.


#include "BehaviorTree/BlackboardComponent.h"
#include "NavigationSystem.h"
#include "AIController.h"
#include "BTTask_FindRandomPatrol.h"


UBTTask_FindRandomPatrol::UBTTask_FindRandomPatrol()
{
	// This name shows up inside the Unreal Editor
	NodeName = TEXT("Find Random Patrol Location");
}


EBTNodeResult::Type UBTTask_FindRandomPatrol::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	// This gets the AI controller and the guard
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController) return EBTNodeResult::Failed;

	APawn* AIPawn = AIController->GetPawn();
	if (!AIPawn) return EBTNodeResult::Failed;

	// This hets the navigation system
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!NavSys) return EBTNodeResult::Failed;

	// This finds a random spot within 15 meters of the guard
	FNavLocation RandomLocation;
	if (NavSys->GetRandomReachablePointInRadius(AIPawn->GetActorLocation(), 1500.0f, RandomLocation))
	{
		// This saves the random spot to the Blackboard
		OwnerComp.GetBlackboardComponent()->SetValueAsVector(GetSelectedBlackboardKey(), RandomLocation.Location);

		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::Failed;
}

