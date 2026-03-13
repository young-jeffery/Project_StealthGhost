// Fill out your copyright notice in the Description page of Project Settings.


#include "BTTask_FindRandomPatrol.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "NavigationSystem.h"
#include "AIController.h"



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

	// This gets the navigation system
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!NavSys) return EBTNodeResult::Failed;

	// This finds a random spot within 15 meters of the guard
	FNavLocation RandomLocation;
	FVector Origin = AIPawn->GetActorLocation();
	bool bFoundValidSpot = false;

	// Loop to get a valid distance from the guard. Restrict it to 10 attempts to avoid infinite loops in case of bad nav mesh
	for (int32 Attempt = 0; Attempt < 10; Attempt++)
	{
		// Pick a random location within 15 meters
		if (NavSys->GetRandomReachablePointInRadius(Origin, 1500.0f, RandomLocation))
		{
			// Measure the 3D distance from the guard to the random location
			float Distance = FVector::Dist(Origin, RandomLocation.Location);

			// if the distance is greater than 8 meters, we consider it a valid patrol point. 
			// This prevents the guard from picking a spot that's too close to its current location, which could look unnatural.
			if (Distance >= 800.0f)
			{
				bFoundValidSpot = true; 
				break;
			}
		}
	}

	// If we found a valid spot, set it in the blackboard and return success. Otherwise, return failure.
	if (bFoundValidSpot)
	{
		OwnerComp.GetBlackboardComponent()->SetValueAsVector(GetSelectedBlackboardKey(), RandomLocation.Location);
		return EBTNodeResult::Succeeded;
	}
	// If we failed to find a valid spot after 10 attempts, return failure.
	return EBTNodeResult::Failed;
}

